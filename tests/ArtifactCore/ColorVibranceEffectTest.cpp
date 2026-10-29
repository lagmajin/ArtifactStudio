#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <initializer_list>
#include <cmath>
#include <vector>

import Graphics.Effect.Creative.ColorVibrance;
import Graphics.Effect.Creative.Posterize;
import Graphics.Effect.Creative.Solarize;
import Graphics.Effect.Creative.Fisheye;
import Graphics.Effect.Creative.Mirror;
import Graphics.Effect.Creative.Pixelate;
import Graphics.Effect.Creative.Halftone;
import Graphics.Effect.Creative.Kaleidoscope;
import Graphics.Effect.Creative.ChromaticAberration;
import Graphics.Effect.Creative.Emboss;
import Graphics.Effect.Creative.EdgeEcho;
import Graphics.Effect.Creative.LightPressure;
import Graphics.Effect.Creative.OldTV;
import Graphics.Effect.Creative.SurfaceMemory;
import Graphics.Effect.Creative.TemporalFossil;
import Graphics.Effect.Creative.DepthMelt;
import Graphics.Effect.Creative.Glitch;
import Graphics.Effect.Creative.PigmentSeparation;
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

TEST(ColorVibranceEffectTest, NegativeVibranceReducesColorSeparation) {
    VideoFrame frame(1, 1);
    const auto red = frame.getChannel(ChannelType::Red);
    const auto green = frame.getChannel(ChannelType::Green);
    const auto blue = frame.getChannel(ChannelType::Blue);
    const auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    red->data()[0] = 0.6f;
    green->data()[0] = 0.5f;
    blue->data()[0] = 0.4f;
    alpha->data()[0] = 0.73f;

    ColorVibranceEffect effect;
    effect.setParameter("Vibrance", -1.0f);
    effect.setParameter("Saturation", 0.0f);
    effect.setParameter("ColorBoost", 1.0f);
    effect.setParameter("MatteAmount", 0.0f);
    effect.process(frame, CreativeEffectContext{});

    constexpr float luma = 0.6f * 0.299f + 0.5f * 0.587f + 0.4f * 0.114f;
    EXPECT_NEAR(red->data()[0], luma + (0.6f - luma) * 0.2f, 1.0e-6f);
    EXPECT_NEAR(green->data()[0], luma + (0.5f - luma) * 0.2f, 1.0e-6f);
    EXPECT_NEAR(blue->data()[0], luma + (0.4f - luma) * 0.2f, 1.0e-6f);
    EXPECT_LT(red->data()[0] - blue->data()[0], 0.2f);
    EXPECT_FLOAT_EQ(alpha->data()[0], 0.73f);
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

TEST(ColorVibranceEffectTest, ZeroMatteSoftnessUsesFiniteThresholdBoundary) {
    VideoFrame frame(3, 1);
    constexpr float red[] = {0.75f, 0.75005f, 0.7502f};
    constexpr float green[] = {0.25f, 0.25f, 0.25f};
    constexpr float blue[] = {0.25f, 0.25f, 0.25f};
    constexpr float alpha[] = {0.2f, 0.4f, 0.6f};
    for (int pixel = 0; pixel < 3; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = red[pixel];
        frame.getChannel(ChannelType::Green)->data()[pixel] = green[pixel];
        frame.getChannel(ChannelType::Blue)->data()[pixel] = blue[pixel];
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha[pixel];
    }

    ColorVibranceEffect effect;
    effect.setParameter("Vibrance", 0.0f);
    effect.setParameter("Saturation", 0.0f);
    effect.setParameter("ColorBoost", 1.0f);
    effect.setParameter("MatteAmount", 1.0f);
    effect.setParameter("MatteThreshold", 0.5f);
    effect.setParameter("MatteSoftness", 0.0f);
    effect.process(frame, CreativeEffectContext{});

    const auto outputAlpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(outputAlpha);
    EXPECT_FLOAT_EQ(outputAlpha->data()[0], 0.0f);
    EXPECT_NEAR(outputAlpha->data()[1], 0.5f, 1.0e-3f);
    EXPECT_FLOAT_EQ(outputAlpha->data()[2], 1.0f);
    for (int pixel = 0; pixel < 3; ++pixel) {
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[pixel], red[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel], green[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[pixel], blue[pixel]);
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

TEST(ColorVibranceEffectTest, MissingOptionalAlphaStillProcessesRgb) {
    VideoFrame frame(1, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 0.40f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.35f;
    frame.getChannel(ChannelType::Blue)->data()[0] = 0.30f;
    frame.removeChannel(ChannelType::Alpha);

    ColorVibranceEffect effect;
    effect.setParameter("Vibrance", 1.0f);
    effect.setParameter("Saturation", 0.0f);
    effect.setParameter("ColorBoost", 1.0f);
    effect.setParameter("MatteAmount", 1.0f);
    effect.process(frame, CreativeEffectContext{});

    EXPECT_GT(frame.getChannel(ChannelType::Red)->data()[0], 0.40f);
    EXPECT_LT(frame.getChannel(ChannelType::Green)->data()[0], 0.35f);
    EXPECT_LT(frame.getChannel(ChannelType::Blue)->data()[0], 0.30f);
    EXPECT_FALSE(frame.getChannel(ChannelType::Alpha));
}

TEST(ColorVibranceEffectTest, MissingRequiredChannelWithoutAlphaLeavesFrameUnchanged) {
    VideoFrame frame(2, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 0.2f;
    frame.getChannel(ChannelType::Red)->data()[1] = 0.8f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.3f;
    frame.getChannel(ChannelType::Green)->data()[1] = 0.7f;
    frame.removeChannel(ChannelType::Blue);
    frame.removeChannel(ChannelType::Alpha);

    ColorVibranceEffect effect;
    effect.setParameter("Vibrance", 1.0f);
    effect.setParameter("Saturation", 1.0f);
    effect.setParameter("ColorBoost", 2.0f);
    effect.setParameter("MatteAmount", 1.0f);
    effect.process(frame, CreativeEffectContext{});

    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[0], 0.2f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[1], 0.8f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[0], 0.3f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[1], 0.7f);
    EXPECT_FALSE(frame.getChannel(ChannelType::Blue));
    EXPECT_FALSE(frame.getChannel(ChannelType::Alpha));
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

TEST(CreativeImageEffectContractTest, PosterizeUsesAllSixtyFourLevels) {
    VideoFrame frame(1, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 0.5f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.25f;
    frame.getChannel(ChannelType::Blue)->data()[0] = 0.75f;
    frame.getChannel(ChannelType::Alpha)->data()[0] = 0.37f;

    PosterizeEffect effect;
    effect.setParameter("Levels", 64.0f);
    effect.process(frame, CreativeEffectContext{});

    EXPECT_NEAR(frame.getChannel(ChannelType::Red)->data()[0], 32.0f / 63.0f,
                1.0e-6f);
    EXPECT_NEAR(frame.getChannel(ChannelType::Green)->data()[0], 16.0f / 63.0f,
                1.0e-6f);
    EXPECT_NEAR(frame.getChannel(ChannelType::Blue)->data()[0], 47.0f / 63.0f,
                1.0e-6f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[0], 0.37f);
}

TEST(CreativeImageEffectContractTest, PosterizeMissingRequiredColorChannelLeavesFrameUnchanged) {
    VideoFrame frame(3, 1);
    const float red[] = {0.1f, 0.43f, 0.9f};
    const float green[] = {0.8f, 0.37f, 0.2f};
    const float alpha[] = {0.0f, 0.45f, 1.0f};
    for (int pixel = 0; pixel < 3; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = red[pixel];
        frame.getChannel(ChannelType::Green)->data()[pixel] = green[pixel];
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha[pixel];
    }
    frame.removeChannel(ChannelType::Blue);

    PosterizeEffect effect;
    effect.setParameter("Levels", 2.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int pixel = 0; pixel < 3; ++pixel) {
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[pixel], red[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel], green[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[pixel], alpha[pixel]);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Blue));
}

TEST(CreativeImageEffectContractTest, PosterizeProcessesRgbWithoutOptionalAlpha) {
    VideoFrame frame(1, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 0.4f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.6f;
    frame.getChannel(ChannelType::Blue)->data()[0] = 0.8f;
    frame.removeChannel(ChannelType::Alpha);

    PosterizeEffect effect;
    effect.setParameter("Levels", 2.0f);
    effect.process(frame, CreativeEffectContext{});

    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[0], 0.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[0], 1.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[0], 1.0f);
    EXPECT_FALSE(frame.getChannel(ChannelType::Alpha));
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

TEST(CreativeImageEffectContractTest, SolarizeClampsThresholdAboveOneAndRgbOutput) {
    VideoFrame frame(1, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 1.5f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.5f;
    frame.getChannel(ChannelType::Blue)->data()[0] = -0.25f;
    frame.getChannel(ChannelType::Alpha)->data()[0] = 1.3f;

    SolarizeEffect effect;
    effect.setParameter("Threshold", 5.0f);
    effect.process(frame, CreativeEffectContext{});

    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[0], 0.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[0], 0.5f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[0], 0.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[0], 1.3f);
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

TEST(CreativeImageEffectContractTest, SolarizeProcessesRgbWithoutOptionalAlpha) {
    VideoFrame frame(1, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 0.25f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.75f;
    frame.getChannel(ChannelType::Blue)->data()[0] = 1.0f;
    frame.removeChannel(ChannelType::Alpha);

    SolarizeEffect effect;
    effect.setParameter("Threshold", 0.5f);
    effect.process(frame, CreativeEffectContext{});

    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[0], 0.25f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[0], 0.5f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[0], 0.0f);
    EXPECT_FALSE(frame.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, FisheyeUnitStrengthAndZoomUseExpectedNearestSamples) {
    VideoFrame frame(3, 3);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int index = 0; index < 9; ++index) {
        red->data()[index] = static_cast<float>(index) / 10.0f;
        green->data()[index] = static_cast<float>(8 - index) / 10.0f;
        blue->data()[index] = static_cast<float>((index * 3) % 9) / 10.0f;
        alpha->data()[index] = static_cast<float>(index + 1) / 10.0f;
    }
    const float expectedAlpha[9] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f,
    };

    FisheyeEffect effect;
    effect.setParameter("Strength", 0.0f);
    effect.setParameter("Zoom", 2.0f);
    effect.process(frame, CreativeEffectContext{});

    const int expectedSourceIndices[9] = {
        0, 1, 1,
        3, 4, 4,
        3, 4, 4,
    };
    for (int index = 0; index < 9; ++index) {
        const int sourceIndex = expectedSourceIndices[index];
        EXPECT_FLOAT_EQ(red->data()[index], static_cast<float>(sourceIndex) / 10.0f)
            << "pixel=" << index;
        EXPECT_FLOAT_EQ(green->data()[index], static_cast<float>(8 - sourceIndex) / 10.0f)
            << "pixel=" << index;
        EXPECT_FLOAT_EQ(blue->data()[index],
                        static_cast<float>((sourceIndex * 3) % 9) / 10.0f)
            << "pixel=" << index;
        EXPECT_FLOAT_EQ(alpha->data()[index], expectedAlpha[index])
            << "pixel=" << index;
    }
}

TEST(CreativeImageEffectContractTest, FisheyeMinimumZoomExpandsAndClampsSamples) {
    VideoFrame frame(3, 3);
    const auto red = frame.getChannel(ChannelType::Red);
    const auto green = frame.getChannel(ChannelType::Green);
    const auto blue = frame.getChannel(ChannelType::Blue);
    const auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int pixel = 0; pixel < 9; ++pixel) {
        red->data()[pixel] = static_cast<float>(pixel) / 10.0f;
        green->data()[pixel] = static_cast<float>(8 - pixel) / 10.0f;
        blue->data()[pixel] = static_cast<float>((pixel * 2) % 9) / 10.0f;
        alpha->data()[pixel] = static_cast<float>(pixel + 1) / 10.0f;
    }

    FisheyeEffect effect;
    effect.setParameter("Strength", 0.0f);
    effect.setParameter("Zoom", 0.5f);
    effect.process(frame, CreativeEffectContext{});

    constexpr int expectedSourceIndices[] = {
        0, 0, 2,
        0, 0, 2,
        6, 6, 8,
    };
    for (int pixel = 0; pixel < 9; ++pixel) {
        const int source = expectedSourceIndices[pixel];
        EXPECT_FLOAT_EQ(red->data()[pixel],
                        static_cast<float>(source) / 10.0f)
            << "red pixel=" << pixel;
        EXPECT_FLOAT_EQ(green->data()[pixel],
                        static_cast<float>(8 - source) / 10.0f)
            << "green pixel=" << pixel;
        EXPECT_FLOAT_EQ(blue->data()[pixel],
                        static_cast<float>((source * 2) % 9) / 10.0f)
            << "blue pixel=" << pixel;
        EXPECT_FLOAT_EQ(alpha->data()[pixel],
                        static_cast<float>(pixel + 1) / 10.0f)
            << "alpha pixel=" << pixel;
    }
}

TEST(CreativeImageEffectContractTest, FisheyeZeroStrengthAndUnitZoomAreExactPassthrough) {
    constexpr int width = 4;
    constexpr int height = 2;
    VideoFrame frame(width, height);
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    float original[width * height][4]{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        original[pixel][0] = static_cast<float>(pixel + 1) / 10.0f;
        original[pixel][1] = static_cast<float>(width * height - pixel) / 11.0f;
        original[pixel][2] = static_cast<float>((pixel * 3) % 7) / 8.0f;
        original[pixel][3] = static_cast<float>(pixel) / 9.0f;
        for (int channel = 0; channel < 4; ++channel) {
            const auto plane = frame.getChannel(channels[channel]);
            ASSERT_TRUE(plane);
            plane->data()[pixel] = original[pixel][channel];
        }
    }

    FisheyeEffect effect;
    effect.setParameter("Strength", 0.0f);
    effect.setParameter("Zoom", 1.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int channel = 0; channel < 4; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(plane->data()[pixel], original[pixel][channel])
                << "channel=" << channel << " pixel=" << pixel;
        }
    }
}

TEST(CreativeImageEffectContractTest, FisheyeProcessesRgbWithoutOptionalAlpha) {
    VideoFrame frame(3, 3);
    for (int pixel = 0; pixel < 9; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = static_cast<float>(pixel) / 10.0f;
        frame.getChannel(ChannelType::Green)->data()[pixel] = static_cast<float>(8 - pixel) / 10.0f;
        frame.getChannel(ChannelType::Blue)->data()[pixel] = static_cast<float>((pixel * 2) % 9) / 10.0f;
    }
    frame.removeChannel(ChannelType::Alpha);

    FisheyeEffect effect;
    effect.setParameter("Strength", 0.0f);
    effect.setParameter("Zoom", 0.5f);
    effect.process(frame, CreativeEffectContext{});

    constexpr int expectedSources[] = {0, 0, 2, 0, 0, 2, 6, 6, 8};
    for (int pixel = 0; pixel < 9; ++pixel) {
        const int source = expectedSources[pixel];
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[pixel],
                        static_cast<float>(source) / 10.0f);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel],
                        static_cast<float>(8 - source) / 10.0f);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[pixel],
                        static_cast<float>((source * 2) % 9) / 10.0f);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, FisheyeMissingRequiredColorChannelLeavesFrameUnchanged) {
    VideoFrame frame(3, 2);
    const float red[] = {0.1f, 0.4f, 0.9f, 0.3f, 0.7f, 0.2f};
    const float green[] = {0.8f, 0.3f, 0.2f, 0.6f, 0.1f, 0.5f};
    const float alpha[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    for (int pixel = 0; pixel < 6; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = red[pixel];
        frame.getChannel(ChannelType::Green)->data()[pixel] = green[pixel];
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha[pixel];
    }
    frame.removeChannel(ChannelType::Blue);

    FisheyeEffect effect;
    effect.setParameter("Strength", 1.0f);
    effect.setParameter("Zoom", 0.5f);
    effect.process(frame, CreativeEffectContext{});

    for (int pixel = 0; pixel < 6; ++pixel) {
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[pixel], red[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel], green[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[pixel], alpha[pixel]);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Blue));
}

TEST(CreativeImageEffectContractTest, MirrorReflectsOnlyTheSelectedHalfAcrossVerticalAxis) {
    VideoFrame frame(4, 1);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    const float inputRed[] = {0.1f, 0.3f, 0.6f, 0.9f};
    const float inputGreen[] = {0.9f, 0.7f, 0.4f, 0.1f};
    const float inputBlue[] = {0.2f, 0.4f, 0.6f, 0.8f};
    const float inputAlpha[] = {0.0f, 0.2f, 0.4f, 0.6f};
    for (int index = 0; index < 4; ++index) {
        red->data()[index] = inputRed[index];
        green->data()[index] = inputGreen[index];
        blue->data()[index] = inputBlue[index];
        alpha->data()[index] = inputAlpha[index];
    }

    MirrorEffect effect;
    effect.setParameter("Angle", 0.0f);
    effect.setParameter("CenterX", 0.5f);
    effect.setParameter("CenterY", 0.5f);
    effect.process(frame, CreativeEffectContext{});

    const int expectedSources[] = {0, 1, 2, 1};
    for (int index = 0; index < 4; ++index) {
        const int sourceIndex = expectedSources[index];
        EXPECT_FLOAT_EQ(red->data()[index], inputRed[sourceIndex]);
        EXPECT_FLOAT_EQ(green->data()[index], inputGreen[sourceIndex]);
        EXPECT_FLOAT_EQ(blue->data()[index], inputBlue[sourceIndex]);
        EXPECT_FLOAT_EQ(alpha->data()[index], inputAlpha[index]);
    }
}

TEST(CreativeImageEffectContractTest, MirrorAnglePiOverTwoReflectsAcrossHorizontalAxis) {
    VideoFrame frame(1, 4);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int y = 0; y < 4; ++y) {
        red->data()[y] = static_cast<float>(y + 1) / 10.0f;
        green->data()[y] = static_cast<float>(y + 2) / 10.0f;
        blue->data()[y] = static_cast<float>(y + 3) / 10.0f;
        alpha->data()[y] = static_cast<float>(y + 1) / 5.0f;
    }

    MirrorEffect effect;
    effect.setParameter("Angle", 1.570796327f);
    effect.setParameter("CenterX", 0.5f);
    effect.setParameter("CenterY", 0.5f);
    effect.process(frame, CreativeEffectContext{});

    const float expectedRed[] = {0.1f, 0.2f, 0.3f, 0.2f};
    const float expectedGreen[] = {0.2f, 0.3f, 0.4f, 0.3f};
    const float expectedBlue[] = {0.3f, 0.4f, 0.5f, 0.4f};
    for (int y = 0; y < 4; ++y) {
        EXPECT_FLOAT_EQ(red->data()[y], expectedRed[y]) << "row=" << y;
        EXPECT_FLOAT_EQ(green->data()[y], expectedGreen[y]) << "row=" << y;
        EXPECT_FLOAT_EQ(blue->data()[y], expectedBlue[y]) << "row=" << y;
        EXPECT_FLOAT_EQ(alpha->data()[y], static_cast<float>(y + 1) / 5.0f)
            << "row=" << y;
    }
}

TEST(CreativeImageEffectContractTest, MirrorAtRightBoundaryPreservesTheWholeFrame) {
    constexpr int width = 4;
    constexpr int height = 2;
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    float original[width * height][4]{};
    VideoFrame frame(width, height);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        original[pixel][0] = static_cast<float>(pixel + 1) / 10.0f;
        original[pixel][1] = static_cast<float>(width * height - pixel) / 11.0f;
        original[pixel][2] = static_cast<float>((pixel * 3) % 7) / 8.0f;
        original[pixel][3] = static_cast<float>(pixel) / 9.0f;
        for (int channel = 0; channel < 4; ++channel) {
            const auto plane = frame.getChannel(channels[channel]);
            ASSERT_TRUE(plane);
            plane->data()[pixel] = original[pixel][channel];
        }
    }

    MirrorEffect effect;
    effect.setParameter("Angle", 0.0f);
    effect.setParameter("CenterX", 1.0f);
    effect.setParameter("CenterY", 0.5f);
    effect.process(frame, CreativeEffectContext{});

    for (int channel = 0; channel < 4; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(plane->data()[pixel], original[pixel][channel])
                << "channel=" << channel << " pixel=" << pixel;
        }
    }
}

TEST(CreativeImageEffectContractTest, MirrorMissingRequiredColorChannelLeavesFrameUnchanged) {
    VideoFrame frame(3, 1);
    const float red[] = {0.1f, 0.4f, 0.9f};
    const float green[] = {0.8f, 0.3f, 0.2f};
    const float alpha[] = {0.0f, 0.45f, 1.0f};
    for (int pixel = 0; pixel < 3; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = red[pixel];
        frame.getChannel(ChannelType::Green)->data()[pixel] = green[pixel];
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha[pixel];
    }
    frame.removeChannel(ChannelType::Blue);

    MirrorEffect effect;
    effect.setParameter("Angle", 0.0f);
    effect.setParameter("CenterX", 0.0f);
    effect.setParameter("CenterY", 0.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int pixel = 0; pixel < 3; ++pixel) {
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[pixel], red[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel], green[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[pixel], alpha[pixel]);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Blue));
}

