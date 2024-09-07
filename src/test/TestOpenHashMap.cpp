//
// Created by Grady Schofield on 7/22/24.
//

#include<catch2/catch_test_macros.hpp>

#include<chrono>
#include<filesystem>
#include<string>
#include<unordered_map>
#include<unordered_set>

#include"gradylib/AltIntHash.hpp"
#include"gradylib/OpenHashMap.hpp"
#include"gradylib/MMapS2IOpenHashMap.hpp"
#include"gradylib/MMapI2SOpenHashMap.hpp"

using namespace std;
namespace fs = std::filesystem;

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

TEST_CASE("MMapI2SOpenHashMap operator[]") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string> m;
    m[0] = "abc";
    m[3] = "def";
    m[4] = "ghi";
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int> m2(tmpFile);
    REQUIRE(m2.size() == m.size());
    REQUIRE(string(m2[0]) == "abc");
    REQUIRE(string(m2[3]) == "def");
    REQUIRE(string(m2[4]) == "ghi");
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapI2SOpenHashMap contains") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string> m;
    m[0] = "abc";
    m[3] = "def";
    m[4] = "ghi";
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int> m2(tmpFile);
    REQUIRE(m2.contains(0));
    REQUIRE(m2.contains(3));
    REQUIRE(m2.contains(4));
    REQUIRE(!m2.contains(1));
    REQUIRE(!m2.contains(2));
    REQUIRE(!m2.contains(5));
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapI2SOpenHashMap iterator") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string> m;
    m[0] = "abc";
    m[3] = "def";
    m[4] = "ghi";
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int> m2(tmpFile);
    auto iter = m2.begin();
    while (iter != m2.end()) {
        REQUIRE(m.contains(iter.key()));
        REQUIRE(string(iter.value()) == m[iter.key()]);
        ++iter;
    }
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapI2SOpenHashMap move constructor") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string> m;
    m[0] = "abc";
    m[3] = "def";
    m[4] = "ghi";
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int> m2(tmpFile);
    gradylib::MMapI2SOpenHashMap<int> m3(move(m2));
    REQUIRE(m3.size() == m.size());
    REQUIRE(string(m3[0]) == "abc");
    REQUIRE(string(m3[3]) == "def");
    REQUIRE(string(m3[4]) == "ghi");
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapI2SOpenHashMap move assignment") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string> m;
    m[0] = "abc";
    m[3] = "def";
    m[4] = "ghi";
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int> m2(tmpFile);
    gradylib::MMapI2SOpenHashMap<int> m3;
    m3 = move(m2);
    REQUIRE(m3.size() == m.size());
    REQUIRE(string(m3[0]) == "abc");
    REQUIRE(string(m3[3]) == "def");
    REQUIRE(string(m3[4]) == "ghi");
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapI2SOpenHashMap clone") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string> m;
    m[0] = "abc";
    m[3] = "def";
    m[4] = "ghi";
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int> m2(tmpFile);
    gradylib::OpenHashMap<int, string> m3(m2.clone());
    REQUIRE(m3.size() == m.size());
    REQUIRE(string(m3[0]) == "abc");
    REQUIRE(string(m3[3]) == "def");
    REQUIRE(string(m3[4]) == "ghi");
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapI2SOpenHashMap operator[] throws on empty map") {
    gradylib::MMapI2SOpenHashMap<int> m;
    REQUIRE_THROWS(m[1] == "abc");
}

TEST_CASE("MMapI2SOpenHashMap operator[] throws on invalid key") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string> m;
    m[0] = "abc";
    m[3] = "def";
    m[4] = "ghi";
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int> m2(tmpFile);
    REQUIRE_THROWS(m2[1] == "abc");
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapI2SOpenHashMap operator[] throws for removed elements") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string> m;
    m[0] = "abc";
    m[3] = "def";
    m[4] = "ghi";
    m.erase(4);
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int> m2(tmpFile);
    REQUIRE(m2.size() == m.size());
    REQUIRE_THROWS(string(m2[4]) == "ghi");
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapI2SOpenHashMap operator[] throws (testing a case where the loop finishes)") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string> m;
    m.reserve(10);
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int> m2(tmpFile);
    REQUIRE(m2.size() == m.size());
    REQUIRE_THROWS(string(m2[4]) == "ghi");
    filesystem::remove(tmpFile);
}

