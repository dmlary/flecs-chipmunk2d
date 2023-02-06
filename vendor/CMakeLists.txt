include(FetchContent)

# fmt to provide custom formatters to spdlog; without this, I'm not sure how to
# use the spdlog bundled header files for fmt
FetchContent_Declare(
    fmt
    OVERRIDE_FIND_PACKAGE
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG 9.1.0)
FetchContent_MakeAvailable(fmt)

# spdlog for debugging output
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG v1.10.0)
FetchContent_MakeAvailable(spdlog)

# googletest to ensure implementations work
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1)
FetchContent_MakeAvailable(googletest)

# flecs ECS library
FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs
    GIT_TAG v3.1.4)
FetchContent_MakeAvailable(flecs)

# chipmunk2d physics library
FetchContent_Declare(
    chipmunk2d
    GIT_REPOSITORY https://github.com/slembcke/Chipmunk2D
    GIT_TAG Chipmunk-7.0.3)
FetchContent_MakeAvailable(chipmunk2d)
target_include_directories(chipmunk PUBLIC ${chipmunk_SOURCE_DIR}/include)
