TOOLS := can_log_raw isoblued
TEST := sc_mod_test
PREFIX := /usr
CFLAGS := -Wall -g

all : $(TOOLS) $(TEST)

isoblued : LDLIBS += -lbluetooth -lpthread
isoblued : ring_buf.o

install : $(TOOLS:%=install_%)

install_% :
	install $* $(PREFIX)/bin/$*

clean :
	-$(RM) $(TOOLS) $(TEST) *.o
