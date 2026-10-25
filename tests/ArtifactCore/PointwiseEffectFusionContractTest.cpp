#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

import Artifact.Render.PointwiseEffectFusion;
import Core.ArtifactString;

using namespace ArtifactCore;

TEST(PointwiseEffectFusionContractTest, DescriptorsSeparatePointwiseAndPassDomains)
{
    const auto matrix = PointwiseEffectFusion::descriptor(
        PointwiseNodeKind::ColorMatrix);
    EXPECT_TRUE(matrix.pointwise);
    EXPECT_EQ(matrix.domain, EffectExecutionDomain::Pointwise);
    EXPECT_EQ(matrix.parameters.slotCount, 3u);

    const auto blur = PointwiseEffectFusion::descriptor(
        PointwiseNodeKind::NeighborhoodBlur);
    EXPECT_FALSE(blur.pointwise);
    EXPECT_EQ(blur.domain, EffectExecutionDomain::Neighborhood);

    const auto temporal = PointwiseEffectFusion::descriptor(
        PointwiseNodeKind::Temporal);
    EXPECT_FALSE(temporal.pointwise);
    EXPECT_EQ(temporal.domain, EffectExecutionDomain::Temporal);
}

TEST(PointwiseEffectFusionContractTest, EveryNodeKindHasItsExpectedDomainAndParameterContract)
{
    const struct {
        PointwiseNodeKind kind;
        EffectExecutionDomain domain;
        std::size_t parameterSlots;
        bool pointwise;
    } cases[] = {
        {PointwiseNodeKind::Exposure, EffectExecutionDomain::Pointwise, 1, true},
        {PointwiseNodeKind::Offset, EffectExecutionDomain::Pointwise, 1, true},
        {PointwiseNodeKind::Gamma, EffectExecutionDomain::Pointwise, 1, true},
        {PointwiseNodeKind::Contrast, EffectExecutionDomain::Pointwise, 1, true},
        {PointwiseNodeKind::Levels, EffectExecutionDomain::Pointwise, 2, true},
        {PointwiseNodeKind::Saturation, EffectExecutionDomain::Pointwise, 1, true},
        {PointwiseNodeKind::Tint, EffectExecutionDomain::Pointwise, 1, true},
        {PointwiseNodeKind::HueRotate, EffectExecutionDomain::Pointwise, 1, true},
        {PointwiseNodeKind::ColorTemperature, EffectExecutionDomain::Pointwise, 1, true},
        {PointwiseNodeKind::ColorMatrix, EffectExecutionDomain::Pointwise, 3, true},
        {PointwiseNodeKind::Clamp, EffectExecutionDomain::Pointwise, 2, true},
        {PointwiseNodeKind::Lut3D, EffectExecutionDomain::Pointwise, 1, true},
        {PointwiseNodeKind::Blend, EffectExecutionDomain::Pointwise, 1, true},
        {PointwiseNodeKind::AlphaConvert, EffectExecutionDomain::Pointwise, 1, true},
        {PointwiseNodeKind::Posterize, EffectExecutionDomain::Pointwise, 1, true},
        {PointwiseNodeKind::Threshold, EffectExecutionDomain::Pointwise, 1, true},
        {PointwiseNodeKind::NeighborhoodBlur, EffectExecutionDomain::Neighborhood, 1, false},
        {PointwiseNodeKind::Neighborhood, EffectExecutionDomain::Neighborhood, 1, false},
        {PointwiseNodeKind::Temporal, EffectExecutionDomain::Temporal, 1, false},
        {PointwiseNodeKind::CpuBoundary, EffectExecutionDomain::CpuBoundary, 1, false},
        {PointwiseNodeKind::SrgbToLinear, EffectExecutionDomain::Pointwise, 0, true},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(static_cast<int>(testCase.kind));
        const auto descriptor = PointwiseEffectFusion::descriptor(testCase.kind);
        EXPECT_EQ(descriptor.domain, testCase.domain);
        EXPECT_EQ(descriptor.parameters.slotCount, testCase.parameterSlots);
        EXPECT_EQ(PointwiseEffectFusion::executionDomain(testCase.kind), testCase.domain);
        EXPECT_EQ(PointwiseEffectFusion::parameterContract(testCase.kind).slotCount,
                  testCase.parameterSlots);

        PointwiseEffectNode node;
        node.kind = testCase.kind;
        EXPECT_EQ(PointwiseEffectFusion::isPointwise(node), testCase.pointwise);
        node.enabled = false;
        EXPECT_FALSE(PointwiseEffectFusion::isPointwise(node));
    }
}

