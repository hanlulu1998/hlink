//
// Created by KINGE on 2025/12/26.
//

#include "eventloop_therad_pool.hpp"
#include <cassert>
#include "eventloop.hpp"
#include "eventloop_thread.hpp"


class hlink::net::EventLoopThreadPool::Impl {
public:
    explicit Impl(EventLoop *loop);

    void start(ThreadInitCallback init_callback);

    void set_thread_num(size_t num_threads);

    EventLoop *get_next_loop();

    [[nodiscard]] EventLoop *get_loop_for_hash(size_t hash_code) const;

    std::vector<EventLoop *> get_all_loops();

    [[nodiscard]] bool is_started() const;

private:
    EventLoop *base_loop_{nullptr};
    bool started_{false};
    size_t num_threads_{0};
    size_t next_{0};
    std::vector<std::unique_ptr<EventLoopThread> > threads_;
    std::vector<EventLoop *> loops_;
};

hlink::net::EventLoopThreadPool::Impl::Impl(EventLoop *loop) : base_loop_(loop) {
}

void hlink::net::EventLoopThreadPool::Impl::start(ThreadInitCallback init_callback) {
    assert(!started_);
    base_loop_->assert_in_loop_thread();
    started_ = true;

    for (int i = 0; i < num_threads_; ++i) {
        auto *thread = new EventLoopThread(std::move(init_callback));
        threads_.emplace_back(std::unique_ptr<EventLoopThread>(thread));
        loops_.push_back(thread->start_loop());
    }

    if (num_threads_ == 0 && init_callback) {
        init_callback(base_loop_);
    }
}

void hlink::net::EventLoopThreadPool::Impl::set_thread_num(const size_t num_threads) {
    num_threads_ = num_threads;
}

hlink::net::EventLoop *hlink::net::EventLoopThreadPool::Impl::get_next_loop() {
    base_loop_->assert_in_loop_thread();
    assert(started_);
    EventLoop *loop = base_loop_;
    if (!loops_.empty()) {
        loop = loops_[next_++];
        if (next_ >= loops_.size()) {
            next_ = 0;
        }
    }
    return loop;
}

hlink::net::EventLoop *hlink::net::EventLoopThreadPool::Impl::get_loop_for_hash(const size_t hash_code) const {
    base_loop_->assert_in_loop_thread();
    EventLoop *loop = base_loop_;
    if (!loops_.empty()) {
        loop = loops_[hash_code % loops_.size()];
    }
    return loop;
}

std::vector<hlink::net::EventLoop *> hlink::net::EventLoopThreadPool::Impl::get_all_loops() {
    base_loop_->assert_in_loop_thread();
    assert(started_);
    if (loops_.empty()) {
        return std::vector(1, base_loop_);
    }
    return loops_;
}

bool hlink::net::EventLoopThreadPool::Impl::is_started() const {
    return started_;
}

hlink::net::EventLoopThreadPool::EventLoopThreadPool(EventLoop *loop) : impl_(std::make_unique<Impl>(loop)) {
}

hlink::net::EventLoopThreadPool::~EventLoopThreadPool() = default;

void hlink::net::EventLoopThreadPool::set_thread_num(const size_t num_threads) {
    impl_->set_thread_num(num_threads);
}

void hlink::net::EventLoopThreadPool::start(ThreadInitCallback init_callback) {
    impl_->start(std::move(init_callback));
}

hlink::net::EventLoop *hlink::net::EventLoopThreadPool::get_next_loop() {
    return impl_->get_next_loop();
}

hlink::net::EventLoop *hlink::net::EventLoopThreadPool::get_loop_for_hash(const size_t hash_code) const {
    return impl_->get_loop_for_hash(hash_code);
}

std::vector<hlink::net::EventLoop *> hlink::net::EventLoopThreadPool::get_all_loops() {
    return impl_->get_all_loops();
}

bool hlink::net::EventLoopThreadPool::is_started() const {
    return impl_->is_started();
}
