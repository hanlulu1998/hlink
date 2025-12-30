//
// Created by KINGE on 2025/12/23.
//
#pragma once
#include <functional>
#include <memory>

namespace hlink {
    template<typename Class, typename... Args>
    class WeakCallback {
    public:
        WeakCallback(const std::weak_ptr<Class> &obj,
                     const std::function<void(Class *, Args...)> &callback) : obj_(obj), callback_(callback) {
        }

        void operator()(Args &&... args) {
            if (std::shared_ptr<Class> ptr = obj_.lock()) {
                callback_(ptr.get(), std::forward<Args>(args)...);
            }
        }

    private:
        std::weak_ptr<Class> obj_;
        std::function<void(Class *, Args...)> callback_;
    };

    template<typename Class, typename... Args>
    WeakCallback<Class, Args...>
    make_weak_callback(const std::shared_ptr<Class> &obj, void (Class::*callback)(Args...)) {
        return {obj, callback};
    }

    template<typename Class, typename... Args>
    WeakCallback<Class, Args...>
    make_weak_callback(const std::shared_ptr<Class> &obj, void (Class::*callback)(Args...) const) {
        return {obj, callback};
    }
}
