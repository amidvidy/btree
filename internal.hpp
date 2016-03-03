#pragma once

#include <tuple>
#include <array>
#include <memory>

#ifndef AMIDVIDY_IN_BTREE_HPP
#error "Do not include this file directly, include btree.hpp instead."
#endif

namespace amidvidy {

// TODO, move a lot of common functionality between leaf and internal nodes up
// here.
template <typename K, typename V, std::size_t BucketSize, typename Compare>
class btree<K, V, BucketSize, Compare>::node {
public:
  virtual ~node() = default;

  virtual iterator search(key_type key) = 0;

  virtual iterator insert(key_type key, value_type value) = 0;

  virtual iterator begin() = 0;

  virtual std::ostream &print(std::ostream &os) = 0;

  virtual void set_parent(internal_node *parent) = 0;

  virtual key_type lowest_key() = 0;
};

template <typename K, typename V, std::size_t BucketSize, typename Compare>
class btree<K, V, BucketSize, Compare>::leaf_node : public btree::node {
public:
  leaf_node(btree *owner) : _owner(owner) {}

  iterator insert(key_type key, value_type value) final {
    // Does this entry fit? otherwise we need to split.
    if (_size == BucketSize) {
      leaf_node *node_for_key = split_for_insert(key);
      // Could end inserting here or the new node, depending on where the key
      // compared to our split point.
      return node_for_key->insert(key, value);
    }
    // Use upper bound so items with same key are kept in insertion order.
    auto storage_iter = std::upper_bound(
        storage_begin(), storage_end(), item_type(key, value), item_comparator);
    if (storage_iter != storage_end()) {
      // We already are in range. Move the matching elements back to make room.
      auto new_end = storage_end() + 1;
      std::move_backward(storage_iter, storage_end(), new_end);
      *storage_iter = item_type(key, value);
    } else {
      *storage_end() = item_type(key, value);
      storage_iter = storage_end();
    }
    ++_size;
    return iterator(this, storage_iter);
  }

  iterator search(key_type key) final {
    auto storage_iter = std::lower_bound(storage_begin(), storage_end(),
                                         item_type(key, -1), item_comparator);

    if (storage_iter != storage_end()) {
      return iterator(this, storage_iter);
    }
    return iterator();
  }

  iterator begin() final { return iterator(this, storage_begin()); }

  leaf_node *next() { return _next; }

  leaf_node *prev() { return _prev; }

  std::ostream &print(std::ostream &os) final {
    os << "leaf_node:" << this << std::endl;
    auto iter = storage_begin();
    while (iter != storage_end()) {
      os << "\t"
         << "(" << std::get<0>(*iter) << ", " << std::get<1>(*iter) << ")"
         << std::endl;
      ++iter;
    }
    return os;
  }

private:
  internal_node *_parent = nullptr;

  friend class iterator;

  leaf_node *_next = nullptr;
  leaf_node *_prev = nullptr;

  std::size_t _size = 0;

  btree *_owner;

  std::array<std::tuple<key_type, value_type>, BucketSize> _storage;

  using storage_iter_type = decltype(std::begin(_storage));

  void set_parent(internal_node *parent) final { _parent = parent; }

  auto storage_begin() { return std::begin(_storage); }

  auto storage_end() { return storage_begin() + _size; }

  key_type lowest_key() final { return std::get<0>(*storage_begin()); }

  static bool item_comparator(const item_type &rhs, const item_type &lhs) {
    return std::get<0>(rhs) < std::get<0>(lhs);
  }

