//
// Created by KINGE on 2025/12/17.
//
#pragma once
#include <span>
#include <string>

#include "noncopyable.hpp"
struct tcp_info;

namespace hlink::net {
    class InetAddr;

    class Socket : public NonCopyable {
    public:
        explicit Socket(int sockfd);

        Socket(Socket &&sock) noexcept;

        ~Socket();

        int fd() const {
            return sockfd_;
        }

        bool get_tcp_info(tcp_info &info) const;

        bool get_tcp_info_string(char *buf, int len) const;

        bool get_tcp_info_string(std::span<std::byte> buf) const;

        void bind_address(const InetAddr &local_addr) const;

        void listen() const;

        int accept(InetAddr &peer_addr) const;

        void shutdown_write() const;

        bool set_tcp_nodelay(bool on) const;

        bool set_reuse_addr(bool on) const;

        bool set_reuse_port(bool on);

        bool set_keep_alive(bool on) const;

    private:
        int sockfd_{-1};
    };
}
