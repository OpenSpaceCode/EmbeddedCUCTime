CC ?= cc
AR ?= ar
OPT ?= -O2
SANITIZE_OPT = -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined \
               -fno-sanitize-recover=all
CFLAGS ?= -std=c99 -Iinclude
BUILD_DIR = build
SANITIZE_DIR = $(BUILD_DIR)/sanitize

WARNINGS = -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
ALL_CFLAGS = $(CFLAGS) $(WARNINGS)

CUC_LIB = $(BUILD_DIR)/libcuc.a
CUC_OBJ = $(BUILD_DIR)/src/cuc.o
CDS_LIB = $(BUILD_DIR)/libcds.a
CDS_OBJ = $(BUILD_DIR)/src/cds.o
CCS_LIB = $(BUILD_DIR)/libccs.a
CCS_OBJ = $(BUILD_DIR)/src/ccs.o
CTEST = $(BUILD_DIR)/tests/ctest
CUC_EXAMPLE = $(BUILD_DIR)/examples/cuc_example
CDS_EXAMPLE = $(BUILD_DIR)/examples/cds_example
CCS_EXAMPLE = $(BUILD_DIR)/examples/ccs_example

CUC_SRC = src/cuc.c
CUC_HDR = include/cuc.h
CDS_SRC = src/cds.c
CDS_HDR = include/cds.h
CCS_SRC = src/ccs.c
CCS_HDR = include/ccs.h

LIBS = $(CUC_LIB) $(CDS_LIB) $(CCS_LIB)
EXAMPLES = $(CUC_EXAMPLE) $(CDS_EXAMPLE) $(CCS_EXAMPLE)

all: $(LIBS) $(CTEST) $(EXAMPLES)

lib: $(LIBS)

$(CUC_OBJ): $(CUC_SRC) $(CUC_HDR)
	mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) $(OPT) -Iinclude -c $(CUC_SRC) -o $@

$(CUC_LIB): $(CUC_OBJ)
	mkdir -p $(dir $@)
	$(AR) rcs $@ $(CUC_OBJ)

$(CDS_OBJ): $(CDS_SRC) $(CDS_HDR)
	mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) $(OPT) -Iinclude -c $(CDS_SRC) -o $@

$(CDS_LIB): $(CDS_OBJ)
	mkdir -p $(dir $@)
	$(AR) rcs $@ $(CDS_OBJ)

$(CCS_OBJ): $(CCS_SRC) $(CCS_HDR)
	mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) $(OPT) -Iinclude -c $(CCS_SRC) -o $@

$(CCS_LIB): $(CCS_OBJ)
	mkdir -p $(dir $@)
	$(AR) rcs $@ $(CCS_OBJ)

ctest: $(CTEST)

$(CTEST): tests/unit_tests.c tests/test_cuc.c tests/test_cds.c tests/test_ccs.c tests/cunit.h \
          tests/test_runners.h $(CUC_SRC) $(CUC_HDR) $(CDS_SRC) $(CDS_HDR) $(CCS_SRC) $(CCS_HDR)
	mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) $(OPT) -Iinclude -Itests \
	    tests/unit_tests.c tests/test_cuc.c tests/test_cds.c tests/test_ccs.c \
	    $(CUC_SRC) $(CDS_SRC) $(CCS_SRC) -o $@

example: $(EXAMPLES)
	$(CUC_EXAMPLE)
	$(CDS_EXAMPLE)
	$(CCS_EXAMPLE)

$(CUC_EXAMPLE): examples/cuc_example.c $(CUC_SRC) $(CUC_HDR)
	mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) $(OPT) -Iinclude examples/cuc_example.c $(CUC_SRC) -o $@

$(CDS_EXAMPLE): examples/cds_example.c $(CDS_SRC) $(CDS_HDR)
	mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) $(OPT) -Iinclude examples/cds_example.c $(CDS_SRC) -o $@

$(CCS_EXAMPLE): examples/ccs_example.c $(CCS_SRC) $(CCS_HDR)
	mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) $(OPT) -Iinclude examples/ccs_example.c $(CCS_SRC) -o $@

run: $(CTEST)
	$(CTEST)

# Instrumented rebuild; program output is shown only on failure and the build is removed
# afterwards, on success and on failure.
sanitize:
	@$(MAKE) --no-print-directory clean >/dev/null
	@$(MAKE) --no-print-directory ctest example OPT="$(SANITIZE_OPT)" >/dev/null \
		|| { $(MAKE) --no-print-directory clean >/dev/null; exit 1; }
	@mkdir -p $(SANITIZE_DIR)
	@echo "Sanitizers (ASan + UBSan):"
	@./$(CTEST) >$(SANITIZE_DIR)/unit_tests.log \
		&& echo "  libraries via unit tests : no errors detected" \
		|| { cat $(SANITIZE_DIR)/unit_tests.log; echo "  libraries via unit tests : FAILED"; \
		     $(MAKE) --no-print-directory clean >/dev/null; exit 1; }
	@./$(CUC_EXAMPLE) >$(SANITIZE_DIR)/cuc_example.log \
		&& echo "  cuc via example          : no errors detected" \
		|| { cat $(SANITIZE_DIR)/cuc_example.log; echo "  cuc via example          : FAILED"; \
		     $(MAKE) --no-print-directory clean >/dev/null; exit 1; }
	@./$(CDS_EXAMPLE) >$(SANITIZE_DIR)/cds_example.log \
		&& echo "  cds via example          : no errors detected" \
		|| { cat $(SANITIZE_DIR)/cds_example.log; echo "  cds via example          : FAILED"; \
		     $(MAKE) --no-print-directory clean >/dev/null; exit 1; }
	@./$(CCS_EXAMPLE) >$(SANITIZE_DIR)/ccs_example.log \
		&& echo "  ccs via example          : no errors detected" \
		|| { cat $(SANITIZE_DIR)/ccs_example.log; echo "  ccs via example          : FAILED"; \
		     $(MAKE) --no-print-directory clean >/dev/null; exit 1; }
	@$(MAKE) --no-print-directory clean >/dev/null
	@echo "Result: PASS"

coverage-html:
	bash tools/coverage-html.sh

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all lib ctest example run sanitize coverage-html clean
