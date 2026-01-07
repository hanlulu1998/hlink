//
// Created by KINGE on 2025/12/22.
//

#include "tcp_conn.hpp"
#include "channel.hpp"
#include "eventloop.hpp"
#include "socket.hpp"
#include "logging.hpp"
#include "sys_socket.hpp"
#include "timerid.hpp"
#include "weak_callback.hpp"
#include <csignal>

// 忽略因为客户端断开没来得及感知继续Write触发的中断
class IgnoreSigPipe {
public:
    IgnoreSigPipe() {
        signal(SIGPIPE, SIG_IGN);
    }
};

IgnoreSigPipe _;

void hlink::net::TcpConn::handle_read(const TimeStamp receive_time) {
    loop_->assert_in_loop_thread();
    int saved_errno = 0;

    if (const ssize_t n = input_buffer_.append_from_fd(channel_->fd(), &saved_errno); n > 0) {
        // 这里约定了回调只是简单的数据处理，而不是关闭连接和再发送数据
        if (message_callback_) {
            message_callback_(shared_from_this(), &input_buffer_, receive_time);
        }
    } else if (n == 0) {
        handle_close();
    } else {
        errno = saved_errno;
        LOG_ERROR("TcpConnection::handle_read error: {}", strerror(errno));
        handle_error();
    }
}

void hlink::net::TcpConn::handle_write() {
    loop_->assert_in_loop_thread();
    if (channel_->is_writing()) {
        if (const ssize_t n = write(channel_->fd(), output_buffer_.read_ptr(), output_buffer_.readable_bytes());
            n > 0) {
            output_buffer_.pop(n);
            if (output_buffer_.readable_bytes() == 0) {
                channel_->disable_writing();
                if (write_complete_callback_) {
                    loop_->run_in_loop([this] {
                        this->write_complete_callback_(shared_from_this());
                    });
                }

                if (state_ == State::DISCONNECTING) {
                    shutdown_in_loop();
                }
            }
        } else {
            LOG_ERROR("TcpConnection::handle_write error: {}", strerror(errno));
        }
    }
}

void hlink::net::TcpConn::handle_close() {
    loop_->assert_in_loop_thread();
    assert(state_ == State::DISCONNECTING|| state_ == State::CONNECTED);
    set_state(State::DISCONNECTED);
    channel_->disable_all();

    const auto guard = shared_from_this();
    if (connection_callback_) {
        connection_callback_(guard);
    }
    if (close_callback_) {
        close_callback_(guard);
    }
}

void hlink::net::TcpConn::handle_error() const {
    const int err = get_socket_error(channel_->fd());
    LOG_ERROR("TcpConnection::handleError [{}] - SO_ERROR = {}", name_, strerror(err));
}

void hlink::net::TcpConn::send_in_loop(const void *message, const size_t len) {
    loop_->assert_in_loop_thread();
    ssize_t n_written{0};
    size_t remaining = len;
    bool is_conn_error = false;

    if (state_ == State::DISCONNECTED) {
        LOG_WARN("Disconnected, give up writing");
        return;
    }

    // 如果channel现在不是写的状态，并且输出buffer里没有东西，我们直接写数据
    if (!channel_->is_writing() && output_buffer_.readable_bytes() == 0) {
        n_written = write(channel_->fd(), message, len);
        if (n_written >= 0) {
            remaining -= n_written;
            if (remaining == 0 && write_complete_callback_) {
                loop_->queue_in_loop([this] {
                    this->write_complete_callback_(this->shared_from_this());
                });
            }
        } else {
            n_written = 0;
            if (errno != EWOULDBLOCK) {
                LOG_ERROR("TcpConnection::send_in_loop error: {}", strerror(errno));
                if (errno == EPIPE || errno == ECONNRESET) {
                    is_conn_error = true;
                }
            }
        }
    }

    assert(remaining<=len);

    // 正常写入输出buffer中
    if (!is_conn_error && remaining > 0) {
        const size_t written_len = output_buffer_.readable_bytes();

        // 写完这次之后就会超过水位
        if (const size_t need_wrote = written_len + remaining;
            need_wrote >= high_water_mark_ && written_len < high_water_mark_ && high_water_mark_) {
            loop_->queue_in_loop([this,need_wrote] {
                this->high_water_mark_callback_(this->shared_from_this(), need_wrote);
            });
        }
        // 向缓冲区中写数据
        output_buffer_.append(static_cast<const char *>(message) + n_written, remaining);

        // 开启写使能
        if (!channel_->is_writing()) {
            channel_->enable_writing();
        }
    }
}

