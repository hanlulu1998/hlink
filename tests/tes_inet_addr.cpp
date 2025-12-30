//
// Created by KINGE on 2025/12/17.
//
#include "inet_addr.hpp"
#include <gtest/gtest.h>
TEST(InetAddressCase, TestResolve) {
    hlink::net::InetAddr addr{};

    hlink::net::InetAddr::resolve("www.baidu.com", addr);
    GTEST_LOG_(INFO) << addr.to_ip_port() << "\n";

    const hlink::net::InetAddr addr2{"192.168.98.1", 8080};
    GTEST_LOG_(INFO) << addr2.to_ip_port() << "\n";
}
