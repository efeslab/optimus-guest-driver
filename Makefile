KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD ?= $(pwd)

cflags-y += -Wno-unused-value -Wno-unused-label -I$(PWD)/include
ccflags-y += -Wno-unused-value -Wno-unused-label -I$(PWD)/include

obj-m := vai.o

vai-y := core/vai_core.o

all: module

app: lib/libvai.c test/vai_app.c
	gcc -g -c lib/libvai.c lib/malloc.c -Wno-unused-value -Wno-unused-label -I$(PWD)/include
	gcc -g -c test/vai_app.c -Wno-unused-value -Wno-unused-label -I$(PWD)/include
	gcc -g -o app vai_app.o libvai.o malloc.o -Wno-unused-value -Wno-unused-label -I$(PWD)/include

module:
	echo $(PWD)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm -rf app
