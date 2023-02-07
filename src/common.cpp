#include "common.hpp"

#include <fmt/core.h>
#include <chipmunk/chipmunk.h>

void
log_init(void)
{
    spdlog::set_pattern("%Y-%m-%dT%H:%M:%S.%e %^%5l%$ %s:%# %!(): %v");
    spdlog::set_level(spdlog::level::trace);
}

