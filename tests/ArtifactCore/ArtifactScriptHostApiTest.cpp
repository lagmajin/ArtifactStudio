#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

import Script.ArtifactScript;

using namespace ArtifactCore;

TEST(ArtifactScriptHostApiTest, InstallsCompositionCallbacks) {
    ArtifactScriptHost host;
    ArtifactScriptCompositionApi api;
    api.getLayer = [](std::string_view name) -> ArtifactScriptValue {
        return ArtifactScriptRef{std::string(name) + "-id"};
    };
    api.getLayerCount = [] { return std::int64_t(3); };
    api.getTime = [] { return 12.5; };
    api.getProperty = [](const ArtifactScriptValue&, std::string_view path) -> ArtifactScriptValue {
        return path == "opacity" ? ArtifactScriptValue(0.75) : ArtifactScriptValue{};
    };
    api.setProperty = [](const ArtifactScriptValue&, std::string_view path,
                         const ArtifactScriptValue& value) {
        return path == "opacity" && std::holds_alternative<double>(value);
    };

    host.installCompositionApi(api);

    ArtifactScriptValue result;
    EXPECT_TRUE(host.callFunction("getLayer", {std::string("Main")}, result));
    ASSERT_TRUE(std::holds_alternative<ArtifactScriptRef>(result));
    EXPECT_EQ(std::get<ArtifactScriptRef>(result).id, "Main-id");

    EXPECT_TRUE(host.callFunction("getLayerCount", {}, result));
    EXPECT_EQ(std::get<std::int64_t>(result), 3);
    EXPECT_TRUE(host.callFunction("getTime", {}, result));
    EXPECT_DOUBLE_EQ(std::get<double>(result), 12.5);
    EXPECT_TRUE(host.callFunction("getProperty", {ArtifactScriptRef{"Main-id"}, std::string("opacity")}, result));
    EXPECT_DOUBLE_EQ(std::get<double>(result), 0.75);
    EXPECT_TRUE(host.callFunction("setProperty", {ArtifactScriptRef{"Main-id"}, std::string("opacity"), 0.5}, result));
    EXPECT_TRUE(std::get<bool>(result));
}

TEST(ArtifactScriptHostApiTest, ReportsRejectedSetProperty) {
    ArtifactScriptHost host;
    ArtifactScriptCompositionApi api;
    api.setProperty = [](const ArtifactScriptValue&, std::string_view,
                         const ArtifactScriptValue&) { return false; };
    host.installCompositionApi(api);

    ArtifactScriptValue result;
    EXPECT_TRUE(host.callFunction("setProperty", {ArtifactScriptRef{"missing"}, std::string("x"), 1.0}, result));
    EXPECT_FALSE(std::get<bool>(result));
    EXPECT_EQ(host.lastError(), "setProperty rejected target or path");
}

