#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

import Artifact.ShaderNode.Core;

namespace {

using Artifact::ShaderNode::MaterialOutputNode;
using Artifact::ShaderNode::MaterialGraphLayout;
using Artifact::ShaderNode::ColorRampNode;
using Artifact::ShaderNode::MathNode;
using Artifact::ShaderNode::MixRGBNode;
using Artifact::ShaderNode::NodeGraph;
using Artifact::ShaderNode::NoiseTextureNode;
using Artifact::ShaderNode::PrincipledBSDFNode;
using Artifact::ShaderNode::RGBNode;
using Artifact::ShaderNode::ValueNode;

TEST(ShaderGraphContractTest, VersionedGraphRoundTripsAndCompilesDeterministically)
{
    NodeGraph graph;
    auto* color = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("base_color")));
    auto* principled = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));

    graph.link(color->outputs[0].get(), principled->inputs[0].get());
    graph.link(principled->outputs[0].get(), output->inputs[0].get());

    const std::string encoded = graph.toJson({});
    NodeGraph restored;
    const auto loaded = restored.fromJson(encoded);
    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_EQ(restored.toJson({}), encoded);

    const auto first = graph.compileMaterialGraph();
    const auto second = restored.compileMaterialGraph();
    ASSERT_TRUE(first.ok) << first.error;
    ASSERT_TRUE(second.ok) << second.error;
    EXPECT_EQ(first.helperHlsl, second.helperHlsl);
    EXPECT_EQ(first.blockHlsl, second.blockHlsl);
    EXPECT_EQ(first.hashHex, second.hashHex);
}

TEST(ShaderGraphContractTest, CanonicalSerializationIgnoresNodeAndLinkInsertionOrder)
{
    NodeGraph forward;
    auto* forwardColor = static_cast<RGBNode*>(
        forward.addNode(std::make_unique<RGBNode>("base_color")));
    auto* forwardRoughness = static_cast<ValueNode*>(
        forward.addNode(std::make_unique<ValueNode>("roughness")));
    auto* forwardSurface = static_cast<PrincipledBSDFNode*>(
        forward.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* forwardOutput = static_cast<MaterialOutputNode*>(
        forward.addNode(std::make_unique<MaterialOutputNode>("output")));
    forward.link(forwardColor->outputs[0].get(), forwardSurface->inputs[0].get());
    forward.link(forwardRoughness->outputs[0].get(),
                 forwardSurface->inputs[2].get());
    forward.link(forwardSurface->outputs[0].get(),
                 forwardOutput->inputs[0].get());

    NodeGraph reverse;
    auto* reverseOutput = static_cast<MaterialOutputNode*>(
        reverse.addNode(std::make_unique<MaterialOutputNode>("output")));
    auto* reverseSurface = static_cast<PrincipledBSDFNode*>(
        reverse.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* reverseRoughness = static_cast<ValueNode*>(
        reverse.addNode(std::make_unique<ValueNode>("roughness")));
    auto* reverseColor = static_cast<RGBNode*>(
        reverse.addNode(std::make_unique<RGBNode>("base_color")));
    reverse.link(reverseSurface->outputs[0].get(),
                 reverseOutput->inputs[0].get());
    reverse.link(reverseRoughness->outputs[0].get(),
                 reverseSurface->inputs[2].get());
    reverse.link(reverseColor->outputs[0].get(),
                 reverseSurface->inputs[0].get());

    EXPECT_EQ(forward.toJson({}), reverse.toJson({}))
        << "Equivalent graph documents should serialize canonically regardless "
           "of node and link insertion order";
}

TEST(ShaderGraphContractTest, LoweringIgnoresNodeAndLinkInsertionOrder)
{
    NodeGraph forward;
    auto* forwardColor = static_cast<RGBNode*>(
        forward.addNode(std::make_unique<RGBNode>("base_color")));
    auto* forwardRoughness = static_cast<ValueNode*>(
        forward.addNode(std::make_unique<ValueNode>("roughness", 0.25f)));
    auto* forwardSurface = static_cast<PrincipledBSDFNode*>(
        forward.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* forwardOutput = static_cast<MaterialOutputNode*>(
        forward.addNode(std::make_unique<MaterialOutputNode>("output")));
    forward.link(forwardColor->outputs[0].get(), forwardSurface->inputs[0].get());
    forward.link(forwardRoughness->outputs[0].get(), forwardSurface->inputs[2].get());
    forward.link(forwardSurface->outputs[0].get(), forwardOutput->inputs[0].get());

    NodeGraph reverse;
    auto* reverseOutput = static_cast<MaterialOutputNode*>(
        reverse.addNode(std::make_unique<MaterialOutputNode>("output")));
    auto* reverseSurface = static_cast<PrincipledBSDFNode*>(
        reverse.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* reverseRoughness = static_cast<ValueNode*>(
        reverse.addNode(std::make_unique<ValueNode>("roughness", 0.25f)));
    auto* reverseColor = static_cast<RGBNode*>(
        reverse.addNode(std::make_unique<RGBNode>("base_color")));
    reverse.link(reverseSurface->outputs[0].get(), reverseOutput->inputs[0].get());
    reverse.link(reverseRoughness->outputs[0].get(), reverseSurface->inputs[2].get());
    reverse.link(reverseColor->outputs[0].get(), reverseSurface->inputs[0].get());

    const auto forwardResult = forward.compileMaterialGraph();
    const auto reverseResult = reverse.compileMaterialGraph();
    ASSERT_TRUE(forwardResult.ok) << forwardResult.error;
    ASSERT_TRUE(reverseResult.ok) << reverseResult.error;
    EXPECT_EQ(forwardResult.helperHlsl, reverseResult.helperHlsl);
    EXPECT_EQ(forwardResult.blockHlsl, reverseResult.blockHlsl);
    EXPECT_EQ(forwardResult.hashHex, reverseResult.hashHex)
        << "Equivalent single-output graphs should lower independently of storage order";
}

TEST(ShaderGraphContractTest, LayoutRoundTripsWithoutChangingSemanticCompileHash)
{
    NodeGraph graph;
    auto* color = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("base_color")));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));
    graph.link(color->outputs[0].get(), surface->inputs[0].get());
    graph.link(surface->outputs[0].get(), output->inputs[0].get());

    const std::vector<MaterialGraphLayout> firstLayout{
        {"base_color", -240.0f, 32.0f},
        {"surface", 40.0f, 32.0f},
        {"output", 360.0f, 32.0f}};
    const std::vector<MaterialGraphLayout> secondLayout{
        {"base_color", 120.0f, -80.0f},
        {"surface", -160.0f, 240.0f},
        {"output", 24.0f, 520.0f}};
    const std::string firstDocument = graph.toJson(firstLayout);
    const std::string secondDocument = graph.toJson(secondLayout);
    EXPECT_NE(firstDocument, secondDocument)
        << "Node positions must remain part of the persisted editor document";

    NodeGraph restored;
    const auto loaded = restored.fromJson(firstDocument);
    ASSERT_TRUE(loaded.ok) << loaded.error;
    ASSERT_EQ(loaded.layout.size(), firstLayout.size());
    EXPECT_EQ(loaded.layout[0].nodeId, "base_color");
    EXPECT_FLOAT_EQ(loaded.layout[0].x, -240.0f);
    EXPECT_FLOAT_EQ(loaded.layout[0].y, 32.0f);

    NodeGraph restoredSecond;
    const auto secondLoaded = restoredSecond.fromJson(secondDocument);
    ASSERT_TRUE(secondLoaded.ok) << secondLoaded.error;
    ASSERT_EQ(secondLoaded.layout.size(), secondLayout.size());
    EXPECT_FLOAT_EQ(secondLoaded.layout[0].x, 120.0f);
    EXPECT_FLOAT_EQ(secondLoaded.layout[0].y, -80.0f);
    EXPECT_EQ(restoredSecond.toJson(secondLoaded.layout), secondDocument)
        << "The second layout payload must also survive load/save unchanged";

    const auto originalCompile = graph.compileMaterialGraph();
    const auto restoredCompile = restored.compileMaterialGraph();
    const auto secondRestoredCompile = restoredSecond.compileMaterialGraph();
    ASSERT_TRUE(originalCompile.ok) << originalCompile.error;
    ASSERT_TRUE(restoredCompile.ok) << restoredCompile.error;
    ASSERT_TRUE(secondRestoredCompile.ok) << secondRestoredCompile.error;
    EXPECT_EQ(originalCompile.helperHlsl, restoredCompile.helperHlsl);
    EXPECT_EQ(originalCompile.helperHlsl, secondRestoredCompile.helperHlsl);
    EXPECT_EQ(originalCompile.blockHlsl, restoredCompile.blockHlsl);
    EXPECT_EQ(originalCompile.blockHlsl, secondRestoredCompile.blockHlsl);
    EXPECT_EQ(originalCompile.hashHex, restoredCompile.hashHex)
        << "Editor layout must not affect the current shader-source hash";
    EXPECT_EQ(originalCompile.hashHex, secondRestoredCompile.hashHex)
        << "Both persisted layouts must remain outside shader-source identity";
}

