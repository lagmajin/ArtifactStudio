#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <vector>

import Artifact.Acoustic.System;
import Artifact.Acoustic;
import Artifact.Acoustic.WaveModel;
import Artifact.Acoustic.WindModel;
import Artifact.Acoustic.AudioTaskSynthesizer;

namespace {

constexpr int kSampleRate = 48000;
constexpr int kDurationSeconds = 5;
constexpr std::size_t kFramesPerBlock = 256;

void writeU16(std::ofstream& output, std::uint16_t value)
{
    const char bytes[] = {
        static_cast<char>(value & 0xffu),
        static_cast<char>((value >> 8) & 0xffu),
    };
    output.write(bytes, sizeof(bytes));
}

void writeU32(std::ofstream& output, std::uint32_t value)
{
    const char bytes[] = {
        static_cast<char>(value & 0xffu),
        static_cast<char>((value >> 8) & 0xffu),
        static_cast<char>((value >> 16) & 0xffu),
        static_cast<char>((value >> 24) & 0xffu),
    };
    output.write(bytes, sizeof(bytes));
}

bool writeWaveFile(const std::filesystem::path& path,
                   const std::vector<float>& interleavedStereo)
{
    if ((interleavedStereo.size() % 2) != 0) return false;
    const std::uint64_t dataSize64 = interleavedStereo.size() * sizeof(std::int16_t);
    if (dataSize64 > std::numeric_limits<std::uint32_t>::max() - 36u) return false;

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;

    const auto dataSize = static_cast<std::uint32_t>(dataSize64);
    output.write("RIFF", 4);
    writeU32(output, 36u + dataSize);
    output.write("WAVEfmt ", 8);
    writeU32(output, 16u);
    writeU16(output, 1u);
    writeU16(output, 2u);
    writeU32(output, kSampleRate);
    writeU32(output, kSampleRate * 2u * sizeof(std::int16_t));
    writeU16(output, 2u * sizeof(std::int16_t));
    writeU16(output, 16u);
    output.write("data", 4);
    writeU32(output, dataSize);

    for (const float sample : interleavedStereo) {
        const float finiteSample = std::isfinite(sample)
            ? std::clamp(sample, -1.0f, 1.0f) : 0.0f;
        const auto pcm = static_cast<std::int16_t>(
            std::lrint(finiteSample * (finiteSample < 0.0f ? 32768.0f : 32767.0f)));
        writeU16(output, static_cast<std::uint16_t>(pcm));
    }
    return output.good();
}

} // namespace

void expectPlayableWav(Artifact::Acoustic::AcousticSystem& acoustic,
                       const char* filename,
                       bool expectStereoSpread = false,
                       Artifact::Acoustic::RainImpactSurface rainSurface =
                           Artifact::Acoustic::RainImpactSurface::Solid)
{
    acoustic.SetRainImpactSurface(rainSurface);
    const std::size_t totalFrames =
        static_cast<std::size_t>(kSampleRate * kDurationSeconds);
    std::vector<float> pcm(totalFrames * 2u, 0.0f);
    std::vector<float> block(kFramesPerBlock * 2u, 0.0f);

    for (std::size_t frame = 0; frame < totalFrames;) {
        const std::size_t frameCount = std::min(kFramesPerBlock, totalFrames - frame);
        acoustic.Update(static_cast<float>(frameCount) / kSampleRate);
        const std::span<float> outputBlock(block.data(), frameCount * 2u);
        ASSERT_TRUE(acoustic.RenderAudioBlock(outputBlock, kSampleRate));
        std::copy_n(block.begin(), frameCount * 2u, pcm.begin() + frame * 2u);
        frame += frameCount;
    }

    double squaredSum = 0.0;
    double stereoDifferenceSquaredSum = 0.0;
    float peak = 0.0f;
    for (std::size_t index = 0; index < pcm.size(); ++index) {
        const float sample = pcm[index];
        ASSERT_TRUE(std::isfinite(sample));
        peak = std::max(peak, std::abs(sample));
        squaredSum += static_cast<double>(sample) * sample;
        if ((index % 2u) == 0u) {
            const double difference = static_cast<double>(sample) - pcm[index + 1u];
            stereoDifferenceSquaredSum += difference * difference;
        }
    }
    const double rms = std::sqrt(squaredSum / pcm.size());
    EXPECT_GT(peak, 0.01f);
    EXPECT_GT(rms, 0.001);
    if (expectStereoSpread) {
        EXPECT_GT(std::sqrt(stereoDifferenceSquaredSum / (pcm.size() / 2u)), 0.001);
    }

    const auto wavePath = std::filesystem::current_path() / "temp" / filename;
    ASSERT_TRUE(writeWaveFile(wavePath, pcm)) << wavePath.string();
    EXPECT_EQ(std::filesystem::file_size(wavePath), 44u + pcm.size() * sizeof(std::int16_t));
}

