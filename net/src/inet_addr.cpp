//
// Created by KINGE on 2025/12/17.
//
#include "inet_addr.hpp"
#include <cassert>
#include <netdb.h>
#include "logging.hpp"
#include "sys_socket.hpp"


hlink::net::InetAddr::InetAddr(const uint16_t port, const bool loopback, const bool ipv6) {
    if (ipv6) {
        auto &sin = addr_.emplace<sockaddr_in6>();
        sin.sin6_family = AF_INET6;
        sin.sin6_port = htons(port);
        sin.sin6_addr = loopback ? in6addr_loopback : in6addr_any;
    } else {
        auto &sin = addr_.emplace<sockaddr_in>();
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        const in_addr_t ip = loopback ? INADDR_LOOPBACK : INADDR_ANY;
        sin.sin_addr.s_addr = htons(ip);
    }
}

hlink::net::InetAddr::InetAddr(const std::string_view ip, const uint16_t port, const bool ipv6) {
    if (ipv6 || ip.contains(':')) {
        auto &sin = addr_.emplace<sockaddr_in6>();
        from_ip_port(ip.data(), port, &sin);
    } else {
        auto &sin = addr_.emplace<sockaddr_in>();
        from_ip_port(ip.data(), port, &sin);
    }
}

hlink::net::InetAddr::InetAddr(const sockaddr_in &addr) : addr_(addr) {
}

hlink::net::InetAddr::InetAddr(const sockaddr_in6 &addr) : addr_(addr) {
}

sa_family_t hlink::net::InetAddr::get_family() const {
    return std::get<sockaddr_in>(addr_).sin_family;
}

std::string hlink::net::InetAddr::to_ip() const {
    char buf[64]{};
    net::to_ip(buf, sizeof(buf), get_sock_addr());
    return buf;
}

std::string hlink::net::InetAddr::to_ip_port() const {
    char buf[64]{};
    net::to_ip_port(buf, sizeof(buf), get_sock_addr());
    return buf;
}

uint16_t hlink::net::InetAddr::port() const {
    return ntohs(network_endian_port());
}

bool hlink::net::InetAddr::resolve(const std::string_view host, InetAddr &addr) {
    addrinfo hints{};
    addrinfo *res{nullptr};

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (const int ret = getaddrinfo(host.data(), nullptr, &hints, &res) != 0) {
        LOG_ERROR("resolve::getaddrinfo error: %s", gai_strerror(ret));
        return false;
    }

    for (const addrinfo *p = res; p; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            addr.addr_ = *reinterpret_cast<sockaddr_in *>(p->ai_addr);
        } else if (p->ai_family == AF_INET6) {
            addr.addr_ = *reinterpret_cast<sockaddr_in6 *>(p->ai_addr);
        }
    }
    freeaddrinfo(res);
    return true;
}

void hlink::net::InetAddr::set_scope_id(const uint32_t scope_id) {
    if (get_family() == AF_INET6) {
        std::get<sockaddr_in6>(addr_).sin6_scope_id = scope_id;
    }
}

const sockaddr *hlink::net::InetAddr::get_sock_addr() const {
    return std::visit([](auto &addr) {
        return reinterpret_cast<const sockaddr *>(&addr);
    }, addr_);
}

void hlink::net::InetAddr::set_ipv6_sock_addr(const sockaddr_in6 &addr) {
    addr_ = addr;
}

uint16_t hlink::net::InetAddr::network_endian_port() const {
    return std::get<sockaddr_in>(addr_).sin_port;
}

uint32_t hlink::net::InetAddr::network_endian_ipv4() const {
    assert(get_family() == AF_INET);
    return std::get<sockaddr_in>(addr_).sin_addr.s_addr;
}
