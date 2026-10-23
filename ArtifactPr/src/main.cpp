#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QStyleFactory>

import ArtifactCore;
import ArtifactPr.AppTheme;
import ArtifactPr.MainWindow;
import ArtifactPr.CLI;
import ArtifactPr.Translation;

int main(int argc, char *argv[])
{
    bool cliMode = false;
    QString languageCode;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--cli")) {
            cliMode = true;
            continue;
        }
        if (arg == QStringLiteral("--lang") && i + 1 < argc) {
            languageCode = QString::fromLocal8Bit(argv[++i]);
        }
    }
    if (cliMode) {
        // CLI must not construct QApplication or any GUI widget. EditorEngine
        // remains the single document/undo/render owner for both front ends.
        QCoreApplication cliApp(argc, argv);
        QStringList cliArguments = cliApp.arguments();
        const int cliMarker = cliArguments.indexOf(QStringLiteral("--cli"));
        if (cliMarker >= 0) {
            cliArguments.removeAt(cliMarker);
        }
        // --lang is a GUI-only switch, but it must not shift the CLI command
        // position, so it is stripped from the argument list as well.
        const int langMarker = cliArguments.indexOf(QStringLiteral("--lang"));
        if (langMarker >= 0) {
            cliArguments.removeAt(langMarker);
            if (langMarker < cliArguments.size()) {
                cliArguments.removeAt(langMarker);
            }
        }
        return runArtifactPrCli(cliArguments);
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ArtifactPr"));
    app.setOrganizationName(QStringLiteral("ArtifactStudio"));

    // 1. ArtifactCore の DCC theme を初期化 (Studio / Dark プリセット)
    auto theme = ArtifactCore::getDCCTheme(ArtifactCore::DccStylePreset::StudioStyle);
    ArtifactCore::applyDCCTheme(app, theme);

    // 2. PrProxyStyle (QProxyStyle + Fusion) を適用。
    //    26 件の setStyleSheet はこの style が polish() / drawControl() で吸収する。
    app.setStyle(new ArtifactPr::PrProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))));

    // 3. 翻訳カタログをロードし言語を選択する。
    //    優先順は --lang > システムロケール > en。カタログは exe と同階層の
    //    translations/ に配置され、CMake の POST_BUILD で複製される。
    ArtifactPr::initializeTranslations(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations")),
        languageCode);

    ArtifactPrMainWindow window;
    window.show();

    return app.exec();
}