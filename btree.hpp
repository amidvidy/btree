#pragma once

#include <cstdint>
#include <memory>
#include <tuple>
#include <iostream>
#include <functional>

namespace amidvidy {

template <typename K, typename V, std::size_t BucketSize = 100u,
          typename Compare = std::less<K>>
class btree {
  class node;
  class leaf_node;
  class internal_node;

public:
  btree();

  using key_type = K;
  using value_type = V;
  using item_type = std::tuple<key_type, value_type>;

  class iterator;

  iterator insert(key_type key, value_type value);
  iterator search(key_type key);

  iterator end();
  iterator begin();

  // For debugging.
  std::ostream &print(std::ostream &os);

private:
  std::unique_ptr<node> _root;
};

} // namespace amidvidy

#define AMIDVIDY_IN_BTREE_HPP
#include "internal.hpp"
#undef AMIDVIDY_IN_BTREE_HPP
