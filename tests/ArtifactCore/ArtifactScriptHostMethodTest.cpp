#include <gtest/gtest.h>

#include <string>
#include <variant>

import Script.ArtifactScript;

using namespace ArtifactCore;

TEST(ArtifactScriptHostMethodTest, DispatchesRegisteredMethod) {
    ArtifactScriptHost host;
    host.registerMethod("Composition", "addLayer",
        [&host](const ArtifactScriptValue& self,
                std::span<const ArtifactScriptValue> args) -> ArtifactScriptValue {
            if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) {
                host.setLastError("addLayer expects a type name");
                return {};
            }
            return ArtifactScriptRef{"layer:" + std::get<std::string>(args[0])};
        });

    EXPECT_TRUE(host.hasMethod("Composition", "addLayer"));
    EXPECT_FALSE(host.hasMethod("Composition", "missing"));

    ArtifactScriptValue self{ArtifactScriptRef{"comp-main"}};
    ArtifactScriptValue result;
    EXPECT_TRUE(host.callMethod("Composition", "addLayer", self, {std::string("Shape")}, result));
    ASSERT_TRUE(std::holds_alternative<ArtifactScriptRef>(result));
    EXPECT_EQ(std::get<ArtifactScriptRef>(result).id, "layer:Shape");
    EXPECT_FALSE(host.callMethod("Composition", "missing", self, {}, result));
}

TEST(ArtifactScriptHostMethodTest, ScriptEntryPointCallsHostMethod) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Use : ArtifactBehaviour
{
    public float created = 0.0;
    void OnUpdate() {
        var comp = getLayer("Main");
        var layer = comp.addLayer("Shape");
        created = 1.0;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptHost& host = ArtifactScriptHost::global();
    host.registerMethod("ObjectRef", "addLayer",
        [&host](const ArtifactScriptValue& self,
                std::span<const ArtifactScriptValue> args) -> ArtifactScriptValue {
            if (!std::holds_alternative<ArtifactScriptRef>(self) ||
                args.size() != 1 || !std::holds_alternative<std::string>(args[0])) {
                host.setLastError("addLayer expects a layer and a type name");
                return {};
            }
            return ArtifactScriptRef{
                std::get<ArtifactScriptRef>(self).id + ":" + std::get<std::string>(args[0])};
        });

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["created"] = 0.0;
    EXPECT_TRUE(evaluator.executeMethod(definition, "OnUpdate", {}, fields));
    EXPECT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    EXPECT_DOUBLE_EQ(std::get<double>(fields.at("created")), 1.0);
}

TEST(ArtifactScriptHostMethodTest, UnknownMethodIsDiagnostic) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Use : ArtifactBehaviour
{
    void OnUpdate() {
        var comp = getLayer("Main");
        comp.missing();
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    EXPECT_FALSE(evaluator.executeMethod(definition, "OnUpdate", {}, fields));
    EXPECT_NE(evaluator.getLastError().find("unknown method"), std::string::npos);
}
