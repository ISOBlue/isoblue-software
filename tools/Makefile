TOOLS := can_log_raw isoblue
TEST := sc_mod_test
PREFIX := /usr
CFLAGS := -Wall -g

all : $(TOOLS) $(TEST)

install : $(TOOLS:%=install_%)

install_% :
	install $* $(PREFIX)/bin/$*

clean :
	-$(RM) $(TOOLS) $(TEST) *.o
