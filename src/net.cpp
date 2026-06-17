#include "net.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace minisql {

static bool read_all(int fd, char* buffer, std::size_t size) {
    std::size_t done = 0;
    while (done < size) {
        ssize_t n = ::recv(fd, buffer + done, size - done, 0);
        if (n <= 0) {
            return false;
        }
        done += static_cast<std::size_t>(n);
    }
    return true;
}

static bool write_all(int fd, const char* buffer, std::size_t size) {
    std::size_t done = 0;
    while (done < size) {
        ssize_t n = ::send(fd, buffer + done, size - done, 0);
        if (n <= 0) {
            return false;
        }
        done += static_cast<std::size_t>(n);
    }
    return true;
}

bool send_frame(int fd, const std::string& payload) {
    std::uint32_t len = htonl(static_cast<std::uint32_t>(payload.size()));
    return write_all(fd, reinterpret_cast<const char*>(&len), sizeof(len)) &&
           write_all(fd, payload.data(), payload.size());
}

bool recv_frame(int fd, std::string& payload) {
    std::uint32_t len = 0;
    if (!read_all(fd, reinterpret_cast<char*>(&len), sizeof(len))) {
        return false;
    }
    len = ntohl(len);
    payload.assign(len, '\0');
    if (len == 0) {
        return true;
    }
    return read_all(fd, payload.data(), len);
}

int connect_tcp(const std::string& host, std::uint16_t port, std::string& error) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error = std::strerror(errno);
        return -1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        error = "invalid IPv4 address";
        ::close(fd);
        return -1;
    }
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        error = std::strerror(errno);
        ::close(fd);
        return -1;
    }
    return fd;
}

int listen_tcp(const std::string& host, std::uint16_t port, std::string& error) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error = std::strerror(errno);
        return -1;
    }
    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        error = "invalid IPv4 address";
        ::close(fd);
        return -1;
    }
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        error = std::strerror(errno);
        ::close(fd);
        return -1;
    }
    if (::listen(fd, 16) != 0) {
        error = std::strerror(errno);
        ::close(fd);
        return -1;
    }
    return fd;
}

} // namespace minisql
