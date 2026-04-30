module;
#include <QFont>
#include <QFileDialog>
#include <QFileInfo>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStringList>
#include <wobjectimpl.h>

module ArtifactPr.MediaPanel;

MediaPanel::MediaPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);

    auto* importBtn = new QPushButton(QStringLiteral("Import"));
    importBtn->setMaximumWidth(70);
    connect(importBtn, &QPushButton::clicked, this, &MediaPanel::onImportClicked);
    toolbar->addWidget(importBtn);
    toolbar->addStretch();

    auto* label = new QLabel(QStringLiteral("Media"));
    QFont titleFont = label->font();
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleFont.setBold(true);
    label->setFont(titleFont);
    toolbar->addWidget(label);
    layout->addLayout(toolbar);

    list_ = new QListWidget(this);
    list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list_->setAcceptDrops(true);
    list_->setDropIndicatorShown(true);
    list_->setDragEnabled(true);
    connect(list_, &QListWidget::itemDoubleClicked, this, &MediaPanel::onItemDoubleClicked);
    layout->addWidget(list_, 1);

    auto* engine = ArtifactPr::EditorEngine::instance();
    connect(engine, &ArtifactPr::EditorEngine::sequenceChanged, this, &MediaPanel::refreshMediaList);
    refreshMediaList(engine->currentSequence());
}

void MediaPanel::refreshMediaList(const ArtifactPr::DemoSequence&)
{
    list_->clear();

    auto* engine = ArtifactPr::EditorEngine::instance();
    const auto& seq = engine->currentSequence();

    for (const auto& track : seq.videoTracks) {
        for (const auto& clip : track.clips) {
            list_->addItem(clip.name);
        }
    }
    for (const auto& track : seq.audioTracks) {
        for (const auto& clip : track.clips) {
            list_->addItem(clip.name);
        }
    }
}

void MediaPanel::addMediaFile(const QString& filePath, const QString& displayName)
{
    auto* item = new QListWidgetItem(displayName);
    item->setData(Qt::UserRole, filePath);
    list_->addItem(item);
}

void MediaPanel::onImportClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("Import Media"), QString(),
        QStringLiteral("Video Files (*.mp4 *.avi *.mov *.mkv);;Audio Files (*.mp3 *.wav *.aac);;All Files (*)"));

    if (!files.isEmpty()) {
        for (const auto& file : files) {
            addMediaFile(file, QFileInfo(file).fileName());
        }
    }
}

void MediaPanel::onItemDoubleClicked(QListWidgetItem* item)
{
    QString filePath = item->data(Qt::UserRole).toString();
    if (!filePath.isEmpty()) {
        Q_EMIT mediaSelected(filePath);
    }
}

W_OBJECT_IMPL(MediaPanel)
