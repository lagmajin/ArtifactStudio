#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <atomic>
#include <crtdbg.h>
#endif
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <QString>

import Color.AutoMatch;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4_RGBA;

namespace {

using ArtifactCore::AutoColorMatcher;
using ArtifactCore::ImageF32x4_RGBA;
using ArtifactCore::SurfaceAlphaMode;
using ArtifactCore::SurfaceColorDescriptor;

#if defined(_MSC_VER) && defined(_DEBUG)
std::atomic_size_t g_coreAllocationCount{0};
std::atomic_size_t g_coreAllocatedBytes{0};

int __cdecl countCoreAllocation(int allocationType, void*, std::size_t size,
                                int, long, unsigned char const*, int) {
    if (allocationType == _HOOK_ALLOC || allocationType == _HOOK_REALLOC) {
        g_coreAllocationCount.fetch_add(1, std::memory_order_relaxed);
        g_coreAllocatedBytes.fetch_add(size, std::memory_order_relaxed);
    }
    return 1;
}

class CoreCrtAllocationCounter {
public:
    CoreCrtAllocationCounter() {
        g_coreAllocationCount.store(0, std::memory_order_relaxed);
        g_coreAllocatedBytes.store(0, std::memory_order_relaxed);
        previousHook_ = _CrtSetAllocHook(countCoreAllocation);
    }

    ~CoreCrtAllocationCounter() { stop(); }

    std::pair<std::size_t, std::size_t> stop() {
        if (active_) {
            _CrtSetAllocHook(previousHook_);
            active_ = false;
        }
        return {g_coreAllocationCount.load(std::memory_order_relaxed),
                g_coreAllocatedBytes.load(std::memory_order_relaxed)};
    }

private:
    _CRT_ALLOC_HOOK previousHook_ = nullptr;
    bool active_ = true;
};
#endif

auto sourcePixels() -> std::array<float, 8>
{
    // The public float-buffer contract stores channels as BGRA.
    return {0.0f, 0.0f, 0.0f, 0.2f,
            1.0f, 2.0f, 1.0f, 0.8f};
}

constexpr std::array<float, 8> kReferencePixels{
    1.0f, 2.0f, 0.25f, 0.1f,
    3.0f, 6.0f, 0.75f, 0.9f,
};

} // namespace

TEST(AutoColorMatcherContractTest, ComputesLogicalRgbTransformsFromBgraBuffers)
{
    const auto source = sourcePixels();
    const auto result = AutoColorMatcher::computeMatch(
        source.data(), kReferencePixels.data(), 2, 1, 2, 1,
        AutoColorMatcher::Method::MeanStddev);

    EXPECT_NEAR(result.scaleR, 0.5f, 1.0e-6f);
    EXPECT_NEAR(result.scaleG, 2.0f, 1.0e-6f);
    EXPECT_NEAR(result.scaleB, 2.0f, 1.0e-6f);
    EXPECT_NEAR(result.offsetR, 0.25f, 1.0e-6f);
    EXPECT_NEAR(result.offsetG, 2.0f, 1.0e-6f);
    EXPECT_NEAR(result.offsetB, 1.0f, 1.0e-6f);
    EXPECT_NEAR(result.confidence, 1.0f / 6.0f, 1.0e-6f);

    auto transformed = source;
    AutoColorMatcher::applyMatch(transformed.data(), 2, 1, result);
    for (int pixel = 0; pixel < 2; ++pixel) {
        const int index = pixel * 4;
        EXPECT_NEAR(transformed[index], kReferencePixels[index], 1.0e-6f);
        EXPECT_NEAR(transformed[index + 1], kReferencePixels[index + 1], 1.0e-6f);
        EXPECT_NEAR(transformed[index + 2], kReferencePixels[index + 2], 1.0e-6f);
        EXPECT_FLOAT_EQ(transformed[index + 3], source[index + 3]);
    }
}

TEST(AutoColorMatcherContractTest, MeanStddevDispatchMatchesUnequalSizedReferenceAndPreservesAlpha)
{
    auto source = sourcePixels();
    const std::array<float, 16> reference{
        1.0f, 2.0f, 0.25f, 0.0f,
        1.0f, 2.0f, 0.25f, 0.2f,
        3.0f, 6.0f, 0.75f, 0.4f,
        3.0f, 6.0f, 0.75f, 0.6f,
    };

    AutoColorMatcher::meanStddevMatch(source.data(), 2, 1,
                                      reference.data(), 4, 1);
    for (int pixel = 0; pixel < 2; ++pixel) {
        const int index = pixel * 4;
        EXPECT_NEAR(source[index], kReferencePixels[index], 1.0e-6f);
        EXPECT_NEAR(source[index + 1], kReferencePixels[index + 1], 1.0e-6f);
        EXPECT_NEAR(source[index + 2], kReferencePixels[index + 2], 1.0e-6f);
        EXPECT_FLOAT_EQ(source[index + 3], sourcePixels()[index + 3]);
    }
}

TEST(AutoColorMatcherContractTest, HistogramMatchMapsQuantilesWithoutChangingAlpha)
{
    std::array<float, 8> source{
        0.0f, 0.0f, 0.0f, 0.3f,
        1.0f, 1.0f, 1.0f, 0.7f,
    };
    const std::array<float, 8> reference{
        0.25f, 0.25f, 0.25f, 0.1f,
        0.75f, 0.75f, 0.75f, 0.9f,
    };

    AutoColorMatcher::histogramMatch(source.data(), 2, 1,
                                     reference.data(), 2, 1);
    for (int pixel = 0; pixel < 2; ++pixel) {
        const int index = pixel * 4;
        for (int channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(source[index + channel], reference[index + channel],
                        1.0f / 255.0f);
        }
        EXPECT_FLOAT_EQ(source[index + 3], pixel == 0 ? 0.3f : 0.7f);
    }
}

TEST(AutoColorMatcherContractTest, ZeroIntensityAndInvalidDimensionsLeaveSourceUntouched)
{
    const auto original = sourcePixels();
    constexpr std::array methods{
        AutoColorMatcher::Method::Reinhard,
        AutoColorMatcher::Method::MeanStddev,
        AutoColorMatcher::Method::Histogram,
    };
    for (const auto method : methods) {
        auto source = original;
        AutoColorMatcher::match(source.data(), kReferencePixels.data(),
                                2, 1, method, 0.0f);
        EXPECT_EQ(source, original);
    }

    auto invalidSource = original;
    AutoColorMatcher::match(invalidSource.data(), kReferencePixels.data(),
                            0, 1, AutoColorMatcher::Method::MeanStddev);
    EXPECT_EQ(invalidSource, original);
    AutoColorMatcher::match(nullptr, kReferencePixels.data(),
                            2, 1, AutoColorMatcher::Method::Histogram);
    AutoColorMatcher::applyMatch(nullptr, 2, 1, {});
}

TEST(AutoColorMatcherContractTest, IntensityBlendsPerChannelMatchAndLeavesAlphaAlone)
{
    auto pixels = sourcePixels();
    const AutoColorMatcher::MatchResult match{
        .scaleR = 2.0f, .scaleG = 3.0f, .scaleB = 4.0f,
        .offsetR = 0.1f, .offsetG = 0.2f, .offsetB = 0.3f,
    };
    AutoColorMatcher::applyMatch(pixels.data(), 2, 1, match, 0.5f);

    EXPECT_NEAR(pixels[0], 0.15f, 1.0e-6f);
    EXPECT_NEAR(pixels[1], 0.1f, 1.0e-6f);
    EXPECT_NEAR(pixels[2], 0.05f, 1.0e-6f);
    EXPECT_FLOAT_EQ(pixels[3], 0.2f);
    EXPECT_NEAR(pixels[4], 2.65f, 1.0e-6f);
    EXPECT_NEAR(pixels[5], 4.1f, 1.0e-6f);
    EXPECT_NEAR(pixels[6], 1.55f, 1.0e-6f);
    EXPECT_FLOAT_EQ(pixels[7], 0.8f);
}

