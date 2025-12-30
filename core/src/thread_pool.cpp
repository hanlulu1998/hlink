//
// Created by KINGE on 2025/12/15.
//

#include "thread_pool.hpp"

#include <cassert>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <deque>
#include <vector>
#include <string>
#include <format>


class hlink::ThreadPool::Impl {
public:
    explicit Impl(const std::string_view name) : name_(name) {
    }

    ~Impl() {
        if (run_flag_) {
            stop();
        }
    }

    void set_max_queue_size(size_t max_queue_size);

    std::string_view get_name() const;

    void set_name(std::string_view name);

    void stop();

    size_t get_queue_size() const;

    bool is_full() const;

    void set_thread_init_task(Task init_task);

    void start(int num_threads);

    void run(Task task);

private:
    bool is_full_() const;

    Task take();

    void run_in_thread();

    mutable std::mutex mtx_{};
    std::condition_variable not_empty_{};
    std::condition_variable not_full_{};
    std::string name_{"ThreadPool"};
    Task init_task_{};
    std::vector<std::unique_ptr<std::jthread> > threads_{};
    std::deque<Task> queue_{};
    size_t max_queue_size_{0};
    bool run_flag_{false};
};

void hlink::ThreadPool::Impl::set_max_queue_size(const size_t max_queue_size) {
    max_queue_size_ = max_queue_size;
}

std::string_view hlink::ThreadPool::Impl::get_name() const {
    return name_;
}

void hlink::ThreadPool::Impl::set_name(const std::string_view name) {
    name_ = name;
}

void hlink::ThreadPool::Impl::stop() { {
        std::lock_guard lock{mtx_};
        run_flag_ = false;
    }
    not_empty_.notify_all();
    not_full_.notify_all();

    for (const auto &thread: threads_) {
        thread->join();
    }
}

size_t hlink::ThreadPool::Impl::get_queue_size() const {
    std::lock_guard lock(mtx_);
    return queue_.size();
}

bool hlink::ThreadPool::Impl::is_full() const {
    std::lock_guard lock(mtx_);
    return is_full_();
}

void hlink::ThreadPool::Impl::set_thread_init_task(Task init_task) {
    init_task_ = std::move(init_task);
}

void hlink::ThreadPool::Impl::start(const int num_threads) {
    assert(threads_.empty());
    run_flag_ = true;
    threads_.reserve(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads_.emplace_back(std::make_unique<std::jthread>(
                [this] { run_in_thread(); }
            )
        );
    }
}

void hlink::ThreadPool::Impl::run(Task task) {
    // 当前没有可用线程，直接运行当前的任务
    if (threads_.empty()) {
        task();
    } else {
        {
            // 当前有可用线程，在队列中添加任务
            std::unique_lock lock{mtx_};
            // 如果队列满了，就等待
            not_full_.wait(lock, [&] {
                return !(is_full_() && run_flag_);
            });

            // 线程池没有启动时，直接返回
            if (!run_flag_) {
                return;
            }
            assert(!is_full_());
            queue_.emplace_back(std::move(task));
        }
        not_empty_.notify_one();
    }
}

bool hlink::ThreadPool::Impl::is_full_() const {
    return max_queue_size_ > 0 && queue_.size() >= max_queue_size_;
}

hlink::ThreadPool::Task hlink::ThreadPool::Impl::take() {
    std::unique_lock lock{mtx_};
    not_empty_.wait(lock, [&] {
        return !(queue_.empty() && run_flag_);
    });

    Task task;
    if (!queue_.empty()) {
        task = std::move(queue_.front());
        queue_.pop_front();
        if (max_queue_size_ > 0) {
            not_full_.notify_one();
        }
    }
    return task;
}

void hlink::ThreadPool::Impl::run_in_thread() {
    try {
        if (init_task_) {
            init_task_();
        }
        while (run_flag_) {
            if (Task task = take()) {
                task();
            }
        }
    } catch (std::exception &e) {
        fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
        fprintf(stderr, "reason: %s\n", e.what());
        abort();
    } catch (...) {
        fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
        throw; // rethrow
    }
}

hlink::ThreadPool::ThreadPool(const std::string_view name) : impl_(std::make_unique<Impl>(name)) {
}

hlink::ThreadPool::~ThreadPool() = default;

void hlink::ThreadPool::set_max_queue_size(const size_t max_queue_size) {
    impl_->set_max_queue_size(max_queue_size);
}

std::string_view hlink::ThreadPool::get_name() const {
    return impl_->get_name();
}

void hlink::ThreadPool::set_name(const std::string_view name) {
    impl_->set_name(name);
}

void hlink::ThreadPool::stop() {
    impl_->stop();
}

size_t hlink::ThreadPool::get_queue_size() const {
    return impl_->get_queue_size();
}

bool hlink::ThreadPool::is_full() const {
    return impl_->is_full();
}

void hlink::ThreadPool::set_thread_init_task(Task init_task) {
    impl_->set_thread_init_task(std::move(init_task));
}

void hlink::ThreadPool::start(const int num_threads) {
    impl_->start(num_threads);
}

void hlink::ThreadPool::run(Task task) {
    impl_->run(std::move(task));
}
