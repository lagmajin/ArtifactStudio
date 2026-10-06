#include <gtest/gtest.h>
#include <initializer_list>
#include <cmath>

import Graphics.Effect.Creative.ColorVibrance;
import Graphics.Effect.Creative.Posterize;
import Graphics.Effect.Creative.Solarize;
import Video.VideoFrame;
import Channel;

using namespace ArtifactCore;

TEST(ColorVibranceEffectTest, BoostsLowSaturationColor) {
    VideoFrame frame(1, 1);
    auto r = frame.getChannel(ChannelType::Red);
    auto g = frame.getChannel(ChannelType::Green);
    auto b = frame.getChannel(ChannelType::Blue);
    ASSERT_TRUE(r);
    ASSERT_TRUE(g);
    ASSERT_TRUE(b);

    r->data()[0] = 0.40f;
    g->data()[0] = 0.35f;
    b->data()[0] = 0.30f;

    ColorVibranceEffect effect;
    effect.setParameter("Vibrance", 1.0f);
    effect.setParameter("Saturation", 0.0f);
    effect.setParameter("ColorBoost", 1.0f);

    CreativeEffectContext context;
    effect.process(frame, context);

    EXPECT_GT(r->data()[0], 0.40f);
    EXPECT_GT(g->data()[0], 0.35f);
    EXPECT_GT(b->data()[0], 0.30f);
}

TEST(ColorVibranceEffectTest, CanWriteMatteAlpha) {
    VideoFrame frame(1, 1);
    auto r = frame.getChannel(ChannelType::Red);
    auto g = frame.getChannel(ChannelType::Green);
    auto b = frame.getChannel(ChannelType::Blue);
    auto a = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(r);
    ASSERT_TRUE(g);
    ASSERT_TRUE(b);
    ASSERT_TRUE(a);

    r->data()[0] = 0.90f;
    g->data()[0] = 0.15f;
    b->data()[0] = 0.10f;
    a->data()[0] = 0.0f;

    ColorVibranceEffect effect;
    effect.setParameter("MatteAmount", 1.0f);
    effect.setParameter("MatteThreshold", 0.10f);
    effect.setParameter("MatteSoftness", 0.50f);

    CreativeEffectContext context;
    effect.process(frame, context);

    EXPECT_GT(a->data()[0], 0.0f);
}

TEST(ColorVibranceEffectTest, NeutralSettingsPreserveRgbaWithinFloatTolerance) {
    VideoFrame frame(2, 1);
    const float expected[2][4] = {
        {0.10f, 0.25f, 0.80f, 0.0f},
        {0.75f, 0.40f, 0.15f, 0.37f},
    };
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    for (int channel = 0; channel < 4; ++channel) {
        auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < 2; ++pixel) {
            plane->data()[pixel] = expected[pixel][channel];
        }
    }

    ColorVibranceEffect effect;
    effect.setParameter("Vibrance", 0.0f);
    effect.setParameter("Saturation", 0.0f);
    effect.setParameter("ColorBoost", 1.0f);
    effect.setParameter("MatteAmount", 0.0f);
    CreativeEffectContext context;
    effect.process(frame, context);

    for (int channel = 0; channel < 4; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < 2; ++pixel) {
            EXPECT_NEAR(plane->data()[pixel], expected[pixel][channel], 1e-6f)
                << "channel=" << channel << " pixel=" << pixel;
        }
    }
}

TEST(ColorVibranceEffectTest, DisabledEffectPreservesInputWithoutClamping) {
    VideoFrame frame(1, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 1.25f;
    frame.getChannel(ChannelType::Green)->data()[0] = -0.20f;
    frame.getChannel(ChannelType::Blue)->data()[0] = 0.35f;
    frame.getChannel(ChannelType::Alpha)->data()[0] = 0.60f;

    ColorVibranceEffect effect;
    effect.setEnabled(false);
    effect.setParameter("Vibrance", 1.0f);
    effect.setParameter("Saturation", 1.0f);
    effect.setParameter("MatteAmount", 1.0f);
    CreativeEffectContext context;
    effect.process(frame, context);

    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[0], 1.25f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[0], -0.20f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[0], 0.35f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[0], 0.60f);
}

