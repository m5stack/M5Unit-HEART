/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Native test for PPG types
*/
#include <gtest/gtest.h>
#include <heart/ppg_types.hpp>

using namespace m5::heart;

TEST(PPGTypes, ChannelBit)
{
    EXPECT_EQ(channel_index(Channel::Green), 0U);
    EXPECT_EQ(channel_index(Channel::Red), 1U);
    EXPECT_EQ(channel_index(Channel::IR), 2U);
    EXPECT_EQ(channel_bit(Channel::Green), 0x01U);
    EXPECT_EQ(channel_bit(Channel::Red), 0x02U);
    EXPECT_EQ(channel_bit(Channel::IR), 0x04U);
}

TEST(PPGTypes, Sample)
{
    PPGSample s{};
    EXPECT_EQ(s.valid, 0U);
    EXPECT_FALSE(s.has(Channel::Green));

    s.set(Channel::Red, 12.5f);
    EXPECT_TRUE(s.has(Channel::Red));
    EXPECT_FALSE(s.has(Channel::IR));
    EXPECT_FLOAT_EQ(s.get(Channel::Red), 12.5f);
    EXPECT_EQ(s.valid, channel_bit(Channel::Red));
}

TEST(PPGTypes, Spec)
{
    PPGSpec spec{};
    EXPECT_EQ(spec.channels, 0U);
    EXPECT_FLOAT_EQ(spec.sampling_rate_hz, 0.0f);
    EXPECT_EQ(spec.unit, ValueUnit::Count);
}
