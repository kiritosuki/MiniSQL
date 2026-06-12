#include "minisql/engine.hpp"
#include "minisql/net.hpp"
#include "minisql/util.hpp"

#include <iostream>
#include <string>
#include <unistd.h>

static std::string arg_value(int argc, char** argv, const std::string& key, const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == key) {
            return argv[i + 1];
        }
    }
    return fallback;
}

int main(int argc, char** argv) {
    std::string host = arg_value(argc, argv, "--host", "127.0.0.1");
    std::uint16_t port = static_cast<std::uint16_t>(std::stoi(arg_value(argc, argv, "--port", "3307")));
    std::string execute = arg_value(argc, argv, "-e", "");

    std::string error;
    int fd = minisql::connect_tcp(host, port, error);
    if (fd < 0) {
        std::cerr << "minisql: " << error << "\n";
        return 1;
    }

    auto run_one = [&](const std::string& sql) -> bool {
        if (!minisql::send_frame(fd, sql)) {
            std::cerr << "send failed\n";
            return false;
        }
        std::string payload;
        if (!minisql::recv_frame(fd, payload)) {
            std::cerr << "server closed connection\n";
            return false;
        }
        minisql::ResultSet result = minisql::deserialize_result(payload);
        std::cout << minisql::format_result(result);
        return result.ok;
    };

    if (!execute.empty()) {
        bool ok = run_one(execute);
        ::close(fd);
        return ok ? 0 : 1;
    }

    std::cout << "Welcome to the MiniSQL monitor. Commands end with semicolon or newline.\n";
    std::string line;
    while (true) {
        std::cout << "minisql> " << std::flush;
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }
        bool ok = run_one(line);
        std::string lowered = minisql::to_lower(minisql::trim(line));
        if (lowered == "exit" || lowered == "quit" || lowered == "exit;" || lowered == "quit;") {
            break;
        }
        if (!ok) {
            continue;
        }
    }
    ::close(fd);
    return 0;
}
