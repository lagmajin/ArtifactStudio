#include <QApplication>
#include <QStyleFactory>

import ArtifactCore;
import ArtifactPr.AppTheme;
import ArtifactPr.MainWindow;

int main(int argc, char *argv[])
{
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