TEST(PointwiseEffectFusionContractTest, StackValidationHonorsMultiSlotBoundaries)
{
    PointwiseEffectStack valid;
    valid.addNode(PointwiseNodeKind::ColorMatrix, 60);
    EXPECT_TRUE(valid.validate().valid);

    PointwiseEffectStack overflow;
    overflow.addNode(PointwiseNodeKind::ColorMatrix, 62);
    const auto overflowResult = overflow.validate();
    EXPECT_FALSE(overflowResult.valid);
    ASSERT_EQ(overflowResult.errors.size(), 1u);
    EXPECT_NE(overflowResult.errors[0].find("3 slot(s)"), std::string::npos);

    PointwiseEffectStack invalidIndex;
    invalidIndex.addNode(PointwiseNodeKind::Exposure, 64);
    EXPECT_FALSE(invalidIndex.validate().valid);

}

TEST(PointwiseEffectFusionContractTest, ParameterWritesHonorSlotBoundsAndPreserveStateOnFailure)
{
    PointwiseEffectStack stack;
    ASSERT_TRUE(stack.setParameter(0, std::array<float, 4>{1.0f, 2.0f, 3.0f, 4.0f}));
    ASSERT_TRUE(stack.setParameter(PointwiseEffectStack::kMaskMixParameterSlot,
                                   0.25f, 0.0f, 0.0f, 0.0f));
    const auto beforeInvalidWrite = stack.parameters();

    EXPECT_FALSE(stack.setParameter(
        static_cast<std::uint32_t>(PointwiseEffectStack::kParameterSlotCount),
        std::array<float, 4>{9.0f, 9.0f, 9.0f, 9.0f}));

    EXPECT_EQ(stack.parameters(), beforeInvalidWrite);
    EXPECT_EQ(stack.parameters()[0], (std::array<float, 4>{1.0f, 2.0f, 3.0f, 4.0f}));
    EXPECT_EQ(stack.parameters()[PointwiseEffectStack::kMaskMixParameterSlot][0], 0.25f);
}

TEST(PointwiseEffectFusionContractTest, AddNodeReturnsStableInsertionIndicesAndKeepsSettings)
{
    PointwiseEffectStack stack;
    const std::uint32_t first = stack.addNode(
        PointwiseNodeKind::Blend, 7, PointwiseBlendMode::Multiply);
    const std::uint32_t second = stack.addNode(PointwiseNodeKind::Exposure, 2);

    EXPECT_EQ(first, 0u);
    EXPECT_EQ(second, 1u);
    ASSERT_EQ(stack.nodes().size(), 2u);
    EXPECT_EQ(stack.nodes()[0].kind, PointwiseNodeKind::Blend);
    EXPECT_EQ(stack.nodes()[0].parameterIndex, 7u);
    EXPECT_EQ(stack.nodes()[0].blendMode, PointwiseBlendMode::Multiply);
    EXPECT_TRUE(stack.nodes()[0].enabled);
    EXPECT_EQ(stack.nodes()[1].kind, PointwiseNodeKind::Exposure);
    EXPECT_EQ(stack.nodes()[1].parameterIndex, 2u);
    EXPECT_EQ(stack.nodes()[1].blendMode, PointwiseBlendMode::Normal);
}

