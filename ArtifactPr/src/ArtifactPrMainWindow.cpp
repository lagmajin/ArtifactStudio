#include "ArtifactPrMainWindow.hpp"
#include "ArtifactPrDemoData.hpp"

#include <QApplication>
#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QFont>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QPalette>
#include <QScrollArea>
#include <QStringList>
#include <QSplitter>
#include <QStatusBar>
#include <QTreeWidgetItem>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

QFrame *makePanelFrame(const QString &title)
{
    auto *frame = new QFrame();
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setFrameShadow(QFrame::Raised);

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto *label = new QLabel(title);
    QFont titleFont = label->font();
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleFont.setBold(true);
    label->setFont(titleFont);
    layout->addWidget(label);
    return frame;
}

QFrame *makeClipCard(const QString &name, const QString &meta)
{
    auto *card = new QFrame();
    card->setFrameShape(QFrame::Box);
    card->setFrameShadow(QFrame::Plain);
    card->setMinimumWidth(140);
    card->setMaximumHeight(76);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(4);

    auto *nameLabel = new QLabel(name);
    QFont nameFont = nameLabel->font();
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);

    auto *metaLabel = new QLabel(meta);
    metaLabel->setWordWrap(true);

    layout->addWidget(nameLabel);
    layout->addWidget(metaLabel);
    return card;
}

QWidget *buildProjectBrowser(const ArtifactPrProjectSpec &project)
{
    auto *panel = makePanelFrame(QStringLiteral("Project"));
    auto *layout = qobject_cast<QVBoxLayout *>(panel->layout());

    auto *tree = new QTreeWidget(panel);
    tree->setHeaderHidden(true);
    auto *projectNode = new QTreeWidgetItem(QStringList{project.name});
    auto *sequenceNode = new QTreeWidgetItem(QStringList{QStringLiteral("Sequences")});
    for (const auto &sequence : project.sequences) {
        sequenceNode->addChild(new QTreeWidgetItem(QStringList{sequence.name}));
    }
    projectNode->addChild(sequenceNode);
    projectNode->addChild(new QTreeWidgetItem(QStringList{QStringLiteral("Media")}));
    projectNode->addChild(new QTreeWidgetItem(QStringList{QStringLiteral("Bins")}));
    projectNode->addChild(new QTreeWidgetItem(QStringList{QStringLiteral("Exports")}));
    tree->addTopLevelItem(projectNode);
    tree->expandAll();

    layout->addWidget(tree, 1);
    return panel;
}

QWidget *buildMediaBrowser(const ArtifactPrProjectSpec &project)
{
    auto *panel = makePanelFrame(QStringLiteral("Media"));
    auto *layout = qobject_cast<QVBoxLayout *>(panel->layout());

    auto *list = new QListWidget(panel);
    list->addItems(project.mediaItems);

    layout->addWidget(list, 1);
    return panel;
}

QWidget *buildSidebar(const ArtifactPrProjectSpec &project)
{
    auto *panel = new QWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(buildProjectBrowser(project), 1);
    layout->addWidget(buildMediaBrowser(project), 1);
    return panel;
}