TEST(CreativeImageEffectContractTest, MirrorProcessesRgbWithoutOptionalAlpha) {
    VideoFrame frame(4, 1);
    const float inputRed[] = {0.1f, 0.3f, 0.6f, 0.9f};
    const float inputGreen[] = {0.9f, 0.7f, 0.4f, 0.1f};
    const float inputBlue[] = {0.2f, 0.4f, 0.6f, 0.8f};
    for (int index = 0; index < 4; ++index) {
        frame.getChannel(ChannelType::Red)->data()[index] = inputRed[index];
        frame.getChannel(ChannelType::Green)->data()[index] = inputGreen[index];
        frame.getChannel(ChannelType::Blue)->data()[index] = inputBlue[index];
    }
    frame.removeChannel(ChannelType::Alpha);

    MirrorEffect effect;
    effect.setParameter("Angle", 0.0f);
    effect.setParameter("CenterX", 0.5f);
    effect.setParameter("CenterY", 0.5f);
    effect.process(frame, CreativeEffectContext{});

    const int expectedSources[] = {0, 1, 2, 1};
    for (int index = 0; index < 4; ++index) {
        const int source = expectedSources[index];
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[index], inputRed[source]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[index], inputGreen[source]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[index], inputBlue[source]);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, PixelateAveragesPartialEdgeBlocksWithoutChangingAlpha) {
    VideoFrame frame(3, 2);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int index = 0; index < 6; ++index) {
        red->data()[index] = static_cast<float>(index);
        green->data()[index] = static_cast<float>(10 + index);
        blue->data()[index] = static_cast<float>(20 + index);
        alpha->data()[index] = static_cast<float>(index) / 10.0f;
    }

    PixelateEffect effect;
    effect.setParameter("BlockSize", 2.0f);
    effect.process(frame, CreativeEffectContext{});

    const float expectedRed[] = {2.0f, 2.0f, 3.5f, 2.0f, 2.0f, 3.5f};
    const float expectedGreen[] = {12.0f, 12.0f, 13.5f, 12.0f, 12.0f, 13.5f};
    const float expectedBlue[] = {22.0f, 22.0f, 23.5f, 22.0f, 22.0f, 23.5f};
    for (int index = 0; index < 6; ++index) {
        EXPECT_FLOAT_EQ(red->data()[index], expectedRed[index]) << "pixel=" << index;
        EXPECT_FLOAT_EQ(green->data()[index], expectedGreen[index]) << "pixel=" << index;
        EXPECT_FLOAT_EQ(blue->data()[index], expectedBlue[index]) << "pixel=" << index;
        EXPECT_FLOAT_EQ(alpha->data()[index], static_cast<float>(index) / 10.0f)
            << "pixel=" << index;
    }
}

TEST(CreativeImageEffectContractTest, PixelateBlockSizeOnePreservesEveryChannel) {
    constexpr int width = 3;
    constexpr int height = 2;
    VideoFrame frame(width, height);
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    float original[width * height][4]{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        original[pixel][0] = static_cast<float>(pixel + 1) / 8.0f;
        original[pixel][1] = static_cast<float>(7 - pixel) / 9.0f;
        original[pixel][2] = static_cast<float>(pixel * 2 + 1) / 10.0f;
        original[pixel][3] = static_cast<float>(pixel) / 5.0f;
        for (int channel = 0; channel < 4; ++channel) {
            const auto plane = frame.getChannel(channels[channel]);
            ASSERT_TRUE(plane);
            plane->data()[pixel] = original[pixel][channel];
        }
    }

    PixelateEffect effect;
    effect.setParameter("BlockSize", 1.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int channel = 0; channel < 4; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(plane->data()[pixel], original[pixel][channel])
                << "channel=" << channel << " pixel=" << pixel;
        }
    }
}

TEST(CreativeImageEffectContractTest, PixelateNonPositiveBlockSizeFallsBackToOnePixel) {
    constexpr int width = 3;
    constexpr int height = 2;
    VideoFrame frame(width, height);
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    float original[width * height][4]{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        original[pixel][0] = static_cast<float>(pixel + 1) / 8.0f;
        original[pixel][1] = static_cast<float>(7 - pixel) / 9.0f;
        original[pixel][2] = static_cast<float>(pixel * 2 + 1) / 10.0f;
        original[pixel][3] = static_cast<float>(pixel) / 5.0f;
        for (int channel = 0; channel < 4; ++channel) {
            const auto plane = frame.getChannel(channels[channel]);
            ASSERT_TRUE(plane);
            plane->data()[pixel] = original[pixel][channel];
        }
    }

    PixelateEffect effect;
    effect.setParameter("BlockSize", 0.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int channel = 0; channel < 4; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(plane->data()[pixel], original[pixel][channel])
                << "channel=" << channel << " pixel=" << pixel;
        }
    }
}

TEST(CreativeImageEffectContractTest, PixelateMissingRequiredColorChannelLeavesFrameUnchanged) {
    VideoFrame frame(3, 1);
    const float red[] = {0.1f, 0.5f, 0.9f};
    const float green[] = {0.8f, 0.4f, 0.2f};
    const float alpha[] = {0.0f, 0.45f, 1.0f};
    for (int pixel = 0; pixel < 3; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = red[pixel];
        frame.getChannel(ChannelType::Green)->data()[pixel] = green[pixel];
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha[pixel];
    }
    frame.removeChannel(ChannelType::Blue);

    PixelateEffect effect;
    effect.setParameter("BlockSize", 3.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int pixel = 0; pixel < 3; ++pixel) {
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[pixel], red[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel], green[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[pixel], alpha[pixel]);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Blue));
}

TEST(CreativeImageEffectContractTest, PixelateAveragesRgbWithoutOptionalAlpha) {
    VideoFrame frame(2, 1);
    frame.getChannel(ChannelType::Red)->data()[0] = 0.2f;
    frame.getChannel(ChannelType::Red)->data()[1] = 0.8f;
    frame.getChannel(ChannelType::Green)->data()[0] = 0.4f;
    frame.getChannel(ChannelType::Green)->data()[1] = 0.6f;
    frame.getChannel(ChannelType::Blue)->data()[0] = 0.1f;
    frame.getChannel(ChannelType::Blue)->data()[1] = 0.9f;
    frame.removeChannel(ChannelType::Alpha);

    PixelateEffect effect;
    effect.setParameter("BlockSize", 2.0f);
    effect.process(frame, CreativeEffectContext{});

    bool changedRgb = false;
    for (const ChannelType channelType : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
        const auto channel = frame.getChannel(channelType);
        ASSERT_TRUE(channel);
        EXPECT_FLOAT_EQ(channel->data()[0], 0.5f);
        EXPECT_FLOAT_EQ(channel->data()[1], 0.5f);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, HalftoneCreatesExpectedDotCenterAndCornerValues) {
    VideoFrame frame(4, 4);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int index = 0; index < 16; ++index) {
        red->data()[index] = 0.25f;
        green->data()[index] = 0.5f;
        blue->data()[index] = 0.75f;
        alpha->data()[index] = static_cast<float>(index) / 20.0f;
    }

    HalftoneEffect effect;
    effect.setParameter("Size", 4.0f);
    effect.setParameter("Angle", 0.0f);
    effect.setParameter("Contrast", 0.0f);
    effect.process(frame, CreativeEffectContext{});

    EXPECT_NEAR(red->data()[0], 0.5004f, 1e-3f);
    EXPECT_FLOAT_EQ(green->data()[0], red->data()[0]);
    EXPECT_FLOAT_EQ(blue->data()[0], red->data()[0]);
    EXPECT_FLOAT_EQ(red->data()[2 * 4 + 2], 0.0f);
    EXPECT_FLOAT_EQ(green->data()[2 * 4 + 2], 0.0f);
    EXPECT_FLOAT_EQ(blue->data()[2 * 4 + 2], 0.0f);
    for (int index = 0; index < 16; ++index) {
        EXPECT_FLOAT_EQ(alpha->data()[index], static_cast<float>(index) / 20.0f)
            << "pixel=" << index;
    }
}

TEST(CreativeImageEffectContractTest, HalftoneProcessesRgbWithoutOptionalAlpha) {
    VideoFrame frame(2, 2);
    for (int pixel = 0; pixel < 4; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = 1.0f;
        frame.getChannel(ChannelType::Green)->data()[pixel] = 1.0f;
        frame.getChannel(ChannelType::Blue)->data()[pixel] = 1.0f;
    }
    frame.removeChannel(ChannelType::Alpha);

    HalftoneEffect effect;
    effect.setParameter("Size", 2.0f);
    effect.setParameter("Angle", 0.0f);
    effect.setParameter("Contrast", 0.0f);
    effect.process(frame, CreativeEffectContext{});

    EXPECT_FALSE(frame.getChannel(ChannelType::Alpha));
    for (const ChannelType channelType : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
        const auto channel = frame.getChannel(channelType);
        ASSERT_TRUE(channel);
        EXPECT_FLOAT_EQ(channel->data()[3], 0.0f);
        for (int pixel = 0; pixel < 4; ++pixel) {
            EXPECT_TRUE(std::isfinite(channel->data()[pixel]));
            EXPECT_GE(channel->data()[pixel], 0.0f);
            EXPECT_LE(channel->data()[pixel], 1.0f);
        }
    }
}

TEST(CreativeImageEffectContractTest, HalftoneContrastChangesDotCoverage) {
    VideoFrame noContrast(4, 4);
    VideoFrame highContrast(4, 4);
    for (int index = 0; index < 16; ++index) {
        for (const auto channel : {ChannelType::Red, ChannelType::Green,
                                   ChannelType::Blue}) {
            auto noContrastPlane = noContrast.getChannel(channel);
            auto highContrastPlane = highContrast.getChannel(channel);
            ASSERT_TRUE(noContrastPlane);
            ASSERT_TRUE(highContrastPlane);
            const float value = channel == ChannelType::Red ? 0.25f
                : channel == ChannelType::Green ? 0.5f : 0.75f;
            noContrastPlane->data()[index] = value;
            highContrastPlane->data()[index] = value;
        }
    }

    HalftoneEffect flatEffect;
    flatEffect.setParameter("Size", 4.0f);
    flatEffect.setParameter("Angle", 0.0f);
    flatEffect.setParameter("Contrast", 0.0f);
    HalftoneEffect contrastEffect;
    contrastEffect.setParameter("Size", 4.0f);
    contrastEffect.setParameter("Angle", 0.0f);
    contrastEffect.setParameter("Contrast", 2.0f);
    flatEffect.process(noContrast, CreativeEffectContext{});
    contrastEffect.process(highContrast, CreativeEffectContext{});

    const auto flatRed = noContrast.getChannel(ChannelType::Red);
    const auto contrastRed = highContrast.getChannel(ChannelType::Red);
    ASSERT_TRUE(flatRed);
    ASSERT_TRUE(contrastRed);
    constexpr int dotEdgePixel = 2 * 4;
    EXPECT_FLOAT_EQ(flatRed->data()[dotEdgePixel], 0.0f);
    EXPECT_FLOAT_EQ(contrastRed->data()[dotEdgePixel], 1.0f);
}

TEST(CreativeImageEffectContractTest, HalftoneAngleRotatesDotGridPhase) {
    VideoFrame unrotated(4, 4);
    VideoFrame rotated(4, 4);
    for (int index = 0; index < 16; ++index) {
        for (const auto channel : {ChannelType::Red, ChannelType::Green,
                                   ChannelType::Blue}) {
            auto unrotatedPlane = unrotated.getChannel(channel);
            auto rotatedPlane = rotated.getChannel(channel);
            ASSERT_TRUE(unrotatedPlane);
            ASSERT_TRUE(rotatedPlane);
            unrotatedPlane->data()[index] = 0.5f;
            rotatedPlane->data()[index] = 0.5f;
        }
        auto unrotatedAlpha = unrotated.getChannel(ChannelType::Alpha);
        auto rotatedAlpha = rotated.getChannel(ChannelType::Alpha);
        ASSERT_TRUE(unrotatedAlpha);
        ASSERT_TRUE(rotatedAlpha);
        unrotatedAlpha->data()[index] = static_cast<float>(index) / 16.0f;
        rotatedAlpha->data()[index] = static_cast<float>(index) / 16.0f;
    }

    HalftoneEffect unrotatedEffect;
    unrotatedEffect.setParameter("Size", 4.0f);
    unrotatedEffect.setParameter("Angle", 0.0f);
    unrotatedEffect.setParameter("Contrast", 1.0f);
    HalftoneEffect rotatedEffect;
    rotatedEffect.setParameter("Size", 4.0f);
    rotatedEffect.setParameter("Angle", 0.785398163f);
    rotatedEffect.setParameter("Contrast", 1.0f);
    unrotatedEffect.process(unrotated, CreativeEffectContext{});
    rotatedEffect.process(rotated, CreativeEffectContext{});

    constexpr int phaseSample = 4;
    const auto unrotatedRed = unrotated.getChannel(ChannelType::Red);
    const auto unrotatedGreen = unrotated.getChannel(ChannelType::Green);
    const auto unrotatedBlue = unrotated.getChannel(ChannelType::Blue);
    const auto rotatedRed = rotated.getChannel(ChannelType::Red);
    const auto rotatedGreen = rotated.getChannel(ChannelType::Green);
    const auto rotatedBlue = rotated.getChannel(ChannelType::Blue);
    ASSERT_TRUE(unrotatedRed);
    ASSERT_TRUE(unrotatedGreen);
    ASSERT_TRUE(unrotatedBlue);
    ASSERT_TRUE(rotatedRed);
    ASSERT_TRUE(rotatedGreen);
    ASSERT_TRUE(rotatedBlue);
    EXPECT_GT(unrotatedRed->data()[phaseSample],
              rotatedRed->data()[phaseSample] + 0.2f);
    EXPECT_FLOAT_EQ(unrotatedRed->data()[phaseSample],
                    unrotatedGreen->data()[phaseSample]);
    EXPECT_FLOAT_EQ(unrotatedRed->data()[phaseSample],
                    unrotatedBlue->data()[phaseSample]);
    EXPECT_FLOAT_EQ(rotatedRed->data()[phaseSample],
                    rotatedGreen->data()[phaseSample]);
    EXPECT_FLOAT_EQ(rotatedRed->data()[phaseSample],
                    rotatedBlue->data()[phaseSample]);
    const auto unrotatedAlpha = unrotated.getChannel(ChannelType::Alpha);
    const auto rotatedAlpha = rotated.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(unrotatedAlpha);
    ASSERT_TRUE(rotatedAlpha);
    for (int index = 0; index < 16; ++index) {
        EXPECT_FLOAT_EQ(unrotatedAlpha->data()[index],
                        static_cast<float>(index) / 16.0f);
        EXPECT_FLOAT_EQ(rotatedAlpha->data()[index],
                        static_cast<float>(index) / 16.0f);
    }
}

TEST(CreativeImageEffectContractTest, HalftoneLuminanceControlsDotCoverage) {
    VideoFrame blackFrame(1, 1);
    VideoFrame whiteFrame(1, 1);
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        blackFrame.getChannel(channel)->data()[0] = 0.0f;
        whiteFrame.getChannel(channel)->data()[0] = 1.0f;
    }
    blackFrame.getChannel(ChannelType::Alpha)->data()[0] = 0.35f;
    whiteFrame.getChannel(ChannelType::Alpha)->data()[0] = 0.65f;

    HalftoneEffect blackEffect;
    blackEffect.setParameter("Size", 4.0f);
    blackEffect.setParameter("Angle", 0.0f);
    blackEffect.setParameter("Contrast", 1.0f);
    HalftoneEffect whiteEffect;
    whiteEffect.setParameter("Size", 4.0f);
    whiteEffect.setParameter("Angle", 0.0f);
    whiteEffect.setParameter("Contrast", 1.0f);
    blackEffect.process(blackFrame, CreativeEffectContext{});
    whiteEffect.process(whiteFrame, CreativeEffectContext{});

    EXPECT_FLOAT_EQ(blackFrame.getChannel(ChannelType::Red)->data()[0], 0.5f);
    EXPECT_FLOAT_EQ(whiteFrame.getChannel(ChannelType::Red)->data()[0], 1.0f);
    for (const ChannelType channel : {ChannelType::Green, ChannelType::Blue}) {
        EXPECT_FLOAT_EQ(blackFrame.getChannel(channel)->data()[0], 0.5f);
        EXPECT_FLOAT_EQ(whiteFrame.getChannel(channel)->data()[0], 1.0f);
    }
    EXPECT_FLOAT_EQ(blackFrame.getChannel(ChannelType::Alpha)->data()[0], 0.35f);
    EXPECT_FLOAT_EQ(whiteFrame.getChannel(ChannelType::Alpha)->data()[0], 0.65f);
}

TEST(CreativeImageEffectContractTest, KaleidoscopeCountOneFoldsUpperRightTowardLowerWedge) {
    VideoFrame frame(3, 3);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int index = 0; index < 9; ++index) {
        red->data()[index] = static_cast<float>(index) / 10.0f;
        green->data()[index] = static_cast<float>(10 + index) / 10.0f;
        blue->data()[index] = static_cast<float>(20 + index) / 10.0f;
        alpha->data()[index] = static_cast<float>(index) / 10.0f;
    }

    KaleidoscopeEffect effect;
    effect.setParameter("Count", 1.0f);
    effect.setParameter("Angle", 0.0f);
    effect.setParameter("CenterX", 0.5f);
    effect.setParameter("CenterY", 0.5f);
    effect.process(frame, CreativeEffectContext{});

    const int destinationIndex = 1 * 3 + 2;
    const int expectedSourceIndex = 2 * 3 + 2;
    EXPECT_FLOAT_EQ(red->data()[destinationIndex],
                    static_cast<float>(expectedSourceIndex) / 10.0f);
    EXPECT_FLOAT_EQ(green->data()[destinationIndex],
                    static_cast<float>(10 + expectedSourceIndex) / 10.0f);
    EXPECT_FLOAT_EQ(blue->data()[destinationIndex],
                    static_cast<float>(20 + expectedSourceIndex) / 10.0f);
    for (int index = 0; index < 9; ++index) {
        EXPECT_FLOAT_EQ(alpha->data()[index], static_cast<float>(index) / 10.0f)
            << "pixel=" << index;
    }
}

TEST(CreativeImageEffectContractTest, KaleidoscopeCenterControlsTheSymmetryOrigin) {
    VideoFrame centered(3, 3);
    VideoFrame originCentered(3, 3);
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    for (const ChannelType channel : channels) {
        auto centeredPlane = centered.getChannel(channel);
        auto originPlane = originCentered.getChannel(channel);
        ASSERT_TRUE(centeredPlane);
        ASSERT_TRUE(originPlane);
        for (int pixel = 0; pixel < 9; ++pixel) {
            const float value = static_cast<float>(pixel) / 10.0f;
            centeredPlane->data()[pixel] = value;
            originPlane->data()[pixel] = value;
        }
    }

    KaleidoscopeEffect centeredEffect;
    centeredEffect.setParameter("Count", 4.0f);
    centeredEffect.setParameter("Angle", 0.0f);
    centeredEffect.setParameter("CenterX", 0.5f);
    centeredEffect.setParameter("CenterY", 0.5f);
    KaleidoscopeEffect originEffect;
    originEffect.setParameter("Count", 4.0f);
    originEffect.setParameter("Angle", 0.0f);
    originEffect.setParameter("CenterX", 0.0f);
    originEffect.setParameter("CenterY", 0.0f);
    centeredEffect.process(centered, CreativeEffectContext{});
    originEffect.process(originCentered, CreativeEffectContext{});

    constexpr int upperRightPixel = 1 * 3 + 2;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto centeredPlane = centered.getChannel(channel);
        const auto originPlane = originCentered.getChannel(channel);
        ASSERT_TRUE(centeredPlane);
        ASSERT_TRUE(originPlane);
        EXPECT_FLOAT_EQ(centeredPlane->data()[upperRightPixel], 0.8f);
        EXPECT_FLOAT_EQ(originPlane->data()[upperRightPixel], 0.5f);
    }
    const auto centeredAlpha = centered.getChannel(ChannelType::Alpha);
    const auto originAlpha = originCentered.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(centeredAlpha);
    ASSERT_TRUE(originAlpha);
    for (int pixel = 0; pixel < 9; ++pixel) {
        EXPECT_FLOAT_EQ(centeredAlpha->data()[pixel],
                        static_cast<float>(pixel) / 10.0f);
        EXPECT_FLOAT_EQ(originAlpha->data()[pixel],
                        static_cast<float>(pixel) / 10.0f);
    }
}