TEST(ShaderGraphContractTest, SemanticChangeChangesCurrentCompileHash)
{
    NodeGraph graph;
    auto* color = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("base_color")));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));
    graph.link(color->outputs[0].get(), surface->inputs[0].get());
    graph.link(surface->outputs[0].get(), output->inputs[0].get());

    const auto initial = graph.compileMaterialGraph();
    ASSERT_TRUE(initial.ok) << initial.error;
    color->r = 0.25f;
    const auto changed = graph.compileMaterialGraph();
    ASSERT_TRUE(changed.ok) << changed.error;
    EXPECT_NE(initial.hashHex, changed.hashHex)
        << "A source-affecting graph parameter change must change the HLSL hash";
}

TEST(ShaderGraphContractTest, CompileHashTracksHelperAndBlockSource)
{
    NodeGraph withNoise;
    auto* noise = static_cast<NoiseTextureNode*>(
        withNoise.addNode(std::make_unique<NoiseTextureNode>("noise")));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        withNoise.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        withNoise.addNode(std::make_unique<MaterialOutputNode>("output")));
    withNoise.link(noise->outputs[0].get(), surface->inputs[0].get());
    withNoise.link(surface->outputs[0].get(), output->inputs[0].get());

    NodeGraph withoutNoise;
    auto* plainColor = static_cast<RGBNode*>(
        withoutNoise.addNode(std::make_unique<RGBNode>("plain_color")));
    auto* plainSurface = static_cast<PrincipledBSDFNode*>(
        withoutNoise.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* plainOutput = static_cast<MaterialOutputNode*>(
        withoutNoise.addNode(std::make_unique<MaterialOutputNode>("output")));
    withoutNoise.link(plainColor->outputs[0].get(), plainSurface->inputs[0].get());
    withoutNoise.link(plainSurface->outputs[0].get(), plainOutput->inputs[0].get());

    const auto procedural = withNoise.compileMaterialGraph();
    const auto constant = withoutNoise.compileMaterialGraph();
    ASSERT_TRUE(procedural.ok) << procedural.error;
    ASSERT_TRUE(constant.ok) << constant.error;
    EXPECT_NE(procedural.helperHlsl, constant.helperHlsl);
    EXPECT_NE(procedural.hashHex, constant.hashHex)
        << "The compile hash must include helper source as well as block source";
}

