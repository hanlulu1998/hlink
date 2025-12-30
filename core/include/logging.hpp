//
// Created by KINGE on 2025/12/16.
//
#pragma once
#include "noncopyable.hpp"
#include <spdlog/spdlog.h>

namespace hlink {
    class Logger : public NonCopyable {
    public:
        static Logger &instance();

        template<typename... Args>
        void log(const spdlog::source_loc source, spdlog::level::level_enum level, fmt::format_string<Args...> fmt,
                 Args &&... args) {
            logger_->log(std::move(source), level, fmt, std::forward<Args>(args)...);
        }

        ~Logger();

    private:
        std::shared_ptr<spdlog::logger> logger_;

        Logger();
    };

#define LOG_TRACE(...) \
hlink::Logger::instance().log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},spdlog::level::trace,__VA_ARGS__)

#define LOG_DEBUG(...) \
hlink::Logger::instance().log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},spdlog::level::debug,__VA_ARGS__)

#define LOG_INFO(...) \
hlink::Logger::instance().log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},spdlog::level::info,__VA_ARGS__)

#define LOG_WARN(...) \
hlink::Logger::instance().log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},spdlog::level::warn,__VA_ARGS__)

#define LOG_ERROR(...) \
hlink::Logger::instance().log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},spdlog::level::err,__VA_ARGS__)

#define LOG_CRITICAL(...) \
{hlink::Logger::instance().log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},spdlog::level::critical,__VA_ARGS__);assert(false);}
} // namespace hlink