TEST(PointwiseEffectFusionContractTest, EmptyStackIsValidAndProducesNoExecutionSegments)
{
    const PointwiseEffectStack stack;

    EXPECT_TRUE(stack.validate().valid);
    EXPECT_TRUE(stack.segments().empty());
    EXPECT_TRUE(stack.domainSegments().empty());

    const auto diagnostics = describePointwiseFusion(stack.nodes());
    EXPECT_EQ(diagnostics.fusedSegmentCount, 0u);
    EXPECT_EQ(diagnostics.fallbackSegmentCount, 0u);
    EXPECT_EQ(diagnostics.fusedNodeCount, 0u);
    EXPECT_TRUE(diagnostics.messages.empty());
}

TEST(PointwiseEffectFusionContractTest, StackSegmentsAtTemporalPassAndTracksAlpha)
{
    PointwiseEffectStack stack;
    stack.addNode(PointwiseNodeKind::Exposure, 0);
    stack.addNode(PointwiseNodeKind::AlphaConvert, 1);
    stack.addNode(PointwiseNodeKind::Gamma, 2);
    stack.addNode(PointwiseNodeKind::Temporal, 3);
    stack.addNode(PointwiseNodeKind::HueRotate, 4);

    const auto segments = stack.segments(PointwiseAlphaMode::Premultiplied);
    ASSERT_EQ(segments.size(), 3u);
    EXPECT_EQ(segments[0].firstNode, 0u);
    EXPECT_EQ(segments[0].nodeCount, 3u);
    EXPECT_EQ(segments[0].inputAlpha, PointwiseAlphaMode::Premultiplied);
    EXPECT_EQ(segments[0].outputAlpha, PointwiseAlphaMode::Straight);
    EXPECT_TRUE(segments[0].isFused);
    EXPECT_EQ(segments[1].firstNode, 3u);
    EXPECT_FALSE(segments[1].isFused);
    EXPECT_EQ(segments[1].fallbackReason,
              "temporal history requires a pass boundary");
    EXPECT_EQ(segments[2].firstNode, 4u);
    EXPECT_EQ(segments[2].inputAlpha, PointwiseAlphaMode::Straight);
    EXPECT_EQ(segments[2].outputAlpha, PointwiseAlphaMode::Straight);
}

TEST(PointwiseEffectFusionContractTest, GeneratedShaderTransitionsAlphaAroundStraightOnlyNodes)
{
    PointwiseEffectStack stack;
    stack.addNode(PointwiseNodeKind::Exposure, 0);
    stack.addNode(PointwiseNodeKind::AlphaConvert, 1);
    stack.addNode(PointwiseNodeKind::Gamma, 2);
    stack.addNode(PointwiseNodeKind::AlphaConvert, 3);

    const auto segments = stack.segments(PointwiseAlphaMode::Premultiplied);
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].outputAlpha, PointwiseAlphaMode::Premultiplied);

    const auto shader = PointwiseEffectFusion::generateComputeShader(
        "d3d12", "rgba16f", stack.nodes(), segments[0]);
    ASSERT_FALSE(shader.source.empty());
    EXPECT_NE(shader.source.find("color.rgb = ToStraight(color);"),
              std::string::npos);
    EXPECT_NE(shader.source.find(
                  "color.rgb = ToPremultiplied(color.rgb, color.a);"),
              std::string::npos);
}

TEST(PointwiseEffectFusionContractTest, DomainSegmentsGroupAdjacentExecutionDomains)
{
    PointwiseEffectStack stack;
    stack.addNode(PointwiseNodeKind::Exposure, 0);
    stack.addNode(PointwiseNodeKind::Gamma, 1);
    stack.addNode(PointwiseNodeKind::Temporal, 2);
    stack.addNode(PointwiseNodeKind::Temporal, 3);
    stack.addNode(PointwiseNodeKind::CpuBoundary, 4);
    stack.addNode(PointwiseNodeKind::HueRotate, 5);

    const auto segments = stack.domainSegments();
    ASSERT_EQ(segments.size(), 4u);
    EXPECT_EQ(segments[0].firstNode, 0u);
    EXPECT_EQ(segments[0].nodeCount, 2u);
    EXPECT_EQ(segments[0].domain, EffectExecutionDomain::Pointwise);
    EXPECT_EQ(segments[1].firstNode, 2u);
    EXPECT_EQ(segments[1].nodeCount, 2u);
    EXPECT_EQ(segments[1].domain, EffectExecutionDomain::Temporal);
    EXPECT_EQ(segments[2].firstNode, 4u);
    EXPECT_EQ(segments[2].domain, EffectExecutionDomain::CpuBoundary);
    EXPECT_EQ(segments[3].firstNode, 5u);
    EXPECT_EQ(segments[3].domain, EffectExecutionDomain::Pointwise);
}

