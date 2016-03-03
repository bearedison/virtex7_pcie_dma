KERNEL_VERSION := $(shell uname -r)
KERNEL_MODLIB  := /lib/modules/$(KERNEL_VERSION)
KERNEL_SOURCES := $(shell test -d $(KERNEL_MODLIB)/source && echo $(KERNEL_MODLIB)/source || echo $(KERNEL_MODLIB)/build)
CPPFLAGS       += -DDRIVER_ERROR -DDRIVER_DEBUG

obj-m   := wupper.o
KDIR	:= $(KERNEL_SOURCES)
PWD	:= $(shell pwd)
default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules 
	mv wupper.ko ../drivers/wupper-${KERNEL_VERSION}.ko
clean:
	$(RM) wupper.ko.unsigned  wupper.mod.c  wupper.mod.o  wupper.o modules.order  Module.symvers .wupper* .tmp_versions/*


