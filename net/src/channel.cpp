//
// Created by KINGE on 2025/12/17.
//

#include "channel.hpp"
#include "eventloop.hpp"
#include <poll.h>
#include <sstream>
#include "logging.hpp"

class hlink::net::Channel::Impl {
public:
    Impl(EventLoop *loop, int fd);

    void handle_event(TimeStamp receive_time);

    void remove(Channel *channel);

    void set_read_callback(ReadEventCallback cb);

    void set_write_callback(EventCallback cb);

    void set_close_callback(EventCallback cb);

    void set_error_callback(EventCallback cb);

    int fd() const;

    int events() const;

    void set_active_events(int events);

    bool is_none_event() const;

    void enable_reading(Channel *channel);

    void disable_reading(Channel *channel);

    void enable_writing(Channel *channel);

    void disable_writing(Channel *channel);

    void disable_all(Channel *channel);

    bool is_writing() const;

    bool is_reading() const;

    int flag() const;

    void set_flag(int flag);

    EventLoop *owner_loop() const;

    std::string events_to_string() const;

    void tie(const std::shared_ptr<void> &obj);

    std::string active_events_to_string() const;

private:
    void update(Channel *channel);

    static std::string events_to_string(int fd, int events);

    void handle_event_with_guard(TimeStamp receive_time);

    EventLoop *loop_{nullptr};
    const int fd_{0};
    int events_{0};
    int active_events_{0};
    int flag_{FLAG_UNADD};

    bool event_handling_{false};
    bool added_to_loop_{false};

    ReadEventCallback read_callback_;
    EventCallback write_callback_;
    EventCallback error_callback_;
    EventCallback close_callback_;

    std::weak_ptr<void> self_tie_;
    bool is_tied_{false};

    static constexpr int NONE_EVENT = 0;
    static constexpr int READ_EVENT = POLLIN | POLLPRI;
    static constexpr int WRITE_EVENT = POLLOUT;
};

hlink::net::Channel::Impl::Impl(EventLoop *loop, const int fd) : loop_(loop), fd_(fd) {
}

void hlink::net::Channel::Impl::handle_event(const TimeStamp receive_time) {
    // 由于channel只保存fd和回调，如果真正持有channel的业务对象TcpConnection析构了，就会产生空指针
    if (is_tied_) {
        if (auto guard = self_tie_.lock()) {
            handle_event_with_guard(receive_time);
        }
    } else {
        handle_event_with_guard(receive_time);
    }
}

void hlink::net::Channel::Impl::remove(Channel *channel) {
    assert(is_none_event());
    added_to_loop_ = false;
    loop_->remove_channel(channel);
}


void hlink::net::Channel::Impl::set_read_callback(ReadEventCallback cb) {
    read_callback_ = std::move(cb);
}

void hlink::net::Channel::Impl::set_write_callback(EventCallback cb) {
    write_callback_ = std::move(cb);
}

void hlink::net::Channel::Impl::set_close_callback(EventCallback cb) {
    close_callback_ = std::move(cb);
}

void hlink::net::Channel::Impl::set_error_callback(EventCallback cb) {
    error_callback_ = std::move(cb);
}

int hlink::net::Channel::Impl::fd() const {
    return fd_;
}

int hlink::net::Channel::Impl::events() const {
    return events_;
}

void hlink::net::Channel::Impl::set_active_events(const int events) {
    active_events_ = events;
}

bool hlink::net::Channel::Impl::is_none_event() const {
    return events_ == NONE_EVENT;
}

void hlink::net::Channel::Impl::enable_reading(Channel *channel) {
    events_ |= READ_EVENT;
    update(channel);
}

void hlink::net::Channel::Impl::disable_reading(Channel *channel) {
    events_ &= ~READ_EVENT;
    update(channel);
}

void hlink::net::Channel::Impl::enable_writing(Channel *channel) {
    events_ |= WRITE_EVENT;
    update(channel);
}

void hlink::net::Channel::Impl::disable_writing(Channel *channel) {
    events_ &= ~WRITE_EVENT;
    update(channel);
}

void hlink::net::Channel::Impl::disable_all(Channel *channel) {
    events_ = NONE_EVENT;
    update(channel);
}

bool hlink::net::Channel::Impl::is_writing() const {
    return events_ & WRITE_EVENT;
}

bool hlink::net::Channel::Impl::is_reading() const {
    return events_ & READ_EVENT;
}

int hlink::net::Channel::Impl::flag() const {
    return flag_;
}

void hlink::net::Channel::Impl::set_flag(int flag) {
    flag_ = flag;
}

hlink::net::EventLoop *hlink::net::Channel::Impl::owner_loop() const {
    return loop_;
}

