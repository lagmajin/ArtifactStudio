#include <gtest/gtest.h>

#include <string>
#include <variant>

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
