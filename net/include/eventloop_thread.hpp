//
// Created by KINGE on 2025/12/26.
//

#pragma once
#include <functional>
#include <memory>
#include "noncopyable.hpp"

namespace hlink::net {
    class EventLoop;

    class EventLoopThread : public NonCopyable {
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;

        explicit EventLoopThread(ThreadInitCallback init_callback);

        ~EventLoopThread();

        EventLoop *start_loop();

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
