#include <QApplication>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QStyleFactory>

import ArtifactCore;
import ArtifactPr.AppTheme;
import ArtifactPr.MainWindow;
import ArtifactPr.CLI;

int main(int argc, char *argv[])
{
    bool cliMode = false;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--cli")) {
            cliMode = true;
            break;
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

    ArtifactPrMainWindow window;
    window.show();

    return app.exec();
}