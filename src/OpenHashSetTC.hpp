//
// Created by Grady Schofield on 7/20/24.
//

/*
 * In the context of simulations, I've needed to duplicate very large unordered_set's.  Creating and destroying
 * unordered_sets is quite slow.  Creation can be made 10x faster when the keys are trivially copyable if we
 * implement the set as an open address hash set by using memcpy to do the work.  Deletion is 100x faster.
 *
 * Interface:
 * ---------
 * insert
 * contains
 * erase
 * reserve
 * size
 * begin
 * end
 */

#ifndef GRADY_LIB_OPENHASHSETTC_HPP
#define GRADY_LIB_OPENHASHSETTC_HPP

#include<type_traits>
#include<vector>

#include<BitPairSet.hpp>

template<typename Key>
requires std::is_trivially_copyable_v<Key> && std::is_default_constructible_v<Key>
class OpenHashSetTC {

    std::vector<Key> keys;
    BitPairSet setFlags;
    double loadFactor = 0.8;
    double growthFactor = 1.2;
    size_t setSize = 0;

    void rehash(size_t size = 0) {
        size_t newSize;
        if (size > 0) {
            if (size < setSize) {
                return;
            }
            newSize = size / loadFactor;
        } else {
            newSize = std::max<size_t>(keys.size() + 1, std::max<size_t>(1, keys.size()) * growthFactor);
        }
        std::vector<Key> newKeys(newSize);
        BitPairSet newSetFlags(newSize);
        for (size_t i = 0; i < keys.size(); ++i) {
            if (!setFlags.isFirstSet(i)) {
                continue;
            }
            Key const & k = keys[i];
            size_t hash = std::hash<Key>{}(k);
            size_t idx = hash % newKeys.size();
            while (newSetFlags.isFirstSet(idx)) {
                ++idx;
                idx = idx == newKeys.size() ? 0 : idx;
            }
            newSetFlags.setBoth(idx);
            newKeys[idx] = k;
        }
        std::swap(keys, newKeys);
        std::swap(setFlags, newSetFlags);
    }

public:

    void insert(Key const & key) {
        size_t hash = std::hash<Key>{}(key);
        size_t idx = hash % keys.size();
        bool doesContain = false;
        size_t firstUnsetIdx = -1;
        bool isFirstUnsetIdxSet = false;
        size_t startIdx = idx;
        for (auto [isSet, wasSet] = setFlags[idx]; isSet || wasSet; std::tie(isSet, wasSet) = setFlags[idx]) {
            if (!isFirstUnsetIdxSet && !isSet) {
                firstUnsetIdx = idx;
                isFirstUnsetIdxSet = true;
            }
            if (isSet && keys[idx] == key) {
                doesContain = true;
                break;
            }
            if (wasSet && keys[idx] == key) {
                doesContain = false;
                break;
            }
            ++idx;
            idx = idx == keys.size() ? 0 : idx;
            if (startIdx == idx) break;
        }
        if (doesContain) {
            return;
        }
        if (setSize >= keys.size() * loadFactor) {
            rehash();
            hash = std::hash<Key>{}(key);
            idx = hash % keys.size();
            startIdx = idx;
            while (setFlags.isFirstSet(idx)) {
                ++idx;
                idx = idx == keys.size() ? 0 : idx;
                if (startIdx == idx) break;
            }
        } else {
            idx = isFirstUnsetIdxSet ? firstUnsetIdx : idx;
        }
        setFlags.setBoth(idx);
        keys[idx] = key;
        ++setSize;
    }

    bool contains(Key const & key) {
        size_t hash = std::hash<Key>{}(key);
        size_t idx = hash % keys.size();
        size_t startIdx = idx;
        for (auto [isSet, wasSet] = setFlags[idx]; isSet || wasSet; std::tie(isSet, wasSet) = setFlags[idx]) {
            if (isSet && keys[idx] == key) {
                return true;
            }
            if (wasSet && keys[idx] == key) {
                return false;
            }
            ++idx;
            idx = idx == keys.size() ? 0 : idx;
            if (startIdx == idx) break;
        }
        return false;
    }

    void erase(Key const & key) {
        size_t hash = std::hash<Key>{}(key);
        size_t idx = hash % keys.size();
        size_t startIdx = idx;
        for (auto [isSet, wasSet] = setFlags[idx]; isSet || wasSet; std::tie(isSet, wasSet) = setFlags[idx]) {
            if (keys[idx] == key) {
                if (isSet) {
                    --setSize;
                    setFlags.unsetFirst(idx);
                }
                return;
            }
            ++idx;
            idx = idx == keys.size() ? 0 : idx;
            if (startIdx == idx) break;
        }
        return;
    }

    void reserve(size_t size) {
        rehash(size);
    }

    class iterator {
        size_t idx;
        OpenHashSetTC * container;
    public:
        iterator(size_t idx, OpenHashSetTC * container)
            : idx(idx), container(container)
        {
        }

        bool operator==(iterator const & other) const {
            return idx == other.idx;
        }

        bool operator!=(iterator const & other) const {
            return idx != other.idx;
        }

        Key const & operator*() const {
            return container->keys[idx];
        }

        iterator & operator++() {
            if (idx == container->keys.size()) {
                return *this;
            }
            ++idx;
            while (idx < container->keys.size() && !container->setFlags.isFirstSet(idx)) {
                ++idx;
            }
            return *this;
        }
    };

    iterator begin() {
        if (setSize == 0) {
            return iterator(keys.size(), this);
        }
        size_t idx = 0;
        while (idx < keys.size() && !setFlags.isFirstSet(idx)) {
            ++idx;
        }
        return iterator(idx, this);
    }

    iterator end() {
        return iterator(keys.size(), this);
    }

    size_t size() const {
        return setSize;
    }
};

#endif