TEST(AutoColorMatcherContractTest,
     ImageOverloadMatchesRawBufferAndEquationMirrorForTaggedDescriptors)
{
    constexpr std::array<float, 24> source{
        0.04f, 0.16f, 0.28f, 0.00f,
        0.22f, 0.31f, 0.43f, 0.25f,
        0.76f, 0.55f, 0.37f, 0.50f,
        0.91f, 0.84f, 0.73f, 0.75f,
        0.13f, 0.67f, 0.48f, 1.00f,
        0.58f, 0.24f, 0.82f, 0.40f,
    };
    constexpr std::array<float, 24> reference{
        0.11f, 0.20f, 0.34f, 0.10f,
        0.30f, 0.38f, 0.49f, 0.20f,
        0.69f, 0.58f, 0.47f, 0.30f,
        0.88f, 0.86f, 0.81f, 0.40f,
        0.20f, 0.63f, 0.52f, 0.50f,
        0.51f, 0.33f, 0.72f, 0.60f,
    };
    constexpr std::array methods{
        AutoColorMatcher::Method::Reinhard,
        AutoColorMatcher::Method::MeanStddev,
        AutoColorMatcher::Method::Histogram,
    };
    constexpr std::array intensities{
        0.25f, 0.5f, 1.0f};
    constexpr std::array<std::array<float, 18>, 3> mirrorOutputAtFullStrength{{
        {0.31380519f, 0.20766932f, 0.09308767f,
         0.45436186f, 0.34856984f, 0.25186837f,
         0.48098001f, 0.56410635f, 0.69243681f,
         0.78061193f, 0.83244199f, 0.85330194f,
         0.56193405f, 0.66861260f, 0.26031235f,
         0.77050108f, 0.35968366f, 0.54740864f},
        {0.36254707f, 0.22677861f, 0.11607682f,
         0.48576921f, 0.36097708f, 0.26559225f,
         0.43648034f, 0.57569468f, 0.71413857f,
         0.73221344f, 0.83514506f, 0.83873475f,
         0.52684325f, 0.68305343f, 0.19083454f,
         0.80614668f, 0.29835114f, 0.56462312f},
        {0.33725491f, 0.20000000f, 0.10980392f,
         0.48627451f, 0.37647060f, 0.29803923f,
         0.46666667f, 0.57647061f, 0.68627453f,
         0.71764708f, 0.85882354f, 0.87843138f,
         0.51764709f, 0.62745100f, 0.20000000f,
         0.80784315f, 0.32941177f, 0.50980395f},
    }};
    constexpr std::array mirrorTolerances{
        2.0e-4f, 2.0e-6f, 1.0f / 255.0f,
    };
    const std::array descriptors{
        SurfaceColorDescriptor::legacyOpenCvBgra32Float(),
        SurfaceColorDescriptor::linearStraightBgra32Float(),
    };

    for (std::size_t methodIndex = 0; methodIndex < methods.size(); ++methodIndex) {
        const auto method = methods[methodIndex];
        for (const float intensity : intensities) {
            std::array<float, source.size()> srgbDescriptorOutput{};
            for (std::size_t descriptorIndex = 0;
                 descriptorIndex < descriptors.size(); ++descriptorIndex) {
                const auto& descriptor = descriptors[descriptorIndex];
                ImageF32x4_RGBA sourceImage;
                ImageF32x4_RGBA referenceImage;
                sourceImage.setFromBGRA32F(source.data(), 3, 2, descriptor);
                referenceImage.setFromBGRA32F(reference.data(), 3, 2, descriptor);

                EXPECT_EQ(sourceImage.colorDescriptor().transfer, descriptor.transfer);
                EXPECT_EQ(sourceImage.colorDescriptor().primaries, descriptor.primaries);

                AutoColorMatcher::match(sourceImage, referenceImage, method, intensity);

                auto rawOutput = source;
                AutoColorMatcher::match(rawOutput.data(), reference.data(),
                                        3, 2, method, intensity);
                const float* imageOutput = sourceImage.rgba32fData();
                for (std::size_t i = 0; i < source.size(); ++i) {
                    EXPECT_FLOAT_EQ(imageOutput[i], rawOutput[i]);
                    if (i % 4u == 3u) {
                        EXPECT_FLOAT_EQ(imageOutput[i], source[i]);
                    } else if (intensity == 1.0f) {
                        const std::size_t pixel = i / 4u;
                        const std::size_t logicalChannel = 2u - (i % 4u);
                        EXPECT_NEAR(
                            imageOutput[i],
                            mirrorOutputAtFullStrength[methodIndex][pixel * 3u + logicalChannel],
                            mirrorTolerances[methodIndex]);
                    }
                    if (descriptorIndex == 0) {
                        srgbDescriptorOutput[i] = imageOutput[i];
                    } else {
                        // The current image overload forwards raw pixels; this
                        // records that descriptor metadata is not interpreted.
                        EXPECT_FLOAT_EQ(imageOutput[i], srgbDescriptorOutput[i]);
                    }
                }
            }
        }
    }
}

