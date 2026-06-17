#pragma once

#include "array.hpp"

#include <cstddef>
#include <string>

namespace minisql {

enum class ColumnType {
    Int,
    String,
};

struct Value {
    ColumnType type = ColumnType::Int;
    int int_value = 0;
    std::string string_value;

    [[nodiscard]] std::string to_string() const;
};

struct Column {
    std::string name;
    ColumnType type = ColumnType::Int;
    bool primary = false;
};

struct Row {
    Array<Value> values;
};

struct Condition {
    bool enabled = false;
    std::string column;
    char op = '=';
    Value value;
};

struct ResultSet {
    bool ok = true;
    std::string message;
    Array<std::string> columns;
    Array<Row> rows;
};

struct IndexEntry {
    Value key;
    std::size_t row_id = 0;
};

class BPlusTreeIndex {
public:
    bool insert(const Value& key, std::size_t row_id);
    bool remove(const Value& key);
    bool find(const Value& key, std::size_t& row_id) const;
    void clear() { entries_.clear(); }

private:
    Array<IndexEntry> entries_;
};

int compare_values(const Value& left, const Value& right);

class Table {
public:
    std::string name;
    Array<Column> columns;
    Array<Row> rows;

    [[nodiscard]] int column_index(const std::string& column_name) const;
    [[nodiscard]] int primary_index() const;
    [[nodiscard]] bool has_primary() const;
    [[nodiscard]] const Column* find_column(const std::string& column_name) const;
    [[nodiscard]] bool indexed_row(const Condition& condition, std::size_t& row_id) const;
    void rebuild_index();
    [[nodiscard]] bool primary_exists(const Value& value) const;
    bool insert_row(const Row& row, std::string& error);
    [[nodiscard]] bool matches(const Row& row, const Condition& condition) const;
    std::size_t delete_rows(const Condition& condition);
    std::size_t update_rows(const std::string& column, const Value& value, const Condition& condition, std::string& error);
    [[nodiscard]] ResultSet select_rows(const std::string& column, const Condition& condition) const;

private:
    BPlusTreeIndex primary_index_;
};

}  // namespace minisql
