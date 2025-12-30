//
// Created by KINGE on 2025/12/18.
//

#include "timerid.hpp"

hlink::net::TimerId::Timer::Timer(TimerCallback cb, const TimeStamp when,
                                  const int64_t interval_ms) : callback_(std::move(cb)),
                                                               expiration_(when),
                                                               interval_ms_(interval_ms),
                                                               repeat_(interval_ms > 0),
                                                               sequence_(
                                                                   num_created_timers_.fetch_add(1)) {
}

void hlink::net::TimerId::Timer::restart(const TimeStamp now) {
    if (repeat_) {
        expiration_ = now + std::chrono::milliseconds{
                          interval_ms_
                      };
    } else {
        expiration_ = INVALID_TIME;
    }
}

hlink::net::TimerId::TimerId() = default;

hlink::net::TimerId::TimerId(Timer *timer, const int64_t seq) : timer_(timer), sequence_(seq) {
}
