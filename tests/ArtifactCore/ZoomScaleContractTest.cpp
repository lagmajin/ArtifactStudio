#include <gtest/gtest.h>

import Core.Scale.Zoom;

using ArtifactCore::ZoomScale2D;

TEST(ZoomScaleContractTest, ConstructionAndSetScaleClampToDefaultBounds)
{
    ZoomScale2D low(-5.0f);
    ZoomScale2D high(15.0f);
    EXPECT_FLOAT_EQ(low.scale(), 0.1f);
    EXPECT_FLOAT_EQ(high.scale(), 10.0f);

    low.setScale(4.0f);
    EXPECT_FLOAT_EQ(low.scale(), 4.0f);
    low.setScale(0.0f);
    EXPECT_FLOAT_EQ(low.scale(), 0.1f);
}

TEST(ZoomScaleContractTest, IncrementalAndMultiplicativeOperationsRespectConfiguredBounds)
{
    ZoomScale2D scale(1.0f);
    scale.setMinScale(0.5f);
    scale.setMaxScale(2.0f);

    scale.ZoomIn(2.0f);
    EXPECT_FLOAT_EQ(scale.scale(), 2.0f);
    scale.ZoomIn(10.0f);
    EXPECT_FLOAT_EQ(scale.scale(), 2.0f);
    scale.ZoomOut(4.0f);
    EXPECT_FLOAT_EQ(scale.scale(), 0.5f);
    scale.ZoomOut(10.0f);
    EXPECT_FLOAT_EQ(scale.scale(), 0.5f);

    scale += 0.75f;
    EXPECT_FLOAT_EQ(scale.scale(), 1.25f);
    scale *= 2.0f;
    EXPECT_FLOAT_EQ(scale.scale(), 2.0f);
    scale -= 5.0f;
    EXPECT_FLOAT_EQ(scale.scale(), 0.5f);
}

TEST(ZoomScaleContractTest, ValueArithmeticCopiesLimitsAndDoesNotMutateSource)
{
    ZoomScale2D source(1.5f);
    source.setMinScale(0.75f);
    source.setMaxScale(2.0f);

    const ZoomScale2D multiplied = source * 2.0f;
    const ZoomScale2D divided = source / 2.0f;
    EXPECT_FLOAT_EQ(multiplied.scale(), 2.0f);
    EXPECT_FLOAT_EQ(divided.scale(), 0.75f);
    EXPECT_FLOAT_EQ(source.scale(), 1.5f);
    EXPECT_FLOAT_EQ(multiplied.minScale(), 0.75f);
    EXPECT_FLOAT_EQ(multiplied.maxScale(), 2.0f);

    ZoomScale2D copied(source);
    copied.setScale(1.0f);
    EXPECT_FLOAT_EQ(source.scale(), 1.5f);
    EXPECT_NE(copied, source);

    ZoomScale2D assigned;
    assigned = source;
    EXPECT_EQ(assigned, source);
    assigned.setScale(1.0f);
    EXPECT_FLOAT_EQ(source.scale(), 1.5f);
}

TEST(ZoomScaleContractTest, ResetRestoresDefaultScaleAndEqualityTracksScale)
{
    ZoomScale2D scale(3.0f);
    EXPECT_FALSE(scale.isDefault());
    scale.reset();
    EXPECT_TRUE(scale.isDefault());
    EXPECT_EQ(scale, ZoomScale2D{});
}
