//
// Created by KINGE on 2025/12/15.
//
#pragma once
// 实现不可拷贝
namespace hlink {
    class NonCopyable {
    public:
        NonCopyable() = default;

        ~NonCopyable() = default;

        NonCopyable(const NonCopyable &) = delete;

        NonCopyable &operator=(const NonCopyable &) = delete;
    };
}
