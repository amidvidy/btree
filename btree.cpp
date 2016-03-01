#include <algorithm>
#include <iterator>
#include <iostream>
#include <stdexcept>

#include "btree.hpp"
#include "internal.hpp"

// ================= iterator ==================================================

void iterator::check_valid() {
    if (!_node || _storage_iter < _node->storage_begin() || _storage_iter >= _node->storage_end()) {
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

internal_node::internal_node(btree* owner) : _owner(owner) {}

iterator internal_node::insert(key_type key, value_type value) {
    auto storage_iter = std::upper_bound(
        storage_begin(), storage_end(), internal_item_type(key, nullptr), internal_item_comparator);
    // Since we currently point to the first key that is greater than us, we want to
    // go back one (so we're pointing at the last key less than or equal to us).
    --storage_iter;
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
        for (auto iter = new_node->storage_begin(); iter != new_node->storage_end(); ++iter) {
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
            new_root->insert_node(std::move(new_node_lowest_key), std::move(new_node));
            // make the new node the root.
            _owner->_root = std::move(new_root);
        }

        if (key >= split_key) {
            return new_node_unowned->insert_node(key, std::move(node));
        }
    }

    node->set_parent(this);

    auto storage_iter = std::upper_bound(
        storage_begin(), storage_end(), internal_item_type(key, nullptr), internal_item_comparator);

    auto new_end = storage_end() + 1;
    std::move_backward(storage_iter, storage_end(), new_end);
    *storage_iter = internal_item_type(key, std::move(node));
    ++_size;
}

std::ostream& internal_node::print(std::ostream& os) {
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

// =============== leaf node ===================================================

leaf_node::leaf_node(btree* owner) : _owner(owner) {}

leaf_node* leaf_node::split_for_insert(key_type to_insert) {
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
    new_node->_size += old_size - _size;  // handle odd branching factors...

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
        new_root->insert_node(std::move(this_node_lowest_key), std::move(this_node));
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

iterator leaf_node::insert(key_type key, value_type value) {
    // Does this entry fit? otherwise we need to split.
    if (_size == kBranchingFactor) {
        leaf_node* node_for_key = split_for_insert(key);
        // Could end inserting here or the new node, depending on where the key
        // compared to our split point.
        return node_for_key->insert(key, value);
    }
    // Use upper bound so items with same key are kept in insertion order.
    auto storage_iter =
        std::upper_bound(storage_begin(), storage_end(), item_type(key, value), item_comparator);
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
        std::lower_bound(storage_begin(), storage_end(), item_type(key, -1), item_comparator);

    if (storage_iter != storage_end()) {
        return iterator(this, storage_iter);
    }
    return iterator();
}

iterator leaf_node::begin() {
    return iterator(this, storage_begin());
}

std::ostream& leaf_node::print(std::ostream& os) {
    os << "leaf_node:" << this << std::endl;
    auto iter = storage_begin();
    while (iter != storage_end()) {
        os << "\t"
           << "(" << std::get<0>(*iter) << ", " << std::get<1>(*iter) << ")" << std::endl;
        ++iter;
    }
    return os;
}

// =============== btree =======================================================

btree::btree() : _root(std::make_unique<leaf_node>(this)) {}

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

std::ostream& btree::print(std::ostream& os) {
    return _root->print(os);
}

int main() {
    btree bt;

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
        std::cout << "(" << std::get<0>(entry) << ", " << std::get<1>(entry) << ")" << std::endl;
    }
}