TEST(ArtifactScriptHostApiTest, InstallsPropertyAndKeyframeApi) {
    ArtifactScriptHost host;
    ArtifactScriptCompositionApi api;
    api.hasProperty = [](const ArtifactScriptValue&, std::string_view path) {
        return path == "layer.opacity";
    };
    api.propertyNames = [](const ArtifactScriptValue&) {
        return std::vector<std::string>{"layer.opacity", "transform.position.x"};
    };
    api.isAnimatable = [](const ArtifactScriptValue&, std::string_view path) {
        return path == "layer.opacity";
    };
    api.hasKeyframes = [](const ArtifactScriptValue&, std::string_view path) {
        return path == "layer.opacity";
    };
    api.keyframeCount = [](const ArtifactScriptValue&,
                           std::string_view path) -> std::int64_t {
        return path == "layer.opacity" ? 2 : 0;
    };
    api.hasKeyframeAt = [](const ArtifactScriptValue&, std::string_view path,
                           std::int64_t frame) {
        return path == "layer.opacity" && frame == 12;
    };
    api.valueAtFrame = [](const ArtifactScriptValue&, std::string_view path,
                          std::int64_t frame) {
        return path == "layer.opacity" && frame == 12 ? ArtifactScriptValue(0.5)
                                                      : ArtifactScriptValue{};
    };
    std::vector<std::int64_t> addedFrames;
    std::vector<std::string> addedInterp;
    std::vector<std::int64_t> removedFrames;
    bool cleared = false;
    api.addKeyframe = [&](const ArtifactScriptValue&, std::string_view path,
                          std::int64_t frame, const ArtifactScriptValue& value,
                          std::string_view interp) {
        if (path != "layer.opacity" || !std::holds_alternative<double>(value)) return false;
        addedFrames.push_back(frame);
        addedInterp.emplace_back(interp);
        return true;
    };
    api.removeKeyframe = [&](const ArtifactScriptValue&, std::string_view path,
                             std::int64_t frame) {
        if (path != "layer.opacity") return false;
        removedFrames.push_back(frame);
        return true;
    };
    api.clearKeyframes = [&](const ArtifactScriptValue&, std::string_view path) {
        cleared = path == "layer.opacity";
        return path == "layer.opacity";
    };
    api.keyframes = [](const ArtifactScriptValue&, std::string_view path) {
        std::vector<ArtifactScriptCompositionApi::KeyframeRow> rows;
        if (path != "layer.opacity") return rows;
        ArtifactScriptCompositionApi::KeyframeRow first;
        first.frame = 0;
        first.value = 0.0;
        first.interp = "linear";
        rows.push_back(std::move(first));
        ArtifactScriptCompositionApi::KeyframeRow second;
        second.frame = 12;
        second.value = 0.5;
        second.interp = "easein";
        rows.push_back(std::move(second));
        return rows;
    };
    host.installCompositionApi(api);

    const ArtifactScriptValue target{ArtifactScriptRef{"Main"}};
    ArtifactScriptValue result;

    EXPECT_TRUE(host.callFunction("hasProperty", {target, std::string("layer.opacity")}, result));
    EXPECT_TRUE(std::get<bool>(result));
    EXPECT_TRUE(host.callFunction("hasProperty", {target, std::string("missing")}, result));
    EXPECT_FALSE(std::get<bool>(result));

    EXPECT_TRUE(host.callFunction("getPropertyNames", {target}, result));
    ASSERT_TRUE(std::holds_alternative<ArtifactScriptArrayPtr>(result));
    const auto names = std::get<ArtifactScriptArrayPtr>(result);
    ASSERT_TRUE(names);
    ASSERT_EQ(names->values.size(), 2u);
    EXPECT_EQ(std::get<std::string>(names->values[0]), "layer.opacity");

    EXPECT_TRUE(host.callFunction("isAnimatable", {target, std::string("layer.opacity")}, result));
    EXPECT_TRUE(std::get<bool>(result));
    EXPECT_TRUE(host.callFunction("hasKeyframes", {target, std::string("layer.opacity")}, result));
    EXPECT_TRUE(std::get<bool>(result));
    EXPECT_TRUE(host.callFunction("getKeyframeCount", {target, std::string("layer.opacity")}, result));
    EXPECT_EQ(std::get<std::int64_t>(result), 2);

    EXPECT_TRUE(host.callFunction(
        "hasKeyframeAt", {target, std::string("layer.opacity"), std::int64_t(12)}, result));
    EXPECT_TRUE(std::get<bool>(result));
    EXPECT_TRUE(host.callFunction(
        "hasKeyframeAt", {target, std::string("layer.opacity"), std::int64_t(13)}, result));
    EXPECT_FALSE(std::get<bool>(result));

    EXPECT_TRUE(host.callFunction(
        "getValueAtFrame", {target, std::string("layer.opacity"), std::int64_t(12)}, result));
    EXPECT_DOUBLE_EQ(std::get<double>(result), 0.5);

    EXPECT_TRUE(host.callFunction(
        "addKeyframe",
        {target, std::string("layer.opacity"), std::int64_t(5), 0.25,
         std::string("easein")},
        result));
    EXPECT_TRUE(std::get<bool>(result));
    ASSERT_EQ(addedFrames.size(), 1u);
    EXPECT_EQ(addedFrames[0], 5);
    ASSERT_EQ(addedInterp.size(), 1u);
    EXPECT_EQ(addedInterp[0], "easein");

    EXPECT_TRUE(host.callFunction(
        "removeKeyframe", {target, std::string("layer.opacity"), std::int64_t(5)}, result));
    EXPECT_TRUE(std::get<bool>(result));
    ASSERT_EQ(removedFrames.size(), 1u);
    EXPECT_EQ(removedFrames[0], 5);

    EXPECT_TRUE(host.callFunction("clearKeyframes", {target, std::string("layer.opacity")}, result));
    EXPECT_TRUE(std::get<bool>(result));
    EXPECT_TRUE(cleared);

    EXPECT_TRUE(host.callFunction("getKeyframes", {target, std::string("layer.opacity")}, result));
    ASSERT_TRUE(std::holds_alternative<ArtifactScriptArrayPtr>(result));
    const auto rows = std::get<ArtifactScriptArrayPtr>(result);
    ASSERT_TRUE(rows);
    ASSERT_EQ(rows->values.size(), 2u);
    ASSERT_TRUE(std::holds_alternative<ArtifactScriptObjectInstancePtr>(rows->values[1]));
    const auto row = std::get<ArtifactScriptObjectInstancePtr>(rows->values[1]);
    ASSERT_TRUE(row);
    EXPECT_EQ(row->className, "Keyframe");
    EXPECT_EQ(std::get<std::int64_t>(row->fields.at("frame")), 12);
    EXPECT_DOUBLE_EQ(std::get<double>(row->fields.at("value")), 0.5);
    EXPECT_EQ(std::get<std::string>(row->fields.at("interp")), "easein");
}

