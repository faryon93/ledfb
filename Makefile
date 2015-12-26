obj-m += ledfb.o
ledfb-objs := ledfb-userland.o ledfb-fb.o

KERNEL=/usr/src/linux-source-3.16
PWD=$(shell pwd)

all:
	$(MAKE) -C $(KERNEL) M=$(PWD) modules

clean: 
	$(MAKE) -C $(KERNEL) M=$(PWD) clean
