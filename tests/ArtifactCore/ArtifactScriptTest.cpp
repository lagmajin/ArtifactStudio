#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <variant>

import Script.ArtifactScript;

using namespace ArtifactCore;

TEST(ArtifactScriptTest, ParseClassAndFields) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Spin : ArtifactBehaviour
{
    public float speed = 90.0;
    private float time = 0.0;
    void OnUpdate(float dt) {}
}
)");

    EXPECT_EQ(definition.rootClass.name, "Spin");
    EXPECT_TRUE(definition.rootClass.derivesFromBehaviour);
    ASSERT_EQ(definition.rootClass.fields.size(), 2u);
    EXPECT_TRUE(definition.rootClass.fields[0].isPublic);
    EXPECT_FALSE(definition.rootClass.fields[1].isPublic);
    ASSERT_EQ(definition.rootClass.methods.size(), 1u);
    EXPECT_TRUE(definition.rootClass.methods[0].isLifecycleHook);
    EXPECT_TRUE(definition.diagnostics.empty());
}

TEST(ArtifactScriptTest, NumericLiteralsSupportScientificNotationAndRejectInvalidRanges) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScientificNumbers : ArtifactBehaviour
{
    public float result = 0.0;
    void OnUpdate()
    {
        result = 1.25e-3 + 2E2 + .5 + 1.0 + 1. - 4e-2;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    instance.fields()["result"] = 0.0;
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")),
                     202.46125);

    const auto incompleteExponent = parser.parse(
        "class BrokenNumber : ArtifactBehaviour\n{\n"
        "    public float result = 0.0;\n"
        "    void OnUpdate() { result = 1e+; }\n}\n");
    ASSERT_FALSE(incompleteExponent.diagnostics.empty());
    EXPECT_EQ(incompleteExponent.diagnostics.front().line, 4u);

    const auto overflowingNumber = parser.parse(
        "class OverflowNumber : ArtifactBehaviour\n{\n"
        "    public float result = 0.0;\n"
        "    void OnUpdate() { result = 1e309; }\n}\n");
    ASSERT_FALSE(overflowingNumber.diagnostics.empty());
    EXPECT_EQ(overflowingNumber.diagnostics.front().line, 4u);
}

TEST(ArtifactScriptTest, IntegerLiteralsAndArithmeticPreserveInt64Precision) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ExactIntegers : ArtifactBehaviour
{
    public int large = 0;
    public int sum = 0;
    public int incremented = 0;
    public int quotient = 0;
    public int negativeQuotient = 0;
    public int remainder = 0;
    public int minimum = 0;
    public bool roundedDoubleEqual = true;
    public bool roundedDoubleNotEqual = false;
    public bool greaterThanRoundedDouble = false;
    public bool lessThanUpperDouble = false;
    public bool reverseGreaterThanRoundedDouble = false;
    public bool minimumEqualsDouble = false;
    public bool maximumLessThanUpperDouble = false;
    void OnUpdate()
    {
        large = 9007199254740993;
        sum = large + 1;
        quotient = 7 / 2;
        negativeQuotient = -7 / 2;
        remainder = -7 % 2;
        minimum = -9223372036854775808;
        roundedDoubleEqual = large == 9007199254740992.0;
        roundedDoubleNotEqual = large != 9007199254740992.0;
        greaterThanRoundedDouble = large > 9007199254740992.0;
        lessThanUpperDouble = large < 9007199254740994.0;
        reverseGreaterThanRoundedDouble = 9007199254740992.0 < large;
        minimumEqualsDouble = minimum == -9223372036854775808.0;
        maximumLessThanUpperDouble = 9223372036854775807 < 9223372036854775808.0;
        large++;
        incremented = large;
        ++incremented;
        incremented--;
    }
}
)");

    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("large")),
              INT64_C(9007199254740994));
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("sum")),
              INT64_C(9007199254740994));
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("large")),
              INT64_C(9007199254740994));
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("incremented")),
              INT64_C(9007199254740994));
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("quotient")), 3);
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("negativeQuotient")), -3);
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("remainder")), -1);
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("minimum")),
              std::numeric_limits<std::int64_t>::min());
    EXPECT_FALSE(std::get<bool>(instance.fields().at("roundedDoubleEqual")));
    EXPECT_TRUE(std::get<bool>(instance.fields().at("roundedDoubleNotEqual")));
    EXPECT_TRUE(std::get<bool>(instance.fields().at("greaterThanRoundedDouble")));
    EXPECT_TRUE(std::get<bool>(instance.fields().at("lessThanUpperDouble")));
    EXPECT_TRUE(std::get<bool>(
        instance.fields().at("reverseGreaterThanRoundedDouble")));
    EXPECT_TRUE(std::get<bool>(instance.fields().at("minimumEqualsDouble")));
    EXPECT_TRUE(std::get<bool>(
        instance.fields().at("maximumLessThanUpperDouble")));
}

TEST(ArtifactScriptTest, IntegerArithmeticReportsOverflow) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class IntegerOverflow : ArtifactBehaviour
{
    public int result = 0;
    void OnUpdate() { result = 9223372036854775807 + 1; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    EXPECT_FALSE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_NE(instance.lastError().find("integer overflow"), std::string::npos);

    auto divisionDefinition = parser.parse(R"(
class IntegerDivisionOverflow : ArtifactBehaviour
{
    public int result = 0;
    void OnUpdate() { result = -9223372036854775808 / -1; }
}
)");
    ASSERT_TRUE(divisionDefinition.diagnostics.empty());
    ArtifactScriptInstance divisionInstance(std::move(divisionDefinition));
    EXPECT_FALSE(divisionInstance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_NE(divisionInstance.lastError().find("integer overflow"),
              std::string::npos);

    auto multiplicationDefinition = parser.parse(R"(
class IntegerMultiplicationOverflow : ArtifactBehaviour
{
    public int result = 0;
    void OnUpdate() { result = 9223372036854775807 * 2; }
}
)");
    ASSERT_TRUE(multiplicationDefinition.diagnostics.empty());
    ArtifactScriptInstance multiplicationInstance(
        std::move(multiplicationDefinition));
    EXPECT_FALSE(multiplicationInstance.invokeHook(
        ArtifactScriptHook::OnUpdate));
    EXPECT_NE(multiplicationInstance.lastError().find("integer overflow"),
              std::string::npos);

    auto incrementDefinition = parser.parse(R"(
class IntegerIncrementOverflow : ArtifactBehaviour
{
    public int result = 0;
    void OnUpdate()
    {
        result = 9223372036854775807;
        result++;
    }
}
)");
    ASSERT_TRUE(incrementDefinition.diagnostics.empty());
    ArtifactScriptInstance incrementInstance(std::move(incrementDefinition));
    EXPECT_FALSE(incrementInstance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_NE(incrementInstance.lastError().find("integer overflow"),
              std::string::npos);
}

TEST(ArtifactScriptTest, InvalidClassMemberReportsDiagnosticWithoutStalling) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Broken : ArtifactBehaviour
{
    this is not a supported declaration;
}
)");

    ASSERT_FALSE(definition.diagnostics.empty());
    EXPECT_NE(definition.diagnostics.front().message.find(
                  "unsupported or invalid class member syntax"),
              std::string::npos);
}

TEST(ArtifactScriptTest, MethodBodyParserRejectsUnsupportedTokenWithoutStalling) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Broken : ArtifactBehaviour
{
    void OnUpdate()
    {
        float value = 1.0;
        @;
    }
}
)");

    ASSERT_FALSE(definition.diagnostics.empty());
    EXPECT_EQ(definition.diagnostics.front().line, 7u);
    EXPECT_EQ(definition.diagnostics.front().column, 9u);
    EXPECT_NE(definition.diagnostics.front().message.find(
                  "unsupported or invalid syntax in method body"),
              std::string::npos);

    const auto nestedDefinition = parser.parse(R"(
class NestedBroken : ArtifactBehaviour
{
    void OnUpdate()
    {
        if (true) {
            float value = 1.0;
            value ^ 2.0;
        }
    }
}
)");
    ASSERT_FALSE(nestedDefinition.diagnostics.empty());
    EXPECT_NE(nestedDefinition.diagnostics.front().message.find(
                  "unsupported or invalid syntax in method body"),
              std::string::npos);
}

TEST(ArtifactScriptTest, MethodBodyBraceScanIgnoresStringsAndComments) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class BraceProbe : ArtifactBehaviour
{
    public string observed = "";
    void OnCreate()
    {
        observed = "a } b { c";
        // These braces do not end the method: } {
        /* Nor do these: } { */
    }
}
)");

    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate))
        << instance.lastError();
    EXPECT_EQ(std::get<std::string>(instance.fields().at("observed")),
              "a } b { c");
}

TEST(ArtifactScriptTest, StringLiteralDecodesEscapesAndKeepsUnescapedText) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class EscapedStrings : ArtifactBehaviour
{
    public string observed = "";
    public string defaultValue = "default\nvalue";
    public string unicode = "";
    public string unicodeDefault = "\u732b";
    void OnUpdate()
    {
        observed = "quote: \" slash: \\ newline: \n return: \r tab: \t";
        unicode = "\u732b\U0001F680\ud83d\ude00";
    }
}
)");

    ASSERT_TRUE(definition.diagnostics.empty());
    ASSERT_EQ(definition.rootClass.fields.size(), 4u);
    EXPECT_EQ(std::get<std::string>(definition.rootClass.fields[1].defaultValue),
              "default\nvalue");
    EXPECT_EQ(std::get<std::string>(definition.rootClass.fields[3].defaultValue),
              std::string("\xE7\x8C\xAB", 3));
    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    EXPECT_EQ(std::get<std::string>(instance.fields().at("observed")),
              "quote: \" slash: \\ newline: \n return: \r tab: \t");
    EXPECT_EQ(std::get<std::string>(instance.fields().at("unicode")),
              std::string("\xE7\x8C\xAB\xF0\x9F\x9A\x80\xF0\x9F\x98\x80", 11));

    const auto invalidEscape = parser.parse(
        "class BrokenEscape : ArtifactBehaviour\n{\n"
        "    void OnUpdate()\n    {\n"
        "        value = \"bad\\q\";\n"
        "    }\n}\n");
    ASSERT_EQ(invalidEscape.diagnostics.size(), 1u);
    EXPECT_EQ(invalidEscape.diagnostics.front().line, 5u);
    EXPECT_EQ(invalidEscape.diagnostics.front().column, 21u);

    const auto invalidFieldEscape = parser.parse(
        "class BrokenFieldEscape : ArtifactBehaviour\n{\n"
        "    public string value = \"bad\\q\";\n}\n");
    ASSERT_EQ(invalidFieldEscape.diagnostics.size(), 1u);
    EXPECT_EQ(invalidFieldEscape.diagnostics.front().line, 3u);
    EXPECT_EQ(invalidFieldEscape.diagnostics.front().column, 31u);

    const auto invalidFieldUnicode = parser.parse(
        "class BrokenFieldUnicode : ArtifactBehaviour\n{\n"
        "    public string value = \"\\uD800\";\n}\n");
    ASSERT_EQ(invalidFieldUnicode.diagnostics.size(), 1u);
    EXPECT_EQ(invalidFieldUnicode.diagnostics.front().line, 3u);
    EXPECT_EQ(invalidFieldUnicode.diagnostics.front().column, 28u);

    const auto invalidUnicode = parser.parse(
        "class BrokenUnicode : ArtifactBehaviour\n{\n"
        "    void OnUpdate()\n    {\n"
        "        value = \"\\uD800\";\n"
        "    }\n}\n");
    ASSERT_EQ(invalidUnicode.diagnostics.size(), 1u);
    EXPECT_EQ(invalidUnicode.diagnostics.front().line, 5u);
    EXPECT_EQ(invalidUnicode.diagnostics.front().column, 18u);
}