TEST(PointwiseEffectFusionContractTest, DiagnosticsCountFusedAndFallbackPasses)
{
    const std::vector<PointwiseEffectNode> nodes = {
        {PointwiseNodeKind::Exposure, 0},
        {PointwiseNodeKind::Gamma, 1},
        {PointwiseNodeKind::Temporal, 2},
        {PointwiseNodeKind::HueRotate, 3},
    };

    const auto diagnostics = describePointwiseFusion(nodes);
    EXPECT_EQ(diagnostics.fusedSegmentCount, 1u);
    EXPECT_EQ(diagnostics.fusedNodeCount, 2u);
    EXPECT_EQ(diagnostics.fallbackSegmentCount, 2u);
    ASSERT_EQ(diagnostics.messages.size(), 3u);
    EXPECT_EQ(diagnostics.messages[0], "fused nodes 0-1");
    EXPECT_NE(diagnostics.messages[1].find("temporal history"),
              std::string::npos);
    EXPECT_NE(diagnostics.messages[2].find("single pointwise node"),
              std::string::npos);
}

TEST(PointwiseEffectFusionContractTest, InvalidSegmentProducesDiagnosticWithoutShader)
{
    const std::vector<PointwiseEffectNode> nodes = {
        {PointwiseNodeKind::Exposure, 0}
    };
    PointwiseFusionSegment invalid;
    invalid.firstNode = 1;
    invalid.nodeCount = 1;

    const auto validation = PointwiseEffectFusion::validateSegment(nodes, invalid);
    EXPECT_FALSE(validation.valid);
    ASSERT_EQ(validation.errors.size(), 1u);

    const auto shader = PointwiseEffectFusion::generateComputeShader(
        "d3d12", "rgba16f", nodes, invalid);
    EXPECT_TRUE(shader.source.empty());
    EXPECT_EQ(shader.entryPoint, "PointwiseFusionCS");
    EXPECT_NE(shader.diagnosticName.find("PointwiseFusion_Invalid"),
              std::string::npos);
}

TEST(PointwiseEffectFusionContractTest, SegmentValidationRequiresBackgroundAndLut)
{
    const std::vector<PointwiseEffectNode> blendNodes = {
        {PointwiseNodeKind::Blend, 0}
    };
    PointwiseFusionSegment segment;
    segment.nodeCount = 1;

    EXPECT_FALSE(PointwiseEffectFusion::validateSegment(blendNodes, segment).valid);
    segment.requiresBackground = true;
    EXPECT_TRUE(PointwiseEffectFusion::validateSegment(blendNodes, segment).valid);

    const std::vector<PointwiseEffectNode> lutNodes = {
        {PointwiseNodeKind::Lut3D, 0}
    };
    segment.requiresBackground = false;
    segment.requiresLut = false;
    EXPECT_FALSE(PointwiseEffectFusion::validateSegment(lutNodes, segment).valid);
    segment.requiresLut = true;
    EXPECT_TRUE(PointwiseEffectFusion::validateSegment(lutNodes, segment).valid);
}

