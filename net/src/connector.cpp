//
// Created by KINGE on 2025/12/26.
//

#include "connector.hpp"
#include "timerid.hpp"
#include "logging.hpp"
#include <cerrno>
#include "sys_socket.hpp"
#include "channel.hpp"
#include "eventloop.hpp"
#include "macros.hpp"

hlink::net::Connector::Connector(EventLoop *loop, const InetAddr &server_addr) : loop_(loop),
    server_addr_(server_addr),
    connected_(false),
    state_(State::DISCONNECTED),
    retry_delay_ms_(INIT_RETRY_DELAY_MS) {
}


void hlink::net::Connector::set_new_connection_callback(NewConnectionCallback cb) {
    new_connection_callback_ = std::move(cb);
}

void hlink::net::Connector::start() {
    connected_ = true;
    loop_->run_in_loop([this] {
        this->start_in_loop();
    });
}

void hlink::net::Connector::stop() {
    connected_ = false;
    loop_->queue_in_loop([this] {
        this->stop_in_loop();
    });
}

void hlink::net::Connector::restart() {
    loop_->assert_in_loop_thread();
    set_state(State::DISCONNECTED);
    retry_delay_ms_ = INIT_RETRY_DELAY_MS;
    connected_ = true;
    start_in_loop();
}

const hlink::net::InetAddr &hlink::net::Connector::get_server_addr() const {
    return server_addr_;
}

void hlink::net::Connector::set_state(const State state) {
    state_ = state;
}

void hlink::net::Connector::start_in_loop() {
    loop_->assert_in_loop_thread();
    assert(state_== State::DISCONNECTED);
    if (connected_) {
        connect();
    } else {
        LOG_DEBUG("do not connect");
    }
}

void hlink::net::Connector::stop_in_loop() {
    loop_->assert_in_loop_thread();
    if (state_ == State::CONNECTING) {
        set_state(State::DISCONNECTED);
        const int sockfd = remove_and_reset_channel();
        retry(sockfd);
    }
}

void hlink::net::Connector::connect() {
    const int sockfd = create_noblocking(server_addr_.get_family());
    const int ret = net::connect(sockfd, server_addr_.get_sock_addr());
    switch (const int saved_errno = ret == 0 ? 0 : errno) {
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
            connecting(sockfd);
            break;
        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            retry(sockfd);
            break;
        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            LOG_ERROR("connect error in Connector::start_in_Loop:{}", strerror(saved_errno));
            close(sockfd);
            break;
        default:
            LOG_ERROR("Unexpected error in Connector::start_in_Loop:{}", strerror(saved_errno));
            close(sockfd);
            break;
    }
}

void hlink::net::Connector::connecting(int sockfd) {
    set_state(State::CONNECTING);
    assert(!channel_);
    channel_ = std::make_unique<Channel>(loop_, sockfd);
    channel_->set_write_callback([this] {
        this->handle_write();
    });
    channel_->set_error_callback([this] {
        this->handle_error();
    });

    channel_->enable_writing();
}

void hlink::net::Connector::handle_write() {
    if (state_ == State::CONNECTING) {
        const int sockfd = remove_and_reset_channel();
        if (int err = get_socket_error(sockfd)) {
            LOG_WARN("Connector::handle_write - SO_ERROR = {}: {}", err, strerror(err));
        } else if (is_self_connection(sockfd)) {
            LOG_WARN("Connector::handle_write self connect");
            retry(sockfd);
        } else {
            set_state(State::CONNECTED);
            if (connected_) {
                new_connection_callback_(sockfd);
            } else {
                close(sockfd);
            }
        }
    } else {
        assert(state_ == State::DISCONNECTED);
    }
}

void hlink::net::Connector::handle_error() {
    LOG_ERROR("Connector::handle_error state = ", state_to_string());
    if (state_ == State::CONNECTING) {
        const int sockfd = remove_and_reset_channel();
        const int err = get_socket_error(sockfd);
        LOG_ERROR("get_socket_error = ", strerror(err));
        retry(sockfd);
    }
}

void hlink::net::Connector::retry(const int sockfd) {
    close(sockfd);
    set_state(State::DISCONNECTED);
    if (connected_) {
        LOG_INFO("Connector::retry - Retry connecting to {} in {} ms", server_addr_.to_ip_port(), retry_delay_ms_);

        loop_->run_after(retry_delay_ms_, [this] {
            this->shared_from_this()->start_in_loop();
        });
        retry_delay_ms_ = std::min(retry_delay_ms_ * 2, MAX_RETRY_DELAY_MS);
    }
}

int hlink::net::Connector::remove_and_reset_channel() {
    channel_->disable_all();
    channel_->remove();
    const int sockfd = channel_->fd();
    loop_->queue_in_loop([this] {
        this->reset_channel();
    });
    return sockfd;
}

void hlink::net::Connector::reset_channel() {
    channel_.reset();
}

const char *hlink::net::Connector::state_to_string() const {
    switch (state_) {
        case State::CONNECTING:
            return "CONNECTING";
        case State::CONNECTED:
            return "CONNECTED";
        case State::DISCONNECTED:
            return "DISCONNECTED";
        case State::DISCONNECTING:
            return "DISCONNECTING";
        default:
            return "UNKNOWN";
    }
}
