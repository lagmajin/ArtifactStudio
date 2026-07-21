#include <gtest/gtest.h>
#include <variant>

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

