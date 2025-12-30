//
// Created by KINGE on 2025/12/22.
//
#pragma once
#include <functional>
#include <memory>

#include "noncopyable.hpp"

namespace hlink::net {
    class EventLoop;
    class InetAddr;

    class Acceptor : public NonCopyable {
    public:
        using NewConnectionCallback = std::function<void(int, const InetAddr &)>;

        void set_new_connection_callback(NewConnectionCallback cb);

        ~Acceptor();

        Acceptor(EventLoop *loop, const InetAddr &listen_address, bool reuse_port);

        void listen();

        bool is_listening() const;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
