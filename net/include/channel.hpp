//
// Created by KINGE on 2025/12/17.
//

#pragma once
#include <functional>
#include "common_types.hpp"
#include "noncopyable.hpp"
#include <memory>

namespace hlink::net {
    class EventLoop;

    class Channel : public NonCopyable {
    public:
        using EventCallback = std::function<void()>;
        using ReadEventCallback = std::function<void(TimeStamp)>;

        static constexpr int FLAG_UNADD = -1;
        static constexpr int FLAG_DELETED = 2;
        static constexpr int FLAG_ADDED = 1;


        Channel(EventLoop *loop, int fd);

        ~Channel();

        void handle_event(TimeStamp receive_time);

        void remove();

        void set_read_callback(ReadEventCallback cb);

        void set_write_callback(EventCallback cb);

        void set_close_callback(EventCallback cb) ;

        void set_error_callback(EventCallback cb);

        int fd() const;

        int events() const;

        void set_active_events(int events) ;

        bool is_none_event() const;

        void enable_reading();

        void disable_reading();

        void enable_writing();

        void disable_writing();

        void disable_all();

        bool is_writing() const;

        bool is_reading() const;

        int flag() const;

        void set_flag(int flag);

        EventLoop *owner_loop() const;

        std::string events_to_string() const;

        std::string active_events_to_string() const;

        void tie(const std::shared_ptr<void> &obj);

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
