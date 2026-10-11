#include <gtest/gtest.h>

#include <QVector>

#include <limits>

import Audio.FormantExtractor;
import Audio.Segment;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(int sampleRate, int frameCount, float value = 0.0f)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.channelData.push_back(QVector<float>(frameCount, value));
    return segment;
}
}

TEST(FormantExtractorStandaloneTest, ClassifiesSilenceBelowIntensityThreshold)
{
    FormantExtractor extractor;
    EXPECT_EQ(extractor.classifyPhoneme({800.0f, 1200.0f, 0.0f, 0.009f}),
              PhonemeLabel::Silence);
}

TEST(FormantExtractorStandaloneTest, ClassifiesLowFormantFricativeAndNasals)
{
    FormantExtractor extractor;
    EXPECT_EQ(extractor.classifyPhoneme({100.0f, 100.0f, 3500.0f, 0.5f}),
              PhonemeLabel::F);
    EXPECT_EQ(extractor.classifyPhoneme({100.0f, 100.0f, 1000.0f, 0.02f}),
              PhonemeLabel::M);
    EXPECT_EQ(extractor.classifyPhoneme({100.0f, 100.0f, 1000.0f, 0.1f}),
              PhonemeLabel::N);
}

TEST(FormantExtractorStandaloneTest, ClassifiesEachVowelAtItsReferenceFormants)
{
    FormantExtractor extractor;
    EXPECT_EQ(extractor.classifyPhoneme({800.0f, 1200.0f, 0.0f, 0.5f}), PhonemeLabel::A);
    EXPECT_EQ(extractor.classifyPhoneme({300.0f, 2300.0f, 0.0f, 0.5f}), PhonemeLabel::I);
    EXPECT_EQ(extractor.classifyPhoneme({350.0f, 1300.0f, 0.0f, 0.5f}), PhonemeLabel::U);
    EXPECT_EQ(extractor.classifyPhoneme({500.0f, 1800.0f, 0.0f, 0.5f}), PhonemeLabel::E);
    EXPECT_EQ(extractor.classifyPhoneme({450.0f, 900.0f, 0.0f, 0.5f}), PhonemeLabel::O);
}

TEST(FormantExtractorStandaloneTest, NonFiniteVowelCoordinatesFallBackToOther)
{
    FormantExtractor extractor;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(extractor.classifyPhoneme({nan, nan, 0.0f, 0.5f}), PhonemeLabel::Other);
}

TEST(FormantExtractorStandaloneTest, ValidThresholdsChangeConsonantBoundary)
{
    FormantExtractor extractor;
    const FormantSet formants{300.0f, 700.0f, 1000.0f, 0.5f};
    EXPECT_EQ(extractor.classifyPhoneme(formants), PhonemeLabel::O);

    extractor.setVowelThresholds(500.0f, 900.0f, 1500.0f, 2800.0f);
    EXPECT_EQ(extractor.classifyPhoneme(formants), PhonemeLabel::N);
}

TEST(FormantExtractorStandaloneTest, InvalidThresholdRangesLeavePreviousThresholdsInEffect)
{
    FormantExtractor extractor;
    extractor.setVowelThresholds(500.0f, 900.0f, 1500.0f, 2800.0f);
    extractor.setVowelThresholds(900.0f, 500.0f, 3000.0f, 1000.0f);
    extractor.setVowelThresholds(std::numeric_limits<float>::quiet_NaN(), 800.0f,
                                 1000.0f, std::numeric_limits<float>::infinity());

    EXPECT_EQ(extractor.classifyPhoneme({300.0f, 700.0f, 1000.0f, 0.5f}),
              PhonemeLabel::N);
}

