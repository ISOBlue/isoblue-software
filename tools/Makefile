TOOLS := can_log_raw isoblued isobus_resend
TEST := sc_mod_test can_stress isoblue_dummy isobus_resend
PREFIX := /usr
CFLAGS := -Wall -Wextra -O3 $(CFLAGS)

all : $(TOOLS) $(TEST)
.PHONY : all

include ../buildnum.mk

isoblued isoblue_dummy : LDLIBS += -lbluetooth
isoblued : LDLIBS += -lleveldb
isoblued : ring_buf.o

ring_buf.o : ring_buf.c ring_buf.h

isobus_resend : LDLIBS += -lsqlite3

install : $(TOOLS:%=install_%)

install_% : %
	install $* $(PREFIX)/bin/$*

.PHONY : clean
clean ::
	-$(RM) $(TOOLS) $(TEST) *.o
