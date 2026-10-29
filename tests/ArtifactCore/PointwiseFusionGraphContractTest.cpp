#include <gtest/gtest.h>

#include <string>
#include <vector>

import Graphics.PointwiseFusion;

using ArtifactCore::PointwiseFusionGraph;
using ArtifactCore::PointwiseOperationKind;
using ArtifactCore::PointwiseValueType;

TEST(PointwiseFusionGraphContractTest, EmptyGraphIsValidButHasNoFinalValue)
{
    const PointwiseFusionGraph graph;
    EXPECT_TRUE(graph.isValid());
    EXPECT_EQ(graph.operationCount(), 0u);
    EXPECT_EQ(graph.finalValue().id, 0u);
    EXPECT_TRUE(graph.emitHlslBody().empty());
}

TEST(PointwiseFusionGraphContractTest, EmitsTypedOperationsInInsertionOrder)
{
    PointwiseFusionGraph graph;
    const auto color = graph.addInput("sourceColor", PointwiseValueType::Float4);
    const auto scale = graph.addConstant("float4(0.5, 0.5, 0.5, 1.0)",
                                         PointwiseValueType::Float4);
    const auto scaled = graph.addOperation(PointwiseOperationKind::Multiply,
                                           {color, scale});
    const auto output = graph.addOperation(PointwiseOperationKind::Saturate,
                                           {scaled});

    ASSERT_TRUE(graph.isValid());
    ASSERT_EQ(graph.operationCount(), 4u);
    EXPECT_EQ(graph.finalValue().id, output.id);
    EXPECT_EQ(graph.finalValue().type, PointwiseValueType::Float4);
    const std::string body = graph.emitHlslBody();
    const auto inputOffset = body.find("float4 value_1 = sourceColor;");
    const auto constantOffset = body.find(
        "float4 value_2 = float4(0.5, 0.5, 0.5, 1.0);");
    const auto multiplyOffset = body.find("float4 value_3 = value_1 * value_2;");
    const auto saturateOffset = body.find("float4 value_4 = saturate(value_3);");
    EXPECT_NE(inputOffset, std::string::npos);
    EXPECT_LT(inputOffset, constantOffset);
    EXPECT_LT(constantOffset, multiplyOffset);
    EXPECT_LT(multiplyOffset, saturateOffset);
}

TEST(PointwiseFusionGraphContractTest, RejectsWrongArityAndForwardReferences)
{
    PointwiseFusionGraph wrongArity;
    const auto input = wrongArity.addInput("color");
    wrongArity.addOperation(PointwiseOperationKind::Add, {input});
    EXPECT_FALSE(wrongArity.isValid());

    PointwiseFusionGraph forwardReference;
    const auto future = forwardReference.addOperation(
        PointwiseOperationKind::Saturate, {{2, PointwiseValueType::Float4}});
    (void)future;
    forwardReference.addInput("later");
    EXPECT_FALSE(forwardReference.isValid());
}

TEST(PointwiseFusionGraphContractTest, RejectsTypeMismatchAcrossOperandsAndOutput)
{
    PointwiseFusionGraph graph;
    const auto scalar = graph.addInput("weight", PointwiseValueType::Float1);
    const auto vector = graph.addInput("color", PointwiseValueType::Float4);
    graph.addOperation(PointwiseOperationKind::Add, {scalar, vector},
                       PointwiseValueType::Float4);
    EXPECT_FALSE(graph.isValid());
}

TEST(PointwiseFusionGraphContractTest, SelectsExplicitOutputAndResetsIdsOnClear)
{
    PointwiseFusionGraph graph;
    const auto first = graph.addInput("first");
    const auto second = graph.addInput("second");
    graph.setOutput(first);
    EXPECT_EQ(graph.finalValue().id, first.id);

    graph.clear();
    EXPECT_TRUE(graph.isValid());
    EXPECT_EQ(graph.operationCount(), 0u);
    const auto afterClear = graph.addInput("afterClear");
    EXPECT_EQ(afterClear.id, 1u);
    EXPECT_EQ(graph.finalValue().id, afterClear.id);
    EXPECT_NE(second.id, afterClear.id);
}

TEST(PointwiseFusionGraphContractTest, SanitizesInputAndFunctionIdentifiers)
{
    PointwiseFusionGraph graph;
    const auto input = graph.addInput("9 source-color");
    graph.setOutput(input);
    const std::string body = graph.emitHlslBody();
    EXPECT_NE(body.find("_9_source_color"), std::string::npos);

    const std::string function = graph.emitHlslFunction("2 composite-pass", input);
    EXPECT_NE(function.find("float4 _2_composite_pass("), std::string::npos);
    EXPECT_NE(function.find("return value_1;"), std::string::npos);
}

TEST(PointwiseFusionGraphContractTest, EmitsFallbackExpressionsForUnwiredOperations)
{
    PointwiseFusionGraph graph;
    graph.addOperation(PointwiseOperationKind::Add, {});
    EXPECT_TRUE(graph.isValid());
    EXPECT_NE(graph.emitHlslBody().find("float4 value_1 = 0;"), std::string::npos);
}
