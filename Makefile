CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -Werror -std=c99 -pedantic -I./include
CXXFLAGS = -Wall -Wextra -Werror -std=c++14 -I./include
LDFLAGS = 

SRC_DIR = src
INC_DIR = include
EXAMPLES_DIR = examples
TESTS_DIR = tests
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

EXAMPLES = $(wildcard $(EXAMPLES_DIR)/*.c)
EXAMPLE_BINS = $(patsubst $(EXAMPLES_DIR)/%.c,$(BIN_DIR)/%,$(EXAMPLES))

# Main GoogleTest binary (no TC_SEGMENT_HEADER_ENABLED)
TEST_SRCS = $(TESTS_DIR)/test_common.cpp $(TESTS_DIR)/test_tm.cpp $(TESTS_DIR)/test_tc.cpp
TEST_OBJS = $(patsubst $(TESTS_DIR)/%.cpp,$(OBJ_DIR)/tests/%.o,$(TEST_SRCS))
TEST_BIN = $(BIN_DIR)/run_tests

# Segment header GoogleTest binary (TC_SEGMENT_HEADER_ENABLED)
SEG_TEST_SRCS = $(TESTS_DIR)/test_tc_segment.cpp
SEG_TEST_OBJS = $(patsubst $(TESTS_DIR)/%.cpp,$(OBJ_DIR)/tests/seg_%.o,$(SEG_TEST_SRCS))
SEG_LIB_OBJ = $(OBJ_DIR)/tests/sdlp_tc_seg.o $(OBJ_DIR)/sdlp_common.o $(OBJ_DIR)/sdlp_tm.o
SEG_TEST_BIN = $(BIN_DIR)/run_segment_tests

GTEST_LIBS = -lgtest -lgtest_main -lpthread

LIB = libsdlp.a

.PHONY: all clean examples lib test gtest

all: lib examples

lib: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

examples: $(EXAMPLE_BINS)

$(BIN_DIR)/%: $(EXAMPLES_DIR)/%.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(LIB) -o $@ $(LDFLAGS)

$(OBJ_DIR)/tests/%.o: $(TESTS_DIR)/%.cpp | $(OBJ_DIR)/tests
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_BIN): $(TEST_OBJS) $(LIB) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(GTEST_LIBS) $(LDFLAGS)

# Segment-header-enabled variants
$(OBJ_DIR)/tests/seg_%.o: $(TESTS_DIR)/%.cpp | $(OBJ_DIR)/tests
	$(CXX) $(CXXFLAGS) -DTC_SEGMENT_HEADER_ENABLED -c $< -o $@

$(OBJ_DIR)/tests/sdlp_tc_seg.o: $(SRC_DIR)/sdlp_tc.c | $(OBJ_DIR)/tests
	$(CC) $(CFLAGS) -DTC_SEGMENT_HEADER_ENABLED -c $< -o $@

$(SEG_TEST_BIN): $(SEG_TEST_OBJS) $(SEG_LIB_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(GTEST_LIBS) $(LDFLAGS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/tests:
	mkdir -p $(OBJ_DIR)/tests

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(LIB)

gtest: $(TEST_BIN) $(SEG_TEST_BIN)
	@echo "Running GoogleTest unit tests..."
	@./$(TEST_BIN)
	@echo ""
	@echo "Running GoogleTest segment header tests..."
	@./$(SEG_TEST_BIN)

.PHONY: test
test: examples gtest
	@echo "Running TM example..."
	@./$(BIN_DIR)/tm_example
	@echo ""
	@echo "Running TC example..."
	@./$(BIN_DIR)/tc_example
