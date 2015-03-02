CXX=gcc
CXXFLAGS = -O3 -Wall

test:
	$(CXX) $(CXXFLAGS) -D TRACE_MAP -Wall hash_test.c xxhash.c -o test

clean:
	-rm test