std::vector<float> renderRainPreview(
    Artifact::Acoustic::RainImpactSurface surface,
    int durationSeconds = 2)
{
    Artifact::Acoustic::AcousticSystem acoustic;
    acoustic.SetRainIntensity(350.0f);
    acoustic.SetRainImpactSurface(surface);

    const std::size_t totalFrames =
        static_cast<std::size_t>(kSampleRate * durationSeconds);
    std::vector<float> pcm(totalFrames * 2u, 0.0f);
    std::vector<float> block(kFramesPerBlock * 2u, 0.0f);
    for (std::size_t frame = 0; frame < totalFrames;) {
        const std::size_t frameCount = std::min(kFramesPerBlock, totalFrames - frame);
        acoustic.Update(static_cast<float>(frameCount) / kSampleRate);
        const std::span<float> outputBlock(block.data(), frameCount * 2u);
        if (!acoustic.RenderAudioBlock(outputBlock, kSampleRate)) {
            return {};
        }
        std::copy_n(block.begin(), frameCount * 2u, pcm.begin() + frame * 2u);
        frame += frameCount;
    }
    return pcm;
}

TEST(AcousticGenerationTest, WavesProducePlayableWav)
{
    Artifact::Acoustic::AcousticSystem acoustic;
    acoustic.SetWaveHeight(0.8f);
    acoustic.SetWavePeriod(3.0f);
    acoustic.SetWaveBreaking(0.9f);

    expectPlayableWav(acoustic, "acoustic_wave_preview.wav");
}

