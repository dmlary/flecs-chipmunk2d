include(GoogleTest)
enable_testing()

SET(IMPL_BINARIES "")
function(add_impl name)
    add_executable(${name}_impl
        main.cpp
        common.cpp
        ${name}_impl.cpp)
    target_compile_options(${name}_impl PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(${name}_impl PRIVATE
        spdlog
        gtest
        flecs
        chipmunk)
    gtest_discover_tests(${name}_impl)
    set(IMPL_BINARIES "${IMPL_BINARIES}${name}_impl;" PARENT_SCOPE)
endfunction()

add_impl(simple_struct)
add_impl(unique_ptr)

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND}
    DEPENDS ${IMPL_BINARIES})
