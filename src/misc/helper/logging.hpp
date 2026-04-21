#ifndef LOGGING_HPP
#define LOGGING_HPP

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#endif

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include "spdlog/fmt/bundled/format.h"

#define LG_D(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LG_I(...) SPDLOG_INFO(__VA_ARGS__)
#define LG_E(...) SPDLOG_CRITICAL(__VA_ARGS__)
#define LG_W(...) SPDLOG_WARN(__VA_ARGS__)

namespace debug
{

inline void createLogger(std::string logFile, uint8_t level)
{
    spdlog::init_thread_pool(8192, 1);
    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFile, 1024 * 1024 * 10, 3);
    std::vector<spdlog::sink_ptr> sinks{stdout_sink, rotating_sink};
    auto logger = std::make_shared<spdlog::async_logger>(
        "mainlogger", sinks.begin(), sinks.end(), spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    spdlog::set_default_logger(logger);
    spdlog::set_pattern(
        "[%m%d|%H:%M:%S.%e] [%^%-8l%$] [%s:%!] %v");
    spdlog::set_level(static_cast<spdlog::level::level_enum>(level));
}

} // namespace debug


#endif