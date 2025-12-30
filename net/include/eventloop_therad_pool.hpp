//
// Created by KINGE on 2025/12/26.
//
#pragma once
#include <functional>
#include <memory>
#include <vector>
namespace hlink::net {
    class EventLoop;

    class EventLoopThreadPool {
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;

        explicit EventLoopThreadPool(EventLoop *loop);

        ~EventLoopThreadPool();

        void set_thread_num(size_t num_threads);

        void start(ThreadInitCallback init_callback);

        EventLoop *get_next_loop();

        EventLoop *get_loop_for_hash(size_t hash_code) const;

        std::vector<EventLoop *> get_all_loops();

        [[nodiscard]] bool is_started() const;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
