#include "minisql/engine.hpp"

#include <cstdlib>
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
    std::filesystem::path root = std::filesystem::temp_directory_path() / "minisql_core_tests";
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
    expect(!exec("insert user values (1002, \"dup\")").ok, "duplicate primary key rejected");

    minisql::ResultSet selected = exec("select name from user where id = 1001");
    expect(selected.ok, "select ok");
    expect(selected.rows.size() == 1, "select one row");
    expect(selected.rows[0].values[0].to_string() == "Peter", "select Peter");

    expect(exec("update user set name = \"john\" where id = 1001").ok, "update row");
    selected = exec("select name from user where id = 1001");
    expect(selected.rows[0].values[0].to_string() == "john", "updated value");

    minisql::ResultSet all = exec("select * from user where id > 1000");
    expect(all.rows.size() == 2, "select greater than");
    expect(all.rows[0].values[0].to_string() == "1001", "insertion order first row");
    expect(all.rows[1].values[0].to_string() == "1002", "insertion order second row");

    expect(exec("delete user where id = 1002").ok, "delete row");
    all = exec("select * from user");
    expect(all.rows.size() == 1, "one row remains");

    expect(std::filesystem::exists(root / "person" / "user.dat"), "table data file exists");
    expect(std::filesystem::exists(root / "person" / "user.idx"), "index file exists");

    expect(exec("drop table user").ok, "drop table");
    expect(exec("drop database person").ok, "drop database");

    std::filesystem::remove_all(root);
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "all minisql tests passed\n";
    return 0;
}
