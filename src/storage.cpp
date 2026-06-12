#include "minisql/storage.hpp"

#include "minisql/util.hpp"

#include <fstream>

namespace minisql {

Storage::Storage(std::filesystem::path root) : root_(std::move(root)) {}

void Storage::ensure_root() const {
    std::filesystem::create_directories(root_);
}

std::filesystem::path Storage::database_path(const std::string& name) const {
    return root_ / name;
}

std::filesystem::path Storage::table_path(const std::string& database, const std::string& table) const {
    return database_path(database) / (table + ".dat");
}

bool Storage::create_database(const std::string& name, std::string& error) const {
    ensure_root();
    auto path = database_path(name);
    if (std::filesystem::exists(path)) {
        error = "database already exists";
        return false;
    }
    return std::filesystem::create_directories(path);
}

bool Storage::drop_database(const std::string& name, std::string& error) const {
    auto path = database_path(name);
    if (!std::filesystem::exists(path)) {
        error = "database does not exist";
        return false;
    }
    std::filesystem::remove_all(path);
    return true;
}

bool Storage::database_exists(const std::string& name) const {
    return std::filesystem::is_directory(database_path(name));
}

bool Storage::create_table(const std::string& database, const Table& table, std::string& error) const {
    if (!database_exists(database)) {
        error = "database does not exist";
        return false;
    }
    auto path = table_path(database, table.name);
    if (std::filesystem::exists(path)) {
        error = "table already exists";
        return false;
    }
    return save_table(database, table, error);
}

bool Storage::drop_table(const std::string& database, const std::string& table, std::string& error) const {
    auto path = table_path(database, table);
    if (!std::filesystem::exists(path)) {
        error = "table does not exist";
        return false;
    }
    std::filesystem::remove(path);
    auto idx = database_path(database) / (table + ".idx");
    if (std::filesystem::exists(idx)) {
        std::filesystem::remove(idx);
    }
    return true;
}

bool Storage::save_table(const std::string& database, const Table& table, std::string& error) const {
    auto path = table_path(database, table.name);
    std::ofstream out(path);
    if (!out) {
        error = "cannot open table for writing";
        return false;
    }
    out << "MINISQL_TABLE_V1\n";
    out << table.columns.size() << "\n";
    for (const Column& col : table.columns) {
        out << col.name << "|" << (col.type == ColumnType::Int ? "int" : "string") << "|" << (col.primary ? "1" : "0") << "\n";
    }
    out << table.rows.size() << "\n";
    for (const Row& row : table.rows) {
        for (std::size_t i = 0; i < row.values.size(); ++i) {
            if (i > 0) {
                out << "|";
            }
            out << escape_field(row.values[i].to_string());
        }
        out << "\n";
    }
    int pidx = table.primary_index();
    if (pidx >= 0) {
        std::ofstream idx(database_path(database) / (table.name + ".idx"));
        if (idx) {
            idx << "MINISQL_INDEX_V1\n";
            idx << table.columns[static_cast<std::size_t>(pidx)].name << "\n";
            for (std::size_t i = 0; i < table.rows.size(); ++i) {
                idx << escape_field(table.rows[i].values[static_cast<std::size_t>(pidx)].to_string()) << "|" << i << "\n";
            }
        }
    }
    return true;
}

static Array<std::string> split_storage_line(const std::string& line) {
    Array<std::string> parts;
    std::string current;
    bool escaping = false;
    for (char ch : line) {
        if (escaping) {
            current.push_back(ch == 'n' ? '\n' : ch);
            escaping = false;
        } else if (ch == '\\') {
            escaping = true;
        } else if (ch == '|') {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

bool Storage::load_table(const std::string& database, const std::string& table_name, Table& table, std::string& error) const {
    std::ifstream in(table_path(database, table_name));
    if (!in) {
        error = "table does not exist";
        return false;
    }
    std::string line;
    if (!std::getline(in, line) || line != "MINISQL_TABLE_V1") {
        error = "invalid table file";
        return false;
    }
    if (!std::getline(in, line)) {
        error = "invalid table file";
        return false;
    }
    int column_count = std::stoi(line);
    Table loaded;
    loaded.name = table_name;
    for (int i = 0; i < column_count; ++i) {
        if (!std::getline(in, line)) {
            error = "invalid table file";
            return false;
        }
        Array<std::string> parts = split_storage_line(line);
        if (parts.size() != 3) {
            error = "invalid table file";
            return false;
        }
        Column col;
        col.name = parts[0];
        col.type = parts[1] == "int" ? ColumnType::Int : ColumnType::String;
        col.primary = parts[2] == "1";
        loaded.columns.push_back(col);
    }
    if (!std::getline(in, line)) {
        error = "invalid table file";
        return false;
    }
    int row_count = std::stoi(line);
    for (int r = 0; r < row_count; ++r) {
        if (!std::getline(in, line)) {
            error = "invalid table file";
            return false;
        }
        Array<std::string> parts = split_storage_line(line);
        if (parts.size() != loaded.columns.size()) {
            error = "invalid row data";
            return false;
        }
        Row row;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            Value value;
            value.type = loaded.columns[i].type;
            if (value.type == ColumnType::Int) {
                value.int_value = std::stoi(parts[i]);
            } else {
                value.string_value = parts[i];
            }
            row.values.push_back(value);
        }
        loaded.rows.push_back(row);
    }
    loaded.rebuild_index();
    table = loaded;
    return true;
}

Array<std::string> Storage::list_databases() const {
    Array<std::string> names;
    ensure_root();
    for (const auto& entry : std::filesystem::directory_iterator(root_)) {
        if (entry.is_directory()) {
            names.push_back(entry.path().filename().string());
        }
    }
    return names;
}

Array<std::string> Storage::list_tables(const std::string& database) const {
    Array<std::string> names;
    auto path = database_path(database);
    if (!std::filesystem::is_directory(path)) {
        return names;
    }
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".dat") {
            names.push_back(entry.path().stem().string());
        }
    }
    return names;
}

} // namespace minisql
