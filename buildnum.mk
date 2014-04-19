BUILD_NUM := $(shell git describe --tags --dirty --abbrev=7 --always)
BUILD_WHO := $(shell echo `whoami`@`hostname`-`date +%s`)
BUILD_NUM_DEPS :=  ../.git/HEAD ../.git/index
CFLAGS += -DBUILD_NUM='"$(BUILD_NUM)-$(BUILD_WHO)"'
