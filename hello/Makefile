ifneq ($(KERNELRELEASE),)
obj-m := hello_version.o
else
KDIR := $(HOME)/linux-kernel-labs/src/linux
all:
	$(MAKE) -C $(KDIR) M=$$PWD

checkpatch:
	$(KDIR)/scripts/checkpatch.pl --no-tree -f hello_version.c
clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
endif
