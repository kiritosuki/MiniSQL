#include "database.hpp"

namespace minisql {

Database::Database(const Storage& storage, std::string name)
    : storage_(&storage), name_(std::move(name)) {}

const std::string& Database::name() const {
    return name_;
}

bool Database::exists() const {
    return storage_ != nullptr && storage_->database_exists(name_);
}

bool Database::create_table(const Table& table, std::string& error) const {
    return storage_->create_table(name_, table, error);
}

bool Database::drop_table(const std::string& table_name, std::string& error) const {
    return storage_->drop_table(name_, table_name, error);
}

bool Database::load_table(const std::string& table_name, Table& table, std::string& error) const {
    return storage_->load_table(name_, table_name, table, error);
}

bool Database::save_table(const Table& table, std::string& error) const {
    return storage_->save_table(name_, table, error);
}

Array<std::string> Database::list_tables() const {
    return storage_->list_tables(name_);
}

}  // namespace minisql