TEST(AutoColorMatcherContractTest, CoreImageOverloadMatchesMiddleburyColorCheckerMirrorPreviews)
{
    const auto repoRoot = std::filesystem::path(__FILE__)
                              .parent_path()
                              .parent_path()
                              .parent_path();
    const auto assetDirectory = repoRoot / "experiments" / "ColorMatchLab" /
                                "assets" / "MiddleburyColorChecker24" /
                                "CanonEOS20D";
    const auto captureDirectory = repoRoot / "experiments" / "ColorMatchLab" /
                                  "captures" / "rd6_colorchecker_pair";
    const auto sourcePath = assetDirectory / "wb1i2e3-jpg.png";
    const auto referencePath = assetDirectory / "wb2i2e3-jpg.png";
    if (!std::filesystem::exists(sourcePath) ||
        !std::filesystem::exists(referencePath) ||
        !std::filesystem::exists(captureDirectory / "Reinhard_i0.25.png")) {
        GTEST_SKIP() << "The licensed Middlebury R&D pair and RD6 captures are not installed";
    }

    ImageF32x4_RGBA sourceImage;
    ImageF32x4_RGBA referenceImage;
    ASSERT_TRUE(sourceImage.load(QString::fromStdString(sourcePath.string())));
    ASSERT_TRUE(referenceImage.load(QString::fromStdString(referencePath.string())));
    ASSERT_EQ(sourceImage.width(), 606);
    ASSERT_EQ(sourceImage.height(), 404);
    ASSERT_EQ(referenceImage.width(), sourceImage.width());
    ASSERT_EQ(referenceImage.height(), sourceImage.height());
    const auto logDescriptor = [](const char* label,
                                  const SurfaceColorDescriptor& descriptor) {
        std::cout << "RD24_INPUT_DESCRIPTOR," << label
                  << ",transfer_known=" << descriptor.transferKnown
                  << ",transfer=" << static_cast<unsigned>(descriptor.transfer)
                  << ",primaries=" << static_cast<unsigned>(descriptor.primaries)
                  << ",alpha_mode=" << static_cast<unsigned>(descriptor.alphaMode)
                  << ",range=" << static_cast<unsigned>(descriptor.range) << '\n';
    };
    logDescriptor("source", sourceImage.colorDescriptor());
    logDescriptor("reference", referenceImage.colorDescriptor());

    constexpr std::array methods{
        AutoColorMatcher::Method::Reinhard,
        AutoColorMatcher::Method::MeanStddev,
        AutoColorMatcher::Method::Histogram,
    };
    constexpr std::array methodNames{"Reinhard", "MeanStddev", "Histogram"};
    constexpr std::array intensities{0.25f, 0.5f, 1.0f};
    constexpr float kEncodedPreviewAndMirrorTolerance = 1.5f / 255.0f;
    struct ChartRegion {
        const char* name;
        std::array<std::array<int, 2>, 5> patchOrigins;
        std::size_t patchCount;
    };
    constexpr std::array chartRegions{
        ChartRegion{"skin_tone_proxy", {{{20, 20}, {121, 20}, {}, {}, {}}}, 2},
        ChartRegion{"white_patch", {{{20, 323}, {}, {}, {}, {}}}, 1},
        ChartRegion{"neutral_gray", {{{121, 323}, {222, 323}, {323, 323},
                                       {424, 323}, {525, 323}}}, 5},
    };
    const auto coreCaptureDirectory = repoRoot / "experiments" / "ColorMatchLab" /
                                      "captures" / "rd24_colorchecker_core";
    std::filesystem::create_directories(coreCaptureDirectory);
    ASSERT_TRUE(sourceImage.save(QString::fromStdString(
        (coreCaptureDirectory / "source_wb1.png").string())));
    ASSERT_TRUE(referenceImage.save(QString::fromStdString(
        (coreCaptureDirectory / "reference_wb2.png").string())));
    std::ofstream coreMetrics(coreCaptureDirectory / "core_patch_metrics.csv",
                              std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(coreMetrics.is_open());
    coreMetrics << "method,intensity,region,pixels,source_mean_delta_e76,"
                   "source_p95_delta_e76,source_max_delta_e76,source_pct_over_2,"
                   "wb2_mean_delta_e76,wb2_p95_delta_e76,wb2_max_delta_e76,"
                   "source_mean_L,source_mean_a,source_mean_b,source_mean_Cstar,"
                   "wb2_mean_L,wb2_mean_a,wb2_mean_b,wb2_mean_Cstar,"
                   "output_mean_L,output_mean_a,output_mean_b,output_mean_Cstar\n";
    coreMetrics << std::setprecision(10);
    const auto toLab = [](float red, float green, float blue) {
        const auto decode = [](float value) -> double {
            const double v = std::clamp(static_cast<double>(value), 0.0, 1.0);
            return v > 0.04045 ? std::pow((v + 0.055) / 1.055, 2.4) : v / 12.92;
        };
        const double r = decode(red), g = decode(green), b = decode(blue);
        const double x = (0.4124564 * r + 0.3575761 * g + 0.1804375 * b) / 0.95047;
        const double y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
        const double z = (0.0193339 * r + 0.1191920 * g + 0.9503041 * b) / 1.08883;
        const auto f = [](double value) {
            return value > 0.008856 ? std::cbrt(value) : 7.787 * value + 16.0 / 116.0;
        };
        const double fx = f(x), fy = f(y), fz = f(z);
        return std::array<double, 3>{116.0 * fy - 16.0,
                                     500.0 * (fx - fy),
                                     200.0 * (fy - fz)};
    };
    const auto deltaE = [](const std::array<double, 3>& lhs,
                           const std::array<double, 3>& rhs) {
        const double d0 = lhs[0] - rhs[0];
        const double d1 = lhs[1] - rhs[1];
        const double d2 = lhs[2] - rhs[2];
        return std::sqrt(d0 * d0 + d1 * d1 + d2 * d2);
    };
    auto summarize = [](std::vector<double>& values) {
        std::sort(values.begin(), values.end());
        const auto p95Index = static_cast<std::size_t>(
            std::ceil(0.95 * static_cast<double>(values.size()))) - 1u;
        return std::array<double, 3>{
            std::accumulate(values.begin(), values.end(), 0.0) /
                static_cast<double>(values.size()),
            values[p95Index], values.back()};
    };

    for (std::size_t methodIndex = 0; methodIndex < methods.size(); ++methodIndex) {
        for (const float intensity : intensities) {
            SCOPED_TRACE(::testing::Message() << methodNames[methodIndex]
                                              << " intensity " << intensity);
            ImageF32x4_RGBA coreOutput(sourceImage);
            AutoColorMatcher::match(coreOutput, referenceImage,
                                    methods[methodIndex], intensity);

            const auto previewName = std::string(methodNames[methodIndex]) +
                                     "_i" +
                                     (intensity == 0.25f ? "0.25.png" :
                                      intensity == 0.5f ? "0.50.png" : "1.00.png");
            ImageF32x4_RGBA mirrorPreview;
            ASSERT_TRUE(mirrorPreview.load(
                QString::fromStdString((captureDirectory / previewName).string())))
                << "Could not load RD6 source-mirror preview " << previewName;
            ASSERT_EQ(mirrorPreview.width(), coreOutput.width());
            ASSERT_EQ(mirrorPreview.height(), coreOutput.height());

            const float* actualPixels = coreOutput.rgba32fData();
            const float* encodedPreviewPixels = mirrorPreview.rgba32fData();
            const float* sourcePixels = sourceImage.rgba32fData();
            float maxRgbAbsDifference = 0.0f;
            for (std::size_t pixel = 0; pixel < sourceImage.totalPixels(); ++pixel) {
                for (std::size_t channel = 0; channel < 3; ++channel) {
                    const auto index = pixel * 4u + channel;
                    const std::size_t logicalChannel = 2u - channel;
                    const float linear = std::clamp(
                        sourcePixels[index], 0.0f, 1.0f);
                    const float sourceSrgb = linear <= 0.0031308f
                        ? 12.92f * linear
                        : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
                    const float expected = encodedPreviewPixels[index];
                    (void)logicalChannel;
                    (void)sourceSrgb;
                    maxRgbAbsDifference = std::max(
                        maxRgbAbsDifference,
                        std::abs(actualPixels[index] - expected));
                }
                EXPECT_FLOAT_EQ(actualPixels[pixel * 4u + 3u], 1.0f);
            }

            EXPECT_LE(maxRgbAbsDifference, kEncodedPreviewAndMirrorTolerance)
                << methodNames[methodIndex] << " intensity " << intensity
                << " max RGB delta against the 8-bit Python mirror preview";

            if (intensity == 0.5f || intensity == 1.0f) {
                const auto outputName = std::string(methodNames[methodIndex]) + "_" +
                                        (intensity == 0.5f ? "0.5.png" : "1.0.png");
                ASSERT_TRUE(coreOutput.save(QString::fromStdString(
                    (coreCaptureDirectory / outputName).string())));
            }
            for (const auto& region : chartRegions) {
                std::vector<double> sourceDeltas;
                std::vector<double> referenceDeltas;
                std::array<double, 3> sourceLabSum{};
                std::array<double, 3> outputLabSum{};
                std::array<double, 3> referenceLabSum{};
                std::size_t pixels = 0;
                for (std::size_t patch = 0; patch < region.patchCount; ++patch) {
                    const int originX = region.patchOrigins[patch][0];
                    const int originY = region.patchOrigins[patch][1];
                    for (int y = originY; y < originY + 60; ++y) {
                        for (int x = originX; x < originX + 60; ++x) {
                            const auto pixel = static_cast<std::size_t>(y) *
                                               static_cast<std::size_t>(sourceImage.width()) +
                                               static_cast<std::size_t>(x);
                            const auto index = pixel * 4u;
                            const auto sourceLab = toLab(sourcePixels[index + 2u],
                                sourcePixels[index + 1u], sourcePixels[index]);
                            const auto outputLab = toLab(actualPixels[index + 2u],
                                actualPixels[index + 1u], actualPixels[index]);
                            const auto referenceLab = toLab(
                                referenceImage.rgba32fData()[index + 2u],
                                referenceImage.rgba32fData()[index + 1u],
                                referenceImage.rgba32fData()[index]);
                            sourceDeltas.push_back(deltaE(sourceLab, outputLab));
                            referenceDeltas.push_back(deltaE(referenceLab, outputLab));
                            for (std::size_t channel = 0; channel < 3; ++channel) {
                                sourceLabSum[channel] += sourceLab[channel];
                                outputLabSum[channel] += outputLab[channel];
                                referenceLabSum[channel] += referenceLab[channel];
                            }
                            ++pixels;
                        }
                    }
                }
                const auto sourceSummary = summarize(sourceDeltas);
                const auto referenceSummary = summarize(referenceDeltas);
                const auto sourceL = sourceLabSum[0] / static_cast<double>(pixels);
                const auto sourceA = sourceLabSum[1] / static_cast<double>(pixels);
                const auto sourceB = sourceLabSum[2] / static_cast<double>(pixels);
                const auto referenceL = referenceLabSum[0] / static_cast<double>(pixels);
                const auto referenceA = referenceLabSum[1] / static_cast<double>(pixels);
                const auto referenceB = referenceLabSum[2] / static_cast<double>(pixels);
                const auto outputL = outputLabSum[0] / static_cast<double>(pixels);
                const auto outputA = outputLabSum[1] / static_cast<double>(pixels);
                const auto outputB = outputLabSum[2] / static_cast<double>(pixels);
                const auto overTwo = std::count_if(sourceDeltas.begin(), sourceDeltas.end(),
                    [](double value) { return value > 2.0; });
                coreMetrics << methodNames[methodIndex] << ',' << intensity << ','
                            << region.name << ',' << pixels << ','
                            << sourceSummary[0] << ',' << sourceSummary[1] << ','
                            << sourceSummary[2] << ','
                            << 100.0 * static_cast<double>(overTwo) /
                                   static_cast<double>(pixels) << ','
                            << referenceSummary[0] << ',' << referenceSummary[1] << ','
                            << referenceSummary[2] << ','
                            << sourceL << ',' << sourceA << ',' << sourceB << ','
                            << std::hypot(sourceA, sourceB) << ','
                            << referenceL << ',' << referenceA << ',' << referenceB << ','
                            << std::hypot(referenceA, referenceB) << ','
                            << outputL << ',' << outputA << ',' << outputB << ','
                            << std::hypot(outputA, outputB) << '\n';
            }
        }
    }
    coreMetrics.flush();
    ASSERT_TRUE(coreMetrics.good());
}


TEST(AutoColorMatcherContractTest,
     CoreImageOverloadMatchesMiddleburyNaturalSceneRawBuffer)
{
    const auto repoRoot = std::filesystem::path(__FILE__)
                              .parent_path()
                              .parent_path()
                              .parent_path();
    const auto captureDirectory = repoRoot / "experiments" / "ColorMatchLab" /
                                  "captures" / "rd7_natural_scene_pair";
    const auto sourceArrayPath = captureDirectory / "source_srgb_from_linear_raw.npy";
    const auto referenceArrayPath = captureDirectory / "reference_camera_jpeg_srgb.npy";
    if (!std::filesystem::exists(sourceArrayPath) ||
        !std::filesystem::exists(referenceArrayPath)) {
        GTEST_SKIP() << "The licensed Middlebury natural-scene pair and RD7 captures are not installed";
    }

    constexpr std::array methods{
        AutoColorMatcher::Method::Reinhard,
        AutoColorMatcher::Method::MeanStddev,
        AutoColorMatcher::Method::Histogram,
    };
    constexpr std::array methodNames{"Reinhard", "MeanStddev", "Histogram"};
    constexpr std::array intensities{0.25f, 0.5f, 1.0f};
    constexpr float kRawBufferTolerance = 1.0e-6f;
    auto loadRgbNpy = [](const std::filesystem::path& path, int width, int height) {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Could not open " + path.string());
        std::array<char, 128> header{};
        input.read(header.data(), static_cast<std::streamsize>(header.size()));
        if (!input || std::string_view(header.data(), 6) != std::string_view("\x93NUMPY", 6))
            throw std::runtime_error("Invalid NPY magic in " + path.string());
        const auto versionMajor = static_cast<unsigned char>(header[6]);
        const std::size_t headerLength = versionMajor == 1
            ? static_cast<unsigned char>(header[8]) |
                  (static_cast<std::size_t>(static_cast<unsigned char>(header[9])) << 8u)
            : static_cast<unsigned char>(header[8]) |
                  (static_cast<std::size_t>(static_cast<unsigned char>(header[9])) << 8u) |
                  (static_cast<std::size_t>(static_cast<unsigned char>(header[10])) << 16u) |
                  (static_cast<std::size_t>(static_cast<unsigned char>(header[11])) << 24u);
        const std::size_t prefixSize = versionMajor == 1 ? 10u : 12u;
        if (prefixSize + headerLength > header.size())
            throw std::runtime_error("NPY header too large in " + path.string());
        input.seekg(static_cast<std::streamoff>(prefixSize + headerLength), std::ios::beg);
        std::vector<float> rgb(static_cast<std::size_t>(width) *
                               static_cast<std::size_t>(height) * 3u);
        input.read(reinterpret_cast<char*>(rgb.data()),
                   static_cast<std::streamsize>(rgb.size() * sizeof(float)));
        if (!input) throw std::runtime_error("Incomplete NPY float32 payload in " + path.string());
        return rgb;
    };
    constexpr int width = 1000;
    constexpr int height = 667;
    const auto sourceRgb = loadRgbNpy(sourceArrayPath, width, height);
    const auto referenceRgb = loadRgbNpy(referenceArrayPath, width, height);
    ImageF32x4_RGBA sourceImage;
    ImageF32x4_RGBA referenceImage;
    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    std::vector<float> sourceBgra(pixelCount * 4u);
    std::vector<float> referenceBgra(pixelCount * 4u);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            sourceBgra[pixel * 4u + channel] = sourceRgb[pixel * 3u + (2u - channel)];
            referenceBgra[pixel * 4u + channel] = referenceRgb[pixel * 3u + (2u - channel)];
        }
        sourceBgra[pixel * 4u + 3u] = 1.0f;
        referenceBgra[pixel * 4u + 3u] = 1.0f;
    }
    sourceImage.setFromBGRA32F(sourceBgra.data(), width, height);
    referenceImage.setFromBGRA32F(referenceBgra.data(), width, height);

    const auto coreCaptureDirectory = repoRoot / "experiments" / "ColorMatchLab" /
                                      "captures" / "rd27_core_natural_scene";
    const auto roiDirectory = repoRoot / "experiments" / "ColorMatchLab" /
                              "captures" / "rd18_lamp_white_roi";
    std::filesystem::create_directories(coreCaptureDirectory);
    ImageF32x4_RGBA leftLampMask;
    ImageF32x4_RGBA rightLampMask;
    ASSERT_TRUE(leftLampMask.load(QString::fromStdString(
        (roiDirectory / "left_lampshade_mask.png").string())));
    ASSERT_TRUE(rightLampMask.load(QString::fromStdString(
        (roiDirectory / "right_lampshade_mask.png").string())));
    ASSERT_EQ(leftLampMask.width(), width);
    ASSERT_EQ(leftLampMask.height(), height);
    ASSERT_EQ(rightLampMask.width(), width);
    ASSERT_EQ(rightLampMask.height(), height);
    struct SceneRegion {
        const char* name;
        const ImageF32x4_RGBA* mask;
    };
    const std::array regions{
        SceneRegion{"whole_frame", nullptr},
        SceneRegion{"left_lampshade", &leftLampMask},
        SceneRegion{"right_lampshade", &rightLampMask},
    };
    std::ofstream metrics(coreCaptureDirectory / "core_roi_metrics.csv",
                          std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(metrics.is_open());
    std::ofstream mirrorDifferential(
        coreCaptureDirectory / "mirror_differential.csv",
        std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(mirrorDifferential.is_open());
    mirrorDifferential << "method,intensity,channels,mae,p95,max\n";
    mirrorDifferential << std::setprecision(10);
    metrics << "method,intensity,region,pixels,source_delta_e76_mean,"
               "source_delta_e76_p95,source_delta_e76_max,reference_delta_e76_mean,"
               "reference_delta_e76_p95,reference_delta_e76_max,rgb_mae,"
               "output_mean_Cstar\n";
    metrics << std::setprecision(10);
    ASSERT_TRUE(sourceImage.save(QString::fromStdString(
        (coreCaptureDirectory / "source.png").string())));
    ASSERT_TRUE(referenceImage.save(QString::fromStdString(
        (coreCaptureDirectory / "reference.png").string())));

    const auto toLab = [](float red, float green, float blue) {
        const auto decode = [](float value) -> double {
            const double v = std::clamp(static_cast<double>(value), 0.0, 1.0);
            return v > 0.04045 ? std::pow((v + 0.055) / 1.055, 2.4) : v / 12.92;
        };
        const double r = decode(red), g = decode(green), b = decode(blue);
        const double x = (0.4124564 * r + 0.3575761 * g + 0.1804375 * b) / 0.95047;
        const double y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
        const double z = (0.0193339 * r + 0.1191920 * g + 0.9503041 * b) / 1.08883;
        const auto f = [](double value) {
            return value > 0.008856 ? std::cbrt(value) : 7.787 * value + 16.0 / 116.0;
        };
        const double fx = f(x), fy = f(y), fz = f(z);
        return std::array<double, 3>{116.0 * fy - 16.0,
                                     500.0 * (fx - fy),
                                     200.0 * (fy - fz)};
    };
    const auto deltaE = [](const std::array<double, 3>& lhs,
                           const std::array<double, 3>& rhs) {
        const double d0 = lhs[0] - rhs[0];
        const double d1 = lhs[1] - rhs[1];
        const double d2 = lhs[2] - rhs[2];
        return std::sqrt(d0 * d0 + d1 * d1 + d2 * d2);
    };
    const auto summarize = [](std::vector<double>& values) {
        std::sort(values.begin(), values.end());
        const auto p95Index = static_cast<std::size_t>(
            std::ceil(0.95 * static_cast<double>(values.size()))) - 1u;
        return std::array<double, 3>{
            std::accumulate(values.begin(), values.end(), 0.0) /
                static_cast<double>(values.size()),
            values[p95Index], values.back()};
    };

    for (std::size_t methodIndex = 0; methodIndex < methods.size(); ++methodIndex) {
        for (const float intensity : intensities) {
            ImageF32x4_RGBA coreOutput(sourceImage);
            AutoColorMatcher::match(coreOutput, referenceImage,
                                    methods[methodIndex], intensity);
            const float* actualPixels = coreOutput.rgba32fData();
            std::vector<float> expected(pixelCount * 4u);
            std::copy(sourceBgra.begin(), sourceBgra.end(), expected.begin());
            AutoColorMatcher::match(expected.data(), referenceBgra.data(),
                                    width, height,
                                    methods[methodIndex], intensity);
            float maxRgbAbsDifference = 0.0f;
            for (std::size_t pixel = 0; pixel < sourceImage.totalPixels(); ++pixel) {
                for (std::size_t channel = 0; channel < 3; ++channel) {
                    const auto index = pixel * 4u + channel;
                    maxRgbAbsDifference = std::max(
                        maxRgbAbsDifference,
                        std::abs(actualPixels[index] - expected[index]));
                }
                EXPECT_FLOAT_EQ(actualPixels[pixel * 4u + 3u],
                                sourceBgra[pixel * 4u + 3u]);
            }
            EXPECT_LE(maxRgbAbsDifference, kRawBufferTolerance)
                << "method index " << methodIndex << " intensity " << intensity
                << " max RGB delta between image overload and raw-buffer Core API";

            const auto intensityName = intensity == 0.25f ? "0.25" :
                                       intensity == 0.5f ? "0.50" : "1.00";
            const auto mirrorPath = captureDirectory /
                (std::string(methodNames[methodIndex]) + "_i" + intensityName + ".npy");
            const auto mirrorRgb = loadRgbNpy(mirrorPath, width, height);
            std::vector<double> mirrorDifferences;
            mirrorDifferences.reserve(pixelCount * 3u);
            for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
                for (std::size_t channel = 0; channel < 3u; ++channel) {
                    const float mirrorValue = mirrorRgb[pixel * 3u + (2u - channel)];
                    mirrorDifferences.push_back(std::abs(
                        static_cast<double>(actualPixels[pixel * 4u + channel]) -
                        static_cast<double>(mirrorValue)));
                }
            }
            const auto mirrorSummary = summarize(mirrorDifferences);
            mirrorDifferential << methodNames[methodIndex] << ',' << intensity << ','
                               << mirrorDifferences.size() << ',' << mirrorSummary[0]
                               << ',' << mirrorSummary[1] << ',' << mirrorSummary[2]
                               << '\n';

            for (const auto& region : regions) {
                std::vector<double> sourceDeltas;
                std::vector<double> referenceDeltas;
                sourceDeltas.reserve(region.mask ? 2048u : pixelCount);
                referenceDeltas.reserve(region.mask ? 2048u : pixelCount);
                std::array<double, 3> outputLabSum{};
                double rgbAbsoluteSum = 0.0;
                std::size_t regionPixels = 0;
                const float* sourcePixels = sourceImage.rgba32fData();
                const float* referencePixels = referenceImage.rgba32fData();
                const float* maskPixels = region.mask
                    ? region.mask->rgba32fData() : nullptr;
                for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
                    const std::size_t index = pixel * 4u;
                    if (maskPixels && maskPixels[index] < 0.5f) continue;
                    const auto sourceLab = toLab(sourcePixels[index + 2u],
                        sourcePixels[index + 1u], sourcePixels[index]);
                    const auto referenceLab = toLab(referencePixels[index + 2u],
                        referencePixels[index + 1u], referencePixels[index]);
                    const auto outputLab = toLab(actualPixels[index + 2u],
                        actualPixels[index + 1u], actualPixels[index]);
                    sourceDeltas.push_back(deltaE(sourceLab, outputLab));
                    referenceDeltas.push_back(deltaE(referenceLab, outputLab));
                    for (std::size_t channel = 0; channel < 3u; ++channel) {
                        outputLabSum[channel] += outputLab[channel];
                        rgbAbsoluteSum += std::abs(
                            static_cast<double>(actualPixels[index + channel]) -
                            static_cast<double>(referencePixels[index + channel]));
                    }
                    ++regionPixels;
                }
                ASSERT_GT(regionPixels, 0u) << region.name;
                const auto sourceSummary = summarize(sourceDeltas);
                const auto referenceSummary = summarize(referenceDeltas);
                const double outputA = outputLabSum[1] /
                                       static_cast<double>(regionPixels);
                const double outputB = outputLabSum[2] /
                                       static_cast<double>(regionPixels);
                metrics << methodNames[methodIndex] << ',' << intensity << ','
                        << region.name << ',' << regionPixels << ','
                        << sourceSummary[0] << ',' << sourceSummary[1] << ','
                        << sourceSummary[2] << ',' << referenceSummary[0] << ','
                        << referenceSummary[1] << ',' << referenceSummary[2] << ','
                        << rgbAbsoluteSum / (static_cast<double>(regionPixels) * 3.0)
                        << ',' << std::hypot(outputA, outputB) << '\n';
            }
            if (intensity == 1.0f) {
                const auto outputPath = coreCaptureDirectory /
                    (std::string(methodNames[methodIndex]) + "_i1.0.png");
                ASSERT_TRUE(coreOutput.save(QString::fromStdString(outputPath.string())));
            }
        }
    }
    metrics.flush();
    ASSERT_TRUE(metrics.good());
    mirrorDifferential.flush();
    ASSERT_TRUE(mirrorDifferential.good());
}

