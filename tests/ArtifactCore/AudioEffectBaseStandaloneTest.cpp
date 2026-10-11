#include <gtest/gtest.h>

#include <QJsonObject>
#include <QString>

#include <memory>
#include <vector>

import Audio.Segment;
import Audio.Effect;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
class BasicEffect final : public AudioEffect {
public:
    String getName() const override { return String("Basic"); }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override
    {
        ++processCalls;
        lastSideChain = sideChain;
        segment.startFrame += 1;
    }

    int processCalls = 0;
    const AudioSegment* lastSideChain = nullptr;
};

class ParameterizedEffect final : public AudioEffect {
public:
    String getName() const override { return String("Parameterized"); }
    String effectType() const override { return String("standalone_fixture"); }
    void process(AudioSegment&, const AudioSegment*) override {}

    std::vector<EffectParameter> getParameters() const override
    {
        return {{String("gain"), String("Gain"), 0.0f, 2.0f, 1.0f, gain}};
    }

    float gain = 0.75f;
};
}

TEST(AudioEffectBaseStandaloneTest, DefaultSchedulingAndIdentityValuesAreStable)
{
    const BasicEffect effect;
    EXPECT_EQ(effect.latencySamples(), 0);
    EXPECT_EQ(effect.tailSamples(), 0);
    EXPECT_FALSE(effect.isBypassed());
    EXPECT_EQ(toStdString(effect.effectType()), "unknown");
}

TEST(AudioEffectBaseStandaloneTest, BypassStateCanBeEnabledAndDisabled)
{
    BasicEffect effect;
    effect.setBypass(true);
    EXPECT_TRUE(effect.isBypassed());
    effect.setBypass(false);
    EXPECT_FALSE(effect.isBypassed());
}

TEST(AudioEffectBaseStandaloneTest, DefaultParameterApiIsEmptyAndReturnsZero)
{
    const BasicEffect effect;
    EXPECT_TRUE(effect.getParameters().empty());
    EXPECT_FLOAT_EQ(effect.getParameterValue(String("missing")), 0.0f);
}

TEST(AudioEffectBaseStandaloneTest, BaseJsonStoresTypeAndBypassState)
{
    ParameterizedEffect effect;
    effect.setBypass(true);
    const QJsonObject json = effect.toJson();
    EXPECT_EQ(json.value("type").toString(), QStringLiteral("standalone_fixture"));
    EXPECT_TRUE(json.value("bypass").toBool());
}

TEST(AudioEffectBaseStandaloneTest, FromJsonRestoresBypassAndDefaultsMissingValueToFalse)
{
    BasicEffect effect;
    effect.fromJson(QJsonObject{{QStringLiteral("bypass"), true}});
    EXPECT_TRUE(effect.isBypassed());
    effect.fromJson(QJsonObject{});
    EXPECT_FALSE(effect.isBypassed());
}

TEST(AudioEffectBaseStandaloneTest, ParameterLookupReadsCurrentDerivedParameterValue)
{
    ParameterizedEffect effect;
    auto parameters = effect.getParameters();
    ASSERT_EQ(parameters.size(), 1u);
    EXPECT_EQ(toStdString(parameters[0].id), "gain");
    EXPECT_FLOAT_EQ(effect.getParameterValue(String("gain")), 0.75f);
    effect.gain = 1.25f;
    EXPECT_FLOAT_EQ(effect.getParameterValue(String("gain")), 1.25f);
    EXPECT_FLOAT_EQ(effect.getParameterValue(String("unknown")), 0.0f);
}

TEST(AudioEffectBaseStandaloneTest, ProcessVirtualDispatchReceivesSegmentAndSidechain)
{
    BasicEffect effect;
    AudioSegment input;
    AudioSegment sideChain;
    input.startFrame = 120;
    effect.process(input, &sideChain);
    EXPECT_EQ(effect.processCalls, 1);
    EXPECT_EQ(effect.lastSideChain, &sideChain);
    EXPECT_EQ(input.startFrame, 121);
}

TEST(AudioEffectBaseStandaloneTest, BaseHasVirtualDestructor)
{
    bool destroyed = false;
    struct DestructionProbe final : AudioEffect {
        explicit DestructionProbe(bool& flag) : flag_(flag) {}
        ~DestructionProbe() override { flag_ = true; }
        String getName() const override { return String("Probe"); }
        void process(AudioSegment&, const AudioSegment*) override {}
        bool& flag_;
    };

    std::unique_ptr<AudioEffect> effect = std::make_unique<DestructionProbe>(destroyed);
    effect.reset();
    EXPECT_TRUE(destroyed);
}