TEST(AcousticGenerationTest, BreakingWaveBubbleCloudProducesPlayableWav)
{
    using namespace Artifact::Acoustic;
    WaveModel wave;
    wave.SetWaveHeight(0.8f);
    wave.SetPeriod(3.0f);
    wave.SetBreaking(0.9f);
    std::array<AcousticBubbleSample, 16> bubbleSamples{};
    for (std::size_t i = 0; i < bubbleSamples.size(); ++i) {
        bubbleSamples[i].position = {
            static_cast<float>(i % 4u) * 0.025f,
            static_cast<float>(i / 4u) * 0.025f,
            0.0f
        };
        bubbleSamples[i].radiusMeters = 0.008f;
    }
    wave.SetBubbleSamples(bubbleSamples);
    AudioTaskSynthesizer coupledSynthesizer;
    AudioTaskSynthesizer uncoupledSynthesizer;

    const std::size_t totalFrames = kSampleRate * kDurationSeconds;
    std::vector<float> coupledPcm(totalFrames * 2u, 0.0f);
    std::vector<float> uncoupledPcm(totalFrames * 2u, 0.0f);
    std::vector<float> coupledBlock(kFramesPerBlock * 2u, 0.0f);
    std::vector<float> uncoupledBlock(kFramesPerBlock * 2u, 0.0f);
    for (std::size_t frame = 0; frame < totalFrames;) {
        const std::size_t frameCount = std::min(kFramesPerBlock, totalFrames - frame);
        wave.Update(static_cast<float>(frameCount) / kSampleRate);
        const AudioTaskBatch generated = wave.GenerateTasks();
        AudioTaskBlock bubbleTasks;
        for (const AudioTask& task : generated) {
            if (task.type == SynthesisType::BubbleCloud &&
                bubbleTasks.count < AudioTaskBlock::Capacity) {
                bubbleTasks.tasks[bubbleTasks.count++] = task;
            }
        }
        AudioTaskBlock uncoupledTasks = bubbleTasks;
        for (std::size_t i = 0; i < uncoupledTasks.count; ++i) {
            uncoupledTasks.tasks[i].couplingStrength = 0.0f;
        }

        const std::span<float> coupledOutput(coupledBlock.data(), frameCount * 2u);
        const std::span<float> uncoupledOutput(uncoupledBlock.data(), frameCount * 2u);
        ASSERT_TRUE(coupledSynthesizer.RenderStereo(
            bubbleTasks, coupledOutput, kSampleRate));
        ASSERT_TRUE(uncoupledSynthesizer.RenderStereo(
            uncoupledTasks, uncoupledOutput, kSampleRate));
        std::copy_n(coupledBlock.begin(), frameCount * 2u,
            coupledPcm.begin() + frame * 2u);
        std::copy_n(uncoupledBlock.begin(), frameCount * 2u,
            uncoupledPcm.begin() + frame * 2u);
        frame += frameCount;
    }

    float coupledPeak = 0.0f;
    float uncoupledPeak = 0.0f;
    double coupledSquaredSum = 0.0;
    double uncoupledSquaredSum = 0.0;
    double coupledLowSquaredSum = 0.0;
    double uncoupledLowSquaredSum = 0.0;
    float coupledLowPass = 0.0f;
    float uncoupledLowPass = 0.0f;
    const float lowPassCoefficient =
        1.0f - std::exp(-6.28318530718f * 500.0f / kSampleRate);
    for (std::size_t i = 0; i < coupledPcm.size(); ++i) {
        ASSERT_TRUE(std::isfinite(coupledPcm[i]));
        ASSERT_TRUE(std::isfinite(uncoupledPcm[i]));
        coupledPeak = std::max(coupledPeak, std::abs(coupledPcm[i]));
        uncoupledPeak = std::max(uncoupledPeak, std::abs(uncoupledPcm[i]));
        coupledSquaredSum += static_cast<double>(coupledPcm[i]) * coupledPcm[i];
        uncoupledSquaredSum += static_cast<double>(uncoupledPcm[i]) * uncoupledPcm[i];
        if ((i % 2u) == 0u) {
            const float coupledMono = 0.5f * (coupledPcm[i] + coupledPcm[i + 1u]);
            const float uncoupledMono = 0.5f * (uncoupledPcm[i] + uncoupledPcm[i + 1u]);
            coupledLowPass += lowPassCoefficient * (coupledMono - coupledLowPass);
            uncoupledLowPass += lowPassCoefficient * (uncoupledMono - uncoupledLowPass);
            coupledLowSquaredSum += static_cast<double>(coupledLowPass) * coupledLowPass;
            uncoupledLowSquaredSum += static_cast<double>(uncoupledLowPass) * uncoupledLowPass;
        }
    }
    EXPECT_GT(coupledPeak, 0.001f);
    EXPECT_GT(uncoupledPeak, 0.001f);
    EXPECT_GT(std::sqrt(coupledSquaredSum / coupledPcm.size()), 0.0001);
    EXPECT_GT(std::sqrt(uncoupledSquaredSum / uncoupledPcm.size()), 0.0001);
    EXPECT_GT(coupledLowSquaredSum, uncoupledLowSquaredSum)
        << "Coupled pair modes should shift spectral energy below 500 Hz";

    const auto wavePath = std::filesystem::current_path() / "temp" /
        "acoustic_wave_bubble_cloud_preview.wav";
    const auto uncoupledPath = std::filesystem::current_path() / "temp" /
        "acoustic_wave_bubble_cloud_uncoupled_preview.wav";
    ASSERT_TRUE(writeWaveFile(wavePath, coupledPcm)) << wavePath.string();
    ASSERT_TRUE(writeWaveFile(uncoupledPath, uncoupledPcm)) << uncoupledPath.string();
}

