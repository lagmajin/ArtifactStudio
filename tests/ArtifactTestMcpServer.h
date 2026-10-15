#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <QCoreApplication>
#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

// Reusable stdio MCP host for standalone GUI test programs. Register app-specific
// tools on the GUI thread, then start(); tool calls are dispatched back to it.
class ArtifactTestMcpServer final : public QObject {
public:
    using Handler = std::function<QJsonObject(const QJsonObject&)>;

    explicit ArtifactTestMcpServer(QObject* parent = nullptr)
        : QObject(parent), serverName_(QStringLiteral("artifact-test-control")),
          serverVersion_(QStringLiteral("1.0.0")) {}

    ArtifactTestMcpServer(QString serverName, QString serverVersion,
                          QObject* parent = nullptr)
        : QObject(parent), serverName_(std::move(serverName)),
          serverVersion_(std::move(serverVersion)) {}

    ~ArtifactTestMcpServer() override
    {
        stopping_.store(true);
        if (reader_.joinable()) reader_.join();
        restoreStdout();
    }

    bool registerTool(QString name, QString description, QJsonObject inputSchema,
                      Handler handler)
    {
        if (reader_.joinable() || !isValidToolName(name) || !handler ||
            tools_.contains(name) ||
            inputSchema.value(QStringLiteral("type")).toString() !=
                QStringLiteral("object")) {
            return false;
        }
        QJsonObject definition{
            {QStringLiteral("name"), name},
            {QStringLiteral("description"), description},
            {QStringLiteral("inputSchema"), inputSchema}
        };
        tools_.insert(name, Tool{std::move(definition), std::move(handler)});
        return true;
    }

    bool start()
    {
        if (reader_.joinable()) return false;
        std::fflush(stdout);
#ifdef _WIN32
        previousStdoutHandle_ = GetStdHandle(STD_OUTPUT_HANDLE);
        outputDescriptor_ = _dup(_fileno(stdout));
        if (outputDescriptor_ < 0)
            return false;
        previousStdoutMode_ = _setmode(_fileno(stdout), _O_BINARY);
        if (previousStdoutMode_ < 0) {
            _close(outputDescriptor_);
            outputDescriptor_ = -1;
            return false;
        }
        if (_setmode(_fileno(stdout), previousStdoutMode_) < 0) {
            restoreStdout();
            return false;
        }
        if (_dup2(_fileno(stderr), _fileno(stdout)) != 0) {
            restoreStdout();
            return false;
        }
        if (_setmode(outputDescriptor_, _O_BINARY) < 0 ||
            !SetStdHandle(STD_OUTPUT_HANDLE, GetStdHandle(STD_ERROR_HANDLE))) {
            restoreStdout();
            return false;
        }
#else
        outputDescriptor_ = dup(STDOUT_FILENO);
        if (outputDescriptor_ < 0)
            return false;
        if (dup2(STDERR_FILENO, STDOUT_FILENO) < 0) {
            restoreStdout();
            return false;
        }
#endif
        try {
            reader_ = std::thread([this] { readLoop(); });
        } catch (...) {
            restoreStdout();
            return false;
        }
        return true;
    }

private:
    struct Tool {
        QJsonObject definition;
        Handler handler;
    };

    static constexpr qsizetype kMaxFrameBytes = 8 * 1024 * 1024;
    static constexpr auto kModernProtocolVersion = "2026-07-28";
    QHash<QString, Tool> tools_;
    QString serverName_;
    QString serverVersion_;
    std::atomic_bool stopping_{false};
    std::thread reader_;
    int outputDescriptor_ = -1;
    bool legacyMode_ = false;
#ifdef _WIN32
    HANDLE previousStdoutHandle_ = INVALID_HANDLE_VALUE;
    int previousStdoutMode_ = -1;
#endif

    static bool isValidToolName(const QString& name)
    {
        if (name.isEmpty() || name.size() > 128) return false;
        for (const QChar character : name) {
            const auto value = character.unicode();
            const bool valid = (value >= 'a' && value <= 'z') ||
                               (value >= 'A' && value <= 'Z') ||
                               (value >= '0' && value <= '9') ||
                               value == '_' || value == '-' || value == '.';
            if (!valid) return false;
        }
        return true;
    }

