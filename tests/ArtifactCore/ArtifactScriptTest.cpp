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
