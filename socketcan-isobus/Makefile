KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

modules :

% :
	$(MAKE) -C $(KDIR) M=$(PWD) CONFIG_MODULE_SIG=n $@
