obj-m += ledfb.o

KERNEL=/lib/modules/$(shell uname -r)/build
PWD=$(shell pwd)

all: ledfb.c
	$(MAKE) -C $(KERNEL) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL) M=$(PWD) clean
