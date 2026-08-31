#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <span>
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

    ArtifactScriptComponent component;
    component.setScriptClass("Spin");
    component.applyDefaults(definition);

    ASSERT_TRUE(std::holds_alternative<double>(component.publicFields().at("speed")));
    EXPECT_DOUBLE_EQ(std::get<double>(component.publicFields().at("speed")), 90.0);
    EXPECT_EQ(component.publicFields().find("time"), component.publicFields().end());
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
    ASSERT_TRUE(std::holds_alternative<double>(result));
    EXPECT_DOUBLE_EQ(std::get<double>(result), 6.0);
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
    ASSERT_TRUE(std::holds_alternative<ArtifactScriptArrayPtr>(component.publicFields().at("points")));

    ArtifactScriptEvaluator evaluator;
    const auto result = evaluator.executeMethod(definition, "first", {}, component.publicFields());
    ASSERT_TRUE(std::holds_alternative<double>(result));
    EXPECT_DOUBLE_EQ(std::get<double>(result), 9.25);
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
        return label;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    const auto result = evaluator.executeMethod(definition, "build", {}, fields);
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_EQ(std::get<std::string>(result), "hello world! yes ne");
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
    float sideEffect()
    {
        total = total + 1.0;
        return true;
    }
    void OnUpdate()
    {
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
    EXPECT_TRUE(evaluator.execute(*definition.rootClass.methods[0].body, {}, fields));
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("total")), 0.0);
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
    EXPECT_FALSE(evaluator.hasError());
    EXPECT_DOUBLE_EQ(std::get<double>(component.publicFields().at("total")), 5.0);
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