TEST(PointwiseEffectFusionContractTest, CompileKeyIncludesOrderAndStaticSpecialization)
{
    const std::vector<PointwiseEffectNode> nodes = {
        {PointwiseNodeKind::Exposure, 0, PointwiseBlendMode::Normal, true,
         false, false},
        {PointwiseNodeKind::Gamma, 1, PointwiseBlendMode::Normal, true,
         false, true},
    };
    PointwiseFusionSegment segment;
    segment.nodeCount = nodes.size();
    segment.isFused = true;

    const auto key = PointwiseEffectFusion::makeCompileKey(
        "d3d12", "rgba16f", nodes, segment);
    ASSERT_EQ(key.orderedNodeKinds.size(), 2u);
    EXPECT_EQ(key.orderedNodeKinds[0], PointwiseNodeKind::Exposure);
    EXPECT_EQ(key.orderedNodeKinds[1], PointwiseNodeKind::Gamma);
    ASSERT_EQ(key.staticSpecializations.size(), 2u);
    EXPECT_FALSE(key.staticSpecializations[0]);
    EXPECT_TRUE(key.staticSpecializations[1]);

    const std::vector<PointwiseEffectNode> reversed = {nodes[1], nodes[0]};
    const auto reversedKey = PointwiseEffectFusion::makeCompileKey(
        "d3d12", "rgba16f", reversed, segment);
    const auto otherBackendKey = PointwiseEffectFusion::makeCompileKey(
        "vulkan", "rgba16f", nodes, segment);
    EXPECT_NE(toStdString(key.toString()), toStdString(reversedKey.toString()));
    EXPECT_NE(toStdString(key.toString()),
              toStdString(otherBackendKey.toString()));
}

TEST(PointwiseEffectFusionContractTest, GeneratedShaderMixesAdjustmentMaskAgainstOriginal)
{
    const std::vector<PointwiseEffectNode> nodes = {
        {PointwiseNodeKind::Exposure, 0}
    };
    PointwiseFusionSegment segment;
    segment.nodeCount = 1;
    segment.requiresMaskMix = true;

    const auto shader = PointwiseEffectFusion::generateComputeShader(
        "d3d12", "rgba16f", nodes, segment);
    ASSERT_FALSE(shader.source.empty());
    EXPECT_NE(shader.source.find("OriginalTexture"), std::string::npos);
    EXPECT_NE(shader.source.find("MaskTexture"), std::string::npos);
    EXPECT_NE(shader.source.find("Parameters[63].x"), std::string::npos);
    EXPECT_NE(shader.source.find("lerp(original, color, maskValue)"),
              std::string::npos);
}

TEST(PointwiseEffectFusionContractTest,
     ShaderCacheSeparatesAdjustmentMaskMixSpecialization)
{
    PointwiseEffectStack stack;
    stack.addNode(PointwiseNodeKind::Exposure, 0);
    const auto segments = stack.segments();
    ASSERT_EQ(segments.size(), 1u);

    PointwiseFusionSegment unmasked = segments[0];
    PointwiseFusionSegment masked = unmasked;
    masked.requiresMaskMix = true;

    PointwiseShaderCache cache;
    const auto unmaskedPlan = cache.makeComputePlan(
        "d3d12", "rgba16f", stack, unmasked, 16, 16);
    const auto maskedPlan = cache.makeComputePlan(
        "d3d12", "rgba16f", stack, masked, 16, 16);

    ASSERT_TRUE(unmaskedPlan.valid());
    ASSERT_TRUE(maskedPlan.valid());
    EXPECT_EQ(cache.entryCount(), 2u);
    EXPECT_EQ(cache.missCount(), 2u);
    EXPECT_EQ(cache.hitCount(), 0u);
    EXPECT_EQ(unmaskedPlan.shader.source.find("MaskTexture"),
              std::string::npos);
    EXPECT_NE(maskedPlan.shader.source.find("MaskTexture"),
              std::string::npos);
    EXPECT_NE(maskedPlan.shader.source.find("lerp(original, color, maskValue)"),
              std::string::npos);
}