TEST(FormantExtractorStandaloneTest, PhonemeLabelsMapToExpectedMouthShapes)
{
    EXPECT_EQ((PhonemeEvent{0, PhonemeLabel::Silence, 0.0f, 0.0f}).mouthShapeIndex(), 0);
    EXPECT_EQ((PhonemeEvent{0, PhonemeLabel::A, 0.0f, 0.0f}).mouthShapeIndex(), 1);
    EXPECT_EQ((PhonemeEvent{0, PhonemeLabel::I, 0.0f, 0.0f}).mouthShapeIndex(), 2);
    EXPECT_EQ((PhonemeEvent{0, PhonemeLabel::U, 0.0f, 0.0f}).mouthShapeIndex(), 3);
    EXPECT_EQ((PhonemeEvent{0, PhonemeLabel::E, 0.0f, 0.0f}).mouthShapeIndex(), 4);
    EXPECT_EQ((PhonemeEvent{0, PhonemeLabel::O, 0.0f, 0.0f}).mouthShapeIndex(), 3);
    EXPECT_EQ((PhonemeEvent{0, PhonemeLabel::M, 0.0f, 0.0f}).mouthShapeIndex(), 5);
    EXPECT_EQ((PhonemeEvent{0, PhonemeLabel::F, 0.0f, 0.0f}).mouthShapeIndex(), 6);
    EXPECT_EQ((PhonemeEvent{0, PhonemeLabel::N, 0.0f, 0.0f}).mouthShapeIndex(), 0);
    EXPECT_EQ((PhonemeEvent{0, PhonemeLabel::Other, 0.0f, 0.0f}).mouthShapeIndex(), 0);
}

TEST(FormantExtractorStandaloneTest, EmptyInputExtractsZeroFormants)
{
    FormantExtractor extractor;
    AudioSegment segment;

    const auto formants = extractor.extractFormants(segment);

    EXPECT_FLOAT_EQ(formants.f1, 0.0f);
    EXPECT_FLOAT_EQ(formants.f2, 0.0f);
    EXPECT_FLOAT_EQ(formants.f3, 0.0f);
    EXPECT_FLOAT_EQ(formants.intensity, 0.0f);
}

TEST(FormantExtractorStandaloneTest, AnalyzeFramePreservesRequestedFrameAndSilence)
{
    FormantExtractor extractor;
    auto segment = makeMono(48000, 256);

    const auto event = extractor.analyzeFrame(segment, 1234);

    EXPECT_EQ(event.frame, 1234);
    EXPECT_EQ(event.label, PhonemeLabel::Silence);
    EXPECT_FLOAT_EQ(event.intensity, 0.0f);
    EXPECT_FLOAT_EQ(event.confidence, 0.3f);
}

TEST(FormantExtractorStandaloneTest, AnalyzeTrackProducesSequentialEventsFromCompleteWindows)
{
    FormantExtractor extractor;
    auto segment = makeMono(8000, 24, 0.25f);

    const auto events = extractor.analyzeTrack(segment, 1000.0, 70);

    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].frame, 70);
    EXPECT_EQ(events[1].frame, 71);
    EXPECT_EQ(events[2].frame, 72);
    EXPECT_FLOAT_EQ(events[0].intensity, 0.25f);
}

TEST(FormantExtractorStandaloneTest, InvalidTrackRateOrSampleRateReturnsNoEvents)
{
    FormantExtractor extractor;
    const auto segment = makeMono(8000, 24, 0.25f);
    EXPECT_TRUE(extractor.analyzeTrack(segment, 0.0).empty());
    EXPECT_TRUE(extractor.analyzeTrack(segment,
                  std::numeric_limits<double>::quiet_NaN()).empty());
    EXPECT_TRUE(extractor.analyzeTrack(makeMono(0, 24, 0.25f), 30.0).empty());
}

TEST(FormantExtractorStandaloneTest, AnalyzeTrackDoesNotEmitPartialTrailingWindow)
{
    FormantExtractor extractor;
    auto segment = makeMono(8000, 10, 0.25f);

    const auto events = extractor.analyzeTrack(segment, 1000.0);

    EXPECT_EQ(events.size(), 1u);
}

