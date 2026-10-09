#include <gtest/gtest.h>

#include <string_view>

import Utils.Text.Path;

using namespace ArtifactCore;

TEST(PathContractTest, NormalizesRepeatedSeparatorsAndPreservesUncPrefix)
{
    const auto windows = normalizePathSeparators("C:\\\\work\\\\asset.mov");
    const auto unc = normalizePathSeparators("\\\\server\\\\share\\\\asset.mov");

    ASSERT_TRUE(windows);
    EXPECT_EQ(windows.value(), "C:/work/asset.mov");
    ASSERT_TRUE(unc);
    EXPECT_EQ(unc.value(), "//server/share/asset.mov");
}

TEST(PathContractTest, SeparatorNormalizationIsIdempotent)
{
    constexpr std::string_view paths[] = {
        "C:\\work\\asset.mov",
        "//server/share/folder/asset.mov",
        "relative\\folder/asset.mov",
    };

    for (const std::string_view path : paths) {
        const auto first = normalizePathSeparators(path);
        ASSERT_TRUE(first);
        const auto second = normalizePathSeparators(first.value());
        ASSERT_TRUE(second);
        EXPECT_EQ(second.value(), first.value()) << path;
    }
}

TEST(PathContractTest, RejectsEmbeddedNulAndKeepsInputContext)
{
    constexpr std::string_view path("bad\0path", 8);
    const auto normalized = normalizePathSeparators(path, "asset.path");

    ASSERT_FALSE(normalized);
    EXPECT_EQ(normalized.errorContext().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(normalized.errorContext().operation, "path.normalizeSeparators");
    EXPECT_EQ(normalized.errorContext().objectId, "asset.path");
}

TEST(PathContractTest, RecognizesDriveAndRootedPathsAsAbsolute)
{
    EXPECT_TRUE(isAbsolutePath("C:/work/asset.mov"));
    EXPECT_TRUE(isAbsolutePath("C:\\work\\asset.mov"));
    EXPECT_TRUE(isAbsolutePath("/work/asset.mov"));
    EXPECT_TRUE(isAbsolutePath("\\\\server\\share\\asset.mov"));
    EXPECT_FALSE(isAbsolutePath("work/asset.mov"));
    EXPECT_FALSE(isAbsolutePath("C:asset.mov"));
    EXPECT_FALSE(isAbsolutePath(""));
}

TEST(PathContractTest, DetectsTraversalByWholePathSegment)
{
    EXPECT_TRUE(hasParentTraversal("cache/../asset.mov"));
    EXPECT_TRUE(hasParentTraversal("cache\\..\\asset.mov"));
    EXPECT_FALSE(hasParentTraversal("cache/..cache/asset.mov"));
    EXPECT_FALSE(hasParentTraversal("cache/asset.mov"));

    EXPECT_TRUE(isSafeRelativePath("cache/asset.mov"));
    EXPECT_FALSE(isSafeRelativePath("/cache/asset.mov"));
    EXPECT_FALSE(isSafeRelativePath("cache/../asset.mov"));
    EXPECT_FALSE(isSafeRelativePath("cache\\..\\asset.mov"));
}

TEST(PathContractTest, DetectsLeadingTrailingAndMixedSeparatorTraversal)
{
    EXPECT_TRUE(hasParentTraversal("../asset.mov"));
    EXPECT_TRUE(hasParentTraversal("cache/.."));
    EXPECT_FALSE(hasParentTraversal("cache\\..../asset.mov"));
    EXPECT_TRUE(hasParentTraversal("cache\\..\\asset.mov"));
    EXPECT_TRUE(hasParentTraversal("cache/..\\asset.mov"));
    EXPECT_FALSE(hasParentTraversal("cache/.../asset.mov"));

    EXPECT_FALSE(isSafeRelativePath("../asset.mov"));
    EXPECT_FALSE(isSafeRelativePath("cache/.."));
    EXPECT_FALSE(isSafeRelativePath("cache/..\\asset.mov"));
}