  // Returns the node to insert the key in to.
  leaf_node *split_for_insert(key_type to_insert) {
    // time to split. allocate a new node.
    auto new_node = std::make_unique<leaf_node>(_owner);
    auto new_node_unowned = new_node.get();
    auto split_point = storage_begin() + (_size / 2);
    auto split_key = std::get<0>(*split_point);

    auto old_next = _next;
    _next = new_node.get();
    new_node->_next = old_next;
    new_node->_prev = this;

    // Copy the second half of our entries to the new node.
    std::move(split_point, storage_end(), new_node->storage_begin());

    auto old_size = _size;
    _size = split_point - storage_begin();
    new_node->_size += old_size - _size; // handle odd branching factors...

    // If we are not the root.
    if (_parent) {
      auto new_node_lowest_key = new_node->lowest_key();
      _parent->insert_node(std::move(new_node_lowest_key), std::move(new_node));
    } else {
      // We are the root.
      // take ownership of ourself.
      auto this_node = std::move(_owner->_root);
      // Make a new internal node for the root.
      auto new_root = std::make_unique<internal_node>(_owner);
      // insert ourself.
      auto this_node_lowest_key = lowest_key();
      new_root->insert_node(std::move(this_node_lowest_key),
                            std::move(this_node));
      // make the new root our parent (as well as the new node's).
      // insert the new node.
      auto new_node_lowest_key = new_node->lowest_key();
      new_root->insert_node(new_node_lowest_key, std::move(new_node));
      // make the new node the root.
      _owner->_root = std::move(new_root);
    }

    if (to_insert >= split_key) {
      return new_node_unowned;
    }
    return this;
  }
};

template <typename K, typename V, std::size_t BucketSize, typename Compare>
class btree<K, V, BucketSize, Compare>::iterator
    : public std::iterator<std::bidirectional_iterator_tag, item_type> {
public:
  iterator() = default;

  iterator(leaf_node *node,
           typename btree<K, V, BucketSize,
                          Compare>::leaf_node::storage_iter_type storage_iter)
      : _node(node), _storage_iter(storage_iter) {}

  item_type &operator*() {
    check_valid();
    return *_storage_iter;
  }

  item_type *operator->() {
    check_valid();
    return *_storage_iter;
  }

  iterator operator++() {
    check_valid();
    ++_storage_iter;
    if (_storage_iter == _node->storage_end()) {
      if (auto next_node = _node->next()) {
        *this = next_node->begin();
      } else {
        *this = iterator();
      }
    }
    return *this;
  }

  iterator operator++(int) {
    auto prev = *this;
    operator++();
    return prev;
  }

  iterator operator--();

  iterator operator--(int);

  friend bool operator==(const iterator &rhs, const iterator &lhs) {
    return rhs.tie() == lhs.tie();
  }

  friend bool operator!=(const iterator &rhs, const iterator &lhs) {
    return !(rhs == lhs);
  }

private:
  void check_valid() {
    if (!_node || _storage_iter < _node->storage_begin() ||
        _storage_iter >= _node->storage_end()) {
      throw std::exception();
    }
  }

  auto tie() const { return std::tie(_node, _storage_iter); }

  leaf_node *_node = nullptr;
  typename leaf_node::storage_iter_type _storage_iter = nullptr;
};

template <typename K, typename V, std::size_t BucketSize, typename Compare>
class btree<K, V, BucketSize, Compare>::internal_node : public node {
  friend class leaf_node;

public:
  internal_node(btree *owner) : _owner(owner) {}

  iterator insert(key_type key, value_type value) final {
    auto storage_iter = std::upper_bound(storage_begin(), storage_end(),
                                         internal_item_type(key, nullptr),
                                         internal_item_comparator);
    // Since we currently point to the first key that is greater than us, we
    // want
    // to
    // go back one (so we're pointing at the last key less than or equal to us).
    --storage_iter;
    return std::get<1>(*storage_iter)->insert(key, value);
  }

  iterator search(key_type key) final {
    return iterator(); // TODO
  }

  iterator begin() final { return std::get<1>(*storage_begin())->begin(); }

