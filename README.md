#ledfb
A framebuffer playback device for ethernet based LED matrix displays.

## Installation
```sh
# compile and insert kernel module (in-memory framebuffer)
$: pacman -S linux-headers
$: make
$: insmod ledfb.ko

# compile and run daemon
$: mkdir build && cd build
$: cmake .. && make
$: sudo ip link set dev enp0s25 mtu 9000
$: sudo ./ledfbd enp0s25 /dev/fb1
```

## Video Playback
```sh
$: mplayer -vo fbdev:/dev/fb1 -vf scale=128:96 -aspect 4:3 -loop 0 video.mp4
```
