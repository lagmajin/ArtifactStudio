module;
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <QAbstractItemView>
#include <wobjectdefs.h>

export module ArtifactPr.MediaPanel;

import ArtifactPr.EditorEngine;

export class MediaPanel : public QWidget
{
    W_OBJECT(MediaPanel)
public:
    explicit MediaPanel(QWidget* parent = nullptr);

    void refreshMediaList(const ArtifactPr::DemoSequence&);
    void addMediaFile(const QString& filePath, const QString& displayName);

Q_SIGNALS:
    void mediaSelected(const QString& filePath) W_SIGNAL(mediaSelected, filePath);

private Q_SLOTS:
    void onImportClicked();
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    QListWidget* list_ = nullptr;
};
