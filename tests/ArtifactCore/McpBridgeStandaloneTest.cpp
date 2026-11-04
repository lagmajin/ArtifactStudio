// 統合MCPサーバー（ArtifactCore/include/AI/McpBridge.ixx）の契約テスト。
//
// Artifact.exe を起動せずに JSON-RPC framing と tool surface だけを検証する。
// 対象は protocol conformance と tools/list までで、個別の debug.* handler の
// runtime 挙動（app状態・property・trace 実データ）はここで扱わない。
//
// 登録は tests/ArtifactCore/CMakeLists.txt の ArtifactCoreMcpBridgeStandaloneTest。

#include <gtest/gtest.h>

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

import Core.AI.Context;
import Core.AI.McpBridge;

using namespace ArtifactCore;

namespace {

QJsonObject makeRequest(const QString& method,
                        const QJsonObject& params = QJsonObject(),
                        const QJsonValue& id = QJsonValue(1))
{
    QJsonObject message;
    message[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    if (!id.isUndefined()) {
        message[QStringLiteral("id")] = id;
    }
    message[QStringLiteral("method")] = method;
    message[QStringLiteral("params")] = params;
    return message;
}

int contentLengthOf(const QByteArray& frame)
{
    const int headerEnd = frame.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return -1;
    }
    const QByteArray header = frame.left(headerEnd);
    const int colon = header.indexOf(':');
    if (colon < 0) {
        return -1;
    }
    bool ok = false;
    const int value = header.mid(colon + 1).trimmed().toInt(&ok);
    return ok ? value : -1;
}

QByteArray bodyOf(const QByteArray& frame)
{
    const int headerEnd = frame.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return QByteArray();
    }
    return frame.mid(headerEnd + 4);
}

QJsonObject decodeSingleFrame(const QByteArray& frame)
{
    const QList<QJsonObject> frames = McpBridge::decodeFrames(frame);
    return frames.isEmpty() ? QJsonObject() : frames.last();
}

QJsonArray toolsOf(const QJsonObject& response)
{
    return response.value(QStringLiteral("result"))
        .toObject()
        .value(QStringLiteral("tools"))
        .toArray();
}

QStringList toolNamesOf(const QJsonArray& tools)
{
    QStringList names;
    names.reserve(tools.size());
    for (const QJsonValue& value : tools) {
        names.push_back(value.toObject().value(QStringLiteral("name")).toString());
    }
    return names;
}

// McpBridge::capabilityList() が公開する debug.* tool の全一覧。
// tool の追加は許容し、削除・改名だけがこの契約の違反として検出される。
const QStringList& expectedDebugToolNames()
{
    static const QStringList names = {
        QStringLiteral("debug.log"),
        QStringLiteral("debug.logCategory"),
        QStringLiteral("debug.filterLog"),
        QStringLiteral("debug.getTools"),
        QStringLiteral("debug.containers"),
        QStringLiteral("debug.containers.annotate"),
        QStringLiteral("debug.state"),
        QStringLiteral("debug.pause"),
        QStringLiteral("debug.resume"),
        QStringLiteral("debug.getSelection"),
        QStringLiteral("debug.getViewport"),
        QStringLiteral("debug.compositionState"),
        QStringLiteral("debug.layerState"),
        QStringLiteral("debug.listCompositions"),
        QStringLiteral("debug.listLayers"),
        QStringLiteral("debug.renderQueue"),
        QStringLiteral("debug.getFarm"),
        QStringLiteral("debug.stepForward"),
        QStringLiteral("debug.stepToFrame"),
        QStringLiteral("debug.getColorPipeline"),
        QStringLiteral("debug.getMaskPaths"),
        QStringLiteral("debug.gpuMemory"),
        QStringLiteral("debug.getRig"),
        QStringLiteral("debug.addDataBreakpoint"),
        QStringLiteral("debug.removeDataBreakpoint"),
        QStringLiteral("debug.listDataBreakpoints"),
        QStringLiteral("debug.addWatchpoint"),
        QStringLiteral("debug.removeWatchpoint"),
        QStringLiteral("debug.listWatchpoints"),
        QStringLiteral("debug.trace"),
        QStringLiteral("debug.flow"),
        QStringLiteral("debug.rootCause"),
        QStringLiteral("debug.diff"),
        QStringLiteral("debug.bisect"),
        QStringLiteral("debug.recipe"),
        QStringLiteral("debug.listProperties"),
        QStringLiteral("debug.getProperty"),
        QStringLiteral("debug.setProperty"),
        QStringLiteral("debug.evaluate"),
        QStringLiteral("debug.query"),
        QStringLiteral("debug.patch.begin"),
        QStringLiteral("debug.patch.apply"),
        QStringLiteral("debug.patch.rollback"),
        QStringLiteral("debug.patch.commit"),
        QStringLiteral("debug.performance"),
        QStringLiteral("debug.memory.list"),
        QStringLiteral("debug.memory.dump"),
        QStringLiteral("debug.memory.graph"),
        QStringLiteral("debug.memory.leaks"),
        QStringLiteral("debug.predict.risks"),
        QStringLiteral("debug.predict.autoWatch"),
        QStringLiteral("debug.stress.run"),
        QStringLiteral("debug.stress.result"),
        QStringLiteral("debug.renderGraph"),
        QStringLiteral("debug.getTimeline"),
        QStringLiteral("debug.regression.capture"),
        QStringLiteral("debug.regression.compare"),
        QStringLiteral("debug.regression.detect"),
    };
    return names;
}

} // namespace

