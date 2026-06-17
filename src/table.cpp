#include "table.hpp"

#include <cstdlib>

namespace minisql {

std::string Value::to_string() const {
    if (type == ColumnType::Int) {
        return std::to_string(int_value);
    }
    return string_value;
}

int compare_values(const Value& left, const Value& right) {
    if (left.type == ColumnType::Int && right.type == ColumnType::Int) {
        if (left.int_value < right.int_value) return -1;
        if (left.int_value > right.int_value) return 1;
        return 0;
    }
    if (left.string_value < right.string_value) return -1;
    if (left.string_value > right.string_value) return 1;
    return 0;
}

bool BPlusTreeIndex::insert(const Value& key, std::size_t row_id) {
    if (root_ == nullptr) {
        root_ = new Node();
        root_->leaf = true;
        root_->keys.push_back(key);
        root_->values.push_back(row_id);
        leaf_head_ = root_;
        return true;
    }

    std::size_t existing = 0;
    if (find(key, existing)) {
        return false;
    }

    SplitResult split = insert_recursive(root_, key, row_id);
    if (split.split) {
        Node* next_root = new Node();
        next_root->leaf = false;
        next_root->keys.push_back(split.pivot);
        next_root->children.push_back(root_);
        next_root->children.push_back(split.right);
        root_ = next_root;
    }
    return true;
}

bool BPlusTreeIndex::remove(const Value& key) {
    if (root_ == nullptr) {
        return false;
    }

    Array<Node*> path;
    Array<std::size_t> child_indices;

    Node* node = root_;
    while (node != nullptr && !node->leaf) {
        std::size_t child = 0;
        while (child < node->keys.size() && compare_values(key, node->keys[child]) >= 0) {
            ++child;
        }
        path.push_back(node);
        child_indices.push_back(child);
        node = node->children[child];
    }

    if (node == nullptr) {
        return false;
    }

    std::size_t pos = 0;
    while (pos < node->keys.size() && compare_values(node->keys[pos], key) < 0) {
        ++pos;
    }
    if (pos >= node->keys.size() || compare_values(node->keys[pos], key) != 0) {
        return false;
    }

    node->keys.erase(pos);
    node->values.erase(pos);

    if (node == root_) {
        if (root_->leaf && root_->keys.empty()) {
            delete root_;
            root_ = nullptr;
            leaf_head_ = nullptr;
        } else {
            rebuild_all_internal_keys(root_);
        }
        return true;
    }

    for (std::size_t level = path.size(); level > 0; --level) {
        Node* parent = path[level - 1];
        std::size_t child_index = child_indices[level - 1];
        Node* child = parent->children[child_index];

        if (!is_underflow(child)) {
            rebuild_internal_keys(parent);
            continue;
        }

        Node* left = child_index > 0 ? parent->children[child_index - 1] : nullptr;
        Node* right = child_index + 1 < parent->children.size() ? parent->children[child_index + 1] : nullptr;

        if (child->leaf) {
            if (left != nullptr && left->keys.size() > kMinKeys) {
                Array<Value> next_keys;
                Array<std::size_t> next_values;
                next_keys.push_back(left->keys[left->keys.size() - 1]);
                next_values.push_back(left->values[left->values.size() - 1]);
                for (std::size_t i = 0; i < child->keys.size(); ++i) {
                    next_keys.push_back(child->keys[i]);
                    next_values.push_back(child->values[i]);
                }
                left->keys.erase(left->keys.size() - 1);
                left->values.erase(left->values.size() - 1);
                child->keys = std::move(next_keys);
                child->values = std::move(next_values);
                rebuild_internal_keys(parent);
                continue;
            }

            if (right != nullptr && right->keys.size() > kMinKeys) {
                child->keys.push_back(right->keys[0]);
                child->values.push_back(right->values[0]);
                right->keys.erase(0);
                right->values.erase(0);
                rebuild_internal_keys(parent);
                continue;
            }

            if (left != nullptr) {
                for (std::size_t i = 0; i < child->keys.size(); ++i) {
                    left->keys.push_back(child->keys[i]);
                    left->values.push_back(child->values[i]);
                }
                left->next = child->next;
                parent->children.erase(child_index);
                parent->keys.erase(child_index - 1);
                if (leaf_head_ == child) {
                    leaf_head_ = left;
                }
                delete child;
            } else if (right != nullptr) {
                for (std::size_t i = 0; i < right->keys.size(); ++i) {
                    child->keys.push_back(right->keys[i]);
                    child->values.push_back(right->values[i]);
                }
                child->next = right->next;
                parent->children.erase(child_index + 1);
                parent->keys.erase(child_index);
                if (leaf_head_ == right) {
                    leaf_head_ = child;
                }
                delete right;
            }
        } else {
            if (left != nullptr && left->children.size() > kMinKeys + 1) {
                Array<Value> next_keys;
                Array<Node*> next_children;

                next_children.push_back(left->children[left->children.size() - 1]);
                for (std::size_t i = 0; i < child->children.size(); ++i) {
                    next_children.push_back(child->children[i]);
                }

                next_keys.push_back(parent->keys[child_index - 1]);
                for (std::size_t i = 0; i < child->keys.size(); ++i) {
                    next_keys.push_back(child->keys[i]);
                }

                parent->keys[child_index - 1] = left->keys[left->keys.size() - 1];
                left->children.erase(left->children.size() - 1);
                left->keys.erase(left->keys.size() - 1);
                child->children = std::move(next_children);
                child->keys = std::move(next_keys);
                rebuild_internal_keys(parent);
                continue;
            }

            if (right != nullptr && right->children.size() > kMinKeys + 1) {
                child->keys.push_back(parent->keys[child_index]);
                child->children.push_back(right->children[0]);
                parent->keys[child_index] = right->keys[0];
                right->children.erase(0);
                right->keys.erase(0);
                rebuild_internal_keys(parent);
                continue;
            }

            if (left != nullptr) {
                left->keys.push_back(parent->keys[child_index - 1]);
                for (std::size_t i = 0; i < child->keys.size(); ++i) {
                    left->keys.push_back(child->keys[i]);
                }
                for (Node* grandchild : child->children) {
                    left->children.push_back(grandchild);
                }
                parent->children.erase(child_index);
                parent->keys.erase(child_index - 1);
                delete child;
                rebuild_internal_keys(left);
            } else if (right != nullptr) {
                child->keys.push_back(parent->keys[child_index]);
                for (std::size_t i = 0; i < right->keys.size(); ++i) {
                    child->keys.push_back(right->keys[i]);
                }
                for (Node* grandchild : right->children) {
                    child->children.push_back(grandchild);
                }
                parent->children.erase(child_index + 1);
                parent->keys.erase(child_index);
                delete right;
                rebuild_internal_keys(child);
            }
        }

        rebuild_internal_keys(parent);
    }

    if (root_ != nullptr && !root_->leaf && root_->children.size() == 1) {
        Node* next_root = root_->children[0];
        root_->children.clear();
        delete root_;
        root_ = next_root;
    }
    if (root_ != nullptr) {
        rebuild_all_internal_keys(root_);
        leaf_head_ = leftmost_leaf(root_);
    }
    if (root_ != nullptr && root_->leaf && root_->keys.empty()) {
        delete root_;
        root_ = nullptr;
        leaf_head_ = nullptr;
    }
    return true;
}

bool BPlusTreeIndex::find(const Value& key, std::size_t& row_id) const {
    Node* leaf = find_leaf(key);
    if (leaf == nullptr) {
        return false;
    }
    for (std::size_t i = 0; i < leaf->keys.size(); ++i) {
        int cmp = compare_values(leaf->keys[i], key);
        if (cmp == 0) {
            row_id = leaf->values[i];
            return true;
        }
        if (cmp > 0) {
            return false;
        }
    }
    return false;
}

BPlusTreeIndex::BPlusTreeIndex(const BPlusTreeIndex& other) {
    copy_from(other);
}

BPlusTreeIndex::BPlusTreeIndex(BPlusTreeIndex&& other) noexcept
    : root_(other.root_), leaf_head_(other.leaf_head_) {
    other.root_ = nullptr;
    other.leaf_head_ = nullptr;
}

BPlusTreeIndex::~BPlusTreeIndex() {
    clear();
}

BPlusTreeIndex& BPlusTreeIndex::operator=(const BPlusTreeIndex& other) {
    if (this != &other) {
        clear();
        copy_from(other);
    }
    return *this;
}

BPlusTreeIndex& BPlusTreeIndex::operator=(BPlusTreeIndex&& other) noexcept {
    if (this != &other) {
        clear();
        root_ = other.root_;
        leaf_head_ = other.leaf_head_;
        other.root_ = nullptr;
        other.leaf_head_ = nullptr;
    }
    return *this;
}

void BPlusTreeIndex::clear() {
    destroy_tree(root_);
    root_ = nullptr;
    leaf_head_ = nullptr;
}

BPlusTreeIndex::Node* BPlusTreeIndex::clone_tree(const Node* node, Node*& previous_leaf, Node*& first_leaf) const {
    if (node == nullptr) {
        return nullptr;
    }

    Node* copy = new Node();
    copy->leaf = node->leaf;
    for (const Value& key : node->keys) {
        copy->keys.push_back(key);
    }

    if (node->leaf) {
        for (std::size_t value : node->values) {
            copy->values.push_back(value);
        }
        if (previous_leaf != nullptr) {
            previous_leaf->next = copy;
        } else {
            first_leaf = copy;
        }
        previous_leaf = copy;
        return copy;
    }

    for (Node* child : node->children) {
        copy->children.push_back(clone_tree(child, previous_leaf, first_leaf));
    }
    return copy;
}

void BPlusTreeIndex::destroy_tree(Node* node) {
    if (node == nullptr) {
        return;
    }
    if (!node->leaf) {
        for (Node* child : node->children) {
            destroy_tree(child);
        }
    }
    delete node;
}

BPlusTreeIndex::Node* BPlusTreeIndex::find_leaf(const Value& key) const {
    Node* node = root_;
    while (node != nullptr && !node->leaf) {
        std::size_t child = 0;
        while (child < node->keys.size() && compare_values(key, node->keys[child]) >= 0) {
            ++child;
        }
        node = node->children[child];
    }
    return node;
}

BPlusTreeIndex::SplitResult BPlusTreeIndex::insert_recursive(Node* node, const Value& key, std::size_t row_id) {
    if (node->leaf) {
        std::size_t pos = 0;
        while (pos < node->keys.size() && compare_values(node->keys[pos], key) < 0) {
            ++pos;
        }

        Array<Value> next_keys;
        Array<std::size_t> next_values;
        for (std::size_t i = 0; i < pos; ++i) {
            next_keys.push_back(node->keys[i]);
            next_values.push_back(node->values[i]);
        }
        next_keys.push_back(key);
        next_values.push_back(row_id);
        for (std::size_t i = pos; i < node->keys.size(); ++i) {
            next_keys.push_back(node->keys[i]);
            next_values.push_back(node->values[i]);
        }
        node->keys = std::move(next_keys);
        node->values = std::move(next_values);

        if (node->keys.size() <= kMaxKeys) {
            return {};
        }
        return split_leaf(node);
    }

    std::size_t child = 0;
    while (child < node->keys.size() && compare_values(key, node->keys[child]) >= 0) {
        ++child;
    }

    SplitResult child_split = insert_recursive(node->children[child], key, row_id);
    if (!child_split.split) {
        return {};
    }

    Array<Value> next_keys;
    Array<Node*> next_children;
    for (std::size_t i = 0; i < child; ++i) {
        next_keys.push_back(node->keys[i]);
    }
    next_keys.push_back(child_split.pivot);
    for (std::size_t i = child; i < node->keys.size(); ++i) {
        next_keys.push_back(node->keys[i]);
    }

    for (std::size_t i = 0; i <= child; ++i) {
        next_children.push_back(node->children[i]);
    }
    next_children.push_back(child_split.right);
    for (std::size_t i = child + 1; i < node->children.size(); ++i) {
        next_children.push_back(node->children[i]);
    }

    node->keys = std::move(next_keys);
    node->children = std::move(next_children);

    if (node->keys.size() <= kMaxKeys) {
        return {};
    }
    return split_internal(node);
}

BPlusTreeIndex::SplitResult BPlusTreeIndex::split_leaf(Node* leaf) {
    Node* right = new Node();
    right->leaf = true;

    std::size_t mid = leaf->keys.size() / 2;
    Array<Value> left_keys;
    Array<std::size_t> left_values;

    for (std::size_t i = 0; i < mid; ++i) {
        left_keys.push_back(leaf->keys[i]);
        left_values.push_back(leaf->values[i]);
    }
    for (std::size_t i = mid; i < leaf->keys.size(); ++i) {
        right->keys.push_back(leaf->keys[i]);
        right->values.push_back(leaf->values[i]);
    }

    leaf->keys = std::move(left_keys);
    leaf->values = std::move(left_values);
    right->next = leaf->next;
    leaf->next = right;

    SplitResult result;
    result.split = true;
    result.pivot = right->keys[0];
    result.right = right;
    return result;
}

BPlusTreeIndex::SplitResult BPlusTreeIndex::split_internal(Node* node) {
    Node* right = new Node();
    right->leaf = false;

    std::size_t mid = node->keys.size() / 2;
    Value pivot = node->keys[mid];

    Array<Value> left_keys;
    Array<Node*> left_children;

    for (std::size_t i = 0; i < mid; ++i) {
        left_keys.push_back(node->keys[i]);
    }
    for (std::size_t i = mid + 1; i < node->keys.size(); ++i) {
        right->keys.push_back(node->keys[i]);
    }
    for (std::size_t i = 0; i <= mid; ++i) {
        left_children.push_back(node->children[i]);
    }
    for (std::size_t i = mid + 1; i < node->children.size(); ++i) {
        right->children.push_back(node->children[i]);
    }

    node->keys = std::move(left_keys);
    node->children = std::move(left_children);

    SplitResult result;
    result.split = true;
    result.pivot = pivot;
    result.right = right;
    return result;
}

bool BPlusTreeIndex::is_underflow(const Node* node) const {
    if (node == nullptr || node == root_) {
        return false;
    }
    return node->keys.size() < kMinKeys;
}

Value BPlusTreeIndex::subtree_first_key(const Node* node) const {
    const Node* current = node;
    while (current != nullptr && !current->leaf) {
        current = current->children[0];
    }
    return current->keys[0];
}

BPlusTreeIndex::Node* BPlusTreeIndex::leftmost_leaf(Node* node) const {
    Node* current = node;
    while (current != nullptr && !current->leaf) {
        current = current->children[0];
    }
    return current;
}

void BPlusTreeIndex::rebuild_internal_keys(Node* node) {
    if (node == nullptr || node->leaf) {
        return;
    }
    Array<Value> next_keys;
    for (std::size_t i = 1; i < node->children.size(); ++i) {
        next_keys.push_back(subtree_first_key(node->children[i]));
    }
    node->keys = std::move(next_keys);
}

void BPlusTreeIndex::rebuild_all_internal_keys(Node* node) {
    if (node == nullptr || node->leaf) {
        return;
    }
    for (Node* child : node->children) {
        rebuild_all_internal_keys(child);
    }
    rebuild_internal_keys(node);
}

void BPlusTreeIndex::copy_from(const BPlusTreeIndex& other) {
    Node* previous_leaf = nullptr;
    Node* first_leaf = nullptr;
    root_ = clone_tree(other.root_, previous_leaf, first_leaf);
    leaf_head_ = first_leaf;
}

void BPlusTreeIndex::collect_entries(const Node* node, Array<IndexEntry>& entries) const {
    if (node == nullptr) {
        return;
    }
    if (node->leaf) {
        for (std::size_t i = 0; i < node->keys.size(); ++i) {
            entries.push_back(IndexEntry{node->keys[i], node->values[i]});
        }
        return;
    }
    for (Node* child : node->children) {
        collect_entries(child, entries);
    }
}

int Table::column_index(const std::string& column_name) const {
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (columns[i].name == column_name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Table::primary_index() const {
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (columns[i].primary) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool Table::has_primary() const {
    return primary_index() >= 0;
}

const Column* Table::find_column(const std::string& column_name) const {
    int idx = column_index(column_name);
    return idx < 0 ? nullptr : &columns[static_cast<std::size_t>(idx)];
}

bool Table::indexed_row(const Condition& condition, std::size_t& row_id) const {
    int pidx = primary_index();
    if (!condition.enabled || condition.op != '=' || pidx < 0 || columns[static_cast<std::size_t>(pidx)].name != condition.column) {
        return false;
    }
    return primary_index_.find(condition.value, row_id);
}

void Table::rebuild_index() {
    primary_index_.clear();
    int pidx = primary_index();
    if (pidx < 0) {
        return;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        primary_index_.insert(rows[i].values[static_cast<std::size_t>(pidx)], i);
    }
}

bool Table::primary_exists(const Value& value) const {
    std::size_t row_id = 0;
    return primary_index_.find(value, row_id);
}

bool Table::insert_row(const Row& row, std::string& error) {
    if (row.values.size() != columns.size()) {
        error = "column count does not match";
        return false;
    }
    int pidx = primary_index();
    if (pidx >= 0 && primary_exists(row.values[static_cast<std::size_t>(pidx)])) {
        error = "duplicate primary key";
        return false;
    }
    rows.push_back(row);
    rebuild_index();
    return true;
}

bool Table::matches(const Row& row, const Condition& condition) const {
    if (!condition.enabled) {
        return true;
    }
    int idx = column_index(condition.column);
    if (idx < 0) {
        return false;
    }
    int cmp = compare_values(row.values[static_cast<std::size_t>(idx)], condition.value);
    if (condition.op == '=') return cmp == 0;
    if (condition.op == '<') return cmp < 0;
    if (condition.op == '>') return cmp > 0;
    return false;
}

std::size_t Table::delete_rows(const Condition& condition) {
    std::size_t indexed = 0;
    if (indexed_row(condition, indexed)) {
        rows.erase(indexed);
        rebuild_index();
        return 1;
    }
    std::size_t count = 0;
    for (std::size_t i = 0; i < rows.size();) {
        if (matches(rows[i], condition)) {
            rows.erase(i);
            ++count;
        } else {
            ++i;
        }
    }
    rebuild_index();
    return count;
}

std::size_t Table::update_rows(const std::string& column, const Value& value, const Condition& condition, std::string& error) {
    int idx = column_index(column);
    if (idx < 0) {
        error = "unknown column: " + column;
        return 0;
    }
    int pidx = primary_index();
    if (idx == pidx) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (!matches(rows[i], condition)) {
                continue;
            }
            for (std::size_t j = 0; j < rows.size(); ++j) {
                if (i != j && compare_values(rows[j].values[static_cast<std::size_t>(pidx)], value) == 0) {
                    error = "duplicate primary key";
                    return 0;
                }
            }
        }
    }
    std::size_t indexed = 0;
    if (indexed_row(condition, indexed)) {
        rows[indexed].values[static_cast<std::size_t>(idx)] = value;
        rebuild_index();
        return 1;
    }
    std::size_t count = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (matches(rows[i], condition)) {
            rows[i].values[static_cast<std::size_t>(idx)] = value;
            ++count;
        }
    }
    rebuild_index();
    return count;
}

ResultSet Table::select_rows(const std::string& column, const Condition& condition) const {
    ResultSet result;
    int selected = -1;
    if (column == "*") {
        for (const Column& col : columns) {
            result.columns.push_back(col.name);
        }
    } else {
        selected = column_index(column);
        if (selected < 0) {
            result.ok = false;
            result.message = "unknown column: " + column;
            return result;
        }
        result.columns.push_back(column);
    }
    std::size_t indexed = 0;
    if (indexed_row(condition, indexed)) {
        const Row& source = rows[indexed];
        Row out;
        if (column == "*") {
            for (const Value& value : source.values) {
                out.values.push_back(value);
            }
        } else {
            out.values.push_back(source.values[static_cast<std::size_t>(selected)]);
        }
        result.rows.push_back(out);
        result.message = "1 row(s)";
        return result;
    }
    for (const Row& source : rows) {
        if (!matches(source, condition)) {
            continue;
        }
        Row out;
        if (column == "*") {
            for (const Value& value : source.values) {
                out.values.push_back(value);
            }
        } else {
            out.values.push_back(source.values[static_cast<std::size_t>(selected)]);
        }
        result.rows.push_back(out);
    }
    result.message = std::to_string(result.rows.size()) + " row(s)";
    return result;
}

} // namespace minisql