  std::ostream &print(std::ostream &os) final {
    os << "internal_node:" << this << std::endl;
    auto storage_iter = storage_begin();
    while (storage_iter != storage_end()) {
      os << "\t"
         << "key: " << std::get<0>(*storage_iter) << std::endl;
      ++storage_iter;
    }
    storage_iter = storage_begin();
    while (storage_iter != storage_end()) {
      std::get<1>(*storage_iter)->print(os);
      ++storage_iter;
    }
    return os;
  }

private:
  void insert_node(key_type key, std::unique_ptr<node> node) {
    if (_size == BucketSize) {
      auto node_for_key = split_for_insert(key);
      return node_for_key->insert_node(key, std::move(node));
    }

    node->set_parent(this);

    auto storage_iter = std::upper_bound(storage_begin(), storage_end(),
                                         internal_item_type(key, nullptr),
                                         internal_item_comparator);

    auto new_end = storage_end() + 1;
    std::move_backward(storage_iter, storage_end(), new_end);
    *storage_iter = internal_item_type(key, std::move(node));
    ++_size;
  }
  void set_parent(internal_node *parent) final { _parent = parent; }

  internal_node *_parent = nullptr;
  btree *_owner = nullptr;
  std::size_t _size = 0;

  using internal_item_type = std::tuple<key_type, std::unique_ptr<node>>;

  std::array<internal_item_type, BucketSize> _storage;

  static bool internal_item_comparator(const internal_item_type &lhs,
                                       const internal_item_type &rhs) {
    return std::get<0>(lhs) < std::get<0>(rhs);
  }

  auto storage_begin() { return std::begin(_storage); }

  auto storage_end() { return storage_begin() + _size; }

  key_type lowest_key() final { return std::get<0>(*storage_begin()); }

  internal_node *split_for_insert(key_type to_insert) {
    // handle splits later.
    // time to split. allocate a new node.
    auto new_node = std::make_unique<internal_node>(_owner);
    auto new_node_unowned = new_node.get();
    auto split_point = storage_begin() + (_size / 2);

    auto split_key = std::get<0>(*split_point);

    // Copy the second half of our entries to the new node.
    std::move(split_point, storage_end(), new_node->storage_begin());

    auto old_size = _size;
    _size = split_point - storage_begin();
    new_node->_size = old_size - _size;

    // Update parent pointers.
    for (auto iter = new_node->storage_begin(); iter != new_node->storage_end();
         ++iter) {
      std::get<1>(*iter)->set_parent(new_node.get());
    }

    // If we are not the root.
    if (_parent) {
      auto new_node_lowest_key = new_node->lowest_key();
      _parent->insert_node(new_node_lowest_key, std::move(new_node));
    } else {
      // We are the root.
      // take ownership of ourself.
      auto this_node = std::move(_owner->_root);
      // Make a new internal node for the root.
      auto new_root = std::make_unique<internal_node>(_owner);
      // insert ourself.
      new_root->insert_node(lowest_key(), std::move(this_node));
      // make the new root our parent (as well as the new node's).
      new_node->_parent = _parent = new_root.get();
      // insert the new node.
      auto new_node_lowest_key = new_node->lowest_key();
      new_root->insert_node(std::move(new_node_lowest_key),
                            std::move(new_node));
      // make the new node the root.
      _owner->_root = std::move(new_root);
    }

    if (to_insert >= split_key) {
      return new_node_unowned;
    }
    return this;
  }
};

template <typename K, typename V, std::size_t B, typename C>
btree<K, V, B, C>::btree()
    : _root(std::make_unique<leaf_node>(this)) {}

template <typename K, typename V, std::size_t B, typename C>
auto btree<K, V, B, C>::insert(key_type key, value_type value) -> iterator {
  return _root->insert(key, value);
}

template <typename K, typename V, std::size_t B, typename C>
auto btree<K, V, B, C>::end() -> iterator {
  return iterator();
}

template <typename K, typename V, std::size_t B, typename C>
auto btree<K, V, B, C>::begin() -> iterator {
  return _root->begin();
}

template <typename K, typename V, std::size_t B, typename C>
std::ostream &btree<K, V, B, C>::print(std::ostream &os) {
  return _root->print(os);
}

} // namespace amidvidy