// ── framing ────────────────────────────────────────────────────────────────

TEST(McpBridgeStandaloneTest, EncodeFrameDeclaresContentLengthInBytes)
{
    const QJsonObject message{
        {QStringLiteral("description"), QStringLiteral("日本語の説明")}
    };

    const QByteArray frame = McpBridge::encodeFrame(message);

    EXPECT_TRUE(frame.startsWith("Content-Length: "));
    const int headerEnd = frame.indexOf("\r\n\r\n");
    ASSERT_GE(headerEnd, 0);

    const QByteArray body = frame.mid(headerEnd + 4);
    ASSERT_GT(contentLengthOf(frame), 0);
    // Content-Length は QString の文字数ではなく UTF-8 の byte 数でなければならない。
    EXPECT_EQ(contentLengthOf(frame), body.size());
    EXPECT_NE(contentLengthOf(frame),
              static_cast<int>(message.value(QStringLiteral("description"))
                                   .toString().toUtf8().size()));
    EXPECT_EQ(body.size(), QJsonDocument(message).toJson(QJsonDocument::Compact).size());
}

TEST(McpBridgeStandaloneTest, DecodeFramesReadsConcatenatedFrames)
{
    const QJsonObject first{{QStringLiteral("id"), 1}};
    const QJsonObject second{{QStringLiteral("id"), 2}};
    const QByteArray buffer = McpBridge::encodeFrame(first) +
                              McpBridge::encodeFrame(second);

    const QList<QJsonObject> frames = McpBridge::decodeFrames(buffer);

    ASSERT_EQ(frames.size(), 2);
    EXPECT_EQ(frames.at(0).value(QStringLiteral("id")).toInt(), 1);
    EXPECT_EQ(frames.at(1).value(QStringLiteral("id")).toInt(), 2);
}

TEST(McpBridgeStandaloneTest, DecodeFramesIgnoresIncompleteFrame)
{
    const QByteArray frame = McpBridge::encodeFrame(QJsonObject{{QStringLiteral("id"), 1}});
    const QByteArray truncated = frame.left(frame.size() - 3);

    EXPECT_TRUE(McpBridge::decodeFrames(truncated).isEmpty());
}

TEST(McpBridgeStandaloneTest, DecodeFramesIgnoresFrameWithoutContentLength)
{
    const QByteArray frame = QByteArrayLiteral("X-Other: 1\r\n\r\n{\"id\":1}");

    EXPECT_TRUE(McpBridge::decodeFrames(frame).isEmpty());
}