void hlink::net::TcpConn::shutdown_in_loop() {
    loop_->assert_in_loop_thread();
    if (!channel_->is_writing()) {
        socket_->shutdown_write();
    }
}

void hlink::net::TcpConn::force_close_in_loop() {
    loop_->assert_in_loop_thread();
    if (state_ == State::CONNECTED || state_ == State::DISCONNECTING) {
        handle_close();
    }
}

void hlink::net::TcpConn::set_state(const State state) {
    state_ = state;
}

const char *hlink::net::TcpConn::state_to_string() const {
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

void hlink::net::TcpConn::start_read_in_loop() {
    loop_->assert_in_loop_thread();
    if (!channel_->is_reading() || !reading_flag_) {
        channel_->enable_reading();
        reading_flag_ = true;
    }
}

void hlink::net::TcpConn::stop_read_in_loop() {
    loop_->assert_in_loop_thread();
    if (reading_flag_ && channel_->is_reading()) {
        channel_->disable_reading();
        reading_flag_ = false;
    }
}

hlink::net::TcpConn::TcpConn(EventLoop *loop, const std::string_view name, const int sockfd, const InetAddr &local_addr,
                             const InetAddr &peer_addr) : loop_(loop), name_(name), state_(State::CONNECTING),
                                                          reading_flag_(true),
                                                          socket_(std::make_unique<Socket>(sockfd)),
                                                          channel_(std::make_unique<Channel>(loop, sockfd)),
                                                          local_address_(local_addr),
                                                          peer_address_(peer_addr),
                                                          high_water_mark_(64 * 1024 * 1024) {
    channel_->set_read_callback([this](const TimeStamp receive_time) {
        this->handle_read(receive_time);
    });

    channel_->set_write_callback([this] {
        this->handle_write();
    });

    channel_->set_close_callback([this] {
        this->handle_close();
    });

    channel_->set_error_callback([this] {
        this->handle_error();
    });

    LOG_DEBUG("TcpConnection::cotr[{}] at fd = {}", name_, sockfd);

    socket_->set_keep_alive(true);
}

hlink::net::TcpConn::~TcpConn() {
    LOG_DEBUG("TcpConnection::cotr[{}] at fd = {}, state = {}", name_, channel_->fd(), state_to_string());
}

hlink::net::EventLoop *hlink::net::TcpConn::get_loop() const {
    return loop_;
}

std::string_view hlink::net::TcpConn::get_name() const {
    return name_;
}

const hlink::net::InetAddr &hlink::net::TcpConn::get_local_addr() const {
    return local_address_;
}

const hlink::net::InetAddr &hlink::net::TcpConn::get_peer_addr() const {
    return peer_address_;
}

bool hlink::net::TcpConn::is_connected() const {
    return state_ == State::CONNECTED;
}

bool hlink::net::TcpConn::is_disconnected() const {
    return state_ == State::DISCONNECTED;
}

bool hlink::net::TcpConn::get_tcp_info(tcp_info &tcp_info) const {
    return socket_->get_tcp_info(tcp_info);
}

std::string hlink::net::TcpConn::get_tcp_info_str() const {
    char buf[1024]{};
    socket_->get_tcp_info_string(buf, sizeof(buf));
    return buf;
}

void hlink::net::TcpConn::send(const std::span<std::byte> message) {
    send(message.data(), message.size());
}

void hlink::net::TcpConn::send(const void *message, const size_t bytes) {
    if (state_ == State::CONNECTED) {
        if (loop_->is_in_loop_thread()) {
            send_in_loop(message, bytes);
        } else {
            loop_->run_in_loop([this, message, bytes] {
                this->send_in_loop(message, bytes);
            });
        }
    }
}

void hlink::net::TcpConn::send(const std::string_view message) {
    send(message.data(), message.size());
}

void hlink::net::TcpConn::send(Buffer &&message) {
    if (state_ == State::CONNECTED) {
        if (loop_->is_in_loop_thread()) {
            send_in_loop(message.read_ptr(), message.readable_bytes());
            // 移动的值用完就放弃了，不需要操作
        } else {
            // 这里不能直接传递Buffer的移动，会使得lambda不可拷贝，出现问题
            loop_->run_in_loop([this, message = std::make_shared<Buffer>(std::move(message))] {
                this->send(message->read_ptr(), message->readable_bytes());
                // 移动的值用完就放弃了，不需要操作
            });
        }
    }
}

void hlink::net::TcpConn::send(Buffer &message) {
    send(&message);
}

void hlink::net::TcpConn::send(Buffer *message) {
    if (state_ == State::CONNECTED) {
        if (loop_->is_in_loop_thread()) {
            send_in_loop(message->read_ptr(), message->readable_bytes());
            message->clear();
        } else {
            loop_->run_in_loop([this, message] {
                this->send(message->read_ptr(), message->readable_bytes());
                message->clear();
            });
        }
    }
}

void hlink::net::TcpConn::shutdown() {
    if (state_ == State::CONNECTED) {
        set_state(State::DISCONNECTING);
        loop_->run_in_loop([this] {
            this->shutdown_in_loop();
        });
    }
}

void hlink::net::TcpConn::force_close() {
    if (state_ == State::CONNECTED || state_ == State::DISCONNECTING) {
        set_state(State::DISCONNECTING);
        loop_->run_in_loop([this] {
            this->shared_from_this()->force_close_in_loop();
        });
    }
}

void hlink::net::TcpConn::force_close_with_delay(const int64_t delay_ms) {
    if (state_ == State::CONNECTED || state_ == State::DISCONNECTING) {
        set_state(State::DISCONNECTING);
        loop_->run_after(delay_ms, [this] {
            make_weak_callback(this->shared_from_this(), &TcpConn::force_close);
        });
    }
}

void hlink::net::TcpConn::set_tcp_nodelay(const bool nodelay) {
    socket_->set_tcp_nodelay(nodelay);
}

void hlink::net::TcpConn::start_read() {
    loop_->run_in_loop([this] {
        this->start_read_in_loop();
    });
}

void hlink::net::TcpConn::stop_read() {
    loop_->run_in_loop([this] {
        this->stop_read_in_loop();
    });
}

bool hlink::net::TcpConn::is_reading() const {
    return reading_flag_;
}

void hlink::net::TcpConn::set_connection_callback(ConnectionCallback cb) {
    connection_callback_ = std::move(cb);
}

void hlink::net::TcpConn::set_message_callback(MessageCallback cb) {
    message_callback_ = std::move(cb);
}

void hlink::net::TcpConn::set_close_callback(CloseCallback cb) {
    close_callback_ = std::move(cb);
}

void hlink::net::TcpConn::set_write_complete_callback(WriteCompleteCallback cb) {
    write_complete_callback_ = std::move(cb);
}

void hlink::net::TcpConn::set_high_water_mark_callback(HighWaterMarkCallback cb) {
    high_water_mark_callback_ = std::move(cb);
}

hlink::Buffer &hlink::net::TcpConn::get_input_buffer() {
    return input_buffer_;
}

hlink::Buffer &hlink::net::TcpConn::get_output_buffer() {
    return output_buffer_;
}

void hlink::net::TcpConn::connect_established() {
    loop_->assert_in_loop_thread();
    assert(state_ == State::CONNECTING);
    set_state(State::CONNECTED);
    channel_->tie(shared_from_this());
    channel_->enable_reading();

    if (connection_callback_) {
        connection_callback_(shared_from_this());
    }
}

void hlink::net::TcpConn::connect_destroyed() {
    loop_->assert_in_loop_thread();
    if (state_ == State::CONNECTED) {
        set_state(State::DISCONNECTED);
        channel_->disable_all();
        if (connection_callback_) {
            connection_callback_(shared_from_this());
        }
    }
    channel_->remove();
}
