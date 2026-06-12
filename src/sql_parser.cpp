#include "minisql/sql_parser.hpp"

#include "minisql/util.hpp"

#include <cctype>
#include <cstdlib>

namespace minisql {

static bool starts_with_word(const std::string& text, const std::string& prefix) {
    if (text.size() < prefix.size()) {
        return false;
    }
    return text.substr(0, prefix.size()) == prefix;
}

static std::string lower_outside_quotes(const std::string& input) {
    std::string out;
    bool quoted = false;
    for (char ch : input) {
        if (ch == '"') {
            quoted = !quoted;
            out.push_back(ch);
        } else if (quoted) {
            out.push_back(ch);
        } else {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    return out;
}

Value SqlParser::parse_literal(const std::string& raw, bool& ok) const {
    std::string text = trim(raw);
    ok = true;
    Value value;
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        value.type = ColumnType::String;
        value.string_value = text.substr(1, text.size() - 2);
        if (value.string_value.size() > 256) {
            ok = false;
        }
        return value;
    }
    char* end = nullptr;
    long parsed = std::strtol(text.c_str(), &end, 10);
    if (end != nullptr && *end == '\0') {
        value.type = ColumnType::Int;
        value.int_value = static_cast<int>(parsed);
        return value;
    }
    ok = false;
    return value;
}

bool SqlParser::parse_condition(const std::string& text, Condition& condition, std::string& error) const {
    std::string cond = trim(text);
    char op = 0;
    std::size_t pos = std::string::npos;
    for (char candidate : {'=', '<', '>'}) {
        pos = cond.find(candidate);
        if (pos != std::string::npos) {
            op = candidate;
            break;
        }
    }
    if (pos == std::string::npos) {
        error = "invalid where condition";
        return false;
    }
    condition.column = trim(cond.substr(0, pos));
    condition.op = op;
    bool ok = false;
    condition.value = parse_literal(cond.substr(pos + 1), ok);
    condition.enabled = true;
    if (!valid_identifier(condition.column) || !ok) {
        error = "invalid where condition";
        return false;
    }
    return true;
}

Statement SqlParser::parse_create_table(const std::string& sql) const {
    Statement stmt;
    stmt.type = StatementType::CreateTable;
    std::string prefix = "create table ";
    std::size_t open = sql.find('(', prefix.size());
    std::size_t close = sql.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close < open) {
        stmt.type = StatementType::Invalid;
        stmt.error = "invalid create table syntax";
        return stmt;
    }
    stmt.table = trim(sql.substr(prefix.size(), open - prefix.size()));
    if (!valid_identifier(stmt.table)) {
        stmt.type = StatementType::Invalid;
        stmt.error = "invalid table name";
        return stmt;
    }
    Array<std::string> defs = split_csv(sql.substr(open + 1, close - open - 1));
    bool has_primary = false;
    for (const std::string& def : defs) {
        Array<std::string> tokens;
        std::string current;
        for (char ch : def) {
            if (std::isspace(static_cast<unsigned char>(ch))) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(ch);
            }
        }
        if (!current.empty()) {
            tokens.push_back(current);
        }
        if (tokens.size() < 2 || tokens.size() > 3) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid column definition";
            return stmt;
        }
        Column column;
        column.name = tokens[0];
        if (!valid_identifier(column.name)) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid column name";
            return stmt;
        }
        std::string type = tokens[1];
        if (type == "int") {
            column.type = ColumnType::Int;
        } else if (type == "string") {
            column.type = ColumnType::String;
        } else {
            stmt.type = StatementType::Invalid;
            stmt.error = "unsupported column type";
            return stmt;
        }
        if (tokens.size() == 3) {
            if (tokens[2] != "primary" || has_primary) {
                stmt.type = StatementType::Invalid;
                stmt.error = "invalid primary key declaration";
                return stmt;
            }
            column.primary = true;
            has_primary = true;
        }
        stmt.columns.push_back(column);
    }
    if (stmt.columns.empty()) {
        stmt.type = StatementType::Invalid;
        stmt.error = "table must have columns";
    }
    return stmt;
}

