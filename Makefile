CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2

all: skiplist_test

skiplist_test: main.cpp skiplist.hpp iorderedset.hpp skiplist_except.hpp
	$(CXX) $(CXXFLAGS) main.cpp -o skiplist_test

run: skiplist_test
	./skiplist_test

clean:
	rm -f skiplist_test

.PHONY: all run clean
