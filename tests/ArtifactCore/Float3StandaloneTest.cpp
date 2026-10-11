#include <gtest/gtest.h>

#include <sstream>

import Float3;

using ArtifactCore::float3;

TEST(Float3StandaloneTest, DefaultAndExplicitConstructionPreserveComponents)
{
    const float3<float> zero;
    EXPECT_FLOAT_EQ(zero.x, 0.0f);
    EXPECT_FLOAT_EQ(zero.y, 0.0f);
    EXPECT_FLOAT_EQ(zero.z, 0.0f);

    const float3<float> value(1.25f, -2.5f, 4.0f);
    EXPECT_FLOAT_EQ(value.x, 1.25f);
    EXPECT_FLOAT_EQ(value.y, -2.5f);
    EXPECT_FLOAT_EQ(value.z, 4.0f);
}

TEST(Float3StandaloneTest, AdditionCombinesCorrespondingComponents)
{
    const float3<float> value = float3<float>(1.0f, 2.0f, 3.0f) +
                                float3<float>(-4.0f, 5.0f, 0.5f);
    EXPECT_FLOAT_EQ(value.x, -3.0f);
    EXPECT_FLOAT_EQ(value.y, 7.0f);
    EXPECT_FLOAT_EQ(value.z, 3.5f);
}

TEST(Float3StandaloneTest, ScalarMultiplicationScalesAllComponents)
{
    const float3<float> value = float3<float>(1.5f, -2.0f, 4.0f) * 2.0f;
    EXPECT_FLOAT_EQ(value.x, 3.0f);
    EXPECT_FLOAT_EQ(value.y, -4.0f);
    EXPECT_FLOAT_EQ(value.z, 8.0f);
}

TEST(Float3StandaloneTest, DotProductUsesAllThreeComponents)
{
    const float3<float> a(1.0f, 2.0f, 3.0f);
    const float3<float> b(4.0f, -5.0f, 6.0f);
    EXPECT_FLOAT_EQ(a.dot(b), 12.0f);
}

TEST(Float3StandaloneTest, LengthIsTheSquareRootOfSelfDotProduct)
{
    EXPECT_FLOAT_EQ(float3<float>(2.0f, 3.0f, 6.0f).length(), 7.0f);
}

TEST(Float3StandaloneTest, NormalizedVectorHasUnitLength)
{
    const float3<float> value = float3<float>(0.0f, 3.0f, 4.0f).normalized();
    EXPECT_FLOAT_EQ(value.x, 0.0f);
    EXPECT_FLOAT_EQ(value.y, 0.6f);
    EXPECT_FLOAT_EQ(value.z, 0.8f);
    EXPECT_NEAR(value.length(), 1.0f, 1e-6f);
}

TEST(Float3StandaloneTest, ZeroVectorNormalizationReturnsZero)
{
    const float3<float> value = float3<float>{}.normalized();
    EXPECT_FLOAT_EQ(value.x, 0.0f);
    EXPECT_FLOAT_EQ(value.y, 0.0f);
    EXPECT_FLOAT_EQ(value.z, 0.0f);
}

TEST(Float3StandaloneTest, SupportsDoublePrecisionComponents)
{
    const float3<double> value(0.1, 0.2, 0.3);
    const auto doubled = value * 2.0;
    EXPECT_DOUBLE_EQ(doubled.x, 0.2);
    EXPECT_DOUBLE_EQ(doubled.y, 0.4);
    EXPECT_DOUBLE_EQ(doubled.z, 0.6);
    EXPECT_NEAR(value.dot(value), 0.14, 1e-15);
}

TEST(Float3StandaloneTest, StreamOutputUsesTupleFormatting)
{
    std::ostringstream stream;
    stream << float3<float>{1.0f, 2.0f, 3.0f};
    EXPECT_EQ(stream.str(), "(1, 2, 3)");
}