Statement SqlParser::parse(const std::string& input) const {
    std::string sql = lower_outside_quotes(trim(input));
    if (!sql.empty() && sql.back() == ';') {
        sql.pop_back();
        sql = trim(sql);
    }
    Statement stmt;
    if (sql.empty()) {
        stmt.type = StatementType::Empty;
        return stmt;
    }
    if (sql == "exit" || sql == "quit") {
        stmt.type = StatementType::Quit;
        return stmt;
    }
    if (sql == "show databases") {
        stmt.type = StatementType::ShowDatabases;
        return stmt;
    }
    if (sql == "show tables") {
        stmt.type = StatementType::ShowTables;
        return stmt;
    }
    if (starts_with_word(sql, "create database ")) {
        stmt.type = StatementType::CreateDatabase;
        stmt.database = trim(sql.substr(16));
        if (!valid_identifier(stmt.database)) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid database name";
        }
        return stmt;
    }
    if (starts_with_word(sql, "drop database ")) {
        stmt.type = StatementType::DropDatabase;
        stmt.database = trim(sql.substr(14));
        if (!valid_identifier(stmt.database)) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid database name";
        }
        return stmt;
    }
    if (starts_with_word(sql, "use ")) {
        stmt.type = StatementType::UseDatabase;
        stmt.database = trim(sql.substr(4));
        if (!valid_identifier(stmt.database)) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid database name";
        }
        return stmt;
    }
    if (starts_with_word(sql, "create table ")) {
        return parse_create_table(sql);
    }
    if (starts_with_word(sql, "drop table ")) {
        stmt.type = StatementType::DropTable;
        stmt.table = trim(sql.substr(11));
        if (!valid_identifier(stmt.table)) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid table name";
        }
        return stmt;
    }
    if (starts_with_word(sql, "insert ")) {
        stmt.type = StatementType::Insert;
        std::size_t values_pos = sql.find(" values");
        std::size_t open = sql.find('(', values_pos == std::string::npos ? 0 : values_pos);
        std::size_t close = sql.rfind(')');
        if (values_pos == std::string::npos || open == std::string::npos || close == std::string::npos || close < open) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid insert syntax";
            return stmt;
        }
        stmt.table = trim(sql.substr(7, values_pos - 7));
        if (!valid_identifier(stmt.table)) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid table name";
            return stmt;
        }
        Array<std::string> values = split_csv(sql.substr(open + 1, close - open - 1));
        for (const std::string& raw : values) {
            bool ok = false;
            Value value = parse_literal(raw, ok);
            if (!ok) {
                stmt.type = StatementType::Invalid;
                stmt.error = "invalid literal";
                return stmt;
            }
            stmt.values.push_back(value);
        }
        return stmt;
    }
    if (starts_with_word(sql, "select ")) {
        stmt.type = StatementType::Select;
        std::size_t from_pos = sql.find(" from ");
        if (from_pos == std::string::npos) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid select syntax";
            return stmt;
        }
        stmt.selected_column = trim(sql.substr(7, from_pos - 7));
        std::size_t where_pos = sql.find(" where ", from_pos + 6);
        stmt.table = where_pos == std::string::npos ? trim(sql.substr(from_pos + 6)) : trim(sql.substr(from_pos + 6, where_pos - from_pos - 6));
        if ((stmt.selected_column != "*" && !valid_identifier(stmt.selected_column)) || !valid_identifier(stmt.table)) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid select syntax";
            return stmt;
        }
        if (where_pos != std::string::npos && !parse_condition(sql.substr(where_pos + 7), stmt.condition, stmt.error)) {
            stmt.type = StatementType::Invalid;
        }
        return stmt;
    }
    if (starts_with_word(sql, "delete ")) {
        stmt.type = StatementType::DeleteRows;
        std::size_t where_pos = sql.find(" where ");
        stmt.table = where_pos == std::string::npos ? trim(sql.substr(7)) : trim(sql.substr(7, where_pos - 7));
        if (!valid_identifier(stmt.table)) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid table name";
            return stmt;
        }
        if (where_pos != std::string::npos && !parse_condition(sql.substr(where_pos + 7), stmt.condition, stmt.error)) {
            stmt.type = StatementType::Invalid;
        }
        return stmt;
    }
    if (starts_with_word(sql, "update ")) {
        stmt.type = StatementType::Update;
        std::size_t set_pos = sql.find(" set ");
        if (set_pos == std::string::npos) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid update syntax";
            return stmt;
        }
        stmt.table = trim(sql.substr(7, set_pos - 7));
        std::size_t where_pos = sql.find(" where ", set_pos + 5);
        std::string assignment = where_pos == std::string::npos ? trim(sql.substr(set_pos + 5)) : trim(sql.substr(set_pos + 5, where_pos - set_pos - 5));
        std::size_t eq = assignment.find('=');
        if (eq == std::string::npos || !valid_identifier(stmt.table)) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid update syntax";
            return stmt;
        }
        stmt.set_column = trim(assignment.substr(0, eq));
        bool ok = false;
        stmt.set_value = parse_literal(assignment.substr(eq + 1), ok);
        if (!valid_identifier(stmt.set_column) || !ok) {
            stmt.type = StatementType::Invalid;
            stmt.error = "invalid update assignment";
            return stmt;
        }
        if (where_pos != std::string::npos && !parse_condition(sql.substr(where_pos + 7), stmt.condition, stmt.error)) {
            stmt.type = StatementType::Invalid;
        }
        return stmt;
    }
    stmt.type = StatementType::Invalid;
    stmt.error = "unsupported SQL statement";
    return stmt;
}

} // namespace minisql