TEST(ShaderGraphContractTest, ColorRampLoweringSortsDistinctStopsByPosition)
{
    const auto makeRampGraph = [](bool reverseStopOrder) {
        NodeGraph graph;
        auto* ramp = static_cast<ColorRampNode*>(
            graph.addNode(std::make_unique<ColorRampNode>("ramp")));
        auto* surface = static_cast<PrincipledBSDFNode*>(
            graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
        auto* output = static_cast<MaterialOutputNode*>(
            graph.addNode(std::make_unique<MaterialOutputNode>("output")));
        const ColorRampNode::Stop low{0.0f, 0.1f, 0.2f, 0.3f, 1.0f};
        const ColorRampNode::Stop middle{0.5f, 0.4f, 0.5f, 0.6f, 1.0f};
        const ColorRampNode::Stop high{1.0f, 0.7f, 0.8f, 0.9f, 1.0f};
        ramp->stops = reverseStopOrder
            ? std::vector<ColorRampNode::Stop>{high, middle, low}
            : std::vector<ColorRampNode::Stop>{low, middle, high};
        graph.link(ramp->outputs[0].get(), surface->inputs[0].get());
        graph.link(surface->outputs[0].get(), output->inputs[0].get());
        return graph;
    };

    auto ordered = makeRampGraph(false);
    auto reversed = makeRampGraph(true);
    const auto orderedCompile = ordered.compileMaterialGraph();
    const auto reversedCompile = reversed.compileMaterialGraph();
    ASSERT_TRUE(orderedCompile.ok) << orderedCompile.error;
    ASSERT_TRUE(reversedCompile.ok) << reversedCompile.error;
    EXPECT_EQ(orderedCompile.blockHlsl, reversedCompile.blockHlsl);
    EXPECT_EQ(orderedCompile.hashHex, reversedCompile.hashHex)
        << "Distinct Color Ramp stops must lower by position, not JSON/vector order";
}

TEST(ShaderGraphContractTest, ColorRampInterpolationUsesExplicitDomainAndEaseMath)
{
    NodeGraph graph;
    auto* ramp = static_cast<ColorRampNode*>(
        graph.addNode(std::make_unique<ColorRampNode>("ramp")));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));
    ramp->interp = ColorRampNode::Interp::Ease;
    ramp->stops = {
        {0.25f, 0.1f, 0.2f, 0.3f, 1.0f},
        {0.75f, 0.9f, 0.8f, 0.7f, 0.5f},
    };
    graph.link(ramp->outputs[0].get(), surface->inputs[0].get());
    graph.link(surface->outputs[0].get(), output->inputs[0].get());

    const auto compiled = graph.compileMaterialGraph();
    ASSERT_TRUE(compiled.ok) << compiled.error;
    for (const std::string_view fragment : {
             "float ramp_t = saturate(0.5f)",
             "float4 ramp_ramp = float4(0.1f,0.2f,0.3f,1.0f)",
             "saturate((ramp_t - 0.25f) / 0.5f)",
             "ramp_ramp = lerp(ramp_ramp, float4(0.9f,0.8f,0.7f,0.5f)",
             "3.0f - 2.0f *"}) {
        EXPECT_NE(compiled.blockHlsl.find(fragment), std::string::npos) << fragment;
    }
}

TEST(ShaderGraphContractTest, ColorRampFixtureClampsStopPositionsBeforeInterpolation)
{
    const auto fixturePath = std::filesystem::path(__FILE__).parent_path().parent_path() /
        "fixtures" / "shader-graph-rd" / "ramp_position_domain_characterization.json";
    std::ifstream fixtureFile(fixturePath, std::ios::binary);
    ASSERT_TRUE(fixtureFile.is_open()) << fixturePath.string();
    const std::string payload{
        std::istreambuf_iterator<char>(fixtureFile),
        std::istreambuf_iterator<char>()};

    NodeGraph graph;
    const auto loaded = graph.fromJson(payload);
    ASSERT_TRUE(loaded.ok) << loaded.error;
    const auto compiled = graph.compileMaterialGraph();
    ASSERT_TRUE(compiled.ok) << compiled.error;
    EXPECT_NE(compiled.blockHlsl.find(
                  "float4 ramp_ramp = float4(0.1f,0.2f,0.3f,1.0f)"),
              std::string::npos);
    EXPECT_NE(compiled.blockHlsl.find(
                  "saturate((ramp_t - 0.0f) / 1.0f)"),
              std::string::npos);
    EXPECT_NE(compiled.blockHlsl.find(
                  "ramp_ramp = lerp(ramp_ramp, float4(0.9f,0.8f,0.7f,0.5f)"),
              std::string::npos);
}

TEST(ShaderGraphContractTest, ColorRampDuplicateStopOrderRemainsSemanticInDocument)
{
    const auto fixtureRoot = std::filesystem::path(__FILE__).parent_path().parent_path() /
        "fixtures" / "shader-graph-rd";
    const auto readFixture = [&](const char* name) {
        std::ifstream file(fixtureRoot / name, std::ios::binary);
        EXPECT_TRUE(file.is_open()) << name;
        return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    };

    const std::string forward = readFixture("ramp_duplicate_stops_forward.json");
    const std::string reversed = readFixture("ramp_duplicate_stops_reversed.json");
    ASSERT_FALSE(forward.empty());
    ASSERT_FALSE(reversed.empty());

    NodeGraph forwardGraph;
    NodeGraph reversedGraph;
    ASSERT_TRUE(forwardGraph.fromJson(forward).ok);
    ASSERT_TRUE(reversedGraph.fromJson(reversed).ok);

    const auto forwardCompiled = forwardGraph.compileMaterialGraph();
    const auto reversedCompiled = reversedGraph.compileMaterialGraph();
    ASSERT_TRUE(forwardCompiled.ok) << forwardCompiled.error;
    ASSERT_TRUE(reversedCompiled.ok) << reversedCompiled.error;

    EXPECT_NE(forwardGraph.toJson(), reversedGraph.toJson());
    // Legacy std::sort tie ordering is unspecified; the candidate Core rule is covered by the CPU formula golden.
}

TEST(ShaderGraphContractTest, LegacyScalarColorConversionsHaveExplicitExpressions)
{
    NodeGraph graph;
    auto* color = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("color")));
    auto* scalar = static_cast<ValueNode*>(
        graph.addNode(std::make_unique<ValueNode>("scalar", 0.25f)));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));
    graph.link(color->outputs[0].get(), surface->inputs[1].get());
    graph.link(scalar->outputs[0].get(), surface->inputs[0].get());
    graph.link(surface->outputs[0].get(), output->inputs[0].get());

    const auto compiled = graph.compileMaterialGraph();
    ASSERT_TRUE(compiled.ok) << compiled.error;
    EXPECT_NE(compiled.blockHlsl.find(
                  "float surface_metallic = saturate(color_Color.x);"),
              std::string::npos)
        << "Legacy Vector4 to Float conversion selects the red/x component";
    EXPECT_NE(compiled.blockHlsl.find(
                  "float4 surface_baseColor = float4(scalar_Value,scalar_Value,scalar_Value,1.0f);"),
              std::string::npos)
        << "Legacy Float to Vector4 conversion replicates RGB and supplies alpha 1";
}