TEST(CreativeImageEffectContractTest, KaleidoscopeProcessesRgbWithoutOptionalAlpha) {
    VideoFrame frame(3, 3);
    for (int pixel = 0; pixel < 9; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = static_cast<float>(pixel) / 10.0f;
        frame.getChannel(ChannelType::Green)->data()[pixel] = static_cast<float>(10 + pixel) / 10.0f;
        frame.getChannel(ChannelType::Blue)->data()[pixel] = static_cast<float>(20 + pixel) / 10.0f;
    }
    frame.removeChannel(ChannelType::Alpha);

    KaleidoscopeEffect effect;
    effect.setParameter("Count", 1.0f);
    effect.setParameter("Angle", 0.0f);
    effect.setParameter("CenterX", 0.5f);
    effect.setParameter("CenterY", 0.5f);
    effect.process(frame, CreativeEffectContext{});

    constexpr int destinationPixel = 5;
    constexpr int expectedSourcePixel = 8;
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[destinationPixel],
                    static_cast<float>(expectedSourcePixel) / 10.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[destinationPixel],
                    static_cast<float>(10 + expectedSourcePixel) / 10.0f);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[destinationPixel],
                    static_cast<float>(20 + expectedSourcePixel) / 10.0f);
    EXPECT_FALSE(frame.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, KaleidoscopeNonPositiveCountUsesOneDivision) {
    constexpr int width = 3;
    constexpr int height = 3;
    VideoFrame invalidCountFrame(width, height);
    VideoFrame minimumCountFrame(width, height);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float red = static_cast<float>(pixel) / 10.0f;
        const float green = static_cast<float>(8 - pixel) / 10.0f;
        const float blue = static_cast<float>((pixel * 2) % 9) / 10.0f;
        const float alpha = static_cast<float>(pixel + 1) / 10.0f;
        invalidCountFrame.getChannel(ChannelType::Red)->data()[pixel] = red;
        invalidCountFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
        invalidCountFrame.getChannel(ChannelType::Blue)->data()[pixel] = blue;
        invalidCountFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        minimumCountFrame.getChannel(ChannelType::Red)->data()[pixel] = red;
        minimumCountFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
        minimumCountFrame.getChannel(ChannelType::Blue)->data()[pixel] = blue;
        minimumCountFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
    }

    KaleidoscopeEffect invalidCount;
    invalidCount.setParameter("Count", 0.0f);
    invalidCount.setParameter("Angle", 0.37f);
    invalidCount.setParameter("CenterX", 0.4f);
    invalidCount.setParameter("CenterY", 0.6f);
    KaleidoscopeEffect minimumCount;
    minimumCount.setParameter("Count", 1.0f);
    minimumCount.setParameter("Angle", 0.37f);
    minimumCount.setParameter("CenterX", 0.4f);
    minimumCount.setParameter("CenterY", 0.6f);
    invalidCount.process(invalidCountFrame, CreativeEffectContext{});
    minimumCount.process(minimumCountFrame, CreativeEffectContext{});

    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue, ChannelType::Alpha}) {
        const auto invalid = invalidCountFrame.getChannel(channel);
        const auto minimum = minimumCountFrame.getChannel(channel);
        ASSERT_TRUE(invalid);
        ASSERT_TRUE(minimum);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(invalid->data()[pixel], minimum->data()[pixel])
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
        }
    }
}

TEST(CreativeImageEffectContractTest, KaleidoscopeMissingRequiredColorChannelLeavesFrameUnchanged) {
    VideoFrame frame(3, 2);
    const float red[] = {0.1f, 0.4f, 0.9f, 0.3f, 0.7f, 0.2f};
    const float green[] = {0.8f, 0.3f, 0.2f, 0.6f, 0.1f, 0.5f};
    const float alpha[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    for (int pixel = 0; pixel < 6; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = red[pixel];
        frame.getChannel(ChannelType::Green)->data()[pixel] = green[pixel];
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha[pixel];
    }
    frame.removeChannel(ChannelType::Blue);

    KaleidoscopeEffect effect;
    effect.setParameter("Count", 24.0f);
    effect.setParameter("Angle", 1.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int pixel = 0; pixel < 6; ++pixel) {
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[pixel], red[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel], green[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[pixel], alpha[pixel]);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Blue));
}

TEST(CreativeImageEffectContractTest, ChromaticAberrationShiftsRedAndBlueInOppositeDirections) {
    VideoFrame frame(3, 1);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    const float inputRed[] = {0.1f, 0.2f, 0.3f};
    const float inputGreen[] = {0.4f, 0.5f, 0.6f};
    const float inputBlue[] = {0.7f, 0.8f, 0.9f};
    const float inputAlpha[] = {0.2f, 0.4f, 0.6f};
    for (int index = 0; index < 3; ++index) {
        red->data()[index] = inputRed[index];
        green->data()[index] = inputGreen[index];
        blue->data()[index] = inputBlue[index];
        alpha->data()[index] = inputAlpha[index];
    }

    ChromaticAberrationEffect effect;
    effect.setParameter("Amount", 1.0f);
    effect.setParameter("Angle", 0.0f);
    effect.process(frame, CreativeEffectContext{});

    const float expectedRed[] = {0.2f, 0.3f, 0.3f};
    const float expectedBlue[] = {0.7f, 0.7f, 0.8f};
    for (int index = 0; index < 3; ++index) {
        EXPECT_FLOAT_EQ(red->data()[index], expectedRed[index]) << "pixel=" << index;
        EXPECT_FLOAT_EQ(green->data()[index], inputGreen[index]) << "pixel=" << index;
        EXPECT_FLOAT_EQ(blue->data()[index], expectedBlue[index]) << "pixel=" << index;
        EXPECT_FLOAT_EQ(alpha->data()[index], inputAlpha[index]) << "pixel=" << index;
    }
}

TEST(CreativeImageEffectContractTest, ChromaticAberrationAngleShiftsVerticallyAndClampsEdges) {
    VideoFrame frame(1, 3);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    const float inputRed[] = {0.1f, 0.2f, 0.3f};
    const float inputGreen[] = {0.4f, 0.5f, 0.6f};
    const float inputBlue[] = {0.7f, 0.8f, 0.9f};
    const float inputAlpha[] = {0.2f, 0.4f, 0.6f};
    for (int y = 0; y < 3; ++y) {
        red->data()[y] = inputRed[y];
        green->data()[y] = inputGreen[y];
        blue->data()[y] = inputBlue[y];
        alpha->data()[y] = inputAlpha[y];
    }

    ChromaticAberrationEffect effect;
    effect.setParameter("Amount", 1.0f);
    effect.setParameter("Angle", 90.0f);
    effect.process(frame, CreativeEffectContext{});

    const float expectedRed[] = {0.2f, 0.3f, 0.3f};
    const float expectedBlue[] = {0.7f, 0.7f, 0.8f};
    for (int y = 0; y < 3; ++y) {
        EXPECT_FLOAT_EQ(red->data()[y], expectedRed[y]) << "row=" << y;
        EXPECT_FLOAT_EQ(green->data()[y], inputGreen[y]) << "row=" << y;
        EXPECT_FLOAT_EQ(blue->data()[y], expectedBlue[y]) << "row=" << y;
        EXPECT_FLOAT_EQ(alpha->data()[y], inputAlpha[y]) << "row=" << y;
    }
}

TEST(CreativeImageEffectContractTest, ChromaticAberrationAmountRoundsAtHalfPixelBoundary) {
    constexpr int width = 5;
    constexpr int height = 1;
    VideoFrame belowThreshold(width, height);
    VideoFrame atThreshold(width, height);
    for (int x = 0; x < width; ++x) {
        const float value = static_cast<float>(x) / (width - 1);
        const float alpha = 0.2f + value * 0.6f;
        for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
            belowThreshold.getChannel(channel)->data()[x] = value;
            atThreshold.getChannel(channel)->data()[x] = value;
        }
        belowThreshold.getChannel(ChannelType::Alpha)->data()[x] = alpha;
        atThreshold.getChannel(ChannelType::Alpha)->data()[x] = alpha;
    }

    ChromaticAberrationEffect subpixel;
    subpixel.setParameter("Amount", 0.49f);
    subpixel.setParameter("Angle", 0.0f);
    subpixel.process(belowThreshold, CreativeEffectContext{});
    ChromaticAberrationEffect halfPixel;
    halfPixel.setParameter("Amount", 0.5f);
    halfPixel.setParameter("Angle", 0.0f);
    halfPixel.process(atThreshold, CreativeEffectContext{});

    const auto belowRed = belowThreshold.getChannel(ChannelType::Red);
    const auto belowGreen = belowThreshold.getChannel(ChannelType::Green);
    const auto belowBlue = belowThreshold.getChannel(ChannelType::Blue);
    const auto belowAlpha = belowThreshold.getChannel(ChannelType::Alpha);
    const auto thresholdRed = atThreshold.getChannel(ChannelType::Red);
    const auto thresholdGreen = atThreshold.getChannel(ChannelType::Green);
    const auto thresholdBlue = atThreshold.getChannel(ChannelType::Blue);
    const auto thresholdAlpha = atThreshold.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(belowRed);
    ASSERT_TRUE(belowGreen);
    ASSERT_TRUE(belowBlue);
    ASSERT_TRUE(belowAlpha);
    ASSERT_TRUE(thresholdRed);
    ASSERT_TRUE(thresholdGreen);
    ASSERT_TRUE(thresholdBlue);
    ASSERT_TRUE(thresholdAlpha);
    for (int x = 0; x < width; ++x) {
        const float value = static_cast<float>(x) / (width - 1);
        const float expectedRed = static_cast<float>(std::min(x + 1, width - 1)) /
                                  (width - 1);
        const float expectedBlue = static_cast<float>(std::max(x - 1, 0)) /
                                   (width - 1);
        EXPECT_FLOAT_EQ(belowRed->data()[x], value) << "subpixel red x=" << x;
        EXPECT_FLOAT_EQ(belowGreen->data()[x], value) << "subpixel green x=" << x;
        EXPECT_FLOAT_EQ(belowBlue->data()[x], value) << "subpixel blue x=" << x;
        EXPECT_FLOAT_EQ(thresholdRed->data()[x], expectedRed) << "red x=" << x;
        EXPECT_FLOAT_EQ(thresholdBlue->data()[x], expectedBlue) << "blue x=" << x;
        EXPECT_FLOAT_EQ(thresholdGreen->data()[x], value) << "green x=" << x;
        const float alpha = 0.2f + value * 0.6f;
        EXPECT_FLOAT_EQ(belowAlpha->data()[x], alpha);
        EXPECT_FLOAT_EQ(thresholdAlpha->data()[x], alpha);
    }
}

TEST(CreativeImageEffectContractTest, ChromaticAberrationMissingRequiredColorChannelLeavesFrameUnchanged) {
    VideoFrame frame(3, 2);
    const float red[] = {0.1f, 0.4f, 0.9f, 0.3f, 0.7f, 0.2f};
    const float green[] = {0.8f, 0.3f, 0.2f, 0.6f, 0.1f, 0.5f};
    const float alpha[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    for (int pixel = 0; pixel < 6; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = red[pixel];
        frame.getChannel(ChannelType::Green)->data()[pixel] = green[pixel];
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha[pixel];
    }
    frame.removeChannel(ChannelType::Blue);

    ChromaticAberrationEffect effect;
    effect.setParameter("Amount", 50.0f);
    effect.setParameter("Angle", 45.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int pixel = 0; pixel < 6; ++pixel) {
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[pixel], red[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel], green[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[pixel], alpha[pixel]);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Blue));
}

TEST(CreativeImageEffectContractTest, ChromaticAberrationProcessesRgbWithoutOptionalAlpha) {
    VideoFrame frame(3, 1);
    const float inputRed[] = {0.1f, 0.4f, 0.9f};
    const float inputGreen[] = {0.2f, 0.5f, 0.8f};
    const float inputBlue[] = {0.3f, 0.6f, 0.7f};
    for (int pixel = 0; pixel < 3; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = inputRed[pixel];
        frame.getChannel(ChannelType::Green)->data()[pixel] = inputGreen[pixel];
        frame.getChannel(ChannelType::Blue)->data()[pixel] = inputBlue[pixel];
    }
    frame.removeChannel(ChannelType::Alpha);

    ChromaticAberrationEffect effect;
    effect.setParameter("Amount", 1.0f);
    effect.setParameter("Angle", 0.0f);
    effect.process(frame, CreativeEffectContext{});

    const float expectedRed[] = {0.4f, 0.9f, 0.9f};
    const float expectedBlue[] = {0.3f, 0.3f, 0.6f};
    for (int pixel = 0; pixel < 3; ++pixel) {
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[pixel], expectedRed[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel], inputGreen[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[pixel], expectedBlue[pixel]);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, EmbossUsesDiagonalDifferenceAndPreservesUnprocessedEdges) {
    VideoFrame frame(2, 2);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    const float inputRed[] = {0.1f, 0.2f, 0.3f, 0.8f};
    const float inputGreen[] = {0.8f, 0.3f, 0.6f, 0.2f};
    const float inputBlue[] = {0.2f, 0.1f, 0.3f, 0.6f};
    for (int index = 0; index < 4; ++index) {
        red->data()[index] = inputRed[index];
        green->data()[index] = inputGreen[index];
        blue->data()[index] = inputBlue[index];
        alpha->data()[index] = static_cast<float>(index) / 4.0f;
    }

    EmbossEffect effect;
    effect.setParameter("Strength", 1.0f);
    effect.setParameter("Height", 2.0f);
    effect.process(frame, CreativeEffectContext{});

    EXPECT_FLOAT_EQ(red->data()[0], 1.0f);
    EXPECT_FLOAT_EQ(green->data()[0], 0.0f);
    EXPECT_FLOAT_EQ(blue->data()[0], 1.0f);
    for (int index = 1; index < 4; ++index) {
        EXPECT_FLOAT_EQ(red->data()[index], inputRed[index]) << "red pixel=" << index;
        EXPECT_FLOAT_EQ(green->data()[index], inputGreen[index])
            << "green pixel=" << index;
        EXPECT_FLOAT_EQ(blue->data()[index], inputBlue[index])
            << "blue pixel=" << index;
    }
    for (int index = 0; index < 4; ++index) {
        EXPECT_FLOAT_EQ(alpha->data()[index], static_cast<float>(index) / 4.0f)
            << "pixel=" << index;
    }
}

TEST(CreativeImageEffectContractTest, EmbossZeroStrengthProducesNeutralReliefAndKeepsAlpha) {
    constexpr int width = 2;
    constexpr int height = 2;
    VideoFrame frame(width, height);
    const float inputRed[] = {0.1f, 0.3f, 0.6f, 0.9f};
    const float inputGreen[] = {0.9f, 0.6f, 0.3f, 0.1f};
    const float inputBlue[] = {0.2f, 0.4f, 0.7f, 0.8f};
    const float inputAlpha[] = {0.15f, 0.35f, 0.65f, 0.85f};
    const float* inputs[] = {inputRed, inputGreen, inputBlue, inputAlpha};
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    for (int channel = 0; channel < 4; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            plane->data()[pixel] = inputs[channel][pixel];
        }
    }

    EmbossEffect effect;
    effect.setParameter("Strength", 0.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int channel = 0; channel < 3; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(plane->data()[pixel], 0.5f)
                << "channel=" << channel << " pixel=" << pixel;
        }
    }
    const auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(alpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        EXPECT_FLOAT_EQ(alpha->data()[pixel], inputAlpha[pixel])
            << "pixel=" << pixel;
    }
}

TEST(CreativeImageEffectContractTest, EmbossMissingRequiredColorChannelLeavesFrameUnchanged) {
    VideoFrame frame(3, 2);
    const float red[] = {0.1f, 0.4f, 0.9f, 0.3f, 0.7f, 0.2f};
    const float green[] = {0.8f, 0.3f, 0.2f, 0.6f, 0.1f, 0.5f};
    const float alpha[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    for (int pixel = 0; pixel < 6; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = red[pixel];
        frame.getChannel(ChannelType::Green)->data()[pixel] = green[pixel];
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha[pixel];
    }
    frame.removeChannel(ChannelType::Blue);

    EmbossEffect effect;
    effect.setParameter("Strength", 2.0f);
    effect.setParameter("Height", 4.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int pixel = 0; pixel < 6; ++pixel) {
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[pixel], red[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel], green[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[pixel], alpha[pixel]);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Blue));
}

TEST(CreativeImageEffectContractTest, OldTvChangesRgbWithoutChangingAlpha) {
    constexpr int width = 5;
    constexpr int height = 4;
    constexpr int pixelCount = width * height;
    VideoFrame frame(width, height);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);

    float originalRed[pixelCount]{};
    float originalGreen[pixelCount]{};
    float originalBlue[pixelCount]{};
    float originalAlpha[pixelCount]{};
    for (int pixel = 0; pixel < pixelCount; ++pixel) {
        const float position = static_cast<float>(pixel) /
                               static_cast<float>(pixelCount - 1);
        red->data()[pixel] = 0.15f + position * 0.55f;
        green->data()[pixel] = 0.75f - position * 0.45f;
        blue->data()[pixel] = 0.25f + position * 0.35f;
        alpha->data()[pixel] = 0.1f + position * 0.8f;
        originalRed[pixel] = red->data()[pixel];
        originalGreen[pixel] = green->data()[pixel];
        originalBlue[pixel] = blue->data()[pixel];
        originalAlpha[pixel] = alpha->data()[pixel];
    }

    OldTVEffect effect;
    effect.process(frame, CreativeEffectContext{.time = 0.75});

    bool changedRgb = false;
    for (int pixel = 0; pixel < pixelCount; ++pixel) {
        const float rgb[] = {red->data()[pixel], green->data()[pixel],
                             blue->data()[pixel]};
        const float originalRgb[] = {originalRed[pixel], originalGreen[pixel],
                                     originalBlue[pixel]};
        for (int channel = 0; channel < 3; ++channel) {
            EXPECT_TRUE(std::isfinite(rgb[channel])) << "pixel=" << pixel;
            changedRgb = changedRgb ||
                std::abs(rgb[channel] - originalRgb[channel]) > 1.0e-6f;
        }
        EXPECT_FLOAT_EQ(alpha->data()[pixel], originalAlpha[pixel])
            << "pixel=" << pixel;
    }
    EXPECT_TRUE(changedRgb);
}

TEST(CreativeImageEffectContractTest, OldTvHandlesSingleColumnFrames) {
    constexpr int width = 1;
    constexpr int height = 5;
    VideoFrame frame(width, height);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);

    float originalAlpha[height]{};
    for (int y = 0; y < height; ++y) {
        const float position = static_cast<float>(y) / (height - 1);
        red->data()[y] = 0.2f + position * 0.5f;
        green->data()[y] = 0.8f - position * 0.4f;
        blue->data()[y] = 0.1f + position * 0.7f;
        alpha->data()[y] = 0.15f + position * 0.7f;
        originalAlpha[y] = alpha->data()[y];
    }

    OldTVEffect effect;
    effect.process(frame, CreativeEffectContext{.time = 1.25});

    for (int y = 0; y < height; ++y) {
        EXPECT_TRUE(std::isfinite(red->data()[y])) << "row=" << y;
        EXPECT_TRUE(std::isfinite(green->data()[y])) << "row=" << y;
        EXPECT_TRUE(std::isfinite(blue->data()[y])) << "row=" << y;
        EXPECT_FLOAT_EQ(alpha->data()[y], originalAlpha[y]) << "row=" << y;
    }
}

TEST(CreativeImageEffectContractTest, OldTvIsRepeatableForTheSameFrameAndTime) {
    constexpr int width = 4;
    constexpr int height = 3;
    constexpr int pixelCount = width * height;
    VideoFrame first(width, height);
    VideoFrame second(width, height);
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    float original[pixelCount][4]{};
    for (int pixel = 0; pixel < pixelCount; ++pixel) {
        const float position = static_cast<float>(pixel) /
                               static_cast<float>(pixelCount - 1);
        original[pixel][0] = 0.1f + position * 0.7f;
        original[pixel][1] = 0.8f - position * 0.6f;
        original[pixel][2] = 0.2f + position * 0.5f;
        original[pixel][3] = 0.15f + position * 0.75f;
        for (int channel = 0; channel < 4; ++channel) {
            auto firstPlane = first.getChannel(channels[channel]);
            auto secondPlane = second.getChannel(channels[channel]);
            ASSERT_TRUE(firstPlane);
            ASSERT_TRUE(secondPlane);
            firstPlane->data()[pixel] = original[pixel][channel];
            secondPlane->data()[pixel] = original[pixel][channel];
        }
    }

    OldTVEffect firstEffect;
    OldTVEffect secondEffect;
    const CreativeEffectContext context{.time = 2.5};
    firstEffect.process(first, context);
    secondEffect.process(second, context);

    for (ChannelType channel : channels) {
        const auto firstPlane = first.getChannel(channel);
        const auto secondPlane = second.getChannel(channel);
        ASSERT_TRUE(firstPlane);
        ASSERT_TRUE(secondPlane);
        for (int pixel = 0; pixel < pixelCount; ++pixel) {
            EXPECT_FLOAT_EQ(firstPlane->data()[pixel], secondPlane->data()[pixel])
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
        }
    }
}

TEST(CreativeImageEffectContractTest, OldTvScanlineParameterModulatesRows) {
    constexpr int width = 2;
    constexpr int height = 4;
    VideoFrame frame(width, height);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        red->data()[pixel] = 0.5f;
        green->data()[pixel] = 0.5f;
        blue->data()[pixel] = 0.5f;
        alpha->data()[pixel] = 0.4f;
    }

    OldTVEffect effect;
    effect.setParameter("Curvature", 0.0f);
    effect.setParameter("Flicker", 0.0f);
    effect.setParameter("Fringe", 0.0f);
    effect.setParameter("Scanline", 1.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int x = 0; x < width; ++x) {
        const int firstRowPixel = x;
        const int secondRowPixel = width + x;
        EXPECT_FLOAT_EQ(red->data()[firstRowPixel], 0.5f);
        EXPECT_FLOAT_EQ(green->data()[firstRowPixel], 0.5f);
        EXPECT_FLOAT_EQ(blue->data()[firstRowPixel], 0.5f);
        const float expectedSecondRow =
            0.5f * (1.0f - std::abs(std::sin(2.0f)) * 0.2f);
        EXPECT_NEAR(red->data()[secondRowPixel], expectedSecondRow, 1.0e-6f);
        EXPECT_NEAR(green->data()[secondRowPixel], expectedSecondRow, 1.0e-6f);
        EXPECT_NEAR(blue->data()[secondRowPixel], expectedSecondRow, 1.0e-6f);
        EXPECT_FLOAT_EQ(alpha->data()[firstRowPixel], 0.4f);
        EXPECT_FLOAT_EQ(alpha->data()[secondRowPixel], 0.4f);
    }
}

