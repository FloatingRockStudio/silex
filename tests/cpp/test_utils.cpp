// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include "util/Utils.h"

using namespace silex::core;

TEST(UtilsTest, SplitString) {
    auto parts = splitString("a/b/c", '/');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
}

TEST(UtilsTest, SplitStringEmpty) {
    auto parts = splitString("", '/');
    EXPECT_TRUE(parts.empty());
}

TEST(UtilsTest, JoinStrings) {
    std::vector<std::string> parts = {"a", "b", "c"};
    EXPECT_EQ(joinStrings(parts, "/"), "a/b/c");
}

TEST(UtilsTest, ToLower) {
    EXPECT_EQ(toLower("Hello World"), "hello world");
}

TEST(UtilsTest, ToUpper) {
    EXPECT_EQ(toUpper("Hello World"), "HELLO WORLD");
}

TEST(UtilsTest, ToTitle) {
    EXPECT_EQ(toTitle("hello world"), "Hello World");
}

TEST(UtilsTest, GlobMatchStar) {
    EXPECT_TRUE(globMatch("*.txt", "file.txt"));
    EXPECT_FALSE(globMatch("*.txt", "file.py"));
}

TEST(UtilsTest, GlobMatchQuestion) {
    EXPECT_TRUE(globMatch("file?.txt", "file1.txt"));
    EXPECT_FALSE(globMatch("file?.txt", "file12.txt"));
}

TEST(UtilsTest, NestedValue) {
    std::map<std::string, std::any> map;
    setNestedValue(map, "a.b.c", std::any(std::string("value")));
    auto result = getNestedValue(map, "a.b.c");
    EXPECT_EQ(std::any_cast<std::string>(result), "value");
}
