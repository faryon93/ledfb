# ledfb
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

## Capabilities and Groups
```sh
$: setcap cap_net_raw=eip ledctrl
$: setcap cap_net_raw=eip build/ledfbd

# Add your user to the group `video`:
$: nano /etc/group
video:x:986:sddm,snowden
```

## Media Playback
```sh
$: mplayer -vo fbdev:/dev/fb1 -vf scale=128:96 -aspect 4:3 -loop 0 video.mp4
$: mplayer -vo fbdev:/dev/fb1 -vf scale=128:96 -aspect 4:3 tv:// -tv driver=v4l2:device=/dev/video0
$: fbi -autodown -noverbose -blend 100 -t 3 -d /dev/fb1 36c3.png hackaday.jpg nyan.jpeg
```