    void restoreStdout()
    {
        if (outputDescriptor_ < 0) return;
#ifdef _WIN32
        if (_dup2(outputDescriptor_, _fileno(stdout)) == 0 &&
            previousStdoutMode_ >= 0) {
            _setmode(_fileno(stdout), previousStdoutMode_);
        }
        if (previousStdoutHandle_ != INVALID_HANDLE_VALUE)
            SetStdHandle(STD_OUTPUT_HANDLE, previousStdoutHandle_);
        _close(outputDescriptor_);
        previousStdoutHandle_ = INVALID_HANDLE_VALUE;
        previousStdoutMode_ = -1;
#else
        dup2(outputDescriptor_, STDOUT_FILENO);
        close(outputDescriptor_);
#endif
        outputDescriptor_ = -1;
    }

    QJsonObject serverInfo() const
    {
        return {{QStringLiteral("name"), serverName_},
                {QStringLiteral("version"), serverVersion_}};
    }

    static QByteArray encodeFrame(const QJsonObject& object)
    {
        return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    }

    static bool hasToolError(const QJsonObject& result)
    {
        const QJsonValue error = result.value(QStringLiteral("error"));
        if (error.isUndefined() || error.isNull()) return false;
        if (error.isBool()) return error.toBool();
        if (error.isString()) return !error.toString().isEmpty();
        if (error.isDouble()) return error.toDouble() != 0.0;
        if (error.isArray()) return !error.toArray().isEmpty();
        if (error.isObject()) return !error.toObject().isEmpty();
        return true;
    }