TEST(ArtifactScriptTest, MissingExpressionReportsItsSourceLocation) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class BrokenExpression : ArtifactBehaviour
{
    void OnUpdate()
    {
        value = 1.0 + ;
    }
}
)");

    ASSERT_EQ(definition.diagnostics.size(), 1u);
    EXPECT_EQ(definition.diagnostics.front().line, 6u);
    EXPECT_EQ(definition.diagnostics.front().column, 23u);
    EXPECT_NE(definition.diagnostics.front().message.find(
                  "unsupported or invalid syntax in method body"),
              std::string::npos);
}

TEST(ArtifactScriptTest, MissingCallAndIndexArgumentsReportSourceLocations) {
    ArtifactScriptParser parser;
    const auto parseBody = [&](std::string_view body) {
        std::string source =
            "class BrokenArguments : ArtifactBehaviour\n{\n"
            "    void OnUpdate()\n    {\n";
        source.append(body);
        source.append("\n    }\n}\n");
        return parser.parse(source);
    };

    const auto missingCallArgument = parseBody("        ping(1.0, );");
    ASSERT_EQ(missingCallArgument.diagnostics.size(), 1u);
    EXPECT_EQ(missingCallArgument.diagnostics.front().line, 5u);
    EXPECT_EQ(missingCallArgument.diagnostics.front().column, 19u);

    const auto missingCallClose = parseBody("        ping(1.0;");
    ASSERT_EQ(missingCallClose.diagnostics.size(), 1u);
    EXPECT_EQ(missingCallClose.diagnostics.front().line, 5u);
    EXPECT_EQ(missingCallClose.diagnostics.front().column, 17u);

    const auto missingIndex = parseBody("        value = values[];");
    ASSERT_EQ(missingIndex.diagnostics.size(), 1u);
    EXPECT_EQ(missingIndex.diagnostics.front().line, 5u);
    EXPECT_EQ(missingIndex.diagnostics.front().column, 24u);

    const auto missingIndexClose = parseBody("        value = values[0;");
    ASSERT_EQ(missingIndexClose.diagnostics.size(), 1u);
    EXPECT_EQ(missingIndexClose.diagnostics.front().line, 5u);
    EXPECT_EQ(missingIndexClose.diagnostics.front().column, 25u);

    const auto missingConstructorArgument = parseBody(
        "        target = new Sink(,);");
    ASSERT_EQ(missingConstructorArgument.diagnostics.size(), 1u);
    EXPECT_EQ(missingConstructorArgument.diagnostics.front().line, 5u);
    EXPECT_EQ(missingConstructorArgument.diagnostics.front().column, 27u);

    const auto validEmptyArguments = parseBody(
        "        ping();\n        target = new Sink();");
    EXPECT_TRUE(validEmptyArguments.diagnostics.empty());
}

TEST(ArtifactScriptTest,
     ControlStatementDelimitersReportMalformedSyntaxLocations) {
    ArtifactScriptParser parser;
    const auto parseBody = [&](std::string_view body) {
        std::string source =
            "class BrokenControl : ArtifactBehaviour\n{\n"
            "    void OnUpdate()\n    {\n";
        source.append(body);
        source.append("\n    }\n}\n");
        return parser.parse(source);
    };

    const auto missingIfOpen = parseBody("        if true) { value = 1.0; }");
    ASSERT_EQ(missingIfOpen.diagnostics.size(), 1u);
    EXPECT_EQ(missingIfOpen.diagnostics.front().line, 5u);
    EXPECT_NE(missingIfOpen.diagnostics.front().column, 0u);

    const auto missingWhileClose = parseBody("        while (true { value = 1.0; }");
    ASSERT_EQ(missingWhileClose.diagnostics.size(), 1u);
    EXPECT_EQ(missingWhileClose.diagnostics.front().line, 5u);
    EXPECT_NE(missingWhileClose.diagnostics.front().column, 0u);

    const auto missingForInitializerSeparator = parseBody(
        "        for (int i = 0 i < 2; i++) { value += 1.0; }");
    ASSERT_EQ(missingForInitializerSeparator.diagnostics.size(), 1u);
    EXPECT_EQ(missingForInitializerSeparator.diagnostics.front().line, 5u);
    EXPECT_NE(missingForInitializerSeparator.diagnostics.front().column, 0u);

    const auto missingForSeparator = parseBody(
        "        for (int i = 0; i < 2 i++) { value += 1.0; }");
    ASSERT_EQ(missingForSeparator.diagnostics.size(), 1u);
    EXPECT_EQ(missingForSeparator.diagnostics.front().line, 5u);
    EXPECT_NE(missingForSeparator.diagnostics.front().column, 0u);

    const auto missingForeachClose = parseBody(
        "        foreach (item in values { value += item; }");
    ASSERT_EQ(missingForeachClose.diagnostics.size(), 1u);
    EXPECT_EQ(missingForeachClose.diagnostics.front().line, 5u);
    EXPECT_NE(missingForeachClose.diagnostics.front().column, 0u);

    const auto missingDoWhileTerminator = parseBody(
        "        do { value += 1.0; } while (false)");
    ASSERT_EQ(missingDoWhileTerminator.diagnostics.size(), 1u);
    EXPECT_EQ(missingDoWhileTerminator.diagnostics.front().line, 6u);
    EXPECT_NE(missingDoWhileTerminator.diagnostics.front().column, 0u);
}

TEST(ArtifactScriptTest, DoWhileExecutesBodyBeforeConditionAndHonorsLoopControl) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class DoWhileProbe : ArtifactBehaviour
{
    public float result = 0.0;
    void OnUpdate()
    {
        var index = 0;
        do { index += 1; result += 1.0; } while (false);
        do
        {
            index += 1;
            if (index < 4) continue;
            result += index;
            break;
        } while (true);
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    ASSERT_TRUE(std::holds_alternative<double>(instance.fields().at("result")));
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 5.0);
}

TEST(ArtifactScriptTest, DoWhileReturnsImmediatelyFromItsBody) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class DoWhileReturnProbe : ArtifactBehaviour
{
    float OnUpdate()
    {
        do { return 42.0; } while (true);
        return 0.0;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    const auto result = evaluator.executeMethod(
        definition, "OnUpdate", {}, fields);
    ASSERT_TRUE(std::holds_alternative<double>(result))
        << evaluator.getLastError();
    EXPECT_DOUBLE_EQ(std::get<double>(result), 42.0);
    EXPECT_FALSE(evaluator.hasError()) << evaluator.getLastError();
}

TEST(ArtifactScriptTest, DoWhileRetainsTheEvaluatorLoopLimit) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class DoWhileLimitProbe : ArtifactBehaviour
{
    void OnUpdate()
    {
        do { } while (true);
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    EXPECT_FALSE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_NE(instance.lastError().find("loop limit"), std::string::npos);
}

TEST(ArtifactScriptTest, ReturnExitsWhileForAndForeachImmediately) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class LoopReturnProbe : ArtifactBehaviour
{
    float fromWhile()
    {
        var index = 0;
        while (index < 3)
        {
            index += 1;
            return index;
        }
        return 99.0;
    }
    float fromFor()
    {
        for (int index = 0; index < 3; index++)
        {
            return index + 1;
        }
        return 99.0;
    }
    float fromForeach()
    {
        var values = [1.0, 2.0, 3.0];
        foreach (item in values)
        {
            return item;
        }
        return 99.0;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    const auto execute = [&](std::string_view methodName) {
        const auto value = evaluator.executeMethod(
            definition, methodName, {}, fields);
        EXPECT_FALSE(evaluator.hasError()) << evaluator.getLastError();
        if (const auto* number = std::get_if<double>(&value)) return *number;
        if (const auto* integer = std::get_if<std::int64_t>(&value))
            return static_cast<double>(*integer);
        ADD_FAILURE() << "expected numeric return value";
        return 0.0;
    };

    EXPECT_DOUBLE_EQ(execute("fromWhile"), 1.0);
    EXPECT_DOUBLE_EQ(execute("fromFor"), 1.0);
    EXPECT_DOUBLE_EQ(execute("fromForeach"), 1.0);
}

TEST(ArtifactScriptTest, BreakAndContinueRequireAnEnclosingLoop) {
    ArtifactScriptParser parser;
    const auto parseBody = [&](std::string_view body) {
        std::string source =
            "class InvalidLoopControl : ArtifactBehaviour\n{\n"
            "    void OnUpdate()\n    {\n";
        source.append(body);
        source.append("\n    }\n}\n");
        return parser.parse(source);
    };

    const auto breakOutsideLoop = parseBody("        break;");
    ASSERT_EQ(breakOutsideLoop.diagnostics.size(), 1u);
    EXPECT_EQ(breakOutsideLoop.diagnostics.front().line, 5u);
    EXPECT_EQ(breakOutsideLoop.diagnostics.front().column, 9u);

    const auto continueOutsideLoop = parseBody("        continue;");
    ASSERT_EQ(continueOutsideLoop.diagnostics.size(), 1u);
    EXPECT_EQ(continueOutsideLoop.diagnostics.front().line, 5u);
    EXPECT_EQ(continueOutsideLoop.diagnostics.front().column, 9u);

    const auto loopDepthDoesNotLeak = parseBody(
        "        while (false) { break; }\n        continue;");
    ASSERT_EQ(loopDepthDoesNotLeak.diagnostics.size(), 1u);
    EXPECT_EQ(loopDepthDoesNotLeak.diagnostics.front().line, 6u);
}

TEST(ArtifactScriptTest, ForLoopWithoutConditionRunsUntilControlFlowExits) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class UnconditionalForProbe : ArtifactBehaviour
{
    public float result = 0.0;
    void OnUpdate()
    {
        var count = 0;
        for (;;)
        {
            count += 1;
            if (count >= 3) break;
        }
        result = count;
    }
    float initializedLoop()
    {
        for (int index = 0;; index += 1)
        {
            if (index >= 3) break;
        }
        return index;
    }
    void exhaustLoopLimit()
    {
        for (;;) { }
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty())
        << (definition.diagnostics.empty()
                ? std::string()
                : definition.diagnostics.front().message + " at " +
                      std::to_string(definition.diagnostics.front().line) + ":" +
                      std::to_string(definition.diagnostics.front().column));

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    const auto& result = instance.fields().at("result");
    ASSERT_TRUE(std::holds_alternative<double>(result) ||
                std::holds_alternative<std::int64_t>(result));
    EXPECT_DOUBLE_EQ(std::holds_alternative<double>(result)
                         ? std::get<double>(result)
                         : static_cast<double>(std::get<std::int64_t>(result)),
                     3.0);

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    const auto initializedResult = evaluator.executeMethod(
        instance.definition(), "initializedLoop", {}, fields);
    ASSERT_TRUE(std::holds_alternative<double>(initializedResult) ||
                std::holds_alternative<std::int64_t>(initializedResult))
        << evaluator.getLastError();
    EXPECT_DOUBLE_EQ(std::holds_alternative<double>(initializedResult)
                         ? std::get<double>(initializedResult)
                         : static_cast<double>(std::get<std::int64_t>(initializedResult)),
                     3.0);

    ArtifactScriptEvaluator limitedEvaluator;
    const auto limitedResult = limitedEvaluator.executeMethod(
        instance.definition(), "exhaustLoopLimit", {}, fields);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(limitedResult));
    EXPECT_NE(limitedEvaluator.getLastError().find("loop limit"),
              std::string::npos);
}

TEST(ArtifactScriptTest, ComponentStoresPublicOverrides) {
    ArtifactScriptComponent component;
    component.setScriptClass("Spin");
    component.publicFields()["speed"] = 120.0;

    EXPECT_EQ(component.scriptClass(), "Spin");
    ASSERT_TRUE(std::holds_alternative<double>(component.publicFields().at("speed")));
    EXPECT_DOUBLE_EQ(std::get<double>(component.publicFields().at("speed")), 120.0);
}

TEST(ArtifactScriptTest, InstanceReportsLifecycleHooks) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class FadeIn : ArtifactBehaviour
{
    void OnStart() {}
}
)");

    ArtifactScriptInstance instance(std::move(definition));
    EXPECT_TRUE(instance.hasMethod("OnStart"));
    EXPECT_TRUE(instance.hasHook(ArtifactScriptHook::OnStart));
    EXPECT_FALSE(instance.hasHook(ArtifactScriptHook::OnUpdate));
}

