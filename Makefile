KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD ?= $(pwd)

cflags-y += -Wno-unused-value -Wno-unused-label -I$(PWD)/include
ccflags-y += -Wno-unused-value -Wno-unused-label -I$(PWD)/include

obj-m := vai.o
obj-m += fake-hv.o

vai-y := core/vai_core.o
fake-hv-y := fake_hv/vai_mdev.o

all: module

app: lib/libvai.c test/vai_app.c
	gcc -g -c lib/libvai.c -Wno-unused-value -Wno-unused-label -I$(PWD)/include
	gcc -g -c test/vai_app.c -Wno-unused-value -Wno-unused-label -I$(PWD)/include
	gcc -g -o app vai_app.o libvai.o -Wno-unused-value -Wno-unused-label -I$(PWD)/include

module:
	echo $(PWD)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm app
