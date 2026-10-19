CXX = clang++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude -pthread
.PHONY: all test clean
all: build/simulate build/core_test build/backend_test build/graph_test
build:
	mkdir -p build
build/simulate: src/simulate.cpp include/mpccv/core.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@
build/core_test: tests/core_test.cpp include/mpccv/core.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@
build/backend_test: tests/backend_test.cpp include/mpccv/backend.hpp include/mpccv/core.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@
build/graph_test: tests/graph_test.cpp include/mpccv/graph.hpp include/mpccv/scene.hpp include/mpccv/core.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@
test: all
	./build/core_test
	./build/backend_test
	./build/graph_test
	./build/simulate > build/demo.csv
clean:
	rm -rf build