    QJsonObject makeError(const QJsonValue& id, int code, const QString& message) const
    {
        return {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("id"), id},
                {QStringLiteral("error"), QJsonObject{
                    {QStringLiteral("code"), code},
                    {QStringLiteral("message"), message}}}};
    }

    QJsonObject handleRequest(const QJsonObject& request)
    {
        const QJsonValue id = request.value(QStringLiteral("id"));
        const QString method = request.value(QStringLiteral("method")).toString();
        const QJsonValue paramsValue = request.value(QStringLiteral("params"));
        if (!paramsValue.isUndefined() && !paramsValue.isObject())
            return makeError(id, -32602, QStringLiteral("params must be an object"));
        const QJsonObject params = paramsValue.toObject();
        const QJsonValue metadataValue = params.value(QStringLiteral("_meta"));
        const QJsonObject metadata = metadataValue.toObject();
        const QString versionKey = QStringLiteral(
            "io.modelcontextprotocol/protocolVersion");
        const QString capabilitiesKey = QStringLiteral(
            "io.modelcontextprotocol/clientCapabilities");
        const QString clientInfoKey = QStringLiteral(
            "io.modelcontextprotocol/clientInfo");
        const bool modern = method == QStringLiteral("server/discover") ||
                            metadata.contains(versionKey) ||
                            metadata.contains(capabilitiesKey);
        if (modern && (!metadataValue.isObject() ||
                       !metadata.value(versionKey).isString() ||
                       !metadata.contains(versionKey) ||
                       !metadata.value(capabilitiesKey).isObject() ||
                       !metadata.contains(capabilitiesKey) ||
                       (metadata.contains(clientInfoKey) &&
                        !metadata.value(clientInfoKey).isObject()))) {
            return makeError(id, -32602,
                QStringLiteral("Modern MCP requests require protocolVersion and clientCapabilities metadata"));
        }
        const QString requestVersion = metadata.value(versionKey).toString();
        if (modern && requestVersion != QLatin1String(kModernProtocolVersion)) {
            QJsonObject error = makeError(
                id, -32022, QStringLiteral("Unsupported MCP protocol version"));
            QJsonObject errorObject = error.value(QStringLiteral("error")).toObject();
            errorObject[QStringLiteral("data")] = QJsonObject{
                {QStringLiteral("supported"), QJsonArray{QStringLiteral("2026-07-28")}},
                {QStringLiteral("requested"), requestVersion}};
            error[QStringLiteral("error")] = errorObject;
            return error;
        }
        if (!modern && !legacyMode_ && method != QStringLiteral("initialize")) {
            return makeError(id, -32001,
                QStringLiteral("Send server/discover or legacy initialize before MCP requests"));
        }
        const QJsonObject server = serverInfo();
        if (method == QStringLiteral("initialize")) {
            if (modern) return makeError(id, -32601,
                QStringLiteral("initialize is unavailable in the stateless MCP protocol"));
            const QString requestedVersion = params.value(QStringLiteral("protocolVersion")).toString();
            const QStringList legacyVersions{QStringLiteral("2025-11-25"),
                QStringLiteral("2025-06-18"), QStringLiteral("2025-03-26"),
                QStringLiteral("2024-11-05")};
            if (!legacyVersions.contains(requestedVersion))
                return makeError(id, -32001, QStringLiteral("Unsupported legacy MCP protocol version"));
            legacyMode_ = true;
            return {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), id},
                    {QStringLiteral("result"), QJsonObject{
                        {QStringLiteral("protocolVersion"), requestedVersion},
                        {QStringLiteral("capabilities"), QJsonObject{
                            {QStringLiteral("tools"), QJsonObject{}}}},
                        {QStringLiteral("serverInfo"), server}}}};
        }
        if (method == QStringLiteral("server/discover")) {
            if (requestVersion != QLatin1String(kModernProtocolVersion))
                return makeError(id, -32001, QStringLiteral("Discovery requires protocol version 2026-07-28"));
            return {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), id},
                    {QStringLiteral("result"), QJsonObject{
                        {QStringLiteral("resultType"), QStringLiteral("complete")},
                        {QStringLiteral("supportedVersions"), QJsonArray{
                            QStringLiteral("2026-07-28")}},
                        {QStringLiteral("capabilities"), QJsonObject{
                            {QStringLiteral("tools"), QJsonObject{}}}},
                        {QStringLiteral("_meta"), QJsonObject{
                            {QStringLiteral("io.modelcontextprotocol/serverInfo"), server}}},
                        {QStringLiteral("ttlMs"), 300000},
                        {QStringLiteral("cacheScope"), QStringLiteral("public")}}}};
        }
        if (method == QStringLiteral("notifications/initialized")) return {};
        if (method == QStringLiteral("ping") && !modern) {
            return {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), id},
                    {QStringLiteral("result"), QJsonObject{}}};
        }
        if (method == QStringLiteral("tools/list")) {
            QJsonArray definitions;
            QStringList names = tools_.keys();
            names.sort(Qt::CaseSensitive);
            for (const QString& name : names) definitions.append(tools_.value(name).definition);
            QJsonObject result{{QStringLiteral("tools"), definitions}};
            if (modern) {
                result[QStringLiteral("resultType")] = QStringLiteral("complete");
                result[QStringLiteral("ttlMs")] = 300000;
                result[QStringLiteral("cacheScope")] = QStringLiteral("public");
                result[QStringLiteral("_meta")] = QJsonObject{
                    {QStringLiteral("io.modelcontextprotocol/serverInfo"), server}};
            }
            return {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), id},
                    {QStringLiteral("result"), result}};
        }
        if (method == QStringLiteral("tools/call")) {
            const QJsonValue nameValue = params.value(QStringLiteral("name"));
            if (!nameValue.isString() || nameValue.toString().isEmpty())
                return makeError(id, -32602,
                    QStringLiteral("tools/call requires a non-empty string name"));
            const QJsonValue argumentsValue =
                params.value(QStringLiteral("arguments"));
            if (!argumentsValue.isUndefined() && !argumentsValue.isObject())
                return makeError(id, -32602,
                    QStringLiteral("tools/call arguments must be an object"));
            const QString name = nameValue.toString();
            const auto tool = tools_.constFind(name);
            if (tool == tools_.cend()) return makeError(id, -32602,
                QStringLiteral("Unknown test control tool: ") + name);
            QJsonObject result;
            try {
                result = tool->handler(
                    argumentsValue.toObject());
            } catch (const std::exception& exception) {
                result = {{QStringLiteral("error"),
                           QString::fromUtf8(exception.what())}};
            } catch (...) {
                result = {{QStringLiteral("error"),
                           QStringLiteral("Test control tool failed unexpectedly")}};
            }
            QJsonArray content = result.value(QStringLiteral("mcpContent")).toArray();
            if (content.isEmpty()) {
                content.append(QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("text")},
                    {QStringLiteral("text"), QString::fromUtf8(
                        QJsonDocument(result).toJson(QJsonDocument::Compact))}});
            }
            QJsonObject structured = result;
            structured.remove(QStringLiteral("mcpContent"));
            QJsonObject toolResult{
                {QStringLiteral("content"), content},
                {QStringLiteral("structuredContent"), structured},
                {QStringLiteral("isError"),
                 hasToolError(structured)}};
            if (modern) {
                toolResult[QStringLiteral("resultType")] = QStringLiteral("complete");
                toolResult[QStringLiteral("_meta")] = QJsonObject{
                    {QStringLiteral("io.modelcontextprotocol/serverInfo"), server}};
            }
            return {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), id},
                    {QStringLiteral("result"), toolResult}};
        }
        return makeError(id, -32601, QStringLiteral("Method not found: ") + method);
    }

    bool dispatchRequest(const QJsonObject& request, QJsonObject* response)
    {
        auto result = std::make_shared<std::promise<QJsonObject>>();
        auto future = result->get_future();
        const bool queued = QMetaObject::invokeMethod(this,
            [this, request, result] { result->set_value(handleRequest(request)); },
            Qt::QueuedConnection);
        if (!queued) return false;
        // Tool handlers may run a bounded endurance wait (up to 24 hours).
        // Keep the dispatch deadline aligned with that supported duration;
        // the old 120-second ceiling closed the stdio server during long runs.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::hours(24);
        while (future.wait_for(std::chrono::milliseconds(50)) != std::future_status::ready) {
            if (stopping_.load() || std::chrono::steady_clock::now() >= deadline)
                return false;
        }
        *response = future.get();
        return true;
    }

    static int readAvailable(char* destination, int capacity)
    {
#ifdef _WIN32
        const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        DWORD available = 0;
        if (input == INVALID_HANDLE_VALUE || input == nullptr ||
            !PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr)) return -1;
        if (available == 0) {
            Sleep(25);
            return 0;
        }
        DWORD readCount = 0;
        if (!ReadFile(input, destination,
                      static_cast<DWORD>(capacity < static_cast<int>(available)
                          ? capacity : available), &readCount, nullptr)) return -1;
        return static_cast<int>(readCount);
