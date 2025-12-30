//
// Created by KINGE on 2025/12/23.
//
#pragma once
#include <memory>
#include "common_types.hpp"

namespace hlink {
    class Buffer;

    namespace net {
        class Connector;
        class TcpConn;
        using TcpConnPtr = std::shared_ptr<TcpConn>;
        using ConnectionCallback = std::function<void(const TcpConnPtr &)>;
        using WriteCompleteCallback = std::function<void(const TcpConnPtr &)>;
        using CloseCallback = std::function<void(const TcpConnPtr &)>;
        using HighWaterMarkCallback = std::function<void(const TcpConnPtr &, size_t)>;
        using MessageCallback = std::function<void(const TcpConnPtr &, Buffer *, TimeStamp)>;
        using ConnectorPtr = std::shared_ptr<Connector>;
    }
}
