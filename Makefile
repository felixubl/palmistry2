CXX ?= c++
CXXFLAGS ?= -O3 -march=native -std=c++17 -Iinclude

.PHONY: all check benchmark clean

all: build/pokereval_check build/pokereval_benchmark

build:
	mkdir -p build

build/pokereval_check: tools/check.cpp include/pokereval/*.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/pokereval_benchmark: bench/benchmark.cpp include/pokereval/*.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

check: build/pokereval_check
	./build/pokereval_check

benchmark: build/pokereval_benchmark
	./build/pokereval_benchmark

clean:
	rm -rf build
