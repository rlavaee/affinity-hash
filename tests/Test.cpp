#include <sstream>
#include <fstream>
#include <string>
#include <unordered_map>
#include "../AhMap.hpp"

template<class A, class B> using map = AhMap<A,B, false, true>;

int main() {
    map<std::string, unsigned> b;
    std::unordered_map<std::string, unsigned> c;

    while(true) {
      std::ifstream input("pride_book.txt");
      std::string word;

      while(input >> word) {
          b[word] += 1;
          c[word] += 1;

          if(b[word] != c[word]) {
            std::cout << "Error! Incorrect count!\n";
            exit(0);
          }
      }
    }
}
