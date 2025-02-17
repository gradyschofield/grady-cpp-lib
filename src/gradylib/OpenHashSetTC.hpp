/*
MIT License

Copyright (c) 2024 Grady Schofield

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*
 * Copying and destroying unordered_sets is quite slow.  Copying can be made faster when the keys are trivially
 * copyable if we implement the set as an open address hash set and use memcpy to do the work.  A set of
 * 100 million ints is 30x faster to copy and deletion is 150x faster.
 *
 * The 'write' method writes to disk in a format amenable to memory mapped loading.  Any write operations
 * on a set that's been loaded from disk will cause a copy. If read-only operations are used exclusively,
 * then no copy is ever created.
 *
 * TODO: byte ordering on disk IO
 *
 * Interface:
 * ---------
 * OpenHashSetTC(filename)
 * insert
 * contains
 * erase
 * reserve
 * size
 * begin
 * end
 * write
 */

#pragma once

#include<fcntl.h>
#include<sys/mman.h>
#include<unistd.h>

#include<cstddef>
#include<filesystem>
#include<fstream>
#include<type_traits>
#include<utility>
#include<vector>

#include"AltIntHash.hpp"
#include"BitPairSet.hpp"

namespace gradylib {

    template<typename Key, template<typename> typename HashFunction = gradylib::AltHash>
    requires std::is_trivially_copyable_v<Key> && std::is_default_constructible_v<Key> && std::equality_comparable<Key>
    class OpenHashSetTC {

        Key *keys = nullptr;
        BitPairSet setFlags;
        double loadFactor = 0.8;
        double growthFactor = 1.2;
        size_t keySize = 0;
        size_t setSize = 0;
        int fd = -1;
        size_t mappingSize = 0;
        void *memoryMapping = nullptr;
        bool readOnly = false;
        HashFunction<Key> hashFunction = HashFunction<Key>{};
        static inline void* (*mmapFunc)(void *, size_t, int, int, int, off_t) = mmap;

        void freeResources() {
            if (memoryMapping) {
                munmap(memoryMapping, mappingSize);
                close(fd);
                memoryMapping = nullptr;
                fd = -1;
            } else {
                delete[] keys;
                keys = nullptr;
            }
        }

        void rehash(size_t size = 0) {
            size_t newSize;
            if (size > 0) {
                if (size < setSize) {
                    return;
                }
                newSize = size / loadFactor;
            } else {
                newSize = std::max<size_t>(keySize + 1, std::max<size_t>(1, keySize) * growthFactor);
            }
            Key *newKeys = new Key[newSize];
            BitPairSet newSetFlags(newSize);
            for (size_t i = 0; i < keySize; ++i) {
                if (!setFlags.isFirstSet(i)) {
                    continue;
                }
                Key const &k = keys[i];
                size_t hash = hashFunction(k);
                size_t idx = hash % newSize;
                while (newSetFlags.isFirstSet(idx)) {
                    ++idx;
                    idx = idx == newSize ? 0 : idx;
                }
                newSetFlags.setBoth(idx);
                newKeys[idx] = k;
            }
            delete[] keys;
            keys = newKeys;
            keySize = newSize;
            std::swap(setFlags, newSetFlags);
        }

    public:
        typedef Key key_type;
        typedef Key value_type;

        OpenHashSetTC() = default;

        OpenHashSetTC(OpenHashSetTC const &s)
                : keys(new Key[s.keySize]), keySize(s.keySize), setFlags(s.setFlags), loadFactor(s.loadFactor),
                  growthFactor(s.growthFactor), setSize(s.setSize) {
            memcpy(keys, s.keys, sizeof(Key) * keySize);
        }

        OpenHashSetTC(OpenHashSetTC &&s)
                : keys(s.keys), keySize(s.keySize), setFlags(std::move(s.setFlags)), loadFactor(s.loadFactor),
                  growthFactor(s.growthFactor), setSize(s.setSize) {
            s.keys = nullptr;
            s.keySize = 0;
            s.setSize = 0;
        }

