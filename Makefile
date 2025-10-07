# Makefile for ST7703 GX040HD panel driver

KERNEL_VERSION ?= $(shell uname -r)
KERNEL_DIR ?= /lib/modules/$(KERNEL_VERSION)/build
PWD := $(shell pwd)

obj-m := panel-sitronix-st7703-gx040hd.o

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install: all
	sudo $(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules_install
	sudo depmod -a $(KERNEL_VERSION)

load:
	sudo modprobe panel-sitronix-st7703

unload:
	sudo modprobe -r panel-sitronix-st7703

overlay:
	sudo dtc -@ -I dts -O dtb -o st7703-gx040hd.dtbo st7703-gx040hd-overlay.dts
	sudo cp st7703-gx040hd.dtbo /boot/firmware/overlays/

.PHONY: all clean install load unload overlay