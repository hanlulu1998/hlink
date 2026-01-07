//
// Created by KINGE on 2025/12/17.
//
#pragma once

#include "noncopyable.hpp"
#include <memory>
#include "common_types.hpp"

namespace hlink::net {
    class Channel;
    class TimerId;

    class EventLoop : public NonCopyable {
    public:
        using Functor = std::function<void()>;

        EventLoop();

        void init();

        ~EventLoop();

        TimerId run_at(TimeStamp time, TimerCallback cb);

        TimerId run_after(int64_t delay_ms, TimerCallback cb);

        TimerId run_every(int64_t interval_ms, TimerCallback cb);

        void cancel(TimerId timer_id);

        static EventLoop *get_eventloop_of_current_thread();

        void loop();

        void quit();

        bool is_in_loop_thread() const;

        void assert_in_loop_thread() const;

        int64_t iteration() const;

        TimeStamp poll_return_time() const;

        void update_channel(Channel *channel);

        void remove_channel(Channel *channel);

        bool has_channel(const Channel *channel) const;

        bool is_event_handling() const;

        void wakeup();

        void run_in_loop(Functor cb);

        void queue_in_loop(Functor cb);

        size_t queue_size() const;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
