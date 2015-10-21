#include <sstream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <time.h>
#include "../AhMap.hpp"

template<class A, class B> using map = AhMap<A,B, false, true>;

clock_t update_avg(double curr_avg, double new_val, double n) {
    return (n / (n+1)) * curr_avg + (1 / (n+1)) * new_val;
}

int main() {
    map<std::string, unsigned> b;
    std::unordered_map<std::string, unsigned> c;
    
    clock_t t, avg = 0.0;
    unsigned n = 1;

    unsigned tot = 100000, p = 0, m = 0, e = 0;

    while(tot--) {
      std::ifstream input("pride_book.txt");
      std::string word;

      t = clock();
      std::cout << t << " , ";

      while(input >> word)
          b[word] += 1;

      std::cout << clock() << "\n";

      t = clock() - t;

      avg = (avg == 0.0) ? t : update_avg(avg, t, n++);

      std::cout << "Avg: " << avg;

      if ((float)t < (float)avg) {
        m++;
        std::cout << " - ";
      } else if ((float)t > (float)avg) {
        p++;
        std::cout << " + ";
      } else {
        e++;
        std::cout << " = ";
      }

      std::cout.flush();

    }
    std::cout << "Plus: " << p << " Minus: " << m << " Equal: " << e << "\n";
}
