CXX=g++
CXXFLAGS = -O3 -std=c++11 -Wall

test:
	$(CXX) $(CXXFLAGS) tests/Test.cpp -o tests/test

clean:
	rm test
