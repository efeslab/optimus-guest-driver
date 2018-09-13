TARGET_MODULE := vai

ifneq ($(KERNELRELEASE),)
	$(TARGET_MODULE)-objs := vai_core.o
	obj-m += $(TARGET_MODULE).o
else
	KERNELDIR := /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

all: module

module:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

endif