TEST(ShaderGraphContractTest, LegacyUvAndVectorConversionsHaveExplicitExpressions)
{
    NodeGraph graph;
    auto* texCoord = static_cast<Artifact::ShaderNode::TexCoordNode*>(
        graph.addNode(std::make_unique<Artifact::ShaderNode::TexCoordNode>("uv")));
    auto* geometry = static_cast<Artifact::ShaderNode::GeometryNode*>(
        graph.addNode(std::make_unique<Artifact::ShaderNode::GeometryNode>("geometry")));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));
    graph.link(texCoord->outputs[0].get(), surface->inputs[6].get());
    graph.link(geometry->outputs[0].get(), surface->inputs[0].get());
    graph.link(surface->outputs[0].get(), output->inputs[0].get());

    const auto compiled = graph.compileMaterialGraph();
    ASSERT_TRUE(compiled.ok) << compiled.error;
    EXPECT_NE(compiled.blockHlsl.find(
                  "float3 surface_normal = float3(uv_UV.xy,0.0f);"),
              std::string::npos)
        << "Legacy Vector2 to Vector3 conversion pads z with zero";
    EXPECT_NE(compiled.blockHlsl.find(
                  "float4 surface_baseColor = float4(geometry_Position.xyz,1.0f);"),
              std::string::npos)
        << "Legacy Vector3 to Vector4 conversion supplies alpha 1";
}

TEST(ShaderGraphContractTest, AllLegacySocketConversionsHaveExplicitExpressions)
{
    NodeGraph graph;
    auto* uv = static_cast<Artifact::ShaderNode::TexCoordNode*>(
        graph.addNode(std::make_unique<Artifact::ShaderNode::TexCoordNode>("uv")));
    auto* geometry = static_cast<Artifact::ShaderNode::GeometryNode*>(
        graph.addNode(std::make_unique<Artifact::ShaderNode::GeometryNode>("geometry")));
    auto* color = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("color")));
    auto* scalar = static_cast<ValueNode*>(
        graph.addNode(std::make_unique<ValueNode>("scalar", 0.25f)));
    auto* noiseFromFloat = static_cast<NoiseTextureNode*>(
        graph.addNode(std::make_unique<NoiseTextureNode>("noise_from_float")));
    auto* noiseFromVector3 = static_cast<NoiseTextureNode*>(
        graph.addNode(std::make_unique<NoiseTextureNode>("noise_from_vector3")));
    auto* noiseFromVector4 = static_cast<NoiseTextureNode*>(
        graph.addNode(std::make_unique<NoiseTextureNode>("noise_from_vector4")));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));

    // Float -> Vector2, Vector3 -> Vector2, and Vector4 -> Vector2.
    graph.link(scalar->outputs[0].get(), noiseFromFloat->inputs[0].get());
    graph.link(geometry->outputs[0].get(), noiseFromVector3->inputs[0].get());
    graph.link(color->outputs[0].get(), noiseFromVector4->inputs[0].get());
    graph.link(color->outputs[0].get(), noiseFromVector4->inputs[1].get());
    graph.link(noiseFromFloat->outputs[0].get(), surface->inputs[7].get());
    graph.link(noiseFromVector3->outputs[0].get(), surface->inputs[2].get());
    graph.link(noiseFromVector4->outputs[0].get(), surface->inputs[4].get());

    // Vector2/Vector3/Vector4 -> Float.
    graph.link(uv->outputs[0].get(), surface->inputs[1].get());
    graph.link(geometry->outputs[0].get(), surface->inputs[5].get());

    // Vector2/Vector3 -> Vector4; Vector4 -> Vector3.
    graph.link(uv->outputs[0].get(), surface->inputs[0].get());
    graph.link(geometry->outputs[0].get(), surface->inputs[3].get());
    graph.link(scalar->outputs[0].get(), surface->inputs[6].get());
    graph.link(color->outputs[0].get(), output->inputs[1].get());

    graph.link(surface->outputs[0].get(), output->inputs[0].get());
    const auto compiled = graph.compileMaterialGraph();
    ASSERT_TRUE(compiled.ok) << compiled.error;
    const auto& block = compiled.blockHlsl;
    for (const std::string_view expression : {
             "float2(scalar_Value,scalar_Value)",
             "float2(geometry_Position.xy)",
             "float2(color_Color.xy)",
             "uv_UV.x", "geometry_Position.x", "color_Color.x",
             "float3(scalar_Value,scalar_Value,scalar_Value)",
             "float4(uv_UV.xy,0.0f,1.0f)",
             "float4(geometry_Position.xyz,1.0f)",
             "color_Color.xyz"}) {
        EXPECT_NE(block.find(expression), std::string::npos)
            << "Missing explicit legacy conversion expression: " << expression;
    }
}

TEST(ShaderGraphContractTest, CycleProducesExplicitFailureFallback)
{
    NodeGraph graph;
    auto* first = static_cast<MathNode*>(
        graph.addNode(std::make_unique<MathNode>("first")));
    auto* second = static_cast<MathNode*>(
        graph.addNode(std::make_unique<MathNode>("second")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));

    graph.link(first->outputs[0].get(), second->inputs[0].get());
    graph.link(second->outputs[0].get(), first->inputs[0].get());
    graph.link(first->outputs[0].get(), output->inputs[0].get());

    const auto result = graph.compileMaterialGraph();
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("cycle"), std::string::npos);
    EXPECT_NE(result.blockHlsl.find("float4(1.0f,0.0f,1.0f,1.0f)"),
              std::string::npos);
}

TEST(ShaderGraphContractTest, MissingOutputDoesNotProduceApplicableGraph)
{
    NodeGraph graph;
    graph.addNode(std::make_unique<RGBNode>("unused_color"));

    const auto result = graph.compileMaterialGraph();
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("no Material Output"), std::string::npos);
    EXPECT_NE(result.blockHlsl.find("graphBaseColor"), std::string::npos);
}

TEST(ShaderGraphContractTest, DocumentLoadCanSucceedWhenMaterialLoweringFails)
{
    const auto fixturePath = std::filesystem::path(__FILE__).parent_path().parent_path() /
        "fixtures" / "shader-graph-rd" / "missing_output_compile_failure.json";
    std::ifstream fixtureFile(fixturePath, std::ios::binary);
    ASSERT_TRUE(fixtureFile.is_open()) << fixturePath.string();
    const std::string payload{
        std::istreambuf_iterator<char>(fixtureFile),
        std::istreambuf_iterator<char>()};

    NodeGraph graph;
    const auto loaded = graph.fromJson(payload);
    ASSERT_TRUE(loaded.ok) << loaded.error;

    const auto lowered = graph.compileMaterialGraph();
    EXPECT_FALSE(lowered.ok);
    EXPECT_NE(lowered.error.find("no Material Output"), std::string::npos);
}

