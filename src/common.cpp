#include "common.hpp"

#include <fmt/core.h>
#include <chipmunk/chipmunk.h>

void
log_init(void)
{
    spdlog::set_pattern("%Y-%m-%dT%H:%M:%S.%e %^%5l%$ [%t] %g:%# %!(): %v");
    spdlog::set_level(spdlog::level::trace);
}

// chipmunk2d 
template <>
struct fmt::formatter<cpVect> : fmt::formatter<string_view> {
    template <typename FormatContext>
    auto format(cpVect obj, FormatContext &ctx) const {
        return fmt::format_to(ctx.out(),
                "{{ .x = {}, .y = {} }}",
                obj.x, obj.y);
    }
};

template <>
struct fmt::formatter<cpBody> : fmt::formatter<string_view> {
    template <typename FormatContext>
    auto format(const cpBody& obj, FormatContext &ctx) const {
        return fmt::format_to(ctx.out(), "pos {}, v {}",
                cpBodyGetPosition(&obj), cpBodyGetVelocity(&obj));
    }
};
