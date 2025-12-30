//
// Created by KINGE on 2025/12/17.
//
#pragma once
#include "noncopyable.hpp"
#include <memory>
#include "common_types.hpp"
struct epoll_event;

namespace hlink::net {
    class Channel;
    class EventLoop;

    class Poller : public NonCopyable {
    public:
        using ChannelList = std::vector<Channel *>;

        explicit Poller(EventLoop *loop);

        ~Poller();

        void update_channel(Channel *channel);

        void remove_channel(Channel *channel);

        TimeStamp poll(int timeout_ms, ChannelList *active_channels);

        bool has_channel(const Channel *channel) const;

        void assert_in_loop_thread() const;

        static Poller *new_poller(EventLoop *loop);

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
