# To build modules outside of the kernel tree, we run "make"
# in the kernel source tree; the Makefile these then includes this
# Makefile once again.
# This conditional selects whether we are being included from the
# kernel Makefile or not.
ifeq ($(KERNELRELEASE),)

# Assume the source tree is where the running kernel was built
KERNELDIR = ~/sources/linux-3.2.6
# The current directory is passed to sub-makes as argument
PWD := $(shell pwd)

modules:
	$(MAKE) ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- -C $(KERNELDIR) M=$(PWD) modules

clean:
	@rm *.o *.order *.mod.c *.symvers *.ko
	
.PHONY: modules clean

else
    # called from kernel build system: just declare what our modules are
obj-m := interrupt.o
endif