std::string hlink::net::Channel::Impl::events_to_string() const {
    return events_to_string(fd_, events_);
}

void hlink::net::Channel::Impl::tie(const std::shared_ptr<void> &obj) {
    // 用来确定自己是否被析构
    self_tie_ = obj;
    is_tied_ = true;
}

std::string hlink::net::Channel::Impl::active_events_to_string() const {
    return events_to_string(fd_, active_events_);
}

void hlink::net::Channel::Impl::update(Channel *channel) {
    added_to_loop_ = true;
    loop_->update_channel(channel);
}

std::string hlink::net::Channel::Impl::events_to_string(const int fd, const int events) {
    std::ostringstream oss;
    oss << fd << ": ";
    if (events & POLLIN) {
        oss << "POLLIN ";
    }

    if (events & POLLPRI) {
        oss << "POLLPRI ";
    }

    if (events & POLLOUT) {
        oss << "POLLOUT ";
    }

    if (events & POLLHUP) {
        oss << "POLLHUP ";
    }

    if (events & POLLRDHUP) {
        oss << "POLLRDHUP ";
    }

    if (events & POLLERR) {
        oss << "POLLERR ";
    }

    if (events & POLLNVAL) {
        oss << "POLLNVAL ";
    }
    return oss.str();
}

void hlink::net::Channel::Impl::handle_event_with_guard(const TimeStamp receive_time) {
    event_handling_ = true;

    if (active_events_ & POLLHUP && !(active_events_ & POLLIN)) {
        LOG_WARN("fd = {} Channel::handle_event POLLHUP", fd_);
        if (close_callback_) {
            close_callback_();
        }
    }

    if (active_events_ & POLLNVAL) {
        LOG_WARN("Channel:handle_event POLLNVAL");
    }

    if (active_events_ & (POLLERR | POLLNVAL)) {
        if (error_callback_) {
            error_callback_();
        }
    }

    if (active_events_ & (POLLIN | POLLPRI | POLLHUP)) {
        if (read_callback_) {
            read_callback_(receive_time);
        }
    }

    if (active_events_ & POLLOUT) {
        if (write_callback_) {
            write_callback_();
        }
    }

    event_handling_ = false;

}

hlink::net::Channel::Channel(EventLoop *loop, const int fd) : impl_(std::make_unique<Impl>(loop, fd)) {
}

hlink::net::Channel::~Channel() = default;

void hlink::net::Channel::handle_event(const TimeStamp receive_time) {
    impl_->handle_event(receive_time);
}

void hlink::net::Channel::remove() {
    impl_->remove(this);
}

void hlink::net::Channel::set_read_callback(ReadEventCallback cb) {
    impl_->set_read_callback(std::move(cb));
}

void hlink::net::Channel::set_write_callback(EventCallback cb) {
    impl_->set_write_callback(std::move(cb));
}

void hlink::net::Channel::set_close_callback(EventCallback cb) {
    impl_->set_close_callback(std::move(cb));
}

void hlink::net::Channel::set_error_callback(EventCallback cb) {
    impl_->set_error_callback(std::move(cb));
}

int hlink::net::Channel::fd() const {
    return impl_->fd();
}

int hlink::net::Channel::events() const {
    return impl_->events();
}

void hlink::net::Channel::set_active_events(const int events) {
    impl_->set_active_events(events);
}

bool hlink::net::Channel::is_none_event() const {
    return impl_->is_none_event();
}

void hlink::net::Channel::enable_reading() {
    impl_->enable_reading(this);
}

void hlink::net::Channel::disable_reading() {
    impl_->disable_reading(this);
}

void hlink::net::Channel::enable_writing() {
    impl_->enable_writing(this);
}

void hlink::net::Channel::disable_writing() {
    impl_->disable_writing(this);
}

void hlink::net::Channel::disable_all() {
    impl_->disable_all(this);
}

bool hlink::net::Channel::is_writing() const {
    return impl_->is_writing();
}

bool hlink::net::Channel::is_reading() const {
    return impl_->is_reading();
}

int hlink::net::Channel::flag() const {
    return impl_->flag();
}

void hlink::net::Channel::set_flag(const int flag) {
    impl_->set_flag(flag);
}

hlink::net::EventLoop *hlink::net::Channel::owner_loop() const {
    return impl_->owner_loop();
}

std::string hlink::net::Channel::events_to_string() const {
    return impl_->events_to_string();
}

std::string hlink::net::Channel::active_events_to_string() const {
    return impl_->active_events_to_string();
}

void hlink::net::Channel::tie(const std::shared_ptr<void> &obj) {
    impl_->tie(obj);
}