TEST(AutoColorMatcherContractTest,
     CoreImageOverloadMeasuresTransparentHiddenRgbInfluence)
{
    const auto repoRoot = std::filesystem::path(__FILE__)
                              .parent_path()
                              .parent_path()
                              .parent_path();
    const auto captureDirectory = repoRoot / "experiments" / "ColorMatchLab" /
                                  "captures" / "rd13_transparent_hidden_rgb";
    const auto blackPath = captureDirectory / "source_black_hidden_rgba.npy";
    const auto magentaPath = captureDirectory / "source_magenta_hidden_rgba.npy";
    const auto targetPath = captureDirectory / "constructed_target_rgb.npy";
    if (!std::filesystem::exists(blackPath) ||
        !std::filesystem::exists(magentaPath) ||
        !std::filesystem::exists(targetPath)) {
        GTEST_SKIP() << "RD13 synthetic transparent portrait fixture is not installed";
    }

    constexpr int width = 1280;
    constexpr int height = 851;
    constexpr std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    constexpr std::size_t transparentWidth = width / 5;
    constexpr std::size_t visiblePixelCount =
        pixelCount - transparentWidth * static_cast<std::size_t>(height);
    const auto loadFloatNpy = [](const std::filesystem::path& path,
                                 std::size_t valueCount) {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Could not open " + path.string());
        std::array<char, 128> header{};
        input.read(header.data(), static_cast<std::streamsize>(header.size()));
        if (!input || std::string_view(header.data(), 6) !=
                          std::string_view("\x93NUMPY", 6)) {
            throw std::runtime_error("Invalid NPY magic in " + path.string());
        }
        const auto versionMajor = static_cast<unsigned char>(header[6]);
        const std::size_t headerLength = versionMajor == 1
            ? static_cast<unsigned char>(header[8]) |
                  (static_cast<std::size_t>(static_cast<unsigned char>(header[9])) << 8u)
            : static_cast<unsigned char>(header[8]) |
                  (static_cast<std::size_t>(static_cast<unsigned char>(header[9])) << 8u) |
                  (static_cast<std::size_t>(static_cast<unsigned char>(header[10])) << 16u) |
                  (static_cast<std::size_t>(static_cast<unsigned char>(header[11])) << 24u);
        const std::size_t prefixSize = versionMajor == 1 ? 10u : 12u;
        if (prefixSize + headerLength > header.size()) {
            throw std::runtime_error("NPY header too large in " + path.string());
        }
        input.seekg(static_cast<std::streamoff>(prefixSize + headerLength),
                    std::ios::beg);
        std::vector<float> values(valueCount);
        input.read(reinterpret_cast<char*>(values.data()),
                   static_cast<std::streamsize>(values.size() * sizeof(float)));
        if (!input) {
            throw std::runtime_error("Incomplete NPY float32 payload in " + path.string());
        }
        return values;
    };

    const auto blackRgba = loadFloatNpy(blackPath, pixelCount * 4u);
    const auto magentaRgba = loadFloatNpy(magentaPath, pixelCount * 4u);
    const auto targetRgb = loadFloatNpy(targetPath, pixelCount * 3u);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const auto offset = pixel * 4u;
        EXPECT_FLOAT_EQ(blackRgba[offset + 3u], magentaRgba[offset + 3u]);
        if (blackRgba[offset + 3u] > 0.0f) {
            for (std::size_t channel = 0; channel < 3u; ++channel) {
                EXPECT_FLOAT_EQ(blackRgba[offset + channel],
                                magentaRgba[offset + channel]);
            }
        }
    }
    std::vector<float> blackBgra(pixelCount * 4u);
    std::vector<float> magentaBgra(pixelCount * 4u);
    std::vector<float> targetBgra(pixelCount * 4u);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const auto rgbaOffset = pixel * 4u;
        const auto bgraOffset = rgbaOffset;
        const auto rgbOffset = pixel * 3u;
        blackBgra[bgraOffset] = blackRgba[rgbaOffset + 2u];
        blackBgra[bgraOffset + 1u] = blackRgba[rgbaOffset + 1u];
        blackBgra[bgraOffset + 2u] = blackRgba[rgbaOffset];
        blackBgra[bgraOffset + 3u] = blackRgba[rgbaOffset + 3u];
        magentaBgra[bgraOffset] = magentaRgba[rgbaOffset + 2u];
        magentaBgra[bgraOffset + 1u] = magentaRgba[rgbaOffset + 1u];
        magentaBgra[bgraOffset + 2u] = magentaRgba[rgbaOffset];
        magentaBgra[bgraOffset + 3u] = magentaRgba[rgbaOffset + 3u];
        targetBgra[bgraOffset] = targetRgb[rgbOffset + 2u];
        targetBgra[bgraOffset + 1u] = targetRgb[rgbOffset + 1u];
        targetBgra[bgraOffset + 2u] = targetRgb[rgbOffset];
        targetBgra[bgraOffset + 3u] = 1.0f;
    }

    ImageF32x4_RGBA blackSource;
    ImageF32x4_RGBA magentaSource;
    ImageF32x4_RGBA reference;
    blackSource.setFromBGRA32F(blackBgra.data(), width, height);
    magentaSource.setFromBGRA32F(magentaBgra.data(), width, height);
    reference.setFromBGRA32F(targetBgra.data(), width, height);
    ASSERT_EQ(blackSource.totalPixels(), pixelCount);
    ASSERT_EQ(magentaSource.totalPixels(), pixelCount);
    ASSERT_EQ(reference.totalPixels(), pixelCount);

    constexpr std::array methods{
        AutoColorMatcher::Method::Reinhard,
        AutoColorMatcher::Method::MeanStddev,
        AutoColorMatcher::Method::Histogram,
    };
    constexpr std::array methodNames{"Reinhard", "MeanStddev", "Histogram"};
    constexpr std::array intensities{0.5f, 1.0f};
    const auto toLab = [](float red, float green, float blue) {
        const auto decode = [](double value) {
            const double v = std::clamp(value, 0.0, 1.0);
            return v > 0.04045 ? std::pow((v + 0.055) / 1.055, 2.4) : v / 12.92;
        };
        const double r = decode(red), g = decode(green), b = decode(blue);
        const double x = (0.4124564 * r + 0.3575761 * g + 0.1804375 * b) / 0.95047;
        const double y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
        const double z = (0.0193339 * r + 0.1191920 * g + 0.9503041 * b) / 1.08883;
        const auto f = [](double value) {
            return value > 0.008856 ? std::cbrt(value)
                                    : 7.787 * value + 16.0 / 116.0;
        };
        const double fx = f(x), fy = f(y), fz = f(z);
        return std::array<double, 3>{116.0 * fy - 16.0,
                                     500.0 * (fx - fy),
                                     200.0 * (fy - fz)};
    };
    const auto difference = [](const std::array<double, 3>& lhs,
                               const std::array<double, 3>& rhs) {
        const double d0 = lhs[0] - rhs[0];
        const double d1 = lhs[1] - rhs[1];
        const double d2 = lhs[2] - rhs[2];
        return std::sqrt(d0 * d0 + d1 * d1 + d2 * d2);
    };
    const auto percentile95 = [](std::vector<double>& values) {
        const std::size_t index = static_cast<std::size_t>(
            std::ceil(0.95 * static_cast<double>(values.size()))) - 1u;
        std::nth_element(values.begin(), values.begin() + index, values.end());
        return values[index];
    };

    for (std::size_t methodIndex = 0; methodIndex < methods.size(); ++methodIndex) {
        for (const float intensity : intensities) {
            ImageF32x4_RGBA blackOutput(blackSource);
            ImageF32x4_RGBA magentaOutput(magentaSource);
            AutoColorMatcher::match(blackOutput, reference, methods[methodIndex], intensity);
            AutoColorMatcher::match(magentaOutput, reference, methods[methodIndex], intensity);
            const float* blackPixels = blackOutput.rgba32fData();
            const float* magentaPixels = magentaOutput.rgba32fData();
            std::vector<double> rgbDifferences;
            std::vector<double> labDifferences;
            rgbDifferences.reserve(visiblePixelCount * 3u);
            labDifferences.reserve(visiblePixelCount);
            double rgbDifferenceSum = 0.0;
            bool finite = true;
            bool alphaPreserved = true;
            for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
                const auto offset = pixel * 4u;
                alphaPreserved = alphaPreserved &&
                    blackPixels[offset + 3u] == blackBgra[offset + 3u] &&
                    magentaPixels[offset + 3u] == magentaBgra[offset + 3u];
                for (std::size_t channel = 0; channel < 4u; ++channel) {
                    finite = finite && std::isfinite(blackPixels[offset + channel]) &&
                             std::isfinite(magentaPixels[offset + channel]);
                }
                if (blackBgra[offset + 3u] <= 0.0f) continue;
                for (std::size_t channel = 0; channel < 3u; ++channel) {
                    const double delta = std::abs(
                        static_cast<double>(blackPixels[offset + channel]) -
                        static_cast<double>(magentaPixels[offset + channel]));
                    rgbDifferenceSum += delta;
                    rgbDifferences.push_back(delta);
                }
                const auto blackLab = toLab(blackPixels[offset + 2u],
                                            blackPixels[offset + 1u],
                                            blackPixels[offset]);
                const auto magentaLab = toLab(magentaPixels[offset + 2u],
                                              magentaPixels[offset + 1u],
                                              magentaPixels[offset]);
                labDifferences.push_back(difference(blackLab, magentaLab));
            }
            EXPECT_TRUE(finite);
            EXPECT_TRUE(alphaPreserved);
            ASSERT_EQ(rgbDifferences.size(), visiblePixelCount * 3u);
            ASSERT_EQ(labDifferences.size(), visiblePixelCount);
            const double rgbP95 = percentile95(rgbDifferences);
            const double labP95 = percentile95(labDifferences);
            const double rgbMax = *std::max_element(rgbDifferences.begin(),
                                                     rgbDifferences.end());
            const double labMax = *std::max_element(labDifferences.begin(),
                                                     labDifferences.end());
            const double labMean = std::accumulate(labDifferences.begin(),
                                                   labDifferences.end(), 0.0) /
                                   static_cast<double>(visiblePixelCount);
            std::cout << std::setprecision(10)
                      << "RD25_CORE_ALPHA," << methodNames[methodIndex] << ','
                      << intensity << ',' << visiblePixelCount << ','
                      << rgbDifferenceSum / static_cast<double>(rgbDifferences.size()) << ','
                      << rgbP95 << ',' << rgbMax << ',' << labMean << ','
                      << labP95 << ',' << labMax << ','
                      << (finite ? 1 : 0) << ',' << (alphaPreserved ? 1 : 0) << '\n';
        }
    }
}

