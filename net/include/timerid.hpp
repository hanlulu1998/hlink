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

            TimeStamp expiration() const {
                return expiration_;
            }

            bool repeat() const {
                return repeat_;
            }

            int64_t sequence() const {
                return sequence_;
            }

            static int64_t num_created_timers() {
                return num_created_timers_.load();
            }

            void restart(TimeStamp now);
        private:
            const TimerCallback callback_{nullptr};
            TimeStamp expiration_;
            const ino64_t interval_ms_{0};
            const bool repeat_{false};
            const int64_t sequence_{1};
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
