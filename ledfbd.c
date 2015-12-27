#include <stdio.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <math.h>
#include <stdint.h>
#include <sys/mman.h>

#include "utils.h"

#define MY_DEST_MAC0	0xDE
#define MY_DEST_MAC1	0xAD
#define MY_DEST_MAC2	0xBE
#define MY_DEST_MAC3	0xEF
#define MY_DEST_MAC4	0xC0
#define MY_DEST_MAC5	0xDE

#define DEFAULT_IF	"eth0"
#define BUF_SIZ		9000

int sockfd;
struct ifreq if_idx;
struct ifreq if_mac;
char sendbuf[BUF_SIZ];
struct ether_header *eh = (struct ether_header *) sendbuf;
struct iphdr *iph = (struct iphdr *) (sendbuf + sizeof(struct ether_header));
struct sockaddr_ll socket_address;
char ifName[IFNAMSIZ];

void eth_send(void *payload, size_t size, size_t opcode, size_t section)
{
	int tx_len = 0;

	/* Construct the Ethernet header */
	memset(sendbuf, 0, BUF_SIZ);
	/* Ethernet header */
	eh->ether_shost[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
	eh->ether_shost[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
	eh->ether_shost[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
	eh->ether_shost[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
	eh->ether_shost[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
	eh->ether_shost[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];
	eh->ether_dhost[0] = MY_DEST_MAC0;
	eh->ether_dhost[1] = MY_DEST_MAC1;
	eh->ether_dhost[2] = MY_DEST_MAC2;
	eh->ether_dhost[3] = MY_DEST_MAC3;
	eh->ether_dhost[4] = MY_DEST_MAC4;
	eh->ether_dhost[5] = MY_DEST_MAC5;
	/* Ethertype field */
	eh->ether_type = htons(ETH_P_IP);
	tx_len += sizeof(struct ether_header);

	/* Packet data */
	sendbuf[tx_len++] = opcode;
	sendbuf[tx_len++] = section;
	if (size != 0)
	{
		memcpy(&sendbuf[tx_len], payload, size);
		tx_len += size;
	}

	/* Index of the network device */
	socket_address.sll_ifindex = if_idx.ifr_ifindex;
	/* Address length*/
	socket_address.sll_halen = ETH_ALEN;
	/* Destination MAC */
	socket_address.sll_addr[0] = MY_DEST_MAC0;
	socket_address.sll_addr[1] = MY_DEST_MAC1;
	socket_address.sll_addr[2] = MY_DEST_MAC2;
	socket_address.sll_addr[3] = MY_DEST_MAC3;
	socket_address.sll_addr[4] = MY_DEST_MAC4;
	socket_address.sll_addr[5] = MY_DEST_MAC5;

	/* Send packet */
	if (sendto(sockfd, sendbuf, tx_len, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
	    printf("Send failed\n");
}


void flip_rgb(unsigned char *dst, unsigned char *src)
{
	int i;
	for (i = 0; i < 32*32; i++)
	{
		dst[(i*3)] 		= src[(i*3) + 2];
		dst[(i*3) + 1]	= src[(i*3) + 1];
		dst[(i*3) + 2]	= src[(i*3)];

	}
}

uint8_t correct_gamma(double g, uint8_t val)
{
	return floor(255 * pow((val / 255.0), g));
}

int main()
{
	int fd;

	printf("opening ledfb-user device... ");
	fd = open("/dev/fb1", O_RDONLY);
	if (-1 == fd)
	{
		printf("FAILED\n");
		return -1;
	}
	printf("OK\n");

	// use the default interface
	strcpy(ifName, DEFAULT_IF);

	/* Open RAW socket to send on */
	if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
	    perror("socket");
	}

	/* Get the index of the interface to send on */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0)
		perror("SIOCGIFINDEX");
	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0)
		perror("SIOCGIFHWADDR");

	double fps = 0;
	uint64_t lastTime = clock_us();
	int updateCounter = 0;


	unsigned char *pixels = (unsigned char*) mmap(0, 32 * 32 * 3, PROT_READ, MAP_SHARED, fd, 0);
	unsigned char pixels2[32 * 32 * 3];
	if (pixels == MAP_FAILED)
		printf("failed to map\n");
	while (1)
	{

		int i;
		for (i = 0; i < 32 * 32 * 3; i++)
			pixels2[i] = correct_gamma(3, pixels[i]);

		eth_send(&pixels2[0],		8*32*3, 0x29, 0);
		eth_send(&pixels2[8*32*3],  8*32*3, 0x29, 1);
		eth_send(&pixels2[16*32*3], 8*32*3, 0x29, 2);
		eth_send(&pixels2[24*32*3], 8*32*3, 0x29, 3);

		if (updateCounter == 6)
		{
			printf("\rframe rate: %ffps", fps);
			fflush(stdout);
			updateCounter = 0;
		}

		updateCounter++;
		usleep(22900);

		uint64_t now = clock_us();
		fps = 1.0 / ((now - lastTime) / 1000.0 / 1000.0);
		lastTime = now;
	}

	munmap(pixels, 32 * 32 * 3);

	close(fd);
	close(sockfd);
}