TEST(McpBridgeStandaloneTest, TryPopFrameConsumesOneFrameAndKeepsRemainder)
{
    const QByteArray first = McpBridge::encodeFrame(QJsonObject{{QStringLiteral("id"), 1}});
    const QByteArray second = McpBridge::encodeFrame(QJsonObject{{QStringLiteral("id"), 2}});
    QByteArray buffer = first + second;

    QJsonObject message;
    ASSERT_TRUE(McpBridge::tryPopFrame(&buffer, &message));
    EXPECT_EQ(message.value(QStringLiteral("id")).toInt(), 1);
    EXPECT_EQ(buffer.size(), second.size());

    ASSERT_TRUE(McpBridge::tryPopFrame(&buffer, &message));
    EXPECT_EQ(message.value(QStringLiteral("id")).toInt(), 2);
    EXPECT_TRUE(buffer.isEmpty());

    EXPECT_FALSE(McpBridge::tryPopFrame(&buffer, &message));
}

TEST(McpBridgeStandaloneTest, TryPopFrameLeavesIncompleteFrameUntouched)
{
    const QByteArray frame = McpBridge::encodeFrame(QJsonObject{{QStringLiteral("id"), 1}});
    const QByteArray truncated = frame.left(frame.size() - 5);
    QByteArray buffer = truncated;

    QJsonObject message;
    EXPECT_FALSE(McpBridge::tryPopFrame(&buffer, &message));
    EXPECT_EQ(buffer, truncated);
}

TEST(McpBridgeStandaloneTest, FrameRoundTripPreservesRequestPayload)
{
    const QJsonObject payload{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), 7},
        {QStringLiteral("method"), QStringLiteral("tools/list")},
        {QStringLiteral("params"), QJsonObject{}}
    };

    const QJsonObject decoded = decodeSingleFrame(McpBridge::encodeFrame(payload));

    EXPECT_EQ(decoded, payload);
}

TEST(McpBridgeStandaloneTest, HandleFrameRejectsMalformedJson)
{
    const QByteArray frame = McpBridge::handleFrame(QByteArrayLiteral("not-json"));

    const QJsonObject response = decodeSingleFrame(frame);
    ASSERT_TRUE(response.contains(QStringLiteral("error")));
    EXPECT_EQ(response.value(QStringLiteral("error"))
                  .toObject()
                  .value(QStringLiteral("code"))
                  .toInt(),
              -32700);
    EXPECT_EQ(response.value(QStringLiteral("jsonrpc")).toString(), QStringLiteral("2.0"));
}

TEST(McpBridgeStandaloneTest, HandleFrameWrapsResponseInContentLengthFrame)
{
    const QByteArray frame = McpBridge::handleFrame(
        QJsonDocument(makeRequest(QStringLiteral("ping"))).toJson(QJsonDocument::Compact));

    EXPECT_TRUE(frame.startsWith("Content-Length: "));
    const QJsonObject response = decodeSingleFrame(frame);
    ASSERT_TRUE(response.contains(QStringLiteral("result")));
    EXPECT_TRUE(response.value(QStringLiteral("result"))
                    .toObject()
                    .value(QStringLiteral("pong"))
                    .toBool());
}

// ── JSON-RPC protocol ─────────────────────────────────────────────────────

TEST(McpBridgeStandaloneTest, InitializeAdvertisesServerInfoAndToolCapability)
{
    const QJsonObject response = McpBridge::handleRequest(
        makeRequest(QStringLiteral("initialize")));

    ASSERT_TRUE(response.contains(QStringLiteral("result")));
    const QJsonObject result = response.value(QStringLiteral("result")).toObject();
    EXPECT_FALSE(result.value(QStringLiteral("protocolVersion")).toString().isEmpty());
    EXPECT_EQ(result.value(QStringLiteral("serverInfo"))
                  .toObject()
                  .value(QStringLiteral("name"))
                  .toString(),
              QStringLiteral("ArtifactStudio"));
    EXPECT_FALSE(result.value(QStringLiteral("serverInfo"))
                     .toObject()
                     .value(QStringLiteral("version"))
                     .toString()
                     .isEmpty());
    const QJsonObject toolCapability = result.value(QStringLiteral("capabilities"))
                                           .toObject()
                                           .value(QStringLiteral("tools"))
                                           .toObject();
    ASSERT_TRUE(toolCapability.contains(QStringLiteral("listChanged")));
    EXPECT_FALSE(toolCapability.value(QStringLiteral("listChanged")).toBool());
    EXPECT_TRUE(result.contains(QStringLiteral("context")));
}