template<typename IntType>
struct IdentityHash {
    size_t operator()(IntType const &i) const noexcept {
        return i;
    }
};

/*
 * Using IdentityHash in a few of the following test ensures certain lines are covered in the code
 */

TEST_CASE("MMapI2SOpenHashMap contains on empty map") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string, IdentityHash> m;
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int, IdentityHash> m2(tmpFile);
    REQUIRE(m2.size() == m.size());
    REQUIRE(!m2.contains(0));
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapI2SOpenHashMap contains on removed element") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string, IdentityHash> m;
    m[0] = "abc";
    m[3] = "def";
    m[4] = "ghi";
    m.erase(4);
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int, IdentityHash> m2(tmpFile);
    REQUIRE(m2.size() == m.size());
    REQUIRE(!m2.contains(4));
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapI2SOpenHashMap iterator operator++ loop") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string, IdentityHash> m;
    m[0] = "abc";
    m[1] = "def";
    m[2] = "ghi";
    m[3] = "ghi";
    m.erase(1);
    m.erase(2);
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int, IdentityHash> m2(tmpFile);
    REQUIRE(m2.size() == m.size());
    auto iter = m2.begin();
    while (iter != m2.end()) {
        REQUIRE(m.contains(iter.key()));
        REQUIRE(m[iter.key()] == string(iter.value()));
        ++iter;
    }
    auto copyIter = iter;
    ++iter;
    REQUIRE(copyIter == iter);
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapI2SOpenHashMap begin on empty map") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string, IdentityHash> m;
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int, IdentityHash> m2(tmpFile);
    REQUIRE(m2.size() == m.size());
    auto iter = m2.begin();
    auto copyIter = iter;
    ++iter;
    REQUIRE(copyIter == iter);
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapI2SOpenHashMap begin required to scan") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string, IdentityHash> m;
    m[0] = "abc";
    m[1] = "def";
    m[2] = "ghi";
    m[3] = "ghi";
    m.erase(0);
    m.erase(1);
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapI2SOpenHashMap<int, IdentityHash> m2(tmpFile);
    REQUIRE(m2.size() == m.size());
    auto iter = m2.begin();
    while (iter != m2.end()) {
        REQUIRE(m.contains(iter.key()));
        REQUIRE(m[iter.key()] == string(iter.value()));
        ++iter;
    }
    auto copyIter = iter;
    ++iter;
    REQUIRE(copyIter == iter);
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapI2SOpenHashMap open nonexistent file throws") {
    REQUIRE_THROWS(gradylib::MMapI2SOpenHashMap<int, IdentityHash>("non existent file"));
}

TEST_CASE("MMapI2SOpenHashMap throw on mmap failure") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<int, string, IdentityHash> m;
    m[0] = "abc";
    m[1] = "def";
    m[2] = "ghi";
    m[3] = "ghi";
    gradylib::writeMappable(tmpFile, m);
    gradylib::GRADY_LIB_MOCK_MMapI2SOpenHashMap_MMAP<int, IdentityHash>();
    REQUIRE_THROWS(gradylib::MMapI2SOpenHashMap<int, IdentityHash>(tmpFile));
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap move constructor") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    m["abc"] = 0;
    m["def"] = 3;
    m["ghi"] = 4;
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    gradylib::MMapS2IOpenHashMap<int> m3(move(m2));
    REQUIRE(m3.size() == m.size());
    REQUIRE(m3["abc"] == 0);
    REQUIRE(m3["def"] == 3);
    REQUIRE(m3["ghi"] == 4);
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap move assignment") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    m["abc"] = 0;
    m["def"] = 3;
    m["ghi"] = 4;
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    gradylib::MMapS2IOpenHashMap<int> m3;
    m3 = move(m2);
    REQUIRE(m3.size() == m.size());
    REQUIRE(m3["abc"] == 0);
    REQUIRE(m3["def"] == 3);
    REQUIRE(m3["ghi"] == 4);
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap open nonexistent file throws") {
    REQUIRE_THROWS(gradylib::MMapS2IOpenHashMap<int>("non existent file"));
}

