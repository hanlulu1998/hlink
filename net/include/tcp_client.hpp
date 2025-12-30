//
// Created by KINGE on 2025/12/26.
//

#pragma once
#include <memory>
#include "callback_types.hpp"
#include "noncopyable.hpp"

namespace hlink::net {
    class Connector;
    class InetAddr;
    class EventLoop;

    class TcpClient : public NonCopyable {
    public:
        TcpClient(EventLoop *loop, const InetAddr &server_addr);

        ~TcpClient();

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
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