#else
        pollfd descriptor{STDIN_FILENO, POLLIN, 0};
        const int ready = poll(&descriptor, 1, 100);
        if (ready == 0) return 0;
        if (ready < 0) return errno == EINTR ? 0 : -1;
        if (descriptor.revents & (POLLERR | POLLNVAL)) return -1;
        const ssize_t count = read(STDIN_FILENO, destination,
                                   static_cast<size_t>(capacity));
        if (count < 0 && errno == EINTR) return 0;
        return count <= 0 ? -1 : static_cast<int>(count);
#endif
    }

    bool writeResponse(const QJsonObject& response)
    {
        const QByteArray frame = encodeFrame(response);
        qsizetype offset = 0;
        while (offset < frame.size()) {
#ifdef _WIN32
            const int written = _write(outputDescriptor_, frame.constData() + offset,
                                       static_cast<unsigned int>(frame.size() - offset));
#else
            const ssize_t written = write(outputDescriptor_, frame.constData() + offset,
                                          static_cast<size_t>(frame.size() - offset));
#endif
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) return false;
            offset += written;
        }
        return true;
    }

    void readLoop()
    {
        QByteArray buffer;
        char chunk[4096];
        while (!stopping_.load()) {
            const int count = readAvailable(chunk, static_cast<int>(sizeof(chunk)));
            if (count < 0) break;
            if (count == 0) continue;
            buffer.append(chunk, count);
            if (buffer.size() > kMaxFrameBytes) break;
            while (true) {
                const int lineEnd = buffer.indexOf('\n');
                if (lineEnd < 0) break;
                QByteArray body = buffer.left(lineEnd);
                buffer.remove(0, lineEnd + 1);
                if (body.endsWith('\r')) body.chop(1);
                if (body.trimmed().isEmpty()) continue;
                QJsonParseError parseError;
                const auto document = QJsonDocument::fromJson(body, &parseError);
                if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                    if (!writeResponse(makeError(QJsonValue(QJsonValue::Null), -32700,
                                                 parseError.errorString()))) {
                        stopping_.store(true);
                        break;
                    }
                    continue;
                }
                const QJsonObject request = document.object();
                if (!request.contains(QStringLiteral("id"))) continue;
                QJsonObject response;
                if (!dispatchRequest(request, &response)) {
                    stopping_.store(true);
                    break;
                }
                if (!writeResponse(response)) {
                    stopping_.store(true);
                    break;
                }
            }
        }
        stopping_.store(true);
        QMetaObject::invokeMethod(QCoreApplication::instance(), [] {
            QCoreApplication::quit();
        }, Qt::QueuedConnection);
    }
};
