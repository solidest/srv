# target macros
BIN_PATH := bin
TARGET_NAME := test

TARGET := $(BIN_PATH)/$(TARGET_NAME)
MAIN_SRC := test.cpp

TEST_TARGET_NAME := main_test
TEST_TARGET := $(BIN_PATH)/$(TEST_TARGET_NAME)
TEST_MAIN_SRC := test.cpp

# compile macros
DIRS := src 
OBJS := 
TEST_OBJS := 
# UTILITIES_OBJS := src/utilities/merge_operator.o

# intermedia compile marcros
# NOTE: ALL_OBJS are intentionally left blank, no need to fill it
ALL_OBJS :=
CLEAN_FILES := $(TARGET) $(TEST_TARGET) $(OBJS)
DIST_CLEAN_FILES := $(OBJS)

LIBS := libuv-1.33.1/out/Release/libuv.a hiredis/libhiredis.a

# recursive wildcard
rwildcard=$(foreach d,$(wildcard $(addsuffix *,$(1))),$(call rwildcard,$(d)/,$(2))$(filter $(subst *,%,$(2)),$(d)))

# default target
default: show-info all

# non-phony targets
$(TARGET): build-subdirs $(OBJS) find-all-objs
	@echo " " CC "\t" $(CCFLAG) "*.o" $(MAIN_SRC) -o $@
	@$(CC) -pthread $(CCFLAG) $(ALL_OBJS)  $(MAIN_SRC) $(LIBS) -o $@ -ldl

$(TEST_TARGET): build-subdirs $(OBJS) find-all-objs
	@echo " " CC "\t" $(CCFLAG) "*.o" $(TEST_MAIN_SRC) -o $@
	@$(CC) -pthread $(CCFLAG) $(ALL_OBJS) $(TEST_OBJS) $(TEST_MAIN_SRC) $(LIBS) -o $@ -ldl

# phony targets
.PHONY: all
all: $(TARGET)
	@echo Target $(TARGET) build finished.

.PHONY: test
test: $(TEST_TARGET)
	@echo Target $(TEST_TARGET) build finished.

.PHONY: clean
clean: clean-subdirs
	@echo CLEAN $(CLEAN_FILES)
	@rm -f $(CLEAN_FILES)

.PHONY: distclean
distclean: clean-subdirs
	@echo CLEAN $(DIST_CLEAN_FILES)
	@rm -f $(DIST_CLEAN_FILES)

# phony funcs
.PHONY: find-all-objs
find-all-objs:
	$(eval ALL_OBJS += $(call rwildcard,$(DIRS),*.o))

.PHONY: show-info
show-info:
	@echo Building Project : $(OBJCCFLAG)

# need to be placed at the end of the file
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
export PROJECT_PATH := $(patsubst %/,%,$(dir $(mkfile_path)))
export MAKE_INCLUDE=$(PROJECT_PATH)/config/make.global
export SUB_MAKE_INCLUDE=$(PROJECT_PATH)/config/submake.global
include $(MAKE_INCLUDE)