TEST(ArtifactScriptTest, ApplyDefaultsFillsPublicFields) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Spin : ArtifactBehaviour
{
    public float speed = 90.0;
    private float time = 0.0;
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptComponent component;
    component.setScriptClass("Spin");
    component.applyDefaults(definition);

    ASSERT_TRUE(std::holds_alternative<double>(component.publicFields().at("speed")));
    EXPECT_DOUBLE_EQ(std::get<double>(component.publicFields().at("speed")), 90.0);
    EXPECT_EQ(component.publicFields().find("time"), component.publicFields().end());
}

TEST(ArtifactScriptTest, SerializedFieldsIncludePublicAndOptedInPrivateFields) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Settings : ArtifactBehaviour
{
    public float speed = 2.0;
    [SerializeField]
    private float seed = 7.0;
    private float runtimeCache = 0.0;
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ASSERT_EQ(definition.rootClass.fields.size(), 3u);
    EXPECT_EQ(definition.rootClass.fields[0].name, "speed");
    EXPECT_EQ(definition.rootClass.fields[1].name, "seed");
    EXPECT_EQ(definition.rootClass.fields[2].name, "runtimeCache");

    ArtifactScriptComponent component;
    component.setScriptClass("Settings");
    component.applyDefaults(definition);
    component.publicFields()["speed"] = 4.0;
    component.publicFields()["seed"] = 9.0;

    const auto saved = component.serializedFields(definition);
    ASSERT_EQ(saved.size(), 2u);
    EXPECT_DOUBLE_EQ(std::get<double>(saved.at("speed")), 4.0);
    EXPECT_DOUBLE_EQ(std::get<double>(saved.at("seed")), 9.0);
    EXPECT_EQ(saved.find("runtimeCache"), saved.end());
}

TEST(ArtifactScriptTest, FieldAttributesDecodeTextAndValidateRanges) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class MotionSettings : ArtifactBehaviour
{
    [Range(-2.5, +4e1)]
    [Header("Motion] Control")]
    [Tooltip("Adjust \"speed\"")]
    public float speed = 2.0;
    public Array values;
}
)");
    ASSERT_TRUE(definition.diagnostics.empty())
        << (definition.diagnostics.empty() ? "" : definition.diagnostics.front().message);
    ASSERT_EQ(definition.rootClass.fields.size(), 2u);
    const auto& speed = definition.rootClass.fields[0];
    ASSERT_TRUE(speed.hasRange);
    EXPECT_DOUBLE_EQ(speed.rangeMin, -2.5);
    EXPECT_DOUBLE_EQ(speed.rangeMax, 40.0);
    EXPECT_EQ(speed.header, "Motion] Control");
    EXPECT_EQ(speed.tooltip, "Adjust \"speed\"");
    ASSERT_TRUE(std::holds_alternative<ArtifactScriptArrayPtr>(
        definition.rootClass.fields[1].defaultValue));
    EXPECT_TRUE(std::get<ArtifactScriptArrayPtr>(
        definition.rootClass.fields[1].defaultValue)->values.empty());

    const auto parseRange = [&](std::string_view range) {
        const std::string source =
            "class InvalidRange : ArtifactBehaviour\n{\n    " +
            std::string(range) +
            "\n    public float value = 0.0;\n}\n";
        return parser.parse(source);
    };
    for (const auto invalidRange : {
             "[Range(0, 1tail)]", "[Range(2, 1)]", "[Range(0, 1, 2)]",
             "[Range(0, 1)"}) {
        const auto invalid = parseRange(invalidRange);
        ASSERT_FALSE(invalid.diagnostics.empty()) << invalidRange;
        EXPECT_EQ(invalid.diagnostics.front().line, 3u) << invalidRange;
    }

    const auto invalidHeader = parser.parse(R"(
class InvalidHeader : ArtifactBehaviour
{
    [Header("unterminated)]
    public float value = 0.0;
}
)");
    ASSERT_FALSE(invalidHeader.diagnostics.empty());
    EXPECT_EQ(invalidHeader.diagnostics.front().line, 4u);
}

TEST(ArtifactScriptTest, SerializedComponentRoundTripsAndFallsBackOnTypeMismatch) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Settings : ArtifactBehaviour
{
    public float speed = 2.0;
    [SerializeField]
    private float seed = 7.0;
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptSerializedComponent source;
    source.className = "Settings";
    source.values["speed"] = 4.5;
    source.values["seed"] = 11.25;
    source.unknown["futureOption"] = std::string("keep me");

    const auto json = serializeScriptComponent(source);
    ArtifactScriptSerializedComponent decoded;
    std::string error;
    ASSERT_TRUE(deserializeScriptComponent(json, decoded, error)) << error;
    EXPECT_EQ(decoded.className, "Settings");
    EXPECT_DOUBLE_EQ(std::get<double>(decoded.values.at("speed")), 4.5);
    EXPECT_DOUBLE_EQ(std::get<double>(decoded.values.at("seed")), 11.25);
    EXPECT_EQ(std::get<std::string>(decoded.unknown.at("futureOption")), "keep me");

    ArtifactScriptComponent component;
    component.applySerializedComponent(definition, decoded);
    EXPECT_EQ(component.scriptClass(), "Settings");
    EXPECT_DOUBLE_EQ(std::get<double>(component.publicFields().at("speed")), 4.5);
    EXPECT_DOUBLE_EQ(std::get<double>(component.publicFields().at("seed")), 11.25);
    EXPECT_EQ(std::get<std::string>(component.publicFields().at("futureOption")), "keep me");

    decoded.values["speed"] = std::string("wrong type");
    component.applySerializedComponent(definition, decoded);
    EXPECT_DOUBLE_EQ(std::get<double>(component.publicFields().at("speed")), 2.0);
}

TEST(ArtifactScriptTest, SerializedInt64ValuesRoundTripWithoutDoubleRounding) {
    ArtifactScriptSerializedComponent source;
    source.className = "ExactSerializedIntegers";
    source.values["aboveDoublePrecision"] = INT64_C(9007199254740993);
    source.values["minimum"] = std::numeric_limits<std::int64_t>::min();
    source.values["maximum"] = std::numeric_limits<std::int64_t>::max();
    auto array = makeShared<ArtifactScriptArray>();
    array->values.emplace_back(INT64_C(9007199254740993));
    array->values.emplace_back(1.5);
    source.values["items"] = ArtifactScriptArrayPtr(std::move(array));

    const std::string json = serializeScriptComponent(source);
    ArtifactScriptSerializedComponent decoded;
    std::string error;
    ASSERT_TRUE(deserializeScriptComponent(json, decoded, error)) << error;
    EXPECT_EQ(std::get<std::int64_t>(decoded.values.at("aboveDoublePrecision")),
              INT64_C(9007199254740993));
    EXPECT_EQ(std::get<std::int64_t>(decoded.values.at("minimum")),
              std::numeric_limits<std::int64_t>::min());
    EXPECT_EQ(std::get<std::int64_t>(decoded.values.at("maximum")),
              std::numeric_limits<std::int64_t>::max());
    const auto decodedArray =
        std::get<ArtifactScriptArrayPtr>(decoded.values.at("items"));
    ASSERT_NE(decodedArray, nullptr);
    ASSERT_EQ(decodedArray->values.size(), 2u);
    EXPECT_EQ(std::get<std::int64_t>(decodedArray->values[0]),
              INT64_C(9007199254740993));
    EXPECT_DOUBLE_EQ(std::get<double>(decodedArray->values[1]), 1.5);

    ArtifactScriptValue parsedInteger;
    ASSERT_TRUE(deserializeScriptValue(
        "9223372036854775807", ArtifactScriptValueType::Int,
        parsedInteger, error)) << error;
    EXPECT_EQ(std::get<std::int64_t>(parsedInteger),
              std::numeric_limits<std::int64_t>::max());
    EXPECT_FALSE(deserializeScriptValue(
        "9223372036854775808", ArtifactScriptValueType::Int,
        parsedInteger, error));
    EXPECT_EQ(error, "expected int64");
    EXPECT_FALSE(deserializeScriptValue(
        "-9223372036854775809", ArtifactScriptValueType::Int,
        parsedInteger, error));
    EXPECT_EQ(error, "expected int64");
    EXPECT_FALSE(deserializeScriptValue(
        "1.5", ArtifactScriptValueType::Int, parsedInteger, error));
    EXPECT_EQ(error, "expected int64");
}

TEST(ArtifactScriptTest, EvaluatorExecutesAssignment) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Move : ArtifactBehaviour
{
    public float x = 0.0;
    void OnUpdate(float dt) { x = x + 1.0; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ASSERT_NE(definition.rootClass.methods[0].body, nullptr);

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["x"] = 0.0;
    EXPECT_TRUE(evaluator.execute(*definition.rootClass.methods[0].body, {0.016}, fields));
    ASSERT_TRUE(std::holds_alternative<double>(fields["x"]));
    EXPECT_DOUBLE_EQ(std::get<double>(fields["x"]), 1.0);
}

TEST(ArtifactScriptTest, EvaluatorReportsMethodDeclarationLocation) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Broken : ArtifactBehaviour
{
    float fail() { return missingFunction(); }
}
)");

    ASSERT_EQ(definition.rootClass.methods.size(), 1u);
    EXPECT_EQ(definition.rootClass.methods[0].line, 4u);
    EXPECT_GT(definition.rootClass.methods[0].column, 0u);

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    evaluator.executeMethod(definition, "fail", {}, fields);

    EXPECT_TRUE(evaluator.hasError());
    EXPECT_EQ(evaluator.getLastError().rfind("line 4:", 0), 0u);
}

TEST(ArtifactScriptTest, EvaluatorHandlesIfElseAndForLoop) {
    ArtifactScriptParser parser;
    const auto conditionDefinition = parser.parse(R"(
class Test : ArtifactBehaviour
{
    public float value = 0.0;
    void OnUpdate()
    {
        if (value > 5.0) { value = 10.0; }
        else { value = 1.0; }
    }
}
)");
    ASSERT_TRUE(conditionDefinition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["value"] = 7.0;
    EXPECT_TRUE(evaluator.execute(*conditionDefinition.rootClass.methods[0].body, {}, fields));
    EXPECT_DOUBLE_EQ(std::get<double>(fields["value"]), 10.0);

    const auto loopDefinition = parser.parse(R"(
class Counter : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate()
    {
        for (int i = 0; i < 4; i = i + 1) { total = total + 2.0; }
    }
}
)");
    ASSERT_TRUE(loopDefinition.diagnostics.empty());
    fields.clear();
    fields["total"] = 0.0;
    EXPECT_TRUE(evaluator.execute(*loopDefinition.rootClass.methods[0].body, {}, fields));
    EXPECT_DOUBLE_EQ(std::get<double>(fields["total"]), 8.0);
}

