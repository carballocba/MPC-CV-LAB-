CXX = clang++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude -pthread
.PHONY: all test clean vst rt check assets lab event-pipeline
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

HEADERS = $(wildcard include/mpccv/*.hpp) include/mpccv/vst/abi.hpp
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
SHARED = -dynamiclib
DL_LIB =
else
SHARED = -shared -fPIC
DL_LIB = -ldl
endif
build/mpc_cv.so: src/vst/plugin.cpp $(HEADERS) | build
	$(CXX) $(CXXFLAGS) $(SHARED) $< -o $@
build/vst_host_test: tests/vst_host_test.cpp $(HEADERS) | build
	$(CXX) $(CXXFLAGS) $< $(DL_LIB) -o $@
vst: build/mpc_cv.so build/vst_host_test
	./build/vst_host_test ./build/mpc_cv.so

build/rt_test: tests/rt_test.cpp src/vst/plugin.cpp $(HEADERS) | build
	$(CXX) $(CXXFLAGS) tests/rt_test.cpp src/vst/plugin.cpp -o $@
rt: build/rt_test
	./build/rt_test
check: test vst rt

build/lab_simulate: src/lab_simulate.cpp $(HEADERS) | build
	$(CXX) $(CXXFLAGS) $< -o $@
assets:
	python3 tools/validate_assets.py
lab: build/lab_simulate
	./build/lab_simulate build/eight-outputs.csv build/capture.bin
	python3 tools/validate_capture.py
check: assets lab

build/event_pipeline_test: tests/event_pipeline_test.cpp $(HEADERS) | build
	$(CXX) $(CXXFLAGS) $< -o $@
check: event-pipeline
event-pipeline: build/event_pipeline_test
	./build/event_pipeline_test