TEST(PointwiseEffectFusionContractTest,
     ShaderCacheSeparatesBlendModeSpecialization)
{
    const struct Case {
        PointwiseBlendMode mode;
        const char* expectedExpression;
    } cases[] = {
        {PointwiseBlendMode::Normal,
         "color.rgb = color.rgb + background.rgb * (1.0 - color.a)"},
        {PointwiseBlendMode::Add, "color.rgb += background.rgb"},
        {PointwiseBlendMode::Multiply, "color.rgb *= background.rgb"},
        {PointwiseBlendMode::Screen,
         "1.0 - (1.0 - color.rgb) * (1.0 - background.rgb)"},
    };
    PointwiseShaderCache cache;
    std::size_t expectedEntries = 0;
    for (const Case& testCase : cases) {
        PointwiseEffectStack stack;
        stack.addNode(PointwiseNodeKind::Blend, 0, testCase.mode);
        const auto segments = stack.segments();
        ASSERT_EQ(segments.size(), 1u);

        const auto plan = cache.makeComputePlan(
            "d3d12", "rgba16f", stack, segments[0], 16, 16);
        ASSERT_TRUE(plan.valid());
        EXPECT_NE(plan.shader.source.find(testCase.expectedExpression),
                  std::string::npos)
            << "blend mode " << expectedEntries;
        ++expectedEntries;
        EXPECT_EQ(cache.entryCount(), expectedEntries);
        EXPECT_EQ(cache.missCount(), expectedEntries);
        EXPECT_EQ(cache.hitCount(), 0u);
    }
}

TEST(PointwiseEffectFusionContractTest,
     ShaderCacheSeparatesOutputAlphaSpecialization)
{
    PointwiseEffectStack stack;
    stack.addNode(PointwiseNodeKind::Exposure, 0);
    const auto segments = stack.segments(PointwiseAlphaMode::Premultiplied);
    ASSERT_EQ(segments.size(), 1u);

    PointwiseFusionSegment straightOutput = segments[0];
    straightOutput.outputAlpha = PointwiseAlphaMode::Straight;
    PointwiseFusionSegment premultipliedOutput = segments[0];
    premultipliedOutput.outputAlpha = PointwiseAlphaMode::Premultiplied;

    PointwiseShaderCache cache;
    const auto straightPlan = cache.makeComputePlan(
        "d3d12", "rgba16f", stack, straightOutput, 16, 16);
    const auto premultipliedPlan = cache.makeComputePlan(
        "d3d12", "rgba16f", stack, premultipliedOutput, 16, 16);

    ASSERT_TRUE(straightPlan.valid());
    ASSERT_TRUE(premultipliedPlan.valid());
    EXPECT_EQ(cache.entryCount(), 2u);
    EXPECT_EQ(cache.missCount(), 2u);
    EXPECT_EQ(cache.hitCount(), 0u);
    EXPECT_NE(straightPlan.shader.source.find("color.rgb = ToStraight(color)"),
              std::string::npos);
    EXPECT_EQ(straightPlan.shader.source.find(
                  "color.rgb = ToPremultiplied(color.rgb, color.a)"),
              std::string::npos);
    EXPECT_NE(premultipliedPlan.shader.source.find(
                  "color.rgb = ToPremultiplied(color.rgb, color.a)"),
              std::string::npos);
}

TEST(PointwiseEffectFusionContractTest, ShaderCacheTracksHitsBackendKeysAndDispatchBounds)
{
    PointwiseEffectStack stack;
    stack.addNode(PointwiseNodeKind::Exposure, 0);
    const auto segments = stack.segments();
    ASSERT_EQ(segments.size(), 1u);

    PointwiseShaderCache cache;
    const auto first = cache.makeComputePlan(
        "d3d12", "rgba16f", stack, segments[0], 17, 31);
    ASSERT_TRUE(first.valid());
    EXPECT_EQ(first.dispatchX(), 2u);
    EXPECT_EQ(first.dispatchY(), 2u);
    EXPECT_EQ(cache.missCount(), 1u);
    EXPECT_EQ(cache.hitCount(), 0u);

    const auto second = cache.makeComputePlan(
        "d3d12", "rgba16f", stack, segments[0], 16, 16);
    EXPECT_TRUE(second.valid());
    EXPECT_EQ(cache.missCount(), 1u);
    EXPECT_EQ(cache.hitCount(), 1u);
    EXPECT_EQ(cache.entryCount(), 1u);

    const auto otherBackend = cache.makeComputePlan(
        "vulkan", "rgba16f", stack, segments[0], 16, 16);
    EXPECT_TRUE(otherBackend.valid());
    EXPECT_EQ(cache.missCount(), 2u);
    EXPECT_EQ(cache.entryCount(), 2u);

    cache.clear();
    EXPECT_EQ(cache.entryCount(), 0u);
    EXPECT_EQ(cache.hitCount(), 0u);
    EXPECT_EQ(cache.missCount(), 0u);
}

