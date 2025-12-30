//
// Created by KINGE on 2025/12/16.
//

#include "logging.hpp"
#include <spdlog/async.h>
#include <spdlog/sinks/daily_file_sink.h>
#ifdef DEBUG_MODE
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

hlink::Logger &hlink::Logger::instance() {
    static Logger logger;
    return logger;
}

hlink::Logger::Logger() {
    spdlog::init_thread_pool(8192, 1);
#ifdef DEBUG_MODE
    logger_ = spdlog::stdout_color_mt<spdlog::async_factory>("console");
#else
    logger_ = spdlog::daily_logger_format_mt<spdlog::async_factory>("async_file_logger", "logs/%Y-%m-%d.log", 0, 0,
                                                                    false);
#endif
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%l] [%s:%#] %v");

#ifdef DEBUG_MODE
    logger_->set_level(spdlog::level::debug);
    logger_->flush_on(spdlog::level::debug);
#else
    logger_->set_level(spdlog::level::info);
    logger_->flush_on(spdlog::level::info);
#endif
}

hlink::Logger::~Logger() {
    spdlog::drop_all();
    spdlog::shutdown();
}
