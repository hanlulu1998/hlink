//
// Created by KINGE on 2025/12/15.
//
#include <gtest/gtest.h>
#include "blocking_queue.hpp"
#include <string>

class BlockingQueueTest : public testing::Test {
public:
    hlink::BlockingQueue<std::string> blocking_queue;
};

TEST_F(BlockingQueueTest, TestPushAndPop) {
    for (int i = 0; i < 10; ++i) {
        std::string s = std::to_string(i);
        blocking_queue.push(s);
    }

    ASSERT_EQ(blocking_queue.size(), 10);

    for (int i = 0; i < 10; ++i) {
        std::string s = blocking_queue.pop();
        blocking_queue.push(std::move(s));
    }

    ASSERT_EQ(blocking_queue.size(), 10);
}

TEST(BlockingQueueCase, TestMove) {
    hlink::BlockingQueue<std::unique_ptr<int> > blocking_queue;
    blocking_queue.push(std::make_unique<int>(1));
    auto x = blocking_queue.pop();
    ASSERT_EQ(*x, 1);
    *x = 123;
    blocking_queue.push(std::move(x));
    auto y = blocking_queue.pop();
    ASSERT_EQ(*y, 123);
}
