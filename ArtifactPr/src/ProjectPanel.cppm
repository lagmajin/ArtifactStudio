module;
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QStringList>
#include <QVariant>
#include <wobjectimpl.h>

module ArtifactPr.ProjectPanel;

ProjectPanel::ProjectPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* label = new QLabel(QStringLiteral("Project"));
    QFont titleFont = label->font();
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleFont.setBold(true);
    label->setFont(titleFont);
    layout->addWidget(label);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setMinimumHeight(120);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(tree_, 1);

    auto* engine = ArtifactPr::EditorEngine::instance();
    connect(engine, &ArtifactPr::EditorEngine::sequenceChanged, this, &ProjectPanel::refreshProjectTree);
    connect(engine, &ArtifactPr::EditorEngine::projectModified, this, [this]() {
        refreshProjectTree(ArtifactPr::EditorEngine::instance()->currentSequence());
    });
    connect(tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item) return;
        const QVariant roleValue = item->data(0, Qt::UserRole);
        const QString sequenceId = roleValue.toString();
        if (sequenceId.startsWith(QStringLiteral("sequence:"))) {
            ArtifactPr::EditorEngine::instance()->selectSequence(
                sequenceId.mid(QStringLiteral("sequence:").size()));
        }
    });
    refreshProjectTree(engine->currentSequence());
}

void ProjectPanel::refreshProjectTree(const ArtifactPr::DemoSequence& seq)
{
    tree_->clear();

    auto* projectNode = new QTreeWidgetItem(QStringList{QStringLiteral("Project")});
    projectNode->setExpanded(true);

    auto* sequencesNode = new QTreeWidgetItem(QStringList{QStringLiteral("Sequences")});
    sequencesNode->setExpanded(true);

    const auto& project = ArtifactPr::EditorEngine::instance()->currentProject();
    for (const auto& projectSequence : project.sequences) {
        auto* seqItem = new QTreeWidgetItem(QStringList{projectSequence.name});
        seqItem->setData(0, Qt::UserRole,
                         QStringLiteral("sequence:") + projectSequence.id);
        sequencesNode->addChild(seqItem);
        if (projectSequence.id == seq.id) seqItem->setSelected(true);
    }

    auto* mediaNode = new QTreeWidgetItem(QStringList{QStringLiteral("Media")});
    for (const auto& media : ArtifactPr::EditorEngine::instance()->mediaPool()) {
        mediaNode->addChild(new QTreeWidgetItem(QStringList{media.name}));
    }

    projectNode->addChild(sequencesNode);
    projectNode->addChild(mediaNode);
    projectNode->addChild(new QTreeWidgetItem(QStringList{QStringLiteral("Bins")}));
    projectNode->addChild(new QTreeWidgetItem(QStringList{QStringLiteral("Exports")}));
    tree_->addTopLevelItem(projectNode);

    tree_->expandAll();
}

W_OBJECT_IMPL(ProjectPanel)
