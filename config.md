# MiniSQL 运行环境配置

- 操作系统：Linux x86_64，推荐 Ubuntu 22.04/24.04 或同等发行版
- 编译器：支持 C++23 的 GCC/G++ 13+ 或 Clang 17+
- 构建工具：CMake 3.16+
- 网络：本机 TCP/IP，默认服务端地址 `127.0.0.1:3307`
- 数据目录：默认 `./data`，可通过 `minisqld --data <dir>` 指定
- 第三方依赖：无

本项目使用 C++23、CMake 和 POSIX socket API。Linux 下可直接编译运行。
