//
// Created by KINGE on 2025/12/16.
//
#pragma once
#include "common_types.hpp"
#include <string>

namespace hlink {
    namespace thread {
        Tid get_thread_id();
    }

    namespace time {
        TimeStamp get_current_time();

        std::string localtime_to_string(TimeStamp timestamp);

        std::string zonetime_to_string(std::string_view zone, TimeStamp timestamp);

        template<typename T>
        auto timestamp_diff(const TimeStamp t1, const TimeStamp t2) {
            auto duration = std::chrono::duration_cast<T>(t1 - t2);
            return duration.count();
        }

        int64_t diff_ms(TimeStamp t1, TimeStamp t2);

        int64_t diff_ns(TimeStamp t1, TimeStamp t2);

        int64_t diff_s(TimeStamp t1, TimeStamp t2);

        int64_t diff_us(TimeStamp t1, TimeStamp t2);

        bool is_valid_time(TimeStamp t);
    }
}