TEST(ArtifactScriptTest, EvaluatorCallsUserMethodFromScript) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class MathBehaviour : ArtifactBehaviour
{
    float add(float a, float b) { return a + b; }
    float twice(float value) { return add(value, value); }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    const auto result = evaluator.executeMethod(definition, "twice", {3.0}, fields);
    ASSERT_TRUE(std::holds_alternative<double>(result)) << evaluator.getLastError();
    EXPECT_DOUBLE_EQ(std::get<double>(result), 6.0);
}

TEST(ArtifactScriptTest, ReplacingDefinitionMethodsInvalidatesCallSiteCache) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class CachedCalls : ArtifactBehaviour
{
    public float value = 0.0;
    float step(float input) { return input + 1.0; }
    void OnUpdate() { value = step(value); }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("value")), 1.0);

    auto replacement = parser.parse(R"(
class CachedCalls : ArtifactBehaviour
{
    public float value = 0.0;
    float step(float input) { return input + 10.0; }
    void OnUpdate() { value = step(value); }
}
)");
    ASSERT_TRUE(replacement.diagnostics.empty());
    instance.definition().rootClass.methods =
        std::move(replacement.rootClass.methods);
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("value")), 11.0);
}

TEST(ArtifactScriptTest, ArrayFieldDefaultsAndReads) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Points : ArtifactBehaviour
{
    public Array points;
    float first()
    {
        push(points, 7.5);
        points[0] = 9.25;
        return points[0];
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptComponent component;
    component.setScriptClass("Points");
    component.applyDefaults(definition);
    ASSERT_NE(component.publicFields().find("points"), component.publicFields().end());
    ASSERT_TRUE(std::holds_alternative<ArtifactScriptArrayPtr>(component.publicFields().at("points")));

    ArtifactScriptEvaluator evaluator;
    const auto result = evaluator.executeMethod(definition, "first", {}, component.publicFields());
    ASSERT_TRUE(std::holds_alternative<double>(result));
    EXPECT_DOUBLE_EQ(std::get<double>(result), 9.25);
}

TEST(ArtifactScriptTest, ArrayIndexRejectsNegativeAndNonFiniteValuesSafely) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class SafeArrayIndex : ArtifactBehaviour
{
    public Array values;
    public float index = 0.0;
    public float result = 0.0;
    public bool writeMode = false;
    public bool compareMode = false;
    void OnUpdate()
    {
        if (writeMode) { values[index] = 99.0; }
        else if (compareMode) { result = values[index] == 10.0 ? 1.0 : 0.0; }
        else { result = values[index]; }
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    auto values = makeShared<ArtifactScriptArray>();
    values->values = {10.0, 20.0};
    instance.fields()["values"] = values;
    instance.fields()["index"] = -1.0;
    instance.fields()["result"] = 0.0;
    instance.fields()["writeMode"] = false;
    instance.fields()["compareMode"] = false;

    const auto expectIndexError = [&](ArtifactScriptValue index) {
        instance.fields()["index"] = std::move(index);
        EXPECT_FALSE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
        EXPECT_NE(instance.lastError().find("array index out of range"),
                  std::string::npos);
    };
    expectIndexError(-1.0);
    expectIndexError(std::numeric_limits<double>::quiet_NaN());
    expectIndexError(std::numeric_limits<double>::infinity());
    expectIndexError(std::numeric_limits<double>::max());
    expectIndexError(std::numeric_limits<std::int64_t>::max());
    instance.fields()["compareMode"] = true;
    expectIndexError(-1.0);
    instance.fields()["compareMode"] = false;

    instance.fields()["index"] = 1.9;
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 20.0);

    instance.fields()["writeMode"] = true;
    instance.fields()["index"] = -0.5;
    EXPECT_FALSE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_NE(instance.lastError().find("array index out of range"),
              std::string::npos);
    EXPECT_DOUBLE_EQ(std::get<double>(values->values[1]), 20.0);
}

TEST(ArtifactScriptTest, PostfixAccessChainsAcrossArrayIndicesAndObjects) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class PostfixAccess : ArtifactBehaviour
{
    public Array matrix;
    public Array nodes;
    public float indexResult = 0.0;
    public float fieldResult = 0.0;
    public float methodResult = 0.0;
    void OnUpdate()
    {
        matrix[0][0] == 2.0;
        ++matrix[0][0];
        matrix[0][0]++;
        matrix[0][1] = 9.0;
        matrix[0][1] += 2.0;
        matrix[0][1] %= 6.0;
        nodes[0].score += 2.0;
        --nodes[0].score;
        nodes[0].score--;
        indexResult = matrix[0][0] + matrix[0][1];
        fieldResult = nodes[0].score;
        methodResult = nodes[0].getScore();
    }
}
class ChainNode : ArtifactBehaviour
{
    public float score = 0.0;
    float getScore() { return score; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));

    auto firstRow = makeShared<ArtifactScriptArray>();
    firstRow->values = {2.0, 7.0};
    auto matrix = makeShared<ArtifactScriptArray>();
    matrix->values = {firstRow};

    auto node = makeShared<ArtifactScriptObjectInstance>();
    node->className = "ChainNode";
    node->fields["score"] = 5.0;
    auto nodes = makeShared<ArtifactScriptArray>();
    nodes->values = {node};

    instance.fields()["matrix"] = matrix;
    instance.fields()["nodes"] = nodes;
    instance.fields()["indexResult"] = 0.0;
    instance.fields()["fieldResult"] = 0.0;
    instance.fields()["methodResult"] = 0.0;
    const bool invoked = instance.invokeHook(ArtifactScriptHook::OnUpdate);
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("indexResult")), 9.0);
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("fieldResult")), 5.0);
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("methodResult")), 5.0);
    ASSERT_TRUE(invoked) << instance.lastError();
}

