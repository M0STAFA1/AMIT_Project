# Makefile for compiling the kernel module

# Specify the kernel source directory (for the running kernel)
KDIR := /lib/modules/$(shell uname -r)/build
# Specify the directory containing this Makefile
PWD := $(shell pwd)

obj-m := proc_info.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
