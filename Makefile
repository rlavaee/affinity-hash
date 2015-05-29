CXX=gcc
CXXFLAGS = -O3 -Wall
LDFLAGS=-L./hash_tracing/
# -L./hash_tracing/ -Wl,-rpath,$(ORIGIN)/hash_tracing

test:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -Wall hash_table.c -o test -lhash_trace

clean:
	-rm test