TEST(McpBridgeStandaloneTest, PingRespondsWithPong)
{
    const QJsonObject response =
        McpBridge::handleRequest(makeRequest(QStringLiteral("ping")));

    ASSERT_TRUE(response.contains(QStringLiteral("result")));
    const QJsonObject result = response.value(QStringLiteral("result")).toObject();
    EXPECT_TRUE(result.value(QStringLiteral("pong")).toBool());
    EXPECT_FALSE(result.value(QStringLiteral("timestamp")).toString().isEmpty());
}

TEST(McpBridgeStandaloneTest, UnknownMethodReturnsMethodNotFound)
{
    const QJsonObject response = McpBridge::handleRequest(
        makeRequest(QStringLiteral("does/not/exist")));

    ASSERT_TRUE(response.contains(QStringLiteral("error")));
    const QJsonObject error = response.value(QStringLiteral("error")).toObject();
    EXPECT_EQ(error.value(QStringLiteral("code")).toInt(), -32601);
    EXPECT_TRUE(error.value(QStringLiteral("message"))
                    .toString()
                    .contains(QStringLiteral("does/not/exist")));
}

TEST(McpBridgeStandaloneTest, MissingMethodReturnsInvalidRequest)
{
    QJsonObject message;
    message[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    message[QStringLiteral("id")] = 1;

    const QJsonObject response = McpBridge::handleRequest(message);

    ASSERT_TRUE(response.contains(QStringLiteral("error")));
    EXPECT_EQ(response.value(QStringLiteral("error"))
                  .toObject()
                  .value(QStringLiteral("code"))
                  .toInt(),
              -32600);
}

TEST(McpBridgeStandaloneTest, ResponseEchoesRequestIdAndOmitsNullId)
{
    const QJsonObject numeric =
        McpBridge::handleRequest(makeRequest(QStringLiteral("ping"), QJsonObject(), 42));
    EXPECT_EQ(numeric.value(QStringLiteral("id")).toInt(), 42);

    const QJsonObject text = McpBridge::handleRequest(
        makeRequest(QStringLiteral("ping"), QJsonObject(), QJsonValue(QStringLiteral("abc"))));
    EXPECT_EQ(text.value(QStringLiteral("id")).toString(), QStringLiteral("abc"));

    const QJsonObject nullId = McpBridge::handleRequest(
        makeRequest(QStringLiteral("ping"), QJsonObject(), QJsonValue(QJsonValue::Null)));
    EXPECT_FALSE(nullId.contains(QStringLiteral("id")));

    for (const QJsonObject& response : {numeric, text, nullId}) {
        EXPECT_EQ(response.value(QStringLiteral("jsonrpc")).toString(),
                  QStringLiteral("2.0"));
    }
}

TEST(McpBridgeStandaloneTest, RequestContextOverridesResponseContext)
{
    const QJsonObject params{
        {QStringLiteral("context"), QJsonObject{
            {QStringLiteral("projectSummary"), QStringLiteral("contract-summary")},
            {QStringLiteral("activeComposition"), QStringLiteral("composition-1")}
        }}
    };

    const QJsonObject response = McpBridge::handleRequest(
        makeRequest(QStringLiteral("tools/list"), params));

    const QJsonObject context = response.value(QStringLiteral("result"))
                                    .toObject()
                                    .value(QStringLiteral("context"))
                                    .toObject();
    EXPECT_EQ(context.value(QStringLiteral("projectSummary")).toString(),
              QStringLiteral("contract-summary"));
    EXPECT_EQ(context.value(QStringLiteral("activeComposition")).toString(),
              QStringLiteral("composition-1"));
}

// ── tools/list surface ────────────────────────────────────────────────────

TEST(McpBridgeStandaloneTest, ToolsListExposesEveryDebugTool)
{
    const QJsonObject response = McpBridge::handleRequest(
        makeRequest(QStringLiteral("tools/list")));

    const QJsonArray tools = toolsOf(response);
    ASSERT_FALSE(tools.isEmpty());

    const QStringList names = toolNamesOf(tools);
    for (const QString& expected : expectedDebugToolNames()) {
        EXPECT_TRUE(names.contains(expected)) << expected.toStdString();
    }
}

TEST(McpBridgeStandaloneTest, ToolsListToolNamesAreUnique)
{
    const QJsonArray tools = toolsOf(
        McpBridge::handleRequest(makeRequest(QStringLiteral("tools/list"))));

    const QStringList names = toolNamesOf(tools);
    QSet<QString> unique;
    for (const QString& name : names) {
        unique.insert(name);
    }
    EXPECT_EQ(unique.size(), names.size());
}

TEST(McpBridgeStandaloneTest, ToolsListEntriesExposeValidJsonSchema)
{
    static const QStringList allowedTypes = {
        QStringLiteral("string"), QStringLiteral("number"), QStringLiteral("integer"),
        QStringLiteral("boolean"), QStringLiteral("array"), QStringLiteral("object")
    };

    const QJsonArray tools = toolsOf(
        McpBridge::handleRequest(makeRequest(QStringLiteral("tools/list"))));

    for (const QJsonValue& value : tools) {
        const QJsonObject tool = value.toObject();
        const QString name = tool.value(QStringLiteral("name")).toString();

        ASSERT_FALSE(name.isEmpty());
        if (name.startsWith(QStringLiteral("debug."))) {
            // debug.* は McpBridge 側が必ず説明文を持つ。
            EXPECT_FALSE(tool.value(QStringLiteral("description")).toString().trimmed().isEmpty())
                << name.toStdString();
        }

        const QJsonObject schema = tool.value(QStringLiteral("inputSchema")).toObject();
        EXPECT_EQ(schema.value(QStringLiteral("type")).toString(), QStringLiteral("object"))
            << name.toStdString();
        ASSERT_TRUE(schema.value(QStringLiteral("properties")).isObject())
            << name.toStdString();

        const QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();
        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
            const QString type = it.value().toObject().value(QStringLiteral("type")).toString();
            EXPECT_TRUE(allowedTypes.contains(type))
                << name.toStdString() << "." << it.key().toStdString()
                << " type=" << type.toStdString();
        }
    }
}

