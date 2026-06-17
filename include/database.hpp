#pragma once

#include "storage.hpp"

#include <string>

namespace minisql {

class Database {
public:
    Database(const Storage& storage, std::string name);

    [[nodiscard]] const std::string& name() const;
    [[nodiscard]] bool exists() const;
    bool create_table(const Table& table, std::string& error) const;
    bool drop_table(const std::string& table_name, std::string& error) const;
    bool load_table(const std::string& table_name, Table& table, std::string& error) const;
    bool save_table(const Table& table, std::string& error) const;
    [[nodiscard]] Array<std::string> list_tables() const;

private:
    const Storage* storage_ = nullptr;
    std::string name_;
};

}  // namespace minisql
