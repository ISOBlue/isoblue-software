BUILD_NUM := $(shell echo `git rev-parse HEAD`-`whoami`@`hostname`-`date +%s`)
BUILD_NUM_DEPS :=  ../.git/HEAD ../.git/index
CFLAGS += -DBUILD_NUM='"$(BUILD_NUM)"'