TEST(AutoColorMatcherContractTest,
     CoreImageOverloadMeasuresPremultipliedAlphaNumericAssociation)
{
    constexpr int width = 64;
    constexpr int height = 64;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<float> sourceBgra(pixelCount * 4u);
    std::vector<float> referenceBgra(pixelCount * 4u);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto pixel = static_cast<std::size_t>(y) * width + x;
            const double alpha = static_cast<double>(x) / (width - 1);
            const double nx = static_cast<double>(x) / (width - 1);
            const double ny = static_cast<double>(y) / (height - 1);
            const std::array<double, 3> sourceRgb{
                0.10 + 0.70 * nx,
                0.16 + 0.58 * ny,
                0.22 + 0.48 * (1.0 - nx * ny),
            };
            const std::array<double, 3> referenceRgb{
                std::clamp(sourceRgb[0] * 0.91 + 0.055, 0.0, 1.0),
                std::clamp(sourceRgb[1] * 1.04 + 0.025, 0.0, 1.0),
                std::clamp(sourceRgb[2] * 1.08 - 0.020, 0.0, 1.0),
            };
            const auto offset = pixel * 4u;
            sourceBgra[offset] = static_cast<float>(sourceRgb[2] * alpha);
            sourceBgra[offset + 1u] = static_cast<float>(sourceRgb[1] * alpha);
            sourceBgra[offset + 2u] = static_cast<float>(sourceRgb[0] * alpha);
            sourceBgra[offset + 3u] = static_cast<float>(alpha);
            referenceBgra[offset] = static_cast<float>(referenceRgb[2] * alpha);
            referenceBgra[offset + 1u] = static_cast<float>(referenceRgb[1] * alpha);
            referenceBgra[offset + 2u] = static_cast<float>(referenceRgb[0] * alpha);
            referenceBgra[offset + 3u] = static_cast<float>(alpha);
        }
    }

    const auto descriptor =
        SurfaceColorDescriptor::canonicalLinearPremultipliedBgra();
    ImageF32x4_RGBA source;
    ImageF32x4_RGBA reference;
    source.setFromBGRA32F(sourceBgra.data(), width, height, descriptor);
    reference.setFromBGRA32F(referenceBgra.data(), width, height, descriptor);
    ASSERT_EQ(source.colorDescriptor().alphaMode,
              SurfaceAlphaMode::Premultiplied);
    ASSERT_EQ(reference.colorDescriptor().alphaMode,
              SurfaceAlphaMode::Premultiplied);

    constexpr std::array methods{
        AutoColorMatcher::Method::Reinhard,
        AutoColorMatcher::Method::MeanStddev,
        AutoColorMatcher::Method::Histogram,
    };
    constexpr std::array methodNames{"Reinhard", "MeanStddev", "Histogram"};
    constexpr std::array intensities{0.5f, 1.0f};
    for (std::size_t methodIndex = 0; methodIndex < methods.size(); ++methodIndex) {
        for (const float intensity : intensities) {
            ImageF32x4_RGBA output(source);
            AutoColorMatcher::match(output, reference, methods[methodIndex], intensity);
            const float* pixels = output.rgba32fData();
            std::size_t belowZeroChannels = 0;
            std::size_t aboveAlphaChannels = 0;
            std::size_t transparentNonzeroChannels = 0;
            double maxExcessOverAlpha = 0.0;
            bool finite = true;
            bool alphaPreserved = true;
            for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
                const auto offset = pixel * 4u;
                const float alpha = sourceBgra[offset + 3u];
                alphaPreserved = alphaPreserved && pixels[offset + 3u] == alpha;
                for (std::size_t channel = 0; channel < 4u; ++channel) {
                    finite = finite && std::isfinite(pixels[offset + channel]);
                }
                for (std::size_t channel = 0; channel < 3u; ++channel) {
                    const double value = pixels[offset + channel];
                    if (value < 0.0) ++belowZeroChannels;
                    if (value > alpha) {
                        ++aboveAlphaChannels;
                        maxExcessOverAlpha = std::max(
                            maxExcessOverAlpha, value - static_cast<double>(alpha));
                    }
                    if (alpha == 0.0f && std::abs(value) > 1.0e-7) {
                        ++transparentNonzeroChannels;
                    }
                }
            }
            EXPECT_TRUE(finite);
            EXPECT_TRUE(alphaPreserved);
            std::cout << std::setprecision(10)
                      << "RD26_CORE_PREMULT," << methodNames[methodIndex] << ','
                      << intensity << ',' << pixelCount << ','
                      << belowZeroChannels << ',' << aboveAlphaChannels << ','
                      << maxExcessOverAlpha << ',' << transparentNonzeroChannels << ','
                      << (finite ? 1 : 0) << ','
                      << (alphaPreserved ? 1 : 0) << '\n';
        }
    }
}

