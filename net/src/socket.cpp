//
// Created by KINGE on 2025/12/17.
//

#include "socket.hpp"
#include "sys_socket.hpp"
#include "inet_addr.hpp"
#include "netinet/in.h"
#include <netinet/tcp.h>
#include "logging.hpp"
#include "macros.hpp"

hlink::net::Socket::Socket(const int sockfd) : sockfd_(sockfd) {
}

hlink::net::Socket::Socket(Socket &&sock) noexcept {
    sockfd_ = sock.sockfd_;
    sock.sockfd_ = -1;
}

hlink::net::Socket::~Socket() {
    close(sockfd_);
}

bool hlink::net::Socket::get_tcp_info(tcp_info &info) const {
    socklen_t len = sizeof(info);
    MEM_ZERO_SET(&info, len);
    return getsockopt(sockfd_, SOL_TCP, TCP_INFO, &info, &len) == 0;
}

bool hlink::net::Socket::get_tcp_info_string(char *buf, const int len) const {
    tcp_info tcpi{};
    const bool ok = get_tcp_info(tcpi);
    if (ok) {
        snprintf(buf, len, "unrecovered=%u "
                 "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
                 "lost=%u retrans=%u rtt=%u rttvar=%u "
                 "sshthresh=%u cwnd=%u total_retrans=%u",
                 tcpi.tcpi_retransmits, // Number of unrecovered [RTO] timeouts
                 tcpi.tcpi_rto, // Retransmit timeout in usec
                 tcpi.tcpi_ato, // Predicted tick of soft clock in usec
                 tcpi.tcpi_snd_mss,
                 tcpi.tcpi_rcv_mss,
                 tcpi.tcpi_lost, // Lost packets
                 tcpi.tcpi_retrans, // Retransmitted packets out
                 tcpi.tcpi_rtt, // Smoothed round trip time in usec
                 tcpi.tcpi_rttvar, // Medium deviation
                 tcpi.tcpi_snd_ssthresh,
                 tcpi.tcpi_snd_cwnd,
                 tcpi.tcpi_total_retrans); // Total retransmits for entire connection
    }

    return ok;
}

bool hlink::net::Socket::get_tcp_info_string(std::span<std::byte> buf) const {
    return get_tcp_info_string(reinterpret_cast<char *>(buf.data()), buf.size());
}

void hlink::net::Socket::bind_address(const InetAddr &local_addr) const {
    bind(sockfd_, local_addr.get_sock_addr());
}

void hlink::net::Socket::listen() const {
    net::listen(sockfd_);
}

int hlink::net::Socket::accept(InetAddr &peer_addr) const {
    sockaddr_in6 addr{};
    MEM_ZERO_SET(&addr, sizeof(addr));
    const int connfd = net::accept(sockfd_, &addr);
    if (connfd >= 0) {
        peer_addr.set_ipv6_sock_addr(addr);
    }
    return connfd;
}

void hlink::net::Socket::shutdown_write() const {
    net::shutdown_write(sockfd_);
}

bool hlink::net::Socket::set_tcp_nodelay(const bool on) const {
    const int optval = on ? 1 : 0;
    return setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY,
                      &optval, sizeof optval) == 0;
}

bool hlink::net::Socket::set_reuse_addr(const bool on) const {
    const int optval = on ? 1 : 0;
    return setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
                      &optval, sizeof optval) == 0;
}

bool hlink::net::Socket::set_reuse_port(const bool on) {
#ifdef SO_REUSEPORT
    const int optval = on ? 1 : 0;
    return setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT,
                      &optval, sizeof optval) == 0;
#else
    if (on) {
        LOG_ERROR("SO_REUSEPORT is not supported.");
        return false;
    }
#endif
}

bool hlink::net::Socket::set_keep_alive(const bool on) const {
    const int optval = on ? 1 : 0;
    return setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
                      &optval, sizeof optval) == 0;
}