TEST(CreativeImageEffectContractTest, OldTvFringeOffsetsRedAndBlueInOppositeDirections) {
    constexpr int width = 5;
    constexpr int height = 1;
    VideoFrame frame(width, height);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int x = 0; x < width; ++x) {
        const float value = static_cast<float>(x) / (width - 1);
        red->data()[x] = value;
        green->data()[x] = value;
        blue->data()[x] = value;
        alpha->data()[x] = 0.3f;
    }

    OldTVEffect effect;
    effect.setParameter("Scanline", 0.0f);
    effect.setParameter("Curvature", 0.0f);
    effect.setParameter("Flicker", 0.0f);
    effect.setParameter("Fringe", 0.2f);
    effect.process(frame, CreativeEffectContext{});

    EXPECT_FLOAT_EQ(red->data()[0], 0.0f);
    EXPECT_FLOAT_EQ(green->data()[0], 0.0f);
    EXPECT_FLOAT_EQ(blue->data()[0], 0.25f);
    EXPECT_FLOAT_EQ(red->data()[4], 0.75f);
    EXPECT_FLOAT_EQ(green->data()[4], 1.0f);
    EXPECT_FLOAT_EQ(blue->data()[4], 1.0f);
    for (int x = 0; x < width; ++x) {
        EXPECT_FLOAT_EQ(alpha->data()[x], 0.3f) << "x=" << x;
    }
}

TEST(CreativeImageEffectContractTest, OldTvCurvatureMovesTheVerticalSampleAtTheEdge) {
    constexpr int width = 5;
    constexpr int height = 4;
    VideoFrame frame(width, height);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int y = 0; y < height; ++y) {
        const float value = static_cast<float>(y) / (height - 1);
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            red->data()[pixel] = value;
            green->data()[pixel] = value;
            blue->data()[pixel] = value;
            alpha->data()[pixel] = 0.6f;
        }
    }

    OldTVEffect effect;
    effect.setParameter("Scanline", 0.0f);
    effect.setParameter("Curvature", 0.5f);
    effect.setParameter("Flicker", 0.0f);
    effect.setParameter("Fringe", 0.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int x = 0; x < width; ++x) {
        const int rowOnePixel = width + x;
        const int middlePixel = 2 * width + x;
        EXPECT_FLOAT_EQ(red->data()[rowOnePixel], 0.0f);
        EXPECT_FLOAT_EQ(green->data()[rowOnePixel], 0.0f);
        EXPECT_FLOAT_EQ(blue->data()[rowOnePixel], 0.0f);
        EXPECT_NEAR(red->data()[middlePixel], 2.0f / 3.0f, 1.0e-6f);
        EXPECT_NEAR(green->data()[middlePixel], 2.0f / 3.0f, 1.0e-6f);
        EXPECT_NEAR(blue->data()[middlePixel], 2.0f / 3.0f, 1.0e-6f);
        EXPECT_FLOAT_EQ(alpha->data()[rowOnePixel], 0.6f);
        EXPECT_FLOAT_EQ(alpha->data()[middlePixel], 0.6f);
    }
}

