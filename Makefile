CC ?= cc
# Optimisation / instrumentation flags; overridden by the sanitize target
OPT ?= -O2
SANITIZE_OPT = -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined \
			   -fno-sanitize-recover=all
# Stronger warnings for code quality; any warning fails the build
CFLAGS ?= $(OPT) -Iinclude -Werror -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
		  -Wcast-align -Wcast-qual -Wpointer-arith -Wformat=2 \
		  -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wundef \
		  -std=c99
LDFLAGS = 

SRC_DIR = src
INC_DIR = include
EXAMPLES_DIR = examples
TEST_DIR = tests
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin
SANITIZE_DIR = $(BUILD_DIR)/sanitize

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

EXAMPLES = $(wildcard $(EXAMPLES_DIR)/*.c)
EXAMPLE_BINS = $(patsubst $(EXAMPLES_DIR)/%.c,$(BIN_DIR)/%,$(EXAMPLES))
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BIN = $(BIN_DIR)/unit_tests

LIB = $(BUILD_DIR)/libsdlp.a

.PHONY: all clean examples lib unit-tests test coverage-html sanitize

all: lib examples

lib: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

examples: $(EXAMPLE_BINS)

unit-tests: $(TEST_BIN)

$(BIN_DIR)/%: $(EXAMPLES_DIR)/%.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(LIB) -o $@ $(LDFLAGS)

# Compile the sources and tests together with TC_SEGMENT_HEADER_ENABLED so the
# segment-header code paths are built and exercised.
$(TEST_BIN): $(SRCS) $(TEST_SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -DTC_SEGMENT_HEADER_ENABLED $(SRCS) $(TEST_SRCS) -o $@ $(LDFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR): | $(BUILD_DIR)
	mkdir -p $(OBJ_DIR)


$(BIN_DIR): | $(BUILD_DIR)
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BUILD_DIR)

coverage-html:
	bash tools/coverage_html.sh

# Rebuild everything with ASan + UBSan and run the unit tests and the examples.
# Program output is shown only on failure; sanitizer reports go to stderr and are always
# visible. The instrumented build is removed afterwards, on success and on failure.
sanitize:
	@$(MAKE) --no-print-directory clean >/dev/null
	@$(MAKE) --no-print-directory lib examples unit-tests OPT="$(SANITIZE_OPT)" >/dev/null \
		|| { $(MAKE) --no-print-directory clean >/dev/null; exit 1; }
	@mkdir -p $(SANITIZE_DIR)
	@echo "Sanitizers (ASan + UBSan):"
	@./$(TEST_BIN) >$(SANITIZE_DIR)/unit_tests.log \
		&& echo "  library via unit tests : no errors detected" \
		|| { cat $(SANITIZE_DIR)/unit_tests.log; echo "  library via unit tests : FAILED"; \
		     $(MAKE) --no-print-directory clean >/dev/null; exit 1; }
	@for example in $(EXAMPLE_BINS); do \
		name=$$(basename $$example); \
		./$$example >$(SANITIZE_DIR)/$$name.log \
			&& echo "  library via $$name : no errors detected" \
			|| { cat $(SANITIZE_DIR)/$$name.log; echo "  library via $$name : FAILED"; \
			     $(MAKE) --no-print-directory clean >/dev/null; exit 1; }; \
	done
	@$(MAKE) --no-print-directory clean >/dev/null
	@echo "Result: PASS"

test: unit-tests
	@echo "Running unit tests (TC_SEGMENT_HEADER_ENABLED)..."
	@./$(TEST_BIN)
