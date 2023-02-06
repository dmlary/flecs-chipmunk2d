#include <cstdio>
#include <gtest/gtest.h>
#include "common.hpp"

GTEST_API_ int main(int argc, char **argv) {
    log_init();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
