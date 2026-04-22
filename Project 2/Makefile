## Makefile for Register Allocation Tool (Project 2)
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -fno-inline
TARGET   := myProg
SRCS     := main.cpp Parser.cpp Graph.cpp Allocator.cpp
OBJS     := $(SRCS:.cpp=.o)

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Quick smoke test using the bundled example files
test: $(TARGET)
	@echo "--- Basic test ---"
	./$(TARGET) -b examples/ranges_basic.txt examples/config_basic.txt /tmp/out_basic.txt
	@cat /tmp/out_basic.txt
	@echo ""
	@echo "--- Spilling test ---"
	./$(TARGET) -b examples/ranges_basic.txt examples/config_spilling.txt /tmp/out_spill.txt
	@cat /tmp/out_spill.txt

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe
