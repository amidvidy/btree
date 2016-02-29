#include <cstdint>
#include <array>
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

class node {
public:
    virtual ~node() = default;
    virtual iterator search(key_type key) = 0;
    virtual iterator insert(key_type key, value_type value) = 0;
    virtual iterator begin() = 0;
};

class leaf_node : public node {
public:
    leaf_node(btree* owner);

    iterator insert(key_type key, value_type value) final;
    iterator search(key_type key) final;
    iterator begin() final;

    leaf_node* next() { return _next; }
    leaf_node* prev() { return _prev; }
private:
    internal_node* _parent = nullptr;

    friend class iterator;

    leaf_node* _next = nullptr;
    leaf_node* _prev = nullptr;

    std::size_t _size = 0;

    std::array<std::tuple<key_type, value_type>, kBranchingFactor> _storage;

    using storage_iter_type = decltype(std::begin(_storage));

    auto storage_begin() {
        return std::begin(_storage);
    }

    auto storage_end() {
        return storage_begin() + _size;
    }

    key_type lowest_key() {
        return std::get<0>(*storage_begin());
    }

    btree* _owner;

    static bool item_comparator(const item_type& rhs, const item_type& lhs) {
        return std::get<0>(rhs) < std::get<0>(lhs);
    }
};

class iterator : public std::iterator<std::bidirectional_iterator_tag,
                                      item_type> {
public:
    iterator() = default;
    iterator(leaf_node* node, leaf_node::storage_iter_type storage_iter) :
        _node(node), _storage_iter(storage_iter) {}

    item_type& operator*();
    item_type* operator->();

    iterator operator++();
    iterator operator++(int);
    iterator operator--();
    iterator operator--(int);

    friend bool operator==(const iterator& rhs, const iterator& lhs);
    friend bool operator!=(const iterator& rhs, const iterator& lhs);
private:
    void check_valid();

    auto tie() const { return std::tie(_node, _storage_iter); }

    leaf_node* _node = nullptr;
    leaf_node::storage_iter_type _storage_iter = nullptr;
};

class internal_node : public node {
    friend class leaf_node;
public:
    iterator insert(key_type key, value_type value) final;
    iterator search(key_type key) final;

    iterator begin() final;

private:
    void insert_node(key_type lowest_key, std::unique_ptr<node> node);

    internal_node* _next = nullptr;
    internal_node* _prev = nullptr;

    node* _parent = nullptr;

    std::size_t _size = 0;

    using internal_item_type = std::tuple<key_type, std::unique_ptr<node>>;

    std::array<internal_item_type, kBranchingFactor> _storage;

    static bool internal_item_comparator(const internal_item_type& lhs,
                                         const internal_item_type& rhs) {
        return std::get<0>(lhs) < std::get<0>(rhs);
    }

    auto storage_begin() { return std::begin(_storage); }
    auto storage_end() { return storage_begin() + _size; }
};

class btree {
    friend class leaf_node;
    friend class internal_node;
public:
    iterator insert(key_type key, value_type value);
    iterator search(key_type key);

    iterator end();
    iterator begin();
private:
    std::unique_ptr<node> _root = std::make_unique<leaf_node>(this);
};
