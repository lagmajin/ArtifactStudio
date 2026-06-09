#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QTextStream>

static void writeJsonLine(FILE* stream, const QJsonObject& obj) {
    QJsonDocument doc(obj);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    fwrite(data.constData(), 1, data.size(), stream);
    fflush(stream);
}

static QJsonObject readJsonLine(QTextStream& in) {
    if (in.atEnd()) return {};
    QString line = in.readLine().trimmed();
    if (line.isEmpty()) return {};
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object();
}

static void emitEvent(FILE* stream, const QJsonObject& event) {
    writeJsonLine(stream, event);
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    QCommandLineOption pluginOpt("plugin", "Path to plugin DLL", "path");
    parser.addOption(pluginOpt);
    parser.addHelpOption();
    parser.process(app);

    if (!parser.isSet(pluginOpt)) {
        qCritical() << "--plugin is required";
        return 2;
    }

    QString pluginPath = parser.value(pluginOpt);
    QJsonObject initEvent;
    initEvent["event"] = QStringLiteral("runnerStarted");
    initEvent["pluginPath"] = pluginPath;
    emitEvent(stdout, initEvent);

    QTextStream in(stdin);
    QLibrary lib(pluginPath);

    while (!in.atEnd()) {
        QJsonObject cmd = readJsonLine(in);
        if (cmd.isEmpty()) continue;

        QString cmdName = cmd["cmd"].toString();

        if (cmdName == "load") {
            if (!lib.load()) {
                QJsonObject err;
                err["event"] = QStringLiteral("loadFailed");
                err["error"] = lib.errorString();
                emitEvent(stderr, err);
                continue;
            }

            auto fnVersion = reinterpret_cast<int(*)()>(
                lib.resolve("ArtifactPlugin_GetAPIVersion"));
            auto fnCount = reinterpret_cast<int(*)()>(
                lib.resolve("ArtifactPlugin_GetPluginCount"));

            if (!fnVersion || !fnCount) {
                lib.unload();
                QJsonObject err;
                err["event"] = QStringLiteral("loadFailed");
                err["error"] = QStringLiteral("Missing required exports");
                emitEvent(stderr, err);
                continue;
            }

            int apiVersion = fnVersion();
            int pluginCount = fnCount();

            QJsonObject loaded;
            loaded["event"] = QStringLiteral("loaded");
            loaded["apiVersion"] = apiVersion;
            loaded["pluginCount"] = pluginCount;
            emitEvent(stdout, loaded);

        } else if (cmdName == "ping") {
            QJsonObject pong;
            pong["event"] = QStringLiteral("pong");
            pong["id"] = cmd["id"].toInt();
            emitEvent(stdout, pong);

        } else if (cmdName == "shutdown") {
            QJsonObject bye;
            bye["event"] = QStringLiteral("shutdown");
            emitEvent(stdout, bye);
            break;

        } else {
            QJsonObject unknown;
            unknown["event"] = QStringLiteral("unknownCommand");
            unknown["cmd"] = cmdName;
            emitEvent(stderr, unknown);
        }
    }

    if (lib.isLoaded()) {
        lib.unload();
    }

    return 0;
}
