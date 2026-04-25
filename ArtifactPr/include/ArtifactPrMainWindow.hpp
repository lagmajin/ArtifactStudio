#pragma once

#include <QMainWindow>
#include <wobjectdefs.h>

class TransportBarWidget;

class ArtifactPrMainWindow : public QMainWindow
{
    W_OBJECT(ArtifactPrMainWindow)
public:
    explicit ArtifactPrMainWindow(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private Q_SLOTS:
    void onExportTriggered();

Q_SIGNALS:
    void requestZoomIn();
    void requestZoomOut();
    void requestZoomReset();

private:
    TransportBarWidget* transportBar_ = nullptr;
};