TEST(ArtifactScriptHostApiTest, RejectsMalformedKeyframeCalls) {
    ArtifactScriptHost host;
    ArtifactScriptCompositionApi api;
    api.addKeyframe = [](const ArtifactScriptValue&, std::string_view, std::int64_t,
                         const ArtifactScriptValue&, std::string_view) { return true; };
    host.installCompositionApi(api);

    ArtifactScriptValue result;
    // Missing the path argument.
    EXPECT_TRUE(host.callFunction("addKeyframe", {ArtifactScriptRef{"Main"}}, result));
    EXPECT_FALSE(std::get<bool>(result));
    EXPECT_EQ(host.lastError(), "addKeyframe expects layer, path, frame[, value[, interp]]");
    // Frame is not numeric.
    EXPECT_TRUE(host.callFunction(
        "addKeyframe",
        {ArtifactScriptRef{"Main"}, std::string("layer.opacity"), std::string("nope")},
        result));
    EXPECT_FALSE(std::get<bool>(result));
    EXPECT_EQ(host.lastError(), "addKeyframe expects an integer frame");
}

TEST(ArtifactScriptHostApiTest, ScriptEntryPointCallsKeyframeApi) {
    ArtifactScriptParser parser;
    const auto definition = parser.parse(R"(
class Use : ArtifactBehaviour
{
    public bool keyed = false;
    public bool listed = false;
    void OnUpdate() {
        keyed = addKeyframe("Main", "layer.opacity", 5, 0.5, "easein");
        listed = hasKeyframes("Main", "layer.opacity");
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptCompositionApi api;
    api.addKeyframe = [](const ArtifactScriptValue&, std::string_view path,
                         std::int64_t frame, const ArtifactScriptValue& value,
                         std::string_view interp) {
        return path == "layer.opacity" && frame == 5 &&
               std::holds_alternative<double>(value) && interp == "easein";
    };
    api.hasKeyframes = [](const ArtifactScriptValue&, std::string_view path) {
        return path == "layer.opacity";
    };
    ArtifactScriptHost::global().installCompositionApi(api);

    ArtifactScriptEvaluator evaluator;
    ArtifactScriptSerializedFields fields;
    fields["keyed"] = false;
    fields["listed"] = false;
    EXPECT_TRUE(evaluator.executeMethod(definition, "OnUpdate", {}, fields));
    EXPECT_FALSE(evaluator.hasError()) << evaluator.getLastError();
    EXPECT_TRUE(std::get<bool>(fields.at("keyed")));
    EXPECT_TRUE(std::get<bool>(fields.at("listed")));
}
