#include <QApplication>

#include "ArtifactPrMainWindow.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ArtifactPr"));
    app.setOrganizationName(QStringLiteral("ArtifactStudio"));

    ArtifactPrMainWindow window;
    window.show();

    return app.exec();
}
