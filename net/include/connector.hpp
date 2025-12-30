//
// Created by KINGE on 2025/12/26.
//

#pragma once
#include <functional>
#include <memory>

#include "inet_addr.hpp"
#include "noncopyable.hpp"

namespace hlink::net {
    class Channel;
    class EventLoop;

    class Connector : public NonCopyable, public std::enable_shared_from_this<Connector> {
    public:
        using NewConnectionCallback = std::function<void(int sockfd)>;

        Connector(EventLoop *loop, const InetAddr &server_addr);

        void set_new_connection_callback(NewConnectionCallback cb);

        void start();

        void stop();

        void restart();

        const InetAddr &get_server_addr() const;

    private:
        enum class State {
            DISCONNECTED,
            CONNECTING,
            CONNECTED,
            DISCONNECTING,
        };

        static constexpr int MAX_RETRY_DELAY_MS = 30 * 1000;
        static constexpr int INIT_RETRY_DELAY_MS = 500;

        void set_state(State state);

        void start_in_loop();

        void stop_in_loop();

        void connect();

        void connecting(int sockfd);

        void handle_write();

        void handle_error();

        void retry(int sockfd);

        int remove_and_reset_channel();

        void reset_channel();

        const char *state_to_string() const;

        EventLoop *loop_;
        InetAddr server_addr_;
        bool connected_;
        State state_;
        std::unique_ptr<Channel> channel_;
        NewConnectionCallback new_connection_callback_;
        int retry_delay_ms_;
    };
}
