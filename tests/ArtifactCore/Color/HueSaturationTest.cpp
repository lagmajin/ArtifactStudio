#include <gtest/gtest.h>
#include <QImage>
#include <QColor>

import ImageProcessing.ColorTransform.HueSaturation;
import ImageProcessing.ColorTransform.UIMapping;

using namespace ArtifactCore;

class HueSaturationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // テスト用画像作成 (100x100 RGB32)
        testImage = QImage(100, 100, QImage::Format_RGB32);
        testImage.fill(QColor(128, 64, 192));  // テスト色

        // アルファ付き画像
        testImageAlpha = QImage(100, 100, QImage::Format_ARGB32);
        testImageAlpha.fill(QColor(128, 64, 192, 128));  // 半透明
    }

    QImage testImage;
    QImage testImageAlpha;
};

TEST_F(HueSaturationTest, BasicApplyDoesNotCrash) {
    HueSaturationEffect effect;
    effect.setSettings(HueSaturationSettings::normal());

    QImage result = effect.apply(testImage);
    EXPECT_EQ(result.size(), testImage.size());
    EXPECT_EQ(result.format(), testImage.format());
}

TEST_F(HueSaturationTest, SrgbToLinearRoundtrip) {
    float r = 0.5f, g = 0.3f, b = 0.8f;

    // Linear → sRGB → Linear 変換で元の値に近似
    ColorConversion::LinearToSrgb(r, g, b);
    ColorConversion::SrgbToLinear(r, g, b);

    // 数値精度内で一致（浮動小数点誤差許容）
    EXPECT_NEAR(r, 0.5f, 0.01f);
    EXPECT_NEAR(g, 0.3f, 0.01f);
    EXPECT_NEAR(b, 0.8f, 0.01f);
}

TEST_F(HueSaturationTest, PremultipliedAlphaHandling) {
    HueSaturationSettings settings;
    settings.premultipliedAlpha = true;
    settings.masterSaturation = 50.0;  // 彩度+50%

    HueSaturationEffect effect;
    effect.setSettings(settings);

    QImage result = effect.apply(testImageAlpha);

    // 結果画像のサイズ・フォーマット確認
    EXPECT_EQ(result.size(), testImageAlpha.size());
    EXPECT_EQ(result.format(), testImageAlpha.format());

    // 中央ピクセルのアルファ値が維持されていることを確認
    QRgb originalPixel = testImageAlpha.pixel(50, 50);
    QRgb resultPixel = result.pixel(50, 50);
    EXPECT_EQ(qAlpha(originalPixel), qAlpha(resultPixel));
}

TEST_F(HueSaturationTest, HdrValuesPreservation) {
    HueSaturationEffect effect;

    // HDR値（>1.0）を設定
    float r = 1.5f, g = 0.8f, b = 2.0f;

    effect.applyPixelLinear(r, g, b);

    // HDR値が保持されている（>1.0）
    EXPECT_GT(r, 1.0f);
    EXPECT_LE(r, 3.0f);  // 極端な値になっていない
    EXPECT_GT(b, 1.0f);
}

TEST_F(HueSaturationTest, UIMappingFunctions) {
    // Hue回転マッピング
    EXPECT_NEAR(mapHueRotation(0), 0.0, 0.01);
    EXPECT_GT(mapHueRotation(180), 0.0);  // 正の値
    EXPECT_LT(mapHueRotation(-180), 0.0); // 負の値

    // Saturation調整マッピング
    EXPECT_NEAR(mapSaturationAdjustment(0), 0.0, 0.01);
    EXPECT_GT(mapSaturationAdjustment(100), 0.0);
    EXPECT_LT(mapSaturationAdjustment(-100), 0.0);

    // Lightness調整マッピング
    EXPECT_NEAR(mapLightnessAdjustment(0), 0.0, 0.01);
    EXPECT_GT(mapLightnessAdjustment(100), 0.0);
    EXPECT_LT(mapLightnessAdjustment(-100), 0.0);

    // Normalizedパラメータ
    EXPECT_NEAR(mapNormalizedParameter(0), 0.0, 0.01);
    EXPECT_NEAR(mapNormalizedParameter(100), 0.5, 0.01);
    EXPECT_NEAR(mapNormalizedParameter(200), 1.0, 0.01);
}

TEST_F(HueSaturationTest, SettingsPresets) {
    HueSaturationSettings normal = HueSaturationSettings::normal();
    EXPECT_EQ(normal.masterHue, 0.0);
    EXPECT_EQ(normal.masterSaturation, 0.0);
    EXPECT_EQ(normal.masterLightness, 0.0);

    HueSaturationSettings increaseSat = HueSaturationSettings::increaseSaturation();
    EXPECT_GT(increaseSat.masterSaturation, 0.0);

    HueSaturationSettings decreaseSat = HueSaturationSettings::decreaseSaturation();
    EXPECT_LT(decreaseSat.masterSaturation, 0.0);

    HueSaturationSettings colorize = HueSaturationSettings::colorize(120.0, 50.0);
    EXPECT_EQ(colorize.masterHue, 120.0);
    EXPECT_EQ(colorize.masterSaturation, 50.0);
    EXPECT_LT(colorize.masterLightness, 0.0);  // 少し暗く
}

TEST_F(HueSaturationTest, ChannelAdjustments) {
    HueSaturationSettings settings;
    settings.redSaturation = 20.0;
    settings.blueLightness = -10.0;

    HueSaturationEffect effect;
    effect.setSettings(settings);

    float r = 0.8f, g = 0.5f, b = 0.3f;  // Red寄りの色
    effect.applyPixelLinear(r, g, b);

    // Redチャンネル調整により彩度が増加（他のチャンネルより）
    // 具体的な数値はHSL変換によるので、変化があることを確認
    EXPECT_NE(r, 0.8f);  // 何らかの変化がある
    EXPECT_NE(g, 0.5f);
    EXPECT_NE(b, 0.3f);
}