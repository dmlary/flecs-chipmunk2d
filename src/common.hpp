#pragma once

#include <fmt/core.h>
#include <chipmunk/chipmunk.h>
#include <chipmunk/chipmunk_structs.h>

/// logging setup
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>
#include <execinfo.h>

#define log_trace SPDLOG_TRACE
#define log_debug SPDLOG_DEBUG
#define log_info SPDLOG_INFO
#define log_warn SPDLOG_WARN
#define log_error SPDLOG_ERROR
#define log_fatal SPDLOG_CRITICAL

#define log_errno(fmt, ...) \
    log_error(fmt "; {} ({})", ## __VA_ARGS__, std::strerror(errno), errno)
#define log_debug_bt(...) \
    log_debug(__VA_ARGS__); \
    log_bt(log_warn)
#define log_warn_bt(...) \
    log_warn(__VA_ARGS__); \
    log_bt(log_warn)
#define log_bt(f) { \
    void *__bt[64]; \
    int __n = backtrace(__bt, 64); \
    char **__sym = backtrace_symbols(__bt, __n); \
    for (int i = 0; i < __n; i++) { \
        f("{}", __sym[i]); \
    } \
    free(__sym); }

/// set up logging for our desired formatting.
void
log_init(void);

/// custom formatters for chipmunk2d structs
template <>
struct fmt::formatter<cpVect> : fmt::formatter<string_view> {
    template <typename FormatContext>
    auto format(cpVect obj, FormatContext &ctx) const {
        return fmt::format_to(ctx.out(),
                "({:.3f}, {:.3f})", obj.x, obj.y);
    }
};

template <>
struct fmt::formatter<cpBody> : fmt::formatter<string_view> {
    template <typename FormatContext>
    auto format(const cpBody& obj, FormatContext &ctx) const {
        return fmt::format_to(ctx.out(), "p {}, v {}",
                cpBodyGetPosition(&obj), cpBodyGetVelocity(&obj));
    }
};
