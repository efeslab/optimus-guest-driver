KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD ?= $(pwd)

cflags-y += -Wno-unused-value -Wno-unused-label -I$(PWD)/include
ccflags-y += -Wno-unused-value -Wno-unused-label -I$(PWD)/include

obj-m := vai.o
obj-m += fake-hv.o

vai-y := core/vai_core.o
fake-hv-y := fake_hv/vai_mdev.o

all: module

module:
	echo $(PWD)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

