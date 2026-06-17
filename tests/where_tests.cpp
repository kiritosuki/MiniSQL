#include "engine.hpp"

#include <filesystem>
#include <iostream>

static int failures = 0;

static void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

int main() {
    std::filesystem::path root = std::filesystem::temp_directory_path() / "minisql_where_tests";
    std::filesystem::remove_all(root);
    minisql::Engine engine(root);

    auto exec = [&](const std::string& sql) {
        minisql::ResultSet result = engine.execute(sql);
        if (!result.ok) {
            std::cerr << sql << " -> " << result.message << "\n";
        }
        return result;
    };

    expect(exec("create database person").ok, "create database");
    expect(exec("use person").ok, "use database");
    expect(exec("create table user (id int primary, name string)").ok, "create table");

    expect(exec("insert user values (1001, \"Peter\")").ok, "insert Peter");
    expect(exec("insert user values (1002, \"Mary\")").ok, "insert Mary");
    expect(exec("insert user values (1003, \"Alice\")").ok, "insert Alice");

    minisql::ResultSet selected = exec("select name from user where id = 1002");
    expect(selected.ok, "select with equals");
    expect(selected.rows.size() == 1, "select equals one row");
    expect(selected.rows[0].values[0].to_string() == "Mary", "select equals returns Mary");

    minisql::ResultSet greater = exec("select * from user where id > 1001");
    expect(greater.ok, "select with greater than");
    expect(greater.rows.size() == 2, "select greater than two rows");
    expect(greater.rows[0].values[0].to_string() == "1002", "greater than first row");
    expect(greater.rows[1].values[0].to_string() == "1003", "greater than second row");

    minisql::ResultSet less = exec("select * from user where id < 1003");
    expect(less.ok, "select with less than");
    expect(less.rows.size() == 2, "select less than two rows");
    expect(less.rows[0].values[0].to_string() == "1001", "less than first row");
    expect(less.rows[1].values[0].to_string() == "1002", "less than second row");

    expect(exec("update user set name = \"Bob\" where id = 1003").ok, "update with where");
    selected = exec("select name from user where id = 1003");
    expect(selected.rows.size() == 1, "updated row still exists");
    expect(selected.rows[0].values[0].to_string() == "Bob", "update changed target row");

    expect(exec("delete user where id = 1001").ok, "delete with where");
    minisql::ResultSet all = exec("select * from user");
    expect(all.ok, "select all after delete");
    expect(all.rows.size() == 2, "two rows remain after delete");
    expect(all.rows[0].values[0].to_string() == "1002", "remaining first row");
    expect(all.rows[1].values[0].to_string() == "1003", "remaining second row");

    std::filesystem::remove_all(root);
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "all where tests passed\n";
    return 0;
}
