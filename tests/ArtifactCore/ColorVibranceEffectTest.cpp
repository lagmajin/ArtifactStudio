#include <gtest/gtest.h>
#include <initializer_list>
#include <cmath>

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

TEST(CreativeImageEffectContractTest, EdgeEchoLightPressureAndOldTvHaveExactNeutralNoOp) {
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

    EdgeEchoEffect edgeEcho;
    edgeEcho.setParameter("Intensity", 0.0f);
    edgeEcho.setParameter("Tint", 0.0f);
    edgeEcho.process(frame, CreativeEffectContext{});
    LightPressureEffect lightPressure;
    lightPressure.setParameter("Pressure", 0.0f);
    lightPressure.setParameter("Bloom", 0.0f);
    lightPressure.setParameter("Spread", 0.0f);
    lightPressure.setParameter("Compression", 0.0f);
    lightPressure.process(frame, CreativeEffectContext{});
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