TEST(McpBridgeStandaloneTest, ToolsListDoesNotExposeLegacyParametersArray)
{
    const QJsonArray tools = toolsOf(
        McpBridge::handleRequest(makeRequest(QStringLiteral("tools/list"))));

    for (const QJsonValue& value : tools) {
        const QJsonObject tool = value.toObject();
        EXPECT_FALSE(tool.contains(QStringLiteral("parameters")))
            << tool.value(QStringLiteral("name")).toString().toStdString();
    }
}

TEST(McpBridgeStandaloneTest, ToolsListSurfaceMatchesGetToolsTool)
{
    const QJsonArray listed = toolsOf(
        McpBridge::handleRequest(makeRequest(QStringLiteral("tools/list"))));

    const QJsonObject callResponse = McpBridge::handleRequest(makeRequest(
        QStringLiteral("tools/call"),
        QJsonObject{{QStringLiteral("name"), QStringLiteral("debug.getTools")}}));
    const QJsonArray reported = callResponse.value(QStringLiteral("result"))
                                    .toObject()
                                    .value(QStringLiteral("structuredContent"))
                                    .toObject()
                                    .value(QStringLiteral("tools"))
                                    .toArray();

    EXPECT_EQ(toolNamesOf(listed), toolNamesOf(reported));
}

// ── tools/call protocol ───────────────────────────────────────────────────

TEST(McpBridgeStandaloneTest, ToolsCallGetToolsReturnsToolSurface)
{
    const QJsonObject response = McpBridge::handleRequest(makeRequest(
        QStringLiteral("tools/call"),
        QJsonObject{{QStringLiteral("name"), QStringLiteral("debug.getTools")}}));

    ASSERT_TRUE(response.contains(QStringLiteral("result")));
    const QJsonObject result = response.value(QStringLiteral("result")).toObject();
    EXPECT_EQ(result.value(QStringLiteral("content")).toString(),
              QStringLiteral("debug.getTools"));
    const QJsonObject structured = result.value(QStringLiteral("structuredContent")).toObject();
    EXPECT_FALSE(structured.value(QStringLiteral("tools")).toArray().isEmpty());
    EXPECT_FALSE(structured.value(QStringLiteral("capabilities"))
                     .toObject()
                     .value(QStringLiteral("tools"))
                     .toObject()
                     .isEmpty());
}

