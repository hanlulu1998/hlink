//
// Created by KINGE on 2025/12/16.
//
#include "utils.hpp"
#include <sys/syscall.h>
#include <unistd.h>
#include <format>

hlink::Tid hlink::thread::get_thread_id() {
    thread_local Tid tid = 0;
    if (tid == 0) {
        tid = syscall(SYS_gettid);
    }
    return tid;
}

hlink::TimeStamp hlink::time::get_current_time() {
    return std::chrono::system_clock::now();
}


std::string hlink::time::localtime_to_string(const TimeStamp timestamp) {
    return std::format("{:%F %T}", std::chrono::zoned_time{std::chrono::current_zone(), timestamp});
}

std::string hlink::time::zonetime_to_string(const std::string_view zone, const TimeStamp timestamp) {
    return std::format("{:%F %T}", std::chrono::zoned_time{zone, timestamp});
}

int64_t hlink::time::diff_ms(const TimeStamp t1, const TimeStamp t2) {
    return timestamp_diff<std::chrono::milliseconds>(t1, t2);
}

int64_t hlink::time::diff_ns(const TimeStamp t1, const TimeStamp t2) {
    return timestamp_diff<std::chrono::nanoseconds>(t1, t2);
}

int64_t hlink::time::diff_s(const TimeStamp t1, const TimeStamp t2) {
    return timestamp_diff<std::chrono::seconds>(t1, t2);
}

int64_t hlink::time::diff_us(const TimeStamp t1, const TimeStamp t2) {
    return timestamp_diff<std::chrono::microseconds>(t1, t2);
}

bool hlink::time::is_valid_time(const TimeStamp t) {
    return t != INVALID_TIME;
}
