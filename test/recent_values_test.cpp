/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Native test for RecentValues
*/
#include <gtest/gtest.h>
#include <cmath>
#include <heart/recent_values.hpp>

using namespace m5::heart;

TEST(RecentValues, Empty)
{
    RecentValues<5> rv{};
    EXPECT_EQ(rv.size(), 0U);
    EXPECT_TRUE(std::isnan(rv.median()));
    EXPECT_TRUE(std::isnan(rv.spread()));
}

TEST(RecentValues, MedianAndSpread)
{
    RecentValues<5> rv{};
    rv.push(3.0f);
    EXPECT_FLOAT_EQ(rv.median(), 3.0f);
    rv.push(1.0f);
    EXPECT_FLOAT_EQ(rv.median(), 2.0f);  // Even count: mean of the middle two
    rv.push(10.0f);
    EXPECT_FLOAT_EQ(rv.median(), 3.0f);
    EXPECT_FLOAT_EQ(rv.spread(), 9.0f);
    EXPECT_EQ(rv.size(), 3U);
}

TEST(RecentValues, KeepsLatest)
{
    RecentValues<3> rv{};
    for (int i = 1; i <= 5; ++i) {
        rv.push(static_cast<float>(i));  // 3, 4, 5 remain
    }
    EXPECT_EQ(rv.size(), 3U);
    EXPECT_FLOAT_EQ(rv.median(), 4.0f);
    EXPECT_FLOAT_EQ(rv.spread(), 2.0f);

    rv.clear();
    EXPECT_EQ(rv.size(), 0U);
    rv.push(7.0f);
    EXPECT_FLOAT_EQ(rv.median(), 7.0f);
}