TEST(AcousticGenerationTest, WindProducesPlayableWav)
{
    Artifact::Acoustic::AcousticSystem acoustic;
    acoustic.SetWindVelocity(15.0f);

    expectPlayableWav(acoustic, "acoustic_wind_preview.wav");
}

TEST(AcousticGenerationTest, RainProducesPlayableWav)
{
    Artifact::Acoustic::AcousticSystem acoustic;
    acoustic.SetRainIntensity(350.0f);

    expectPlayableWav(acoustic, "acoustic_rain_preview.wav", true);
}

TEST(AcousticGenerationTest, WaterRainProducesPlayableWav)
{
    Artifact::Acoustic::AcousticSystem acoustic;
    acoustic.SetRainIntensity(350.0f);

    expectPlayableWav(acoustic, "acoustic_rain_water_preview.wav", true,
        Artifact::Acoustic::RainImpactSurface::Water);
}

TEST(AcousticGenerationTest, RainDoesNotRepeatAtOneSecondOffset)
{
    const std::vector<float> pcm = renderRainPreview(
        Artifact::Acoustic::RainImpactSurface::Solid);
    ASSERT_EQ(pcm.size(), static_cast<std::size_t>(2 * kSampleRate * 2));

    constexpr std::size_t windowSamples = kSampleRate / 10u * 2u;
    constexpr std::size_t secondWindowOffset = kSampleRate * 2u;
    EXPECT_FALSE(std::equal(
        pcm.begin(), pcm.begin() + windowSamples,
        pcm.begin() + secondWindowOffset));
}

TEST(AcousticGenerationTest, WaterRainDiffersFromSolidImpact)
{
    const std::vector<float> solid = renderRainPreview(
        Artifact::Acoustic::RainImpactSurface::Solid);
    const std::vector<float> water = renderRainPreview(
        Artifact::Acoustic::RainImpactSurface::Water);
    ASSERT_FALSE(solid.empty());
    ASSERT_EQ(solid.size(), water.size());

    double differenceSquaredSum = 0.0;
    for (std::size_t i = 0; i < solid.size(); ++i) {
        ASSERT_TRUE(std::isfinite(solid[i]));
        ASSERT_TRUE(std::isfinite(water[i]));
        const double difference = static_cast<double>(solid[i]) - water[i];
        differenceSquaredSum += difference * difference;
    }
    const double differenceRms =
        std::sqrt(differenceSquaredSum / solid.size());
    EXPECT_GT(differenceRms, 0.00001);
}

TEST(AcousticGenerationTest, WindGustsModulateFourOrderedNoiseBands)
{
    using namespace Artifact::Acoustic;
    WindModel wind;
    wind.SetVelocity(15.0f);

    const AudioTaskBatch initial = wind.GenerateTasks();
    ASSERT_EQ(initial.count, 4u);
    float initialLowBandAmplitude = initial.tasks[0].amplitude;
    EXPECT_GT(initialLowBandAmplitude, 0.0f);

    bool observedGustChange = false;
    for (int step = 0; step < 300; ++step) {
        wind.Update(0.02f);
        const AudioTaskBatch tasks = wind.GenerateTasks();
        ASSERT_EQ(tasks.count, 4u);
        for (std::size_t i = 0; i < tasks.count; ++i) {
            EXPECT_EQ(tasks.tasks[i].type, SynthesisType::BandPassNoise);
            EXPECT_TRUE(std::isfinite(tasks.tasks[i].frequency));
            EXPECT_TRUE(std::isfinite(tasks.tasks[i].amplitude));
            EXPECT_GE(tasks.tasks[i].amplitude, 0.0f);
            if (i > 0) {
                EXPECT_GT(tasks.tasks[i].frequency,
                          tasks.tasks[i - 1].frequency);
            }
        }
        observedGustChange |=
            std::abs(tasks.tasks[0].amplitude - initialLowBandAmplitude) > 0.001f;
    }
    EXPECT_TRUE(observedGustChange);
}

