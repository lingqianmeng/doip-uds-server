#include <gtest/gtest.h>

#include "doip_uds/version.hpp"

TEST(Version, IsNotEmpty) {
    EXPECT_FALSE(doip_uds::version().empty());
}

TEST(Version, MatchesProjectVersion) {
    EXPECT_EQ(doip_uds::version(), "0.1.0");
}