TEST(ColorVibranceEffectTest, MatteMovesAlphaTowardSaturationMask) {
    VideoFrame frame(2, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 0.90f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.10f;
    frame.getChannel(ChannelType::Blue)->data()[0] = 0.10f;
    frame.getChannel(ChannelType::Alpha)->data()[0] = 0.10f;
    frame.getChannel(ChannelType::Red)->data()[1] = 0.30f;
    frame.getChannel(ChannelType::Green)->data()[1] = 0.30f;
    frame.getChannel(ChannelType::Blue)->data()[1] = 0.30f;
    frame.getChannel(ChannelType::Alpha)->data()[1] = 0.80f;
    const float rgbBefore[3][2] = {
        {0.90f, 0.30f}, {0.10f, 0.30f}, {0.10f, 0.30f},
    };

    ColorVibranceEffect effect;
    effect.setParameter("Vibrance", 0.0f);
    effect.setParameter("Saturation", 0.0f);
    effect.setParameter("ColorBoost", 1.0f);
    effect.setParameter("MatteAmount", 0.5f);
    effect.setParameter("MatteThreshold", 0.20f);
    effect.setParameter("MatteSoftness", 0.20f);
    CreativeEffectContext context;
    effect.process(frame, context);

    EXPECT_NEAR(frame.getChannel(ChannelType::Alpha)->data()[0], 0.55f, 1e-6f);
    EXPECT_NEAR(frame.getChannel(ChannelType::Alpha)->data()[1], 0.40f, 1e-6f);
    const ChannelType rgbChannels[] = {
        ChannelType::Red, ChannelType::Green, ChannelType::Blue,
    };
    for (int channel = 0; channel < 3; ++channel) {
        const auto plane = frame.getChannel(rgbChannels[channel]);
        EXPECT_FLOAT_EQ(plane->data()[0], rgbBefore[channel][0]);
        EXPECT_FLOAT_EQ(plane->data()[1], rgbBefore[channel][1]);
    }
}

TEST(ColorVibranceEffectTest, MissingRequiredColorChannelLeavesFrameUnchanged) {
    VideoFrame frame(1, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 0.20f;
    frame.getChannel(ChannelType::Blue)->data()[0] = 0.80f;
    frame.getChannel(ChannelType::Alpha)->data()[0] = 0.45f;
    frame.removeChannel(ChannelType::Green);

    ColorVibranceEffect effect;
    effect.setParameter("Vibrance", 1.0f);
    effect.setParameter("MatteAmount", 1.0f);
    CreativeEffectContext context;
    effect.process(frame, context);

    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[0], 0.20f);
    EXPECT_FALSE(frame.getChannel(ChannelType::Green));
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[0], 0.80f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[0], 0.45f);
}

TEST(ColorVibranceEffectTest, FiniteOutOfRangeRgbInputsProduceFiniteClampedOutput) {
    VideoFrame frame(1, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 2.0f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.25f;
    frame.getChannel(ChannelType::Blue)->data()[0] = -1.0f;
    frame.getChannel(ChannelType::Alpha)->data()[0] = 0.42f;

    ColorVibranceEffect effect;
    effect.setParameter("Vibrance", 1.0f);
    effect.setParameter("Saturation", 1.0f);
    effect.setParameter("ColorBoost", 2.0f);
    CreativeEffectContext context;
    effect.process(frame, context);

    for (const ChannelType channel : {
             ChannelType::Red, ChannelType::Green, ChannelType::Blue}) {
        const float value = frame.getChannel(channel)->data()[0];
        EXPECT_TRUE(std::isfinite(value));
        EXPECT_GE(value, 0.0f);
        EXPECT_LE(value, 1.0f);
    }
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[0], 0.42f);
}