TEST(ArtifactScriptTest, ParenthesizedExpressionsControlPrecedenceWithoutRuntimeNodes) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class GroupedExpressions : ArtifactBehaviour
{
    public Array values;
    public float groupedResult = 0.0;
    public float nestedResult = 0.0;
    public float indexedResult = 0.0;
    void OnUpdate()
    {
        groupedResult = (2.0 + 3.0) * 4.0;
        nestedResult = 2.0 * (3.0 + (4.0 * 5.0));
        indexedResult = (values)[1];
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    auto values = makeShared<ArtifactScriptArray>();
    values->values = {11.0, 23.0};
    instance.fields()["values"] = values;
    instance.fields()["groupedResult"] = 0.0;
    instance.fields()["nestedResult"] = 0.0;
    instance.fields()["indexedResult"] = 0.0;

    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    EXPECT_DOUBLE_EQ(
        std::get<double>(instance.fields().at("groupedResult")), 20.0);
    EXPECT_DOUBLE_EQ(
        std::get<double>(instance.fields().at("nestedResult")), 46.0);
    EXPECT_DOUBLE_EQ(
        std::get<double>(instance.fields().at("indexedResult")), 23.0);

    const auto malformed = parser.parse(R"(
class MissingGroupingDelimiter : ArtifactBehaviour
{
    public float result = 0.0;
    void OnUpdate() { result = (1.0 + 2.0; }
}
)");
    ASSERT_FALSE(malformed.diagnostics.empty());
    EXPECT_EQ(malformed.diagnostics.front().line, 5u);
    EXPECT_GT(malformed.diagnostics.front().column, 0u);
}

TEST(ArtifactScriptTest, SimpleAssignmentMovesValuesAcrossLocalAndArrayTargets) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class AssignedValues : ArtifactBehaviour
{
    public string observed = "";
    void assign()
    {
        Array values;
        push(values, "old");
        string local = "first";
        local = "replacement";
        values[0] = local;
        observed = values[0];
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptSerializedFields fields;
    ArtifactScriptEvaluator evaluator;
    const auto result = evaluator.executeMethod(definition, "assign", {}, fields);
    ASSERT_TRUE(std::holds_alternative<std::monostate>(result));
    EXPECT_EQ(std::get<std::string>(fields.at("observed")), "replacement");
}

TEST(ArtifactScriptTest, ArrayLiteralCreatesValues) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Points : ArtifactBehaviour
{
    float second()
    {
        Array values = [1.0, 2.5, 4.0];
        return values[1];
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    const auto result = evaluator.executeMethod(definition, "second", {}, fields);
    ASSERT_TRUE(std::holds_alternative<double>(result));
    EXPECT_DOUBLE_EQ(std::get<double>(result), 2.5);
}

TEST(ArtifactScriptTest, StringSplitPreservesEmptyAndNonOverlappingParts) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class SplitStrings : ArtifactBehaviour
{
    public Array parts;
    public Array noMatch;
    public Array emptySource;
    public Array overlappingDelimiter;
    void OnUpdate()
    {
        parts = split(",red,,blue,", ",");
        noMatch = split("unchanged", "--");
        emptySource = split("", "|");
        overlappingDelimiter = split("aaaaa", "aa");
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();

    const auto readArray = [&](const char* name) -> const auto& {
        return *std::get<ArtifactScriptArrayPtr>(instance.fields().at(name));
    };
    const auto& parts = readArray("parts").values;
    ASSERT_EQ(parts.size(), 5u);
    EXPECT_TRUE(std::get<std::string>(parts[0]).empty());
    EXPECT_EQ(std::get<std::string>(parts[1]), "red");
    EXPECT_TRUE(std::get<std::string>(parts[2]).empty());
    EXPECT_EQ(std::get<std::string>(parts[3]), "blue");
    EXPECT_TRUE(std::get<std::string>(parts[4]).empty());

    const auto& noMatch = readArray("noMatch").values;
    ASSERT_EQ(noMatch.size(), 1u);
    EXPECT_EQ(std::get<std::string>(noMatch[0]), "unchanged");

    const auto& emptySource = readArray("emptySource").values;
    ASSERT_EQ(emptySource.size(), 1u);
    EXPECT_TRUE(std::get<std::string>(emptySource[0]).empty());

    const auto& overlapping = readArray("overlappingDelimiter").values;
    ASSERT_EQ(overlapping.size(), 3u);
    EXPECT_TRUE(std::get<std::string>(overlapping[0]).empty());
    EXPECT_TRUE(std::get<std::string>(overlapping[1]).empty());
    EXPECT_EQ(std::get<std::string>(overlapping[2]), "a");
}

TEST(ArtifactScriptTest, StringSplitRejectsInvalidArguments) {
    ArtifactScriptParser parser;
    const auto run = [&](std::string_view expression) {
        auto definition = parser.parse("class InvalidSplit : ArtifactBehaviour\n{\n"
                                       "    void OnUpdate()\n    {\n        " +
                                       std::string(expression) +
                                       ";\n    }\n}\n");
        EXPECT_TRUE(definition.diagnostics.empty());
        ArtifactScriptInstance instance(std::move(definition));
        EXPECT_TRUE(instance.hasHook(ArtifactScriptHook::OnUpdate));
        EXPECT_FALSE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
        EXPECT_FALSE(instance.lastError().empty());
        return instance.lastError();
    };

    EXPECT_NE(run("split(1.0, \",\")").find("split expects"),
              std::string::npos);
    EXPECT_NE(run("split(\"value\", \"\")").find("delimiter must not be empty"),
              std::string::npos);
}

TEST(ArtifactScriptTest, StringJoinPreallocatesAndPreservesEmptyParts) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class JoinStrings : ArtifactBehaviour
{
    public string joined;
    public string emptyArray;
    public string onePart;
    public string emptyDelimiter;
    void OnUpdate()
    {
        joined = join(["red", "", "blue"], "|");
        emptyArray = join([], "|");
        onePart = join(["solo"], "|");
        emptyDelimiter = join(["a", "b"], "");
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();

    EXPECT_EQ(std::get<std::string>(instance.fields().at("joined")), "red||blue");
    EXPECT_TRUE(std::get<std::string>(instance.fields().at("emptyArray")).empty());
    EXPECT_EQ(std::get<std::string>(instance.fields().at("onePart")), "solo");
    EXPECT_EQ(std::get<std::string>(instance.fields().at("emptyDelimiter")), "ab");
}

TEST(ArtifactScriptTest, StringJoinRejectsInvalidArgumentsAndElementTypes) {
    ArtifactScriptParser parser;
    const auto run = [&](std::string_view expression) {
        auto definition = parser.parse("class InvalidJoin : ArtifactBehaviour\n{\n"
                                       "    void OnUpdate()\n    {\n        " +
                                       std::string(expression) +
                                       ";\n    }\n}\n");
        EXPECT_TRUE(definition.diagnostics.empty());
        ArtifactScriptInstance instance(std::move(definition));
        EXPECT_TRUE(instance.hasHook(ArtifactScriptHook::OnUpdate));
        EXPECT_FALSE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
        return instance.lastError();
    };

    EXPECT_NE(run("join(1.0, \",\")").find("join expects an array"),
              std::string::npos);
    EXPECT_NE(run("join([\"ok\", 2], \",\")").find("array of strings"),
              std::string::npos);
}

TEST(ArtifactScriptTest, StringReplaceUsesLiteralNonOverlappingMatches) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ReplaceStrings : ArtifactBehaviour
{
    public string repeated;
    public string noMatch;
    public string removed;
    public string overlapping;
    public string unicode;
    void OnUpdate()
    {
        repeated = replace("--a----b--", "--", "/");
        noMatch = replace("stable", "--", "/");
        removed = replace("banana", "na", "");
        overlapping = replace("aaaaa", "aa", "x");
        unicode = replace("猫/猫", "猫", "犬");
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();

    EXPECT_EQ(std::get<std::string>(instance.fields().at("repeated")), "/a//b/");
    EXPECT_EQ(std::get<std::string>(instance.fields().at("noMatch")), "stable");
    EXPECT_EQ(std::get<std::string>(instance.fields().at("removed")), "ba");
    EXPECT_EQ(std::get<std::string>(instance.fields().at("overlapping")), "xxa");
    EXPECT_EQ(std::get<std::string>(instance.fields().at("unicode")), "犬/犬");
}

TEST(ArtifactScriptTest, StringReplaceRejectsInvalidArgumentsAndEmptySearch) {
    ArtifactScriptParser parser;
    const auto run = [&](std::string_view expression) {
        auto definition = parser.parse("class InvalidReplace : ArtifactBehaviour\n{\n"
                                       "    void OnUpdate()\n    {\n        " +
                                       std::string(expression) +
                                       ";\n    }\n}\n");
        EXPECT_TRUE(definition.diagnostics.empty());
        ArtifactScriptInstance instance(std::move(definition));
        EXPECT_TRUE(instance.hasHook(ArtifactScriptHook::OnUpdate));
        EXPECT_FALSE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
        return instance.lastError();
    };

    EXPECT_NE(run("replace(1.0, \",\", \":\")").find("replace expects"),
              std::string::npos);
    EXPECT_NE(run("replace(\"value\", \"\", \"x\")").find(
                  "search string must not be empty"),
              std::string::npos);
}

TEST(ArtifactScriptTest, StringContainsUsesLiteralUtf8Substrings) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class StringContains : ArtifactBehaviour
{
    public bool findsSuffix;
    public bool rejectsDifferentCase;
    public bool acceptsEmptySubstring;
    public bool findsUnicode;
    void OnUpdate()
    {
        findsSuffix = contains("ArtifactScript", "Script");
        rejectsDifferentCase = contains("ArtifactScript", "script");
        acceptsEmptySubstring = contains("value", "");
        findsUnicode = contains("猫と犬", "犬");
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();

    EXPECT_TRUE(std::get<bool>(instance.fields().at("findsSuffix")));
    EXPECT_FALSE(std::get<bool>(instance.fields().at("rejectsDifferentCase")));
    EXPECT_TRUE(std::get<bool>(instance.fields().at("acceptsEmptySubstring")));
    EXPECT_TRUE(std::get<bool>(instance.fields().at("findsUnicode")));
}

TEST(ArtifactScriptTest, StringIndexOfReturnsUtf8ByteOffsets) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class StringIndexOf : ArtifactBehaviour
{
    public int findsSuffix = -1;
    public int acceptsEmptySubstring = -1;
    public int rejectsMissingSubstring = 0;
    public int findsUnicodeBytes = -1;
    public int choosesFirstOverlappingMatch = -1;
    void OnUpdate()
    {
        findsSuffix = indexOf("ArtifactScript", "Script");
        acceptsEmptySubstring = indexOf("value", "");
        rejectsMissingSubstring = indexOf("value", "missing");
        findsUnicodeBytes = indexOf("猫と犬", "犬");
        choosesFirstOverlappingMatch = indexOf("ababa", "aba");
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();

    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("findsSuffix")), 8);
    EXPECT_EQ(std::get<std::int64_t>(
                  instance.fields().at("acceptsEmptySubstring")), 0);
    EXPECT_EQ(std::get<std::int64_t>(
                  instance.fields().at("rejectsMissingSubstring")), -1);
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("findsUnicodeBytes")),
              6);
    EXPECT_EQ(std::get<std::int64_t>(
                  instance.fields().at("choosesFirstOverlappingMatch")), 0);
}

TEST(ArtifactScriptTest, StringPrefixSuffixMatchLiteralUtf8ByteSequences) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class StringPrefixSuffix : ArtifactBehaviour
{
    public bool findsPrefix;
    public bool findsSuffix;
    public bool rejectsWrongPrefix;
    public bool rejectsWrongSuffix;
    public bool acceptsEmptyPrefix;
    public bool acceptsEmptySuffix;
    public bool findsUnicodePrefix;
    public bool findsUnicodeSuffix;
    void OnUpdate()
    {
        findsPrefix = startsWith("ArtifactScript", "Artifact");
        findsSuffix = endsWith("ArtifactScript", "Script");
        rejectsWrongPrefix = startsWith("ArtifactScript", "script");
        rejectsWrongSuffix = endsWith("ArtifactScript", "artifact");
        acceptsEmptyPrefix = startsWith("value", "");
        acceptsEmptySuffix = endsWith("value", "");
        findsUnicodePrefix = startsWith("猫と犬", "猫");
        findsUnicodeSuffix = endsWith("猫と犬", "犬");
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();

    EXPECT_TRUE(std::get<bool>(instance.fields().at("findsPrefix")));
    EXPECT_TRUE(std::get<bool>(instance.fields().at("findsSuffix")));
    EXPECT_FALSE(std::get<bool>(instance.fields().at("rejectsWrongPrefix")));
    EXPECT_FALSE(std::get<bool>(instance.fields().at("rejectsWrongSuffix")));
    EXPECT_TRUE(std::get<bool>(instance.fields().at("acceptsEmptyPrefix")));
    EXPECT_TRUE(std::get<bool>(instance.fields().at("acceptsEmptySuffix")));
    EXPECT_TRUE(std::get<bool>(instance.fields().at("findsUnicodePrefix")));
    EXPECT_TRUE(std::get<bool>(instance.fields().at("findsUnicodeSuffix")));
}

TEST(ArtifactScriptTest, StringPrefixSuffixRejectInvalidArguments) {
    ArtifactScriptParser parser;
    const auto run = [&](std::string_view expression) {
        auto definition = parser.parse("class InvalidStringMatch : ArtifactBehaviour\n{\n"
                                       "    void OnUpdate()\n    {\n        " +
                                       std::string(expression) +
                                       ";\n    }\n}\n");
        EXPECT_TRUE(definition.diagnostics.empty());
        ArtifactScriptInstance instance(std::move(definition));
        EXPECT_TRUE(instance.hasHook(ArtifactScriptHook::OnUpdate));
        EXPECT_FALSE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
        return instance.lastError();
    };

    EXPECT_NE(run("startsWith(1, \"x\")").find("startsWith expects"),
              std::string::npos);
    EXPECT_NE(run("endsWith(\"value\")").find("endsWith expects"),
              std::string::npos);
}

TEST(ArtifactScriptTest, EvaluatorBuiltinFunctions) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Test : ArtifactBehaviour
{
    public float value = 0.0;
    void OnUpdate() { value = clamp(value, 0.0, 100.0); }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["value"] = 150.0;
    EXPECT_TRUE(evaluator.execute(*definition.rootClass.methods[0].body, {}, fields));
    EXPECT_DOUBLE_EQ(std::get<double>(fields["value"]), 100.0);
}

TEST(ArtifactScriptTest, ArraySearchBuiltinsUseScriptEqualitySemantics) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ArraySearch : ArtifactBehaviour
{
    public Array values;
    public bool mixedContains = false;
    public bool boolContains = false;
    public bool sameArrayContains = false;
    public bool freshArrayContains = true;
    public bool mismatchedTypeContains = true;
    public float mixedIndex = -1.0;
    public float stringIndex = -1.0;
    void OnUpdate()
    {
        mixedContains = contains(values, 7.0);
        boolContains = contains(values, true);
        sameArrayContains = contains(values, values[3]);
        freshArrayContains = contains(values, [3.0]);
        mismatchedTypeContains = contains(values, false);
        mixedIndex = indexOf(values, 7.0);
        stringIndex = indexOf(values, "tag");
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    auto nestedArray = makeShared<ArtifactScriptArray>();
    nestedArray->values = {3.0};
    auto object = makeShared<ArtifactScriptObjectInstance>();
    object->className = "SearchTarget";
    auto values = makeShared<ArtifactScriptArray>();
    values->values = {
        std::int64_t{7}, true, std::string("tag"), nestedArray, object};
    instance.fields()["values"] = values;
    instance.fields()["mixedContains"] = false;
    instance.fields()["boolContains"] = false;
    instance.fields()["sameArrayContains"] = false;
    instance.fields()["freshArrayContains"] = true;
    instance.fields()["mismatchedTypeContains"] = true;
    instance.fields()["mixedIndex"] = -1.0;
    instance.fields()["stringIndex"] = -1.0;

    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    EXPECT_TRUE(std::get<bool>(instance.fields().at("mixedContains")));
    EXPECT_TRUE(std::get<bool>(instance.fields().at("boolContains")));
    EXPECT_TRUE(std::get<bool>(instance.fields().at("sameArrayContains")));
    EXPECT_FALSE(std::get<bool>(instance.fields().at("freshArrayContains")));
    EXPECT_FALSE(std::get<bool>(instance.fields().at("mismatchedTypeContains")));
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("mixedIndex")), 0);
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("stringIndex")), 2);
}

TEST(ArtifactScriptTest, HotReloadMigratesFields) {
    constexpr auto sourceV1 = R"(
class Spin : ArtifactBehaviour
{
    public float speed = 90.0;
    public float radius = 50.0;
}
)";
    constexpr auto sourceV2 = R"(
class Spin : ArtifactBehaviour
{
    public float speed = 90.0;
    public float angle = 0.0;
}
)";

    ArtifactScriptParser parser;
    auto definition = parser.parse(sourceV1);
    ArtifactScriptSerializedFields fields;
    fields["speed"] = 120.0;
    fields["radius"] = 30.0;

    ArtifactScriptHotReload hotReload;
    const auto result = hotReload.reload(sourceV2, &definition, &fields);
    ASSERT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(std::get<double>(result.migratedFields.at("speed")), 120.0);
    EXPECT_EQ(result.migratedFields.find("radius"), result.migratedFields.end());
    EXPECT_DOUBLE_EQ(std::get<double>(result.migratedFields.at("angle")), 0.0);
}

TEST(ArtifactScriptTest, HotReloadMigratesSerializedPrivateFields) {
    constexpr auto sourceV1 = R"(
class Settings : ArtifactBehaviour
{
    public float speed = 2.0;
    [SerializeField]
    private float seed = 7.0;
    private float runtimeCache = 0.0;
}
)";
    constexpr auto sourceV2 = R"(
class Settings : ArtifactBehaviour
{
    public float speed = 2.0;
    [SerializeField]
    private float seed = 9.0;
    [SerializeField]
    private int persistedMode = 3;
    private float runtimeCache = 1.0;
}
)";
    ArtifactScriptParser parser;
    const auto previous = parser.parse(sourceV1);
    const auto saved = parser.parse(sourceV2);
    ASSERT_TRUE(previous.diagnostics.empty());
    ASSERT_TRUE(saved.diagnostics.empty());
    ArtifactScriptSerializedFields fields{{"speed", 4.5}, {"seed", 11.0},
                                          {"runtimeCache", 8.0}};

    ArtifactScriptHotReload hotReload;
    const auto result = hotReload.reload(sourceV2, &previous, &fields);
    ASSERT_TRUE(result.success) << result.errorMessage;
    EXPECT_DOUBLE_EQ(std::get<double>(result.migratedFields.at("speed")), 4.5);
    EXPECT_DOUBLE_EQ(std::get<double>(result.migratedFields.at("seed")), 11.0);
    EXPECT_EQ(std::get<std::int64_t>(result.migratedFields.at("persistedMode")), 3);
    EXPECT_EQ(result.migratedFields.find("runtimeCache"),
              result.migratedFields.end());
}

