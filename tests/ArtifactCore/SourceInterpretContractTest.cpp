#include <gtest/gtest.h>

#include <QString>

import Media.SourceInterpret;

using namespace ArtifactCore;

TEST(SourceInterpretContractTest, EmptyReportHasNoImpact)
{
    const InterpretImpactReport report;
    EXPECT_FALSE(report.hasAnyImpact());
}

TEST(SourceInterpretContractTest, EachReportedImpactMakesReportNonempty)
{
    InterpretImpactReport report;
    report.affectedLayerCount = 1;
    EXPECT_TRUE(report.hasAnyImpact());

    report = {};
    report.affectedKeyframeCount = 1;
    EXPECT_TRUE(report.hasAnyImpact());

    report = {};
    report.hasTimeRemap = true;
    EXPECT_TRUE(report.hasAnyImpact());

    report = {};
    report.hasFrameBlend = true;
    EXPECT_TRUE(report.hasAnyImpact());

    report = {};
    report.durationWillChange = true;
    EXPECT_TRUE(report.hasAnyImpact());
}

TEST(SourceInterpretContractTest, DescriptiveDetailsAloneDoNotImplyImpact)
{
    InterpretImpactReport report;
    report.affectedKeyframeCount = 0;
    report.oldDurationSeconds = 10.0;
    report.newDurationSeconds = 20.0;
    report.affectedLayerNames = {QStringLiteral("Footage layer")};
    report.warnings = {QStringLiteral("Review interpretation")};

    EXPECT_FALSE(report.hasAnyImpact());
}

TEST(SourceInterpretContractTest, ColorOverrideIsPresentWhenEitherColorFieldIsSet)
{
    SourceInterpretOverride override;
    EXPECT_FALSE(override.hasColorOverride());

    override.inputColorSpace = QStringLiteral("ACEScg");
    EXPECT_TRUE(override.hasColorOverride());
    override.inputColorSpace.clear();
    override.inputTransferFunction = QStringLiteral("PQ");
    EXPECT_TRUE(override.hasColorOverride());
}

TEST(SourceInterpretContractTest, FrameRateOverrideZeroMeansUseSourceDefault)
{
    SourceInterpretOverride override;
    EXPECT_DOUBLE_EQ(override.frameRate, 0.0);
    EXPECT_FALSE(override.isActive);

    override.frameRate = 23.976;
    override.isActive = true;
    EXPECT_DOUBLE_EQ(override.frameRate, 23.976);
    EXPECT_TRUE(override.isActive);
}
