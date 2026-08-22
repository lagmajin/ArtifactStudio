module;

#include <QFont>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QPalette>
#include <wobjectimpl.h>

module ArtifactPr.MediaPanel;

import ArtifactPr.MediaThumbnailer;

namespace {

bool isVideoExtension(const QString& suffix)
{
    return suffix == QStringLiteral("mp4")
        || suffix == QStringLiteral("avi")
        || suffix == QStringLiteral("mov")
        || suffix == QStringLiteral("mkv");
}

bool isImageExtension(const QString& suffix)
{
    return suffix == QStringLiteral("png")
        || suffix == QStringLiteral("jpg")
        || suffix == QStringLiteral("jpeg")
        || suffix == QStringLiteral("bmp")
        || suffix == QStringLiteral("tif")
        || suffix == QStringLiteral("tiff")
        || suffix == QStringLiteral("webp");
}

/// ListWidget の item に thumbnail + filename を描画する delegate。
/// QImage は paint() で QPixmap 化されるため、IO / 互換境界用途として OK。
class MediaThumbnailDelegate : public QStyledItemDelegate {
public:
    explicit MediaThumbnailDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void setThumbnail(const QString& filePath, const QPixmap& pix) {
        cache_[filePath] = pix;
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(180, 110);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (!painter) return;

        const QString filePath = index.data(Qt::UserRole).toString();
        const QString displayName = index.data(Qt::DisplayRole).toString();

        // 背景 (selection)
        if (option.state & QStyle::State_Selected) {
            painter->fillRect(option.rect, option.palette.highlight());
        } else {
            painter->fillRect(option.rect, option.palette.base());
        }

        const int thumbW = 160;
        const int thumbH = 90;
        const int padX = 8;
        const int padY = 8;
        const QRect itemRect = option.rect;

        QRect thumbRect(itemRect.left() + padX, itemRect.top() + padY,
                        thumbW, thumbH);
        auto it = cache_.constFind(filePath);
        if (it != cache_.constEnd() && !it.value().isNull()) {
            painter->drawPixmap(thumbRect, it.value());
        } else {
            painter->setPen(QColor(80, 80, 80));
            painter->setBrush(QColor(40, 40, 40));
            painter->drawRect(thumbRect);
            painter->drawText(thumbRect, Qt::AlignCenter, QStringLiteral("(no preview)"));
        }

        QRect textRect(itemRect.left() + padX,
                       thumbRect.bottom() + 4,
                       itemRect.width() - 2 * padX, 16);
        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, displayName);
    }

private:
    QHash<QString, QPixmap> cache_;
};

} // namespace

MediaPanel::MediaPanel(QWidget* parent)
    : QWidget(parent), thumbnailer_(new ArtifactPr::MediaThumbnailer(this))
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
    list_->setItemDelegate(new MediaThumbnailDelegate(list_));
    connect(list_, &QListWidget::itemDoubleClicked, this, &MediaPanel::onItemDoubleClicked);

    // 検索バー (PrSearchFilter)
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText(QStringLiteral("Search media..."));
    searchEdit_->setClearButtonEnabled(true);
    connect(searchEdit_, &QLineEdit::textChanged,
            this, &MediaPanel::applySearchFilter);
    layout->addWidget(searchEdit_);

    // thumbnail 完成時に delegate に通知
    connect(thumbnailer_, &ArtifactPr::MediaThumbnailer::thumbnailReady,
            this, [this](ArtifactPr::MediaThumbnail thumb) {
        if (!thumb.valid) return;
        auto* delegate = qobject_cast<MediaThumbnailDelegate*>(list_->itemDelegate());
        if (!delegate) return;
        QPixmap pix = QPixmap::fromImage(thumb.image);
        delegate->setThumbnail(thumb.filePath, pix);
        for (int i = 0; i < list_->count(); ++i) {
            auto* item = list_->item(i);
            if (item && item->data(Qt::UserRole).toString() == thumb.filePath) {
                list_->update(list_->indexFromItem(item));
                break;
            }
        }
    });
    thumbnailer_->start();

    layout->addWidget(list_, 1);

    auto* engine = ArtifactPr::EditorEngine::instance();
    connect(engine, &ArtifactPr::EditorEngine::sequenceChanged, this, &MediaPanel::refreshMediaList);
    refreshMediaList(engine->currentSequence());
}

