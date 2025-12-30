//
// Created by KINGE on 2025/12/18.
//
#pragma once
#include "common_types.hpp"

namespace hlink::net {
    int create_timerfd();

    timespec time_from_now(TimeStamp when);

    void read_timerfd(int timerfd);

    void reset_timerfd(int timerfd, TimeStamp expiration);
}
