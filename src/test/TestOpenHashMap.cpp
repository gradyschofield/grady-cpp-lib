//
// Created by Grady Schofield on 7/22/24.
//

#include<catch2/catch_test_macros.hpp>

#include<chrono>
#include<string>
#include<unordered_map>
#include<unordered_set>

#include"gradylib/AltIntHash.hpp"
#include"gradylib/OpenHashMap.hpp"
#include"gradylib/MMapS2IOpenHashMap.hpp"
#include"gradylib/MMapI2SOpenHashMap.hpp"

using namespace std;


TEST_CASE("Open hash map") {
    auto randString = []() {
        int len = rand() % 12 + 3;
        vector<char> buffer(len);
        for (int i = 0; i < len; ++i) {
            buffer[i] = rand() % (1 + 126-32) + 32;
        }
        return string(buffer.data(), len);
    };
    long mapSize = 1E6;
    gradylib::OpenHashMap<string, long> map;
    map.reserve(mapSize);
    vector<string> strs;
    strs.reserve(mapSize);
    auto startTime = chrono::high_resolution_clock::now();
    for (long i = 0; i < mapSize; ++i) {
        strs.push_back(randString());
    }
    auto endTime = chrono::high_resolution_clock::now();
    cout << "string create time " << chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count() << " ms\n";

    startTime = chrono::high_resolution_clock::now();
    long idx = 0;
    for (long i = 0; i < mapSize; ++i) {
        map[strs[i]] = idx++;
    }
    endTime = chrono::high_resolution_clock::now();
    cout << "build time " << chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count() << " ms\n";
    cout << "Map size " << map.size() << "\n";

    unordered_map<string, long> test;
    startTime = chrono::high_resolution_clock::now();
    idx = 0;
    for (long i = 0; i < mapSize; ++i) {
        test[strs[i]] = idx++;
    }
    endTime = chrono::high_resolution_clock::now();
    cout << "unordered_map build time " << chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count() << " ms\n";

    startTime = chrono::high_resolution_clock::now();
    for (auto & p : test) {
        REQUIRE(map.contains(p.first));
        REQUIRE(map[p.first] == p.second);
    }
    REQUIRE(test.size() == map.size());
    endTime = chrono::high_resolution_clock::now();
    cout << "OpenHashMap check " << chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count() << " ms\n";

    gradylib::writeMappable("stringmap.bin", map);

    gradylib::MMapS2IOpenHashMap<long> map2("stringmap.bin");

    startTime = chrono::high_resolution_clock::now();
    for (auto & p : test) {
        REQUIRE(map2.contains(p.first));
        REQUIRE(map2[p.first] == p.second);
    }
    REQUIRE(test.size() == map2.size());
    endTime = chrono::high_resolution_clock::now();
    cout << "MMapS2IOpenHashMap check " << chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count() << " ms\n";

    int num = 0;
    gradylib::OpenHashMap<long, string, gradylib::AltIntHash> sidx;
    //sidx.reserve(map2.size());
    for (auto [str, idx] : map2) {
        //sidx.put(idx, string(str));
        sidx[idx] = string(str);
    }

    REQUIRE(sidx.size() == map2.size());

    for (auto && [idx, str] : sidx) {
        REQUIRE(map2.contains(str));
        REQUIRE(map2[str] == idx);
    }


    gradylib::OpenHashMap<long, string> i2s;
    i2s.reserve(map.size());
    unordered_map<long, string> testI2s;
    for (auto && [str, idx] : map) {
        i2s.put(idx, str);
        testI2s[idx] = str;
    }

    gradylib::writeMappable("i2s.bin", i2s);

    gradylib::MMapI2SOpenHashMap<long> i2sLoaded("i2s.bin");

    REQUIRE(testI2s.size() == i2sLoaded.size());

    for (auto && [idx, str] : testI2s) {
        REQUIRE(i2s.contains(idx));
        REQUIRE(i2s[idx] == str);
    }

}