TEST(PointwiseEffectFusionContractTest,
     ShaderCacheSeparatesTargetFormatSpecialization)
{
    PointwiseEffectStack stack;
    stack.addNode(PointwiseNodeKind::Exposure, 0);
    const auto segments = stack.segments();
    ASSERT_EQ(segments.size(), 1u);

    PointwiseShaderCache cache;
    const auto rgba16f = cache.makeComputePlan(
        "d3d12", "rgba16f", stack, segments[0], 16, 16);
    const auto rgba32f = cache.makeComputePlan(
        "d3d12", "rgba32f", stack, segments[0], 16, 16);

    ASSERT_TRUE(rgba16f.valid());
    ASSERT_TRUE(rgba32f.valid());
    EXPECT_NE(toStdString(rgba16f.shader.key.toString()),
              toStdString(rgba32f.shader.key.toString()));
    EXPECT_EQ(cache.entryCount(), 2u);
    EXPECT_EQ(cache.missCount(), 2u);
    EXPECT_EQ(cache.hitCount(), 0u);
}

TEST(PointwiseEffectFusionContractTest,
     ShaderCacheFindAndGetOrGenerateShareCompileKeyEntries)
{
    PointwiseEffectStack stack;
    stack.addNode(PointwiseNodeKind::Exposure, 0);
    const auto segments = stack.segments();
    ASSERT_EQ(segments.size(), 1u);
    const auto key = PointwiseEffectFusion::makeCompileKey(
        "d3d12", "rgba16f", stack.nodes(), segments[0]);

    PointwiseShaderCache cache;
    EXPECT_EQ(cache.find(key), nullptr);

    const auto& generated = cache.getOrGenerate(
        "d3d12", "rgba16f", stack.nodes(), segments[0]);
    ASSERT_FALSE(generated.source.empty());
    EXPECT_EQ(cache.find(key), &generated);
    EXPECT_EQ(cache.missCount(), 1u);
    EXPECT_EQ(cache.hitCount(), 0u);

    const auto& cached = cache.getOrGenerate(
        "d3d12", "rgba16f", stack.nodes(), segments[0]);
    EXPECT_EQ(&cached, &generated);
    EXPECT_EQ(cache.entryCount(), 1u);
    EXPECT_EQ(cache.missCount(), 1u);
    EXPECT_EQ(cache.hitCount(), 1u);
}

TEST(PointwiseEffectFusionContractTest, NeighborhoodBlurPlanDeclaresClampedSamplingPass)
{
    PointwiseShaderCache cache;
    const auto plan = cache.makeNeighborhoodBlurPlan(
        "d3d12", "rgba16f", 17, 16);

    ASSERT_TRUE(plan.valid());
    EXPECT_EQ(plan.shader.entryPoint, "NeighborhoodBlurCS");
    EXPECT_EQ(plan.parameterBuffer, "NeighborhoodParameters");
    EXPECT_EQ(plan.dispatchX(), 2u);
    EXPECT_EQ(plan.dispatchY(), 1u);
    EXPECT_NE(plan.shader.source.find("NeighborhoodBlurCS"), std::string::npos);
    EXPECT_NE(plan.shader.source.find("clamp(pixel + int2(x, y)"),
              std::string::npos);
    EXPECT_NE(plan.shader.source.find("lerp(SourceTexture.Load"),
              std::string::npos);
}

