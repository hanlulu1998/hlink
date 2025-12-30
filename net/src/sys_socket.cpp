//
// Created by KINGE on 2025/12/17.
//

#include "sys_socket.hpp"
#include <cerrno>
#include <fcntl.h>
#include <cstdio>  // snprintf
#include <sys/socket.h>
#include <sys/uio.h>  // readv
#include <unistd.h>
#include "logging.hpp"
#include "macros.hpp"

int hlink::net::set_nonblocking(const int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    return fcntl(sockfd, F_SETFL, flags);
}

int hlink::net::set_close_on_exec(const int sockfd) {
    int flags = fcntl(sockfd, F_GETFD, 0);
    flags |= FD_CLOEXEC;
    return fcntl(sockfd, F_SETFD, flags);
}

int hlink::net::create_noblocking(const sa_family_t family) {
    // 注意valgrind对socket原子操作支持不够好，手动设置让valgrind能够正确识别
#if VALGRIND
    const int sockfd = socket(family, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        LOG_CRITICAL("socket::create_noblocking error: {}", strerror(errno));
    }

    int ret = set_nonblocking(sockfd);
    if (ret < 0) {
        LOG_ERROR("socket::create_noblocking error: {}", strerror(errno));
    }

    ret = set_close_on_exec(sockfd);
    if (ret < 0) {
        LOG_ERROR("socket::create_noblocking error: {}", strerror(errno));
    }
#else
    const int sockfd = socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0) {
        LOG_CRITICAL("socket::create_noblocking error: {}", strerror(errno));
    }
#endif
    return sockfd;
}

int hlink::net::connect(const int sockfd, const sockaddr *addr) {
    return ::connect(sockfd, addr, sizeof(sockaddr_in6));
}

void hlink::net::bind(const int sockfd, const sockaddr *addr) {
    if (::bind(sockfd, addr, sizeof(sockaddr_in6)) < 0) {
        LOG_CRITICAL("socket::bind error: {}", strerror(errno));
    }
}

void hlink::net::listen(const int sockfd) {
    if (::listen(sockfd,SOMAXCONN) < 0) {
        LOG_CRITICAL("socket::listen error: {}", strerror(errno));
    }
}

int hlink::net::accept(const int sockfd, sockaddr_in6 *addr) {
    socklen_t addr_len = sizeof(*addr);
#if VALGRIND || defined (NO_ACCEPT4)
    int connfd = ::accept(sockfd, reinterpret_cast<sockaddr *>(addr), &addr_len);
    int ret = set_nonblocking(connfd);
    if (ret < 0) {
        LOG_ERROR("socket::accept error: {}", strerror(errno));
    }
    ret = set_close_on_exec(connfd);
    if (ret < 0) {
        LOG_ERROR("socket::accept error: {}", strerror(errno));
    }
#else
    const int connfd = accept4(sockfd, reinterpret_cast<sockaddr *>(addr), &addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif
    if (connfd < 0) {
        const int saved_errno = errno;
        LOG_ERROR("socket::accept error: {}", strerror(saved_errno));

        switch (saved_errno) {
            case EAGAIN: //当前没有连接
            case ECONNABORTED: // 远程客户端终止
            case EINTR: // 被信号终止
            case EPROTO: // 协议错误
            case EPERM: // 防火墙问题
            case EMFILE: // 文件描述符达到上限
                // expected errors
                errno = saved_errno;
                break;
            case EBADF: // 错误的socket fd
            case EFAULT: // addr参数不可用
            case EINVAL: // socket没有listen或者无效
            case ENFILE: // 系统文件描述符已经满了
            case ENOBUFS: // 内核缓冲不足
            case ENOMEM: // 内存不足
            case ENOTSOCK: // fd不是socket
            case EOPNOTSUPP: // socket类型不支持accept
                LOG_CRITICAL("unexpected error of ::accept {}", strerror(saved_errno));
            default:
                LOG_CRITICAL("unknown error of ::accept error: {}", strerror(saved_errno));
        }
    }
    return connfd;
}

ssize_t hlink::net::read(const int sockfd, void *buf, const size_t count) {
    return ::read(sockfd, buf, count);
}

ssize_t hlink::net::read(const int sockfd, std::span<std::byte> buf) {
    return read(sockfd, buf.data(), buf.size());
}

ssize_t hlink::net::readv(const int sockfd, const iovec *iov, const int iovcnt) {
    return ::readv(sockfd, iov, iovcnt);
}

ssize_t hlink::net::write(const int sockfd, const void *buf, const size_t count) {
    return ::write(sockfd, buf, count);
}

ssize_t hlink::net::write(const int sockfd, const std::span<std::byte> buf) {
    return write(sockfd, buf.data(), buf.size());
}

void hlink::net::close(const int sockfd) {
    if (::close(sockfd) < 0) {
        LOG_ERROR("socket::close error: {}", strerror(errno));
    }
}

void hlink::net::shutdown_write(const int sockfd) {
    if (shutdown(sockfd, SHUT_WR) < 0) {
        LOG_ERROR("socket::shutdown_write error: {}", strerror(errno));
    }
}

void hlink::net::to_ip_port(char *buf, const size_t size, const sockaddr *addr) {
    if (addr->sa_family == AF_INET6) {
        buf[0] = '[';
        to_ip(buf + 1, size - 1, addr);
        const size_t end = strlen(buf);
        const auto ipv6_addr = reinterpret_cast<const sockaddr_in6 *>(addr);
        const uint16_t port = ntohs(ipv6_addr->sin6_port);
        assert(size > end);
        snprintf(buf + end, size - end, "]:%u", port);
        return;
    }
    to_ip(buf, size, addr);
    const size_t end = strlen(buf);
    const auto ipv4_addr = reinterpret_cast<const sockaddr_in *>(addr);
    const uint16_t port = ntohs(ipv4_addr->sin_port);
    assert(size > end);
    snprintf(buf + end, size - end, ":%u", port);
}

void hlink::net::to_ip(char *buf, const size_t size, const sockaddr *addr) {
    if (addr->sa_family == AF_INET) {
        assert(size >= INET_ADDRSTRLEN);
        const auto ipv4_addr = reinterpret_cast<const sockaddr_in *>(addr);
        inet_ntop(AF_INET, &ipv4_addr->sin_addr, buf, static_cast<socklen_t>(size));
    } else if (addr->sa_family == AF_INET6) {
        assert(size >= INET6_ADDRSTRLEN);
        const auto ipv6_addr = reinterpret_cast<const sockaddr_in6 *>(addr);
        inet_ntop(AF_INET6, &ipv6_addr->sin6_addr, buf, static_cast<socklen_t>(size));
    }
}

void hlink::net::to_ip(std::span<std::byte> buf, const sockaddr *addr) {
    to_ip(reinterpret_cast<char *>(buf.data()), buf.size(), addr);
}

void hlink::net::from_ip_port(const char *ip, const uint16_t port, sockaddr_in *addr) {
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr->sin_addr) <= 0) {
        LOG_ERROR("socket::from_ip_port error: {}", strerror(errno));
    }
}

