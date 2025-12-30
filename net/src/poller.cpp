//
// Created by KINGE on 2025/12/17.
//

#include "poller.hpp"
#include <cassert>
#include <cerrno>
#include <sys/epoll.h>
#include <unistd.h>
#include "channel.hpp"
#include "eventloop.hpp"
#include "logging.hpp"
#include "utils.hpp"

class hlink::net::Poller::Impl {
public:
    explicit Impl(EventLoop *loop);

    ~Impl();

    TimeStamp poll(int timeout_ms, ChannelList *active_channels);

    void update_channel(Channel *channel);

    void remove_channel(Channel *channel);

    bool has_channel(const Channel *channel) const;

    void assert_in_loop_thread() const;

    static Poller *new_poller(EventLoop *loop);

private:
    void fill_active_channels(int num_events, ChannelList *active_channels) const;

    static const char *epoll_operation_to_string(int op);

    void update(int op, Channel *channel) const;

    static constexpr int INIT_EVENT_LIST_SIZE = 16;

    EventLoop *loop_{nullptr};

    using EventList = std::vector<epoll_event>;
    using ChannelMap = std::unordered_map<int, Channel *>;
    EventList events_;
    ChannelMap channels_;
    int epoll_fd_{-1};
};

void hlink::net::Poller::Impl::fill_active_channels(const int num_events, ChannelList *active_channels) const {
    assert(num_events< events_.size());
    for (int i = 0; i < num_events; ++i) {
        auto channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_active_events(events_[i].events);
        active_channels->push_back(channel);
    }
}

const char *hlink::net::Poller::Impl::epoll_operation_to_string(const int op) {
    switch (op) {
        case EPOLL_CTL_ADD:
            return "ADD";
        case EPOLL_CTL_DEL:
            return "DEL";
        case EPOLL_CTL_MOD:
            return "MOD";
        default:
            assert(false && "ERROR op");
            return "Unknown Operation";
    }
}

void hlink::net::Poller::Impl::update(const int op, Channel *channel) const {
    epoll_event event{};
    event.events = channel->events();
    event.data.ptr = channel;

    const int fd = channel->fd();
    LOG_TRACE("epoll_ctrl op = {}, fd = {}, event={}", epoll_operation_to_string(op), fd, channel->events_to_string());
    if (epoll_ctl(epoll_fd_, op, fd, &event) < 0) {
        if (op == EPOLL_CTL_DEL) {
            LOG_ERROR("epoll_ctl error: {}, op = {}", strerror(errno), epoll_operation_to_string(op));
        } else {
            LOG_CRITICAL("epoll_ctl error: {}, op = {}", strerror(errno), epoll_operation_to_string(op));
        }
    }
}

hlink::net::Poller::Impl::Impl(EventLoop *loop) : loop_(loop), events_(INIT_EVENT_LIST_SIZE),
                                                  epoll_fd_(epoll_create1(EPOLL_CLOEXEC)) {
    if (epoll_fd_ < 0) {
        LOG_CRITICAL("epoll create failed");
    }
}

hlink::net::Poller::Impl::~Impl() {
    close(epoll_fd_);
}

hlink::TimeStamp hlink::net::Poller::Impl::poll(const int timeout_ms, ChannelList *active_channels) {
    LOG_TRACE("fd total count: {}", channels_.size());
    const int num_events = epoll_wait(epoll_fd_, events_.data(), static_cast<int>(events_.size()), timeout_ms);
    int saved_errno = errno;
    const auto now = time::get_current_time();
    if (num_events > 0) {
        LOG_TRACE("{} events happened", num_events);
        fill_active_channels(num_events, active_channels);
        // 如果事件达到超过了容量，就需要扩容
        if (num_events == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (num_events == 0) {
        LOG_TRACE("nothing happened");
    } else {
        if (saved_errno != EINTR) {
            errno = saved_errno;
            LOG_ERROR("epoll_wait error: {}", saved_errno);
        }
    }
    return now;
}


void hlink::net::Poller::Impl::update_channel(Channel *channel) {
    // FLAG_UNADD刚出创建的
    LOG_TRACE("Poller::update_channel assert in loop thread");
    assert_in_loop_thread();
    const int flag = channel->flag();
    LOG_TRACE("fd = {}, events = {}, index = {}", channel->fd(), channel->events(), flag);

    if (const int fd = channel->fd(); flag == Channel::FLAG_UNADD || flag == Channel::FLAG_DELETED) {
        if (flag == Channel::FLAG_UNADD) {
            assert(!channels_.contains(fd));
            channels_[fd] = channel;
        } else {
            assert(channels_.contains(fd));
            assert(channels_[fd] == channel);
        }

        channel->set_flag(Channel::FLAG_ADDED);
        update(EPOLL_CTL_ADD, channel);
    } else {
        assert(channels_.contains(fd));
        assert(channels_[fd] == channel);
        assert(flag == Channel::FLAG_ADDED);

        if (channel->is_none_event()) {
            update(EPOLL_CTL_DEL, channel);
            channel->set_flag(Channel::FLAG_DELETED);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void hlink::net::Poller::Impl::remove_channel(Channel *channel) {
    LOG_TRACE("Poller::remove_channel in loop thread");
    assert_in_loop_thread();
    const int fd = channel->fd();
    LOG_TRACE("fd = {}", fd);
    assert(channels_.contains(fd));
    assert(channels_[fd] == channel);
    assert(channel->is_none_event());

    const int flag = channel->flag();
    assert(flag == Channel::FLAG_DELETED || flag == Channel::FLAG_ADDED);
    const size_t n = channels_.erase(fd);
    assert(n == 1);

    if (flag == Channel::FLAG_ADDED) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_flag(Channel::FLAG_UNADD);
}

bool hlink::net::Poller::Impl::has_channel(const Channel *channel) const {
    assert_in_loop_thread();
    const int fd = channel->fd();
    return channels_.contains(fd) && channels_.at(fd) == channel;
}

void hlink::net::Poller::Impl::assert_in_loop_thread() const {
    loop_->assert_in_loop_thread();
}

hlink::net::Poller *hlink::net::Poller::Impl::new_poller(EventLoop *loop) {
    return new Poller(loop);
}

hlink::net::Poller::Poller(EventLoop *loop) : impl_(std::make_unique<Impl>(loop)) {
}

hlink::net::Poller::~Poller() = default;

hlink::TimeStamp hlink::net::Poller::poll(const int timeout_ms, ChannelList *active_channels) {
    return impl_->poll(timeout_ms, active_channels);
}

void hlink::net::Poller::update_channel(Channel *channel) {
    impl_->update_channel(channel);
}

void hlink::net::Poller::remove_channel(Channel *channel) {
    impl_->remove_channel(channel);
}

bool hlink::net::Poller::has_channel(const Channel *channel) const {
    return impl_->has_channel(channel);
}

void hlink::net::Poller::assert_in_loop_thread() const {
    impl_->assert_in_loop_thread();
}

hlink::net::Poller *hlink::net::Poller::new_poller(EventLoop *loop) {
    return Impl::new_poller(loop);
}
