#include "table.hpp"

#include <iostream>

static int failures = 0;

static void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

int main() {
    minisql::BPlusTreeIndex index;

    for (int i = 1; i <= 12; ++i) {
        minisql::Value key;
        key.type = minisql::ColumnType::Int;
        key.int_value = i;
        expect(index.insert(key, static_cast<std::size_t>(i * 10)), "insert key");
    }

    for (int i = 1; i <= 12; ++i) {
        minisql::Value key;
        key.type = minisql::ColumnType::Int;
        key.int_value = i;
        std::size_t row_id = 0;
        expect(index.find(key, row_id), "find inserted key");
        expect(row_id == static_cast<std::size_t>(i * 10), "row id matches");
    }

    for (int value : {3, 4, 5, 6, 7, 8, 9}) {
        minisql::Value key;
        key.type = minisql::ColumnType::Int;
        key.int_value = value;
        expect(index.remove(key), "remove existing key");
    }

    for (int value : {1, 2, 10, 11, 12}) {
        minisql::Value key;
        key.type = minisql::ColumnType::Int;
        key.int_value = value;
        std::size_t row_id = 0;
        expect(index.find(key, row_id), "remaining key still found");
    }

    for (int value : {3, 4, 5, 6, 7, 8, 9}) {
        minisql::Value key;
        key.type = minisql::ColumnType::Int;
        key.int_value = value;
        std::size_t row_id = 0;
        expect(!index.find(key, row_id), "removed key no longer found");
    }

    for (int value : {1, 2, 10, 11, 12}) {
        minisql::Value key;
        key.type = minisql::ColumnType::Int;
        key.int_value = value;
        expect(index.remove(key), "remove remaining key");
    }

    minisql::Value missing;
    missing.type = minisql::ColumnType::Int;
    missing.int_value = 42;
    std::size_t row_id = 0;
    expect(!index.find(missing, row_id), "empty tree does not find key");
    expect(!index.remove(missing), "remove missing key from empty tree");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "all bplustree delete tests passed\n";
    return 0;
}