TEST(ArtifactScriptTest, FileAddEditAndReload) {
    const auto path = std::filesystem::temp_directory_path() / "artifact_script_hot_reload_test.artscript";
    constexpr auto sourceV1 = R"(
class Spin : ArtifactBehaviour
{
    public float speed = 90.0;
}
)";
    constexpr auto sourceV2 = R"(
class Spin : ArtifactBehaviour
{
    public float speed = 90.0;
    public float angle = 0.0;
}
)";

    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(output);
        output << sourceV1;
    }

    ArtifactScriptHotReload hotReload;
    ASSERT_TRUE(hotReload.addFile(path.string()));
    ASSERT_NE(hotReload.definitionFor(path.string()), nullptr);

    const auto firstWriteTime = std::filesystem::last_write_time(path);
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(output);
        output << sourceV2;
    }
    std::filesystem::last_write_time(path, firstWriteTime + std::chrono::seconds(1));

    const auto changes = hotReload.reloadChanged();
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_TRUE(changes[0].result.success);
    hotReload.removeFile(path.string());
    std::filesystem::remove(path);
}

TEST(ArtifactScriptTest, InvokeHookExecutesScript) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class Counter : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate(float dt) { total += dt * 2.0; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    instance.fields()["dt"] = 0.5;
    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 1.0);
    EXPECT_TRUE(instance.wasHookInvoked(ArtifactScriptHook::OnUpdate));
}

TEST(ArtifactScriptTest, StringConcatenationAndComparison) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Greet : ArtifactBehaviour
{
    public string label = "";
    string build()
    {
        string name = "world";
        label = "hello " + name + "!";
        if (name == "world") { label += " yes"; }
        if (name != "no") { label += " ne"; }
        if (name < "zzz") { label += " lt"; }
        if (name > "a") { label += " gt"; }
        if (name <= "world") { label += " le"; }
        if (name >= "world") { label += " ge"; }
        return label;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    const auto result = evaluator.executeMethod(definition, "build", {}, fields);
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_EQ(std::get<std::string>(result),
              "hello world! yes ne lt gt le ge");
}

TEST(ArtifactScriptTest, StringConcatenationPreservesScalarConversions) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class StringScalarConcatenation : ArtifactBehaviour
{
    public int count = 12;
    public string label = "";
    void build()
    {
        label = "count=" + count + ", enabled=" + true;
        label += 2.5;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptSerializedFields fields;
    fields["count"] = std::int64_t{12};
    ArtifactScriptEvaluator evaluator;
    const auto result = evaluator.executeMethod(definition, "build", {}, fields);
    ASSERT_TRUE(std::holds_alternative<std::monostate>(result));
    EXPECT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    EXPECT_EQ(std::get<std::string>(fields.at("label")),
              "count=12, enabled=true2.5");
}

TEST(ArtifactScriptTest, SimpleBinaryOperandsTrackMutableAstNames) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class MutableBinaryOperands : ArtifactBehaviour
{
    public float result = 0.0;
    void OnUpdate() { result = input + 1.0; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["input"] = 2.0;
    fields["replacement"] = 8.0;
    evaluator.executeMethod(definition, "OnUpdate", {}, fields);
    ASSERT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("result")), 3.0);

    auto& expression = definition.rootClass.methods[0]
                           .body->statements[0]->assignValue;
    ASSERT_TRUE(expression);
    ASSERT_EQ(expression->kind, ArtifactScriptExpr::Kind::Binary);
    ASSERT_TRUE(expression->left);
    ASSERT_EQ(expression->left->kind, ArtifactScriptExpr::Kind::Variable);
    expression->left->variableName = "replacement";

    evaluator.executeMethod(definition, "OnUpdate", {}, fields);
    ASSERT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("result")), 9.0);
}

TEST(ArtifactScriptTest, BinaryIndexOperandsReadCurrentValues) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class BinaryIndexOperands : ArtifactBehaviour
{
    public Array values;
    public float result = 0.0;
    void OnCreate()
    {
        push(values, 3.0);
        push(values, 4.0);
    }
    void OnUpdate() { result = values[0] + values[1]; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate))
        << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 7.0);
}

TEST(ArtifactScriptTest, StringCompoundAssignmentAppendsScalarValues) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class StringScalarCompoundAssignment : ArtifactBehaviour
{
    public string label = "start";
    void appendValues()
    {
        label += "-text";
        label += 12;
        label += false;
        label += 0.5;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptSerializedFields fields;
    fields["label"] = std::string("start");
    ArtifactScriptEvaluator evaluator;
    const auto result = evaluator.executeMethod(
        definition, "appendValues", {}, fields);
    ASSERT_TRUE(std::holds_alternative<std::monostate>(result));
    EXPECT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    EXPECT_EQ(std::get<std::string>(fields.at("label")),
              "start-text12false0.5");
}

TEST(ArtifactScriptTest, StringAdditionFusionDoesNotRepeatCalls) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class StringAdditionSideEffects : ArtifactBehaviour
{
    public int calls = 0;
    public string label = "";
    string middle() { calls += 1; return "middle"; }
    void build() { label = "before" + middle() + "after"; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptSerializedFields fields;
    fields["calls"] = 0.0;
    fields["label"] = std::string{};
    ArtifactScriptEvaluator evaluator;
    const auto result = evaluator.executeMethod(definition, "build", {}, fields);
    ASSERT_TRUE(std::holds_alternative<std::monostate>(result));
    EXPECT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("calls")), 1.0);
    EXPECT_EQ(std::get<std::string>(fields.at("label")),
              "beforemiddleafter");
}

TEST(ArtifactScriptTest, CompoundAssignmentAndIncrement) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Math : ArtifactBehaviour
{
    public float value = 10.0;
    float run()
    {
        value += 5.0;
        value -= 3.0;
        value *= 2.0;
        value /= 4.0;
        value++;
        return value;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["value"] = 10.0;
    const auto result = evaluator.executeMethod(definition, "run", {}, fields);
    ASSERT_TRUE(std::holds_alternative<double>(result));
    // ((10 + 5 - 3) * 2 / 4) + 1 = 7
    EXPECT_DOUBLE_EQ(std::get<double>(result), 7.0);
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("value")), 7.0);
}

TEST(ArtifactScriptTest, NumericCompoundAssignmentUpdatesAllTargets) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class NumericCompoundAssignment : ArtifactBehaviour
{
    public float value = 10.0;
    public int step = 2;
    public Array values;
    void OnCreate() { push(values, 3.0); }
    float run()
    {
        var local = 10.0;
        local += step;
        values[0] *= local;
        value += step;
        value -= 2.0;
        value *= 2.0;
        value /= 4.0;
        value %= 4.0;
        return values[0] + value;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptComponent component;
    component.setScriptClass("NumericCompoundAssignment");
    component.applyDefaults(definition);
    auto& fields = component.publicFields();
    evaluator.executeMethod(definition, "OnCreate", {}, fields);
    ASSERT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    const auto result = evaluator.executeMethod(definition, "run", {}, fields);
    ASSERT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    ASSERT_TRUE(std::holds_alternative<double>(result));
    // local = 12, array value = 3 * 12, and field value = (((10 + 2 - 2) * 2) / 4) % 4.
    EXPECT_DOUBLE_EQ(std::get<double>(result), 37.0);
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("value")), 1.0);
    const auto& values = std::get<ArtifactScriptArrayPtr>(fields.at("values"));
    ASSERT_TRUE(values);
    ASSERT_EQ(values->values.size(), 1u);
    EXPECT_DOUBLE_EQ(std::get<double>(values->values[0]), 36.0);
}

TEST(ArtifactScriptTest, NumericCompoundDivisionByZeroKeepsDiagnostic) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class NumericCompoundDivisionByZero : ArtifactBehaviour
{
    public float value = 10.0;
    void OnUpdate() { value /= 0.0; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["value"] = 10.0;
    const auto result =
        evaluator.executeMethod(definition, "OnUpdate", {}, fields);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(result));
    EXPECT_NE(evaluator.getLastError().find("div0"), std::string::npos);
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("value")), 10.0);
}

TEST(ArtifactScriptTest, StringCompoundAssignmentUpdatesLocalsFieldsAndArrayItems) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class StringCompoundAssignment : ArtifactBehaviour
{
    public string field = "field";
    public Array values;
    public string result = "";
    void OnCreate()
    {
        field = "field";
        push(values, "array");
    }
    void OnUpdate()
    {
        string local = "local";
        local += "-done";
        field += "-done";
        values[0] += "-done";
        result = local + "|" + field + "|" + values[0];
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate))
        << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    EXPECT_EQ(std::get<std::string>(instance.fields().at("field")), "field-done");
    EXPECT_EQ(std::get<std::string>(instance.fields().at("result")),
              "local-done|field-done|array-done");
    const auto values = std::get<ArtifactScriptArrayPtr>(instance.fields().at("values"));
    ASSERT_TRUE(values);
    ASSERT_EQ(values->values.size(), 1u);
    EXPECT_EQ(std::get<std::string>(values->values.front()), "array-done");
}

