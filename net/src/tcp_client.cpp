//
// Created by KINGE on 2025/12/26.
//

#include "tcp_client.hpp"
#include "eventloop.hpp"
#include "connector.hpp"
#include "inet_addr.hpp"
#include "tcp_conn.hpp"
#include "buffer.hpp"
#include "logging.hpp"
#include <mutex>
#include "channel.hpp"
#include "macros.hpp"
#include "sys_socket.hpp"
#include "timerid.hpp"

inline void default_connection_callback(const hlink::net::TcpConnPtr &connection) {
    LOG_INFO("{} -> {} is {}", connection->get_local_addr().to_ip_port(),
             connection->get_peer_addr().to_ip_port(), connection->is_connected()?"UP":"DOWN");
}

inline void default_message_callback(const hlink::net::TcpConnPtr &, hlink::Buffer *buffer, hlink::TimeStamp) {
    buffer->clear();
}


class hlink::net::TcpClient::Impl {
public:
    Impl(EventLoop *loop, const InetAddr &server_addr);

    ~Impl();

    void connect();

    void disconnect();

    void stop();

    TcpConnPtr connection();

    [[nodiscard]] EventLoop *get_loop() const;

    [[nodiscard]] bool is_enabled_retry() const;

    void enable_retry();

    void set_connection_callback(ConnectionCallback cb);

    void set_message_callback(MessageCallback cb);

    void set_write_complete_callback(WriteCompleteCallback cb);

private:
    void new_connection(int sockfd);

    void remove_connection(const TcpConnPtr &conn);

    EventLoop *loop_;
    ConnectorPtr connector_;

    ConnectionCallback connection_callback_;
    MessageCallback message_callback_;
    WriteCompleteCallback write_complete_callback_;

    bool retry_{false};
    bool connected_{true};

    int next_conn_id_{1};
    std::mutex mutex_;
    TcpConnPtr connection_;
};


hlink::net::TcpClient::Impl::Impl(EventLoop *loop, const InetAddr &server_addr) : loop_(loop),
    connector_(std::make_shared<Connector>(loop, server_addr)),
    connection_callback_(default_connection_callback),
    message_callback_(default_message_callback) {
    connector_->set_new_connection_callback([this](const int sockfd) {
        this->new_connection(sockfd);
    });
}

hlink::net::TcpClient::Impl::~Impl() {
    TcpConnPtr conn;
    bool unique = false; {
        std::lock_guard lg(mutex_);
        unique = connection_.unique();
        conn = connection_;
    }

    auto close_callback = [this](const TcpConnPtr &conn) {
        this->loop_->queue_in_loop([conn] {
            conn->connect_destroyed();
        });
    };

    if (conn) {
        loop_->run_in_loop([conn, close_callback] {
            conn->set_close_callback(close_callback);
        });
        if (unique) {
            conn->force_close();
        }
    } else {
        connector_->stop();
        loop_->run_after(1000, [conn,close_callback] {
            close_callback(conn);
        });
    }
}

void hlink::net::TcpClient::Impl::connect() {
    connected_ = true;
    connector_->start();
}

void hlink::net::TcpClient::Impl::disconnect() {
    connected_ = false; {
        std::lock_guard lg(mutex_);
        if (connection_) {
            connection_->shutdown();
        }
    }
}

void hlink::net::TcpClient::Impl::stop() {
    connected_ = false;
    connector_->stop();
}

hlink::net::TcpConnPtr hlink::net::TcpClient::Impl::connection() {
    return connection_;
}

hlink::net::EventLoop *hlink::net::TcpClient::Impl::get_loop() const {
    return loop_;
}

bool hlink::net::TcpClient::Impl::is_enabled_retry() const {
    return retry_;
}

void hlink::net::TcpClient::Impl::enable_retry() {
    retry_ = true;
}

void hlink::net::TcpClient::Impl::set_connection_callback(ConnectionCallback cb) {
    connection_callback_ = std::move(cb);
}

void hlink::net::TcpClient::Impl::set_message_callback(MessageCallback cb) {
    message_callback_ = std::move(cb);
}

void hlink::net::TcpClient::Impl::set_write_complete_callback(WriteCompleteCallback cb) {
    write_complete_callback_ = std::move(cb);
}

void hlink::net::TcpClient::Impl::new_connection(int sockfd) {
    loop_->assert_in_loop_thread();
    sockaddr_in6 addr{};
    get_peer_addr(&addr, sockfd);
    InetAddr peer_addr(addr);
    MEM_ZERO_SET(&addr, 0);
    get_local_addr(&addr, sockfd);
    InetAddr local_addr(addr);

    ++next_conn_id_;
    auto conn = std::make_shared<TcpConn>(
        loop_,
        std::string("conn") + std::to_string(next_conn_id_ - 1),
        sockfd,
        local_addr,
        peer_addr
    );

    conn->set_connection_callback(connection_callback_);
    conn->set_message_callback(message_callback_);
    conn->set_write_complete_callback(write_complete_callback_);
    conn->set_close_callback([this](const TcpConnPtr &conn) {
        this->remove_connection(conn);
    }); {
        std::lock_guard lg(mutex_);
        connection_ = conn;
    }

    conn->connect_established();
}

void hlink::net::TcpClient::Impl::remove_connection(const TcpConnPtr &conn) {
    loop_->assert_in_loop_thread(); {
        std::lock_guard lg(mutex_);
        connection_.reset();
    }
    loop_->queue_in_loop([conn] {
        conn->connect_destroyed();
    });

    if (retry_ && connected_) {
        connector_->restart();
    }
}

hlink::net::TcpClient::TcpClient(EventLoop *loop, const InetAddr &server_addr) : impl_(
    std::make_unique<Impl>(loop, server_addr)) {
}

hlink::net::TcpClient::~TcpClient() = default;


void hlink::net::TcpClient::connect() {
    impl_->connect();
}

void hlink::net::TcpClient::disconnect() {
    impl_->disconnect();
}

void hlink::net::TcpClient::stop() {
    impl_->stop();
}

hlink::net::TcpConnPtr hlink::net::TcpClient::connection() {
    return impl_->connection();
}

hlink::net::EventLoop *hlink::net::TcpClient::get_loop() const {
    return impl_->get_loop();
}

bool hlink::net::TcpClient::is_enabled_retry() const {
    return impl_->is_enabled_retry();
}

void hlink::net::TcpClient::enable_retry() {
    impl_->enable_retry();
}

void hlink::net::TcpClient::set_connection_callback(ConnectionCallback cb) {
    impl_->set_connection_callback(std::move(cb));
}

void hlink::net::TcpClient::set_message_callback(MessageCallback cb) {
    impl_->set_message_callback(std::move(cb));
}

void hlink::net::TcpClient::set_write_complete_callback(WriteCompleteCallback cb) {
    impl_->set_write_complete_callback(std::move(cb));
}
