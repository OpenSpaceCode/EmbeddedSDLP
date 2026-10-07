CC ?= cc
# Stronger warnings for code quality; any warning fails the build
CFLAGS ?= -O2 -Iinclude -Werror -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
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

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

EXAMPLES = $(wildcard $(EXAMPLES_DIR)/*.c)
EXAMPLE_BINS = $(patsubst $(EXAMPLES_DIR)/%.c,$(BIN_DIR)/%,$(EXAMPLES))
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BIN = $(BIN_DIR)/unit_tests

LIB = $(BUILD_DIR)/libsdlp.a

.PHONY: all clean examples lib unit-tests test coverage-html

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
	rm -rf $(BUILD_DIR) obj bin libsdlp.a

coverage-html:
	bash tools/coverage_html.sh

test: unit-tests
	@echo "Running unit tests (TC_SEGMENT_HEADER_ENABLED)..."
	@./$(TEST_BIN)
