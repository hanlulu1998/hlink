//
// Created by KINGE on 2025/12/22.
//

#pragma once
#include <functional>
#include <memory>
#include <string>
#include "noncopyable.hpp"
#include "callback_types.hpp"

namespace hlink::net {
    class EventLoop;
    class InetAddr;

    class TcpServer : public NonCopyable {
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;

        enum class Option {
            NO_REUSE_PORT,
            REUSE_PORT,
        };

        TcpServer(EventLoop *loop, const InetAddr &listen_addr,
                  Option option = Option::NO_REUSE_PORT);

        ~TcpServer();

        void start();

        [[nodiscard]] const std::string &ip_port() const;

        [[nodiscard]] EventLoop *get_loop() const;

        void set_thread_num(int num_threads);

        void set_thread_init_callback(ThreadInitCallback cb);

        void set_connection_callback(ConnectionCallback cb);

        void set_message_callback(MessageCallback cb);

        void set_write_complete_callback(WriteCompleteCallback cb);

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
