#include <sstream>
#include <fstream>
#include <string>
#include <unordered_map>
#include "../AhMap.hpp"

template<class A, class B> using map = AhMap<A,B, false, true>;
int main() {
    map<std::string, unsigned> b;

    while(true) {
      std::ifstream input( "alice_book.txt" );
      std::string word;

      while(input >> word)
          b[word] += 1;
    }
}
