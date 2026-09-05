#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include <QString>

import Layer.Matte;
import FloatRGBA;
import Utils.Id;

using namespace ArtifactCore;

namespace {

MatteNode makeNode(MatteMode mode) {
    MatteNode node;
    node.setSourceLayerId(Id());
    node.setMode(mode);
    return node;
}

} // namespace

TEST(MatteStackTest, SampleAlphaModes) {
    const FloatRGBA pixel(0.2f, 0.4f, 0.6f, 0.8f);
    EXPECT_FLOAT_EQ(MatteEvaluator::sample(pixel, MatteMode::None), 1.0f);
    EXPECT_FLOAT_EQ(MatteEvaluator::sample(pixel, MatteMode::Alpha), 0.8f);
    EXPECT_FLOAT_EQ(MatteEvaluator::sample(pixel, MatteMode::AlphaInverted), 0.2f);
}

TEST(MatteStackTest, SampleLuminanceStandardsDiffer) {
    // Pure green separates Rec.601 (0.587) from Rec.709 (0.7152).
    const FloatRGBA green(0.0f, 1.0f, 0.0f, 1.0f);
    EXPECT_NEAR(MatteEvaluator::sample(green, MatteMode::Luminance,
                                       LuminanceStandard::Rec601),
                0.587f, 1e-5f);
    EXPECT_NEAR(MatteEvaluator::sample(green, MatteMode::Luminance,
                                       LuminanceStandard::Rec709),
                0.7152f, 1e-5f);
    EXPECT_NEAR(MatteEvaluator::sample(green, MatteMode::LuminanceInverted,
                                       LuminanceStandard::Rec709),
                1.0f - 0.7152f, 1e-5f);
}

TEST(MatteStackTest, CombineClamps) {
    EXPECT_FLOAT_EQ(MatteEvaluator::combine(0.7f, 0.5f, MatteStackMode::Add), 1.0f);
    EXPECT_FLOAT_EQ(MatteEvaluator::combine(0.3f, 0.5f, MatteStackMode::Common), 0.3f);
    EXPECT_FLOAT_EQ(MatteEvaluator::combine(0.2f, 0.5f, MatteStackMode::Subtract), 0.0f);
    EXPECT_FLOAT_EQ(MatteEvaluator::combine(0.2f, 0.5f, MatteStackMode::Difference), 0.3f);
    EXPECT_FLOAT_EQ(MatteEvaluator::combine(0.9f, 0.4f, MatteStackMode::Difference), 0.5f);
}

TEST(MatteStackTest, EvaluateEmptyIsPassthrough) {
    EXPECT_FLOAT_EQ(MatteEvaluator::evaluate({}, MatteStackMode::Add), 1.0f);
    EXPECT_FLOAT_EQ(MatteEvaluator::evaluate({0.4f}, MatteStackMode::Subtract), 0.4f);
    EXPECT_FLOAT_EQ(
        MatteEvaluator::evaluate({0.3f, 0.5f, 0.2f}, MatteStackMode::Common), 0.2f);
}

TEST(MatteStackTest, ApplyScalesAllChannels) {
    const FloatRGBA out = MatteEvaluator::apply(FloatRGBA(0.5f, 0.5f, 0.5f, 0.5f), 0.5f);
    EXPECT_FLOAT_EQ(out.r(), 0.25f);
    EXPECT_FLOAT_EQ(out.g(), 0.25f);
    EXPECT_FLOAT_EQ(out.b(), 0.25f);
    EXPECT_FLOAT_EQ(out.a(), 0.25f);
}

TEST(MatteStackTest, StackOrderAdd) {
    MatteStack stack;
    stack.setStackMode(MatteStackMode::Add);
    stack.addNode(makeNode(MatteMode::Alpha));
    stack.addNode(makeNode(MatteMode::Alpha));
    const std::vector<std::vector<float>> sources = {{0.3f}, {0.5f}};
    const auto result = evaluateMatteStack(sources, stack, 1, 1);
    ASSERT_TRUE(result.isValid());
    EXPECT_FLOAT_EQ(result.alphaMask[0], 0.8f);
}

TEST(MatteStackTest, StackSubtractAndDifference) {
    MatteStack sub;
    sub.setStackMode(MatteStackMode::Subtract);
    sub.addNode(makeNode(MatteMode::Alpha));
    sub.addNode(makeNode(MatteMode::Alpha));
    const std::vector<std::vector<float>> sources = {{0.8f}, {0.3f}};
    EXPECT_FLOAT_EQ(evaluateMatteStack(sources, sub, 1, 1).alphaMask[0], 0.5f);

    MatteStack diff;
    diff.setStackMode(MatteStackMode::Difference);
    diff.addNode(makeNode(MatteMode::Alpha));
    diff.addNode(makeNode(MatteMode::Alpha));
    EXPECT_FLOAT_EQ(evaluateMatteStack(sources, diff, 1, 1).alphaMask[0], 0.5f);
}

TEST(MatteStackTest, DisabledAndNilNodesSkipped) {
    MatteStack stack;
    stack.setStackMode(MatteStackMode::Add);
    MatteNode off = makeNode(MatteMode::Alpha);
    off.setEnabled(false);
    stack.addNode(off);
    MatteNode nilSource;
    nilSource.setMode(MatteMode::Alpha);
    stack.addNode(nilSource);
    stack.addNode(makeNode(MatteMode::Alpha));
    const std::vector<std::vector<float>> sources = {{0.4f}};
    const auto result = evaluateMatteStack(sources, stack, 1, 1);
    ASSERT_TRUE(result.isValid());
    EXPECT_FLOAT_EQ(result.alphaMask[0], 0.4f);
}

TEST(MatteStackTest, InvertedNodeMode) {
    MatteStack stack;
    stack.setStackMode(MatteStackMode::Common);
    stack.addNode(makeNode(MatteMode::AlphaInverted));
    const std::vector<std::vector<float>> sources = {{0.3f}};
    const auto result = evaluateMatteStack(sources, stack, 1, 1);
    ASSERT_TRUE(result.isValid());
    EXPECT_FLOAT_EQ(result.alphaMask[0], 0.7f);
}

TEST(MatteStackTest, EmptyStackIsPassthrough) {
    MatteStack stack;
    const auto result = evaluateMatteStack({}, stack, 2, 2);
    ASSERT_TRUE(result.isValid());
    for (float v : result.alphaMask) {
        EXPECT_FLOAT_EQ(v, 1.0f);
    }
}

TEST(MatteStackTest, StackModeRoundTrip) {
    EXPECT_EQ(matteStackModeToString(MatteStackMode::Difference),
              QStringLiteral("Difference"));
    EXPECT_EQ(matteStackModeFromString(QStringLiteral("difference")),
              MatteStackMode::Difference);
    EXPECT_EQ(matteStackModeFromString(QStringLiteral("bogus")),
              MatteStackMode::Add);
}
