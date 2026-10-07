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

TEST(ArtifactScriptObjectTest, SameObjectNestedMethodMutationIsVisibleAfterRead) {
    auto definition = parseOk(R"(
class Use : ArtifactBehaviour
{
    public ObjectRef counter;
    public float result = 0.0;
    void OnCreate() { counter = new Counter(); }
    void OnUpdate() { result = counter.outer(); }
}
class Counter : ArtifactBehaviour
{
    public float value = 1.0;
    float outer()
    {
        var before = this.value;
        this.bump();
        return before + this.value;
    }
    void bump() { this.value = this.value + 1.0; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();

    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 3.0);
    const auto counter = std::get<ArtifactScriptObjectInstancePtr>(
        instance.fields().at("counter"));
    ASSERT_TRUE(counter);
    EXPECT_DOUBLE_EQ(std::get<double>(counter->fields.at("value")), 2.0);
}

TEST(ArtifactScriptObjectTest, FailedMethodRollsBackInstanceFieldWrites) {
    auto definition = parseOk(R"(
class Use : ArtifactBehaviour
{
    void OnCreate() { counter = new Counter(); }
    void OnUpdate() { counter.fail(); }
}
class Counter : ArtifactBehaviour
{
    public float value = 3.0;
    void fail()
    {
        this.value = 9.0;
        var invalid = 1.0 / 0.0;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    EXPECT_FALSE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_NE(instance.lastError().find("div0"), std::string::npos);

    const auto counter = std::get<ArtifactScriptObjectInstancePtr>(
        instance.fields().at("counter"));
    ASSERT_TRUE(counter);
    EXPECT_DOUBLE_EQ(std::get<double>(counter->fields.at("value")), 3.0);
}

TEST(ArtifactScriptObjectTest, SuccessfulMethodCommitsFieldsBeyondInlineOverlay) {
    auto definition = parseOk(R"(
class Use : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate()
    {
        var counter = new Counter();
        counter.update();
        total = counter.sum();
    }
}
class Counter : ArtifactBehaviour
{
    public float a = 0.0;
    public float b = 0.0;
    public float c = 0.0;
    public float d = 0.0;
    public float e = 0.0;
    public float f = 0.0;
    public float g = 0.0;
    public float h = 0.0;
    public float i = 0.0;
    void update()
    {
        this.a = 1.0;
        this.b = 2.0;
        this.c = 3.0;
        this.d = 4.0;
        this.e = 5.0;
        this.f = 6.0;
        this.g = 7.0;
        this.h = 8.0;
        this.i = 9.0;
    }
    float sum()
    {
        return this.a + this.b + this.c + this.d + this.e +
               this.f + this.g + this.h + this.i;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 45.0);
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

TEST(ArtifactScriptObjectTest,
     DeepInheritanceObjectConstructionPreservesAllBaseFields) {
    std::string source = R"(
class Use : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate()
    {
        var value = new Level32();
        total = )";
    for (int i = 0; i < 33; ++i) {
        if (i != 0) source += " + ";
        source += "value.value" + std::to_string(i);
    }
    source += ";\n    }\n}\n";
    for (int i = 0; i < 33; ++i) {
        source += "class Level" + std::to_string(i) + " : ";
        source += i == 0 ? "ArtifactBehaviour" : "Level" + std::to_string(i - 1);
        source += "\n{\n    public float value" + std::to_string(i) + " = " +
                  std::to_string(i + 1) + ".0;\n}\n";
    }

    auto definition = parseOk(source);
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 561.0);
}

TEST(ArtifactScriptObjectTest, ObjectMethodCallSiteCacheTracksRuntimeClass) {
    auto definition = parseOk(R"(
class Use : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate()
    {
        var baseItem = new Base();
        var childItem = new Child();
        for (int index = 0; index < 16; index += 1) {
            if (index % 2 == 0) { target = childItem; }
            else { target = baseItem; }
            total = total + target.who();
        }
    }
}
class Base : ArtifactBehaviour
{
    float who() { return 1.0; }
}
class Child : Base
{
    float who() { return 2.0; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("total")), 24.0);
}

TEST(ArtifactScriptObjectTest, MutableDefinitionInvalidatesPersistentMethodCallCache) {
    auto definition = parseOk(R"(
class Use : ArtifactBehaviour
{
    public float result = 0.0;
    float first() { return 1.0; }
    float second() { return 2.0; }
    void OnUpdate() { result = first(); }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 1.0);

    auto& mutableDefinition = instance.definition();
    auto& call = mutableDefinition.rootClass.methods[2]
                     .body->statements[0]->assignValue;
    ASSERT_TRUE(call);
    ASSERT_EQ(call->kind, ArtifactScriptExpr::Kind::Call);
    call->callName = "second";

    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 2.0);
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

TEST(ArtifactScriptObjectTest, MutableDefinitionDisablesPersistentClassLookupCache) {
    auto definition = parseOk(R"(
class Use : ArtifactBehaviour
{
    public ObjectRef target;
    void OnUpdate() { target = new First(); }
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

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    ASSERT_TRUE(std::holds_alternative<ArtifactScriptObjectInstancePtr>(
        instance.fields().at("target")));
    EXPECT_EQ(std::get<ArtifactScriptObjectInstancePtr>(
        instance.fields().at("target"))->className, "First");

    auto& mutableDefinition = instance.definition();
    auto& expression = mutableDefinition.rootClass.methods[0]
                           .body->statements[0]->assignValue;
    ASSERT_TRUE(expression);
    ASSERT_EQ(expression->kind, ArtifactScriptExpr::Kind::New);
    expression->newClassName = "Second";

    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    ASSERT_TRUE(std::holds_alternative<ArtifactScriptObjectInstancePtr>(
        instance.fields().at("target")));
    EXPECT_EQ(std::get<ArtifactScriptObjectInstancePtr>(
        instance.fields().at("target"))->className, "Second");

    mutableDefinition.rootClass.methods[0].body.reset();
    EXPECT_FALSE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
}
