#pragma once

#include "table.hpp"

#include <string>

namespace minisql {

enum class StatementType {
    Empty,
    Invalid,
    Quit,
    ShowDatabases,
    ShowTables,
    CreateDatabase,
    DropDatabase,
    UseDatabase,
    CreateTable,
    DropTable,
    Insert,
    Select,
    DeleteRows,
    Update,
};

struct Statement {
    StatementType type = StatementType::Empty;
    std::string error;
    std::string database;
    std::string table;
    Array<Column> columns;
    Array<Value> values;
    std::string selected_column;
    Condition condition;
    std::string set_column;
    Value set_value;
};

class SqlParser {
public:
    [[nodiscard]] Statement parse(const std::string& input) const;

private:
    [[nodiscard]] Value parse_literal(const std::string& raw, bool& ok) const;
    bool parse_condition(const std::string& text, Condition& condition, std::string& error) const;
    [[nodiscard]] Statement parse_create_table(const std::string& sql) const;
};

}  // namespace minisql
