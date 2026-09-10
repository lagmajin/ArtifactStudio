#include <gtest/gtest.h>

#include <string>
#include <variant>

import Script.ArtifactScript;

using namespace ArtifactCore;

namespace {

ArtifactScriptDefinition parseOk(std::string_view source) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(source);
    EXPECT_TRUE(definition.diagnostics.empty());
    return definition;
}

}  // namespace

TEST(ArtifactScriptObjectTest, NewInitializesFieldsAndThis) {
    auto definition = parseOk(R"(
class Point : ArtifactBehaviour
{
    public float x = 0.0;
    public float y = 0.0;
    void OnConstruct(float px, float py) {
        this.x = px;
        this.y = py;
    }
    float sum() { return this.x + this.y; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    EXPECT_TRUE(evaluator.execute(*definition.rootClass.methods[0].body, {3.0, 4.0}, fields));
}

TEST(ArtifactScriptObjectTest, FieldReadWriteAndMethodDispatch) {
    auto definition = parseOk(R"(
class Point : ArtifactBehaviour
{
    public float x = 0.0;
    public float y = 0.0;
    void OnConstruct(float px, float py) {
        this.x = px;
        this.y = py;
    }
    float sum() { return this.x + this.y; }
    void bump(float d) { this.x = this.x + d; }
}
)");
    ArtifactScriptParser parser;
    auto script = parser.parse(R"(
class Use : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate() {
        var p = new Point(1.0, 2.0);
        p.bump(5.0);
        total = p.sum();
    }
}
)");
    ASSERT_TRUE(script.diagnostics.empty());
    (void)definition;
    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["total"] = 0.0;
    EXPECT_TRUE(evaluator.executeMethod(script, "OnUpdate", {}, fields));
    EXPECT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    ASSERT_TRUE(std::holds_alternative<double>(fields.at("total")));
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("total")), 8.0);
}

TEST(ArtifactScriptObjectTest, InheritanceAndIsOperator) {
    auto definition = parseOk(R"(
class Base : ArtifactBehaviour
{
    public float x = 1.0;
    float who() { return 1.0; }
}
class Child : Base
{
    float who() { return 2.0; }
}
class Use : ArtifactBehaviour
{
    public float a = 0.0;
    public float b = 0.0;
    public bool flag = false;
    void OnUpdate() {
        var c = new Child();
        a = c.who();
        b = c.x;
        flag = c is Child;
        if (c is Base) { a = a + 10.0; }
        if (c is Point) { a = 999.0; }
    }
}
)");
    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["a"] = 0.0;
    fields["b"] = 0.0;
    fields["flag"] = false;
    EXPECT_TRUE(evaluator.executeMethod(definition, "OnUpdate", {}, fields));
    EXPECT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("a")), 12.0);
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("b")), 1.0);
    EXPECT_TRUE(std::get<bool>(fields.at("flag")));
}

TEST(ArtifactScriptObjectTest, MultiClassRegistry) {
    auto definition = parseOk(R"(
class First : ArtifactBehaviour
{
    public float v = 1.0;
}
class Second : ArtifactBehaviour
{
    public float w = 2.0;
}
)");
    EXPECT_EQ(definition.rootClass.name, "First");
    ASSERT_EQ(definition.classes.size(), 2u);
    EXPECT_EQ(definition.classes[0].name, "First");
    EXPECT_EQ(definition.classes[1].name, "Second");
}