TEST(PointwiseEffectFusionContractTest, TemporalPlanDeclaresHistoryResourceAndBlend)
{
    PointwiseShaderCache cache;
    const auto plan = cache.makeTemporalBlendPlan(
        "d3d12", "rgba16f", 16, 17);

    ASSERT_TRUE(plan.valid());
    EXPECT_EQ(plan.shader.entryPoint, "TemporalBlendCS");
    EXPECT_EQ(plan.parameterBuffer, "TemporalParameters");
    EXPECT_EQ(plan.dispatchX(), 1u);
    EXPECT_EQ(plan.dispatchY(), 2u);
    EXPECT_TRUE(plan.shader.key.requiresHistory);
    EXPECT_NE(plan.shader.source.find("HistoryTexture"), std::string::npos);
    EXPECT_NE(plan.shader.source.find("saturate(Parameters[0].x)"),
              std::string::npos);
    EXPECT_NE(plan.shader.source.find("lerp("), std::string::npos);
}

TEST(PointwiseEffectFusionContractTest,
     SpecializedPassPlansUseDistinctStableCacheEntries)
{
    PointwiseShaderCache cache;
    const auto blur = cache.makeNeighborhoodBlurPlan(
        "d3d12", "rgba16f", 17, 16);
    const auto temporal = cache.makeTemporalBlendPlan(
        "d3d12", "rgba16f", 16, 17);

    ASSERT_TRUE(blur.valid());
    ASSERT_TRUE(temporal.valid());
    EXPECT_EQ(blur.shader.entryPoint, "NeighborhoodBlurCS");
    EXPECT_EQ(temporal.shader.entryPoint, "TemporalBlendCS");
    EXPECT_EQ(cache.entryCount(), 2u);
    EXPECT_EQ(cache.missCount(), 2u);
    EXPECT_EQ(cache.hitCount(), 0u);

    const auto blurAtOtherSize = cache.makeNeighborhoodBlurPlan(
        "d3d12", "rgba16f", 64, 32);
    const auto temporalAtOtherSize = cache.makeTemporalBlendPlan(
        "d3d12", "rgba16f", 32, 64);
    EXPECT_TRUE(blurAtOtherSize.valid());
    EXPECT_TRUE(temporalAtOtherSize.valid());
    EXPECT_EQ(blurAtOtherSize.shader.entryPoint, "NeighborhoodBlurCS");
    EXPECT_EQ(temporalAtOtherSize.shader.entryPoint, "TemporalBlendCS");
    EXPECT_EQ(cache.entryCount(), 2u);
    EXPECT_EQ(cache.missCount(), 2u);
    EXPECT_EQ(cache.hitCount(), 2u);
}

TEST(PointwiseEffectFusionContractTest, ComputePlanRejectsZeroDimensions)
{
    PointwiseEffectStack stack;
    stack.addNode(PointwiseNodeKind::Exposure, 0);
    const auto segments = stack.segments();
    ASSERT_EQ(segments.size(), 1u);

    PointwiseShaderCache cache;
    const auto zeroWidth = cache.makeComputePlan(
        "d3d12", "rgba16f", stack, segments[0], 0, 32);
    const auto zeroHeight = cache.makeComputePlan(
        "d3d12", "rgba16f", stack, segments[0], 32, 0);
    EXPECT_FALSE(zeroWidth.valid());
    EXPECT_FALSE(zeroHeight.valid());
}

TEST(PointwiseEffectFusionContractTest,
     ComputeDispatchDimensionsDoNotOverflowAtUint32Maximum)
{
    PointwiseEffectStack stack;
    stack.addNode(PointwiseNodeKind::Exposure, 0);
    const auto segments = stack.segments();
    ASSERT_EQ(segments.size(), 1u);

    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    PointwiseShaderCache cache;
    const auto plan = cache.makeComputePlan(
        "d3d12", "rgba16f", stack, segments[0], maximum, maximum);

    ASSERT_TRUE(plan.valid());
    constexpr std::uint32_t expectedGroups =
        maximum / 16u + (maximum % 16u != 0u ? 1u : 0u);
    EXPECT_EQ(plan.dispatchX(), expectedGroups);
    EXPECT_EQ(plan.dispatchY(), expectedGroups);
}