TEST(AcousticGenerationTest, DebugSnapshotKeepsHighestPriorityTasksWithinLodBudget)
{
    using namespace Artifact::Acoustic;

    AcousticSystem acoustic;
    constexpr std::uint32_t modelCount = 5;
    for (std::uint32_t index = 0; index < modelCount; ++index) {
        acoustic.OnCollision(index + 1, "Steel",
                             0.1f + static_cast<float>(index) * 0.1f, 0.0f);
    }

    const AcousticSnapshot snapshot = acoustic.FetchDebugSnapshot();

    ASSERT_EQ(snapshot.activeTasks.size(), AudioTaskBlock::Capacity);
    ASSERT_EQ(snapshot.culledTasks.size(), modelCount * 8 - AudioTaskBlock::Capacity);
    EXPECT_EQ(snapshot.activeTasks.size() + snapshot.culledTasks.size(), modelCount * 8);

    const auto priority = [](const AudioTaskDebug& task) {
        return task.amp * task.attenuation;
    };
    for (std::size_t index = 1; index < snapshot.activeTasks.size(); ++index) {
        EXPECT_GE(priority(snapshot.activeTasks[index - 1]),
                  priority(snapshot.activeTasks[index]));
    }
    for (std::size_t index = 1; index < snapshot.culledTasks.size(); ++index) {
        EXPECT_GE(priority(snapshot.culledTasks[index - 1]),
                  priority(snapshot.culledTasks[index]));
    }

    const float lowestActivePriority = priority(snapshot.activeTasks.back());
    const float highestCulledPriority = priority(snapshot.culledTasks.front());
    EXPECT_GE(lowestActivePriority, highestCulledPriority)
        << "The fixed task budget should retain the highest-priority acoustic work";

    const AudioTaskBlock audioBlock = acoustic.FetchAudioTasks();
    ASSERT_EQ(audioBlock.count, AudioTaskBlock::Capacity);
    for (std::size_t index = 0; index < audioBlock.count; ++index) {
        EXPECT_EQ(audioBlock.tasks[index].type, snapshot.activeTasks[index].type);
        EXPECT_FLOAT_EQ(audioBlock.tasks[index].amplitude,
                        snapshot.activeTasks[index].amp);
        EXPECT_FLOAT_EQ(audioBlock.tasks[index].frequency,
                        snapshot.activeTasks[index].freq);
    }
}