        explicit OpenHashSetTC(std::string filename) {
            fd = open(filename.c_str(), O_RDONLY);
            if (fd < 0) {
                std::ostringstream sstr;
                sstr << "Error opening file " << filename;
                throw gradylibMakeException(sstr.str());
            }
            mappingSize = std::filesystem::file_size(filename);
            memoryMapping = mmapFunc(0, mappingSize, PROT_READ, MAP_SHARED, fd, 0);
            if (memoryMapping == MAP_FAILED) {
                std::ostringstream sstr;
                sstr << "memory map failed: " << strerror(errno);
                throw gradylibMakeException(sstr.str());
            }
            std::byte *ptr = static_cast<std::byte *>(memoryMapping);
            setSize = *static_cast<size_t *>(static_cast<void *>(ptr));
            ptr += 8;
            keySize = *static_cast<size_t *>(static_cast<void *>(ptr));
            ptr += 8;
            loadFactor = *static_cast<double *>(static_cast<void *>(ptr));
            ptr += 8;
            growthFactor = *static_cast<double *>(static_cast<void *>(ptr));
            ptr += 8;
            size_t bitPairSetOffset = *static_cast<size_t *>(static_cast<void *>(ptr));
            ptr += 8;
            keys = static_cast<Key *>(static_cast<void *>(ptr));
            setFlags = BitPairSet(static_cast<void *>(bitPairSetOffset + static_cast<std::byte *>(memoryMapping)));
            readOnly = true;
        }

        explicit OpenHashSetTC(std::ifstream & ifs) {
            ifs.read(static_cast<char*>(static_cast<void*>(&setSize)), sizeof(setSize));
            ifs.read(static_cast<char*>(static_cast<void*>(&keySize)), sizeof(keySize));
            ifs.read(static_cast<char*>(static_cast<void*>(&loadFactor)), sizeof(loadFactor));
            ifs.read(static_cast<char*>(static_cast<void*>(&growthFactor)), sizeof(growthFactor));
            size_t bitPairSetOffset;
            ifs.read(static_cast<char*>(static_cast<void*>(&bitPairSetOffset)), sizeof(bitPairSetOffset));
            keys = new Key[keySize];
            ifs.read(static_cast<char*>(static_cast<void*>(keys)), sizeof(Key) * keySize);
            ifs.seekg(bitPairSetOffset);
            setFlags = BitPairSet(ifs);
        }

        ~OpenHashSetTC() {
            freeResources();
        }

        OpenHashSetTC &operator=(OpenHashSetTC const &s) {
            if (s.readOnly) {
                std::ostringstream ostr;
                ostr << "Can't copy readonly OpenHashSetTC";
                throw gradylibMakeException(ostr.str());
            }
            if (this == &s) {
                return *this;
            }
            freeResources();
            keys = new Key[s.keySize];
            memcpy(keys, s.keys, sizeof(Key) * s.keySize);
            keySize = s.keySize;
            setFlags = s.setFlags;
            loadFactor = s.loadFactor;
            growthFactor = s.growthFactor;
            setSize = s.setSize;
            return *this;
        }

        OpenHashSetTC &operator=(OpenHashSetTC &&s) noexcept {
            keys = s.keys;
            keySize = s.keySize;
            setFlags = s.setFlags;
            loadFactor = s.loadFactor;
            growthFactor = s.growthFactor;
            setSize = s.setSize;
            readOnly = s.readOnly;
            fd = s.fd;
            memoryMapping = s.memoryMapping;
            mappingSize = s.mappingSize;

            s.keys = nullptr;
            s.keySize = 0;
            s.setSize = 0;
            s.fd = -1;
            s.memoryMapping = nullptr;
            s.mappingSize = 0;
            return *this;
        }

        template<typename KeyType>
        requires std::is_convertible_v<KeyType, Key>
        void insert(KeyType && keyArg) {
            Key key{keyArg};
            if (readOnly) {
                std::ostringstream sstr;
                sstr << "Cannot modify set";
                throw gradylibMakeException(sstr.str());
            }
            size_t hash;
            size_t idx;
            size_t startIdx;
            bool doesContain = false;
            size_t firstUnsetIdx = -1;
            bool isFirstUnsetIdxSet = false;
            if (keySize > 0) {
                hash = hashFunction(key);
                idx = hash % keySize;
                startIdx = idx;
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
                    idx = idx == keySize ? 0 : idx;
                    if (startIdx == idx) break;
                }
            }
            if (doesContain) {
                return;
            }
            if (setSize >= keySize * loadFactor) {
                rehash();
                hash = hashFunction(key);
                idx = hash % keySize;
                startIdx = idx;
                while (setFlags.isFirstSet(idx)) {
                    ++idx;
                    idx = idx == keySize ? 0 : idx;
                    if (startIdx == idx) break;
                }
            } else {
                idx = isFirstUnsetIdxSet ? firstUnsetIdx : idx;
            }
            setFlags.setBoth(idx);
            keys[idx] = key;
            ++setSize;
        }

