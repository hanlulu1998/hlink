//
// Created by KINGE on 2025/12/29.
//
#include "tcp_server.hpp"
#include "eventloop.hpp"
#include <gtest/gtest.h>
#include "inet_addr.hpp"
#include "callback_types.hpp"
#include "tcp_conn.hpp"
using namespace hlink::net;
using namespace hlink;

class EchoServer {
public:
    EchoServer(EventLoop *loop, const InetAddr &listen_addr) : loop_(loop),
                                                               server_(loop, listen_addr) {
        server_.set_connection_callback([this](const TcpConnPtr &conn) {
            this->on_connection(conn);
        });

        server_.set_message_callback([this](const TcpConnPtr &conn, Buffer *buf, TimeStamp time) {
            this->on_message(conn, buf, time);
        });

        server_.set_thread_num(1);
    }

    void start() {
        server_.start();
    }

    void set_thread_num(const int num_threads) {
        server_.set_thread_num(num_threads);
    }

private:
    EventLoop *loop_;
    TcpServer server_;

    void on_connection(const TcpConnPtr &conn) {
        GTEST_LOG_(INFO) << conn->get_peer_addr().to_ip_port() << " -> "
                << conn->get_local_addr().to_ip_port() << " is "
                << (conn->is_connected() ? "UP" : "DOWN");

        GTEST_LOG_(INFO) << conn->get_tcp_info_str();
        conn->send("hello\n");
    }

    void on_message(const TcpConnPtr &conn, Buffer *buf, TimeStamp time) {
        const std::string message = buf->pop_all_as_string();
        GTEST_LOG_(INFO) << conn->get_name() << " recv " << message.size() << "bytes at " << time;
        if (message == "exit\n") {
            conn->send("byte\n");
            conn->shutdown();
        }
        if (message == "quit\n") {
            loop_->quit();
        }

        conn->send(message);
    }
};

TEST(TestTcpServerCase, TestEchoServer) {
    EventLoop loop;
    loop.init();
    const InetAddr listen_addr(2000, false, false);
    EchoServer server(&loop, listen_addr);
    server.set_thread_num(4);
    server.start();
    loop.loop();
}