QWidget *buildTimelineLane(const QString &laneName, const QVector<ArtifactPrClipSpec> &clips)
{
    auto *lane = new QWidget();
    auto *layout = new QHBoxLayout(lane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *nameLabel = new QLabel(laneName);
    nameLabel->setMinimumWidth(44);
    nameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(nameLabel);

    auto *clipStrip = new QWidget();
    auto *clipLayout = new QHBoxLayout(clipStrip);
    clipLayout->setContentsMargins(0, 0, 0, 0);
    clipLayout->setSpacing(10);

    for (const auto &clip : clips) {
        clipLayout->addWidget(makeClipCard(clip.name, clip.meta));
    }
    clipLayout->addStretch(1);

    layout->addWidget(clipStrip, 1);
    return lane;
}

QWidget *buildTimelinePanel(const ArtifactPrSequenceSpec &sequence)
{
    auto *panel = makePanelFrame(QStringLiteral("Timeline"));
    auto *layout = qobject_cast<QVBoxLayout *>(panel->layout());

    auto *ruler = new QLabel(QStringLiteral("0s      5s      10s      15s      20s"));
    ruler->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(ruler);

    auto *sequenceSummary = new QLabel(
        QStringLiteral("%1    %2    %3").arg(sequence.name, sequence.resolution, sequence.frameRate));
    layout->addWidget(sequenceSummary);

    auto *scrollArea = new QScrollArea(panel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *timelineHost = new QWidget();
    auto *timelineLayout = new QVBoxLayout(timelineHost);
    timelineLayout->setContentsMargins(0, 0, 0, 0);
    timelineLayout->setSpacing(12);

    for (const auto &track : sequence.videoTracks) {
        timelineLayout->addWidget(buildTimelineLane(track.name, track.clips));
    }
    for (const auto &track : sequence.audioTracks) {
        timelineLayout->addWidget(buildTimelineLane(track.name, track.clips));
    }
    timelineLayout->addStretch(1);

    scrollArea->setWidget(timelineHost);
    layout->addWidget(scrollArea, 1);

    return panel;
}

QWidget *buildPreviewPanel(const ArtifactPrSequenceSpec &sequence)
{
    auto *panel = makePanelFrame(QStringLiteral("Program Monitor"));
    auto *layout = qobject_cast<QVBoxLayout *>(panel->layout());

    auto *preview = new QLabel(QStringLiteral("%1 preview").arg(sequence.name));
    preview->setAlignment(Qt::AlignCenter);
    preview->setMinimumSize(320, 180);
    preview->setFrameShape(QFrame::Box);
    layout->addWidget(preview, 2);

    auto *inspector = new QListWidget(panel);
    inspector->addItems(QStringList{
        QStringLiteral("Sequence: %1").arg(sequence.name),
        QStringLiteral("Resolution: %1").arg(sequence.resolution),
        QStringLiteral("Frame rate: %1").arg(sequence.frameRate),
        QStringLiteral("Playback: Real-time"),
    });
    layout->addWidget(inspector, 1);

    return panel;
}

QWidget *buildTransportBar()
{
    auto *bar = new QFrame();
    bar->setFrameShape(QFrame::StyledPanel);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    auto addButton = [layout](const QString &text) {
        auto *button = new QToolButton();
        button->setText(text);
        layout->addWidget(button);
        return button;
    };

    addButton(QStringLiteral("◀"));
    addButton(QStringLiteral("⏮"));
    addButton(QStringLiteral("▶"));
    addButton(QStringLiteral("⏭"));

    auto *timecode = new QLabel(QStringLiteral("00:00:00:00"));
    QFont timecodeFont = timecode->font();
    timecodeFont.setBold(true);
    timecode->setFont(timecodeFont);
    layout->addWidget(timecode);

    layout->addStretch(1);

    auto *exportButton = new QPushButton(QStringLiteral("Export"));
    layout->addWidget(exportButton);

    return bar;
}

void applyWorkspacePalette(QWidget *root)
{
    QPalette palette = qApp->palette();
    palette.setColor(QPalette::Window, QColor(27, 32, 38));
    palette.setColor(QPalette::WindowText, QColor(235, 238, 242));
    palette.setColor(QPalette::Base, QColor(18, 22, 27));
    palette.setColor(QPalette::AlternateBase, QColor(31, 37, 44));
    palette.setColor(QPalette::Text, QColor(235, 238, 242));
    palette.setColor(QPalette::Button, QColor(40, 47, 55));
    palette.setColor(QPalette::ButtonText, QColor(235, 238, 242));
    palette.setColor(QPalette::Highlight, QColor(82, 150, 224));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    root->setPalette(palette);
    qApp->setPalette(palette);
}

} // namespace

ArtifactPrMainWindow::ArtifactPrMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    const ArtifactPrProjectSpec project = makeArtifactPrDemoProject();
    const ArtifactPrSequenceSpec sequence = project.sequences.isEmpty()
                                                ? ArtifactPrSequenceSpec{}
                                                : project.sequences.first();

    setWindowTitle(QStringLiteral("ArtifactPr"));
    resize(1600, 980);

    auto *root = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);

    auto *workspaceSplitter = new QSplitter(Qt::Horizontal, root);
    workspaceSplitter->addWidget(buildSidebar(project));
    workspaceSplitter->addWidget(buildTimelinePanel(sequence));
    workspaceSplitter->addWidget(buildPreviewPanel(sequence));
    workspaceSplitter->setStretchFactor(0, 0);
    workspaceSplitter->setStretchFactor(1, 1);
    workspaceSplitter->setStretchFactor(2, 0);
    workspaceSplitter->setSizes(QList<int>{280, 880, 360});

    rootLayout->addWidget(workspaceSplitter, 1);
    rootLayout->addWidget(buildTransportBar(), 0);

    setCentralWidget(root);
    statusBar()->showMessage(QStringLiteral("ArtifactCore-backed editing shell"));
    applyWorkspacePalette(root);
}
