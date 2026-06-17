#pragma once

#include <cstdint>
#include <string>

namespace minisql {

bool send_frame(int fd, const std::string& payload);
bool recv_frame(int fd, std::string& payload);
int connect_tcp(const std::string& host, std::uint16_t port, std::string& error);
int listen_tcp(const std::string& host, std::uint16_t port, std::string& error);

}  // namespace minisql