TEST(AcousticGenerationTest,
     SpatialAttenuationIsAppliedBeforeAcousticLodBudgetSelection)
{
    using namespace Artifact::Acoustic;

    AcousticSystem acoustic;
    acoustic.SetListener({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
    constexpr std::uint32_t farLayerId = 1;
    acoustic.UpdateLayerSpatial(farLayerId, {100.0f, 0.0f, 0.0f},
                                {0.0f, 0.0f, 0.0f});
    acoustic.OnCollision(farLayerId, "Steel", 10.0f, 0.0f);

    constexpr std::uint32_t nearLayerCount = 4;
    for (std::uint32_t index = 0; index < nearLayerCount; ++index) {
        const std::uint32_t layerId = index + 2;
        acoustic.UpdateLayerSpatial(layerId, {1.0f, 0.0f, 0.0f},
                                    {0.0f, 0.0f, 0.0f});
        acoustic.OnCollision(layerId, "Steel", 0.2f, 0.0f);
    }

    const AcousticSnapshot snapshot = acoustic.FetchDebugSnapshot();
    ASSERT_EQ(snapshot.activeTasks.size(), AudioTaskBlock::Capacity);
    ASSERT_EQ(snapshot.culledTasks.size(), 8u);
    for (const AudioTaskDebug& task : snapshot.activeTasks) {
        EXPECT_NE(task.layerId, farLayerId)
            << "the nearby sources should occupy the bounded active-task budget "
               "after spatial attenuation";
    }
    for (const AudioTaskDebug& task : snapshot.culledTasks) {
        EXPECT_EQ(task.layerId, farLayerId)
            << "the strongly attenuated distant source should be culled first";
    }

    const AudioTaskBlock audioBlock = acoustic.FetchAudioTasks();
    ASSERT_EQ(audioBlock.count, AudioTaskBlock::Capacity);
    for (std::size_t index = 0; index < audioBlock.count; ++index) {
        EXPECT_NEAR(audioBlock.tasks[index].attenuation,
                    snapshot.activeTasks[index].attenuation, 1.0e-6f);
    }
}

TEST(AcousticGenerationTest,
     SpatialChangesReclassifyTasksAcrossTheAcousticLodBudget)
{
    using namespace Artifact::Acoustic;

    AcousticSystem acoustic;
    acoustic.SetListener({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
    constexpr std::uint32_t movingLayerId = 1;
    acoustic.UpdateLayerSpatial(movingLayerId, {100.0f, 0.0f, 0.0f},
                                {0.0f, 0.0f, 0.0f});
    acoustic.OnCollision(movingLayerId, "Steel", 10.0f, 0.0f);

    constexpr std::uint32_t nearbyLayerCount = 4;
    for (std::uint32_t index = 0; index < nearbyLayerCount; ++index) {
        const std::uint32_t layerId = index + 2;
        acoustic.UpdateLayerSpatial(layerId, {1.0f, 0.0f, 0.0f},
                                    {0.0f, 0.0f, 0.0f});
        acoustic.OnCollision(layerId, "Steel", 0.2f, 0.0f);
    }

    const AcousticSnapshot whileFar = acoustic.FetchDebugSnapshot();
    ASSERT_EQ(whileFar.activeTasks.size(), AudioTaskBlock::Capacity);
    ASSERT_EQ(whileFar.culledTasks.size(), 8u);
    for (const AudioTaskDebug& task : whileFar.culledTasks) {
        EXPECT_EQ(task.layerId, movingLayerId);
    }

    acoustic.UpdateLayerSpatial(movingLayerId, {1.0f, 0.0f, 0.0f},
                                {0.0f, 0.0f, 0.0f});
    const AcousticSnapshot afterMovingNear = acoustic.FetchDebugSnapshot();
    ASSERT_EQ(afterMovingNear.activeTasks.size(), AudioTaskBlock::Capacity);
    ASSERT_EQ(afterMovingNear.culledTasks.size(), whileFar.culledTasks.size());
    EXPECT_TRUE(std::all_of(afterMovingNear.activeTasks.begin(),
                            afterMovingNear.activeTasks.end(),
                            [movingLayerId](const AudioTaskDebug& task) {
                                return task.layerId == movingLayerId;
                            }))
        << "the newly highest-priority source should replace lower-priority "
           "tasks within the active LOD budget";
    for (const AudioTaskDebug& task : afterMovingNear.culledTasks) {
        EXPECT_NE(task.layerId, movingLayerId)
            << "the moved-near source should no longer remain stale in the culled set";
    }
}

TEST(AcousticGenerationTest, DebugSnapshotUsesFullBudgetWhenTaskCountFits)
{
    using namespace Artifact::Acoustic;

    AcousticSystem acoustic;
    acoustic.OnCollision(7, "Steel", 0.2f, 0.0f);

    const AcousticSnapshot snapshot = acoustic.FetchDebugSnapshot();
    EXPECT_EQ(snapshot.frameNumber, 0u);
    EXPECT_EQ(snapshot.activeTasks.size(), 8u);
    EXPECT_TRUE(snapshot.culledTasks.empty());

    const AcousticSnapshot nextSnapshot = acoustic.FetchDebugSnapshot();
    EXPECT_EQ(nextSnapshot.frameNumber, 1u);
    EXPECT_EQ(nextSnapshot.activeTasks.size(), snapshot.activeTasks.size());
    EXPECT_TRUE(nextSnapshot.culledTasks.empty());
}

TEST(AcousticGenerationTest, DebugSnapshotRetainsAllTasksAtExactLodBudget)
{
    using namespace Artifact::Acoustic;

    AcousticSystem acoustic;
    for (std::uint32_t index = 0; index < 4; ++index) {
        acoustic.OnCollision(index + 1, "Steel", 0.1f, 0.0f);
    }

    const AcousticSnapshot snapshot = acoustic.FetchDebugSnapshot();
    EXPECT_EQ(snapshot.activeTasks.size(), AudioTaskBlock::Capacity);
    EXPECT_TRUE(snapshot.culledTasks.empty());
}

TEST(AcousticGenerationTest,
     OverBudgetLODSelectionRemainsStableAcrossUnchangedSnapshots)
{
    using namespace Artifact::Acoustic;

    AcousticSystem acoustic;
    constexpr std::uint32_t modelCount = 5;
    for (std::uint32_t index = 0; index < modelCount; ++index) {
        acoustic.OnCollision(index + 1, "Steel",
                             0.1f + static_cast<float>(index) * 0.1f, 0.0f);
    }

    const AcousticSnapshot first = acoustic.FetchDebugSnapshot();
    const AcousticSnapshot repeated = acoustic.FetchDebugSnapshot();

    ASSERT_EQ(first.activeTasks.size(), AudioTaskBlock::Capacity);
    ASSERT_EQ(repeated.activeTasks.size(), first.activeTasks.size());
    ASSERT_EQ(repeated.culledTasks.size(), first.culledTasks.size());
    const auto expectSameTasks = [](const auto& expected, const auto& actual) {
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t index = 0; index < expected.size(); ++index) {
            EXPECT_EQ(actual[index].layerId, expected[index].layerId)
                << "LOD budget selection should preserve task identity at index "
                << index;
            EXPECT_EQ(actual[index].type, expected[index].type);
            EXPECT_FLOAT_EQ(actual[index].freq, expected[index].freq);
            EXPECT_FLOAT_EQ(actual[index].amp, expected[index].amp);
            EXPECT_FLOAT_EQ(actual[index].attenuation,
                            expected[index].attenuation);
        }
    };
    expectSameTasks(first.activeTasks, repeated.activeTasks);
    expectSameTasks(first.culledTasks, repeated.culledTasks);
}

TEST(AcousticGenerationTest, OfflineRenderAcceptsUnevenBlocksAndWritesFullDuration)
{
    using namespace Artifact::Acoustic;

    AcousticSystem acoustic;
    acoustic.SetWaveHeight(0.65f);
    acoustic.SetWavePeriod(4.2f);
    acoustic.SetWaveBreaking(0.55f);

    constexpr std::size_t totalFrames = 2u * kSampleRate + 137u;
    constexpr std::array<std::size_t, 4> blockSizes{127u, 511u, 64u, 256u};
    std::vector<float> pcm(totalFrames * 2u, 0.0f);
    std::vector<float> block(511u * 2u, 0.0f);

    std::size_t frame = 0;
    std::size_t blockIndex = 0;
    while (frame < totalFrames) {
        const std::size_t requested = blockSizes[blockIndex++ % blockSizes.size()];
        const std::size_t frameCount = std::min(requested, totalFrames - frame);
        acoustic.Update(static_cast<float>(frameCount) / kSampleRate);
        const std::span<float> outputBlock(block.data(), frameCount * 2u);
        ASSERT_TRUE(acoustic.RenderAudioBlock(outputBlock, kSampleRate));
        ASSERT_TRUE(std::all_of(outputBlock.begin(), outputBlock.end(),
                                [](float sample) { return std::isfinite(sample); }));
        std::copy(outputBlock.begin(), outputBlock.end(), pcm.begin() + frame * 2u);
        frame += frameCount;
    }

    const auto wavePath = std::filesystem::current_path() / "temp" /
                          "offline_wave_2s_plus_137_frames.wav";
    ASSERT_TRUE(writeWaveFile(wavePath, pcm)) << wavePath.string();
    EXPECT_EQ(std::filesystem::file_size(wavePath), 44u + totalFrames * 2u * sizeof(std::int16_t));
}
