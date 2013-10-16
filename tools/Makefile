TOOLS := can_log_raw isoblued
TEST := sc_mod_test
PREFIX := /usr
CFLAGS := -Wall -Wextra -O3 $(CFLAGS)

all : $(TOOLS) $(TEST)

include ../buildnum.mk

isoblued : LDLIBS += -lbluetooth
isoblued : ring_buf.o

ring_buf.o : ring_buf.c ring_buf.h

install : $(TOOLS:%=install_%)

install_% :
	install $* $(PREFIX)/bin/$*

clean ::
	-$(RM) $(TOOLS) $(TEST) *.o
