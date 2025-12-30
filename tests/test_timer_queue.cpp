//
// Created by KINGE on 2025/12/22.
//
#include "timer_queue.hpp"
#include "eventloop.hpp"
#include <unistd.h>
#include <thread>
#include <gtest/gtest.h>
#include "timerid.hpp"
#include "utils.hpp"
int test_timer_queue_cnt = 0;
hlink::net::EventLoop *g_test_timer_queue_loop;

void print_tid() {
    GTEST_LOG_(INFO) << "tid: " << std::this_thread::get_id();
    GTEST_LOG_(INFO) << "now: " << hlink::time::get_current_time();
}

void print(const char *msg) {
    GTEST_LOG_(INFO) << "msg: " << hlink::time::get_current_time() << " " << msg << "\n";
    if (++test_timer_queue_cnt == 20) {
        g_test_timer_queue_loop->quit();
    }
}

void cancel(hlink::net::TimerId timer_id) {
    g_test_timer_queue_loop->cancel(timer_id);
    GTEST_LOG_(INFO) << "cancelled at " << hlink::time::get_current_time() << "\n";
}

TEST(TestTimerQueueCase, TestAll) {
    print_tid();
    sleep(1); {
        hlink::net::EventLoop event_loop;
        event_loop.init();
        g_test_timer_queue_loop = &event_loop;

        GTEST_LOG_(INFO) << "mian";
        event_loop.run_after(1000, [] {
            print("once1");
        });

        event_loop.run_after(1500, [] {
            print("once1.5");
        });

        event_loop.run_after(2500, [] {
            print("once2.5");
        });

        event_loop.run_after(3500, [] {
            print("once3.5");
        });

        auto t45 = event_loop.run_after(4500, [] {
            print("once4.5");
        });

        event_loop.run_after(4200, [t45] {
            cancel(t45);
        });

        event_loop.run_after(4800, [t45] {
            cancel(t45);
        });

        event_loop.run_every(2000, [] {
            print("every2");
        });

        auto t3 = event_loop.run_every(3000, [] {
            print("every3");
        });

        event_loop.run_after(9001, [t3] {
            cancel(t3);
        });

        event_loop.loop();

        GTEST_LOG_(INFO) << "main loop exits";
    }
}
