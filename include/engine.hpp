#pragma once

#include "sql_parser.hpp"
#include "storage.hpp"

#include <filesystem>
#include <string>

namespace minisql {

class Engine {
public:
    explicit Engine(const std::filesystem::path& data_dir);

    [[nodiscard]] ResultSet execute(const std::string& sql);

private:
    [[nodiscard]] bool require_database(ResultSet& result) const;
    [[nodiscard]] bool coerce_value(Value& value, ColumnType type, std::string& error) const;
    [[nodiscard]] ResultSet execute_statement(const Statement& stmt);

    Storage storage_;
    SqlParser parser_;
    std::string current_database_;
};

std::string serialize_result(const ResultSet& result);
ResultSet deserialize_result(const std::string& payload);
std::string format_result(const ResultSet& result);

}  // namespace minisql
