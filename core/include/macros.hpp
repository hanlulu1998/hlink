//
// Created by KINGE on 2025/12/15.
//
#pragma once
namespace hlink {
    // 唯一标识符
#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y) CONCAT_IMPL(x, y)
#define UNIQUE_NAME(base) CONCAT(base, __COUNTER__)
#define MEM_ZERO_SET(ptr,size) memset(ptr,0,size)
#define PTR_ADDRESS(ptr) reinterpret_cast<uintptr_t>(ptr)


}
