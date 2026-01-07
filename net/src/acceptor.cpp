//
// Created by KINGE on 2025/12/22.
//

#include "acceptor.hpp"
#include "eventloop.hpp"
#include "sys_socket.hpp"
#include "logging.hpp"
#include "inet_addr.hpp"
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include "channel.hpp"
#include "socket.hpp"

class hlink::net::Acceptor::Impl {
public:
    Impl(EventLoop *loop, const InetAddr &listen_address, bool reuse_port);

    ~Impl();

    void set_new_connection_callback(NewConnectionCallback cb);

    void listen();

    [[nodiscard]] bool is_listening() const;

private:
    void handle_read();

    EventLoop *loop_{nullptr};
    Socket accept_socket_;
    Channel accept_channel_;
    NewConnectionCallback new_connection_callback_;
    bool listening_{false};
    int idle_fd_{-1};
};


hlink::net::Acceptor::Impl::Impl(EventLoop *loop, const InetAddr &listen_address, const bool reuse_port) : loop_(loop),
    accept_socket_(create_noblocking(listen_address.get_family())),
    accept_channel_(loop, accept_socket_.fd()),
    idle_fd_(open("/dev/null",O_RDONLY | O_CLOEXEC)) {
    if (idle_fd_ < 0) {
        LOG_CRITICAL("Open /dev/null error: {}", strerror(errno));
    }
    accept_socket_.set_reuse_addr(true);
    accept_socket_.set_reuse_port(reuse_port);
    accept_socket_.bind_address(listen_address);
    accept_channel_.set_read_callback([this](TimeStamp) {
        this->handle_read();
    });
}

hlink::net::Acceptor::Impl::~Impl() {
    accept_channel_.disable_all();
    accept_channel_.remove();
    close(idle_fd_);
}

void hlink::net::Acceptor::Impl::set_new_connection_callback(NewConnectionCallback cb) {
    new_connection_callback_ = std::move(cb);
}

void hlink::net::Acceptor::Impl::listen() {
    loop_->assert_in_loop_thread();
    listening_ = true;
    accept_socket_.listen();
    accept_channel_.enable_reading();
}

bool hlink::net::Acceptor::Impl::is_listening() const {
    return listening_;
}

void hlink::net::Acceptor::Impl::handle_read() {
    loop_->assert_in_loop_thread();
    InetAddr peer_address;
    if (const int connfd = accept_socket_.accept(peer_address); connfd >= 0) {
        if (new_connection_callback_) {
            new_connection_callback_(connfd, peer_address);
        } else {
            close(connfd);
        }
    } else {
        LOG_ERROR("Acceptor::handle_read error: {}", strerror(errno));
        // 如果fd不够了，利用空闲的fd不断关闭
        if (errno == EMFILE) {
            close(idle_fd_);
            idle_fd_ = ::accept(accept_socket_.fd(), nullptr, nullptr);
            close(idle_fd_);
            idle_fd_ = open("/dev/null",O_RDONLY | O_CLOEXEC);
        }
    }
}

void hlink::net::Acceptor::set_new_connection_callback(NewConnectionCallback cb) {
    impl_->set_new_connection_callback(std::move(cb));
}

hlink::net::Acceptor::~Acceptor() = default;


hlink::net::Acceptor::Acceptor(EventLoop *loop, const InetAddr &listen_address, bool reuse_port) : impl_(
    std::make_unique<Impl>(loop, listen_address, reuse_port)) {
}

void hlink::net::Acceptor::listen() {
    impl_->listen();
}

bool hlink::net::Acceptor::is_listening() const {
    return impl_->is_listening();
}
