#include <gtest/gtest.h>

#include <array>
#include <cmath>

import StarfieldGenerator;

using namespace ArtifactCore;

TEST(StarfieldSpectralContractTest, MainSequenceTemperatureMatchesEachSpectralClass)
{
    constexpr std::array<float, 7> expected = {
        40000.0f, 20000.0f, 8500.0f, 6500.0f, 5700.0f, 4500.0f, 3000.0f};

    for (int index = 0; index < static_cast<int>(expected.size()); ++index) {
        const auto type = static_cast<SpectralType>(index);
        EXPECT_FLOAT_EQ(StarfieldGenerator::spectralTemperature(type, LuminosityClass::V),
                       expected[static_cast<std::size_t>(index)]);
    }
}

TEST(StarfieldSpectralContractTest, LuminosityClassAppliesDocumentedTemperatureModifiers)
{
    constexpr std::array<float, 8> expected = {
        36000.0f, 36000.0f, 36000.0f, 38000.0f,
        40000.0f, 40000.0f, 42000.0f, 42000.0f};

    for (int index = 0; index < static_cast<int>(expected.size()); ++index) {
        const auto luminosity = static_cast<LuminosityClass>(index);
        EXPECT_FLOAT_EQ(StarfieldGenerator::spectralTemperature(SpectralType::O, luminosity),
                       expected[static_cast<std::size_t>(index)]);
    }
}

TEST(StarfieldSpectralContractTest, AbsoluteMagnitudeCombinesSpectralAndLuminosityOffsets)
{
    constexpr std::array<float, 7> mainSequence = {
        -5.0f, -1.5f, 1.5f, 3.0f, 4.8f, 6.5f, 10.0f};
    constexpr std::array<float, 8> luminosityOffset = {
        -7.0f, -5.5f, -3.0f, -0.5f, 0.0f, 0.0f, 2.0f, 10.0f};

    for (int spectral = 0; spectral < static_cast<int>(mainSequence.size()); ++spectral) {
        for (int luminosity = 0; luminosity < static_cast<int>(luminosityOffset.size()); ++luminosity) {
            const float actual = StarfieldGenerator::absoluteMagnitude(
                static_cast<SpectralType>(spectral), static_cast<LuminosityClass>(luminosity));
            EXPECT_FLOAT_EQ(actual, mainSequence[static_cast<std::size_t>(spectral)] +
                                        luminosityOffset[static_cast<std::size_t>(luminosity)]);
        }
    }
}

TEST(StarfieldSpectralContractTest, SpectralRgbProducesFiniteBoundedClassSpecificColors)
{
    std::array<std::array<float, 3>, 7> colors{};
    for (int index = 0; index < static_cast<int>(colors.size()); ++index) {
        auto& color = colors[static_cast<std::size_t>(index)];
        StarfieldGenerator::spectralToRgb(static_cast<SpectralType>(index), LuminosityClass::V,
                                          color[0], color[1], color[2]);
        for (const float channel : color) {
            EXPECT_TRUE(std::isfinite(channel));
            EXPECT_GE(channel, 0.0f);
            EXPECT_LE(channel, 1.0f);
        }
    }

    EXPECT_GT(colors[0][2], colors[0][0]);
    EXPECT_GT(colors[6][0], colors[6][2]);
    EXPECT_NE(colors[0], colors[6]);
}

TEST(StarfieldSpectralContractTest, InvalidEnumValuesClampToNearestSupportedClass)
{
    EXPECT_FLOAT_EQ(StarfieldGenerator::spectralTemperature(
                        static_cast<SpectralType>(-1), LuminosityClass::V),
                    StarfieldGenerator::spectralTemperature(SpectralType::O, LuminosityClass::V));
    EXPECT_FLOAT_EQ(StarfieldGenerator::spectralTemperature(
                        static_cast<SpectralType>(99), LuminosityClass::V),
                    StarfieldGenerator::spectralTemperature(SpectralType::M, LuminosityClass::V));
    EXPECT_FLOAT_EQ(StarfieldGenerator::spectralTemperature(
                        SpectralType::O, static_cast<LuminosityClass>(99)),
                    StarfieldGenerator::spectralTemperature(SpectralType::O, LuminosityClass::VII));
    EXPECT_FLOAT_EQ(StarfieldGenerator::absoluteMagnitude(
                        static_cast<SpectralType>(-1), static_cast<LuminosityClass>(-1)), -12.0f);
}
