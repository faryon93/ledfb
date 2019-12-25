/*
 * ledfbd - constructs ethernet packets for ledmatrix from a framebuffer
 * Copyright (C) 2016 Maximilian Pachl
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <math.h>
#include <malloc.h>


// ----------------------------------------------------------------------------------
//  constants
// ----------------------------------------------------------------------------------

/** Frame cycle time in milliseconds. */
#define FRAME_CYCLE_TIME	25

/** Maximum packet size in bytes. */
#define PACKET_SIZE			2000

/** Ethertype of the packet. */
#define PACKET_ETHERTYPE	0x0801

/** LED matrix panel dimensions. */
#define PANEL_SIZE_X		128
#define PANEL_SIZE_Y		32
#define PANEL_BPP			3
#define PANEL_CHUNKS		8

/** All mac adresses of the led panels. */
static uint8_t panel_addrs[][6] = 
{
	{0xDE, 0xAD, 0xBE, 0xEF, 0xC0, 0xD1},
    {0xDE, 0xAD, 0xBE, 0xEF, 0xC0, 0xD2},
    {0xDE, 0xAD, 0xBE, 0xEF, 0xC0, 0xD0},
};

/** Definition of the panel matrix. */
#define PANEL_COUNT			(sizeof(panel_addrs) / sizeof(panel_addrs[0]))

/** Pixel protocol opcodes. */
#define PP_OP_STORE_FRAME	0x29

/** Gamma correction value. */
#define GAMMA               1.5

// ----------------------------------------------------------------------------------
//  local variables
// ----------------------------------------------------------------------------------

/** The user requested the application to shutdown. */
static int closereq = 0;

/** The buffer containing the ethernet frame to send. */
static uint8_t packet_buffer[PACKET_SIZE];


// ----------------------------------------------------------------------------------
//  local helper functions
// ----------------------------------------------------------------------------------

static uint64_t clock_us(void)
{
	struct timeval time;
	gettimeofday(&time, NULL);

	return (time.tv_sec * 1000 * 1000) + time.tv_usec;
}

static uint8_t correct_gamma(double g, uint8_t val)
{
	return (uint8_t) floor(255 * pow((val / 255.0), g));
}

static int eth_prepare_packet(struct sockaddr *source_hwaddr)
{
	struct ether_header *eh = (struct ether_header *)packet_buffer;

	// ethernet header
	eh->ether_shost[0] = ((uint8_t*)&source_hwaddr->sa_data)[0];
	eh->ether_shost[1] = ((uint8_t*)&source_hwaddr->sa_data)[1];
	eh->ether_shost[2] = ((uint8_t*)&source_hwaddr->sa_data)[2];
	eh->ether_shost[3] = ((uint8_t*)&source_hwaddr->sa_data)[3];
	eh->ether_shost[4] = ((uint8_t*)&source_hwaddr->sa_data)[4];
	eh->ether_shost[5] = ((uint8_t*)&source_hwaddr->sa_data)[5];
	eh->ether_type = htons(PACKET_ETHERTYPE);

	return sizeof(struct ether_header);
}

static int eth_send_packet(int fd, uint8_t dst[6], int ifindex, int len)
{
	struct ether_header *eh = (struct ether_header *)packet_buffer;
	struct sockaddr_ll addr;

	// set the destination mac address
	memcpy(eh->ether_dhost, dst, 6);

	// prepare the socketaddr
	addr.sll_ifindex = ifindex;
	addr.sll_halen = ETH_ALEN;
	memcpy(addr.sll_addr, dst, 6);

	return sendto(fd, packet_buffer, len, 0,
		(struct sockaddr*)&addr, sizeof(struct sockaddr_ll));
}


// ----------------------------------------------------------------------------------
//  signal handlers
// ----------------------------------------------------------------------------------

static void sigint_handler(int signal)
{
	closereq = 1;
    printf("Shutting down application...\n");
}


