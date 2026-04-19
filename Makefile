# Makefile for sampling-colouring project
# Requires: OpenCV 4.x (installed via Homebrew on Mac or apt on Linux)
# Usage:
#   make          – build release binary
#   make debug    – build with -g -O0 for debugger use
#   make clean    – remove build artefacts

CXX      := g++
TARGET   := sampling_colouring

# OpenCV flags via pkg-config (works on Mac/Linux with OpenCV 4)
OPENCV_CFLAGS := $(shell pkg-config --cflags opencv4 2>/dev/null || \
                          pkg-config --cflags opencv   2>/dev/null)
OPENCV_LIBS   := $(shell pkg-config --libs   opencv4 2>/dev/null || \
                          pkg-config --libs   opencv   2>/dev/null)

CXXFLAGS := -std=c++17 -Wall -Wextra -O2 $(OPENCV_CFLAGS)
LDFLAGS  := $(OPENCV_LIBS)

SRCS := main.cpp \
        seed.cpp \
        watershed_task.cpp \
        adjacency.cpp \
        coloring.cpp \
        utils.cpp \
        ui.cpp

OBJS := $(SRCS:.cpp=.o)

# Default target
.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: ./$(TARGET)"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Debug build
.PHONY: debug
debug: CXXFLAGS := -std=c++17 -Wall -Wextra -g -O0 $(OPENCV_CFLAGS)
debug: $(TARGET)

# Test runner (non-interactive, no GUI)
TEST_OBJS := test_runner.o seed.o watershed_task.o adjacency.o coloring.o utils.o ui.o

test_runner: $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: ./test_runner"

# Remove build artefacts
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET) $(TEST_OBJS) test_runner
	@echo "Clean complete."
