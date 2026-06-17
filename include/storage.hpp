#pragma once

#include "table.hpp"

#include <filesystem>
#include <string>

namespace minisql {

class Storage {
public:
    explicit Storage(std::filesystem::path root);

    void ensure_root() const;
    [[nodiscard]] std::filesystem::path database_path(const std::string& name) const;
    [[nodiscard]] std::filesystem::path table_path(const std::string& database, const std::string& table) const;
    bool create_database(const std::string& name, std::string& error) const;
    bool drop_database(const std::string& name, std::string& error) const;
    [[nodiscard]] bool database_exists(const std::string& name) const;
    bool create_table(const std::string& database, const Table& table, std::string& error) const;
    bool drop_table(const std::string& database, const std::string& table, std::string& error) const;
    bool save_table(const std::string& database, const Table& table, std::string& error) const;
    bool load_table(const std::string& database, const std::string& table_name, Table& table, std::string& error) const;
    [[nodiscard]] Array<std::string> list_databases() const;
    [[nodiscard]] Array<std::string> list_tables(const std::string& database) const;

private:
    std::filesystem::path root_;
};

}  // namespace minisql
