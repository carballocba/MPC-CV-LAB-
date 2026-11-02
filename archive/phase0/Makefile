CXX = clang++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude -pthread
.PHONY: all test clean
all: build/simulate build/core_test
build:
	mkdir -p build
build/simulate: src/simulate.cpp include/mpccv/core.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@
build/core_test: tests/core_test.cpp include/mpccv/core.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@
test: all
	./build/core_test
	./build/simulate > build/demo.csv
clean:
	rm -rf build
