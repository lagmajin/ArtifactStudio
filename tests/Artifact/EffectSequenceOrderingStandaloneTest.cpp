#include <gtest/gtest.h>

#include <vector>

import Artifact.Effect.Abstract;
import Memory.SharedPtr;

namespace {

class OrderingEffect final : public Artifact::ArtifactAbstractEffect {};

} // namespace

TEST(EffectSequenceOrderingStandaloneTest, SortsDistinctPipelineStages)
{
    auto rasterizer = ArtifactCore::makeShared<OrderingEffect>();
    auto preprocess = ArtifactCore::makeShared<OrderingEffect>();
    auto layerTransform = ArtifactCore::makeShared<OrderingEffect>();
    rasterizer->setPipelineStage(Artifact::EffectPipelineStage::Rasterizer);
    preprocess->setPipelineStage(Artifact::EffectPipelineStage::PreProcess);
    layerTransform->setPipelineStage(Artifact::EffectPipelineStage::LayerTransform);

    const std::vector<Artifact::ArtifactAbstractEffectPtr> input{
        rasterizer, layerTransform, preprocess};
    EXPECT_FALSE(Artifact::ArtifactAbstractEffect::isStageOrderValid(input));

    const auto sorted = Artifact::ArtifactAbstractEffect::sortedByStage(input);
    ASSERT_EQ(sorted.size(), 3u);
    EXPECT_EQ(sorted[0].get(), preprocess.get());
    EXPECT_EQ(sorted[1].get(), rasterizer.get());
    EXPECT_EQ(sorted[2].get(), layerTransform.get());
    EXPECT_TRUE(Artifact::ArtifactAbstractEffect::isStageOrderValid(sorted));
}

TEST(EffectSequenceOrderingStandaloneTest, KeepsInsertionOrderWithinSameStage)
{
    auto first = ArtifactCore::makeShared<OrderingEffect>();
    auto second = ArtifactCore::makeShared<OrderingEffect>();
    auto third = ArtifactCore::makeShared<OrderingEffect>();
    first->setPipelineStage(Artifact::EffectPipelineStage::Rasterizer);
    second->setPipelineStage(Artifact::EffectPipelineStage::Rasterizer);
    third->setPipelineStage(Artifact::EffectPipelineStage::Rasterizer);

    const std::vector<Artifact::ArtifactAbstractEffectPtr> input{
        first, second, third};
    const auto sorted = Artifact::ArtifactAbstractEffect::sortedByStage(input);

    ASSERT_EQ(sorted.size(), input.size());
    EXPECT_EQ(sorted[0].get(), first.get());
    EXPECT_EQ(sorted[1].get(), second.get());
    EXPECT_EQ(sorted[2].get(), third.get());
    EXPECT_TRUE(Artifact::ArtifactAbstractEffect::isStageOrderValid(sorted));
}