// ----------------------------------------------------------------------------------
//  entry point
// ----------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
	int fb = -1, sock = -1;
	int framesize, ifindex, mtu, payload_offset, errorcode = -1;
	uint8_t *framebuffer = NULL;
	struct fb_var_screeninfo vinfo;
	struct ifreq iface;
	struct sockaddr hwaddr;
	struct sigaction signal_handler;

	// make sure all cmd args are present
	if (argc < 3)
	{
		printf("usage: ./ledfbd iface fbdev\n");
		goto err;
	}

	// open the framebuffer device file
	fb = open(argv[2], O_RDONLY);
	if (-1 == fb)
	{
		perror("open");
		goto err;
	}

	// inquire screen infos
	if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) == -1) {
		perror("FBIOGET_VSCREENINFO");
		goto err;
	}

	// map the framebuffer pixels to userspace
	framesize = vinfo.xres * vinfo.yres * (vinfo.bits_per_pixel / 8);
    printf("Framebuffer width: %d, height: %d\n", vinfo.xres, vinfo.yres);

	framebuffer = (unsigned char*)mmap(0, framesize, PROT_READ, MAP_SHARED, fb, 0);
	if (framebuffer == MAP_FAILED)
	{
		perror("mmap");
		goto err;
	}

	size_t fb2_size = vinfo.xres * vinfo.yres * (vinfo.bits_per_pixel / 8);
	uint8_t *fb2 = malloc(fb2_size);
	memset(fb2, 0, fb2_size);

	// Open RAW socket to send on
	if ((sock = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
	    perror("socket");
	    goto err;
	}

	// Get the index of the interface to send on
	memset(&iface, 0, sizeof(struct ifreq));
	strncpy(iface.ifr_name, argv[1], IFNAMSIZ - 1);
	if (ioctl(sock, SIOCGIFINDEX, &iface) < 0)
	{
		perror("SIOCGIFINDEX");
		goto err;
	}
	ifindex = iface.ifr_ifindex;

	// Get the MAC address of the interface to send on
	if (ioctl(sock, SIOCGIFHWADDR, &iface) < 0)
	{
		perror("SIOCGIFHWADDR");
		goto err;
	}
	hwaddr = iface.ifr_hwaddr;

	// get the mtu of the sending interface
	if (ioctl(sock, SIOCGIFMTU, &iface) < 0)
	{
		perror("SIOCGIFMTU");
		goto err;
	}
	mtu = iface.ifr_mtu;

	// setup the ethernet packet
	payload_offset = eth_prepare_packet(&hwaddr);
	uint8_t *packet = packet_buffer + payload_offset;

	// setup SIGINT handler
	signal_handler.sa_handler = sigint_handler;
	sigemptyset(&signal_handler.sa_mask);
	signal_handler.sa_flags = 0;
	sigaction(SIGINT, &signal_handler, NULL);

	// mainloop composing ethernet packets
	while (!closereq)
	{
		// time before pixel sending
		uint64_t start = clock_us();

		for (int p = 0; p < PANEL_COUNT; p++)
		{
			// TODO: calculate the x and y offset from the panel position
			//		 to support multiple led matrix modules

            int px = 0;
            int py = p * PANEL_SIZE_Y;

			for (int chunk = 0; chunk < PANEL_CHUNKS; chunk++)
			{
				int packet_pos = 0;

				// opcode and chunk
				packet[packet_pos++] = PP_OP_STORE_FRAME;
				packet[packet_pos++] = (uint8_t)chunk;

                int chunk_x = (chunk < 4) ? 0 : 64;
                int chunk_y = (chunk % 4) * 8;

//                printf("px: %d, py: %d: c_x: %d, cy: %d => x: %d, y: %d\n",
//					   px, py, chunk_x, chunk_y, px + chunk_x, py + chunk_y);

				for (int y = 0; y < 8; y++)
                {
					for (int x = 0; x < 64; x++)
					{
						int fb_x = (vinfo.xres - (px + chunk_x + x)) - 1;
						int fb_y = (vinfo.yres - (py + chunk_y + y)) - 1;

						uint8_t *fb_base = &framebuffer[(fb_y * vinfo.xres + fb_x) * PANEL_BPP];
						packet[packet_pos++] = correct_gamma(GAMMA, fb_base[0]);
						packet[packet_pos++] = correct_gamma(GAMMA, fb_base[1]);
						packet[packet_pos++] = correct_gamma(GAMMA, fb_base[2]);
					}
                }

                // ethernet header (?) + opcode (1) + segment (1) + image data (8 * 64 *3)
				int sz = (8 * 64 * PANEL_BPP) + payload_offset + 2;
				if (eth_send_packet(sock, panel_addrs[p], ifindex, sz) < 0)
				{
					perror("sendto");
				}
            }
		}

		// sleep the rest of the time to accomplish the cycle time
		uint64_t time_left = (FRAME_CYCLE_TIME * 1000) - (clock_us() - start);
		if (time_left > 0)
			usleep(time_left);
		else
			printf("warn: no time to sleep\n");
	}
	errorcode = 0;

    printf("Bye Bye :)\n");

	// free all allocated ressources
err:
	if (sock > -1)
		close(sock);

	if (framebuffer)
		munmap(framebuffer, framesize);

	if (fb > -1)
		close(fb);	

	return errorcode;
}
