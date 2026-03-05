# ============================================================================
# Makefile — Scientific Conference Organization Tool (DA 2026 - Project I)
# ============================================================================

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -O2
LDFLAGS  =

# Main program
TARGET   = myProg
SRC      = main.cpp
HEADERS  = domain.h parser.h graph.h output.h

# Test suites
TEST_DOMAIN = test_domain
TEST_PARSER = test_parser
TEST_GRAPH  = test_graph
TEST_OUTPUT = test_output

.PHONY: all clean tests run

# ---- Build targets ----

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(LDFLAGS)

# ---- Tests ----

tests: $(TEST_DOMAIN) $(TEST_PARSER) $(TEST_GRAPH) $(TEST_OUTPUT)
	@echo "=== Running all test suites ==="
	./$(TEST_DOMAIN)
	./$(TEST_PARSER)
	./$(TEST_GRAPH)
	./$(TEST_OUTPUT)
	@echo "=== ALL TESTS PASSED ==="

$(TEST_DOMAIN): test_domain.cpp domain.h
	$(CXX) $(CXXFLAGS) -o $@ test_domain.cpp

$(TEST_PARSER): test_parser.cpp parser.h domain.h
	$(CXX) $(CXXFLAGS) -o $@ test_parser.cpp

$(TEST_GRAPH): test_graph.cpp graph.h parser.h domain.h
	$(CXX) $(CXXFLAGS) -o $@ test_graph.cpp

$(TEST_OUTPUT): test_output.cpp output.h graph.h parser.h domain.h
	$(CXX) $(CXXFLAGS) -o $@ test_output.cpp

# ---- Convenience ----

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TEST_DOMAIN) $(TEST_PARSER) $(TEST_GRAPH) $(TEST_OUTPUT)
