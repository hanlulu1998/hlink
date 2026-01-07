//
// Created by KINGE on 2025/12/18.
//
#pragma once
#include <atomic>
#include "noncopyable.hpp"
#include "common_types.hpp"

namespace hlink::net {
    class TimerId {
    public:
        class Timer : public NonCopyable {
        public:
            Timer(TimerCallback cb, TimeStamp when, int64_t interval_ms);

            void run() const {
                callback_();
            }

            [[nodiscard]] TimeStamp expiration() const {
                return expiration_;
            }

            [[nodiscard]] bool repeat() const {
                return repeat_;
            }

            [[nodiscard]] int64_t sequence() const {
                return sequence_;
            }

            static int64_t num_created_timers() {
                return num_created_timers_.load();
            }

            void restart(TimeStamp now);

        private:
            const TimerCallback callback_{nullptr};
            TimeStamp expiration_;
            const int64_t interval_ms_;
            const bool repeat_;
            const int64_t sequence_;
            inline static std::atomic_int64_t num_created_timers_{0};
        };

        TimerId();

        TimerId(Timer *timer, int64_t seq);

        friend class TimerQueue;

    private:
        Timer *timer_{nullptr};
        int64_t sequence_{1};
    };
}
