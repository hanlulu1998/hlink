//
// Created by KINGE on 2025/12/15.
//
#include <gtest/gtest.h>
#include "thread_pool.hpp"
#include <chrono>
#include <thread>
#include <latch>
using namespace std::chrono_literals;

void print() {
    std::cout << std::this_thread::get_id() << std::endl;
}


void print_str(const std::string_view str) {
    std::cout << str << std::endl;
    std::this_thread::sleep_for(100ms);
}

void test(const int max_size) {
    hlink::ThreadPool pool("MainThreadPool");

    pool.set_max_queue_size(max_size);
    pool.start(5);

    pool.run(print);
    pool.run(print);

    for (int i = 0; i < 10; ++i) {
        pool.run([i] { print_str(std::format("task: {}", i)); });
    }

    std::latch latch(1);
    pool.run([&] {
        latch.count_down();
    });
    latch.wait();
    pool.stop();
}

TEST(ThreadPoolCase, TestThreadPool) {
    test(0);
    test(1);
    test(5);
    test(10);
    test(50);
}

void long_task(const int i) {
    printf("long task: %d\n", i);
    std::this_thread::sleep_for(3s);
}

TEST(ThreadPoolCase, TestEarlyStop) {
    hlink::ThreadPool pool("ThreadPool");
    pool.set_max_queue_size(5);
    pool.start(3);

    std::thread t1([&] {
        for (int i = 0; i < 10; ++i) {
            pool.run([i] { long_task(i); });
        }
    });

    std::this_thread::sleep_for(5s);

    pool.stop();

    t1.join();


    pool.run(print);
}
