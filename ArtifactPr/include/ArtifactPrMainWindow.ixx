module;
#include <QMainWindow>
#include <QKeyEvent>
#include <QCloseEvent>
#include <wobjectdefs.h>

export module ArtifactPr.MainWindow;

import ArtifactPr.Shortcut;
import ArtifactPr.ShortcutHelpDialog;
import ArtifactPr.StatusNotifier;
import ArtifactPr.TimecodeOverlayWidget;

export class TransportBarWidget;
export class MediaPanel;

export class ArtifactPrMainWindow : public QMainWindow
{
    W_OBJECT(ArtifactPrMainWindow)
public:
    explicit ArtifactPrMainWindow(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private Q_SLOTS:
    void onExportTriggered();
    void onProjectModified();
    void onUndoRedo();

Q_SIGNALS:
    void requestZoomIn();
    void requestZoomOut();
    void requestZoomReset();

private:
    TransportBarWidget* transportBar_ = nullptr;
    ArtifactPr::PrShortcutRegistry shortcutRegistry_;
    ArtifactPr::PrStatusNotifier statusNotifier_;
    ArtifactPr::ShortcutHelpDialog* helpDialog_ = nullptr;
    QTimer* autoSaveTimer_ = nullptr;
    ArtifactPr::TimecodeOverlayWidget* timecodeOverlay_ = nullptr;
    MediaPanel* mediaPanel_ = nullptr;
    bool projectDirty_ = false;
};
