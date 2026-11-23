#include <QApplication>
#include "BoneVisualizerWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("BoneVisualizerLab");

    BoneVisualizerWindow window;
    window.show();

    return app.exec();
}
