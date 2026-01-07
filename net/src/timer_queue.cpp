//
// Created by KINGE on 2025/12/19.
//

#include "timer_queue.hpp"

#include <cassert>
#include <ranges>
#include <set>

#include "channel.hpp"
#include "eventloop.hpp"
#include "timerid.hpp"
#include "sys_timerfd.hpp"
#include "utils.hpp"

class hlink::net::TimerQueue::Impl {
public:
    explicit Impl(EventLoop *loop);

    void init();

    ~Impl();

    TimerId add_timer(TimerCallback cb, TimeStamp when, int64_t interval);

    void cancel(TimerId timer_id);

private:
    using Entry = std::pair<TimeStamp, TimerId::Timer *>;
    using TimerSet = std::set<Entry>;

    using ActiveTimer = std::pair<TimerId::Timer *, int64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;

    void add_timer_in_loop(TimerId::Timer *timer);

    void cancel_timer_in_loop(TimerId timer_id);

    void handle_read();

    std::vector<Entry> get_expired(TimeStamp now);

    void reset(const std::vector<Entry> &expired, TimeStamp now);

    bool insert(TimerId::Timer *timer);

    EventLoop *loop_{nullptr};
    const int timerfd_{0};
    bool run_expired_timers_{false};


    Channel timer_channel_;
    TimerSet timers_;
    ActiveTimerSet active_timers_;
    ActiveTimerSet cancel_timers_;
};


void hlink::net::TimerQueue::Impl::add_timer_in_loop(TimerId::Timer *timer) {
    loop_->assert_in_loop_thread();
    if (insert(timer)) {
        // 当前插入的最早，所以需要立马调整，否则原来地设置的保持即可
        reset_timerfd(timerfd_, timer->expiration());
    }
}

void hlink::net::TimerQueue::Impl::cancel_timer_in_loop(TimerId timer_id) {
    loop_->assert_in_loop_thread();
    assert(timers_.size() == active_timers_.size());

    const ActiveTimer timer{timer_id.timer_, timer_id.sequence_};
    if (const auto it = active_timers_.find(timer); it != active_timers_.end()) {
        timers_.erase({it->first->expiration(), it->first});
        delete it->first;
        active_timers_.erase(it);
    } else if (run_expired_timers_) {
        // 这个定时器暂时找不到，也不能删除，但是当前回调结束，就当作取消处理
        cancel_timers_.insert(timer);
    }
}

void hlink::net::TimerQueue::Impl::handle_read() {
    loop_->assert_in_loop_thread();
    // 必须读，否则timerfd时间不消失
    read_timerfd(timerfd_);
    const auto now = time::get_current_time();
    auto expired_timers = get_expired(now);

    run_expired_timers_ = true;
    // 清理上一轮的标记
    cancel_timers_.clear();

    for (const auto &val: expired_timers | std::views::values) {
        val->run();
    }

    run_expired_timers_ = false;
    reset(expired_timers, now);
}

std::vector<hlink::net::TimerQueue::Impl::Entry> hlink::net::TimerQueue::Impl::get_expired(TimeStamp now) {
    std::vector<Entry> expired_timers;
    const Entry sentry(now, reinterpret_cast<TimerId::Timer *>(UINTPTR_MAX));

    // 由于set会自动排序，所以我们找到从小到现在已经过期的
    const auto end = timers_.lower_bound(sentry);
    std::copy(timers_.begin(), end, back_inserter(expired_timers));

    timers_.erase(timers_.begin(), end);

    for (const auto &val: expired_timers | std::views::values) {
        ActiveTimer active_timer(val, val->sequence());
        active_timers_.erase(active_timer);
    }

    return expired_timers;
}


void hlink::net::TimerQueue::Impl::reset(const std::vector<Entry> &expired, const TimeStamp now) {
    for (const auto &timer: expired | std::views::values) {
        if (ActiveTimer active_timer(timer, timer->sequence());
            timer->repeat() && !cancel_timers_.contains(active_timer)) {
            timer->restart(now);
            insert(timer);
        } else {
            // 这里会清除cancel_timers_中的timer
            delete timer;
        }
    }

    if (!timers_.empty()) {
        if (const TimeStamp next_expired = timers_.begin()->second->expiration(); time::is_valid_time(next_expired)) {
            reset_timerfd(timerfd_, next_expired);
        }
    }
}

bool hlink::net::TimerQueue::Impl::insert(TimerId::Timer *timer) {
    loop_->assert_in_loop_thread();
    bool earliest_changed = false;
    auto when = timer->expiration();

    // 判断新的定时器是不是最早的那个，如果是的则需要调整当前timerfd的到期时间
    if (const auto it = timers_.begin(); it == timers_.end() || when < it->first) {
        earliest_changed = true;
    }
    timers_.insert({when, timer});
    active_timers_.insert({timer, timer->sequence()});
    return earliest_changed;
}

hlink::net::TimerQueue::Impl::Impl(EventLoop *loop) : loop_(loop),
                                                      timerfd_(create_timerfd()),
                                                      timer_channel_(loop, timerfd_) {
}

void hlink::net::TimerQueue::Impl::init() {
    timer_channel_.set_read_callback([this](TimeStamp) {
        handle_read();
    });
    timer_channel_.enable_reading();
}

hlink::net::TimerQueue::Impl::~Impl() {
    timer_channel_.disable_all();
    timer_channel_.remove();
    close(timerfd_);
    for (const auto &val: timers_ | std::views::values) {
        delete val;
    }
}

hlink::net::TimerId hlink::net::TimerQueue::Impl::add_timer(TimerCallback cb, const TimeStamp when,
                                                            const int64_t interval) {
    auto *timer = new TimerId::Timer(std::move(cb), when, interval);
    loop_->run_in_loop([this,timer] {
        this->add_timer_in_loop(timer);
    });

    return {timer, timer->sequence()};
}

void hlink::net::TimerQueue::Impl::cancel(TimerId timer_id) {
    loop_->run_in_loop([this, timer_id] {
        this->cancel_timer_in_loop(timer_id);
    });
}

hlink::net::TimerQueue::TimerQueue(EventLoop *loop) : impl_(std::make_unique<Impl>(loop)) {
}

hlink::net::TimerQueue::~TimerQueue() = default;

void hlink::net::TimerQueue::init() {
    impl_->init();
}

hlink::net::TimerId hlink::net::TimerQueue::add_timer(TimerCallback cb, const TimeStamp when,
                                                      const int64_t interval) {
    return impl_->add_timer(std::move(cb), when, interval);
}

void hlink::net::TimerQueue::cancel(const TimerId timer_id) {
    impl_->cancel(timer_id);
}
