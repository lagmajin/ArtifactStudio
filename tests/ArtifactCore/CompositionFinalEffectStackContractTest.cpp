#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <memory>

import Core.Composition.FinalEffect;

namespace {

using ArtifactCore::CompositionFinalEffect;
using ArtifactCore::CompositionFinalEffectStack;
using ArtifactCore::FinalEffectType;

TEST(CompositionFinalEffectContractTest,
     EffectMetadataParametersAndOpacityRoundTripThroughJson)
{
    CompositionFinalEffect effect(FinalEffectType::ColorGrading);
    effect.setName(QStringLiteral("Warm grade"));
    effect.setEnabled(false);
    effect.setOpacity(0.35f);
    effect.setParameter(QStringLiteral("temperature"), 0.2f);
    effect.setParameter(QStringLiteral("contrast"), 1.15f);

    const QJsonObject json = effect.toJson();
    EXPECT_EQ(json.value(QStringLiteral("type")).toInt(),
              static_cast<int>(FinalEffectType::ColorGrading));
    EXPECT_EQ(json.value(QStringLiteral("name")).toString(),
              QStringLiteral("Warm grade"));
    EXPECT_FALSE(json.value(QStringLiteral("enabled")).toBool());
    EXPECT_FLOAT_EQ(static_cast<float>(json.value(
                         QStringLiteral("opacity")).toDouble()),
                    0.35f);

    auto restored = CompositionFinalEffect::createFromJson(json);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getType(), FinalEffectType::ColorGrading);
    EXPECT_EQ(restored->getName(), QStringLiteral("Warm grade"));
    EXPECT_FALSE(restored->isEnabled());
    EXPECT_FLOAT_EQ(restored->getOpacity(), 0.35f);
    EXPECT_FLOAT_EQ(restored->getParameter(QStringLiteral("temperature")),
                    0.2f);
    EXPECT_FLOAT_EQ(restored->getParameter(QStringLiteral("contrast")),
                    1.15f);
    EXPECT_FLOAT_EQ(restored->getParameter(QStringLiteral("missing"), 4.0f),
                    4.0f);

    effect.setOpacity(2.0f);
    EXPECT_FLOAT_EQ(effect.getOpacity(), 1.0f);
    effect.setOpacity(-1.0f);
    EXPECT_FLOAT_EQ(effect.getOpacity(), 0.0f);
}

TEST(CompositionFinalEffectStackContractTest,
     OrderEnabledSelectionLookupAndRemovalRemainConsistent)
{
    CompositionFinalEffectStack stack;
    stack.addEffect(nullptr);
    stack.addEffect(std::make_unique<CompositionFinalEffect>(
        FinalEffectType::Exposure));
    stack.addEffect(std::make_unique<CompositionFinalEffect>(
        FinalEffectType::LUT));
    stack.addEffect(std::make_unique<CompositionFinalEffect>(
        FinalEffectType::Vignette));
    ASSERT_EQ(stack.getEffectCount(), 3);

    stack.getEffect(1)->setEnabled(false);
    ASSERT_EQ(stack.getEnabledEffects().size(), 2u);
    EXPECT_EQ(stack.findEffect(QStringLiteral("LUT")), stack.getEffect(1));
    EXPECT_EQ(stack.getEffect(-1), nullptr);
    EXPECT_EQ(stack.getEffect(3), nullptr);

    stack.moveEffect(2, 0);
    ASSERT_EQ(stack.getEffectCount(), 3);
    EXPECT_EQ(stack.getEffect(0)->getType(), FinalEffectType::Vignette);
    EXPECT_EQ(stack.getEffect(1)->getType(), FinalEffectType::Exposure);
    EXPECT_EQ(stack.getEffect(2)->getType(), FinalEffectType::LUT);
    EXPECT_EQ(stack.getEnabledEffects().size(), 2u);

    stack.removeEffect(QStringLiteral("LUT"));
    stack.removeEffect(-1);
    ASSERT_EQ(stack.getEffectCount(), 2);
    EXPECT_EQ(stack.findEffect(QStringLiteral("LUT")), nullptr);
    stack.removeEffect(0);
    ASSERT_EQ(stack.getEffectCount(), 1);
    EXPECT_EQ(stack.getEffect(0)->getType(), FinalEffectType::Exposure);
}

TEST(CompositionFinalEffectStackContractTest,
     StackJsonRoundTripPreservesEffectOrderAndEnabledState)
{
    CompositionFinalEffectStack stack;
    auto exposure = std::make_unique<CompositionFinalEffect>(
        FinalEffectType::Exposure);
    exposure->setParameter(QStringLiteral("stops"), 1.5f);
    stack.addEffect(std::move(exposure));

    auto lut = std::make_unique<CompositionFinalEffect>(FinalEffectType::LUT);
    lut->setName(QStringLiteral("Show LUT"));
    lut->setEnabled(false);
    lut->setOpacity(0.6f);
    stack.addEffect(std::move(lut));

    const QJsonObject encoded = stack.toJson();
    ASSERT_TRUE(encoded.value(QStringLiteral("effects")).isArray());
    ASSERT_EQ(encoded.value(QStringLiteral("effects")).toArray().size(), 2);

    CompositionFinalEffectStack restored;
    restored.fromJson(encoded);
    ASSERT_EQ(restored.getEffectCount(), 2);
    ASSERT_NE(restored.getEffect(0), nullptr);
    ASSERT_NE(restored.getEffect(1), nullptr);
    EXPECT_EQ(restored.getEffect(0)->getType(), FinalEffectType::Exposure);
    EXPECT_FLOAT_EQ(restored.getEffect(0)->getParameter(
                        QStringLiteral("stops")), 1.5f);
    EXPECT_EQ(restored.getEffect(1)->getType(), FinalEffectType::LUT);
    EXPECT_EQ(restored.getEffect(1)->getName(), QStringLiteral("Show LUT"));
    EXPECT_FALSE(restored.getEffect(1)->isEnabled());
    EXPECT_FLOAT_EQ(restored.getEffect(1)->getOpacity(), 0.6f);
    EXPECT_EQ(restored.getEnabledEffects().size(), 1u);

    restored.fromJson(QJsonObject{});
    EXPECT_EQ(restored.getEffectCount(), 0);
}

} // namespace
