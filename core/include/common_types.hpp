//
// Created by KINGE on 2025/12/16.
//
#pragma once
#include <chrono>
#include <functional>

namespace hlink {
    using Tid = long int;
    using TimeStamp = std::chrono::system_clock::time_point;
    constexpr TimeStamp INVALID_TIME = TimeStamp::min();
    using TimerCallback = std::function<void()>;
}