void hlink::net::from_ip_port(const char *ip, const uint16_t port, sockaddr_in6 *addr) {
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);
    if (inet_pton(AF_INET6, ip, &addr->sin6_addr) <= 0) {
        LOG_ERROR("socket::from_ip_port error: {}", strerror(errno));
    }
}

int hlink::net::get_socket_error(const int sockfd) {
    int optval = 0;
    socklen_t optlen = sizeof optval;
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        return errno;
    }
    return optval;
}

void hlink::net::get_local_addr(sockaddr_in6 *addr, const int sockfd) {
    socklen_t addr_len = sizeof(*addr);
    MEM_ZERO_SET(addr, addr_len);
    if (getsockname(sockfd, reinterpret_cast<sockaddr *>(addr), &addr_len) < 0) {
        LOG_ERROR("socket::get_local_addr error: {}", strerror(errno));
    }
}

void hlink::net::get_peer_addr(sockaddr_in6 *addr, int sockfd) {
    socklen_t addr_len = sizeof(*addr);
    MEM_ZERO_SET(addr, addr_len);
    if (getpeername(sockfd, reinterpret_cast<sockaddr *>(addr), &addr_len) < 0) {
        LOG_ERROR("socket::get_peer_addr error: {}", strerror(errno));
    }
}

bool hlink::net::is_self_connection(const int sockfd) {
    sockaddr_in6 peer_addr{};
    sockaddr_in6 local_addr{};
    get_peer_addr(&peer_addr, sockfd);
    get_local_addr(&local_addr, sockfd);
    if (local_addr.sin6_family == AF_INET) {
        const sockaddr_in *local_ipv4_addr = reinterpret_cast<sockaddr_in *>(&local_addr);
        const sockaddr_in *peer_ipv4_addr = reinterpret_cast<sockaddr_in *>(&peer_addr);
        return local_ipv4_addr->sin_addr.s_addr == peer_ipv4_addr->sin_addr.s_addr
               && local_ipv4_addr->sin_port == peer_ipv4_addr->sin_port;
    }
    if (local_addr.sin6_family == AF_INET6) {
        return memcmp(&local_addr.sin6_addr, &peer_addr.sin6_addr, sizeof(local_addr.sin6_addr)) == 0
               && local_addr.sin6_port == peer_addr.sin6_port;
    }
    return false;
}
