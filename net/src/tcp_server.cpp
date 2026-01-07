//
// Created by KINGE on 2025/12/22.
//

#include "tcp_server.hpp"
#include <unordered_map>
#include <atomic>
#include <ranges>

#include "buffer.hpp"
#include "acceptor.hpp"
#include "eventloop.hpp"
#include "eventloop_therad_pool.hpp"
#include "inet_addr.hpp"
#include "logging.hpp"
#include "common_types.hpp"
#include "sys_socket.hpp"
#include "tcp_conn.hpp"


inline void default_connection_callback(const hlink::net::TcpConnPtr &connection) {
    LOG_INFO("{} -> {} is {}", connection->get_local_addr().to_ip_port(),
             connection->get_peer_addr().to_ip_port(), connection->is_connected()?"UP":"DOWN");
}

inline void default_message_callback(const hlink::net::TcpConnPtr &, hlink::Buffer *buffer, hlink::TimeStamp) {
    buffer->clear();
}


class hlink::net::TcpServer::Impl {
public:
    Impl(EventLoop *loop, const InetAddr &listen_addr,
         Option option = Option::NO_REUSE_PORT);

    ~Impl();

    void start();

    void set_thread_num(size_t num_threads) const;

    [[nodiscard]] const std::string &ip_port() const;

    [[nodiscard]] EventLoop *get_loop() const;

    void set_thread_init_callback(ThreadInitCallback cb);

    void set_connection_callback(ConnectionCallback cb);

    void set_message_callback(MessageCallback cb);

    void set_write_complete_callback(WriteCompleteCallback cb);

private:
    void new_connection(int sockfd, const InetAddr &peer_address);

    void remove_connection(const TcpConnPtr &conn);

    void remove_connection_in_loop(const TcpConnPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnPtr>;

    EventLoop *loop_;
    const std::string ip_port_;

    std::unique_ptr<Acceptor> acceptor_;
    std::shared_ptr<EventLoopThreadPool> thread_pool_;

    ConnectionCallback connection_callback_;
    MessageCallback message_callback_;
    WriteCompleteCallback write_complete_callback_;
    ThreadInitCallback thread_init_callback_;
    std::atomic_bool started_;
    int next_conn_id_;
    ConnectionMap connections_;
};


hlink::net::TcpServer::Impl::Impl(EventLoop *loop, const InetAddr &listen_addr, const Option option) : loop_(loop),
    ip_port_(listen_addr.to_ip_port()),
    acceptor_(std::make_unique<Acceptor>(loop, listen_addr, option == Option::REUSE_PORT)),
    thread_pool_(std::make_shared<EventLoopThreadPool>(loop)),
    connection_callback_(default_connection_callback),
    message_callback_(default_message_callback),
    started_(false),
    next_conn_id_(1) {
    acceptor_->set_new_connection_callback([this](const int sockfd, const InetAddr &addr) {
        this->new_connection(sockfd, addr);
    });
}

hlink::net::TcpServer::Impl::~Impl() {
    loop_->assert_in_loop_thread();
    for (auto &val: connections_ | std::views::values) {
        TcpConnPtr conn(val);
        val.reset();
        conn->get_loop()->run_in_loop([conn] {
            conn->connect_destroyed();
        });
    }
}

void hlink::net::TcpServer::Impl::start() {
    if (started_ == false) {
        started_ = true;
        thread_pool_->start(thread_init_callback_);
        loop_->run_in_loop([this] {
            this->acceptor_->listen();
        });
    }
}

void hlink::net::TcpServer::Impl::set_thread_num(const size_t num_threads) const {
    thread_pool_->set_thread_num(num_threads);
}

const std::string &hlink::net::TcpServer::Impl::ip_port() const {
    return ip_port_;
}

hlink::net::EventLoop *hlink::net::TcpServer::Impl::get_loop() const {
    return loop_;
}

void hlink::net::TcpServer::Impl::set_thread_init_callback(ThreadInitCallback cb) {
    thread_init_callback_ = std::move(cb);
}

void hlink::net::TcpServer::Impl::set_connection_callback(ConnectionCallback cb) {
    connection_callback_ = std::move(cb);
}

void hlink::net::TcpServer::Impl::set_message_callback(MessageCallback cb) {
    message_callback_ = std::move(cb);
}

void hlink::net::TcpServer::Impl::set_write_complete_callback(WriteCompleteCallback cb) {
    write_complete_callback_ = std::move(cb);
}

void hlink::net::TcpServer::Impl::new_connection(int sockfd, const InetAddr &peer_address) {
    loop_->assert_in_loop_thread();
    EventLoop *io_loop = thread_pool_->get_next_loop();
    ++next_conn_id_;

    std::string conn_name = std::string("conn") + std::to_string(next_conn_id_ - 1);
    LOG_INFO("TcpServer::new_connection {} from {}", conn_name, peer_address.to_ip_port());

    sockaddr_in6 addr{};
    get_local_addr(&addr, sockfd);

    InetAddr local_addr(addr);

    auto conn = std::make_shared<TcpConn>(io_loop, conn_name, sockfd,
                                          local_addr, peer_address);
    connections_[conn_name] = conn;

    conn->set_connection_callback(connection_callback_);
    conn->set_message_callback(message_callback_);
    conn->set_write_complete_callback(write_complete_callback_);
    conn->set_close_callback([this](const TcpConnPtr &conn_ptr) {
        this->remove_connection(conn_ptr);
    });

    io_loop->run_in_loop([conn] {
        conn->connect_established();
    });
}

void hlink::net::TcpServer::Impl::remove_connection(const TcpConnPtr &conn) {
    loop_->run_in_loop([this,conn] {
        this->remove_connection_in_loop(conn);
    });
}

void hlink::net::TcpServer::Impl::remove_connection_in_loop(const TcpConnPtr &conn) {
    loop_->assert_in_loop_thread();
    const size_t n = connections_.erase(std::string(conn->get_name()));
    assert(n == 1);
    EventLoop *io_loop = conn->get_loop();
    io_loop->run_in_loop([conn] {
        conn->connect_destroyed();
    });
}

hlink::net::TcpServer::TcpServer(EventLoop *loop, const InetAddr &listen_addr, Option option) : impl_(
    std::make_unique<Impl>(loop, listen_addr, option)) {
}

hlink::net::TcpServer::~TcpServer() = default;

void hlink::net::TcpServer::start() {
    impl_->start();
}


const std::string &hlink::net::TcpServer::ip_port() const {
    return impl_->ip_port();
}

hlink::net::EventLoop *hlink::net::TcpServer::get_loop() const {
    return impl_->get_loop();
}

void hlink::net::TcpServer::set_thread_num(const int num_threads) {
    impl_->set_thread_num(num_threads);
}

void hlink::net::TcpServer::set_thread_init_callback(ThreadInitCallback cb) {
    impl_->set_thread_init_callback(std::move(cb));
}

void hlink::net::TcpServer::set_connection_callback(ConnectionCallback cb) {
    impl_->set_connection_callback(std::move(cb));
}

void hlink::net::TcpServer::set_message_callback(MessageCallback cb) {
    impl_->set_message_callback(std::move(cb));
}

void hlink::net::TcpServer::set_write_complete_callback(WriteCompleteCallback cb) {
    impl_->set_write_complete_callback(std::move(cb));
}