TEST(ShaderGraphContractTest, MultipleMaterialOutputsRequireExplicitSelection)
{
    NodeGraph graph;
    graph.addNode(std::make_unique<MaterialOutputNode>("output_a"));
    graph.addNode(std::make_unique<MaterialOutputNode>("output_b"));

    const auto result = graph.compileMaterialGraph();
    EXPECT_FALSE(result.ok)
        << "A graph with multiple Material Output nodes must not select one by storage order";
    EXPECT_NE(result.error.find("multiple Material Output"), std::string::npos)
        << "Ambiguous output selection must have a stable diagnostic";
}

TEST(ShaderGraphContractTest, FailedLoadPreservesExistingGraph)
{
    NodeGraph graph;
    auto* color = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("preserved_color")));
    auto* principled = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("preserved_surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("preserved_output")));
    graph.link(color->outputs[0].get(), principled->inputs[0].get());
    graph.link(principled->outputs[0].get(), output->inputs[0].get());
    const std::string before = graph.toJson({});

    const std::array<std::string, 5> invalidJsonCases{
        "{",
        R"({"version":99,"nodes":[],"links":[]})",
        R"({"version":1,"nodes":[{"id":"unknown","type":"NotANode"}],"links":[]})",
        R"({"version":1,"kind":"artifact-material-graph","nodes":[{"id":"duplicate","type":"RGB"},{"id":"duplicate","type":"Value"}],"links":[]})",
        R"({"version":1,"kind":"artifact-material-graph","nodes":[{"id":"","type":"RGB"}],"links":[]})",
    };
    for (const std::string& invalidJson : invalidJsonCases) {
        const auto loaded = graph.fromJson(invalidJson);
        EXPECT_FALSE(loaded.ok);
        EXPECT_FALSE(loaded.error.empty());
        EXPECT_EQ(graph.toJson({}), before);
    }

    const std::array<std::string, 2> invalidDocumentCases{
        R"({"version":1,"kind":"unrelated-document","nodes":[{"id":"output","type":"MaterialOutput"}],"links":[]})",
        R"({"version":1,"kind":"artifact-material-graph"})",
    };
    for (const std::string& invalidJson : invalidDocumentCases) {
        const auto loaded = graph.fromJson(invalidJson);
        EXPECT_FALSE(loaded.ok);
        EXPECT_FALSE(loaded.error.empty());
        EXPECT_EQ(graph.toJson({}), before);
    }
}

TEST(ShaderGraphContractTest, TypeMismatchLinkDoesNotReportSuccessfulDocumentLoad)
{
    NodeGraph graph;
    auto* existingColor = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("existing_color")));
    auto* existingSurface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("existing_surface")));
    auto* existingOutput = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("existing_output")));
    graph.link(existingColor->outputs[0].get(), existingSurface->inputs[0].get());
    graph.link(existingSurface->outputs[0].get(), existingOutput->inputs[0].get());
    const std::string before = graph.toJson({});
    const auto loaded = graph.fromJson(
        R"({"version":1,"kind":"artifact-material-graph","nodes":[{"id":"scalar","type":"Value","params":{"value":0.75}},{"id":"surface","type":"Principled","params":{}},{"id":"output","type":"MaterialOutput","params":{}}],"links":[{"from":"scalar","out":"Value","to":"surface","in":"BaseColor"},{"from":"surface","out":"BSDF","to":"output","in":"Surface"}]})");

    EXPECT_FALSE(loaded.ok)
        << "A skipped invalid link must not be reported as a successful graph load";
    EXPECT_FALSE(loaded.error.empty());
    EXPECT_EQ(graph.toJson({}), before)
        << "A document with a type-mismatched link must not partially replace the existing graph";
}

TEST(ShaderGraphContractTest, DuplicateInputLinksAreRejectedWithoutReplacingExistingGraph)
{
    NodeGraph graph;
    auto* existingColor = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("existing_color")));
    auto* existingSurface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("existing_surface")));
    auto* existingOutput = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("existing_output")));
    graph.link(existingColor->outputs[0].get(),
               existingSurface->inputs[0].get());
    graph.link(existingSurface->outputs[0].get(),
               existingOutput->inputs[0].get());
    const std::string before = graph.toJson({});

    const auto loaded = graph.fromJson(
        R"({"version":1,"kind":"artifact-material-graph","nodes":[{"id":"red","type":"RGB","params":{"r":1,"g":0,"b":0,"a":1}},{"id":"blue","type":"RGB","params":{"r":0,"g":0,"b":1,"a":1}},{"id":"surface","type":"Principled","params":{}},{"id":"output","type":"MaterialOutput","params":{}}],"links":[{"from":"red","out":"Color","to":"surface","in":"BaseColor"},{"from":"blue","out":"Color","to":"surface","in":"BaseColor"},{"from":"surface","out":"BSDF","to":"output","in":"Surface"}]})");

    EXPECT_FALSE(loaded.ok)
        << "A single-writer socket must reject a second incoming link";
    EXPECT_FALSE(loaded.error.empty());
    EXPECT_EQ(graph.toJson({}), before)
        << "A rejected partial graph must not replace the previous document";
}

TEST(ShaderGraphContractTest, FiniteJsonNumberOutsideFloatRangeIsDiagnosed)
{
    NodeGraph graph;
    const auto loaded = graph.fromJson(
        R"({"version":1,"kind":"artifact-material-graph","nodes":[{"id":"large_value","type":"Value","params":{"value":1e100}},{"id":"surface","type":"Principled","params":{}},{"id":"output","type":"MaterialOutput","params":{}}],"links":[{"from":"large_value","out":"Value","to":"surface","in":"Metallic"},{"from":"surface","out":"BSDF","to":"output","in":"Surface"}]})");

    EXPECT_TRUE(!loaded.ok || !loaded.error.empty())
        << "A finite JSON double outside binary32 range must be rejected or normalized with a diagnostic";
}

TEST(ShaderGraphContractTest, OutOfRangeNodeEnumIsDiagnosed)
{
    NodeGraph graph;
    const auto loaded = graph.fromJson(
        R"({"version":1,"kind":"artifact-material-graph","nodes":[{"id":"math","type":"Math","params":{"op":17,"clamp":false}},{"id":"surface","type":"Principled","params":{}},{"id":"output","type":"MaterialOutput","params":{}}],"links":[{"from":"math","out":"Value","to":"surface","in":"BaseColor"},{"from":"surface","out":"BSDF","to":"output","in":"Surface"}]})");

    EXPECT_TRUE(!loaded.ok || !loaded.error.empty())
        << "Out-of-range serialized enum values must be rejected or explicitly diagnosed when normalized";
}