        bool contains(Key const &key) const {
            if (keySize == 0) return false;
            size_t hash = hashFunction(key);
            size_t idx = hash % keySize;
            size_t startIdx = idx;
            for (auto [isSet, wasSet] = setFlags[idx]; isSet || wasSet; std::tie(isSet, wasSet) = setFlags[idx]) {
                if (isSet && keys[idx] == key) {
                    return true;
                }
                if (wasSet && keys[idx] == key) {
                    return false;
                }
                ++idx;
                idx = idx == keySize ? 0 : idx;
                if (startIdx == idx) break;
            }
            return false;
        }

        void erase(Key const &key) {
            if (readOnly) {
                std::ostringstream sstr;
                sstr << "Cannot modify mmap";
                throw gradylibMakeException(sstr.str());
            }
            size_t hash = hashFunction(key);
            size_t idx = hash % keySize;
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
                idx = idx == keySize ? 0 : idx;
                if (startIdx == idx) break;
            }
        }

        void reserve(size_t size) {
            if (readOnly) {
                std::ostringstream sstr;
                sstr << "Cannot modify set";
                throw gradylibMakeException(sstr.str());
            }
            rehash(size);
        }

        class const_iterator {
            size_t idx;
            OpenHashSetTC const * container;
        public:
            const_iterator(size_t idx, OpenHashSetTC const *container)
                    : idx(idx), container(container) {
            }

            bool operator==(const_iterator const &other) const {
                return idx == other.idx && container == other.container;
            }

            bool operator!=(const_iterator const &other) const {
                return idx != other.idx || container != other.container;
            }

            Key const & operator*() const {
                return container->keys[idx];
            }

            const_iterator & operator++() {
                if (idx == container->keySize) {
                    return *this;
                }
                ++idx;
                while (idx < container->keySize && !container->setFlags.isFirstSet(idx)) {
                    ++idx;
                }
                return *this;
            }
        };

        const_iterator begin() const {
            if (setSize == 0) {
                return const_iterator(keySize, this);
            }
            size_t idx = 0;
            while (idx < keySize && !setFlags.isFirstSet(idx)) {
                ++idx;
            }
            return const_iterator(idx, this);
        }

        const_iterator end() const {
            return const_iterator(keySize, this);
        }

        size_t size() const {
            return setSize;
        }

        void clear() {
            if (readOnly) {
                std::ostringstream sstr;
                sstr << "Can't clear a readonly set";
                throw gradylibMakeException(sstr.str());
            }
            setFlags.clear();
            setSize = 0;
        }

        /*
         * 8 set size
         * 8 key size
         * 8 load factor
         * 8 growth factor
         * 8 file offset from beg for the BitPairSet
         * sizeof(Key)*key size
         * (possible 4 byte pad, use offset above to skip over it)
         * 8 bit pair set size
         * 8 underlying array size
         * 4 * underlying array size
         */
        void write(std::filesystem::path filename, int alignment = alignof(void*)) {
            std::ofstream ofs(filename, std::ios::binary);
            ofs.write((char *) &setSize, 8);
            ofs.write((char *) &keySize, 8);
            ofs.write((char *) &loadFactor, 8);
            ofs.write((char *) &growthFactor, 8);
            size_t keyArraySize = sizeof(Key) * keySize;
            size_t bitPairSetOffset = ofs.tellp();
            int padLength = 0;
            if (keyArraySize % 8 == 0) {
                bitPairSetOffset += 8 + keyArraySize;
            } else {
                padLength = 8 - keyArraySize % 8;
                bitPairSetOffset += 8 + keyArraySize + padLength;
            }
            ofs.write((char *) &bitPairSetOffset, 8);
            ofs.write((char *) keys, sizeof(Key) * keySize);
            if (padLength > 0) {
                for (int i = 0; i < padLength; ++i) {
                    char pad = 0;
                    ofs.write((char *) &pad, 1);
                }
            }
            setFlags.write(ofs);
        }

        template<typename, template<typename> typename>
        friend void GRADY_LIB_MOCK_OpenHashSetTC_MMAP();

        template<typename, template<typename> typename>
        friend void GRADY_LIB_DEFAULT_OpenHashSetTC_MMAP();
    };

    template<typename Key, template<typename> typename HashFunction = gradylib::AltHash>
    void GRADY_LIB_MOCK_OpenHashSetTC_MMAP() {
        OpenHashSetTC<Key, HashFunction>::mmapFunc = [](void *, size_t, int, int, int, off_t) -> void *{
            return MAP_FAILED;
        };
    }

    template<typename Key, template<typename> typename HashFunction = gradylib::AltHash>
    void GRADY_LIB_DEFAULT_OpenHashSetTC_MMAP() {
        OpenHashSetTC<Key, HashFunction>::mmapFunc = mmap;
    }
}

