//
// Created by KINGE on 2025/12/29.
//

#include <thread>

#include "tcp_client.hpp"
#include "eventloop.hpp"
#include <gtest/gtest.h>
#include "inet_addr.hpp"
#include "callback_types.hpp"
#include "tcp_conn.hpp"
using namespace hlink::net;
using namespace hlink;

class ChatClient {
public:
    ChatClient(EventLoop *loop, const InetAddr &server_addr) : loop_(loop),
                                                               client_(loop, server_addr) {
        client_.set_connection_callback([this](const TcpConnPtr &conn) {
            this->on_connection(conn);
        });

        client_.set_message_callback([this](const TcpConnPtr &conn, Buffer *buf, const TimeStamp time) {
            this->on_message(conn, buf, time);
        });
    }

    void connect() {
        client_.connect();
    }

    void send(const std::string_view message) {
        this->client_.connection()->send(message);
    }

private:
    EventLoop *loop_;
    TcpClient client_;

    void on_connection(const TcpConnPtr &conn) {
        GTEST_LOG_(INFO) << conn->get_local_addr().to_ip_port() << " -> " << conn->get_peer_addr().to_ip_port()
                << " is "
                << (conn->is_connected() ? "UP" : "DOWN");

        GTEST_LOG_(INFO) << conn->get_tcp_info_str();
        conn->send("hello\n");
    }

    void on_message(const TcpConnPtr &conn, Buffer *buf, TimeStamp time) {
        const std::string message = buf->pop_all_as_string();
        std::cout << message << std::flush;
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

TEST(TestTcpClientCase, TestEchoClient) {
    EventLoop loop;
    loop.init();
    const InetAddr server_addr("127.0.0.1", 2000);
    ChatClient client(&loop, server_addr);
    client.connect();

    std::jthread thread([&client] {
        std::string line;
        while (std::getline(std::cin, line)) {
            client.send(line + '\n');
            if (line == "exit" || line == "quit") {
                break;
            }
        }
    });
    loop.loop();
}