TEST(ArtifactScriptTest, BreakAndContinue) {
    ArtifactScriptParser parser;
    const auto breakDefinition = parser.parse(R"(
class Loop : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate()
    {
        for (int i = 0; i < 10; i++) {
            if (i == 3) { break; }
            total++;
        }
    }
}
)");
    ASSERT_TRUE(breakDefinition.diagnostics.empty());
    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["total"] = 0.0;
    EXPECT_TRUE(evaluator.execute(*breakDefinition.rootClass.methods[0].body, {}, fields));
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("total")), 3.0);

    const auto continueDefinition = parser.parse(R"(
class Skip : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate()
    {
        for (int i = 0; i < 5; i++) {
            if (i == 2) { continue; }
            total += 1.0;
        }
    }
}
)");
    ASSERT_TRUE(continueDefinition.diagnostics.empty());
    fields.clear();
    fields["total"] = 0.0;
    EXPECT_TRUE(evaluator.execute(*continueDefinition.rootClass.methods[0].body, {}, fields));
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("total")), 4.0);
}

TEST(ArtifactScriptTest, NullLiteralHasDistinctEqualityAndKeywordPrefixIdentifiers) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class NullSemantics : ArtifactBehaviour
{
    public float result = 0.0;
    void OnUpdate()
    {
        var absent = null;
        var null_value = 2.0;
        if (absent == null) { result += 1.0; }
        if (absent != 0.0) { result += 2.0; }
        if (absent != false) { result += 4.0; }
        if (null == null) { result += 8.0; }
        result += null_value;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 17.0);
}

TEST(ArtifactScriptTest, TernaryOperator) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Pick : ArtifactBehaviour
{
    public float result = 0.0;
    public string label = "";
    void OnUpdate()
    {
        result = value > 10 ? 1.0 : -1.0;
        label = value > 10 ? "big" : "small";
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["value"] = 20.0;
    fields["result"] = 0.0;
    fields["label"] = std::string();
    EXPECT_TRUE(evaluator.execute(*definition.rootClass.methods[0].body, {}, fields));
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("result")), 1.0);
    EXPECT_EQ(std::get<std::string>(fields.at("label")), "big");
}

TEST(ArtifactScriptTest, ShortCircuitEvaluation) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Guard : ArtifactBehaviour
{
    public float total = 0.0;
    public bool conjunction = false;
    public bool disjunction = false;
    public bool falseConjunction = true;
    public bool trueDisjunction = false;
    float sideEffect()
    {
        total = total + 1.0;
        return true;
    }
    void OnUpdate()
    {
        conjunction = 1 && 2;
        disjunction = 0 || 2;
        falseConjunction = 0 && sideEffect();
        trueDisjunction = 1 || sideEffect();
        // false && sideEffect() must not run sideEffect
        if (false && sideEffect()) { total = 100.0; }
        // true || sideEffect() must not run it either
        if (true || sideEffect()) { }
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["total"] = 0.0;
    EXPECT_TRUE(std::holds_alternative<std::monostate>(
        evaluator.executeMethod(definition, "OnUpdate", {}, fields)));
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("total")), 0.0);
    EXPECT_TRUE(std::get<bool>(fields.at("conjunction")));
    EXPECT_TRUE(std::get<bool>(fields.at("disjunction")));
    EXPECT_FALSE(std::get<bool>(fields.at("falseConjunction")));
    EXPECT_TRUE(std::get<bool>(fields.at("trueDisjunction")));
}

TEST(ArtifactScriptTest, NullCoalescingIsLazyRightAssociativeAndNullSpecific) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class Coalesce : ArtifactBehaviour
{
    public float calls = 0.0;
    public float result = 0.0;
    public string label = "";
    float fallback()
    {
        calls += 1.0;
        return 7.0;
    }

    void OnUpdate()
    {
        var absent = null;
        var present = 3.0;
        var zero = 0.0;
        result = absent ?? null ?? fallback();
        result += present ?? fallback();
        result += zero ?? fallback();
        var precedence = null ?? false ? 1.0 : 2.0;
        result += precedence;
        label = "" ?? "fallback";
        if (false ?? true) { calls += 100.0; }
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ASSERT_EQ(definition.rootClass.methods.size(), 2u);
    const auto& updateStatements =
        definition.rootClass.methods[1].body->statements;
    ASSERT_GE(updateStatements.size(), 4u);
    const auto& chainedCoalesce = *updateStatements[3]->assignValue;
    ASSERT_EQ(chainedCoalesce.kind, ArtifactScriptExpr::Kind::Binary);
    ASSERT_EQ(chainedCoalesce.binaryOp, ArtifactScriptBinaryOp::Coalesce);
    ASSERT_NE(chainedCoalesce.right, nullptr);
    EXPECT_EQ(chainedCoalesce.right->kind, ArtifactScriptExpr::Kind::Binary);
    EXPECT_EQ(chainedCoalesce.right->binaryOp, ArtifactScriptBinaryOp::Coalesce);

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("calls")), 1.0);
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 12.0);
    EXPECT_EQ(std::get<std::string>(instance.fields().at("label")), "");
}

TEST(ArtifactScriptTest, NullCoalescingAssignmentIsLazyAcrossLvalueKinds) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class CoalesceAssignment : ArtifactBehaviour
{
    public float calls = 0.0;
    public float indexCalls = 0.0;
    public float result = 0.0;
    public bool enabled = false;
    public int zeroValue = 0;
    public string label = "";
    public Array values;
    public Payload payload;

    float fallback()
    {
        calls += 1.0;
        return 7.0;
    }

    int nextIndex()
    {
        indexCalls += 1.0;
        return 0;
    }

    int invalidFallback()
    {
        return 9223372036854775807 + 1;
    }

    void OnUpdate()
    {
        enabled ??= fallback();
        zeroValue ??= fallback();
        label ??= "fallback";

        var localValue = 3.0;
        localValue ??= fallback();
        result += localValue;

        var preservedValue = 9.0;
        preservedValue ??= invalidFallback();
        result += preservedValue;

        var missingValue = null;
        missingValue ??= fallback();
        result += missingValue;

        values = [1.0, null];
        values[nextIndex()] ??= fallback();
        values[1] ??= fallback();
        result += values[1];

        payload = new Payload();
        payload.value ??= fallback();
        payload.value ??= fallback();
        result += payload.value;
    }
}
class Payload : ArtifactBehaviour
{
    public var value = null;
}
)");
    ASSERT_TRUE(definition.diagnostics.empty())
        << (definition.diagnostics.empty()
                ? ""
                : definition.diagnostics.front().message);

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("calls")), 3.0);
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("indexCalls")), 1.0);
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 33.0);
    EXPECT_FALSE(std::get<bool>(instance.fields().at("enabled")));
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("zeroValue")), 0);
    EXPECT_TRUE(std::get<std::string>(instance.fields().at("label")).empty());
    const auto& values = std::get<ArtifactScriptArrayPtr>(
        instance.fields().at("values"))->values;
    EXPECT_DOUBLE_EQ(std::get<double>(values[0]), 1.0);
    EXPECT_DOUBLE_EQ(std::get<double>(values[1]), 7.0);
    const auto& payload = std::get<ArtifactScriptObjectInstancePtr>(
        instance.fields().at("payload"));
    ASSERT_NE(payload, nullptr);
    EXPECT_DOUBLE_EQ(std::get<double>(payload->fields.at("value")), 7.0);
}

TEST(ArtifactScriptTest, VarDeclarationAndForeach) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Sum : ArtifactBehaviour
{
    public Array values;
    public float total = 0.0;
    void OnUpdate()
    {
        push(values, 2.0);
        push(values, 3.0);
        var sum = 0.0;
        foreach (item in values) {
            sum += item;
        }
        total = sum;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptComponent component;
    component.setScriptClass("Sum");
    component.applyDefaults(definition);

    ArtifactScriptEvaluator evaluator;
    evaluator.executeMethod(definition, "OnUpdate", {}, component.publicFields());
    EXPECT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    EXPECT_DOUBLE_EQ(std::get<double>(component.publicFields().at("total")), 5.0);
}

TEST(ArtifactScriptTest,
     MutableDefinitionInvalidatesForeachMutationAnalysisCache) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ForeachMutationCacheProbe : ArtifactBehaviour
{
    public Array values;
    public float total = 0.0;
    void OnUpdate()
    {
        foreach (item in values) { total += item; }
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    auto values = makeShared<ArtifactScriptArray>();
    values->values.push_back(1.0);
    values->values.push_back(2.0);
    instance.fields()["values"] = values;
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 3.0);

    auto updatedDefinition = parser.parse(R"(
class ForeachMutationCacheProbe : ArtifactBehaviour
{
    public Array values;
    public float total = 0.0;
    void OnUpdate()
    {
        foreach (item in values) {
            values[1] = 9.0;
            total += item;
        }
    }
}
)");
    ASSERT_TRUE(updatedDefinition.diagnostics.empty());
    auto& mutableDefinition = instance.definition();
    ASSERT_EQ(mutableDefinition.rootClass.methods.size(), 1u);
    mutableDefinition.rootClass.methods[0].body =
        std::move(updatedDefinition.rootClass.methods[0].body);
    values->values[0] = 1.0;
    values->values[1] = 2.0;
    instance.fields()["total"] = 0.0;

    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 3.0);
    EXPECT_DOUBLE_EQ(std::get<double>(values->values[1]), 9.0);
}

TEST(ArtifactScriptTest, ForeachUsesSnapshotWhenSourceArrayMutates) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class MutatingForeach : ArtifactBehaviour
{
    public Array values;
    public float total = 0.0;
    public float item = 41.0;
    void OnCreate()
    {
        push(values, 2.0);
        push(values, 3.0);
    }
    void OnUpdate()
    {
        foreach (item in values) {
            total += item;
            push(values, item);
        }
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 5.0);
    const auto values = std::get<ArtifactScriptArrayPtr>(instance.fields().at("values"));
    ASSERT_TRUE(values);
    ASSERT_EQ(values->values.size(), 4u);
    EXPECT_DOUBLE_EQ(std::get<double>(values->values[2]), 2.0);
    EXPECT_DOUBLE_EQ(std::get<double>(values->values[3]), 3.0);
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("item")), 41.0);
}

TEST(ArtifactScriptTest, ReadOnlyForeachKeepsOriginalArrayWhenSourceFieldChanges) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ReplacedForeachSource : ArtifactBehaviour
{
    public Array values;
    public Array replacement;
    public float total = 0.0;
    void OnCreate()
    {
        push(values, 1.0);
        push(values, 2.0);
        push(replacement, 9.0);
    }
    void OnUpdate()
    {
        foreach (item in values) {
            total += item;
            values = replacement;
        }
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 3.0);
    const auto values = std::get<ArtifactScriptArrayPtr>(instance.fields().at("values"));
    ASSERT_TRUE(values);
    ASSERT_EQ(values->values.size(), 1u);
    EXPECT_DOUBLE_EQ(std::get<double>(values->values.front()), 9.0);
}

TEST(ArtifactScriptTest, ForeachLoopItemWriteDoesNotMutateArrayOrExistingField) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class WritableForeachItem : ArtifactBehaviour
{
    public Array values;
    public float total = 0.0;
    public float item = 41.0;
    void OnCreate()
    {
        push(values, 2.0);
        push(values, 3.0);
    }
    void OnUpdate()
    {
        foreach (item in values) {
            item += 1.0;
            total += item;
        }
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 7.0);
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("item")), 41.0);
    const auto values = std::get<ArtifactScriptArrayPtr>(instance.fields().at("values"));
    ASSERT_TRUE(values);
    ASSERT_EQ(values->values.size(), 2u);
    EXPECT_DOUBLE_EQ(std::get<double>(values->values[0]), 2.0);
    EXPECT_DOUBLE_EQ(std::get<double>(values->values[1]), 3.0);
}

