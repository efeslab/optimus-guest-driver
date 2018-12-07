KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD ?= $(pwd)

cflags-y += -Wno-unused-value -Wno-unused-label -I$(PWD)/include
ccflags-y += -Wno-unused-value -Wno-unused-label -I$(PWD)/include

obj-m := vai.o

vai-y := core/vai_core.o

all: module

INCLUDE_FILE = include/vai/vai.h include/vai/malloc.h include/vai/vai_types.h
libvai.so: lib/libvai.c lib/malloc.c ${INCLUDE_FILE}
	gcc -g -shared -fPIC lib/libvai.c lib/malloc.c -o libvai.so -I${PWD}/include
app: test/vai_app.c libvai.so
	gcc -g test/vai_app.c -Wno-unused-value -Wno-unused-label -I$(PWD)/include -L${PWD} -lvai -oapp

module:
	echo $(PWD)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm -rf app libvai.so
