include(GoogleTest)
enable_testing()

add_executable(tests
    main.cpp
    common.cpp
    example_test.cpp
)
gtest_discover_tests(tests)

target_link_libraries(tests PUBLIC
    spdlog
    gtest
    flecs
    chipmunk)

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND}
    DEPENDS tests)