TEST(ShaderGraphContractTest, NodeOwnedParametersRoundTrip)
{
    NodeGraph graph;
    auto* value = static_cast<ValueNode*>(
        graph.addNode(std::make_unique<ValueNode>("value", 0.375f)));
    auto* color = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("color")));
    color->r = 0.1f;
    color->g = 0.2f;
    color->b = 0.3f;
    color->a = 0.4f;
    auto* math = static_cast<MathNode*>(
        graph.addNode(std::make_unique<MathNode>("math", MathNode::Op::Fract)));
    math->clampResult = true;
    auto* mix = static_cast<MixRGBNode*>(
        graph.addNode(std::make_unique<MixRGBNode>(
            "mix", MixRGBNode::Blend::Screen)));
    mix->clampFactor = false;
    mix->clampResult = true;
    auto* ramp = static_cast<Artifact::ShaderNode::ColorRampNode*>(
        graph.addNode(std::make_unique<Artifact::ShaderNode::ColorRampNode>("ramp")));
    ramp->interp = Artifact::ShaderNode::ColorRampNode::Interp::Ease;
    ramp->stops = {
        {0.2f, 0.8f, 0.1f, 0.2f, 0.4f},
        {0.9f, 0.1f, 0.7f, 0.3f, 1.0f},
    };
    auto* texture = static_cast<Artifact::ShaderNode::ImageTextureNode*>(
        graph.addNode(std::make_unique<Artifact::ShaderNode::ImageTextureNode>(
            "texture", Artifact::ShaderNode::ImageTextureNode::Slot::Opacity)));
    auto* mapRange = static_cast<Artifact::ShaderNode::MapRangeNode*>(
        graph.addNode(std::make_unique<Artifact::ShaderNode::MapRangeNode>("map")));
    mapRange->interp = Artifact::ShaderNode::MapRangeNode::Interp::SmootherStep;
    mapRange->clampRange = false;
    auto* legacyColor = static_cast<Artifact::ShaderNode::ColorNode*>(
        graph.addNode(std::make_unique<Artifact::ShaderNode::ColorNode>("legacy")));
    legacyColor->r = 0.7f;
    legacyColor->g = 0.6f;
    legacyColor->b = 0.5f;
    legacyColor->a = 0.4f;

    ASSERT_NE(value, nullptr);
    ASSERT_NE(texture, nullptr);
    const std::string encoded = graph.toJson({});
    NodeGraph restored;
    const auto loaded = restored.fromJson(encoded);
    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_EQ(restored.toJson({}), encoded);
}

TEST(ShaderGraphContractTest, ImageTextureSlotsSelectDeclaredResources)
{
    using Artifact::ShaderNode::ImageTextureNode;
    NodeGraph graph;
    auto* base = static_cast<ImageTextureNode*>(graph.addNode(
        std::make_unique<ImageTextureNode>("base_texture", ImageTextureNode::Slot::BaseColor)));
    auto* opacity = static_cast<ImageTextureNode*>(graph.addNode(
        std::make_unique<ImageTextureNode>("opacity_texture", ImageTextureNode::Slot::Opacity)));
    auto* emission = static_cast<ImageTextureNode*>(graph.addNode(
        std::make_unique<ImageTextureNode>("emission_texture", ImageTextureNode::Slot::Emission)));
    auto* metallicRoughness = static_cast<ImageTextureNode*>(graph.addNode(
        std::make_unique<ImageTextureNode>(
            "metallic_roughness_texture", ImageTextureNode::Slot::MetallicRoughness)));
    auto* normal = static_cast<ImageTextureNode*>(graph.addNode(
        std::make_unique<ImageTextureNode>("normal_texture", ImageTextureNode::Slot::Normal)));
    auto* occlusion = static_cast<ImageTextureNode*>(graph.addNode(
        std::make_unique<ImageTextureNode>("occlusion_texture", ImageTextureNode::Slot::Occlusion)));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));

    graph.link(base->outputs[0].get(), surface->inputs[0].get());
    graph.link(opacity->outputs[1].get(), surface->inputs[5].get());
    graph.link(emission->outputs[0].get(), surface->inputs[3].get());
    graph.link(metallicRoughness->outputs[0].get(), surface->inputs[1].get());
    graph.link(normal->outputs[0].get(), surface->inputs[6].get());
    graph.link(occlusion->outputs[1].get(), surface->inputs[7].get());
    graph.link(surface->outputs[0].get(), output->inputs[0].get());

    const auto compiled = graph.compileMaterialGraph();
    ASSERT_TRUE(compiled.ok) << compiled.error;
    for (const std::string_view expression : {
             "base_texture_Color = g_BaseColorTexture.Sample(g_BaseColorSampler, In.UV)",
             "opacity_texture_Color = g_OpacityTexture.Sample(g_BaseColorSampler, In.UV)",
             "emission_texture_Color = g_EmissionTexture.Sample(g_BaseColorSampler, In.UV)",
             "metallic_roughness_texture_Color = g_MetallicRoughnessTexture.Sample(g_BaseColorSampler, In.UV)",
             "normal_texture_Color = g_NormalTexture.Sample(g_BaseColorSampler, In.UV)",
             "occlusion_texture_Color = g_OcclusionTexture.Sample(g_BaseColorSampler, In.UV)"}) {
        EXPECT_NE(compiled.blockHlsl.find(expression), std::string::npos)
            << "ImageTexture slot lowering differs from current resource/sampler contract: "
            << expression;
    }
}

TEST(ShaderGraphContractTest, UnconnectedNoiseScaleUsesLoweringDefault)
{
    NodeGraph graph;
    auto* noise = static_cast<NoiseTextureNode*>(
        graph.addNode(std::make_unique<NoiseTextureNode>("noise")));
    auto* principled = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));
    graph.link(noise->outputs[1].get(), principled->inputs[0].get());
    graph.link(principled->outputs[0].get(), output->inputs[0].get());

    const auto result = graph.compileMaterialGraph();
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_NE(result.blockHlsl.find("max(5.0f, 0.001f)"), std::string::npos);
}

