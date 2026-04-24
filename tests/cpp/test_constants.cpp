// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include <silex/constants.h>

using namespace silex;

TEST(ConstantsTest, SegmentFlagsBitwiseOr) {
    auto flags = SegmentFlags::ReadOnly | SegmentFlags::Deprecated;
    EXPECT_TRUE(hasFlag(flags, SegmentFlags::ReadOnly));
    EXPECT_TRUE(hasFlag(flags, SegmentFlags::Deprecated));
    EXPECT_FALSE(hasFlag(flags, SegmentFlags::Omit));
}

TEST(ConstantsTest, ResolverStatusBitwiseOr) {
    auto status = ResolverStatus::Success | ResolverStatus::Ambiguous;
    EXPECT_TRUE(hasFlag(status, ResolverStatus::Success));
    EXPECT_TRUE(hasFlag(status, ResolverStatus::Ambiguous));
    EXPECT_FALSE(hasFlag(status, ResolverStatus::Error));
}

TEST(ConstantsTest, VerbosityOrdering) {
    EXPECT_LT(static_cast<int>(Verbosity::Quiet), static_cast<int>(Verbosity::Info));
    EXPECT_LT(static_cast<int>(Verbosity::Info), static_cast<int>(Verbosity::Flow));
    EXPECT_LT(static_cast<int>(Verbosity::Flow), static_cast<int>(Verbosity::Detail));
}