TEST_CASE("MMapS2IOpenHashMap throw on mmap failure") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    m["abc"] = 0;
    m["def"] = 3;
    m["ghi"] = 4;
    gradylib::writeMappable(tmpFile, m);
    gradylib::GRADY_LIB_MOCK_MMapS2IOpenHashMap_MMAP<int>();
    REQUIRE_THROWS(gradylib::MMapS2IOpenHashMap<int>(tmpFile));
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap operator[] throws on nonexistent element") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    m["abc"] = 0;
    m["def"] = 3;
    m["ghi"] = 4;
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    REQUIRE_THROWS(m2["jkl"]);
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap operator[] throws on empty map") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    REQUIRE_THROWS(m2["jkl"]);
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap operator[] throw on nonexistent element that was removed") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    m["abc"] = 0;
    m["def"] = 3;
    m["ghi"] = 4;
    m.erase("def");
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    REQUIRE_THROWS(m2["def"]);
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap operator[] throws (testing a case where the loop finishes)") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    m.reserve(10);
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    REQUIRE(m2.size() == m.size());
    REQUIRE_THROWS(m2["ghi"]);
    filesystem::remove(tmpFile);
}
TEST_CASE("MMapS2IOpenHashMap contains returns false on empty map") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    REQUIRE(!m2.contains("ghi"));
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap contains returns false for removed element") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    m["abc"] = 0;
    m.erase("abc");
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    REQUIRE(!m2.contains("abc"));
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap contains returns false all wasSet bits set") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    m.reserve(10);
    gradylib::GRADY_LIB_MOCK_OpenHashMap_SET_SECOND_BITS(m);
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    REQUIRE(!m2.contains("abc"));
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap contains returns false (finish loop)") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    m.reserve(10);
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    REQUIRE(!m2.contains("abc"));
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap iterator") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    m["abc"] = 0;
    m["def"] = 3;
    m["ghi"] = 4;
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    auto iter = m2.begin();
    while (iter != m2.end()) {
        REQUIRE(m.contains(iter.key()));
        REQUIRE(iter.value() == m[iter.key()]);
        ++iter;
    }
    auto iterCopy = iter;
    ++iter;
    REQUIRE(iterCopy == iter);
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap begin on empty map") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    auto iter = m2.begin();
    while (iter != m2.end()) {
        REQUIRE(m.contains(iter.key()));
        REQUIRE(iter.value() == m[iter.key()]);
        ++iter;
    }
    auto iterCopy = iter;
    ++iter;
    REQUIRE(iterCopy == iter);
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap begin requiring scan") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    m["abc"] = 0;
    m["def"] = 3;
    m["ghi"] = 4;

    m.erase("abc");
    m.erase("def");
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    auto iter = m2.begin();
    while (iter != m2.end()) {
        REQUIRE(m.contains(iter.key()));
        REQUIRE(iter.value() == m[iter.key()]);
        ++iter;
    }
    auto iterCopy = iter;
    ++iter;
    REQUIRE(iterCopy == iter);
    filesystem::remove(tmpFile);
}

TEST_CASE("MMapS2IOpenHashMap clone") {
    fs::path tmpPath = filesystem::temp_directory_path();
    fs::path tmpFile = tmpPath / "map.bin";
    gradylib::OpenHashMap<string, int> m;
    m["abc"] = 0;
    m["def"] = 3;
    m["ghi"] = 4;
    gradylib::writeMappable(tmpFile, m);
    gradylib::MMapS2IOpenHashMap<int> m2(tmpFile);
    gradylib::OpenHashMap<string, int> m3(m2.clone());
    REQUIRE(m3.size() == m.size());
    REQUIRE(0 == m3["abc"]);
    REQUIRE(3 == m3["def"]);
    REQUIRE(4 == m3["ghi"]);
    filesystem::remove(tmpFile);
}

