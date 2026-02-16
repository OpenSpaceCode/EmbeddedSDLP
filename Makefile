CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -pedantic -I./include
LDFLAGS = 

SRC_DIR = src
INC_DIR = include
EXAMPLES_DIR = examples
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

EXAMPLES = $(wildcard $(EXAMPLES_DIR)/*.c)
EXAMPLE_BINS = $(patsubst $(EXAMPLES_DIR)/%.c,$(BIN_DIR)/%,$(EXAMPLES))

LIB = libsdlp.a

.PHONY: all clean examples lib

all: lib examples

lib: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

examples: $(EXAMPLE_BINS)

$(BIN_DIR)/%: $(EXAMPLES_DIR)/%.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(LIB) -o $@ $(LDFLAGS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(LIB)

.PHONY: test
test: examples
	@echo "Running TM example..."
	@./$(BIN_DIR)/tm_example
	@echo ""
	@echo "Running TC example..."
	@./$(BIN_DIR)/tc_example
