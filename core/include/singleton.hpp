//
// Created by KINGE on 2025/12/15.
//

#pragma once
#include "NonCopyable.hpp"

// 单例模式
namespace hlink {
    template<typename T>
    class Singleton : public NonCopyable {
    public:
        Singleton() = delete;

        // c++11之后保证静态变量的安全初始化
        static T *instance() {
            static T *ptr = new T{};
            return ptr;
        }
    };
}
