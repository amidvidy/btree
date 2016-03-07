#include <algorithm>
#include <iterator>
#include <iostream>
#include <stdexcept>

#include "btree.hpp"

int main() {
  amidvidy::btree<std::int64_t, std::int64_t> bt;

  for (int i = 0; i < 10; ++i) {
    std::cout << "inserting: " << i << std::endl;
    bt.insert(i, 1);
    bt.print(std::cout);
  }
  for (int i = 0; i < 10; ++i) {
    std::cout << "inserting: " << i << std::endl;
    bt.insert(i, 2);
    bt.print(std::cout);
  }
  // for (int i = 0; i < 10; ++i) {
  //     std::cout << "inserting: " << i << std::endl;
  //     bt.insert(i, 3);
  //     // bt.print(std::cout);
  // }

  for (auto entry : bt) {
    std::cout << "(" << std::get<0>(entry) << ", " << std::get<1>(entry) << ")"
              << std::endl;
  }
}
