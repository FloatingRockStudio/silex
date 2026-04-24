// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include <algorithm>
#include <silex/GenericResolver.h>
#include "segmenters/FilesystemSegmenter.h"

using namespace silex;
using namespace silex::resolvers;
using namespace silex::segmenters;

TEST(FilesystemSegmenterTest, SplitPath) {
    FilesystemSegmenter seg;
    auto parts = seg.splitPath("/projects/show", "/projects/show/assets/chr/model");
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "assets");
    EXPECT_EQ(parts[1], "chr");
    EXPECT_EQ(parts[2], "model");
}

TEST(FilesystemSegmenterTest, JoinSegments) {
    FilesystemSegmenter seg;
    auto result = seg.joinSegments("/projects/show", {"assets", "chr", "model"});
    std::replace(result.begin(), result.end(), '\\', '/');
    EXPECT_EQ(result, "/projects/show/assets/chr/model");
}

TEST(FilesystemSegmenterTest, MatchesRoot) {
    FilesystemSegmenter seg;
    EXPECT_TRUE(seg.matchesRoot("/projects/show", "/projects/show/assets"));
    EXPECT_FALSE(seg.matchesRoot("/projects/show", "/other/path"));
}

TEST(GenericResolverTest, Construction) {
    SilexParseOptions opts;
    GenericResolver resolver(opts);
    // Should not throw
}

TEST(GenericResolverTest, SchemaFromPathNoSchemas) {
    SilexParseOptions opts;
    GenericResolver resolver(opts);
    auto result = resolver.schemaFromPath("/nonexistent/path");
    EXPECT_FALSE(result.has_value());
}