TEST(CreativeImageEffectContractTest, OldTvFlickerParameterAddsDeterministicNoise) {
    constexpr int width = 4;
    constexpr int height = 3;
    constexpr int pixelCount = width * height;
    VideoFrame noFlicker(width, height);
    VideoFrame withFlicker(width, height);
    VideoFrame sameTimeRepeat(width, height);
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    for (ChannelType channel : channels) {
        auto noFlickerPlane = noFlicker.getChannel(channel);
        auto withFlickerPlane = withFlicker.getChannel(channel);
        auto repeatPlane = sameTimeRepeat.getChannel(channel);
        ASSERT_TRUE(noFlickerPlane);
        ASSERT_TRUE(withFlickerPlane);
        ASSERT_TRUE(repeatPlane);
        for (int pixel = 0; pixel < pixelCount; ++pixel) {
            const float value = channel == ChannelType::Alpha ? 0.4f : 0.5f;
            noFlickerPlane->data()[pixel] = value;
            withFlickerPlane->data()[pixel] = value;
            repeatPlane->data()[pixel] = value;
        }
    }

    OldTVEffect noFlickerEffect;
    noFlickerEffect.setParameter("Scanline", 0.0f);
    noFlickerEffect.setParameter("Curvature", 0.0f);
    noFlickerEffect.setParameter("Fringe", 0.0f);
    noFlickerEffect.setParameter("Flicker", 0.0f);
    OldTVEffect withFlickerEffect;
    withFlickerEffect.setParameter("Scanline", 0.0f);
    withFlickerEffect.setParameter("Curvature", 0.0f);
    withFlickerEffect.setParameter("Fringe", 0.0f);
    withFlickerEffect.setParameter("Flicker", 0.5f);
    const CreativeEffectContext context{};
    noFlickerEffect.process(noFlicker, context);
    withFlickerEffect.process(withFlicker, context);
    withFlickerEffect.process(sameTimeRepeat, context);

    bool changedRgb = false;
    for (ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                ChannelType::Blue}) {
        const auto expected = noFlicker.getChannel(channel);
        const auto actual = withFlicker.getChannel(channel);
        const auto repeated = sameTimeRepeat.getChannel(channel);
        ASSERT_TRUE(expected);
        ASSERT_TRUE(actual);
        ASSERT_TRUE(repeated);
        for (int pixel = 0; pixel < pixelCount; ++pixel) {
            EXPECT_TRUE(std::isfinite(actual->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            EXPECT_FLOAT_EQ(actual->data()[pixel], repeated->data()[pixel])
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            changedRgb = changedRgb ||
                std::abs(actual->data()[pixel] - expected->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(changedRgb);
    const auto noFlickerAlpha = noFlicker.getChannel(ChannelType::Alpha);
    const auto withFlickerAlpha = withFlicker.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(noFlickerAlpha);
    ASSERT_TRUE(withFlickerAlpha);
    for (int pixel = 0; pixel < pixelCount; ++pixel) {
        EXPECT_FLOAT_EQ(noFlickerAlpha->data()[pixel], 0.4f);
        EXPECT_FLOAT_EQ(withFlickerAlpha->data()[pixel], 0.4f);
    }
}

TEST(CreativeImageEffectContractTest, OldTvFlickerChangesAcrossTimesAndKeepsAlpha) {
    constexpr int width = 8;
    constexpr int height = 5;
    VideoFrame firstFrame(width, height);
    VideoFrame laterFrame(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float red = static_cast<float>(x) / (width - 1);
            const float green = static_cast<float>(y) / (height - 1);
            const float blue = static_cast<float>((x + y) % width) / (width - 1);
            firstFrame.getChannel(ChannelType::Red)->data()[pixel] = red;
            firstFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
            firstFrame.getChannel(ChannelType::Blue)->data()[pixel] = blue;
            laterFrame.getChannel(ChannelType::Red)->data()[pixel] = red;
            laterFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
            laterFrame.getChannel(ChannelType::Blue)->data()[pixel] = blue;
            const float alpha = static_cast<float>(pixel) / (width * height - 1);
            firstFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
            laterFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        }
    }

    OldTVEffect firstEffect;
    firstEffect.setParameter("Scanline", 0.0f);
    firstEffect.setParameter("Curvature", 0.0f);
    firstEffect.setParameter("Flicker", 0.5f);
    firstEffect.setParameter("Fringe", 0.0f);
    OldTVEffect laterEffect;
    laterEffect.setParameter("Scanline", 0.0f);
    laterEffect.setParameter("Curvature", 0.0f);
    laterEffect.setParameter("Flicker", 0.5f);
    laterEffect.setParameter("Fringe", 0.0f);
    firstEffect.process(firstFrame, CreativeEffectContext{.time = 0.25});
    laterEffect.process(laterFrame, CreativeEffectContext{.time = 0.75});

    bool timeChangedRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto first = firstFrame.getChannel(channel);
        const auto later = laterFrame.getChannel(channel);
        ASSERT_TRUE(first);
        ASSERT_TRUE(later);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(later->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            timeChangedRgb = timeChangedRgb ||
                std::abs(later->data()[pixel] - first->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(timeChangedRgb);

    const auto firstAlpha = firstFrame.getChannel(ChannelType::Alpha);
    const auto laterAlpha = laterFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(firstAlpha);
    ASSERT_TRUE(laterAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(firstAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(laterAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, OldTvRgbOutputDoesNotDependOnOptionalAlphaPlane) {
    constexpr int width = 4;
    constexpr int height = 3;
    VideoFrame withAlpha(width, height);
    VideoFrame withoutAlpha(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float red = static_cast<float>((x * 3 + y) % 7) / 7.0f;
            const float green = static_cast<float>((x + y * 2) % 6) / 6.0f;
            const float blue = static_cast<float>((x * 2 + y * 3) % 8) / 8.0f;
            for (VideoFrame* frame : {&withAlpha, &withoutAlpha}) {
                frame->getChannel(ChannelType::Red)->data()[pixel] = red;
                frame->getChannel(ChannelType::Green)->data()[pixel] = green;
                frame->getChannel(ChannelType::Blue)->data()[pixel] = blue;
            }
            withAlpha.getChannel(ChannelType::Alpha)->data()[pixel] =
                static_cast<float>(pixel) / (width * height - 1);
        }
    }
    withoutAlpha.removeChannel(ChannelType::Alpha);

    OldTVEffect withAlphaEffect;
    withAlphaEffect.setParameter("Scanline", 0.8f);
    withAlphaEffect.setParameter("Curvature", 0.4f);
    withAlphaEffect.setParameter("Flicker", 0.3f);
    withAlphaEffect.setParameter("Fringe", 0.7f);
    OldTVEffect withoutAlphaEffect;
    withoutAlphaEffect.setParameter("Scanline", 0.8f);
    withoutAlphaEffect.setParameter("Curvature", 0.4f);
    withoutAlphaEffect.setParameter("Flicker", 0.3f);
    withoutAlphaEffect.setParameter("Fringe", 0.7f);
    const CreativeEffectContext context{.time = 1.75};
    withAlphaEffect.process(withAlpha, context);
    withoutAlphaEffect.process(withoutAlpha, context);

    bool changedRgb = false;
    for (const ChannelType channelType : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
        const auto expected = withAlpha.getChannel(channelType);
        const auto actual = withoutAlpha.getChannel(channelType);
        ASSERT_TRUE(expected);
        ASSERT_TRUE(actual);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(actual->data()[pixel]));
            EXPECT_FLOAT_EQ(actual->data()[pixel], expected->data()[pixel]);
            const int x = pixel % width;
            const int y = pixel / width;
            const float original = channelType == ChannelType::Red
                ? static_cast<float>((x * 3 + y) % 7) / 7.0f
                : channelType == ChannelType::Green
                    ? static_cast<float>((x + y * 2) % 6) / 6.0f
                    : static_cast<float>((x * 2 + y * 3) % 8) / 8.0f;
            changedRgb = changedRgb ||
                std::abs(actual->data()[pixel] - original) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(changedRgb);
    EXPECT_FALSE(withoutAlpha.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, OldTvNeutralParametersProduceExactNoOp) {
    const float inputRed[] = {0.1f, 0.3f, 0.6f, 0.9f};
    const float inputGreen[] = {0.9f, 0.7f, 0.4f, 0.1f};
    const float inputBlue[] = {0.2f, 0.4f, 0.6f, 0.8f};
    const float inputAlpha[] = {0.0f, 0.2f, 0.4f, 0.6f};
    VideoFrame frame(2, 2);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int index = 0; index < 4; ++index) {
        red->data()[index] = inputRed[index];
        green->data()[index] = inputGreen[index];
        blue->data()[index] = inputBlue[index];
        alpha->data()[index] = inputAlpha[index];
    }

    OldTVEffect oldTv;
    oldTv.setParameter("Scanline", 0.0f);
    oldTv.setParameter("Curvature", 0.0f);
    oldTv.setParameter("Flicker", 0.0f);
    oldTv.setParameter("Fringe", 0.0f);
    oldTv.process(frame, CreativeEffectContext{});

    for (int index = 0; index < 4; ++index) {
        EXPECT_FLOAT_EQ(red->data()[index], inputRed[index]) << "red pixel=" << index;
        EXPECT_FLOAT_EQ(green->data()[index], inputGreen[index])
            << "green pixel=" << index;
        EXPECT_FLOAT_EQ(blue->data()[index], inputBlue[index])
            << "blue pixel=" << index;
        EXPECT_FLOAT_EQ(alpha->data()[index], inputAlpha[index])
            << "alpha pixel=" << index;
    }
}

TEST(CreativeImageEffectContractTest, OldTvMissingRequiredColorChannelLeavesFrameUnchanged) {
    VideoFrame frame(3, 2);
    const float red[] = {0.1f, 0.4f, 0.9f, 0.3f, 0.7f, 0.2f};
    const float green[] = {0.8f, 0.3f, 0.2f, 0.6f, 0.1f, 0.5f};
    const float alpha[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    for (int pixel = 0; pixel < 6; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = red[pixel];
        frame.getChannel(ChannelType::Green)->data()[pixel] = green[pixel];
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha[pixel];
    }
    frame.removeChannel(ChannelType::Blue);

    OldTVEffect effect;
    effect.setParameter("Scanline", 1.0f);
    effect.setParameter("Curvature", 0.5f);
    effect.setParameter("Flicker", 0.5f);
    effect.setParameter("Fringe", 1.0f);
    effect.process(frame, CreativeEffectContext{.time = 12.0});

    for (int pixel = 0; pixel < 6; ++pixel) {
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[pixel], red[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel], green[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[pixel], alpha[pixel]);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Blue));
}

TEST(CreativeImageEffectContractTest, EdgeEchoZeroIntensityAndTintPreserveInput) {
    const float inputRed[] = {0.1f, 0.3f, 0.6f, 0.9f};
    const float inputGreen[] = {0.9f, 0.7f, 0.4f, 0.1f};
    const float inputBlue[] = {0.2f, 0.4f, 0.6f, 0.8f};
    const float inputAlpha[] = {0.0f, 0.2f, 0.4f, 0.6f};
    VideoFrame frame(2, 2);
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    const float* inputs[] = {inputRed, inputGreen, inputBlue, inputAlpha};
    for (int channel = 0; channel < 4; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < 4; ++pixel) {
            plane->data()[pixel] = inputs[channel][pixel];
        }
    }

    EdgeEchoEffect effect;
    effect.setParameter("Intensity", 0.0f);
    effect.setParameter("Tint", 0.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int channel = 0; channel < 4; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < 4; ++pixel) {
            EXPECT_FLOAT_EQ(plane->data()[pixel], inputs[channel][pixel])
                << "channel=" << channel << " pixel=" << pixel;
        }
    }
}

TEST(CreativeImageEffectContractTest, EdgeEchoActiveSettingsChangeFiniteRgbAndKeepAlpha) {
    constexpr int width = 4;
    constexpr int height = 2;
    VideoFrame frame(width, height);
    const float inputRed[] = {0.1f, 0.2f, 0.8f, 0.9f,
                              0.1f, 0.2f, 0.8f, 0.9f};
    const float inputGreen[] = {0.8f, 0.7f, 0.2f, 0.1f,
                                0.8f, 0.7f, 0.2f, 0.1f};
    const float inputBlue[] = {0.2f, 0.3f, 0.7f, 0.8f,
                               0.2f, 0.3f, 0.7f, 0.8f};
    const float inputAlpha[] = {0.1f, 0.2f, 0.3f, 0.4f,
                                0.5f, 0.6f, 0.7f, 0.8f};
    const float* inputs[] = {inputRed, inputGreen, inputBlue, inputAlpha};
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    for (int channel = 0; channel < 4; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            plane->data()[pixel] = inputs[channel][pixel];
        }
    }

    EdgeEchoEffect effect;
    effect.setParameter("Intensity", 0.8f);
    effect.setParameter("Thickness", 1.0f);
    effect.setParameter("EchoCount", 2.0f);
    effect.setParameter("Tint", 0.5f);
    effect.process(frame, CreativeEffectContext{});

    bool changedRgb = false;
    for (int channel = 0; channel < 3; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(plane->data()[pixel]));
            changedRgb = changedRgb ||
                std::abs(plane->data()[pixel] - inputs[channel][pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(changedRgb);
    const auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(alpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        EXPECT_FLOAT_EQ(alpha->data()[pixel], inputAlpha[pixel])
            << "pixel=" << pixel;
    }
}

TEST(CreativeImageEffectContractTest, EdgeEchoRgbOutputDoesNotDependOnOptionalAlphaPlane) {
    constexpr int width = 4;
    constexpr int height = 2;
    VideoFrame withAlpha(width, height);
    VideoFrame withoutAlpha(width, height);
    const float inputRed[] = {0.1f, 0.2f, 0.8f, 0.9f,
                              0.1f, 0.2f, 0.8f, 0.9f};
    const float inputGreen[] = {0.8f, 0.7f, 0.2f, 0.1f,
                                0.8f, 0.7f, 0.2f, 0.1f};
    const float inputBlue[] = {0.2f, 0.3f, 0.7f, 0.8f,
                               0.2f, 0.3f, 0.7f, 0.8f};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        for (VideoFrame* frame : {&withAlpha, &withoutAlpha}) {
            frame->getChannel(ChannelType::Red)->data()[pixel] = inputRed[pixel];
            frame->getChannel(ChannelType::Green)->data()[pixel] = inputGreen[pixel];
            frame->getChannel(ChannelType::Blue)->data()[pixel] = inputBlue[pixel];
        }
        withAlpha.getChannel(ChannelType::Alpha)->data()[pixel] =
            static_cast<float>(pixel) / (width * height - 1);
    }
    withoutAlpha.removeChannel(ChannelType::Alpha);

    EdgeEchoEffect withAlphaEffect;
    withAlphaEffect.setParameter("Intensity", 0.8f);
    withAlphaEffect.setParameter("Thickness", 1.0f);
    withAlphaEffect.setParameter("EchoCount", 2.0f);
    withAlphaEffect.setParameter("Tint", 0.5f);
    EdgeEchoEffect withoutAlphaEffect;
    withoutAlphaEffect.setParameter("Intensity", 0.8f);
    withoutAlphaEffect.setParameter("Thickness", 1.0f);
    withoutAlphaEffect.setParameter("EchoCount", 2.0f);
    withoutAlphaEffect.setParameter("Tint", 0.5f);
    withAlphaEffect.process(withAlpha, CreativeEffectContext{});
    withoutAlphaEffect.process(withoutAlpha, CreativeEffectContext{});

    bool changedRgb = false;
    for (const ChannelType channelType : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
        const auto expected = withAlpha.getChannel(channelType);
        const auto actual = withoutAlpha.getChannel(channelType);
        ASSERT_TRUE(expected);
        ASSERT_TRUE(actual);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(actual->data()[pixel]));
            EXPECT_FLOAT_EQ(actual->data()[pixel], expected->data()[pixel]);
            const float original = channelType == ChannelType::Red ? inputRed[pixel]
                : channelType == ChannelType::Green ? inputGreen[pixel]
                                                    : inputBlue[pixel];
            changedRgb = changedRgb ||
                std::abs(actual->data()[pixel] - original) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(changedRgb);
    EXPECT_FALSE(withoutAlpha.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, EdgeEchoThicknessChangesEchoSamplingDistance) {
    constexpr int width = 12;
    constexpr int height = 8;
    VideoFrame thinEchoFrame(width, height);
    VideoFrame thickEchoFrame(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float base = x < width / 2 ? 0.15f : 0.85f;
            thinEchoFrame.getChannel(ChannelType::Red)->data()[pixel] = base;
            thinEchoFrame.getChannel(ChannelType::Green)->data()[pixel] =
                std::clamp(base + static_cast<float>(y) * 0.01f, 0.0f, 1.0f);
            thinEchoFrame.getChannel(ChannelType::Blue)->data()[pixel] = 1.0f - base;
            thickEchoFrame.getChannel(ChannelType::Red)->data()[pixel] = base;
            thickEchoFrame.getChannel(ChannelType::Green)->data()[pixel] =
                std::clamp(base + static_cast<float>(y) * 0.01f, 0.0f, 1.0f);
            thickEchoFrame.getChannel(ChannelType::Blue)->data()[pixel] = 1.0f - base;
            const float alpha = static_cast<float>(pixel) / (width * height - 1);
            thinEchoFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
            thickEchoFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        }
    }

    EdgeEchoEffect thinEcho;
    thinEcho.setParameter("Intensity", 0.9f);
    thinEcho.setParameter("Thickness", 0.5f);
    thinEcho.setParameter("EchoCount", 4.0f);
    thinEcho.setParameter("Tint", 0.2f);
    EdgeEchoEffect thickEcho;
    thickEcho.setParameter("Intensity", 0.9f);
    thickEcho.setParameter("Thickness", 8.0f);
    thickEcho.setParameter("EchoCount", 4.0f);
    thickEcho.setParameter("Tint", 0.2f);
    thinEcho.process(thinEchoFrame, CreativeEffectContext{});
    thickEcho.process(thickEchoFrame, CreativeEffectContext{});

    bool thicknessChangedRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto thin = thinEchoFrame.getChannel(channel);
        const auto thick = thickEchoFrame.getChannel(channel);
        ASSERT_TRUE(thin);
        ASSERT_TRUE(thick);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(thick->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            thicknessChangedRgb = thicknessChangedRgb ||
                std::abs(thick->data()[pixel] - thin->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(thicknessChangedRgb);

    const auto thinAlpha = thinEchoFrame.getChannel(ChannelType::Alpha);
    const auto thickAlpha = thickEchoFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(thinAlpha);
    ASSERT_TRUE(thickAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(thinAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(thickAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, EdgeEchoCountChangesAccumulatedEchoSamples) {
    constexpr int width = 16;
    constexpr int height = 8;
    VideoFrame oneEchoFrame(width, height);
    VideoFrame sixEchoFrame(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float base = x < width / 2 ? 0.12f : 0.88f;
            const float green = std::clamp(base + static_cast<float>(y) * 0.01f,
                                           0.0f, 1.0f);
            oneEchoFrame.getChannel(ChannelType::Red)->data()[pixel] = base;
            oneEchoFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
            oneEchoFrame.getChannel(ChannelType::Blue)->data()[pixel] = 1.0f - base;
            sixEchoFrame.getChannel(ChannelType::Red)->data()[pixel] = base;
            sixEchoFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
            sixEchoFrame.getChannel(ChannelType::Blue)->data()[pixel] = 1.0f - base;
            const float alpha = static_cast<float>(pixel) / (width * height - 1);
            oneEchoFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
            sixEchoFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        }
    }

    EdgeEchoEffect oneEcho;
    oneEcho.setParameter("Intensity", 0.85f);
    oneEcho.setParameter("Thickness", 1.5f);
    oneEcho.setParameter("EchoCount", 1.0f);
    oneEcho.setParameter("Tint", 0.25f);
    EdgeEchoEffect sixEcho;
    sixEcho.setParameter("Intensity", 0.85f);
    sixEcho.setParameter("Thickness", 1.5f);
    sixEcho.setParameter("EchoCount", 6.0f);
    sixEcho.setParameter("Tint", 0.25f);
    oneEcho.process(oneEchoFrame, CreativeEffectContext{});
    sixEcho.process(sixEchoFrame, CreativeEffectContext{});

    bool countChangedRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto one = oneEchoFrame.getChannel(channel);
        const auto six = sixEchoFrame.getChannel(channel);
        ASSERT_TRUE(one);
        ASSERT_TRUE(six);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(six->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            countChangedRgb = countChangedRgb ||
                std::abs(six->data()[pixel] - one->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(countChangedRgb);

    const auto oneAlpha = oneEchoFrame.getChannel(ChannelType::Alpha);
    const auto sixAlpha = sixEchoFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(oneAlpha);
    ASSERT_TRUE(sixAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(oneAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(sixAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, EdgeEchoTintAddsEdgeColorContribution) {
    constexpr int width = 10;
    constexpr int height = 6;
    VideoFrame untintedFrame(width, height);
    VideoFrame tintedFrame(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float value = x < width / 2 ? 0.2f : 0.65f;
            const float red = value;
            const float green = std::min(value + 0.12f, 1.0f);
            const float blue = std::max(value - 0.1f, 0.0f);
            untintedFrame.getChannel(ChannelType::Red)->data()[pixel] = red;
            untintedFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
            untintedFrame.getChannel(ChannelType::Blue)->data()[pixel] = blue;
            tintedFrame.getChannel(ChannelType::Red)->data()[pixel] = red;
            tintedFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
            tintedFrame.getChannel(ChannelType::Blue)->data()[pixel] = blue;
            const float alpha = static_cast<float>(pixel) / (width * height - 1);
            untintedFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
            tintedFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        }
    }

    EdgeEchoEffect untinted;
    untinted.setParameter("Intensity", 0.8f);
    untinted.setParameter("Thickness", 1.0f);
    untinted.setParameter("EchoCount", 3.0f);
    untinted.setParameter("Tint", 0.0f);
    EdgeEchoEffect tinted;
    tinted.setParameter("Intensity", 0.8f);
    tinted.setParameter("Thickness", 1.0f);
    tinted.setParameter("EchoCount", 3.0f);
    tinted.setParameter("Tint", 1.0f);
    untinted.process(untintedFrame, CreativeEffectContext{});
    tinted.process(tintedFrame, CreativeEffectContext{});

    bool tintChangedEdgeRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto noTint = untintedFrame.getChannel(channel);
        const auto withTint = tintedFrame.getChannel(channel);
        ASSERT_TRUE(noTint);
        ASSERT_TRUE(withTint);
        for (int y = 0; y < height; ++y) {
            for (int x = width / 2 - 1; x <= width / 2; ++x) {
                const int pixel = y * width + x;
                EXPECT_TRUE(std::isfinite(withTint->data()[pixel]))
                    << "channel=" << static_cast<int>(channel)
                    << " x=" << x << " y=" << y;
                tintChangedEdgeRgb = tintChangedEdgeRgb ||
                    std::abs(withTint->data()[pixel] - noTint->data()[pixel]) > 1.0e-6f;
            }
        }
    }
    EXPECT_TRUE(tintChangedEdgeRgb);

    const auto untintedAlpha = untintedFrame.getChannel(ChannelType::Alpha);
    const auto tintedAlpha = tintedFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(untintedAlpha);
    ASSERT_TRUE(tintedAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(untintedAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(tintedAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, EdgeEchoMissingRequiredColorChannelLeavesFrameUnchanged) {
    VideoFrame frame(3, 2);
    const float red[] = {0.1f, 0.4f, 0.9f, 0.3f, 0.7f, 0.2f};
    const float green[] = {0.8f, 0.3f, 0.2f, 0.6f, 0.1f, 0.5f};
    const float alpha[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    for (int pixel = 0; pixel < 6; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = red[pixel];
        frame.getChannel(ChannelType::Green)->data()[pixel] = green[pixel];
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha[pixel];
    }
    frame.removeChannel(ChannelType::Blue);

    EdgeEchoEffect effect;
    effect.setParameter("Intensity", 1.0f);
    effect.setParameter("Thickness", 8.0f);
    effect.setParameter("EchoCount", 6.0f);
    effect.setParameter("Tint", 1.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int pixel = 0; pixel < 6; ++pixel) {
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[pixel], red[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel], green[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[pixel], alpha[pixel]);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Blue));
}

TEST(CreativeImageEffectContractTest, LightPressureZeroControlsPreserveInput) {
    const float inputRed[] = {0.1f, 0.3f, 0.6f, 0.9f};
    const float inputGreen[] = {0.9f, 0.7f, 0.4f, 0.1f};
    const float inputBlue[] = {0.2f, 0.4f, 0.6f, 0.8f};
    const float inputAlpha[] = {0.0f, 0.2f, 0.4f, 0.6f};
    VideoFrame frame(2, 2);
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    const float* inputs[] = {inputRed, inputGreen, inputBlue, inputAlpha};
    for (int channel = 0; channel < 4; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < 4; ++pixel) {
            plane->data()[pixel] = inputs[channel][pixel];
        }
    }

    LightPressureEffect effect;
    effect.setParameter("Pressure", 0.0f);
    effect.setParameter("Bloom", 0.0f);
    effect.setParameter("Spread", 0.0f);
    effect.setParameter("Compression", 0.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int channel = 0; channel < 4; ++channel) {
        const auto plane = frame.getChannel(channels[channel]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < 4; ++pixel) {
            EXPECT_FLOAT_EQ(plane->data()[pixel], inputs[channel][pixel])
                << "channel=" << channel << " pixel=" << pixel;
        }
    }
}

TEST(CreativeImageEffectContractTest, LightPressureActiveSettingsChangeFiniteRgbAndKeepAlpha) {
    constexpr int width = 3;
    constexpr int height = 2;
    VideoFrame frame(width, height);
    const float alphaValues[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = 0.5f;
        frame.getChannel(ChannelType::Green)->data()[pixel] = 0.5f;
        frame.getChannel(ChannelType::Blue)->data()[pixel] = 0.5f;
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alphaValues[pixel];
    }

    LightPressureEffect effect;
    effect.setParameter("Pressure", 0.8f);
    effect.setParameter("Bloom", 0.6f);
    effect.setParameter("Spread", 0.5f);
    effect.setParameter("Compression", 0.0f);
    effect.process(frame, CreativeEffectContext{.time = 0.0});

    bool changedRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto plane = frame.getChannel(channel);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(plane->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            changedRgb = changedRgb ||
                std::abs(plane->data()[pixel] - 0.5f) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(changedRgb);
    const auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(alpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        EXPECT_FLOAT_EQ(alpha->data()[pixel], alphaValues[pixel])
            << "pixel=" << pixel;
    }
}

TEST(CreativeImageEffectContractTest, LightPressureRgbOutputDoesNotDependOnOptionalAlphaPlane) {
    constexpr int width = 3;
    constexpr int height = 2;
    VideoFrame withAlpha(width, height);
    VideoFrame withoutAlpha(width, height);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        for (VideoFrame* frame : {&withAlpha, &withoutAlpha}) {
            frame->getChannel(ChannelType::Red)->data()[pixel] = 0.5f;
            frame->getChannel(ChannelType::Green)->data()[pixel] = 0.5f;
            frame->getChannel(ChannelType::Blue)->data()[pixel] = 0.5f;
        }
        withAlpha.getChannel(ChannelType::Alpha)->data()[pixel] =
            static_cast<float>(pixel) / (width * height - 1);
    }
    withoutAlpha.removeChannel(ChannelType::Alpha);

    LightPressureEffect withAlphaEffect;
    withAlphaEffect.setParameter("Pressure", 0.8f);
    withAlphaEffect.setParameter("Bloom", 0.6f);
    withAlphaEffect.setParameter("Spread", 0.5f);
    withAlphaEffect.setParameter("Compression", 0.0f);
    LightPressureEffect withoutAlphaEffect;
    withoutAlphaEffect.setParameter("Pressure", 0.8f);
    withoutAlphaEffect.setParameter("Bloom", 0.6f);
    withoutAlphaEffect.setParameter("Spread", 0.5f);
    withoutAlphaEffect.setParameter("Compression", 0.0f);
    const CreativeEffectContext context{.time = 0.0};
    withAlphaEffect.process(withAlpha, context);
    withoutAlphaEffect.process(withoutAlpha, context);

    bool changedRgb = false;
    for (const ChannelType channelType : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
        const auto expected = withAlpha.getChannel(channelType);
        const auto actual = withoutAlpha.getChannel(channelType);
        ASSERT_TRUE(expected);
        ASSERT_TRUE(actual);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(actual->data()[pixel]));
            EXPECT_FLOAT_EQ(actual->data()[pixel], expected->data()[pixel]);
            changedRgb = changedRgb ||
                std::abs(actual->data()[pixel] - 0.5f) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(changedRgb);
    EXPECT_FALSE(withoutAlpha.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, LightPressureBloomAddsHighlightContribution) {
    constexpr int width = 4;
    constexpr int height = 3;
    VideoFrame noBloomFrame(width, height);
    VideoFrame bloomFrame(width, height);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
            noBloomFrame.getChannel(channel)->data()[pixel] = 0.5f;
            bloomFrame.getChannel(channel)->data()[pixel] = 0.5f;
        }
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        noBloomFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        bloomFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
    }

    LightPressureEffect noBloomEffect;
    noBloomEffect.setParameter("Pressure", 0.8f);
    noBloomEffect.setParameter("Bloom", 0.0f);
    noBloomEffect.setParameter("Spread", 0.0f);
    noBloomEffect.setParameter("Compression", 0.0f);
    LightPressureEffect bloomEffect;
    bloomEffect.setParameter("Pressure", 0.8f);
    bloomEffect.setParameter("Bloom", 1.0f);
    bloomEffect.setParameter("Spread", 0.0f);
    bloomEffect.setParameter("Compression", 0.0f);
    const CreativeEffectContext context{.time = 0.75};
    noBloomEffect.process(noBloomFrame, context);
    bloomEffect.process(bloomFrame, context);

    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto noBloom = noBloomFrame.getChannel(channel);
        const auto bloom = bloomFrame.getChannel(channel);
        ASSERT_TRUE(noBloom);
        ASSERT_TRUE(bloom);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(bloom->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            EXPECT_GT(bloom->data()[pixel], noBloom->data()[pixel])
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
        }
    }

    const auto noBloomAlpha = noBloomFrame.getChannel(ChannelType::Alpha);
    const auto bloomAlpha = bloomFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(noBloomAlpha);
    ASSERT_TRUE(bloomAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(noBloomAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(bloomAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, LightPressureStrengthWarpsHighContrastInput) {
    constexpr int width = 10;
    constexpr int height = 6;
    VideoFrame unpressuredFrame(width, height);
    VideoFrame pressuredFrame(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float value = x < width / 2 ? 0.15f : 0.9f;
            for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                              ChannelType::Blue}) {
                unpressuredFrame.getChannel(channel)->data()[pixel] = value;
                pressuredFrame.getChannel(channel)->data()[pixel] = value;
            }
            const float alpha = static_cast<float>(pixel) / (width * height - 1);
            unpressuredFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
            pressuredFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        }
    }

    LightPressureEffect unpressured;
    unpressured.setParameter("Pressure", 0.0f);
    unpressured.setParameter("Bloom", 0.0f);
    unpressured.setParameter("Spread", 0.5f);
    unpressured.setParameter("Compression", 0.0f);
    LightPressureEffect pressured;
    pressured.setParameter("Pressure", 1.0f);
    pressured.setParameter("Bloom", 0.0f);
    pressured.setParameter("Spread", 0.5f);
    pressured.setParameter("Compression", 0.0f);
    const CreativeEffectContext context{.time = 0.75};
    unpressured.process(unpressuredFrame, context);
    pressured.process(pressuredFrame, context);

    bool pressureChangedRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto unpressuredPixels = unpressuredFrame.getChannel(channel);
        const auto pressuredPixels = pressuredFrame.getChannel(channel);
        ASSERT_TRUE(unpressuredPixels);
        ASSERT_TRUE(pressuredPixels);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(pressuredPixels->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            pressureChangedRgb = pressureChangedRgb ||
                std::abs(pressuredPixels->data()[pixel] -
                         unpressuredPixels->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(pressureChangedRgb);

    const auto unpressuredAlpha = unpressuredFrame.getChannel(ChannelType::Alpha);
    const auto pressuredAlpha = pressuredFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(unpressuredAlpha);
    ASSERT_TRUE(pressuredAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(unpressuredAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(pressuredAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, LightPressureCompressionRespondsToLuminanceGradient) {
    constexpr int width = 10;
    constexpr int height = 6;
    VideoFrame uncompressedFrame(width, height);
    VideoFrame compressedFrame(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float value = x < width / 2 ? 0.15f : 0.9f;
            for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                              ChannelType::Blue}) {
                uncompressedFrame.getChannel(channel)->data()[pixel] = value;
                compressedFrame.getChannel(channel)->data()[pixel] = value;
            }
            const float alpha = static_cast<float>(pixel) / (width * height - 1);
            uncompressedFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
            compressedFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        }
    }

    LightPressureEffect uncompressed;
    uncompressed.setParameter("Pressure", 0.0f);
    uncompressed.setParameter("Bloom", 0.0f);
    uncompressed.setParameter("Spread", 0.5f);
    uncompressed.setParameter("Compression", 0.0f);
    LightPressureEffect compressed;
    compressed.setParameter("Pressure", 0.0f);
    compressed.setParameter("Bloom", 0.0f);
    compressed.setParameter("Spread", 0.5f);
    compressed.setParameter("Compression", 1.0f);
    const CreativeEffectContext context{.time = 0.75};
    uncompressed.process(uncompressedFrame, context);
    compressed.process(compressedFrame, context);

    bool compressionChangedRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto uncompressedPixels = uncompressedFrame.getChannel(channel);
        const auto compressedPixels = compressedFrame.getChannel(channel);
        ASSERT_TRUE(uncompressedPixels);
        ASSERT_TRUE(compressedPixels);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(compressedPixels->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            compressionChangedRgb = compressionChangedRgb ||
                std::abs(compressedPixels->data()[pixel] -
                         uncompressedPixels->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(compressionChangedRgb);

    const auto uncompressedAlpha = uncompressedFrame.getChannel(ChannelType::Alpha);
    const auto compressedAlpha = compressedFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(uncompressedAlpha);
    ASSERT_TRUE(compressedAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(uncompressedAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(compressedAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, LightPressureSpreadChangesWarpDistance) {
    constexpr int width = 16;
    constexpr int height = 8;
    VideoFrame narrowSpreadFrame(width, height);
    VideoFrame broadSpreadFrame(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float value = x < width / 2 ? 0.12f : 0.88f;
            for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                              ChannelType::Blue}) {
                narrowSpreadFrame.getChannel(channel)->data()[pixel] = value;
                broadSpreadFrame.getChannel(channel)->data()[pixel] = value;
            }
            const float alpha = static_cast<float>(pixel) / (width * height - 1);
            narrowSpreadFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
            broadSpreadFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        }
    }

    LightPressureEffect narrowSpread;
    narrowSpread.setParameter("Pressure", 0.9f);
    narrowSpread.setParameter("Bloom", 0.0f);
    narrowSpread.setParameter("Spread", 0.0f);
    narrowSpread.setParameter("Compression", 0.0f);
    LightPressureEffect broadSpread;
    broadSpread.setParameter("Pressure", 0.9f);
    broadSpread.setParameter("Bloom", 0.0f);
    broadSpread.setParameter("Spread", 1.0f);
    broadSpread.setParameter("Compression", 0.0f);
    const CreativeEffectContext context{.time = 0.75};
    narrowSpread.process(narrowSpreadFrame, context);
    broadSpread.process(broadSpreadFrame, context);

    bool spreadChangedRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto narrow = narrowSpreadFrame.getChannel(channel);
        const auto broad = broadSpreadFrame.getChannel(channel);
        ASSERT_TRUE(narrow);
        ASSERT_TRUE(broad);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(broad->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            spreadChangedRgb = spreadChangedRgb ||
                std::abs(broad->data()[pixel] - narrow->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(spreadChangedRgb);

    const auto narrowAlpha = narrowSpreadFrame.getChannel(ChannelType::Alpha);
    const auto broadAlpha = broadSpreadFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(narrowAlpha);
    ASSERT_TRUE(broadAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(narrowAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(broadAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, LightPressureMissingRequiredColorChannelLeavesFrameUnchanged) {
    VideoFrame frame(3, 2);
    const float green[] = {0.8f, 0.3f, 0.2f, 0.6f, 0.1f, 0.5f};
    const float blue[] = {0.1f, 0.9f, 0.3f, 0.7f, 0.2f, 0.8f};
    const float alpha[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    for (int pixel = 0; pixel < 6; ++pixel) {
        frame.getChannel(ChannelType::Green)->data()[pixel] = green[pixel];
        frame.getChannel(ChannelType::Blue)->data()[pixel] = blue[pixel];
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha[pixel];
    }
    frame.removeChannel(ChannelType::Red);

    LightPressureEffect effect;
    effect.setParameter("Pressure", 1.0f);
    effect.setParameter("Bloom", 1.0f);
    effect.setParameter("Spread", 1.0f);
    effect.setParameter("Compression", 1.0f);
    effect.process(frame, CreativeEffectContext{});

    for (int pixel = 0; pixel < 6; ++pixel) {
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel], green[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Blue)->data()[pixel], blue[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[pixel], alpha[pixel]);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Red));
}

TEST(CreativeImageEffectContractTest, TemporalEffectsInitializeHistoryFromFirstFrameWithoutChangingPixels) {
    const float inputRed[] = {0.1f, 0.3f, 0.6f, 0.9f};
    const float inputGreen[] = {0.9f, 0.7f, 0.4f, 0.1f};
    const float inputBlue[] = {0.2f, 0.4f, 0.6f, 0.8f};
    const float inputAlpha[] = {0.0f, 0.2f, 0.4f, 0.6f};
    const CreativeEffectContext firstFrame{.time = 2.0, .frameIndex = 12};

    VideoFrame surfaceMemoryFrame(2, 2);
    VideoFrame temporalFossilFrame(2, 2);
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    const float* expectedChannels[] = {
        inputRed, inputGreen, inputBlue, inputAlpha,
    };
    for (int channelIndex = 0; channelIndex < 4; ++channelIndex) {
        auto surfacePlane = surfaceMemoryFrame.getChannel(channels[channelIndex]);
        auto fossilPlane = temporalFossilFrame.getChannel(channels[channelIndex]);
        ASSERT_TRUE(surfacePlane);
        ASSERT_TRUE(fossilPlane);
        for (int pixel = 0; pixel < 4; ++pixel) {
            surfacePlane->data()[pixel] = expectedChannels[channelIndex][pixel];
            fossilPlane->data()[pixel] = expectedChannels[channelIndex][pixel];
        }
    }

    SurfaceMemoryEffect surfaceMemory;
    surfaceMemory.process(surfaceMemoryFrame, firstFrame);
    TemporalFossilEffect temporalFossil;
    temporalFossil.process(temporalFossilFrame, firstFrame);

    for (int channelIndex = 0; channelIndex < 4; ++channelIndex) {
        const auto surfacePlane = surfaceMemoryFrame.getChannel(channels[channelIndex]);
        const auto fossilPlane = temporalFossilFrame.getChannel(channels[channelIndex]);
        ASSERT_TRUE(surfacePlane);
        ASSERT_TRUE(fossilPlane);
        for (int pixel = 0; pixel < 4; ++pixel) {
            EXPECT_FLOAT_EQ(surfacePlane->data()[pixel], expectedChannels[channelIndex][pixel])
                << "surface channel=" << channelIndex << " pixel=" << pixel;
            EXPECT_FLOAT_EQ(fossilPlane->data()[pixel], expectedChannels[channelIndex][pixel])
                << "fossil channel=" << channelIndex << " pixel=" << pixel;
        }
    }
}

TEST(CreativeImageEffectContractTest, SurfaceMemoryRetainsPriorFrameAndPreservesAlpha) {
    constexpr int width = 3;
    constexpr int height = 2;
    VideoFrame previousFrame(width, height);
    VideoFrame currentFrame(width, height);
    VideoFrame freshCurrentFrame(width, height);
    const float alphaValues[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
            previousFrame.getChannel(channel)->data()[pixel] = 0.2f;
            currentFrame.getChannel(channel)->data()[pixel] = 0.8f;
            freshCurrentFrame.getChannel(channel)->data()[pixel] = 0.8f;
        }
        previousFrame.getChannel(ChannelType::Alpha)->data()[pixel] = 0.25f;
        currentFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alphaValues[pixel];
        freshCurrentFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alphaValues[pixel];
    }

    SurfaceMemoryEffect effect;
    effect.setParameter("Retention", 1.0f);
    effect.setParameter("Refresh", 0.0f);
    effect.setParameter("Texture", 0.0f);
    effect.setParameter("Smear", 0.0f);
    effect.process(previousFrame, CreativeEffectContext{.time = 0.0, .frameIndex = 0});
    effect.process(currentFrame, CreativeEffectContext{.time = 0.5, .frameIndex = 1});

    SurfaceMemoryEffect freshEffect;
    freshEffect.setParameter("Retention", 1.0f);
    freshEffect.setParameter("Refresh", 0.0f);
    freshEffect.setParameter("Texture", 0.0f);
    freshEffect.setParameter("Smear", 0.0f);
    freshEffect.process(freshCurrentFrame,
                        CreativeEffectContext{.time = 0.5, .frameIndex = 1});

    bool differsFromFreshFrame = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto retained = currentFrame.getChannel(channel);
        const auto fresh = freshCurrentFrame.getChannel(channel);
        ASSERT_TRUE(retained);
        ASSERT_TRUE(fresh);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(retained->data()[pixel]));
            EXPECT_LT(retained->data()[pixel], 0.8f)
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            EXPECT_FLOAT_EQ(fresh->data()[pixel], 0.8f)
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            differsFromFreshFrame = differsFromFreshFrame ||
                std::abs(retained->data()[pixel] - fresh->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(differsFromFreshFrame);
    const auto alpha = currentFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(alpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        EXPECT_FLOAT_EQ(alpha->data()[pixel], alphaValues[pixel]);
    }
}

TEST(CreativeImageEffectContractTest, SurfaceMemoryRgbOutputDoesNotDependOnOptionalAlphaPlane) {
    constexpr int width = 4;
    constexpr int height = 3;
    VideoFrame previousWithAlpha(width, height);
    VideoFrame currentWithAlpha(width, height);
    VideoFrame previousWithoutAlpha(width, height);
    VideoFrame currentWithoutAlpha(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float previousRed = 0.15f + static_cast<float>(x) * 0.08f;
            const float previousGreen = 0.7f - static_cast<float>(y) * 0.12f;
            const float previousBlue = 0.25f + static_cast<float>((x + y) % 4) * 0.1f;
            const float currentRed = 0.8f - static_cast<float>(x) * 0.06f;
            const float currentGreen = 0.2f + static_cast<float>(y) * 0.15f;
            const float currentBlue = 0.3f + static_cast<float>((2 * x + y) % 5) * 0.09f;
            for (VideoFrame* frame : {&previousWithAlpha, &previousWithoutAlpha}) {
                frame->getChannel(ChannelType::Red)->data()[pixel] = previousRed;
                frame->getChannel(ChannelType::Green)->data()[pixel] = previousGreen;
                frame->getChannel(ChannelType::Blue)->data()[pixel] = previousBlue;
            }
            for (VideoFrame* frame : {&currentWithAlpha, &currentWithoutAlpha}) {
                frame->getChannel(ChannelType::Red)->data()[pixel] = currentRed;
                frame->getChannel(ChannelType::Green)->data()[pixel] = currentGreen;
                frame->getChannel(ChannelType::Blue)->data()[pixel] = currentBlue;
            }
            previousWithAlpha.getChannel(ChannelType::Alpha)->data()[pixel] = 0.2f;
            currentWithAlpha.getChannel(ChannelType::Alpha)->data()[pixel] =
                static_cast<float>(pixel) / (width * height - 1);
        }
    }
    previousWithoutAlpha.removeChannel(ChannelType::Alpha);
    currentWithoutAlpha.removeChannel(ChannelType::Alpha);

    SurfaceMemoryEffect withAlphaEffect;
    withAlphaEffect.setParameter("Retention", 0.8f);
    withAlphaEffect.setParameter("Refresh", 0.4f);
    withAlphaEffect.setParameter("Texture", 0.3f);
    withAlphaEffect.setParameter("Smear", 0.25f);
    SurfaceMemoryEffect withoutAlphaEffect;
    withoutAlphaEffect.setParameter("Retention", 0.8f);
    withoutAlphaEffect.setParameter("Refresh", 0.4f);
    withoutAlphaEffect.setParameter("Texture", 0.3f);
    withoutAlphaEffect.setParameter("Smear", 0.25f);
    const CreativeEffectContext firstContext{.time = 0.5, .frameIndex = 0};
    const CreativeEffectContext secondContext{.time = 1.25, .frameIndex = 1};
    withAlphaEffect.process(previousWithAlpha, firstContext);
    withAlphaEffect.process(currentWithAlpha, secondContext);
    withoutAlphaEffect.process(previousWithoutAlpha, firstContext);
    withoutAlphaEffect.process(currentWithoutAlpha, secondContext);

    bool changedRgb = false;
    for (const ChannelType channelType : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
        const auto expected = currentWithAlpha.getChannel(channelType);
        const auto actual = currentWithoutAlpha.getChannel(channelType);
        ASSERT_TRUE(expected);
        ASSERT_TRUE(actual);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int pixel = y * width + x;
                EXPECT_TRUE(std::isfinite(actual->data()[pixel]));
                EXPECT_FLOAT_EQ(actual->data()[pixel], expected->data()[pixel]);
                const float original = channelType == ChannelType::Red
                    ? 0.8f - static_cast<float>(x) * 0.06f
                    : channelType == ChannelType::Green
                        ? 0.2f + static_cast<float>(y) * 0.15f
                        : 0.3f + static_cast<float>((2 * x + y) % 5) * 0.09f;
                changedRgb = changedRgb ||
                    std::abs(actual->data()[pixel] - original) > 1.0e-6f;
            }
        }
    }
    const auto alpha = currentWithAlpha.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(alpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        EXPECT_FLOAT_EQ(alpha->data()[pixel], static_cast<float>(pixel) / (width * height - 1));
    }
    EXPECT_TRUE(changedRgb);
    EXPECT_FALSE(currentWithoutAlpha.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, SurfaceMemoryRewindResetsPriorSurface) {
    constexpr int width = 2;
    constexpr int height = 2;
    VideoFrame earlier(width, height);
    VideoFrame later(width, height);
    VideoFrame rewound(width, height);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
            earlier.getChannel(channel)->data()[pixel] = 0.2f;
            later.getChannel(channel)->data()[pixel] = 0.8f;
            rewound.getChannel(channel)->data()[pixel] = 0.4f;
        }
        earlier.getChannel(ChannelType::Alpha)->data()[pixel] = 0.3f;
        later.getChannel(ChannelType::Alpha)->data()[pixel] = 0.6f;
        rewound.getChannel(ChannelType::Alpha)->data()[pixel] = 0.9f;
    }

    SurfaceMemoryEffect effect;
    effect.setParameter("Retention", 1.0f);
    effect.setParameter("Refresh", 0.0f);
    effect.setParameter("Texture", 0.0f);
    effect.setParameter("Smear", 0.0f);
    effect.process(earlier, CreativeEffectContext{.time = 0.0, .frameIndex = 5});
    effect.process(later, CreativeEffectContext{.time = 0.5, .frameIndex = 6});
    effect.process(rewound, CreativeEffectContext{.time = 0.2, .frameIndex = 3});

    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto plane = rewound.getChannel(channel);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(plane->data()[pixel], 0.4f)
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
        }
    }
    const auto alpha = rewound.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(alpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        EXPECT_FLOAT_EQ(alpha->data()[pixel], 0.9f) << "pixel=" << pixel;
    }
}

TEST(CreativeImageEffectContractTest, SurfaceMemoryMissingChannelDoesNotAdvanceMemory) {
    constexpr int width = 3;
    constexpr int height = 2;
    VideoFrame initial(width, height);
    VideoFrame incomplete(width, height);
    VideoFrame afterIncomplete(width, height);
    VideoFrame directCurrent(width, height);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        for (VideoFrame* frame : {&initial, &incomplete, &afterIncomplete, &directCurrent}) {
            frame->getChannel(ChannelType::Red)->data()[pixel] = 0.2f;
            frame->getChannel(ChannelType::Green)->data()[pixel] = 0.2f;
            frame->getChannel(ChannelType::Blue)->data()[pixel] = 0.2f;
            frame->getChannel(ChannelType::Alpha)->data()[pixel] = 0.35f;
        }
        incomplete.getChannel(ChannelType::Red)->data()[pixel] = 0.9f;
        incomplete.getChannel(ChannelType::Green)->data()[pixel] = 0.9f;
        afterIncomplete.getChannel(ChannelType::Red)->data()[pixel] = 0.8f;
        afterIncomplete.getChannel(ChannelType::Green)->data()[pixel] = 0.8f;
        afterIncomplete.getChannel(ChannelType::Blue)->data()[pixel] = 0.8f;
        directCurrent.getChannel(ChannelType::Red)->data()[pixel] = 0.8f;
        directCurrent.getChannel(ChannelType::Green)->data()[pixel] = 0.8f;
        directCurrent.getChannel(ChannelType::Blue)->data()[pixel] = 0.8f;
    }
    incomplete.removeChannel(ChannelType::Blue);

    SurfaceMemoryEffect withGapEffect;
    withGapEffect.setParameter("Retention", 1.0f);
    withGapEffect.setParameter("Refresh", 0.0f);
    withGapEffect.setParameter("Texture", 0.0f);
    withGapEffect.setParameter("Smear", 0.0f);
    SurfaceMemoryEffect directEffect;
    directEffect.setParameter("Retention", 1.0f);
    directEffect.setParameter("Refresh", 0.0f);
    directEffect.setParameter("Texture", 0.0f);
    directEffect.setParameter("Smear", 0.0f);
    withGapEffect.process(initial, CreativeEffectContext{.frameIndex = 0});
    directEffect.process(initial, CreativeEffectContext{.frameIndex = 0});
    withGapEffect.process(incomplete, CreativeEffectContext{.frameIndex = 1});
    withGapEffect.process(afterIncomplete, CreativeEffectContext{.frameIndex = 2});
    directEffect.process(directCurrent, CreativeEffectContext{.frameIndex = 2});

    for (int pixel = 0; pixel < width * height; ++pixel) {
        for (const ChannelType channelType : {ChannelType::Red, ChannelType::Green,
                                              ChannelType::Blue}) {
            const auto afterGap = afterIncomplete.getChannel(channelType);
            const auto direct = directCurrent.getChannel(channelType);
            ASSERT_TRUE(afterGap);
            ASSERT_TRUE(direct);
            EXPECT_FLOAT_EQ(afterGap->data()[pixel], direct->data()[pixel])
                << "channel=" << static_cast<int>(channelType) << " pixel=" << pixel;
        }
        EXPECT_FLOAT_EQ(incomplete.getChannel(ChannelType::Red)->data()[pixel], 0.9f);
        EXPECT_FLOAT_EQ(incomplete.getChannel(ChannelType::Green)->data()[pixel], 0.9f);
        EXPECT_FLOAT_EQ(incomplete.getChannel(ChannelType::Alpha)->data()[pixel], 0.35f);
    }
    EXPECT_FALSE(incomplete.getChannel(ChannelType::Blue));
}

TEST(CreativeImageEffectContractTest, TemporalFossilCreatesResidueFromPriorFrame) {
    constexpr int width = 3;
    constexpr int height = 2;
    VideoFrame previousFrame(width, height);
    VideoFrame currentFrame(width, height);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
            previousFrame.getChannel(channel)->data()[pixel] = 0.2f;
            currentFrame.getChannel(channel)->data()[pixel] = 0.8f;
        }
        previousFrame.getChannel(ChannelType::Alpha)->data()[pixel] = 0.15f;
        currentFrame.getChannel(ChannelType::Alpha)->data()[pixel] =
            static_cast<float>(pixel + 1) / 10.0f;
    }

    TemporalFossilEffect effect;
    effect.setParameter("Persistence", 1.0f);
    effect.setParameter("Decay", 0.5f);
    effect.setParameter("EdgeThreshold", 0.0f);
    effect.setParameter("ChromaEcho", 0.0f);
    effect.process(previousFrame,
                   CreativeEffectContext{.time = 0.0, .frameIndex = 0});
    effect.process(currentFrame,
                   CreativeEffectContext{.time = 0.25, .frameIndex = 1});

    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto plane = currentFrame.getChannel(channel);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(plane->data()[pixel], 0.3f)
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
        }
    }
    const auto alpha = currentFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(alpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        EXPECT_FLOAT_EQ(alpha->data()[pixel], static_cast<float>(pixel + 1) / 10.0f)
            << "pixel=" << pixel;
    }
}

TEST(CreativeImageEffectContractTest, TemporalFossilRgbHistoryDoesNotDependOnOptionalAlphaPlane) {
    constexpr int width = 3;
    constexpr int height = 2;
    VideoFrame previousWithAlpha(width, height);
    VideoFrame currentWithAlpha(width, height);
    VideoFrame previousWithoutAlpha(width, height);
    VideoFrame currentWithoutAlpha(width, height);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        for (VideoFrame* frame : {&previousWithAlpha, &previousWithoutAlpha}) {
            frame->getChannel(ChannelType::Red)->data()[pixel] = 0.2f;
            frame->getChannel(ChannelType::Green)->data()[pixel] = 0.2f;
            frame->getChannel(ChannelType::Blue)->data()[pixel] = 0.2f;
        }
        for (VideoFrame* frame : {&currentWithAlpha, &currentWithoutAlpha}) {
            frame->getChannel(ChannelType::Red)->data()[pixel] = 0.8f;
            frame->getChannel(ChannelType::Green)->data()[pixel] = 0.8f;
            frame->getChannel(ChannelType::Blue)->data()[pixel] = 0.8f;
        }
        previousWithAlpha.getChannel(ChannelType::Alpha)->data()[pixel] = 0.25f;
        currentWithAlpha.getChannel(ChannelType::Alpha)->data()[pixel] =
            static_cast<float>(pixel + 1) / 10.0f;
    }
    previousWithoutAlpha.removeChannel(ChannelType::Alpha);
    currentWithoutAlpha.removeChannel(ChannelType::Alpha);

    TemporalFossilEffect withAlphaEffect;
    withAlphaEffect.setParameter("Persistence", 1.0f);
    withAlphaEffect.setParameter("Decay", 0.5f);
    withAlphaEffect.setParameter("EdgeThreshold", 0.0f);
    withAlphaEffect.setParameter("ChromaEcho", 0.0f);
    TemporalFossilEffect withoutAlphaEffect;
    withoutAlphaEffect.setParameter("Persistence", 1.0f);
    withoutAlphaEffect.setParameter("Decay", 0.5f);
    withoutAlphaEffect.setParameter("EdgeThreshold", 0.0f);
    withoutAlphaEffect.setParameter("ChromaEcho", 0.0f);
    const CreativeEffectContext firstContext{.time = 0.0, .frameIndex = 0};
    const CreativeEffectContext secondContext{.time = 0.25, .frameIndex = 1};
    withAlphaEffect.process(previousWithAlpha, firstContext);
    withAlphaEffect.process(currentWithAlpha, secondContext);
    withoutAlphaEffect.process(previousWithoutAlpha, firstContext);
    withoutAlphaEffect.process(currentWithoutAlpha, secondContext);

    for (const ChannelType channelType : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
        const auto expected = currentWithAlpha.getChannel(channelType);
        const auto actual = currentWithoutAlpha.getChannel(channelType);
        ASSERT_TRUE(expected);
        ASSERT_TRUE(actual);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(actual->data()[pixel], expected->data()[pixel]);
            EXPECT_FLOAT_EQ(actual->data()[pixel], 0.3f);
        }
    }
    const auto outputAlpha = currentWithAlpha.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(outputAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        EXPECT_FLOAT_EQ(outputAlpha->data()[pixel], static_cast<float>(pixel + 1) / 10.0f);
    }
    EXPECT_FALSE(currentWithoutAlpha.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, TemporalFossilMissingChannelDoesNotAdvanceHistory) {
    constexpr int width = 3;
    constexpr int height = 2;
    VideoFrame initial(width, height);
    VideoFrame incomplete(width, height);
    VideoFrame afterIncomplete(width, height);
    VideoFrame directCurrent(width, height);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        for (VideoFrame* frame : {&initial, &incomplete, &afterIncomplete, &directCurrent}) {
            frame->getChannel(ChannelType::Red)->data()[pixel] = 0.2f;
            frame->getChannel(ChannelType::Green)->data()[pixel] = 0.2f;
            frame->getChannel(ChannelType::Blue)->data()[pixel] = 0.2f;
            frame->getChannel(ChannelType::Alpha)->data()[pixel] = 0.35f;
        }
        afterIncomplete.getChannel(ChannelType::Red)->data()[pixel] = 0.8f;
        afterIncomplete.getChannel(ChannelType::Green)->data()[pixel] = 0.8f;
        afterIncomplete.getChannel(ChannelType::Blue)->data()[pixel] = 0.8f;
        directCurrent.getChannel(ChannelType::Red)->data()[pixel] = 0.8f;
        directCurrent.getChannel(ChannelType::Green)->data()[pixel] = 0.8f;
        directCurrent.getChannel(ChannelType::Blue)->data()[pixel] = 0.8f;
    }
    incomplete.removeChannel(ChannelType::Blue);

    TemporalFossilEffect withGapEffect;
    withGapEffect.setParameter("Persistence", 1.0f);
    withGapEffect.setParameter("Decay", 0.5f);
    withGapEffect.setParameter("EdgeThreshold", 0.0f);
    withGapEffect.setParameter("ChromaEcho", 0.0f);
    TemporalFossilEffect directEffect;
    directEffect.setParameter("Persistence", 1.0f);
    directEffect.setParameter("Decay", 0.5f);
    directEffect.setParameter("EdgeThreshold", 0.0f);
    directEffect.setParameter("ChromaEcho", 0.0f);
    withGapEffect.process(initial, CreativeEffectContext{.frameIndex = 0});
    directEffect.process(initial, CreativeEffectContext{.frameIndex = 0});
    withGapEffect.process(incomplete, CreativeEffectContext{.frameIndex = 1});
    withGapEffect.process(afterIncomplete, CreativeEffectContext{.frameIndex = 2});
    directEffect.process(directCurrent, CreativeEffectContext{.frameIndex = 2});

    for (int pixel = 0; pixel < width * height; ++pixel) {
        for (const ChannelType channelType : {ChannelType::Red, ChannelType::Green,
                                              ChannelType::Blue}) {
            const auto afterGap = afterIncomplete.getChannel(channelType);
            const auto direct = directCurrent.getChannel(channelType);
            ASSERT_TRUE(afterGap);
            ASSERT_TRUE(direct);
            EXPECT_FLOAT_EQ(afterGap->data()[pixel], direct->data()[pixel])
                << "channel=" << static_cast<int>(channelType) << " pixel=" << pixel;
        }
        EXPECT_FLOAT_EQ(incomplete.getChannel(ChannelType::Red)->data()[pixel], 0.2f);
        EXPECT_FLOAT_EQ(incomplete.getChannel(ChannelType::Green)->data()[pixel], 0.2f);
        EXPECT_FLOAT_EQ(incomplete.getChannel(ChannelType::Alpha)->data()[pixel], 0.35f);
    }
    EXPECT_FALSE(incomplete.getChannel(ChannelType::Blue));
}

TEST(CreativeImageEffectContractTest, DepthMeltPreservesUniformGrayAndAlpha) {
    VideoFrame frame(3, 2);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int index = 0; index < 6; ++index) {
        red->data()[index] = 0.5f;
        green->data()[index] = 0.5f;
        blue->data()[index] = 0.5f;
        alpha->data()[index] = static_cast<float>(index) / 10.0f;
    }

    DepthMeltEffect effect;
    effect.setParameter("Melt", 0.0f);
    effect.setParameter("Heat", 0.0f);
    effect.process(frame, CreativeEffectContext{.time = 3.25});

    for (int index = 0; index < 6; ++index) {
        EXPECT_FLOAT_EQ(red->data()[index], 0.5f) << "red pixel=" << index;
        EXPECT_FLOAT_EQ(green->data()[index], 0.5f) << "green pixel=" << index;
        EXPECT_FLOAT_EQ(blue->data()[index], 0.5f) << "blue pixel=" << index;
        EXPECT_FLOAT_EQ(alpha->data()[index], static_cast<float>(index) / 10.0f)
            << "alpha pixel=" << index;
    }
}

TEST(CreativeImageEffectContractTest, DepthMeltActiveSettingsAreRepeatableAndKeepAlpha) {
    constexpr int width = 8;
    constexpr int height = 6;
    constexpr int pixelCount = width * height;
    VideoFrame first(width, height);
    VideoFrame repeated(width, height);
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    float original[pixelCount][4]{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            original[pixel][0] = 0.15f + x * 0.07f + y * 0.025f;
            original[pixel][1] = 0.8f - x * 0.055f - y * 0.03f;
            original[pixel][2] = 0.2f + ((x + 2 * y) % 7) * 0.08f;
            original[pixel][3] = 0.1f + static_cast<float>(pixel) / 64.0f;
            for (int channel = 0; channel < 4; ++channel) {
                const auto firstPlane = first.getChannel(channels[channel]);
                const auto repeatedPlane = repeated.getChannel(channels[channel]);
                ASSERT_TRUE(firstPlane);
                ASSERT_TRUE(repeatedPlane);
                firstPlane->data()[pixel] = original[pixel][channel];
                repeatedPlane->data()[pixel] = original[pixel][channel];
            }
        }
    }

    DepthMeltEffect firstEffect;
    firstEffect.setParameter("Melt", 0.8f);
    firstEffect.setParameter("Gravity", 0.6f);
    firstEffect.setParameter("Heat", 0.5f);
    firstEffect.setParameter("Detail", 0.7f);
    DepthMeltEffect repeatedEffect;
    repeatedEffect.setParameter("Melt", 0.8f);
    repeatedEffect.setParameter("Gravity", 0.6f);
    repeatedEffect.setParameter("Heat", 0.5f);
    repeatedEffect.setParameter("Detail", 0.7f);
    const CreativeEffectContext context{.time = 1.25, .frameIndex = 37};
    firstEffect.process(first, context);
    repeatedEffect.process(repeated, context);

    bool changedRgb = false;
    for (int channel = 0; channel < 3; ++channel) {
        const auto firstPlane = first.getChannel(channels[channel]);
        const auto repeatedPlane = repeated.getChannel(channels[channel]);
        ASSERT_TRUE(firstPlane);
        ASSERT_TRUE(repeatedPlane);
        for (int pixel = 0; pixel < pixelCount; ++pixel) {
            EXPECT_TRUE(std::isfinite(firstPlane->data()[pixel]))
                << "channel=" << channel << " pixel=" << pixel;
            EXPECT_FLOAT_EQ(firstPlane->data()[pixel], repeatedPlane->data()[pixel])
                << "channel=" << channel << " pixel=" << pixel;
            changedRgb = changedRgb || std::abs(
                firstPlane->data()[pixel] - original[pixel][channel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(changedRgb);
    const auto firstAlpha = first.getChannel(ChannelType::Alpha);
    const auto repeatedAlpha = repeated.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(firstAlpha);
    ASSERT_TRUE(repeatedAlpha);
    for (int pixel = 0; pixel < pixelCount; ++pixel) {
        EXPECT_FLOAT_EQ(firstAlpha->data()[pixel], original[pixel][3]);
        EXPECT_FLOAT_EQ(repeatedAlpha->data()[pixel], original[pixel][3]);
    }
}

TEST(CreativeImageEffectContractTest, DepthMeltRgbOutputDoesNotDependOnOptionalAlphaPlane) {
    constexpr int width = 8;
    constexpr int height = 6;
    VideoFrame withAlpha(width, height);
    VideoFrame withoutAlpha(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float red = 0.15f + static_cast<float>(x) * 0.07f +
                              static_cast<float>(y) * 0.025f;
            const float green = 0.8f - static_cast<float>(x) * 0.055f -
                                static_cast<float>(y) * 0.03f;
            const float blue = 0.2f + static_cast<float>((x + 2 * y) % 7) * 0.08f;
            for (VideoFrame* frame : {&withAlpha, &withoutAlpha}) {
                frame->getChannel(ChannelType::Red)->data()[pixel] = red;
                frame->getChannel(ChannelType::Green)->data()[pixel] = green;
                frame->getChannel(ChannelType::Blue)->data()[pixel] = blue;
            }
            withAlpha.getChannel(ChannelType::Alpha)->data()[pixel] =
                0.1f + static_cast<float>(pixel) / 64.0f;
        }
    }
    withoutAlpha.removeChannel(ChannelType::Alpha);

    DepthMeltEffect withAlphaEffect;
    withAlphaEffect.setParameter("Melt", 0.8f);
    withAlphaEffect.setParameter("Gravity", 0.6f);
    withAlphaEffect.setParameter("Heat", 0.5f);
    withAlphaEffect.setParameter("Detail", 0.7f);
    DepthMeltEffect withoutAlphaEffect;
    withoutAlphaEffect.setParameter("Melt", 0.8f);
    withoutAlphaEffect.setParameter("Gravity", 0.6f);
    withoutAlphaEffect.setParameter("Heat", 0.5f);
    withoutAlphaEffect.setParameter("Detail", 0.7f);
    const CreativeEffectContext context{.time = 1.25, .frameIndex = 37};
    withAlphaEffect.process(withAlpha, context);
    withoutAlphaEffect.process(withoutAlpha, context);

    bool changedRgb = false;
    for (const ChannelType channelType : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
        const auto expected = withAlpha.getChannel(channelType);
        const auto actual = withoutAlpha.getChannel(channelType);
        ASSERT_TRUE(expected);
        ASSERT_TRUE(actual);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int pixel = y * width + x;
                EXPECT_TRUE(std::isfinite(actual->data()[pixel]));
                EXPECT_FLOAT_EQ(actual->data()[pixel], expected->data()[pixel]);
                const float original = channelType == ChannelType::Red
                    ? 0.15f + static_cast<float>(x) * 0.07f + static_cast<float>(y) * 0.025f
                    : channelType == ChannelType::Green
                        ? 0.8f - static_cast<float>(x) * 0.055f - static_cast<float>(y) * 0.03f
                        : 0.2f + static_cast<float>((x + 2 * y) % 7) * 0.08f;
                changedRgb = changedRgb ||
                    std::abs(actual->data()[pixel] - original) > 1.0e-6f;
            }
        }
    }
    EXPECT_TRUE(changedRgb);
    EXPECT_FALSE(withoutAlpha.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, DepthMeltZeroMeltPreservesInputWithOtherControlsActive) {
    constexpr int width = 3;
    constexpr int height = 2;
    const float inputRed[] = {0.05f, 0.2f, 0.45f, 0.6f, 0.8f, 0.95f};
    const float inputGreen[] = {0.9f, 0.7f, 0.5f, 0.3f, 0.1f, 0.0f};
    const float inputBlue[] = {0.15f, 0.35f, 0.55f, 0.75f, 0.85f, 1.0f};
    const float inputAlpha[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    const float* inputs[] = {inputRed, inputGreen, inputBlue, inputAlpha};
    VideoFrame frame(width, height);
    for (int channelIndex = 0; channelIndex < 4; ++channelIndex) {
        const auto plane = frame.getChannel(channels[channelIndex]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            plane->data()[pixel] = inputs[channelIndex][pixel];
        }
    }

    DepthMeltEffect effect;
    effect.setParameter("Melt", 0.0f);
    effect.setParameter("Gravity", 1.0f);
    effect.setParameter("Heat", 1.0f);
    effect.setParameter("Detail", 1.0f);
    effect.process(frame, CreativeEffectContext{.time = 4.0});

    for (int channelIndex = 0; channelIndex < 4; ++channelIndex) {
        const auto plane = frame.getChannel(channels[channelIndex]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(plane->data()[pixel], inputs[channelIndex][pixel])
                << "channel=" << channelIndex << " pixel=" << pixel;
        }
    }
}

TEST(CreativeImageEffectContractTest, DepthMeltGravityChangesVerticalDisplacement) {
    constexpr int width = 8;
    constexpr int height = 8;
    VideoFrame lowGravityFrame(width, height);
    VideoFrame highGravityFrame(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float value = 0.08f + static_cast<float>(y) * 0.11f;
            for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                              ChannelType::Blue}) {
                lowGravityFrame.getChannel(channel)->data()[pixel] = value;
                highGravityFrame.getChannel(channel)->data()[pixel] = value;
            }
            const float alpha = static_cast<float>(pixel) / (width * height - 1);
            lowGravityFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
            highGravityFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        }
    }

    DepthMeltEffect lowGravity;
    lowGravity.setParameter("Melt", 0.8f);
    lowGravity.setParameter("Gravity", 0.0f);
    lowGravity.setParameter("Heat", 0.0f);
    lowGravity.setParameter("Detail", 0.5f);
    DepthMeltEffect highGravity;
    highGravity.setParameter("Melt", 0.8f);
    highGravity.setParameter("Gravity", 1.0f);
    highGravity.setParameter("Heat", 0.0f);
    highGravity.setParameter("Detail", 0.5f);
    const CreativeEffectContext context{.time = 1.25};
    lowGravity.process(lowGravityFrame, context);
    highGravity.process(highGravityFrame, context);

    bool gravityChangedRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto low = lowGravityFrame.getChannel(channel);
        const auto high = highGravityFrame.getChannel(channel);
        ASSERT_TRUE(low);
        ASSERT_TRUE(high);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(high->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            gravityChangedRgb = gravityChangedRgb ||
                std::abs(high->data()[pixel] - low->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(gravityChangedRgb);

    const auto lowAlpha = lowGravityFrame.getChannel(ChannelType::Alpha);
    const auto highAlpha = highGravityFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(lowAlpha);
    ASSERT_TRUE(highAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(lowAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(highAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, DepthMeltHeatChangesWarpAndHighlight) {
    constexpr int width = 8;
    constexpr int height = 8;
    VideoFrame coolFrame(width, height);
    VideoFrame hotFrame(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float red = 0.05f + static_cast<float>(x) * 0.08f;
            const float green = 0.05f + static_cast<float>(y) * 0.08f;
            const float blue = 0.1f + static_cast<float>((x + y) % width) * 0.07f;
            coolFrame.getChannel(ChannelType::Red)->data()[pixel] = red;
            coolFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
            coolFrame.getChannel(ChannelType::Blue)->data()[pixel] = blue;
            hotFrame.getChannel(ChannelType::Red)->data()[pixel] = red;
            hotFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
            hotFrame.getChannel(ChannelType::Blue)->data()[pixel] = blue;
            const float alpha = static_cast<float>(pixel) / (width * height - 1);
            coolFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
            hotFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        }
    }

    DepthMeltEffect coolEffect;
    coolEffect.setParameter("Melt", 0.75f);
    coolEffect.setParameter("Gravity", 0.5f);
    coolEffect.setParameter("Heat", 0.0f);
    coolEffect.setParameter("Detail", 0.4f);
    DepthMeltEffect hotEffect;
    hotEffect.setParameter("Melt", 0.75f);
    hotEffect.setParameter("Gravity", 0.5f);
    hotEffect.setParameter("Heat", 1.0f);
    hotEffect.setParameter("Detail", 0.4f);
    const CreativeEffectContext context{.time = 2.0};
    coolEffect.process(coolFrame, context);
    hotEffect.process(hotFrame, context);

    bool heatChangedRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto cool = coolFrame.getChannel(channel);
        const auto hot = hotFrame.getChannel(channel);
        ASSERT_TRUE(cool);
        ASSERT_TRUE(hot);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(hot->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            heatChangedRgb = heatChangedRgb ||
                std::abs(hot->data()[pixel] - cool->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(heatChangedRgb);

    const auto coolAlpha = coolFrame.getChannel(ChannelType::Alpha);
    const auto hotAlpha = hotFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(coolAlpha);
    ASSERT_TRUE(hotAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(coolAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(hotAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, DepthMeltDetailChangesGradientResponse) {
    constexpr int width = 8;
    constexpr int height = 8;
    VideoFrame smoothDetailFrame(width, height);
    VideoFrame sharpDetailFrame(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float value = 0.06f + static_cast<float>(y) * 0.12f;
            for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                              ChannelType::Blue}) {
                smoothDetailFrame.getChannel(channel)->data()[pixel] = value;
                sharpDetailFrame.getChannel(channel)->data()[pixel] = value;
            }
            const float alpha = static_cast<float>(pixel) / (width * height - 1);
            smoothDetailFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
            sharpDetailFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        }
    }

    DepthMeltEffect smoothDetail;
    smoothDetail.setParameter("Melt", 0.8f);
    smoothDetail.setParameter("Gravity", 0.5f);
    smoothDetail.setParameter("Heat", 0.0f);
    smoothDetail.setParameter("Detail", 0.0f);
    DepthMeltEffect sharpDetail;
    sharpDetail.setParameter("Melt", 0.8f);
    sharpDetail.setParameter("Gravity", 0.5f);
    sharpDetail.setParameter("Heat", 0.0f);
    sharpDetail.setParameter("Detail", 1.0f);
    const CreativeEffectContext context{.time = 1.5};
    smoothDetail.process(smoothDetailFrame, context);
    sharpDetail.process(sharpDetailFrame, context);

    bool detailChangedRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto smooth = smoothDetailFrame.getChannel(channel);
        const auto sharp = sharpDetailFrame.getChannel(channel);
        ASSERT_TRUE(smooth);
        ASSERT_TRUE(sharp);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(sharp->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            detailChangedRgb = detailChangedRgb ||
                std::abs(sharp->data()[pixel] - smooth->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(detailChangedRgb);

    const auto smoothAlpha = smoothDetailFrame.getChannel(ChannelType::Alpha);
    const auto sharpAlpha = sharpDetailFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(smoothAlpha);
    ASSERT_TRUE(sharpAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(smoothAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(sharpAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, GlitchIsRepeatableAtFixedTimeAndAppliesSharedGrainToRgb) {
    VideoFrame first(4, 2);
    VideoFrame second(4, 2);
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    for (const ChannelType channelType : channels) {
        auto firstPlane = first.getChannel(channelType);
        auto secondPlane = second.getChannel(channelType);
        ASSERT_TRUE(firstPlane);
        ASSERT_TRUE(secondPlane);
        for (int index = 0; index < 8; ++index) {
            const float value = channelType == ChannelType::Alpha
                ? static_cast<float>(index) / 10.0f
                : 0.5f;
            firstPlane->data()[index] = value;
            secondPlane->data()[index] = value;
        }
    }

    GlitchCreativeEffect firstEffect;
    GlitchCreativeEffect secondEffect;
    const CreativeEffectContext context{.time = 1.75, .frameIndex = 52};
    firstEffect.process(first, context);
    secondEffect.process(second, context);

    const auto firstRed = first.getChannel(ChannelType::Red);
    const auto firstGreen = first.getChannel(ChannelType::Green);
    const auto firstBlue = first.getChannel(ChannelType::Blue);
    const auto firstAlpha = first.getChannel(ChannelType::Alpha);
    const auto secondRed = second.getChannel(ChannelType::Red);
    const auto secondGreen = second.getChannel(ChannelType::Green);
    const auto secondBlue = second.getChannel(ChannelType::Blue);
    const auto secondAlpha = second.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(firstRed);
    ASSERT_TRUE(firstGreen);
    ASSERT_TRUE(firstBlue);
    ASSERT_TRUE(firstAlpha);
    ASSERT_TRUE(secondRed);
    ASSERT_TRUE(secondGreen);
    ASSERT_TRUE(secondBlue);
    ASSERT_TRUE(secondAlpha);
    for (int index = 0; index < 8; ++index) {
        EXPECT_TRUE(std::isfinite(firstRed->data()[index]));
        EXPECT_FLOAT_EQ(firstRed->data()[index], firstGreen->data()[index]);
        EXPECT_FLOAT_EQ(firstRed->data()[index], firstBlue->data()[index]);
        EXPECT_FLOAT_EQ(firstRed->data()[index], secondRed->data()[index]);
        EXPECT_FLOAT_EQ(firstGreen->data()[index], secondGreen->data()[index]);
        EXPECT_FLOAT_EQ(firstBlue->data()[index], secondBlue->data()[index]);
        EXPECT_FLOAT_EQ(firstAlpha->data()[index], static_cast<float>(index) / 10.0f);
        EXPECT_FLOAT_EQ(secondAlpha->data()[index], static_cast<float>(index) / 10.0f);
    }
}

TEST(CreativeImageEffectContractTest, GlitchRgbOutputDoesNotDependOnOptionalAlphaPlane) {
    constexpr int width = 8;
    constexpr int height = 4;
    VideoFrame withAlpha(width, height);
    VideoFrame withoutAlpha(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float red = static_cast<float>((x * 3 + y) % 9) / 9.0f;
            const float green = static_cast<float>((x + y * 2) % 8) / 8.0f;
            const float blue = static_cast<float>((x * 2 + y * 3) % 10) / 10.0f;
            for (VideoFrame* frame : {&withAlpha, &withoutAlpha}) {
                frame->getChannel(ChannelType::Red)->data()[pixel] = red;
                frame->getChannel(ChannelType::Green)->data()[pixel] = green;
                frame->getChannel(ChannelType::Blue)->data()[pixel] = blue;
            }
            withAlpha.getChannel(ChannelType::Alpha)->data()[pixel] =
                static_cast<float>(pixel) / (width * height - 1);
        }
    }
    withoutAlpha.removeChannel(ChannelType::Alpha);

    GlitchCreativeEffect withAlphaEffect;
    withAlphaEffect.setParameter("Amount", 0.8f);
    withAlphaEffect.setParameter("RGBSplit", 0.6f);
    withAlphaEffect.setParameter("BlockNoise", 0.0f);
    GlitchCreativeEffect withoutAlphaEffect;
    withoutAlphaEffect.setParameter("Amount", 0.8f);
    withoutAlphaEffect.setParameter("RGBSplit", 0.6f);
    withoutAlphaEffect.setParameter("BlockNoise", 0.0f);
    const CreativeEffectContext context{.time = 2.75};
    withAlphaEffect.process(withAlpha, context);
    withoutAlphaEffect.process(withoutAlpha, context);

    bool changedRgb = false;
    for (const ChannelType channelType : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
        const auto expected = withAlpha.getChannel(channelType);
        const auto actual = withoutAlpha.getChannel(channelType);
        ASSERT_TRUE(expected);
        ASSERT_TRUE(actual);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int pixel = y * width + x;
                EXPECT_TRUE(std::isfinite(actual->data()[pixel]));
                EXPECT_FLOAT_EQ(actual->data()[pixel], expected->data()[pixel]);
                const float original = channelType == ChannelType::Red
                    ? static_cast<float>((x * 3 + y) % 9) / 9.0f
                    : channelType == ChannelType::Green
                        ? static_cast<float>((x + y * 2) % 8) / 8.0f
                        : static_cast<float>((x * 2 + y * 3) % 10) / 10.0f;
                changedRgb = changedRgb ||
                    std::abs(actual->data()[pixel] - original) > 1.0e-6f;
            }
        }
    }
    EXPECT_TRUE(changedRgb);
    EXPECT_FALSE(withoutAlpha.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, GlitchRgbSplitOffsetsRedAndBlueAroundGreen) {
    constexpr int width = 64;
    VideoFrame frame(width, 1);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int x = 0; x < width; ++x) {
        const float value = static_cast<float>(x) / (width - 1);
        red->data()[x] = value;
        green->data()[x] = value;
        blue->data()[x] = value;
        alpha->data()[x] = 0.25f + value * 0.5f;
    }

    GlitchCreativeEffect effect;
    effect.setParameter("Amount", 1.0f);
    effect.setParameter("RGBSplit", 1.0f);
    effect.setParameter("BlockNoise", 0.0f);
    effect.process(frame, CreativeEffectContext{.time = 0.0});

    constexpr int centerPixel = width / 2;
    EXPECT_LT(red->data()[centerPixel], green->data()[centerPixel]);
    EXPECT_LT(green->data()[centerPixel], blue->data()[centerPixel]);
    for (int x = 0; x < width; ++x) {
        EXPECT_FLOAT_EQ(alpha->data()[x],
                        0.25f + static_cast<float>(x) / (width - 1) * 0.5f)
            << "pixel=" << x;
    }
}

TEST(CreativeImageEffectContractTest, GlitchZeroAmountSuppressesRgbSplit) {
    constexpr int width = 8;
    constexpr int height = 2;
    VideoFrame splitFrame(width, height);
    VideoFrame noSplitFrame(width, height);
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto splitPlane = splitFrame.getChannel(channel);
        const auto noSplitPlane = noSplitFrame.getChannel(channel);
        ASSERT_TRUE(splitPlane);
        ASSERT_TRUE(noSplitPlane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            const float value = static_cast<float>(pixel % width) / (width - 1);
            splitPlane->data()[pixel] = value;
            noSplitPlane->data()[pixel] = value;
        }
    }
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = 0.2f + static_cast<float>(pixel) / 20.0f;
        splitFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        noSplitFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
    }

    GlitchCreativeEffect splitEffect;
    splitEffect.setParameter("Amount", 0.0f);
    splitEffect.setParameter("RGBSplit", 1.0f);
    splitEffect.setParameter("BlockNoise", 0.0f);
    GlitchCreativeEffect noSplitEffect;
    noSplitEffect.setParameter("Amount", 0.0f);
    noSplitEffect.setParameter("RGBSplit", 0.0f);
    noSplitEffect.setParameter("BlockNoise", 0.0f);
    const CreativeEffectContext context{.time = 0.75, .frameIndex = 19};
    splitEffect.process(splitFrame, context);
    noSplitEffect.process(noSplitFrame, context);

    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto splitPlane = splitFrame.getChannel(channel);
        const auto noSplitPlane = noSplitFrame.getChannel(channel);
        ASSERT_TRUE(splitPlane);
        ASSERT_TRUE(noSplitPlane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(splitPlane->data()[pixel], noSplitPlane->data()[pixel])
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
        }
    }
    const auto splitAlpha = splitFrame.getChannel(ChannelType::Alpha);
    const auto noSplitAlpha = noSplitFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(splitAlpha);
    ASSERT_TRUE(noSplitAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float expectedAlpha = 0.2f + static_cast<float>(pixel) / 20.0f;
        EXPECT_FLOAT_EQ(splitAlpha->data()[pixel], expectedAlpha);
        EXPECT_FLOAT_EQ(noSplitAlpha->data()[pixel], expectedAlpha);
    }
}

TEST(CreativeImageEffectContractTest, DisabledGlitchPreservesEveryChannelExactly) {
    constexpr int width = 3;
    constexpr int height = 2;
    const float inputRed[] = {0.05f, 0.25f, 0.45f, 0.65f, 0.85f, 1.0f};
    const float inputGreen[] = {0.9f, 0.7f, 0.5f, 0.3f, 0.1f, 0.0f};
    const float inputBlue[] = {0.15f, 0.35f, 0.55f, 0.75f, 0.85f, 0.95f};
    const float inputAlpha[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    const ChannelType channels[] = {
        ChannelType::Red, ChannelType::Green,
        ChannelType::Blue, ChannelType::Alpha,
    };
    const float* inputs[] = {inputRed, inputGreen, inputBlue, inputAlpha};
    VideoFrame frame(width, height);
    for (int channelIndex = 0; channelIndex < 4; ++channelIndex) {
        const auto plane = frame.getChannel(channels[channelIndex]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            plane->data()[pixel] = inputs[channelIndex][pixel];
        }
    }

    GlitchCreativeEffect effect;
    effect.setEnabled(false);
    effect.process(frame, CreativeEffectContext{.time = 7.25, .frameIndex = 217});

    for (int channelIndex = 0; channelIndex < 4; ++channelIndex) {
        const auto plane = frame.getChannel(channels[channelIndex]);
        ASSERT_TRUE(plane);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(plane->data()[pixel], inputs[channelIndex][pixel])
                << "channel=" << channelIndex << " pixel=" << pixel;
        }
    }
}

TEST(CreativeImageEffectContractTest, GlitchMissingRequiredColorChannelLeavesFrameUnchanged) {
    VideoFrame frame(3, 2);
    const float red[] = {0.1f, 0.4f, 0.9f, 0.3f, 0.7f, 0.2f};
    const float green[] = {0.8f, 0.3f, 0.2f, 0.6f, 0.1f, 0.5f};
    const float alpha[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    for (int pixel = 0; pixel < 6; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = red[pixel];
        frame.getChannel(ChannelType::Green)->data()[pixel] = green[pixel];
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha[pixel];
    }
    frame.removeChannel(ChannelType::Blue);

    GlitchCreativeEffect effect;
    effect.setParameter("Amount", 1.0f);
    effect.setParameter("RGBSplit", 1.0f);
    effect.setParameter("BlockNoise", 1.0f);
    effect.process(frame, CreativeEffectContext{.time = 14.0});

    for (int pixel = 0; pixel < 6; ++pixel) {
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[pixel], red[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[pixel], green[pixel]);
        EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[pixel], alpha[pixel]);
    }
    EXPECT_FALSE(frame.getChannel(ChannelType::Blue));
}

TEST(CreativeImageEffectContractTest, PigmentSeparationUniformZeroTextureMatchesExpectedPaperMix) {
    VideoFrame frame(2, 2);
    auto red = frame.getChannel(ChannelType::Red);
    auto green = frame.getChannel(ChannelType::Green);
    auto blue = frame.getChannel(ChannelType::Blue);
    auto alpha = frame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int index = 0; index < 4; ++index) {
        red->data()[index] = 0.5f;
        green->data()[index] = 0.5f;
        blue->data()[index] = 0.5f;
        alpha->data()[index] = static_cast<float>(index) / 4.0f;
    }

    PigmentSeparationEffect effect;
    effect.setParameter("Spread", 0.0f);
    effect.setParameter("Bleed", 0.0f);
    effect.setParameter("Flow", 0.0f);
    effect.setParameter("Granulation", 0.0f);
    effect.process(frame, CreativeEffectContext{.time = 4.0});

    constexpr float expected = 0.47525f;
    for (int index = 0; index < 4; ++index) {
        EXPECT_NEAR(red->data()[index], expected, 1e-6f) << "red pixel=" << index;
        EXPECT_NEAR(green->data()[index], expected, 1e-6f) << "green pixel=" << index;
        EXPECT_NEAR(blue->data()[index], expected, 1e-6f) << "blue pixel=" << index;
        EXPECT_FLOAT_EQ(alpha->data()[index], static_cast<float>(index) / 4.0f)
            << "alpha pixel=" << index;
    }
}

TEST(CreativeImageEffectContractTest, PigmentSeparationGranulationAddsPaperTexture) {
    constexpr int width = 8;
    constexpr int height = 6;
    VideoFrame smoothFrame(width, height);
    VideoFrame texturedFrame(width, height);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
            smoothFrame.getChannel(channel)->data()[pixel] = 0.5f;
            texturedFrame.getChannel(channel)->data()[pixel] = 0.5f;
        }
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        smoothFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        texturedFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
    }

    PigmentSeparationEffect smoothEffect;
    smoothEffect.setParameter("Spread", 0.0f);
    smoothEffect.setParameter("Bleed", 0.0f);
    smoothEffect.setParameter("Flow", 0.0f);
    smoothEffect.setParameter("Granulation", 0.0f);
    PigmentSeparationEffect texturedEffect;
    texturedEffect.setParameter("Spread", 0.0f);
    texturedEffect.setParameter("Bleed", 0.0f);
    texturedEffect.setParameter("Flow", 0.0f);
    texturedEffect.setParameter("Granulation", 1.0f);
    const CreativeEffectContext context{.time = 2.5};
    smoothEffect.process(smoothFrame, context);
    texturedEffect.process(texturedFrame, context);

    bool textureChangedRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto smooth = smoothFrame.getChannel(channel);
        const auto textured = texturedFrame.getChannel(channel);
        ASSERT_TRUE(smooth);
        ASSERT_TRUE(textured);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(textured->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            textureChangedRgb = textureChangedRgb ||
                std::abs(textured->data()[pixel] - smooth->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(textureChangedRgb);
    const auto smoothAlpha = smoothFrame.getChannel(ChannelType::Alpha);
    const auto texturedAlpha = texturedFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(smoothAlpha);
    ASSERT_TRUE(texturedAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(smoothAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(texturedAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, PigmentSeparationSpreadSoftensHighContrastEdge) {
    constexpr int width = 32;
    constexpr int height = 8;
    VideoFrame noSpreadFrame(width, height);
    VideoFrame spreadFrame(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float value = x < width / 2 ? 0.0f : 1.0f;
            for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                              ChannelType::Blue}) {
                noSpreadFrame.getChannel(channel)->data()[pixel] = value;
                spreadFrame.getChannel(channel)->data()[pixel] = value;
            }
            const float alpha = static_cast<float>(pixel) / (width * height - 1);
            noSpreadFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
            spreadFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        }
    }

    PigmentSeparationEffect noSpreadEffect;
    noSpreadEffect.setParameter("Spread", 0.0f);
    noSpreadEffect.setParameter("Bleed", 0.0f);
    noSpreadEffect.setParameter("Flow", 0.0f);
    noSpreadEffect.setParameter("Granulation", 0.0f);
    PigmentSeparationEffect spreadEffect;
    spreadEffect.setParameter("Spread", 1.0f);
    spreadEffect.setParameter("Bleed", 0.0f);
    spreadEffect.setParameter("Flow", 0.0f);
    spreadEffect.setParameter("Granulation", 0.0f);
    const CreativeEffectContext context{.time = 1.75};
    noSpreadEffect.process(noSpreadFrame, context);
    spreadEffect.process(spreadFrame, context);

    bool spreadChangedEdge = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto noSpread = noSpreadFrame.getChannel(channel);
        const auto spread = spreadFrame.getChannel(channel);
        ASSERT_TRUE(noSpread);
        ASSERT_TRUE(spread);
        for (int y = 0; y < height; ++y) {
            for (int x = width / 2 - 2; x <= width / 2 + 1; ++x) {
                const int pixel = y * width + x;
                EXPECT_TRUE(std::isfinite(spread->data()[pixel]))
                    << "channel=" << static_cast<int>(channel)
                    << " x=" << x << " y=" << y;
                spreadChangedEdge = spreadChangedEdge ||
                    std::abs(spread->data()[pixel] - noSpread->data()[pixel]) > 1.0e-6f;
            }
        }
    }
    EXPECT_TRUE(spreadChangedEdge);

    const auto noSpreadAlpha = noSpreadFrame.getChannel(ChannelType::Alpha);
    const auto spreadAlpha = spreadFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(noSpreadAlpha);
    ASSERT_TRUE(spreadAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(noSpreadAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(spreadAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, PigmentSeparationFlowChangesDirectionalSampling) {
    constexpr int width = 12;
    constexpr int height = 10;
    VideoFrame stillFlowFrame(width, height);
    VideoFrame variedFlowFrame(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float red = static_cast<float>(x) / (width - 1);
            const float green = static_cast<float>(y) / (height - 1);
            const float blue = static_cast<float>((x + 2 * y) % width) / (width - 1);
            stillFlowFrame.getChannel(ChannelType::Red)->data()[pixel] = red;
            stillFlowFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
            stillFlowFrame.getChannel(ChannelType::Blue)->data()[pixel] = blue;
            variedFlowFrame.getChannel(ChannelType::Red)->data()[pixel] = red;
            variedFlowFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
            variedFlowFrame.getChannel(ChannelType::Blue)->data()[pixel] = blue;
            const float alpha = static_cast<float>(pixel) / (width * height - 1);
            stillFlowFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
            variedFlowFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        }
    }

    PigmentSeparationEffect stillFlowEffect;
    stillFlowEffect.setParameter("Spread", 0.65f);
    stillFlowEffect.setParameter("Bleed", 0.0f);
    stillFlowEffect.setParameter("Flow", 0.0f);
    stillFlowEffect.setParameter("Granulation", 0.0f);
    PigmentSeparationEffect variedFlowEffect;
    variedFlowEffect.setParameter("Spread", 0.65f);
    variedFlowEffect.setParameter("Bleed", 0.0f);
    variedFlowEffect.setParameter("Flow", 1.0f);
    variedFlowEffect.setParameter("Granulation", 0.0f);
    const CreativeEffectContext context{.time = 3.0};
    stillFlowEffect.process(stillFlowFrame, context);
    variedFlowEffect.process(variedFlowFrame, context);

    bool flowChangedRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto stillFlow = stillFlowFrame.getChannel(channel);
        const auto variedFlow = variedFlowFrame.getChannel(channel);
        ASSERT_TRUE(stillFlow);
        ASSERT_TRUE(variedFlow);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(variedFlow->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            flowChangedRgb = flowChangedRgb ||
                std::abs(variedFlow->data()[pixel] - stillFlow->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(flowChangedRgb);

    const auto stillFlowAlpha = stillFlowFrame.getChannel(ChannelType::Alpha);
    const auto variedFlowAlpha = variedFlowFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(stillFlowAlpha);
    ASSERT_TRUE(variedFlowAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(stillFlowAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(variedFlowAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, PigmentSeparationBleedChangesWetChannelMix) {
    constexpr int width = 12;
    constexpr int height = 10;
    VideoFrame dryFrame(width, height);
    VideoFrame wetFrame(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float red = static_cast<float>(x) / (width - 1);
            const float green = static_cast<float>(y) / (height - 1);
            const float blue = static_cast<float>((x + 2 * y) % width) / (width - 1);
            dryFrame.getChannel(ChannelType::Red)->data()[pixel] = red;
            dryFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
            dryFrame.getChannel(ChannelType::Blue)->data()[pixel] = blue;
            wetFrame.getChannel(ChannelType::Red)->data()[pixel] = red;
            wetFrame.getChannel(ChannelType::Green)->data()[pixel] = green;
            wetFrame.getChannel(ChannelType::Blue)->data()[pixel] = blue;
            const float alpha = static_cast<float>(pixel) / (width * height - 1);
            dryFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
            wetFrame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha;
        }
    }

    PigmentSeparationEffect dryEffect;
    dryEffect.setParameter("Spread", 0.0f);
    dryEffect.setParameter("Bleed", 0.0f);
    dryEffect.setParameter("Flow", 0.0f);
    dryEffect.setParameter("Granulation", 0.0f);
    PigmentSeparationEffect wetEffect;
    wetEffect.setParameter("Spread", 0.0f);
    wetEffect.setParameter("Bleed", 1.0f);
    wetEffect.setParameter("Flow", 0.0f);
    wetEffect.setParameter("Granulation", 0.0f);
    const CreativeEffectContext context{.time = 4.25};
    dryEffect.process(dryFrame, context);
    wetEffect.process(wetFrame, context);

    bool bleedChangedRgb = false;
    for (const ChannelType channel : {ChannelType::Red, ChannelType::Green,
                                      ChannelType::Blue}) {
        const auto dry = dryFrame.getChannel(channel);
        const auto wet = wetFrame.getChannel(channel);
        ASSERT_TRUE(dry);
        ASSERT_TRUE(wet);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_TRUE(std::isfinite(wet->data()[pixel]))
                << "channel=" << static_cast<int>(channel) << " pixel=" << pixel;
            bleedChangedRgb = bleedChangedRgb ||
                std::abs(wet->data()[pixel] - dry->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(bleedChangedRgb);

    const auto dryAlpha = dryFrame.getChannel(ChannelType::Alpha);
    const auto wetAlpha = wetFrame.getChannel(ChannelType::Alpha);
    ASSERT_TRUE(dryAlpha);
    ASSERT_TRUE(wetAlpha);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float alpha = static_cast<float>(pixel) / (width * height - 1);
        EXPECT_FLOAT_EQ(dryAlpha->data()[pixel], alpha);
        EXPECT_FLOAT_EQ(wetAlpha->data()[pixel], alpha);
    }
}

TEST(CreativeImageEffectContractTest, DisabledPigmentSeparationPreservesEveryChannelExactly) {
    constexpr int width = 7;
    constexpr int height = 5;
    VideoFrame frame(width, height);
    std::vector<std::vector<float>> original(4, std::vector<float>(width * height));
    const std::array<ChannelType, 4> channels = {
        ChannelType::Red, ChannelType::Green, ChannelType::Blue, ChannelType::Alpha};
    for (int channelIndex = 0; channelIndex < static_cast<int>(channels.size()); ++channelIndex) {
        auto channel = frame.getChannel(channels[channelIndex]);
        ASSERT_TRUE(channel);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            const float value = static_cast<float>((pixel * 11 + channelIndex * 17) % 37) / 36.0f;
            channel->data()[pixel] = value;
            original[channelIndex][pixel] = value;
        }
    }

    PigmentSeparationEffect effect;
    effect.setEnabled(false);
    effect.setParameter("Spread", 1.0f);
    effect.setParameter("Bleed", 1.0f);
    effect.setParameter("Flow", 1.0f);
    effect.setParameter("Granulation", 1.0f);
    effect.process(frame, CreativeEffectContext{.time = 8.5});

    for (int channelIndex = 0; channelIndex < static_cast<int>(channels.size()); ++channelIndex) {
        const auto channel = frame.getChannel(channels[channelIndex]);
        ASSERT_TRUE(channel);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(channel->data()[pixel], original[channelIndex][pixel])
                << "channel=" << channelIndex << " pixel=" << pixel;
        }
    }
}

TEST(CreativeImageEffectContractTest, PigmentSeparationGranulationIsDeterministicAtTimeAndVariesAcrossTime) {
    constexpr int width = 10;
    constexpr int height = 8;
    VideoFrame firstFrame(width, height);
    VideoFrame repeatFrame(width, height);
    VideoFrame laterFrame(width, height);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        const float value = 0.15f + static_cast<float>((pixel * 7) % 29) / 40.0f;
        for (VideoFrame* frame : {&firstFrame, &repeatFrame, &laterFrame}) {
            frame->getChannel(ChannelType::Red)->data()[pixel] = value;
            frame->getChannel(ChannelType::Green)->data()[pixel] = value * 0.8f;
            frame->getChannel(ChannelType::Blue)->data()[pixel] = value * 0.6f;
            frame->getChannel(ChannelType::Alpha)->data()[pixel] =
                static_cast<float>(pixel) / (width * height - 1);
        }
    }

    auto makeGrainEffect = [] {
        PigmentSeparationEffect effect;
        effect.setParameter("Spread", 0.0f);
        effect.setParameter("Bleed", 0.0f);
        effect.setParameter("Flow", 0.0f);
        effect.setParameter("Granulation", 1.0f);
        return effect;
    };
    auto firstEffect = makeGrainEffect();
    auto repeatEffect = makeGrainEffect();
    auto laterEffect = makeGrainEffect();
    firstEffect.process(firstFrame, CreativeEffectContext{.time = 2.0});
    repeatEffect.process(repeatFrame, CreativeEffectContext{.time = 2.0});
    laterEffect.process(laterFrame, CreativeEffectContext{.time = 22.0});

    bool timeChangedGrain = false;
    for (const ChannelType channelType : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
        const auto first = firstFrame.getChannel(channelType);
        const auto repeat = repeatFrame.getChannel(channelType);
        const auto later = laterFrame.getChannel(channelType);
        ASSERT_TRUE(first);
        ASSERT_TRUE(repeat);
        ASSERT_TRUE(later);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(first->data()[pixel], repeat->data()[pixel]);
            EXPECT_TRUE(std::isfinite(later->data()[pixel]));
            timeChangedGrain = timeChangedGrain ||
                std::abs(first->data()[pixel] - later->data()[pixel]) > 1.0e-6f;
        }
    }
    EXPECT_TRUE(timeChangedGrain);

    for (VideoFrame* frame : {&firstFrame, &repeatFrame, &laterFrame}) {
        const auto alpha = frame->getChannel(ChannelType::Alpha);
        ASSERT_TRUE(alpha);
        for (int pixel = 0; pixel < width * height; ++pixel) {
            EXPECT_FLOAT_EQ(alpha->data()[pixel],
                            static_cast<float>(pixel) / (width * height - 1));
        }
    }
}

TEST(CreativeImageEffectContractTest, PigmentSeparationRgbOutputDoesNotDependOnOptionalAlphaPlane) {
    constexpr int width = 8;
    constexpr int height = 6;
    VideoFrame withAlpha(width, height);
    VideoFrame withoutAlpha(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixel = y * width + x;
            const float red = static_cast<float>(x) / (width - 1);
            const float green = static_cast<float>(y) / (height - 1);
            const float blue = static_cast<float>((x + 2 * y) % width) / (width - 1);
            for (VideoFrame* frame : {&withAlpha, &withoutAlpha}) {
                frame->getChannel(ChannelType::Red)->data()[pixel] = red;
                frame->getChannel(ChannelType::Green)->data()[pixel] = green;
                frame->getChannel(ChannelType::Blue)->data()[pixel] = blue;
            }
            withAlpha.getChannel(ChannelType::Alpha)->data()[pixel] =
                static_cast<float>(pixel) / (width * height - 1);
        }
    }
    withoutAlpha.removeChannel(ChannelType::Alpha);

    PigmentSeparationEffect withAlphaEffect;
    withAlphaEffect.setParameter("Spread", 0.65f);
    withAlphaEffect.setParameter("Bleed", 0.55f);
    withAlphaEffect.setParameter("Flow", 0.7f);
    withAlphaEffect.setParameter("Granulation", 0.8f);
    PigmentSeparationEffect withoutAlphaEffect;
    withoutAlphaEffect.setParameter("Spread", 0.65f);
    withoutAlphaEffect.setParameter("Bleed", 0.55f);
    withoutAlphaEffect.setParameter("Flow", 0.7f);
    withoutAlphaEffect.setParameter("Granulation", 0.8f);
    const CreativeEffectContext context{.time = 3.25};
    withAlphaEffect.process(withAlpha, context);
    withoutAlphaEffect.process(withoutAlpha, context);

    bool changedRgb = false;
    for (const ChannelType channelType : {ChannelType::Red, ChannelType::Green,
                                          ChannelType::Blue}) {
        const auto expected = withAlpha.getChannel(channelType);
        const auto actual = withoutAlpha.getChannel(channelType);
        ASSERT_TRUE(expected);
        ASSERT_TRUE(actual);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int pixel = y * width + x;
                EXPECT_TRUE(std::isfinite(actual->data()[pixel]));
                EXPECT_FLOAT_EQ(actual->data()[pixel], expected->data()[pixel]);
                const float original = channelType == ChannelType::Red
                    ? static_cast<float>(x) / (width - 1)
                    : channelType == ChannelType::Green
                        ? static_cast<float>(y) / (height - 1)
                        : static_cast<float>((x + 2 * y) % width) / (width - 1);
                changedRgb = changedRgb ||
                    std::abs(actual->data()[pixel] - original) > 1.0e-6f;
            }
        }
    }
    EXPECT_TRUE(changedRgb);
    EXPECT_FALSE(withoutAlpha.getChannel(ChannelType::Alpha));
}

TEST(CreativeImageEffectContractTest, PigmentSeparationMissingRequiredColorChannelLeavesFrameUnchanged) {
    VideoFrame frame(2, 1);
    const float red[] = {0.2f, 0.7f};
    const float green[] = {0.4f, 0.3f};
    const float alpha[] = {0.1f, 0.9f};
    for (int pixel = 0; pixel < 2; ++pixel) {
        frame.getChannel(ChannelType::Red)->data()[pixel] = red[pixel];
        frame.getChannel(ChannelType::Green)->data()[pixel] = green[pixel];
        frame.getChannel(ChannelType::Alpha)->data()[pixel] = alpha[pixel];
    }
    frame.removeChannel(ChannelType::Blue);

    PigmentSeparationEffect effect;
    effect.setParameter("Spread", 1.0f);
    effect.setParameter("Bleed", 1.0f);
    effect.setParameter("Flow", 1.0f);
    effect.setParameter("Granulation", 1.0f);
    effect.process(frame, CreativeEffectContext{.time = 6.0});

    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[0], red[0]);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Red)->data()[1], red[1]);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[0], green[0]);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Green)->data()[1], green[1]);
    EXPECT_FALSE(frame.getChannel(ChannelType::Blue));
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[0], alpha[0]);
    EXPECT_FLOAT_EQ(frame.getChannel(ChannelType::Alpha)->data()[1], alpha[1]);
}
