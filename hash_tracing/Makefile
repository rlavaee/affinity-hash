CXX=gcc
SO=.so
CXXFLAGS = -std=c++11 -I ../include -Wall
LXDFLAGS = -lstdc++

all: libhash_trace$(SO)

clean:
	-rm libhash_trace$(SO) HashTracing.o

HashTracing.o: HashTracing.cpp HashTracing.hpp
	$(CXX) $(CXXFLAGS) -g -fPIC -c -o $@ HashTracing.cpp

libhash_trace$(SO): HashTracing.o
	$(CXX) $(LXDFLAGS) -shared -o $@ $<
