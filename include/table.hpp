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
    BPlusTreeIndex() = default;
    BPlusTreeIndex(const BPlusTreeIndex& other);
    BPlusTreeIndex(BPlusTreeIndex&& other) noexcept;
    ~BPlusTreeIndex();

    BPlusTreeIndex& operator=(const BPlusTreeIndex& other);
    BPlusTreeIndex& operator=(BPlusTreeIndex&& other) noexcept;

    bool insert(const Value& key, std::size_t row_id);
    bool remove(const Value& key);
    bool find(const Value& key, std::size_t& row_id) const;
    void clear();

private:
    struct Node {
        bool leaf = true;
        Array<Value> keys;
        Array<Node*> children;
        Array<std::size_t> values;
        Node* next = nullptr;
    };

    struct SplitResult {
        bool split = false;
        Value pivot;
        Node* right = nullptr;
    };

    struct RemoveResult {
        bool removed = false;
    };

    static constexpr std::size_t kMaxKeys = 4;
    static constexpr std::size_t kMinKeys = 2;

    [[nodiscard]] Node* clone_tree(const Node* node, Node*& previous_leaf, Node*& first_leaf) const;
    void destroy_tree(Node* node);
    [[nodiscard]] Node* find_leaf(const Value& key) const;
    [[nodiscard]] SplitResult insert_recursive(Node* node, const Value& key, std::size_t row_id);
    [[nodiscard]] SplitResult split_leaf(Node* leaf);
    [[nodiscard]] SplitResult split_internal(Node* node);
    [[nodiscard]] bool is_underflow(const Node* node) const;
    [[nodiscard]] Value subtree_first_key(const Node* node) const;
    [[nodiscard]] Node* leftmost_leaf(Node* node) const;
    void rebuild_internal_keys(Node* node);
    void rebuild_all_internal_keys(Node* node);
    void copy_from(const BPlusTreeIndex& other);
    void collect_entries(const Node* node, Array<IndexEntry>& entries) const;

    Node* root_ = nullptr;
    Node* leaf_head_ = nullptr;
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