TEST(ArtifactScriptTest, StringForeachSnapshotSurvivesMutationAndRepeatedHooks) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class MutatingStringForeach : ArtifactBehaviour
{
    public Array values;
    public float visited = 0.0;
    void OnCreate()
    {
        push(values, "first string longer than small string storage");
        push(values, "second string also longer than small string storage");
    }
    void OnUpdate()
    {
        foreach (item in values) {
            visited += 1;
            push(values, item);
        }
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("visited")), 2.0);
    auto values = std::get<ArtifactScriptArrayPtr>(instance.fields().at("values"));
    ASSERT_TRUE(values);
    ASSERT_EQ(values->values.size(), 4u);
    EXPECT_EQ(std::get<std::string>(values->values[2]),
              "first string longer than small string storage");
    EXPECT_EQ(std::get<std::string>(values->values[3]),
              "second string also longer than small string storage");

    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("visited")), 6.0);
    EXPECT_EQ(values->values.size(), 8u);
}

TEST(ArtifactScriptTest, PopPreservesLongStringContents) {
    constexpr std::string_view expected =
        "a long string value that exceeds the small string buffer";
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class PopStringValue : ArtifactBehaviour
{
    public Array values;
    public string result = "";
    void OnCreate() { push(values, "a long string value that exceeds the small string buffer"); }
    void OnUpdate() { result = pop(values); }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_EQ(std::get<std::string>(instance.fields().at("result")), expected);
    const auto values = std::get<ArtifactScriptArrayPtr>(instance.fields().at("values"));
    ASSERT_TRUE(values);
    EXPECT_TRUE(values->values.empty());
}

TEST(ArtifactScriptTest, NestedForeachScopesCommitFieldWrites) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class NestedForeach : ArtifactBehaviour
{
    public Array outerValues;
    public Array innerValues;
    public float total = 0.0;
    void OnCreate()
    {
        push(outerValues, 2.0);
        push(outerValues, 3.0);
        push(innerValues, 4.0);
        push(innerValues, 5.0);
    }
    void OnUpdate()
    {
        foreach (outer in outerValues) {
            foreach (inner in innerValues) {
                total += outer * inner;
            }
        }
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 45.0);
}

TEST(ArtifactScriptTest, LargeForeachUsesSnapshotWhenSourceArrayMutates) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class LargeMutatingForeach : ArtifactBehaviour
{
    public Array values;
    public float total = 0.0;
    void OnCreate()
    {
        for (int index = 0; index < 1025; index += 1) {
            push(values, index);
        }
    }
    void OnUpdate()
    {
        foreach (item in values) {
            total += item;
            if (item == 0.0) push(values, 1000.0);
        }
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 524800.0);
    auto values = std::get<ArtifactScriptArrayPtr>(instance.fields().at("values"));
    ASSERT_TRUE(values);
    ASSERT_EQ(values->values.size(), 1026u);

    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 1050600.0);
    EXPECT_EQ(values->values.size(), 1027u);
}

TEST(ArtifactScriptTest, DeepForeachFallsBackAfterReusableSnapshotLimit) {
    std::string source = R"(
class DeepForeach : ArtifactBehaviour
{
    public Array values;
    public float visits = 0.0;
    void OnCreate() { push(values, 1.0); }
    void OnUpdate() {
)";
    constexpr int nestedDepth = 9;
    for (int depth = 0; depth < nestedDepth; ++depth) {
        source += "foreach (item" + std::to_string(depth) + " in values) {\n";
    }
    source += "visits += 1.0;\n";
    for (int depth = 0; depth < nestedDepth; ++depth) source += "}\n";
    source += "}\n}\n";

    ArtifactScriptParser parser;
    auto definition = parser.parse(source);
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("visits")), 1.0);
}

TEST(ArtifactScriptTest, ForeachOverlayOverflowCommitsFields) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ForeachOverlayOverflow : ArtifactBehaviour
{
    public Array values;
    public float first = 0.0;
    public float second = 0.0;
    public float third = 0.0;
    public float fourth = 0.0;
    public float fifth = 0.0;
    public float sixth = 0.0;
    public float seventh = 0.0;
    public float eighth = 0.0;
    public float ninth = 0.0;
    void OnCreate() { push(values, 1.0); }
    void OnUpdate()
    {
        foreach (item in values) {
            first += item;
            second += item;
            third += item;
            fourth += item;
            fifth += item;
            sixth += item;
            seventh += item;
            eighth += item;
            ninth += item;
        }
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    for (const auto* field : {"first", "second", "third", "fourth", "fifth",
                              "sixth", "seventh", "eighth", "ninth"}) {
        EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at(field)), 1.0);
    }
}

TEST(ArtifactScriptTest, FailedForeachDoesNotCommitScalarFieldWrites) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class FailedForeach : ArtifactBehaviour
{
    public Array values;
    public float total = 7.0;
    void OnCreate() { push(values, 1.0); }
    void OnUpdate()
    {
        foreach (item in values) {
            total += 1.0;
            missing;
        }
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    EXPECT_FALSE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 7.0);
}

TEST(ArtifactScriptTest, UserMethodArgumentsUseOverflowStorageAboveFive) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class SixArguments : ArtifactBehaviour
{
    public float total = 0.0;
    float sum(float a, float b, float c, float d, float e, float f)
    {
        return a + b + c + d + e + f;
    }
    void OnUpdate() {
        total = sum(1.0, 2.0, 3.0, 4.0, 5.0,
                    sum(1.0, 2.0, 3.0, 4.0, 5.0, 6.0));
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 36.0);
}

TEST(ArtifactScriptTest, UserMethodArgumentsAboveReusableLimitUseFallback) {
    std::string parameters;
    std::string arguments;
    for (int i = 0; i < 33; ++i) {
        if (i != 0) {
            parameters += ", ";
            arguments += ", ";
        }
        parameters += "float p" + std::to_string(i);
        arguments += std::to_string(i + 1) + ".0";
    }
    const std::string source =
        "class ThirtyThreeArguments : ArtifactBehaviour {\n"
        "public float total = 0.0;\n"
        "float edge(" + parameters + ") { return p0 + p32; }\n"
        "void OnUpdate() { total = edge(" + arguments + "); }\n"
        "}";
    ArtifactScriptParser parser;
    auto definition = parser.parse(source);
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 34.0);
}

TEST(ArtifactScriptTest, NestedCallsBeyondReusableDepthUseFallback) {
    std::string nestedCall = "1.0";
    for (int depth = 0; depth < 10; ++depth) {
        nestedCall = "sum6(0.0, 0.0, 0.0, 0.0, 0.0, " + nestedCall + ")";
    }
    const std::string source =
        "class DeepArgumentCalls : ArtifactBehaviour {\n"
        "public float total = 0.0;\n"
        "float sum6(float a, float b, float c, float d, float e, float f) "
        "{ return a + b + c + d + e + f; }\n"
        "void OnUpdate() { total = " + nestedCall + "; }\n"
        "}";
    ArtifactScriptParser parser;
    auto definition = parser.parse(source);
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 1.0);
}

TEST(ArtifactScriptTest, MoreThanTwelveLocalsUseOverflowStorage) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ManyLocals : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate()
    {
        float a = 1.0;
        float b = 2.0;
        float c = 3.0;
        float d = 4.0;
        float e = 5.0;
        float f = 6.0;
        float g = 7.0;
        float h = 8.0;
        float i = 9.0;
        float j = 10.0;
        float k = 11.0;
        float l = 12.0;
        float m = 13.0;
        float n = 14.0;
        total = a + b + c + d + e + f + g + h + i + j + k + l + m + n;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 105.0);
}

TEST(ArtifactScriptTest, LocalsBeyondReusableOverflowCapacityUseFallback) {
    std::string source =
        "class FortyFiveLocals : ArtifactBehaviour {\n"
        "public float total = 0.0;\n"
        "void OnUpdate() {\n";
    for (int i = 0; i < 45; ++i) {
        source += "float value" + std::to_string(i) + " = " +
                  std::to_string(i + 1) + ".0;\n";
    }
    source += "total = value0 + value44;\n}\n}";

    ArtifactScriptParser parser;
    auto definition = parser.parse(source);
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 46.0);
}

TEST(ArtifactScriptTest, HostBindingRegistry) {
    ArtifactScriptHost& host = ArtifactScriptHost::global();
    host.registerFunction("doubleIt", [&host](std::span<const ArtifactScriptValue> args) -> ArtifactScriptValue {
        if (args.size() != 1 || !std::holds_alternative<double>(args[0])) {
            host.setLastError("doubleIt expects one number");
            return {};
        }
        return std::get<double>(args[0]) * 2.0;
    });

    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Use : ArtifactBehaviour
{
    public float value = 0.0;
    void OnUpdate() { value = doubleIt(21.0); }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["value"] = 0.0;
    EXPECT_TRUE(evaluator.execute(*definition.rootClass.methods[0].body, {}, fields));
    EXPECT_TRUE(evaluator.getLastError().empty());
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("value")), 42.0);
}

TEST(ArtifactScriptTest, ClassLookupIndexTracksDefinitionChangesBetweenExecutions) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class LookupRoot : ArtifactBehaviour
{
    public ObjectRef target;
    void OnUpdate()
    {
        target = new First();
    }
}
class First : ArtifactBehaviour
{
    public float value = 1.0;
}
class Second : ArtifactBehaviour
{
    public float value = 2.0;
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ASSERT_EQ(definition.rootClass.methods.size(), 1u);
    ASSERT_EQ(definition.rootClass.methods[0].body->statements.size(), 1u);

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    evaluator.executeMethod(definition, "OnUpdate", {}, fields);
    ASSERT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    ASSERT_TRUE(std::holds_alternative<ArtifactScriptObjectInstancePtr>(fields.at("target")));
    ASSERT_TRUE(std::get<ArtifactScriptObjectInstancePtr>(fields.at("target")));
    EXPECT_EQ(std::get<ArtifactScriptObjectInstancePtr>(fields.at("target"))->className, "First");

    auto& newExpression = definition.rootClass.methods[0].body->statements[0]->assignValue;
    ASSERT_TRUE(newExpression);
    ASSERT_EQ(newExpression->kind, ArtifactScriptExpr::Kind::New);
    newExpression->newClassName = "Second";

    evaluator.executeMethod(definition, "OnUpdate", {}, fields);
    ASSERT_TRUE(std::holds_alternative<ArtifactScriptObjectInstancePtr>(fields.at("target")));
    ASSERT_TRUE(std::get<ArtifactScriptObjectInstancePtr>(fields.at("target")));
    EXPECT_EQ(std::get<ArtifactScriptObjectInstancePtr>(fields.at("target"))->className, "Second");
}

TEST(ArtifactScriptTest, PrintLogCollectsOutput) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Talker : ArtifactBehaviour
{
    void OnUpdate()
    {
        print("hello", 42);
        log("second");
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptHost& host = ArtifactScriptHost::global();
    host.drainLog();
    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    EXPECT_TRUE(evaluator.execute(*definition.rootClass.methods[0].body, {}, fields));

    const auto lines = host.drainLog();
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "hello 42");
    EXPECT_EQ(lines[1], "second");
}
