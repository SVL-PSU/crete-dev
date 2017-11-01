obj-m += crete_kprobe_e1000.o
my_dir = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(my_dir) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(my_dir) clean
