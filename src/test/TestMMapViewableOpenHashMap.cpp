//
// Created by Grady Schofield on 8/3/24.
//

#include<fstream>
#include<span>
#include<vector>

#include<MMapViewableOpenHashMap.hpp>

using namespace gradylib;
using namespace std;

struct Ser {
    vector<int> x;

    void serialize(ofstream & ofs) const {
        size_t n = x.size();
        ofs.write(static_cast<char*>(static_cast<void*>(&n)), sizeof(n));
        ofs.write(static_cast<char*>(static_cast<void *>(const_cast<int*>(x.data()))), 4 * n);
    }

    std::span<int const> makeView(std::byte const * ptr) {
        size_t n = *static_cast<size_t const *>(static_cast<void const *>(ptr));
        ptr += sizeof(size_t);
        return std::span(static_cast<int const *>(static_cast<void const *>(ptr)), n);
    }
};

//It's necessary to put serialize in the std namespace so the function can be found by argument dependent lookup
namespace std {
    void serialize(ofstream &ofs, vector<int> const & v) {
        size_t n = v.size();
        ofs.write(static_cast<char*>(static_cast<void*>(&n)), sizeof(n));
        ofs.write(static_cast<char*>(static_cast<void *>(const_cast<int*>(v.data()))), 4 * n);
    }

    template<typename T>
    std::span<T const> makeView(std::byte const * ptr) {
        size_t n = *static_cast<size_t const *>(static_cast<void const *>(ptr));
        ptr += sizeof(size_t);
        return std::span(static_cast<int const *>(static_cast<void const *>(ptr)), n);
    }
}

int main( ) {
    MMapViewableOpenHashMap<int, vector<int>>::Builder z;

    z.put(4, vector<int>{1, 2, 3});

    MMapViewableOpenHashMap<int, Ser>::Builder z2;
    z2.put(5, Ser{{1,2,3}});

    return 0;
}