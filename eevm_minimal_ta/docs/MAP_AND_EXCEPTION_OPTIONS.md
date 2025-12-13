// SPDX-License-Identifier: BSD-2-Clause
/*
 * SortedVectorMap - std::map replacement using sorted std::vector
 * 
 * NO EXCEPTIONS NEEDED!
 * Uses binary search (std::lower_bound) for O(log n) lookup
 * Insert/erase are O(n) but acceptable for small datasets (<1000 items)
 */

#ifndef SORTED_VECTOR_MAP_H
#define SORTED_VECTOR_MAP_H

#include <vector>
#include <algorithm>
#include <utility>  // std::pair

template<typename Key, typename Value>
class SortedVectorMap {
private:
    std::vector<std::pair<Key, Value>> data_;
    
    // Helper: Find position for key using binary search
    typename std::vector<std::pair<Key, Value>>::iterator 
    find_position(const Key& key) {
        return std::lower_bound(
            data_.begin(), 
            data_.end(), 
            key,
            [](const std::pair<Key, Value>& p, const Key& k) {
                return p.first < k;
            }
        );
    }
    
    typename std::vector<std::pair<Key, Value>>::const_iterator 
    find_position(const Key& key) const {
        return std::lower_bound(
            data_.begin(), 
            data_.end(), 
            key,
            [](const std::pair<Key, Value>& p, const Key& k) {
                return p.first < k;
            }
        );
    }

public:
    // Types (STL-compatible)
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<Key, Value>;
    using iterator = typename std::vector<value_type>::iterator;
    using const_iterator = typename std::vector<value_type>::const_iterator;
    
    // Constructor
    SortedVectorMap() = default;
    
    // Size operations
    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    
    // Clear all elements
    void clear() { data_.clear(); }
    
    // Insert or update
    // Returns: {iterator, was_inserted}
    std::pair<iterator, bool> insert(const Key& key, const Value& value) {
        auto it = find_position(key);
        
        if (it != data_.end() && it->first == key) {
            // Key exists, update value
            it->second = value;
            return {it, false};
        } else {
            // Key doesn't exist, insert
            auto new_it = data_.insert(it, {key, value});
            return {new_it, true};
        }
    }
    
    // Insert pair (STL-compatible)
    std::pair<iterator, bool> insert(const value_type& pair) {
        return insert(pair.first, pair.second);
    }
    
    // operator[] for map-like access
    Value& operator[](const Key& key) {
        auto it = find_position(key);
        
        if (it != data_.end() && it->first == key) {
            return it->second;
        } else {
            // Insert with default value
            auto new_it = data_.insert(it, {key, Value{}});
            return new_it->second;
        }
    }
    
    // Find element
    iterator find(const Key& key) {
        auto it = find_position(key);
        if (it != data_.end() && it->first == key)
            return it;
        return data_.end();
    }
    
    const_iterator find(const Key& key) const {
        auto it = find_position(key);
        if (it != data_.end() && it->first == key)
            return it;
        return data_.end();
    }
    
    // Count occurrences (0 or 1 for map)
    size_t count(const Key& key) const {
        auto it = find_position(key);
        return (it != data_.end() && it->first == key) ? 1 : 0;
    }
    
    // Erase element by key
    // Returns: number of elements erased (0 or 1)
    size_t erase(const Key& key) {
        auto it = find_position(key);
        if (it != data_.end() && it->first == key) {
            data_.erase(it);
            return 1;
        }
        return 0;
    }
    
    // Erase element by iterator
    iterator erase(iterator it) {
        return data_.erase(it);
    }
    
    // Iterators
    iterator begin() { return data_.begin(); }
    iterator end() { return data_.end(); }
    const_iterator begin() const { return data_.begin(); }
    const_iterator end() const { return data_.end(); }
    const_iterator cbegin() const { return data_.begin(); }
    const_iterator cend() const { return data_.end(); }
    
    // Reserve capacity (optimization)
    void reserve(size_t capacity) { data_.reserve(capacity); }
};

#endif // SORTED_VECTOR_MAP_H
