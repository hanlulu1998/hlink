//
// Created by KINGE on 2025/12/15.
//
#pragma once
#include <functional>
#include "noncopyable.hpp"
#include <memory>
#include <string_view>

namespace hlink {
    class ThreadPool : public NonCopyable {
    public:
        using Task = std::function<void()>;

        explicit ThreadPool(std::string_view name);

        ~ThreadPool();

        void set_max_queue_size(size_t max_queue_size);

        [[nodiscard]] std::string_view get_name() const;

        void set_name(std::string_view name);

        void stop();

        [[nodiscard]] size_t get_queue_size() const;

        [[nodiscard]] bool is_full() const;

        void set_thread_init_task(Task init_task);

        void start(int num_threads);

        void run(Task task);

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