TEST(McpBridgeStandaloneTest, ToolsCallDebugLogReturnsEntries)
{
    const QJsonObject response = McpBridge::handleRequest(makeRequest(
        QStringLiteral("tools/call"),
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.log")},
            {QStringLiteral("arguments"), QJsonObject{{QStringLiteral("count"), 5}}}
        }));

    ASSERT_TRUE(response.contains(QStringLiteral("result")));
    const QJsonObject structured = response.value(QStringLiteral("result"))
                                       .toObject()
                                       .value(QStringLiteral("structuredContent"))
                                       .toObject();
    const QJsonArray entries = structured.value(QStringLiteral("entries")).toArray();
    EXPECT_EQ(structured.value(QStringLiteral("count")).toInt(), entries.size());
    EXPECT_LE(entries.size(), 5);
}

TEST(McpBridgeStandaloneTest, ToolsCallUnknownToolReturnsInvalidParams)
{
    const QJsonObject response = McpBridge::handleRequest(makeRequest(
        QStringLiteral("tools/call"),
        QJsonObject{{QStringLiteral("name"), QStringLiteral("debug.definitelyMissingTool")}}));

    ASSERT_TRUE(response.contains(QStringLiteral("error")));
    EXPECT_EQ(response.value(QStringLiteral("error"))
                  .toObject()
                  .value(QStringLiteral("code"))
                  .toInt(),
              -32602);
}

TEST(McpBridgeStandaloneTest, ToolsCallWithoutNameReturnsInvalidParams)
{
    const QJsonObject response = McpBridge::handleRequest(makeRequest(
        QStringLiteral("tools/call"), QJsonObject{}));

    ASSERT_TRUE(response.contains(QStringLiteral("error")));
    EXPECT_EQ(response.value(QStringLiteral("error"))
                  .toObject()
                  .value(QStringLiteral("code"))
                  .toInt(),
              -32602);
}

// 内部 parameters で配列型として宣言されたパラメータは、tools/list の
// inputSchema でも "array" として公開されなければならない。array<string> や
// array<int> が文字列へ落ちると、外部 MCP クライアントは配列引数を渡せない。
TEST(McpBridgeStandaloneTest, ToolsListArrayParametersExposeArraySchema)
{
    const QJsonArray internalTools = McpBridge::capabilityList()
                                         .value(QStringLiteral("tools"))
                                         .toArray();
    const QJsonArray listedTools = toolsOf(
        McpBridge::handleRequest(makeRequest(QStringLiteral("tools/list"))));

    QHash<QString, QJsonObject> listedByName;
    for (const QJsonValue& value : listedTools) {
        const QJsonObject tool = value.toObject();
        listedByName.insert(tool.value(QStringLiteral("name")).toString(), tool);
    }

    int checked = 0;
    for (const QJsonValue& value : internalTools) {
        const QJsonObject tool = value.toObject();
        const QString toolName = tool.value(QStringLiteral("name")).toString();
        const QJsonArray parameters = tool.value(QStringLiteral("parameters")).toArray();
        for (const QJsonValue& parameterValue : parameters) {
            const QJsonObject parameter = parameterValue.toObject();
            const QString type = parameter.value(QStringLiteral("type")).toString().trimmed();
            if (!type.startsWith(QStringLiteral("array"), Qt::CaseInsensitive)) {
                continue;
            }
            const QString parameterName =
                parameter.value(QStringLiteral("name")).toString().trimmed();
            if (parameterName.isEmpty()) {
                continue;
            }
            ++checked;

            ASSERT_TRUE(listedByName.contains(toolName)) << toolName.toStdString();
            const QJsonObject properties = listedByName.value(toolName)
                                               .value(QStringLiteral("inputSchema"))
                                               .toObject()
                                               .value(QStringLiteral("properties"))
                                               .toObject();
            ASSERT_TRUE(properties.contains(parameterName))
                << toolName.toStdString() << "." << parameterName.toStdString();
            EXPECT_EQ(properties.value(parameterName)
                          .toObject()
                          .value(QStringLiteral("type"))
                          .toString(),
                      QStringLiteral("array"))
                << toolName.toStdString() << "." << parameterName.toStdString()
                << " internalType=" << type.toStdString();
        }
    }

    // 検査対象が空だと検査が成立しないため、配列型パラメータが1つ以上あること。
    EXPECT_GT(checked, 0);
}
