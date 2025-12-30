//
// Created by KINGE on 2025/12/26.
//

#include "eventloop_thread.hpp"
#include <thread>
#include <condition_variable>
#include "eventloop.hpp"

class hlink::net::EventLoopThread::Impl {
public:
    explicit Impl(ThreadInitCallback init_callback);

    ~Impl();

    EventLoop *start_loop();

private:
    void thread_func();

    EventLoop *loop_{nullptr};
    bool exiting_{false};
    std::optional<std::jthread> thread_;

    std::mutex mutex_;
    std::condition_variable cond_var_;
    ThreadInitCallback init_callback_;
};


hlink::net::EventLoopThread::Impl::Impl(ThreadInitCallback init_callback) : thread_(std::nullopt),
                                                                            init_callback_(std::move(init_callback)) {
}

hlink::net::EventLoopThread::Impl::~Impl() {
    exiting_ = true;
    if (loop_ != nullptr) {
        loop_->quit();
        thread_->join();
    }
}

hlink::net::EventLoop *hlink::net::EventLoopThread::Impl::start_loop() {
    // 启动线程
    if (!thread_) {
        thread_.emplace([this] {
            this->thread_func();
        });
    }

    EventLoop *loop{nullptr}; {
        std::unique_lock lock(mutex_);
        cond_var_.wait(lock, [this] {
            return loop_ != nullptr;
        });
        loop = loop_;
    }
    return loop;
}

void hlink::net::EventLoopThread::Impl::thread_func() {
    EventLoop loop;
    if (init_callback_) {
        init_callback_(&loop);
    } {
        std::lock_guard lg(mutex_);
        loop_ = &loop;
    }
    cond_var_.notify_all();
    loop.loop(); {
        std::lock_guard lg(mutex_);
        loop_ = nullptr;
    }
}

hlink::net::EventLoopThread::EventLoopThread(ThreadInitCallback init_callback) : impl_(
    std::make_unique<Impl>(std::move(init_callback))) {
}

hlink::net::EventLoopThread::~EventLoopThread() = default;

hlink::net::EventLoop *hlink::net::EventLoopThread::start_loop() {
    return impl_->start_loop();
}