TEST(AutoColorMatcherContractTest,
     CoreImageOverloadMeasuresRealPortraitSyntheticCast)
{
    const auto repoRoot = std::filesystem::path(__FILE__)
                              .parent_path()
                              .parent_path()
                              .parent_path();
    const auto assetPath = repoRoot / "experiments" / "ColorMatchLab" / "assets" /
                           "WikimediaCC0" / "Pexels-photo-707448-1280.jpg";
    if (!std::filesystem::exists(assetPath)) {
        GTEST_SKIP() << "The licensed CC0 portrait base plate is not installed";
    }

    ImageF32x4_RGBA sourceImage;
    ASSERT_TRUE(sourceImage.load(QString::fromStdString(assetPath.string())));
    ASSERT_EQ(sourceImage.width(), 1280);
    ASSERT_EQ(sourceImage.height(), 851);

    const auto pixelCount = sourceImage.totalPixels();
    const float* sourcePixels = sourceImage.rgba32fData();
    std::vector<float> referencePixels(pixelCount * 4u);
    constexpr std::array scale{0.94f, 1.015f, 1.075f};
    constexpr std::array offset{0.012f, 0.006f, -0.008f};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        referencePixels[pixel * 4u] = std::clamp(
            sourcePixels[pixel * 4u + 2u] * scale[0] + offset[0], 0.0f, 1.0f);
        referencePixels[pixel * 4u + 1u] = std::clamp(
            sourcePixels[pixel * 4u + 1u] * scale[1] + offset[1], 0.0f, 1.0f);
        referencePixels[pixel * 4u + 2u] = std::clamp(
            sourcePixels[pixel * 4u] * scale[2] + offset[2], 0.0f, 1.0f);
        referencePixels[pixel * 4u + 3u] = sourcePixels[pixel * 4u + 3u];
    }
    ImageF32x4_RGBA referenceImage;
    referenceImage.setFromRGBA32F(referencePixels.data(),
                                  sourceImage.width(), sourceImage.height());

    const auto prepDirectory = repoRoot / "experiments" / "ColorMatchLab" /
                               "captures" / "rd9_real_skin_white_scene";
    ImageF32x4_RGBA faceMask;
    ImageF32x4_RGBA armMask;
    ImageF32x4_RGBA blouseMask;
    ASSERT_TRUE(faceMask.load(QString::fromStdString(
        (prepDirectory / "roi_face_skin_reviewed.png").string())));
    ASSERT_TRUE(armMask.load(QString::fromStdString(
        (prepDirectory / "roi_arm_skin_reviewed.png").string())));
    ASSERT_TRUE(blouseMask.load(QString::fromStdString(
        (prepDirectory / "roi_white_blouse_light_fabric_candidate.png").string())));
    ASSERT_EQ(faceMask.width(), sourceImage.width());
    ASSERT_EQ(faceMask.height(), sourceImage.height());

    constexpr std::array methods{
        AutoColorMatcher::Method::Reinhard,
        AutoColorMatcher::Method::MeanStddev,
        AutoColorMatcher::Method::Histogram,
    };
    constexpr std::array methodNames{"Reinhard", "MeanStddev", "Histogram"};
    constexpr std::array intensities{
        0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
        0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    constexpr float kPreviewTolerance = 1.5f / 255.0f;

    auto toLab = [](float red, float green, float blue) {
        const auto decode = [](float value) -> double {
            const double v = std::clamp(static_cast<double>(value), 0.0, 1.0);
            return v > 0.04045 ? std::pow((v + 0.055) / 1.055, 2.4) : v / 12.92;
        };
        const double r = decode(red);
        const double g = decode(green);
        const double b = decode(blue);
        const double x = (0.4124564 * r + 0.3575761 * g + 0.1804375 * b) / 0.95047;
        const double y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
        const double z = (0.0193339 * r + 0.1191920 * g + 0.9503041 * b) / 1.08883;
        const auto f = [](double value) {
            return value > 0.008856 ? std::cbrt(value) : 7.787 * value + 16.0 / 116.0;
        };
        const double fx = f(x), fy = f(y), fz = f(z);
        return std::array<double, 3>{116.0 * fy - 16.0,
                                     500.0 * (fx - fy),
                                     200.0 * (fy - fz)};
    };
    struct RoiAggregate {
        std::size_t pixels{};
        double deltaInput{};
        double deltaTarget{};
        double sourceA{};
        double sourceB{};
        double outputA{};
        double outputB{};
        double targetA{};
        double targetB{};
        double sourceChroma{};
        double outputChroma{};
        double targetChroma{};
        double outputHueSin{};
        double outputHueCos{};
        std::vector<double> deltaInputValues;
        std::vector<double> deltaTargetValues;
    };
    constexpr std::array<const char*, 5> roiNames{
        "face_skin", "arm_skin", "skin_combined", "light_blouse_candidate",
        "full_frame"};
    std::cout << std::setprecision(9);
    const auto captureDirectory = repoRoot / "experiments" / "ColorMatchLab" /
                                  "captures" / "rd22_core_outputs";
    std::filesystem::create_directories(captureDirectory);
    ASSERT_TRUE(sourceImage.save(QString::fromStdString(
        (captureDirectory / "source.png").string())));
    ASSERT_TRUE(referenceImage.save(QString::fromStdString(
        (captureDirectory / "synthetic_reference.png").string())));

    for (std::size_t methodIndex = 0; methodIndex < methods.size(); ++methodIndex) {
        for (const float intensity : intensities) {
            ImageF32x4_RGBA coreOutput(sourceImage);
            AutoColorMatcher::match(coreOutput, referenceImage,
                                    methods[methodIndex], intensity);
            if (intensity == 0.0f || intensity == 0.5f || intensity == 1.0f) {
                const auto outputPath = captureDirectory /
                    (std::string(methodNames[methodIndex]) + "_" +
                     std::to_string(intensity).substr(0, 3) + ".png");
                ASSERT_TRUE(coreOutput.save(QString::fromStdString(outputPath.string())))
                    << outputPath.string();
            }
            const float* sourceRawPixels = sourceImage.rgba32fData();
            const float* referenceRawPixels = referenceImage.rgba32fData();
            std::vector<float> rawOutputPixels(pixelCount * 4u);
            std::copy(sourceRawPixels, sourceRawPixels + pixelCount * 4u,
                      rawOutputPixels.begin());
            AutoColorMatcher::match(rawOutputPixels.data(), referenceRawPixels,
                                    sourceImage.width(), sourceImage.height(),
                                    methods[methodIndex], intensity);
            const float* actualPixels = coreOutput.rgba32fData();
            float maxRgbAbsDifference = 0.0f;
            for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
                for (std::size_t channel = 0; channel < 3; ++channel) {
                    const auto index = pixel * 4u + channel;
                    maxRgbAbsDifference = std::max(
                        maxRgbAbsDifference,
                        std::abs(actualPixels[index] - rawOutputPixels[index]));
                }
                EXPECT_FLOAT_EQ(actualPixels[pixel * 4u + 3u],
                                sourceRawPixels[pixel * 4u + 3u]);
            }
            EXPECT_LE(maxRgbAbsDifference, 1.0e-6f)
                << methodNames[methodIndex] << " intensity " << intensity
                << " max RGB delta between image overload and raw-buffer Core API";

            const float* face = faceMask.rgba32fData();
            const float* arm = armMask.rgba32fData();
            const float* blouse = blouseMask.rgba32fData();
            std::array<RoiAggregate, 5> regions{};
            const float* outputPixels = coreOutput.rgba32fData();
            for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
                const bool inFace = face[pixel * 4u] > 0.5f;
                const bool inArm = arm[pixel * 4u] > 0.5f;
                const bool inBlouse = blouse[pixel * 4u] > 0.5f;
                const std::array<bool, 5> included{
                    inFace, inArm, inFace || inArm, inBlouse, true};
                const auto srcLab = toLab(sourceRawPixels[pixel * 4u + 2u],
                                          sourceRawPixels[pixel * 4u + 1u],
                                          sourceRawPixels[pixel * 4u]);
                const auto refLab = toLab(referenceRawPixels[pixel * 4u + 2u],
                                          referenceRawPixels[pixel * 4u + 1u],
                                          referenceRawPixels[pixel * 4u]);
                const auto outLab = toLab(outputPixels[pixel * 4u + 2u],
                                          outputPixels[pixel * 4u + 1u],
                                          outputPixels[pixel * 4u]);
                for (std::size_t roi = 0; roi < regions.size(); ++roi) {
                    if (!included[roi]) continue;
                    auto& acc = regions[roi];
                    ++acc.pixels;
                    const double srcChroma = std::hypot(srcLab[1], srcLab[2]);
                    const double refChroma = std::hypot(refLab[1], refLab[2]);
                    const double outChroma = std::hypot(outLab[1], outLab[2]);
                    const double deltaInput = std::sqrt(
                        std::pow(outLab[0] - srcLab[0], 2) +
                        std::pow(outLab[1] - srcLab[1], 2) +
                        std::pow(outLab[2] - srcLab[2], 2));
                    const double deltaTarget = std::sqrt(
                        std::pow(outLab[0] - refLab[0], 2) +
                        std::pow(outLab[1] - refLab[1], 2) +
                        std::pow(outLab[2] - refLab[2], 2));
                    acc.deltaInput += deltaInput;
                    acc.deltaTarget += deltaTarget;
                    acc.deltaInputValues.push_back(deltaInput);
                    acc.deltaTargetValues.push_back(deltaTarget);
                    acc.sourceA += srcLab[1]; acc.sourceB += srcLab[2];
                    acc.outputA += outLab[1]; acc.outputB += outLab[2];
                    acc.targetA += refLab[1]; acc.targetB += refLab[2];
                    acc.sourceChroma += srcChroma;
                    acc.outputChroma += outChroma;
                    acc.targetChroma += refChroma;
                    const double hue = std::atan2(outLab[2], outLab[1]);
                    acc.outputHueSin += std::sin(hue);
                    acc.outputHueCos += std::cos(hue);
                }
            }
            for (std::size_t roi = 0; roi < roiNames.size(); ++roi) {
                auto& acc = regions[roi];
                ASSERT_GT(acc.pixels, 0u);
                const double count = static_cast<double>(acc.pixels);
                const double srcCstar = std::hypot(acc.sourceA / count,
                                                   acc.sourceB / count);
                const double outputCstar = std::hypot(acc.outputA / count,
                                                      acc.outputB / count);
                const double targetCstar = std::hypot(acc.targetA / count,
                                                      acc.targetB / count);
                const double outputHue = std::fmod(
                    std::atan2(acc.outputHueSin, acc.outputHueCos) * 180.0 / 3.141592653589793 + 360.0,
                    360.0);
                std::sort(acc.deltaInputValues.begin(), acc.deltaInputValues.end());
                std::sort(acc.deltaTargetValues.begin(), acc.deltaTargetValues.end());
                const auto p95Index = static_cast<std::size_t>(
                    std::ceil(static_cast<double>(acc.pixels) * 0.95) - 1.0);
                const auto countAbove = [](const std::vector<double>& values, double threshold) {
                    return std::count_if(values.begin(), values.end(),
                                         [threshold](double value) { return value > threshold; });
                };
                std::cout << "RD20_CORE_ROI," << methodNames[methodIndex] << ','
                          << intensity << ',' << roiNames[roi] << ',' << acc.pixels
                          << ',' << acc.deltaInput / count << ',' << acc.deltaTarget / count
                          << ',' << srcCstar << ',' << outputCstar << ',' << targetCstar
                          << ',' << acc.sourceChroma / count << ','
                          << acc.outputChroma / count << ',' << acc.targetChroma / count
                          << ',' << outputHue
                          << ',' << acc.deltaInputValues[p95Index]
                          << ',' << acc.deltaInputValues.back()
                          << ',' << 100.0 * static_cast<double>(countAbove(acc.deltaInputValues, 2.0)) / count
                          << ',' << acc.deltaTargetValues[p95Index]
                          << ',' << acc.deltaTargetValues.back()
                          << ',' << 100.0 * static_cast<double>(countAbove(acc.deltaTargetValues, 2.0)) / count
                          << '\n';
            }
        }
    }
}