void MediaPanel::applySearchFilter(const QString& text)
{
    const QString query = text.trimmed();
    for (int i = 0; i < list_->count(); ++i) {
        auto* item = list_->item(i);
        if (!item) continue;

        const QString filePath = item->data(Qt::UserRole).toString();
        const bool matches = query.isEmpty()
            || item->text().contains(query, Qt::CaseInsensitive)
            || filePath.contains(query, Qt::CaseInsensitive);
        item->setHidden(!matches);
    }
}

void MediaPanel::refreshMediaList(const ArtifactPr::DemoSequence&)
{
    list_->clear();

    auto* engine = ArtifactPr::EditorEngine::instance();
    const auto& seq = engine->currentSequence();
    const auto alreadyListed = [this](const QString& filePath) {
        for (int i = 0; i < list_->count(); ++i) {
            const auto* item = list_->item(i);
            if (item && item->data(Qt::UserRole).toString() == filePath) return true;
        }
        return false;
    };

    for (const auto& track : seq.videoTracks) {
        for (const auto& clip : track.clips) {
            if (!clip.sourceFile.isEmpty() && !alreadyListed(clip.sourceFile)) {
                const QString displayName = clip.name.isEmpty()
                    ? QFileInfo(clip.sourceFile).fileName()
                    : clip.name;
                addMediaFile(clip.sourceFile, displayName);
            }
        }
    }
    for (const auto& track : seq.audioTracks) {
        for (const auto& clip : track.clips) {
            if (!clip.sourceFile.isEmpty() && !alreadyListed(clip.sourceFile)) {
                const QString displayName = clip.name.isEmpty()
                    ? QFileInfo(clip.sourceFile).fileName()
                    : clip.name;
                addMediaFile(clip.sourceFile, displayName);
            }
        }
    }

    for (const auto& media : engine->mediaPool()) {
        if ((media.type == QStringLiteral("video") ||
             media.type == QStringLiteral("audio") ||
             media.type == QStringLiteral("image"))
            && !media.filePath.isEmpty()
            && !alreadyListed(media.filePath)) {
            addMediaFile(media.filePath, media.name.isEmpty()
                ? QFileInfo(media.filePath).fileName()
                : media.name);
        }
    }

    applySearchFilter(searchEdit_->text());
}

void MediaPanel::addMediaFile(const QString& filePath, const QString& displayName)
{
    auto* item = new QListWidgetItem(displayName);
    item->setData(Qt::UserRole, filePath);
    list_->addItem(item);

    // thumbnail を非同期要求 (映像ファイルの場合のみ)
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (isVideoExtension(suffix) && QFileInfo::exists(filePath)) {
        ArtifactPr::ThumbnailRequest req;
        req.filePath = filePath;
        req.targetSize = QSize(160, 90);
        req.seekToMs = 1000;
        thumbnailer_->request(req);
    }
}

void MediaPanel::onImportClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("Import Media"), QString(),
        QStringLiteral("Media Files (*.mp4 *.avi *.mov *.mkv *.mp3 *.wav *.aac *.flac *.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;Video Files (*.mp4 *.avi *.mov *.mkv);;Audio Files (*.mp3 *.wav *.aac *.flac);;Image Files (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;All Files (*)"));

    if (!files.isEmpty()) {
        for (const auto& file : files) {
            const QString suffix = QFileInfo(file).suffix().toLower();
            const bool isVideo = isVideoExtension(suffix);
            const bool isImage = isImageExtension(suffix);
            bool alreadyListed = false;
            for (int i = 0; i < list_->count(); ++i) {
                const auto* item = list_->item(i);
                if (item && item->data(Qt::UserRole).toString() == file) {
                    alreadyListed = true;
                    break;
                }
            }
            if (!alreadyListed) {
                addMediaFile(file, QFileInfo(file).fileName());
            }
            const bool isAudio = suffix == QStringLiteral("mp3")
                || suffix == QStringLiteral("wav")
                || suffix == QStringLiteral("aac")
                || suffix == QStringLiteral("flac");
            if (isVideo || isAudio || isImage) {
                auto* engine = ArtifactPr::EditorEngine::instance();
                QString mediaType;
                if (isVideo) mediaType = QStringLiteral("video");
                else if (isImage) mediaType = QStringLiteral("image");
                else mediaType = QStringLiteral("audio");
                bool alreadyRegistered = false;
                for (const auto& media : engine->mediaPool()) {
                    if (media.filePath == file && media.type == mediaType) {
                        alreadyRegistered = true;
                        break;
                    }
                }
                if (!alreadyRegistered) {
                    engine->addMediaToPool(file, QFileInfo(file).fileName(), mediaType);
                }
            }
        }
        applySearchFilter(searchEdit_->text());
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
