//
// Created by KINGE on 2025/12/17.
//

#include "eventloop.hpp"

#include "channel.hpp"
#include "logging.hpp"
#include "utils.hpp"
#include "macros.hpp"
#include <mutex>
#include "poller.hpp"
#include <sys/eventfd.h>
#include "sys_socket.hpp"
#include "timerid.hpp"
#include "timer_queue.hpp"
thread_local hlink::net::EventLoop *loop_in_this_thread = nullptr;

class hlink::net::EventLoop::Impl {
public:
    explicit Impl(EventLoop *loop) : parent_loop_(loop), tid_(thread::get_thread_id()),
                                     poller_(Poller::new_poller(loop)),
                                     timer_queue_(std::make_unique<TimerQueue>(loop)),
                                     wakeup_fd_(eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC)),
                                     wakeup_channel_(std::make_unique<Channel>(parent_loop_, wakeup_fd_)) {
    }

    void init();

    ~Impl() {
        wakeup_channel_->disable_all();
        wakeup_channel_->remove();
        close(wakeup_fd_);
    }

    TimerId run_at(TimeStamp time, TimerCallback cb);

    TimerId run_after(int64_t delay_ms, TimerCallback cb);

    TimerId run_every(int64_t interval_ms, TimerCallback cb);

    void cancel(TimerId timer_id);

    bool is_in_loop_thread() const;

    void assert_in_loop_thread() const;

    void run_in_loop(Functor cb);

    void queue_in_loop(Functor cb);

    void loop();

    TimeStamp poll_return_time() const;

    void quit();

    int64_t iteration() const;

    void update_channel(Channel *channel);

    void remove_channel(Channel *channel);

    bool has_channel(const Channel *channel) const;

    bool is_event_handling() const;

    void wakeup();

    size_t queue_size() const;

private:
    void handle_wakeup();

    void do_queued_functors();

    EventLoop *const parent_loop_{nullptr};


    using ChannelList = std::vector<Channel *>;
    const Tid tid_{0};
    mutable std::mutex mutex_{};

    std::vector<Functor> queue_functors_{};
    bool run_queue_functors_{false};
    bool looping_flag_{false};

    std::atomic_bool quit_flag_{false};
    int64_t iteration_{0};
    std::mutex mtx_;

    bool event_handling_{false};
    Channel *current_active_channel_{nullptr};
    ChannelList active_channels_;


    std::unique_ptr<Poller> poller_;

    TimeStamp poll_return_time_;

    std::unique_ptr<TimerQueue> timer_queue_;

    int wakeup_fd_{-1};

    std::unique_ptr<Channel> wakeup_channel_;

    static constexpr int POLL_TIMEOUT_MS = 10000;
};

hlink::net::TimerId hlink::net::EventLoop::Impl::run_at(const TimeStamp time, TimerCallback cb) {
    return timer_queue_->add_timer(std::move(cb), time, 0);
}

hlink::net::TimerId hlink::net::EventLoop::Impl::run_after(const int64_t delay_ms, TimerCallback cb) {
    return run_at(time::get_current_time() + std::chrono::milliseconds{delay_ms}, std::move(cb));
}

hlink::net::TimerId hlink::net::EventLoop::Impl::run_every(const int64_t interval_ms, TimerCallback cb) {
    return timer_queue_->add_timer(std::move(cb), time::get_current_time() + std::chrono::milliseconds{interval_ms},
                                   interval_ms);
}

void hlink::net::EventLoop::Impl::cancel(const TimerId timer_id) {
    return timer_queue_->cancel(timer_id);
}

void hlink::net::EventLoop::Impl::handle_wakeup() {
    uint64_t one = 1;
    if (ssize_t n = read(wakeup_fd_, &one, sizeof(one)); n != sizeof(one)) {
        LOG_ERROR("EventLoop::handle_wakeup reads {} bytes instead of 8", n);
    }
}

void hlink::net::EventLoop::Impl::do_queued_functors() {
    std::vector<Functor> functors;
    run_queue_functors_ = true;
    // 这里需要加锁，因为别的线程也在插入任务
    {
        std::lock_guard lg(mtx_);
        if (queue_functors_.empty()) {
            run_queue_functors_ = false;
            return;
        }
        functors.swap(queue_functors_);
    }
    for (const auto &functor: functors) {
        functor();
    }
    run_queue_functors_ = false;
}

void hlink::net::EventLoop::Impl::loop() {
    assert(!looping_flag_);
    assert_in_loop_thread();
    looping_flag_ = true;

    while (!quit_flag_) {
        active_channels_.clear();
        poll_return_time_ = poller_->poll(POLL_TIMEOUT_MS, &active_channels_);
        ++iteration_;
        event_handling_ = true;
        for (Channel *channel: active_channels_) {
            current_active_channel_ = channel;
            current_active_channel_->handle_event(poll_return_time_);
        }

        current_active_channel_ = nullptr;
        event_handling_ = false;
        do_queued_functors();
    }
    looping_flag_ = false;
}

hlink::TimeStamp hlink::net::EventLoop::Impl::poll_return_time() const {
    return poll_return_time_;
}

void hlink::net::EventLoop::Impl::quit() {
    quit_flag_ = true;
    if (!is_in_loop_thread()) {
        wakeup();
    }
}

