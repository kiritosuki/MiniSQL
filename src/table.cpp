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
    std::size_t pos = 0;
    while (pos < entries_.size() && compare_values(entries_[pos].key, key) < 0) {
        ++pos;
    }
    if (pos < entries_.size() && compare_values(entries_[pos].key, key) == 0) {
        return false;
    }
    Array<IndexEntry> next;
    for (std::size_t i = 0; i < pos; ++i) {
        next.push_back(entries_[i]);
    }
    next.push_back(IndexEntry{key, row_id});
    for (std::size_t i = pos; i < entries_.size(); ++i) {
        next.push_back(entries_[i]);
    }
    entries_ = std::move(next);
    return true;
}

bool BPlusTreeIndex::remove(const Value& key) {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (compare_values(entries_[i].key, key) == 0) {
            entries_.erase(i);
            return true;
        }
    }
    return false;
}

bool BPlusTreeIndex::find(const Value& key, std::size_t& row_id) const {
    std::size_t left = 0;
    std::size_t right = entries_.size();
    while (left < right) {
        std::size_t mid = left + (right - left) / 2;
        int cmp = compare_values(entries_[mid].key, key);
        if (cmp == 0) {
            row_id = entries_[mid].row_id;
            return true;
        }
        if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    return false;
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
