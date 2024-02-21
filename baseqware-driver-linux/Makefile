MOD = module/baseq-module
KPATH :=/lib/modules/$(shell uname -r)/build
PWD :=$(shell pwd)
obj-m = $(MOD).o

all:
	$(MAKE) -C $(KPATH) M=$(PWD) modules

clean:
	$(MAKE) -C $(KPATH) M=$(PWD) clean
