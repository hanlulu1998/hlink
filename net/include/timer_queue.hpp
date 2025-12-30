//
// Created by KINGE on 2025/12/19.
//
#pragma once
#include <memory>
#include "common_types.hpp"
#include "noncopyable.hpp"

namespace hlink::net {
    class EventLoop;
    class TimerId;

    class TimerQueue : public NonCopyable {
    public:
        explicit TimerQueue(EventLoop *loop);

        ~TimerQueue();

        void init();

        TimerId add_timer(TimerCallback cb, TimeStamp when, int64_t interval);

        void cancel(TimerId timer_id);

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
