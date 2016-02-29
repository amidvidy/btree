#include <algorithm>
#include <iterator>
#include <iostream>
#include <stdexcept>

#include "btree.hpp"

 // ================= iterator ==================================================

 void iterator::check_valid() {
     if (!_node ||
         _storage_iter < _node->storage_begin() ||
         _storage_iter >= _node->storage_end()) {
         throw std::exception();
     }
 }

 item_type& iterator::operator*() {
     check_valid();
     return *_storage_iter;
 }

 iterator iterator::operator++() {
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

 bool operator==(const iterator& rhs, const iterator& lhs) {
     return rhs.tie() == lhs.tie();
 }

 bool operator!=(const iterator& rhs, const iterator& lhs) {
     return !(rhs == lhs);
 }

 // ============ internal_node ==================================================

 iterator internal_node::insert(key_type key, value_type value) {
     auto storage_iter = std::lower_bound(storage_begin(),
                                          storage_end(),
                                          key,
                                          internal_item_comparator);
     if (storage_iter == storage_end()) {
         // If we're larger than the largest key we've seen, insert in the last valid bucket.
         --storage_iter;
     }
     return std::get<1>(*storage_iter)->insert(key, value);
 }

 iterator internal_node::begin() {
     return std::get<1>(*storage_begin())->begin();
 }

 iterator internal_node::search(key_type key) {
     return iterator();
 }

 void internal_node::insert_node(key_type key, std::unique_ptr<node> node) {
     if (_size == kBranchingFactor) {
         // handle splits later.
         return;
     }

     auto storage_iter = std::upper_bound(storage_begin(),
                                          storage_end(),
                                          key,
                                          internal_item_comparator);

     auto new_end = storage_end() + 1;
     std::move_backward(storage_iter, storage_end(), new_end);
     *storage_iter = internal_item_type(key, std::move(node));
     ++_size;
}


// =============== leaf node ===================================================

leaf_node::leaf_node(btree* owner) : _owner(owner) {}

iterator leaf_node::insert(key_type key, value_type value) {
    // Does this entry fit? otherwise we need to split.
    if (_size == kBranchingFactor) {
        std::cout << "splitting..." << std::endl;
        // time to split. allocate a new node.
        auto new_node = std::make_unique<leaf_node>(_owner);

        auto split_point = storage_begin() + (_size / 2);

        auto old_next = _next;
        _next = new_node.get();
        new_node->_next = old_next;
        new_node->_prev = this;

        // Copy the second half of our entries to the new node.
        std::move(split_point, storage_end(), new_node->storage_begin());

        auto old_size = _size;
        _size = old_size / 2;
        new_node->_size += old_size - _size; // handle odd branching factors...

        // If we are not the root.
        if (_parent) {
            _parent->insert_node(new_node->lowest_key(), std::move(new_node));
        } else {
            // We are the root.
            // take ownership of ourself.
            auto this_node = std::move(_owner->_root);

            // Make a new internal node for the root.
            auto new_root = std::make_unique<internal_node>();

            // insert ourself.
            new_root->insert_node(lowest_key(), std::move(this_node));

            // make the new root our parent.
            _parent = new_root.get();

            // insert the new node.
            new_root->insert_node(new_node->lowest_key(), std::move(new_node));

            // make the new node the root.
            _owner->_root = std::move(new_root);
        }

        // run the insert from the parent, in case the new key isn't in our range.
        return _parent->insert(key, value);
    }
     // Use upper bound so items with same key are kept in insertion order.
    auto storage_iter = std::upper_bound(storage_begin(), storage_end(), key, item_comparator);
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

iterator leaf_node::search(key_type key) {
    auto storage_iter =
        std::lower_bound(storage_begin(), storage_end(), key, item_comparator);

    if (storage_iter != storage_end()) {
        return iterator(this, storage_iter);
    }
    return iterator();
}

iterator leaf_node::begin() {
    return iterator(this, storage_begin());
}

// =============== btree =======================================================

iterator btree::insert(key_type key, value_type value) {
    return _root->insert(key, value);
}

iterator btree::search(key_type key) {
    return _root->search(key);
}

iterator btree::end() {
    return iterator();
}

iterator btree::begin() {
    return _root->begin();
}

int main() {
    btree bt;

    for (int i = 0; i < 2000; ++i) {
        bt.insert(i, i);
    }

    auto iter = bt.begin();

    while (iter != bt.end()) {
        std::cout << "found: ("
                  << std::get<0>(*iter)
                  << ":"
                  << std::get<1>(*iter)
                  << ")"
                  << std::endl;
        ++iter;
    }
    std::cout << "at end of cursor :(" << std::endl;
}
