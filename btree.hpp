#pragma once

#include <cstdint>
#include <memory>
#include <tuple>

using key_type = std::int64_t;
using value_type = std::int64_t;
using item_type = std::tuple<key_type, value_type>;

// Keep branching factor small to stress splitting logic.
// TODO: raise to something larger.
constexpr std::size_t kBranchingFactor = 10;

class btree;
class node;
class leaf_node;
class internal_node;
class iterator;

class btree {
    friend class leaf_node;
    friend class internal_node;
public:
    btree();

    iterator insert(key_type key, value_type value);
    iterator search(key_type key);

    iterator end();
    iterator begin();
private:
    std::unique_ptr<node> _root;
};
