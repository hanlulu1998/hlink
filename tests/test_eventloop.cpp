//
// Created by KINGE on 2025/12/18.
//
#include "eventloop.hpp"
#include <gtest/gtest.h>
#include <thread>
#include "utils.hpp"
#include <sys/timerfd.h>
#include "channel.hpp"
#include "timerid.hpp"

hlink::net::EventLoop *g_loop = nullptr;

void timeout(hlink::TimeStamp) {
    GTEST_LOG_(INFO) << "Timeout!\n";
    g_loop->quit();
}


void call_back(hlink::net::EventLoop *loop) {
    GTEST_LOG_(INFO) << "call_back tid = " << hlink::thread::get_thread_id() << std::endl;
    GTEST_LOG_(INFO) << "call_back loop quit" << std::endl;
    loop->quit();
}

void thread_func() {
    GTEST_LOG_(INFO) << "thread_func tid = " << hlink::thread::get_thread_id() << std::endl;
    ASSERT_EQ(hlink::net::EventLoop::get_eventloop_of_current_thread(), nullptr);
    hlink::net::EventLoop loop;
    loop.init();
    ASSERT_EQ(hlink::net::EventLoop::get_eventloop_of_current_thread(), &loop);
    loop.run_after(1000, std::bind(call_back, &loop));
    loop.loop();
}

TEST(TestEventLoopCase, TestTimer) {
    GTEST_LOG_(INFO) << "TestSimple tid = " << hlink::thread::get_thread_id() << "\n";
    ASSERT_EQ(hlink::net::EventLoop::get_eventloop_of_current_thread(), nullptr);
    hlink::net::EventLoop loop;
    loop.init();
    ASSERT_EQ(hlink::net::EventLoop::get_eventloop_of_current_thread(), &loop);
    std::jthread thread1(thread_func);

    loop.run_after(5000, [&loop] {
        GTEST_LOG_(INFO) << "main loop quit" << std::endl;
        loop.quit();
    });
    GTEST_LOG_(INFO) << hlink::time::get_current_time() << "\n";
    loop.loop();
    GTEST_LOG_(INFO) << hlink::time::get_current_time() << "\n";
}


TEST(TestEventLoopCase, TestSimple) {
    GTEST_LOG_(INFO) << "TestSimple tid = " << hlink::thread::get_thread_id();
    ASSERT_EQ(hlink::net::EventLoop::get_eventloop_of_current_thread(), nullptr);
    hlink::net::EventLoop loop;
    loop.init();
    ASSERT_EQ(hlink::net::EventLoop::get_eventloop_of_current_thread(), &loop);

    const int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    hlink::net::Channel channel(&loop, timerfd);
    channel.set_read_callback(timeout);
    channel.enable_reading();

    itimerspec howlong;
    bzero(&howlong, sizeof(howlong));
    howlong.it_value.tv_sec = 5;
    timerfd_settime(timerfd, 0, &howlong, nullptr);

    loop.loop();
    close(timerfd);
}
