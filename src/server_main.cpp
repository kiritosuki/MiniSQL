#include "minisql/engine.hpp"
#include "minisql/net.hpp"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

using minisql::Engine;

static volatile std::sig_atomic_t running = 1;

static void stop_server(int) {
    running = 0;
}

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
    std::string data = arg_value(argc, argv, "--data", "data");

    std::signal(SIGINT, stop_server);
    std::signal(SIGTERM, stop_server);

    std::string error;
    int server = minisql::listen_tcp(host, port, error);
    if (server < 0) {
        std::cerr << "minisqld: " << error << "\n";
        return 1;
    }
    std::cout << "minisqld listening on " << host << ":" << port << ", data=" << data << "\n";

    Engine engine(data);
    while (running) {
        sockaddr_storage peer{};
        socklen_t peer_len = sizeof(peer);
        int client = ::accept(server, reinterpret_cast<sockaddr*>(&peer), &peer_len);
        if (client < 0) {
            if (running) {
                std::cerr << "accept failed\n";
            }
            continue;
        }
        std::string sql;
        while (minisql::recv_frame(client, sql)) {
            minisql::ResultSet result = engine.execute(sql);
            minisql::send_frame(client, minisql::serialize_result(result));
            if (sql == "exit" || sql == "quit") {
                break;
            }
        }
        ::close(client);
    }
    ::close(server);
    return 0;
}
