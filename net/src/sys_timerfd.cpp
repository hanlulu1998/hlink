//
// Created by KINGE on 2025/12/18.
//

#include "sys_timerfd.hpp"
#include <sys/timerfd.h>
#include <unistd.h>
#include "logging.hpp"
#include <cerrno>

#include "macros.hpp"
#include "utils.hpp"

int hlink::net::create_timerfd() {
    const int timerfd = timerfd_create(CLOCK_MONOTONIC,
                                       TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        LOG_CRITICAL("Failed in create_timerfd: {}", strerror(errno));
    }
    return timerfd;
}

timespec hlink::net::time_from_now(const TimeStamp when) {
    const int microseconds = std::max(100L, time::diff_us(when, time::get_current_time()));
    timespec ts{};
    ts.tv_sec = microseconds / 1000000;
    ts.tv_nsec = microseconds % 1000000 * 1000;
    return ts;
}

void hlink::net::read_timerfd(const int timerfd) {
    uint64_t expirations{0};
    if (ssize_t n = read(timerfd, &expirations, sizeof(expirations)); n != sizeof(expirations)) {
        LOG_ERROR("Timerfd reads {} bytes instead of 8", n);
    }
}

void hlink::net::reset_timerfd(const int timerfd, const TimeStamp expiration) {
    itimerspec new_value{};
    itimerspec old_value{};

    MEM_ZERO_SET(&new_value, sizeof(new_value));
    MEM_ZERO_SET(&old_value, sizeof(old_value));

    new_value.it_value = time_from_now(expiration);
    if (timerfd_settime(timerfd, 0, &new_value, &old_value) < 0) {
        LOG_ERROR("Timerfd resetting error: {}", strerror(errno));
    }
}
