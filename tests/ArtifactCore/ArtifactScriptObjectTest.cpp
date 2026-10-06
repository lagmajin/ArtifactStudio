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
class Use : ArtifactBehaviour
{
    public float total = 0.0;
    float OnUpdate() {
        var p = new Point();
        return p.sum();
    }
}
class Point : ArtifactBehaviour
{
    public float x = 0.0;
    public float y = 0.0;
    void OnConstruct() {
        this.x = 3.0;
        this.y = 4.0;
    }
    float sum() { return this.x + this.y; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ASSERT_FALSE(definition.rootClass.methods.empty());
    ASSERT_EQ(definition.rootClass.methods[0].name, "OnUpdate");
    ASSERT_NE(definition.rootClass.methods[0].body, nullptr);
    ASSERT_EQ(definition.rootClass.methods[0].body->statements.size(), 2u);
    ASSERT_EQ(definition.classes.size(), 2u);
    ASSERT_EQ(definition.classes[1].methods[0].name, "OnConstruct");
    EXPECT_TRUE(definition.classes[1].methods[0].parameters.empty());
    ASSERT_NE(definition.classes[1].methods[0].body, nullptr);
    EXPECT_EQ(definition.classes[1].methods[0].body->statements.size(), 2u);
    ASSERT_TRUE(definition.classes[1].methods[0].body->statements[0]->fieldAssign);
    EXPECT_EQ(definition.classes[1].methods[0].body->statements[0]->declName, "this");
    EXPECT_EQ(definition.classes[1].methods[0].body->statements[0]->assignField, "x");

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    const auto result = evaluator.executeMethod(definition, "OnUpdate", {}, fields);
    ASSERT_TRUE(std::holds_alternative<double>(result)) << evaluator.getLastError();
    EXPECT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    EXPECT_DOUBLE_EQ(std::get<double>(result), 7.0);
}

TEST(ArtifactScriptObjectTest, FieldReadWriteAndMethodDispatch) {
    auto definition = parseOk(R"(
class Use : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate() {
        var p = new Point(1.0, 2.0);
        p.bump(5.0);
        total = p.sum();
    }
}
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
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["total"] = 0.0;
    const auto result = evaluator.executeMethod(definition, "OnUpdate", {}, fields);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(result));
    EXPECT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    ASSERT_TRUE(std::holds_alternative<double>(fields.at("total")));
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("total")), 8.0);
}

TEST(ArtifactScriptObjectTest, InheritanceAndIsOperator) {
    auto definition = parseOk(R"(
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
class Base : ArtifactBehaviour
{
    public float x = 1.0;
    float who() { return 1.0; }
}
class Child : Base
{
    float who() { return 2.0; }
}
)");
    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["a"] = 0.0;
    fields["b"] = 0.0;
    fields["flag"] = false;
    const auto result = evaluator.executeMethod(definition, "OnUpdate", {}, fields);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(result));
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
