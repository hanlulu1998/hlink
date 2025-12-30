//
// Created by KINGE on 2025/12/22.
//

#pragma once
#include <memory>
#include <span>
#include "noncopyable.hpp"
#include "callback_types.hpp"
#include "inet_addr.hpp"
#include "buffer.hpp"
struct tcp_info;

namespace hlink::net {
    class Channel;
    class EventLoop;
    class Socket;

    // 由于TcpConn需要频繁和shared_ptr交互，所以不使用PImpl模式，避免生命周期的上下依赖
    class TcpConn : public NonCopyable, public std::enable_shared_from_this<TcpConn> {
    public:
        TcpConn(EventLoop *loop, std::string_view name, int sockfd, const InetAddr &local_addr, const InetAddr &peer_addr);

        ~TcpConn();

        EventLoop *get_loop() const;

        std::string_view get_name() const;

        const InetAddr &get_local_addr() const;

        const InetAddr &get_peer_addr() const;

        bool is_connected() const;

        bool is_disconnected() const;

        bool get_tcp_info(tcp_info &tcp_info) const;

        std::string get_tcp_info_str() const;

        void send(std::span<std::byte> message);

        void send(const void *message, size_t bytes);

        void send(std::string_view message);

        void send(Buffer &&message);

        void send(Buffer &message);

        void send(Buffer *message);

        void shutdown();

        void force_close();

        void force_close_with_delay(int64_t delay_ms);

        void set_tcp_nodelay(bool nodelay);

        void start_read();

        void stop_read();

        bool is_reading() const;

        void set_connection_callback(ConnectionCallback cb);

        void set_message_callback(MessageCallback cb);

        void set_close_callback(CloseCallback cb);

        void set_write_complete_callback(WriteCompleteCallback cb);

        void set_high_water_mark_callback(HighWaterMarkCallback cb);

        Buffer &get_input_buffer();

        Buffer &get_output_buffer();

        void connect_established();

        void connect_destroyed();

    private:
        enum class State {
            DISCONNECTED,
            CONNECTING,
            CONNECTED,
            DISCONNECTING,
        };

        void handle_read(TimeStamp receive_time);

        void handle_write();

        void handle_close();

        void handle_error() const;

        void send_in_loop(const void *message, size_t len);

        void shutdown_in_loop();

        void force_close_in_loop();

        void set_state(State state);

        const char *state_to_string() const;

        void start_read_in_loop();

        void stop_read_in_loop();

        EventLoop *loop_;
        const std::string name_;
        State state_;
        bool reading_flag_;

        std::unique_ptr<Socket> socket_;
        std::unique_ptr<Channel> channel_;
        const InetAddr local_address_;
        const InetAddr peer_address_;

        ConnectionCallback connection_callback_;
        MessageCallback message_callback_;
        WriteCompleteCallback write_complete_callback_;
        HighWaterMarkCallback high_water_mark_callback_;
        CloseCallback close_callback_;

        size_t high_water_mark_;
        Buffer input_buffer_;
        Buffer output_buffer_;
    };
}
