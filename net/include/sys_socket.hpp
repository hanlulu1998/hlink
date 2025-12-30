//
// Created by KINGE on 2025/12/17.
//
#pragma once
#include <arpa/inet.h>
#include <span>

namespace hlink::net {
    int set_nonblocking(int sockfd);

    int set_close_on_exec(int sockfd);

    int create_noblocking(sa_family_t family);

    int connect(int sockfd, const sockaddr *addr);

    void bind(int sockfd, const sockaddr *addr);

    void listen(int sockfd);

    int accept(int sockfd, sockaddr_in6 *addr);

    ssize_t read(int sockfd, void *buf, size_t count);

    ssize_t read(int sockfd, std::span<std::byte> buf);

    ssize_t readv(int sockfd, const iovec *iov, int iovcnt);

    ssize_t write(int sockfd, const void *buf, size_t count);

    ssize_t write(int sockfd, std::span<std::byte> buf);

    void close(int sockfd);

    void shutdown_write(int sockfd);

    void to_ip_port(char *buf, size_t size,
                    const sockaddr *addr);

    void to_ip(char *buf, size_t size,
               const sockaddr *addr);

    void to_ip(std::span<std::byte> buf,const sockaddr *addr);


    void from_ip_port(const char *ip, uint16_t port,
                      sockaddr_in *addr);

    void from_ip_port(const char *ip, uint16_t port,
                      sockaddr_in6 *addr);

    int get_socket_error(int sockfd);

    void get_local_addr(sockaddr_in6 *addr, int sockfd);

    void get_peer_addr(sockaddr_in6 *addr, int sockfd);

    bool is_self_connection(int sockfd);
}
