//
// Created by KINGE on 2025/12/17.
//

#pragma once
#include <netinet/in.h>
#include <string>
#include <variant>

namespace hlink::net {
    class InetAddr {
    public:
        explicit InetAddr(uint16_t port = 0, bool loopback = false, bool ipv6 = false);

        InetAddr(std::string_view ip, uint16_t port, bool ipv6 = false);

        explicit InetAddr(const sockaddr_in &addr);

        explicit InetAddr(const sockaddr_in6 &addr);

        [[nodiscard]] sa_family_t get_family() const;

        [[nodiscard]] std::string to_ip() const;

        [[nodiscard]] std::string to_ip_port() const;

        [[nodiscard]] uint16_t port() const;

        [[nodiscard]] const sockaddr *get_sock_addr() const;

        void set_ipv6_sock_addr(const sockaddr_in6 &addr);

        [[nodiscard]] uint16_t network_endian_port() const;

        [[nodiscard]] uint32_t network_endian_ipv4() const;

        static bool resolve(std::string_view host, InetAddr &addr);

        void set_scope_id(uint32_t scope_id);

    private:
        std::variant<sockaddr_in, sockaddr_in6> addr_;
    };
}
