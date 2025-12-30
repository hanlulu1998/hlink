//
// Created by KINGE on 2025/12/15.
//
#pragma once
#include <mutex>
#include <deque> //deque性能更好
#include <condition_variable>
#include "noncopyable.hpp"

namespace hlink {
    template<typename T>
    class BlockingQueue : public NonCopyable {
    public:
        using queue_type = std::deque<T>;

        template<typename U>
        void push(U &&t) { {
                std::lock_guard lock{mtx_};
                queue_.emplace_back(std::forward<U>(t));
            }
            cond_var_.notify_one();
        }

        [[nodiscard]] T pop() {
            std::unique_lock lock{mtx_};
            cond_var_.wait(lock, [&] { return !queue_.empty(); });
            T value = std::move(queue_.front());
            queue_.pop_front();
            return value;
        }

        [[nodiscard]] queue_type drain() {
            queue_type drained; {
                std::lock_guard lock{mtx_};
                drained = std::move(queue_);
                assert(queue_.empty());
            }
            return drained;
        }

        [[nodiscard]] size_t size() {
            std::lock_guard lock{mtx_};
            return queue_.size();
        }

    private:
        std::mutex mtx_{};
        std::condition_variable cond_var_{};
        queue_type queue_{};
    };
}