TEST(ShaderGraphContractTest, ProceduralAlgorithmsHavePinnedSourceContracts)
{
    NodeGraph graph;
    auto* noise = static_cast<NoiseTextureNode*>(
        graph.addNode(std::make_unique<NoiseTextureNode>("noise")));
    auto* voronoi = static_cast<Artifact::ShaderNode::VoronoiTextureNode*>(
        graph.addNode(std::make_unique<Artifact::ShaderNode::VoronoiTextureNode>("voronoi")));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));
    graph.link(noise->outputs[0].get(), surface->inputs[0].get());
    graph.link(voronoi->outputs[0].get(), surface->inputs[2].get());
    graph.link(surface->outputs[0].get(), output->inputs[0].get());

    const auto compiled = graph.compileMaterialGraph();
    ASSERT_TRUE(compiled.ok) << compiled.error;
    for (const std::string_view fragment : {
             "float artMatGraph_hash21(float2 p)",
             "float artMatGraph_vnoise(float2 p)",
             "float artMatGraph_fbm4(float2 p)",
             "for (int k = 0; k < 4; ++k)",
             "p = p * 2.03f + float2(17.3f, 9.1f)",
             "void artMatGraph_voronoi(float2 p, out float dist, out float3 cell)",
             "for (int j = -1; j <= 1; ++j)",
             "for (int i = -1; i <= 1; ++i)",
             "float d = dot(r, r)",
             "dist = sqrt(dist)"}) {
        EXPECT_NE(compiled.helperHlsl.find(fragment), std::string::npos) << fragment;
    }
    for (const std::string_view fragment : {
             "artMatGraph_fbm4(noise_p)",
             "artMatGraph_voronoi(voronoi_p, voronoi_Distance, voronoi_cell)"}) {
        EXPECT_NE(compiled.blockHlsl.find(fragment), std::string::npos) << fragment;
    }
    EXPECT_NE(compiled.blockHlsl.find("max(5.0f, 0.001f)"), std::string::npos);
}

TEST(ShaderGraphContractTest, VoronoiFixtureConnectsCellColorAndDistance)
{
    const auto fixturePath = std::filesystem::path(__FILE__).parent_path().parent_path() /
        "fixtures" / "shader-graph-rd" / "voronoi_cells_surface.json";
    std::ifstream fixtureFile(fixturePath, std::ios::binary);
    ASSERT_TRUE(fixtureFile.is_open()) << fixturePath.string();
    const std::string payload{
        std::istreambuf_iterator<char>(fixtureFile),
        std::istreambuf_iterator<char>()};

    NodeGraph graph;
    const auto loaded = graph.fromJson(payload);
    ASSERT_TRUE(loaded.ok) << loaded.error;
    const auto compiled = graph.compileMaterialGraph();
    ASSERT_TRUE(compiled.ok) << compiled.error;
    EXPECT_NE(compiled.blockHlsl.find(
                  "float4 voronoi_Color = float4(voronoi_cell, 1.0f)"),
              std::string::npos);
    EXPECT_NE(compiled.blockHlsl.find("float voronoi_Distance = 0.0f"),
              std::string::npos);
    EXPECT_NE(compiled.blockHlsl.find(
                  "voronoi_Distance = sqrt(voronoi_Distance)"),
              std::string::npos);
    EXPECT_NE(compiled.blockHlsl.find("surface_roughness = saturate(voronoi_Distance)"),
              std::string::npos);
}

TEST(ShaderGraphContractTest, PrincipledAlphaLowersToExplicitMaterialAlpha)
{
    NodeGraph graph;
    const auto loaded = graph.fromJson(
        R"({"version":1,"kind":"artifact-material-graph","nodes":[{"id":"base_color","type":"RGB","params":{"r":0.8,"g":0.2,"b":0.1,"a":1.0}},{"id":"half_opacity","type":"Value","params":{"value":0.5}},{"id":"surface","type":"Principled","params":{}},{"id":"output","type":"MaterialOutput","params":{}}],"links":[{"from":"base_color","out":"Color","to":"surface","in":"BaseColor"},{"from":"half_opacity","out":"Value","to":"surface","in":"Alpha"},{"from":"surface","out":"BSDF","to":"output","in":"Surface"}]})");
    ASSERT_TRUE(loaded.ok) << loaded.error;

    const auto compiled = graph.compileMaterialGraph();
    ASSERT_TRUE(compiled.ok) << compiled.error;
    EXPECT_NE(compiled.blockHlsl.find(
                  "float half_opacity_Value = 0.5f;"), std::string::npos);
    EXPECT_NE(compiled.blockHlsl.find(
                  "float surface_alpha = saturate(half_opacity_Value);"),
              std::string::npos);
    EXPECT_NE(compiled.blockHlsl.find(
                  "float graphAlpha = surface_alpha;"), std::string::npos);
}

TEST(ShaderGraphContractTest, PrincipledMetallicRoughnessAndEmissionLowerExplicitly)
{
    NodeGraph graph;
    auto* metallic = static_cast<ValueNode*>(
        graph.addNode(std::make_unique<ValueNode>("metallic", 0.75f)));
    auto* roughness = static_cast<ValueNode*>(
        graph.addNode(std::make_unique<ValueNode>("roughness", 0.25f)));
    auto* emission = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("emission")));
    emission->r = 0.25f;
    emission->g = 0.5f;
    emission->b = 1.0f;
    auto* emissionStrength = static_cast<ValueNode*>(
        graph.addNode(std::make_unique<ValueNode>("emission_strength", 2.5f)));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));
    graph.link(metallic->outputs[0].get(), surface->inputs[1].get());
    graph.link(roughness->outputs[0].get(), surface->inputs[2].get());
    graph.link(emission->outputs[0].get(), surface->inputs[3].get());
    graph.link(emissionStrength->outputs[0].get(), surface->inputs[4].get());
    graph.link(surface->outputs[0].get(), output->inputs[0].get());

    const auto compiled = graph.compileMaterialGraph();
    ASSERT_TRUE(compiled.ok) << compiled.error;
    EXPECT_NE(compiled.blockHlsl.find(
                  "float surface_metallic = saturate(metallic_Value);"),
              std::string::npos);
    EXPECT_NE(compiled.blockHlsl.find(
                  "float surface_roughness = saturate(roughness_Value);"),
              std::string::npos);
    EXPECT_NE(compiled.blockHlsl.find(
                  "float3 surface_emission = (emission_Color).rgb * max(emission_strength_Value, 0.0f);"),
              std::string::npos);
    EXPECT_NE(compiled.blockHlsl.find(
                  "float graphMetallic = surface_metallic;"), std::string::npos);
    EXPECT_NE(compiled.blockHlsl.find(
                  "float graphRoughness = surface_roughness;"), std::string::npos);
    EXPECT_NE(compiled.blockHlsl.find(
                  "float3 graphEmission = surface_emission;"), std::string::npos);
}