int64_t hlink::net::EventLoop::Impl::iteration() const {
    return iteration_;
}


void hlink::net::EventLoop::Impl::update_channel(Channel *channel) {
    assert(channel->owner_loop() == parent_loop_);
    assert_in_loop_thread();
    poller_->update_channel(channel);
}

void hlink::net::EventLoop::Impl::remove_channel(Channel *channel) {
    assert(channel->owner_loop() == parent_loop_);
    assert_in_loop_thread();
    if (event_handling_) {
        assert(
            current_active_channel_ == channel || std::find(active_channels_.begin(),active_channels_.end(),channel) ==
            active_channels_.end());
    }
    poller_->remove_channel(channel);
}

bool hlink::net::EventLoop::Impl::has_channel(const Channel *channel) const {
    assert(channel->owner_loop() == parent_loop_);
    assert_in_loop_thread();
    return poller_->has_channel(channel);
}

bool hlink::net::EventLoop::Impl::is_event_handling() const {
    return event_handling_;
}

void hlink::net::EventLoop::Impl::wakeup() {
    constexpr uint64_t one = 1;
    if (write(wakeup_fd_, &one, sizeof(one)) != sizeof(one)) {
        LOG_ERROR("Failed to write to wakeup fd");
    }
}

size_t hlink::net::EventLoop::Impl::queue_size() const {
    std::lock_guard lg(mutex_);
    return queue_functors_.size();
}

void hlink::net::EventLoop::Impl::init() {
    timer_queue_->init();
    wakeup_channel_->set_read_callback([this](TimeStamp) {
        this->handle_wakeup();
    });
    wakeup_channel_->enable_reading();
}

bool hlink::net::EventLoop::Impl::is_in_loop_thread() const {
    return tid_ == thread::get_thread_id();
}

void hlink::net::EventLoop::Impl::assert_in_loop_thread() const {
    if (!is_in_loop_thread()) {
        LOG_CRITICAL("EventLoop {:#x} was created in thread_id = {}, current thread_id = {}",
                     PTR_ADDRESS(this),
                     tid_, thread::get_thread_id()
        );
    }
}

void hlink::net::EventLoop::Impl::run_in_loop(Functor cb) {
    if (is_in_loop_thread()) {
        cb();
    } else {
        queue_in_loop(std::move(cb));
    }
}

void hlink::net::EventLoop::Impl::queue_in_loop(Functor cb) { {
        std::lock_guard lg(mutex_);
        queue_functors_.emplace_back(std::move(cb));
    }
    if (!is_in_loop_thread() || run_queue_functors_) {
        wakeup();
    }
}


hlink::net::EventLoop::EventLoop() : impl_(std::make_unique<Impl>(this)) {
    if (loop_in_this_thread) {
        LOG_CRITICAL("Another EventLoop {:#x} exists in thread {}", PTR_ADDRESS(this), thread::get_thread_id());
    }
    loop_in_this_thread = this;
}

void hlink::net::EventLoop::init() {
    impl_->init();
}

hlink::net::EventLoop::~EventLoop() {
    loop_in_this_thread = nullptr;
}

hlink::net::TimerId hlink::net::EventLoop::run_at(const TimeStamp time, TimerCallback cb) {
    return impl_->run_at(time, std::move(cb));
}

hlink::net::TimerId hlink::net::EventLoop::run_after(const int64_t delay_ms, TimerCallback cb) {
    return impl_->run_after(delay_ms, std::move(cb));
}

hlink::net::TimerId hlink::net::EventLoop::run_every(const int64_t interval_ms, TimerCallback cb) {
    return impl_->run_every(interval_ms, std::move(cb));
}

void hlink::net::EventLoop::cancel(const TimerId timer_id) {
    return impl_->cancel(timer_id);
}

hlink::net::EventLoop *hlink::net::EventLoop::get_eventloop_of_current_thread() {
    return loop_in_this_thread;
}

void hlink::net::EventLoop::loop() {
    impl_->loop();
}

void hlink::net::EventLoop::quit() {
    impl_->quit();
}

bool hlink::net::EventLoop::is_in_loop_thread() const {
    return impl_->is_in_loop_thread();
}

void hlink::net::EventLoop::assert_in_loop_thread() const {
    return impl_->assert_in_loop_thread();
}

int64_t hlink::net::EventLoop::iteration() const {
    return impl_->iteration();
}

hlink::TimeStamp hlink::net::EventLoop::poll_return_time() const {
    return impl_->poll_return_time();
}

void hlink::net::EventLoop::update_channel(Channel *channel) {
    impl_->update_channel(channel);
}

void hlink::net::EventLoop::remove_channel(Channel *channel) {
    impl_->remove_channel(channel);
}

bool hlink::net::EventLoop::has_channel(const Channel *channel) const {
    return impl_->has_channel(channel);
}

bool hlink::net::EventLoop::is_event_handling() const {
    return impl_->is_event_handling();
}

void hlink::net::EventLoop::wakeup() {
    impl_->wakeup();
}

void hlink::net::EventLoop::run_in_loop(Functor cb) {
    impl_->run_in_loop(std::move(cb));
}

void hlink::net::EventLoop::queue_in_loop(Functor cb) {
    impl_->queue_in_loop(std::move(cb));
}

size_t hlink::net::EventLoop::queue_size() const {
    return impl_->queue_size();
}
