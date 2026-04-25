#include <QApplication>
#include <QPalette>
#include <QColor>

#include "ArtifactPrMainWindow.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ArtifactPr"));
    app.setOrganizationName(QStringLiteral("ArtifactStudio"));

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(27, 32, 38));
    darkPalette.setColor(QPalette::WindowText, QColor(235, 238, 242));
    darkPalette.setColor(QPalette::Base, QColor(18, 22, 27));
    darkPalette.setColor(QPalette::AlternateBase, QColor(31, 37, 44));
    darkPalette.setColor(QPalette::Text, QColor(235, 238, 242));
    darkPalette.setColor(QPalette::Button, QColor(40, 47, 55));
    darkPalette.setColor(QPalette::ButtonText, QColor(235, 238, 242));
    darkPalette.setColor(QPalette::Highlight, QColor(82, 150, 224));
    darkPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    app.setPalette(darkPalette);

    ArtifactPrMainWindow window;
    window.show();

    return app.exec();
}
