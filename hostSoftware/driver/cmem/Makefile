KERNEL_VERSION := $(shell uname -r)
KERNEL_MODLIB  := /lib/modules/$(KERNEL_VERSION)
KERNEL_SOURCES := $(shell test -d $(KERNEL_MODLIB)/source && echo $(KERNEL_MODLIB)/source || echo $(KERNEL_MODLIB)/build)
CPPFLAGS       += -DDRIVER_ERROR -DDRIVER_DEBUG

obj-m   := cmem.o
KDIR	:= $(KERNEL_SOURCES)
PWD	:= $(shell pwd)
default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules 
	mv cmem.ko ../drivers/cmem-${KERNEL_VERSION}.ko
clean:
	$(RM) cmem.ko.unsigned cmem.mod.c  cmem.mod.o  cmem.o  modules.order  Module.symvers .cmem* .tmp_versions/*