TEST(AutoColorMatcherContractTest,
     CoreImageOverloadMeasuresShadowedPortraitSyntheticCast)
{
    const auto repoRoot = std::filesystem::path(__FILE__)
                              .parent_path()
                              .parent_path()
                              .parent_path();
    const auto prepDirectory = repoRoot / "experiments" / "ColorMatchLab" /
                               "captures" / "rd23_shadowed_portrait";
    ImageF32x4_RGBA sourceImage;
    ASSERT_TRUE(sourceImage.load(QString::fromStdString(
        (prepDirectory / "source.png").string())));
    ASSERT_EQ(sourceImage.width(), 1536);
    ASSERT_EQ(sourceImage.height(), 1024);

    ImageF32x4_RGBA faceMask;
    ASSERT_TRUE(faceMask.load(QString::fromStdString(
        (prepDirectory / "face_skin_candidate.png").string())));
    ASSERT_EQ(faceMask.width(), sourceImage.width());
    ASSERT_EQ(faceMask.height(), sourceImage.height());

    const auto pixelCount = sourceImage.totalPixels();
    const float* sourcePixels = sourceImage.rgba32fData();
    const float* maskPixels = faceMask.rgba32fData();
    std::vector<float> referencePixels(pixelCount * 4u);
    constexpr std::array scale{0.94f, 1.015f, 1.075f};
    constexpr std::array offset{0.012f, 0.006f, -0.008f};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        // setFromRGBA32F accepts logical RGBA, while the source backing is BGRA.
        referencePixels[pixel * 4u] = std::clamp(
            sourcePixels[pixel * 4u + 2u] * scale[0] + offset[0], 0.0f, 1.0f);
        referencePixels[pixel * 4u + 1u] = std::clamp(
            sourcePixels[pixel * 4u + 1u] * scale[1] + offset[1], 0.0f, 1.0f);
        referencePixels[pixel * 4u + 2u] = std::clamp(
            sourcePixels[pixel * 4u] * scale[2] + offset[2], 0.0f, 1.0f);
        referencePixels[pixel * 4u + 3u] = sourcePixels[pixel * 4u + 3u];
    }
    ImageF32x4_RGBA referenceImage;
    referenceImage.setFromRGBA32F(referencePixels.data(), sourceImage.width(),
                                  sourceImage.height());

    constexpr std::array methods{
        AutoColorMatcher::Method::Reinhard,
        AutoColorMatcher::Method::MeanStddev,
        AutoColorMatcher::Method::Histogram,
    };
    constexpr std::array methodNames{"Reinhard", "MeanStddev", "Histogram"};
    constexpr std::array intensities{0.0f, 0.5f, 1.0f};
    constexpr double kDescriptiveDeltaThreshold = 2.0;
    const auto outputDirectory = prepDirectory / "core_outputs";
    std::filesystem::create_directories(outputDirectory);
    std::ofstream metrics(outputDirectory / "face_metrics.csv",
                          std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(metrics.is_open());
    metrics << "method,intensity,roi_pixels,source_mean_delta_e76,"
               "source_p95_delta_e76,source_max_delta_e76,source_pct_over_2,"
               "synthetic_reference_mean_delta_e76\n";
    metrics << std::setprecision(10);

    const auto toLab = [](float red, float green, float blue) {
        const auto decode = [](float value) -> double {
            const double v = std::clamp(static_cast<double>(value), 0.0, 1.0);
            return v > 0.04045 ? std::pow((v + 0.055) / 1.055, 2.4) : v / 12.92;
        };
        const double r = decode(red), g = decode(green), b = decode(blue);
        const double x = (0.4124564 * r + 0.3575761 * g + 0.1804375 * b) / 0.95047;
        const double y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
        const double z = (0.0193339 * r + 0.1191920 * g + 0.9503041 * b) / 1.08883;
        const auto f = [](double value) {
            return value > 0.008856 ? std::cbrt(value) : 7.787 * value + 16.0 / 116.0;
        };
        const double fx = f(x), fy = f(y), fz = f(z);
        return std::array<double, 3>{116.0 * fy - 16.0,
                                     500.0 * (fx - fy),
                                     200.0 * (fy - fz)};
    };
    const auto deltaE = [](const std::array<double, 3>& lhs,
                           const std::array<double, 3>& rhs) {
        const double d0 = lhs[0] - rhs[0];
        const double d1 = lhs[1] - rhs[1];
        const double d2 = lhs[2] - rhs[2];
        return std::sqrt(d0 * d0 + d1 * d1 + d2 * d2);
    };

    std::size_t roiPixels = 0;
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        if (maskPixels[pixel * 4u] > 0.5f) {
            ++roiPixels;
        }
    }
    ASSERT_GT(roiPixels, 1000u);

    for (std::size_t methodIndex = 0; methodIndex < methods.size(); ++methodIndex) {
        for (const float intensity : intensities) {
            ImageF32x4_RGBA output(sourceImage);
            AutoColorMatcher::match(output, referenceImage, methods[methodIndex],
                                    intensity);
            if (intensity == 0.0f || intensity == 0.5f || intensity == 1.0f) {
                const auto outputPath = outputDirectory /
                    (std::string(methodNames[methodIndex]) + "_" +
                     std::to_string(intensity).substr(0, 3) + ".png");
                ASSERT_TRUE(output.save(QString::fromStdString(outputPath.string())))
                    << outputPath.string();
            }
            const float* outputPixels = output.rgba32fData();
            std::vector<double> sourceDeltas;
            sourceDeltas.reserve(roiPixels);
            double sourceDeltaSum = 0.0;
            double targetDeltaSum = 0.0;
            std::size_t overThreshold = 0;
            double maximumDelta = 0.0;
            for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
                if (maskPixels[pixel * 4u] <= 0.5f) {
                    continue;
                }
                const auto sourceLab = toLab(
                    sourcePixels[pixel * 4u + 2u], sourcePixels[pixel * 4u + 1u],
                    sourcePixels[pixel * 4u]);
                const auto outputLab = toLab(
                    outputPixels[pixel * 4u + 2u], outputPixels[pixel * 4u + 1u],
                    outputPixels[pixel * 4u]);
                const auto targetLab = toLab(
                    referencePixels[pixel * 4u], referencePixels[pixel * 4u + 1u],
                    referencePixels[pixel * 4u + 2u]);
                const double sourceDelta = deltaE(sourceLab, outputLab);
                sourceDeltas.push_back(sourceDelta);
                sourceDeltaSum += sourceDelta;
                targetDeltaSum += deltaE(targetLab, outputLab);
                maximumDelta = std::max(maximumDelta, sourceDelta);
                overThreshold += sourceDelta > kDescriptiveDeltaThreshold;
            }
            std::sort(sourceDeltas.begin(), sourceDeltas.end());
            const auto p95Index = static_cast<std::size_t>(
                std::ceil(0.95 * static_cast<double>(sourceDeltas.size()))) - 1u;
            metrics << methodNames[methodIndex] << ',' << intensity << ','
                    << roiPixels << ','
                    << sourceDeltaSum / static_cast<double>(roiPixels) << ','
                    << sourceDeltas[p95Index] << ',' << maximumDelta << ','
                    << 100.0 * static_cast<double>(overThreshold) /
                           static_cast<double>(roiPixels) << ','
                    << targetDeltaSum / static_cast<double>(roiPixels) << '\n';
        }
    }
    metrics.flush();
    ASSERT_TRUE(metrics.good());

    std::ofstream performance(outputDirectory / "performance.csv",
                              std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(performance.is_open());
    performance << "image,width,height,method,intensity,samples,hardware_threads,"
                   "min_ms,median_ms,max_ms\n";
    performance << std::setprecision(10);
    std::ofstream allocations(outputDirectory / "allocations.csv",
                              std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(allocations.is_open());
    allocations << "image,width,height,method,intensity,hook,allocation_count,requested_bytes\n";
    auto benchmark = [&](std::string_view imageName,
                         const ImageF32x4_RGBA& benchmarkSource,
                         const ImageF32x4_RGBA& benchmarkReference,
                         AutoColorMatcher::Method method,
                         const char* methodName) {
        const auto count = benchmarkSource.totalPixels() * 4u;
        std::vector<float> working(count);
        const float* original = benchmarkSource.rgba32fData();
        std::copy(original, original + count, working.begin());
        AutoColorMatcher::match(working.data(), benchmarkReference.rgba32fData(),
                                benchmarkSource.width(), benchmarkSource.height(),
                                method, 1.0f);
        constexpr std::size_t kSamples = 7;
        std::array<double, kSamples> elapsedMilliseconds{};
        for (std::size_t sample = 0; sample < kSamples; ++sample) {
            std::copy(original, original + count, working.begin());
            const auto begin = std::chrono::steady_clock::now();
            AutoColorMatcher::match(working.data(), benchmarkReference.rgba32fData(),
                                    benchmarkSource.width(), benchmarkSource.height(),
                                    method, 1.0f);
            const auto end = std::chrono::steady_clock::now();
            elapsedMilliseconds[sample] =
                std::chrono::duration<double, std::milli>(end - begin).count();
        }
        std::sort(elapsedMilliseconds.begin(), elapsedMilliseconds.end());
        performance << imageName << ',' << benchmarkSource.width() << ','
                    << benchmarkSource.height() << ',' << methodName << ",1,"
                    << kSamples << ',' << std::thread::hardware_concurrency() << ','
                    << elapsedMilliseconds.front() << ','
                    << elapsedMilliseconds[kSamples / 2u] << ','
                    << elapsedMilliseconds.back() << '\n';

        std::copy(original, original + count, working.begin());
#if defined(_MSC_VER) && defined(_DEBUG)
        std::pair<std::size_t, std::size_t> allocationSample{};
        {
            CoreCrtAllocationCounter counter;
            AutoColorMatcher::match(working.data(), benchmarkReference.rgba32fData(),
                                    benchmarkSource.width(), benchmarkSource.height(),
                                    method, 1.0f);
            allocationSample = counter.stop();
        }
        allocations << imageName << ',' << benchmarkSource.width() << ','
                    << benchmarkSource.height() << ',' << methodName << ",1,"
                    << "MSVC_Debug_CRT," << allocationSample.first << ','
                    << allocationSample.second << '\n';
#else
        allocations << imageName << ',' << benchmarkSource.width() << ','
                    << benchmarkSource.height() << ',' << methodName << ",1,"
                    << "unavailable,0,0\n";
#endif
    };
    for (std::size_t methodIndex = 0; methodIndex < methods.size(); ++methodIndex) {
        benchmark("portrait_1536x1024", sourceImage, referenceImage,
                  methods[methodIndex], methodNames[methodIndex]);
    }

    const auto smallerAsset = repoRoot / "experiments" / "ColorMatchLab" /
                              "assets" / "WikimediaCC0" /
                              "Pexels-photo-707448-1280.jpg";
    ImageF32x4_RGBA smallerSource;
    ASSERT_TRUE(smallerSource.load(QString::fromStdString(smallerAsset.string())));
    ASSERT_EQ(smallerSource.width(), 1280);
    ASSERT_EQ(smallerSource.height(), 851);
    const auto smallerPixelCount = smallerSource.totalPixels();
    const float* smallerPixels = smallerSource.rgba32fData();
    std::vector<float> smallerReferenceLogical(smallerPixelCount * 4u);
    for (std::size_t pixel = 0; pixel < smallerPixelCount; ++pixel) {
        smallerReferenceLogical[pixel * 4u] = std::clamp(
            smallerPixels[pixel * 4u + 2u] * scale[0] + offset[0], 0.0f, 1.0f);
        smallerReferenceLogical[pixel * 4u + 1u] = std::clamp(
            smallerPixels[pixel * 4u + 1u] * scale[1] + offset[1], 0.0f, 1.0f);
        smallerReferenceLogical[pixel * 4u + 2u] = std::clamp(
            smallerPixels[pixel * 4u] * scale[2] + offset[2], 0.0f, 1.0f);
        smallerReferenceLogical[pixel * 4u + 3u] = smallerPixels[pixel * 4u + 3u];
    }
    ImageF32x4_RGBA smallerReference;
    smallerReference.setFromRGBA32F(smallerReferenceLogical.data(),
                                    smallerSource.width(), smallerSource.height());
    for (std::size_t methodIndex = 0; methodIndex < methods.size(); ++methodIndex) {
        benchmark("portrait_1280x851", smallerSource, smallerReference,
                  methods[methodIndex], methodNames[methodIndex]);
    }
    performance.flush();
    ASSERT_TRUE(performance.good());
    allocations.flush();
    ASSERT_TRUE(allocations.good());
    std::cout << "RD23 face ROI pixels=" << roiPixels
              << "; metrics are synthetic-cast diagnostics, not acceptance; "
              << "Core timing samples saved for 1280x851 and 1536x1024\n";
}
