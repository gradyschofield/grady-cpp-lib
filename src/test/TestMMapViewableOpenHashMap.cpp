//
// Created by Grady Schofield on 8/3/24.
//
#include<catch2/catch_test_macros.hpp>

#include<fstream>
#include<span>
#include<vector>

#include<gradylib/MMapViewableOpenHashMap.hpp>

using namespace gradylib;
using namespace std;

struct Ser {
    vector<int> x;

    void serialize(ofstream & ofs) const {
        size_t n = x.size();
        ofs.write(static_cast<char*>(static_cast<void*>(&n)), sizeof(n));
        ofs.write(static_cast<char*>(static_cast<void *>(const_cast<int*>(x.data()))), 4 * n);
    }

    struct View {
        std::span<int const> x;
    };

    static decltype(auto) makeView(std::byte const * ptr) {
        size_t n = *static_cast<size_t const *>(static_cast<void const *>(ptr));
        ptr += sizeof(size_t);
        return View{std::span(static_cast<int const *>(static_cast<void const *>(ptr)), n)};
    }
};

//It's necessary to put serialize in the std namespace so the function can be found by argument dependent lookup
namespace std {
    void serialize(ofstream &ofs, vector<int> const & v) {
        size_t n = v.size();
        ofs.write(static_cast<char*>(static_cast<void*>(&n)), sizeof(n));
        ofs.write(static_cast<char*>(static_cast<void *>(const_cast<int*>(v.data()))), sizeof(int) * n);
    }

    template<typename Value>
    decltype(auto) makeView(std::byte const * ptr);

    template<>
    decltype(auto) makeView<vector<int>>(std::byte const * ptr) {
        size_t n = *static_cast<size_t const *>(static_cast<void const *>(ptr));
        ptr += sizeof(size_t);
        return std::span(static_cast<int const *>(static_cast<void const *>(ptr)), n);
    }
}

TEST_CASE("Memory mapped viewable object open hash map") {
    MMapViewableOpenHashMap<int, vector<int>>::Builder z;

    z.put(4, vector<int>{1, 2, 3});

    z.write("viewable.bin");

    MMapViewableOpenHashMap<int, vector<int>> dz("viewable.bin");

    REQUIRE(dz.contains(4));
    auto view = dz.at(4);
    REQUIRE(view.size() == 3);
    for (int i = 0; i < 3; ++i) {
        REQUIRE(view[i] == i+1);
    }

    MMapViewableOpenHashMap<int, Ser>::Builder z2;
    z2.put(5, Ser{{1,2,3}});

    z2.write("viewable2.bin");

    MMapViewableOpenHashMap<int, Ser> dz2("viewable2.bin");

    REQUIRE(dz2.contains(5));
    auto view2 = dz2.at(5);
    REQUIRE(view2.x.size() == 3);
    for (int i = 0; i < 3; ++i) {
        REQUIRE(view2.x[i] == i+1);
    }
}