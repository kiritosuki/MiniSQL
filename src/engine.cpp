#include "engine.hpp"

#include "util.hpp"

#include <iomanip>
#include <sstream>

namespace minisql {

Engine::Engine(const std::filesystem::path& data_dir) : storage_(data_dir) {
    storage_.ensure_root();
}

bool Engine::require_database(ResultSet& result) const {
    if (current_database_.empty()) {
        result.ok = false;
        result.message = "no database selected";
        return false;
    }
    return true;
}

bool Engine::coerce_value(Value& value, ColumnType type, std::string& error) const {
    if (value.type == type) {
        if (type == ColumnType::String && value.string_value.size() > 256) {
            error = "string literal exceeds 256 characters";
            return false;
        }
        return true;
    }
    error = "value type does not match column type";
    return false;
}

ResultSet Engine::execute(const std::string& sql) {
    Statement stmt = parser_.parse(sql);
    if (stmt.type == StatementType::Empty) {
        ResultSet result;
        result.message = "";
        return result;
    }
    if (stmt.type == StatementType::Invalid) {
        ResultSet result;
        result.ok = false;
        result.message = stmt.error;
        return result;
    }
    return execute_statement(stmt);
}

ResultSet Engine::execute_statement(const Statement& stmt) {
    ResultSet result;
    std::string error;
    switch (stmt.type) {
        case StatementType::CreateDatabase:
            result.ok = storage_.create_database(stmt.database, error);
            result.message = result.ok ? "database created" : error;
            return result;
        case StatementType::DropDatabase:
            result.ok = storage_.drop_database(stmt.database, error);
            if (result.ok && current_database_ == stmt.database) {
                current_database_.clear();
            }
            result.message = result.ok ? "database dropped" : error;
            return result;
        case StatementType::UseDatabase:
            if (!storage_.database_exists(stmt.database)) {
                result.ok = false;
                result.message = "database does not exist";
            } else {
                current_database_ = stmt.database;
                result.message = "database changed";
            }
            return result;
        case StatementType::CreateTable: {
            if (!require_database(result)) return result;
            Table table;
            table.name = stmt.table;
            table.columns = stmt.columns;
            result.ok = storage_.create_table(current_database_, table, error);
            result.message = result.ok ? "table created" : error;
            return result;
        }
        case StatementType::DropTable:
            if (!require_database(result)) return result;
            result.ok = storage_.drop_table(current_database_, stmt.table, error);
            result.message = result.ok ? "table dropped" : error;
            return result;
        case StatementType::Insert: {
            if (!require_database(result)) return result;
            Table table;
            if (!storage_.load_table(current_database_, stmt.table, table, error)) {
                result.ok = false;
                result.message = error;
                return result;
            }
            if (stmt.values.size() != table.columns.size()) {
                result.ok = false;
                result.message = "column count does not match";
                return result;
            }
            Row row;
            for (std::size_t i = 0; i < stmt.values.size(); ++i) {
                Value value = stmt.values[i];
                if (!coerce_value(value, table.columns[i].type, error)) {
                    result.ok = false;
                    result.message = error;
                    return result;
                }
                row.values.push_back(value);
            }
            if (!table.insert_row(row, error)) {
                result.ok = false;
                result.message = error;
                return result;
            }
            storage_.save_table(current_database_, table, error);
            result.message = "1 row inserted";
            return result;
        }
        case StatementType::Select: {
            if (!require_database(result)) return result;
            Table table;
            if (!storage_.load_table(current_database_, stmt.table, table, error)) {
                result.ok = false;
                result.message = error;
                return result;
            }
            Condition cond = stmt.condition;
            if (cond.enabled) {
                int idx = table.column_index(cond.column);
                if (idx < 0 || !coerce_value(cond.value, table.columns[static_cast<std::size_t>(idx)].type, error)) {
                    result.ok = false;
                    result.message = idx < 0 ? "unknown column: " + cond.column : error;
                    return result;
                }
            }
            return table.select_rows(stmt.selected_column, cond);
        }
        case StatementType::DeleteRows: {
            if (!require_database(result)) return result;
            Table table;
            if (!storage_.load_table(current_database_, stmt.table, table, error)) {
                result.ok = false;
                result.message = error;
                return result;
            }
            Condition cond = stmt.condition;
            if (cond.enabled) {
                int idx = table.column_index(cond.column);
                if (idx < 0 || !coerce_value(cond.value, table.columns[static_cast<std::size_t>(idx)].type, error)) {
                    result.ok = false;
                    result.message = idx < 0 ? "unknown column: " + cond.column : error;
                    return result;
                }
            }
            std::size_t count = table.delete_rows(cond);
            storage_.save_table(current_database_, table, error);
            result.message = std::to_string(count) + " row(s) deleted";
            return result;
        }
        case StatementType::Update: {
            if (!require_database(result)) return result;
            Table table;
            if (!storage_.load_table(current_database_, stmt.table, table, error)) {
                result.ok = false;
                result.message = error;
                return result;
            }
            int set_idx = table.column_index(stmt.set_column);
            if (set_idx < 0) {
                result.ok = false;
                result.message = "unknown column: " + stmt.set_column;
                return result;
            }
            Value set_value = stmt.set_value;
            if (!coerce_value(set_value, table.columns[static_cast<std::size_t>(set_idx)].type, error)) {
                result.ok = false;
                result.message = error;
                return result;
            }
            Condition cond = stmt.condition;
            if (cond.enabled) {
                int idx = table.column_index(cond.column);
                if (idx < 0 || !coerce_value(cond.value, table.columns[static_cast<std::size_t>(idx)].type, error)) {
                    result.ok = false;
                    result.message = idx < 0 ? "unknown column: " + cond.column : error;
                    return result;
                }
            }
            std::size_t count = table.update_rows(stmt.set_column, set_value, cond, error);
            if (!error.empty()) {
                result.ok = false;
                result.message = error;
                return result;
            }
            storage_.save_table(current_database_, table, error);
            result.message = std::to_string(count) + " row(s) updated";
            return result;
        }
        case StatementType::ShowDatabases: {
            result.columns.push_back("Database");
            Array<std::string> names = storage_.list_databases();
            for (const std::string& name : names) {
                Row row;
                Value value;
                value.type = ColumnType::String;
                value.string_value = name;
                row.values.push_back(value);
                result.rows.push_back(row);
            }
            result.message = std::to_string(result.rows.size()) + " row(s)";
            return result;
        }
        case StatementType::ShowTables: {
            if (!require_database(result)) return result;
            result.columns.push_back("Tables_in_" + current_database_);
            Array<std::string> names = storage_.list_tables(current_database_);
            for (const std::string& name : names) {
                Row row;
                Value value;
                value.type = ColumnType::String;
                value.string_value = name;
                row.values.push_back(value);
                result.rows.push_back(row);
            }
            result.message = std::to_string(result.rows.size()) + " row(s)";
            return result;
        }
        case StatementType::Quit:
            result.message = "bye";
            return result;
        default:
            result.ok = false;
            result.message = "unsupported statement";
            return result;
    }
}

std::string serialize_result(const ResultSet& result) {
    std::ostringstream out;
    out << (result.ok ? "OK" : "ERR") << "\n";
    out << result.message << "\n";
    out << result.columns.size() << "\n";
    for (const std::string& column : result.columns) {
        out << escape_field(column) << "\n";
    }
    out << result.rows.size() << "\n";
    for (const Row& row : result.rows) {
        for (std::size_t i = 0; i < row.values.size(); ++i) {
            if (i > 0) out << "|";
            out << escape_field(row.values[i].to_string());
        }
        out << "\n";
    }
    return out.str();
}

ResultSet deserialize_result(const std::string& payload) {
    std::istringstream in(payload);
    ResultSet result;
    std::string line;
    std::getline(in, line);
    result.ok = line == "OK";
    std::getline(in, result.message);
    std::getline(in, line);
    int column_count = std::stoi(line.empty() ? "0" : line);
    for (int i = 0; i < column_count; ++i) {
        std::getline(in, line);
        result.columns.push_back(unescape_field(line));
    }
    std::getline(in, line);
    int row_count = std::stoi(line.empty() ? "0" : line);
    for (int r = 0; r < row_count; ++r) {
        std::getline(in, line);
        Array<std::string> parts = split_csv(line);
        if (column_count == 1) {
            parts.clear();
            parts.push_back(unescape_field(line));
        }
        Row row;
        if (column_count > 1) {
            parts = Array<std::string>();
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
        }
        for (std::size_t i = 0; i < parts.size(); ++i) {
            Value value;
            value.type = ColumnType::String;
            value.string_value = parts[i];
            row.values.push_back(value);
        }
        result.rows.push_back(row);
    }
    return result;
}

std::string format_result(const ResultSet& result) {
    std::ostringstream out;
    if (!result.ok) {
        out << "ERROR: " << result.message << "\n";
        return out.str();
    }
    if (!result.columns.empty()) {
        for (std::size_t i = 0; i < result.columns.size(); ++i) {
            out << (i == 0 ? "| " : " | ") << result.columns[i];
        }
        out << " |\n";
        for (std::size_t i = 0; i < result.columns.size(); ++i) {
            out << "+";
            std::size_t width = result.columns[i].size() + 2;
            for (std::size_t j = 0; j < width; ++j) out << "-";
        }
        out << "+\n";
        for (const Row& row : result.rows) {
            for (std::size_t i = 0; i < row.values.size(); ++i) {
                out << (i == 0 ? "| " : " | ") << row.values[i].to_string();
            }
            out << " |\n";
        }
    }
    if (!result.message.empty()) {
        out << result.message << "\n";
    }
    return out.str();
}

} // namespace minisql
