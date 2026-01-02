#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#define LG_D(...) spdlog::debug(__VA_ARGS__);
#define LG_I(...) spdlog::info(__VA_ARGS__);
#define LG_E(...) spdlog::critical(__VA_ARGS__);
#define LG_W(...) spdlog::warn(__VA_ARGS__);

namespace logging
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
    spdlog::set_level(static_cast<spdlog::level::level_enum>(level));
}

} // namespace logging

#endif