TEST(ShaderGraphContractTest, OutOfRangeGraphBaseColorAlphaIsDiagnosed)
{
    NodeGraph graph;
    const auto loaded = graph.fromJson(
        R"({"version":1,"kind":"artifact-material-graph","nodes":[{"id":"base_color","type":"RGB","params":{"r":0.8,"g":0.2,"b":0.1,"a":1.5}},{"id":"surface","type":"Principled","params":{}},{"id":"output","type":"MaterialOutput","params":{}}],"links":[{"from":"base_color","out":"Color","to":"surface","in":"BaseColor"},{"from":"surface","out":"BSDF","to":"output","in":"Surface"}]})");

    bool diagnosed = !loaded.ok || !loaded.error.empty();
    if (loaded.ok) {
        const auto compiled = graph.compileMaterialGraph();
        diagnosed = diagnosed || !compiled.ok || !compiled.error.empty();
    }
    EXPECT_TRUE(diagnosed)
        << "BaseColor alpha outside [0,1] must be rejected or explicitly normalized with a diagnostic";
}

TEST(ShaderGraphContractTest, EmptyColorRampStopsAreNotSilentlyIgnored)
{
    NodeGraph graph;
    const auto loaded = graph.fromJson(
        R"({"version":1,"kind":"artifact-material-graph","nodes":[{"id":"ramp","type":"ColorRamp","params":{"interp":0,"stops":[]}}],"links":[]})");

    EXPECT_TRUE(!loaded.ok || !loaded.error.empty())
        << "An empty Color Ramp must be rejected or explicitly diagnosed when normalized";
}

TEST(ShaderGraphContractTest, ExcessColorRampStopsAreNotSilentlyTruncated)
{
    std::string json =
        R"({"version":1,"kind":"artifact-material-graph","nodes":[{"id":"ramp","type":"ColorRamp","params":{"interp":0,"stops":[)";
    for (int index = 0; index < 65; ++index) {
        if (index != 0) {
            json += ',';
        }
        json += std::string("{\"pos\":") + std::to_string(index / 64.0) +
                ",\"r\":0.25,\"g\":0.5,\"b\":0.75,\"a\":1.0}";
    }
    json += R"(]}}],"links":[]})";

    NodeGraph graph;
    const auto loaded = graph.fromJson(json);
    EXPECT_TRUE(!loaded.ok || !loaded.error.empty())
        << "Stops above the supported limit must be rejected or explicitly diagnosed";
}

TEST(ShaderGraphContractTest, LinkRejectsPinsOwnedByAnotherGraph)
{
    NodeGraph graph;
    NodeGraph foreignGraph;
    auto* foreignColor = static_cast<RGBNode*>(
        foreignGraph.addNode(std::make_unique<RGBNode>("foreign_color")));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));

    graph.link(foreignColor->outputs[0].get(), surface->inputs[0].get());
    EXPECT_TRUE(graph.links.empty())
        << "A graph must not retain a Pin* owned by another graph";
}

TEST(ShaderGraphContractTest, LinkRejectsIncompatiblePinTypes)
{
    NodeGraph graph;
    auto* scalar = static_cast<ValueNode*>(
        graph.addNode(std::make_unique<ValueNode>("scalar", 0.75f)));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));

    graph.link(scalar->outputs[0].get(), surface->inputs[0].get());
    EXPECT_TRUE(graph.links.empty())
        << "Float output must not be connected to a Vector4 BaseColor input";
}

TEST(ShaderGraphContractTest, LinkRejectsMultipleWritersForOneInput)
{
    NodeGraph graph;
    auto* firstColor = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("first_color")));
    auto* secondColor = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("second_color")));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));

    graph.link(firstColor->outputs[0].get(), surface->inputs[0].get());
    graph.link(secondColor->outputs[0].get(), surface->inputs[0].get());
    EXPECT_EQ(graph.links.size(), 1u)
        << "A graph input may have at most one incoming link";
}

TEST(ShaderGraphContractTest, SanitizedNodeIdentifiersDoNotCollide)
{
    NodeGraph graph;
    auto* firstColor = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("base-color")));
    auto* secondColor = static_cast<RGBNode*>(
        graph.addNode(std::make_unique<RGBNode>("base_color")));
    auto* mix = static_cast<MixRGBNode*>(
        graph.addNode(std::make_unique<MixRGBNode>("mix")));
    auto* surface = static_cast<PrincipledBSDFNode*>(
        graph.addNode(std::make_unique<PrincipledBSDFNode>("surface")));
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));
    graph.link(firstColor->outputs[0].get(), mix->inputs[1].get());
    graph.link(secondColor->outputs[0].get(), mix->inputs[2].get());
    graph.link(mix->outputs[0].get(), surface->inputs[0].get());
    graph.link(surface->outputs[0].get(), output->inputs[0].get());

    const auto result = graph.compileMaterialGraph();
    ASSERT_TRUE(result.ok) << result.error;
    constexpr std::string_view duplicateDeclaration =
        "float4 base_color_Color =";
    const size_t first = result.blockHlsl.find(duplicateDeclaration);
    ASSERT_NE(first, std::string::npos);
    EXPECT_EQ(result.blockHlsl.find(duplicateDeclaration,
                                   first + duplicateDeclaration.size()),
              std::string::npos)
        << "Distinct graph IDs must not emit duplicate HLSL declarations";
}

TEST(ShaderGraphContractTest, GraphTraversalHandlesDeepChainsWithoutStackFailure)
{
    // Stress shape only: the intended production maximum remains unset until
    // allocation and traversal measurements establish a supported bound.
    constexpr size_t nodeCount = 2048;
    NodeGraph graph;
    auto* previous = static_cast<MathNode*>(
        graph.addNode(std::make_unique<MathNode>("math_0")));
    for (size_t index = 1; index < nodeCount; ++index) {
        const std::string id = "math_" + std::to_string(index);
        auto* current = static_cast<MathNode*>(
            graph.addNode(std::make_unique<MathNode>(id)));
        graph.link(previous->outputs[0].get(), current->inputs[0].get());
        previous = current;
    }
    auto* output = static_cast<MaterialOutputNode*>(
        graph.addNode(std::make_unique<MaterialOutputNode>("output")));
    graph.link(previous->outputs[0].get(), output->inputs[1].get());

    const auto result = graph.compileMaterialGraph();
    EXPECT_FALSE(result.ok)
        << "Unsupported graph shapes must fail explicitly without exhausting the process stack";
}

} // namespace
