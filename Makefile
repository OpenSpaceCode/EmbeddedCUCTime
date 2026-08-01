CC ?= cc
AR ?= ar
OPT ?= -O2
CFLAGS ?= -std=c99 -Wall -Wextra -Iinclude
BUILD_DIR = build

LIB = $(BUILD_DIR)/libcuc.a
OBJ = $(BUILD_DIR)/src/cuc.o
CTEST = $(BUILD_DIR)/tests/ctest
EXAMPLE = $(BUILD_DIR)/examples/cuc_example

SRC = src/cuc.c
HDR = include/cuc.h

all: $(LIB) $(CTEST) $(EXAMPLE)

lib: $(LIB)

$(OBJ): $(SRC) $(HDR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OPT) -Iinclude -c $(SRC) -o $@

$(LIB): $(OBJ)
	mkdir -p $(dir $@)
	$(AR) rcs $@ $(OBJ)

ctest: $(CTEST)

$(CTEST): tests/unit_tests.c tests/test_cuc.c tests/cunit.h tests/test_runners.h $(SRC) $(HDR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OPT) -Iinclude -Itests tests/unit_tests.c tests/test_cuc.c $(SRC) -o $@

example: $(EXAMPLE)

$(EXAMPLE): examples/cuc_example.c $(SRC) $(HDR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OPT) -Iinclude examples/cuc_example.c $(SRC) -o $@

run: $(CTEST)
	$(CTEST)

coverage-html:
	bash tools/coverage-html.sh

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all lib ctest example run coverage-html clean