TEST(CreativeImageEffectContractTest, PosterizeQuantizesAtHalfStepBoundariesAndPreservesAlpha) {
    VideoFrame frame(2, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 0.49f;
    frame.getChannel(ChannelType::Red)->data()[1] = 0.50f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.0f;
    frame.getChannel(ChannelType::Green)->data()[1] = 1.0f;
    frame.getChannel(ChannelType::Blue)->data()[0] = 0.17f;
    frame.getChannel(ChannelType::Blue)->data()[1] = 0.83f;
    frame.getChannel(ChannelType::Alpha)->data()[0] = 0.23f;
    frame.getChannel(ChannelType::Alpha)->data()[1] = 0.71f;

    PosterizeEffect effect;
    effect.setParameter("Levels", 4.0f);
    CreativeEffectContext context;
    effect.process(frame, context);

    EXPECT_NEAR(frame.getChannel(ChannelType::Red)->data()[0], 1.0f / 3.0f, 1e-6f);
    EXPECT_NEAR(frame.getChannel(ChannelType::Red)->data()[1], 2.0f / 3.0f, 1e-6f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[0], 0.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[1], 1.0f);
    EXPECT_NEAR(frame.getChannel(ChannelType::Blue)->data()[0], 1.0f / 3.0f, 1e-6f);
    EXPECT_NEAR(frame.getChannel(ChannelType::Blue)->data()[1], 2.0f / 3.0f, 1e-6f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[0], 0.23f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[1], 0.71f);
}

TEST(CreativeImageEffectContractTest, PosterizeClampsLevelsToAtLeastTwo) {
    VideoFrame frame(2, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 0.49f;
    frame.getChannel(ChannelType::Red)->data()[1] = 0.51f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.25f;
    frame.getChannel(ChannelType::Green)->data()[1] = 0.75f;
    frame.getChannel(ChannelType::Blue)->data()[0] = 0.0f;
    frame.getChannel(ChannelType::Blue)->data()[1] = 1.0f;
    frame.getChannel(ChannelType::Alpha)->data()[0] = 0.2f;
    frame.getChannel(ChannelType::Alpha)->data()[1] = 0.8f;

    PosterizeEffect effect;
    effect.setParameter("Levels", -100.0f);
    CreativeEffectContext context;
    effect.process(frame, context);

    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[0], 0.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[1], 1.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[0], 0.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[1], 1.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[0], 0.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[1], 1.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[0], 0.2f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[1], 0.8f);
}

TEST(CreativeImageEffectContractTest, SolarizeThresholdIsStrictAndAlphaIsUnchanged) {
    VideoFrame frame(4, 1);
    const float red[] = {0.25f, 0.50f, 0.75f, 1.0f};
    const float green[] = {0.75f, 0.50f, 0.25f, 1.0f};
    const float blue[] = {0.40f, 0.60f, 0.20f, 0.80f};
    const float alpha[] = {0.12f, 0.34f, 0.56f, 0.78f};
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    const float* source[] = {red, green, blue, alpha};
    for (int channel = 0; channel < 4; ++channel) {
        auto plane = frame.getChannel(channels[channel]);
        for (int pixel = 0; pixel < 4; ++pixel) {
            plane->data()[pixel] = source[channel][pixel];
        }
    }

    SolarizeEffect effect;
    effect.setParameter("Threshold", 0.5f);
    CreativeEffectContext context;
    effect.process(frame, context);

    const float expectedRed[] = {0.25f, 0.50f, 0.50f, 0.0f};
    const float expectedGreen[] = {0.50f, 0.50f, 0.25f, 0.0f};
    const float expectedBlue[] = {0.40f, 0.80f, 0.20f, 0.40f};
    const float* expected[] = {expectedRed, expectedGreen, expectedBlue, alpha};
    for (int channel = 0; channel < 4; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        for (int pixel = 0; pixel < 4; ++pixel) {
            EXPECT_NEAR(plane->data()[pixel], expected[channel][pixel], 1e-6f)
                << "channel=" << channel << " pixel=" << pixel;
        }
    }
}

TEST(CreativeImageEffectContractTest, SolarizeClampsThresholdAndDisabledEffectIsExactPassthrough) {
    VideoFrame frame(2, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 0.25f;
    frame.getChannel(ChannelType::Red)->data()[1] = 0.75f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.1f;
    frame.getChannel(ChannelType::Green)->data()[1] = 0.9f;
    frame.getChannel(ChannelType::Blue)->data()[0] = -0.2f;
    frame.getChannel(ChannelType::Blue)->data()[1] = 1.2f;
    frame.getChannel(ChannelType::Alpha)->data()[0] = 0.35f;
    frame.getChannel(ChannelType::Alpha)->data()[1] = 0.65f;

    SolarizeEffect effect;
    effect.setParameter("Threshold", -10.0f);
    effect.setEnabled(false);
    CreativeEffectContext context;
    effect.process(frame, context);

    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[0], 0.25f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[1], 0.75f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[0], 0.1f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[1], 0.9f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[0], -0.2f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[1], 1.2f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[0], 0.35f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[1], 0.65f);
}

TEST(CreativeImageEffectContractTest, SolarizeClampsThresholdBelowZero) {
    VideoFrame frame(2, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 0.25f;
    frame.getChannel(ChannelType::Red)->data()[1] = 0.75f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.0f;
    frame.getChannel(ChannelType::Green)->data()[1] = 1.0f;
    frame.getChannel(ChannelType::Blue)->data()[0] = 0.4f;
    frame.getChannel(ChannelType::Blue)->data()[1] = 0.8f;
    frame.getChannel(ChannelType::Alpha)->data()[0] = 0.3f;
    frame.getChannel(ChannelType::Alpha)->data()[1] = 0.7f;

    SolarizeEffect effect;
    effect.setParameter("Threshold", -10.0f);
    CreativeEffectContext context;
    effect.process(frame, context);

    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[0], 0.75f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[1], 0.25f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[0], 1.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[1], 0.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[0], 0.6f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[1], 0.2f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[0], 0.3f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[1], 0.7f);
}

TEST(CreativeImageEffectContractTest, SolarizeLeavesFrameUnchangedWhenRequiredChannelIsMissing) {
    VideoFrame frame(1, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 0.25f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.50f;
    frame.getChannel(ChannelType::Blue)->data()[0] = 0.75f;
    frame.getChannel(ChannelType::Alpha)->data()[0] = 0.40f;
    frame.removeChannel(ChannelType::Green);

    SolarizeEffect effect;
    effect.setParameter("Threshold", 0.1f);
    CreativeEffectContext context;
    effect.process(frame, context);

    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[0], 0.25f);
    EXPECT_FALSE(frame.getChannel(ChannelType::Green));
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[0], 0.75f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[0], 0.40f);
}
