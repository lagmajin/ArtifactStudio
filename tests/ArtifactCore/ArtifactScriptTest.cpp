#include <gtest/gtest.h>
#include <variant>
#include <chrono>
#include <filesystem>
#include <fstream>

import Script.ArtifactScript;

using namespace ArtifactCore;

TEST(ArtifactScriptTest, ParseClassAndFields) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
class Spin : ArtifactBehaviour
{
    public float speed = 90.0;
    private float time = 0.0;

    void OnUpdate(float dt)
    {
    }
}
)");

    ASSERT_FALSE(def.rootClass.name.empty());
    EXPECT_EQ(def.rootClass.name, "Spin");
    EXPECT_TRUE(def.rootClass.derivesFromBehaviour);
    ASSERT_EQ(def.rootClass.fields.size(), 2u);
    EXPECT_TRUE(def.rootClass.fields[0].isPublic);
    EXPECT_FALSE(def.rootClass.fields[1].isPublic);
    ASSERT_EQ(def.rootClass.methods.size(), 1u);
    EXPECT_EQ(def.rootClass.methods[0].name, "OnUpdate");
    EXPECT_TRUE(def.rootClass.methods[0].isLifecycleHook);
    EXPECT_TRUE(def.diagnostics.empty());
}

TEST(ArtifactScriptTest, ComponentStoresPublicOverrides) {
    ArtifactScriptComponent component;
    component.setScriptClass("Spin");
    component.publicFields()["speed"] = 120.0;

    EXPECT_EQ(component.scriptClass(), "Spin");
    ASSERT_TRUE(std::holds_alternative<double>(component.publicFields().at("speed")));
    EXPECT_EQ(std::get<double>(component.publicFields().at("speed")), 120.0);
}

TEST(ArtifactScriptTest, InstanceReportsLifecycleHooks) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
class FadeIn : ArtifactBehaviour
{
    void OnStart()
    {
    }
}
)");

    ArtifactScriptInstance instance(def);
    EXPECT_TRUE(instance.hasMethod("OnStart"));
    EXPECT_TRUE(instance.hasHook(ArtifactScriptHook::OnStart));
    EXPECT_FALSE(instance.hasHook(ArtifactScriptHook::OnUpdate));
}

TEST(ArtifactScriptTest, ApplyDefaultsFillsPublicFields) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
class Spin : ArtifactBehaviour
{
    public float speed = 90.0;
    private float time = 0.0;
}
)");

    ArtifactScriptComponent component;
    component.setScriptClass("Spin");
    component.applyDefaults(def);

    ASSERT_TRUE(std::holds_alternative<double>(component.publicFields().at("speed")));
    EXPECT_EQ(std::get<double>(component.publicFields().at("speed")), 90.0);
    EXPECT_EQ(component.publicFields().find("time"), component.publicFields().end());
}

TEST(ArtifactScriptTest, InvokeHookTracksLastHook) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
class Spin : ArtifactBehaviour
{
    void OnUpdate(float dt)
    {
    }
}
)");

    ArtifactScriptInstance instance(def);
    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_TRUE(instance.wasHookInvoked(ArtifactScriptHook::OnUpdate));
    EXPECT_FALSE(instance.invokeHook(ArtifactScriptHook::OnDestroy));
}

TEST(ArtifactScriptTest, ParseMethodBodyAssignment) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
class Move : ArtifactBehaviour
{
    public float x = 0.0;
    void OnUpdate(float dt)
    {
        x = x + dt * 10.0;
    }
}
)");
    ASSERT_EQ(def.rootClass.methods.size(), 1u);
    ASSERT_NE(def.rootClass.methods[0].body, nullptr);
    EXPECT_EQ(def.rootClass.methods[0].body->statements.size(), 1u);
}

TEST(ArtifactScriptTest, EvaluatorExecutesAssignment) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
class Move : ArtifactBehaviour
{
    public float x = 0.0;
    void OnUpdate(float dt)
    {
        x = x + 1.0;
    }
}
)");
    ASSERT_NE(def.rootClass.methods[0].body, nullptr);

    ArtifactScriptEvaluator eval;
    ArtifactScriptSerializedFields fields;
    fields["x"] = 0.0;

    std::vector<ArtifactScriptValue> args;
    args.push_back(0.016); // dt
    EXPECT_TRUE(eval.execute(*def.rootClass.methods[0].body, args, fields));
    EXPECT_FALSE(eval.hasError());

    ASSERT_TRUE(std::holds_alternative<double>(fields["x"]));
    EXPECT_DOUBLE_EQ(std::get<double>(fields["x"]), 1.0);
}

