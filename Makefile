CC ?= cc
# Optimisation / instrumentation flags; overridden by the sanitize target and by
# tools/coverage_html.sh
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
# The unit tests and the examples are built a second time with the optional TC Segment
# Header compiled in. That build has its own directory, so its coverage data stays apart
# from the default one.
SEGMENT_DIR = $(BUILD_DIR)/segment_header
SEGMENT_TEST_BIN = $(SEGMENT_DIR)/unit_tests
SEGMENT_EXAMPLE_BINS = $(patsubst $(EXAMPLES_DIR)/%.c,$(SEGMENT_DIR)/%,$(EXAMPLES))

LIB = $(BUILD_DIR)/libsdlp.a

.PHONY: all clean examples lib unit-tests test coverage-html sanitize

all: lib examples

lib: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

examples: $(EXAMPLE_BINS) $(SEGMENT_EXAMPLE_BINS)

unit-tests: $(TEST_BIN) $(SEGMENT_TEST_BIN)

$(BIN_DIR)/%: $(EXAMPLES_DIR)/%.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(LIB) -o $@ $(LDFLAGS)

# Segment-header configuration: the library is built without it, so its sources are
# compiled together with each example, all with TC_SEGMENT_HEADER_ENABLED.
$(SEGMENT_DIR)/%: $(EXAMPLES_DIR)/%.c $(SRCS) | $(SEGMENT_DIR)
	$(CC) $(CFLAGS) -DTC_SEGMENT_HEADER_ENABLED $< $(SRCS) -o $@ $(LDFLAGS)

# Default configuration: the tests link the library exactly as `make lib` builds it.
$(TEST_BIN): $(TEST_SRCS) $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(TEST_SRCS) $(LIB) -o $@ $(LDFLAGS)

# Segment-header configuration: the library is built without it, so the sources are
# compiled together with the tests, all with TC_SEGMENT_HEADER_ENABLED.
$(SEGMENT_TEST_BIN): $(SRCS) $(TEST_SRCS) | $(SEGMENT_DIR)
	$(CC) $(CFLAGS) -DTC_SEGMENT_HEADER_ENABLED $(SRCS) $(TEST_SRCS) -o $@ $(LDFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR): | $(BUILD_DIR)
	mkdir -p $(OBJ_DIR)


$(BIN_DIR): | $(BUILD_DIR)
	mkdir -p $(BIN_DIR)

$(SEGMENT_DIR): | $(BUILD_DIR)
	mkdir -p $(SEGMENT_DIR)

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
		&& echo "  library via unit tests                  : no errors detected" \
		|| { cat $(SANITIZE_DIR)/unit_tests.log; \
		     echo "  library via unit tests                  : FAILED"; \
		     $(MAKE) --no-print-directory clean >/dev/null; exit 1; }
	@./$(SEGMENT_TEST_BIN) >$(SANITIZE_DIR)/unit_tests_segment_header.log \
		&& echo "  library via unit tests (segment header) : no errors detected" \
		|| { cat $(SANITIZE_DIR)/unit_tests_segment_header.log; \
		     echo "  library via unit tests (segment header) : FAILED"; \
		     $(MAKE) --no-print-directory clean >/dev/null; exit 1; }
	@for example in $(EXAMPLE_BINS) $(SEGMENT_EXAMPLE_BINS); do \
		name=$$(basename $$example); \
		case $$example in $(SEGMENT_DIR)/*) name="$$name (segment header)";; esac; \
		log=$(SANITIZE_DIR)/$$(echo $$example | tr '/' '_').log; \
		./$$example >$$log \
			&& printf "  library via %-27s : no errors detected\n" "$$name" \
			|| { cat $$log; printf "  library via %-27s : FAILED\n" "$$name"; \
			     $(MAKE) --no-print-directory clean >/dev/null; exit 1; }; \
	done
	@$(MAKE) --no-print-directory clean >/dev/null
	@echo "Result: PASS"

test: unit-tests
	@echo "Running unit tests (default configuration)..."
	@./$(TEST_BIN)
	@echo "Running unit tests (TC_SEGMENT_HEADER_ENABLED)..."
	@./$(SEGMENT_TEST_BIN)