TEST(ArtifactScriptTest, EvaluatorHandlesIfElse) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
class Test : ArtifactBehaviour
{
    public float val = 0.0;
    void OnUpdate(float dt)
    {
        if (val > 5.0) {
            val = 10.0;
        } else {
            val = 1.0;
        }
    }
}

TEST(ArtifactScriptTest, EvaluatorHandlesForLoop) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
class Counter : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate()
    {
        for (int i = 0; i < 4; i = i + 1) {
            total = total + 2;
        }
    }
}

TEST(ArtifactScriptTest, EvaluatorExecutesUserMethodAndReturnsValue) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
class MathBehaviour : ArtifactBehaviour
{
    float add(float a, float b)
    {
        return a + b;
    }
}

TEST(ArtifactScriptTest, EvaluatorCallsUserMethodFromScript) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
class MathBehaviour : ArtifactBehaviour
{
    float add(float a, float b) { return a + b; }
    float twice(float value) { return add(value, value); }
}
)");
    ASSERT_TRUE(def.diagnostics.empty());
    ArtifactScriptEvaluator eval;
    ArtifactScriptSerializedFields fields;
    const auto result = eval.executeMethod(def, "twice", {3.0}, fields);
    ASSERT_TRUE(std::holds_alternative<double>(result));
    EXPECT_DOUBLE_EQ(std::get<double>(result), 6.0);
}

TEST(ArtifactScriptTest, ArrayFieldDefaultsAndSize) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
class Points : ArtifactBehaviour
{
    public Array points;
    float count()
    {
        return size(points);
    }
}

TEST(ArtifactScriptTest, ArrayPushAndIndexRead) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
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

TEST(ArtifactScriptTest, ArrayLiteralCreatesValues) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
class Points : ArtifactBehaviour
{
    float second()
    {
        Array values = [1.0, 2.5, 4.0];
        return values[1];
    }
}
)");
    ASSERT_TRUE(def.diagnostics.empty());
    ArtifactScriptEvaluator eval;
    ArtifactScriptSerializedFields fields;
    const auto result = eval.executeMethod(def, "second", {}, fields);
    ASSERT_TRUE(std::holds_alternative<double>(result));
    EXPECT_DOUBLE_EQ(std::get<double>(result), 2.5);
}
)");
    ASSERT_TRUE(def.diagnostics.empty());
    ArtifactScriptComponent component;
    component.setScriptClass("Points");
    component.applyDefaults(def);
    ArtifactScriptEvaluator eval;
    const auto result = eval.executeMethod(def, "first", {}, component.publicFields());
    ASSERT_TRUE(std::holds_alternative<double>(result));
    EXPECT_DOUBLE_EQ(std::get<double>(result), 9.25);
}
)");
    ASSERT_TRUE(def.diagnostics.empty());
    ArtifactScriptComponent component;
    component.setScriptClass("Points");
    component.applyDefaults(def);
    ASSERT_TRUE(std::holds_alternative<ArtifactScriptArrayPtr>(component.publicFields().at("points")));
    ArtifactScriptEvaluator eval;
    const auto result = eval.executeMethod(def, "count", {}, component.publicFields());
    ASSERT_TRUE(std::holds_alternative<std::int64_t>(result));
    EXPECT_EQ(std::get<std::int64_t>(result), 0);
}
)");
    ASSERT_TRUE(def.diagnostics.empty());
    ArtifactScriptEvaluator eval;
    ArtifactScriptSerializedFields fields;
    const auto result = eval.executeMethod(def, "add", {2.0, 3.0}, fields);
    ASSERT_TRUE(std::holds_alternative<double>(result));
    EXPECT_DOUBLE_EQ(std::get<double>(result), 5.0);
}
)");
    ASSERT_TRUE(def.diagnostics.empty());
    ArtifactScriptEvaluator eval;
    ArtifactScriptSerializedFields fields;
    fields["total"] = 0.0;
    ASSERT_TRUE(eval.execute(*def.rootClass.methods[0].body, {}, fields));
    EXPECT_DOUBLE_EQ(std::get<double>(fields["total"]), 8.0);
}
)");
    auto& body = *def.rootClass.methods[0].body;
    ArtifactScriptEvaluator eval;
    ArtifactScriptSerializedFields f1, f2;
    f1["val"] = 3.0; f2["val"] = 7.0;

    EXPECT_TRUE(eval.execute(body, {}, f1));
    EXPECT_DOUBLE_EQ(std::get<double>(f1["val"]), 1.0); // else branch

    EXPECT_TRUE(eval.execute(body, {}, f2));
    EXPECT_DOUBLE_EQ(std::get<double>(f2["val"]), 10.0); // then branch
}

TEST(ArtifactScriptTest, EvaluatorBuiltinFunctions) {
    ArtifactScriptParser parser;
    const auto def = parser.parse(R"(
class Test : ArtifactBehaviour
{
    public float v = 0.0;
    void OnUpdate(float dt)
    {
        v = clamp(v, 0.0, 100.0);
    }
}
)");
    ArtifactScriptEvaluator eval;
    ArtifactScriptSerializedFields f;
    f["v"] = 150.0;
    EXPECT_TRUE(eval.execute(*def.rootClass.methods[0].body, {}, f));
    EXPECT_DOUBLE_EQ(std::get<double>(f["v"]), 100.0);
}

TEST(ArtifactScriptTest, HotReloadMigratesFields) {
    const char* v1 = R"(
class Spin : ArtifactBehaviour
{
    public float speed = 90.0;
    public float radius = 50.0;
}

TEST(ArtifactScriptTest, FileAddEditAndReload) {
    const auto path = std::filesystem::temp_directory_path() / "artifact_script_hot_reload_test.artscript";
    const char* v1 = R"(
class Spin : ArtifactBehaviour
{
    public float speed = 90.0;
}
)";
    const char* v2 = R"(
class Spin : ArtifactBehaviour
{
    public float speed = 90.0;
    public float angle = 0.0;
}
)";

    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(output);
        output << v1;
    }

    ArtifactScriptHotReload hotReload;
    ASSERT_TRUE(hotReload.addFile(path.string()));
    ASSERT_NE(hotReload.definitionFor(path.string()), nullptr);
    ASSERT_NE(hotReload.fieldsFor(path.string()), nullptr);
    EXPECT_EQ(hotReload.definitionFor(path.string())->rootClass.name, "Spin");

    const auto firstWriteTime = std::filesystem::last_write_time(path);
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(output);
        output << v2;
    }
    std::filesystem::last_write_time(path, firstWriteTime + std::chrono::seconds(1));

    const auto changes = hotReload.reloadChanged();
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_TRUE(changes[0].result.success);
    EXPECT_EQ(changes[0].path, path.string());
    EXPECT_NE(hotReload.definitionFor(path.string())->rootClass.fields.end(),
              hotReload.definitionFor(path.string())->rootClass.fields.begin());

    hotReload.removeFile(path.string());
    EXPECT_EQ(hotReload.definitionFor(path.string()), nullptr);
    std::filesystem::remove(path);
}
)";
    const char* v2 = R"(
class Spin : ArtifactBehaviour
{
    public float speed = 90.0;
    public float angle = 0.0;
}
)";
    ArtifactScriptParser parser;
    auto def1 = parser.parse(v1);
    ArtifactScriptSerializedFields fields;
    fields["speed"] = 120.0; fields["radius"] = 30.0;

    ArtifactScriptHotReload hr;
    auto result = hr.reload(v2, &def1, &fields);
    EXPECT_TRUE(result.success);

    // speed preserved (same name + type)
    ASSERT_TRUE(result.migratedFields.find("speed") != result.migratedFields.end());
    EXPECT_DOUBLE_EQ(std::get<double>(result.migratedFields["speed"]), 120.0);

    // radius dropped (not in v2)
    EXPECT_TRUE(result.migratedFields.find("radius") == result.migratedFields.end());

    // angle added with default
    ASSERT_TRUE(result.migratedFields.find("angle") != result.migratedFields.end());
    EXPECT_DOUBLE_EQ(std::get<double>(result.migratedFields["angle"]), 0.0);
}
