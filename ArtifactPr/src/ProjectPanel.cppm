module;
#include <QTreeWidgetItem>
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
    refreshProjectTree(engine->currentSequence());
}

void ProjectPanel::refreshProjectTree(const ArtifactPr::DemoSequence& seq)
{
    tree_->clear();

    auto* projectNode = new QTreeWidgetItem(QStringList{QStringLiteral("Project")});
    projectNode->setExpanded(true);

    auto* sequencesNode = new QTreeWidgetItem(QStringList{QStringLiteral("Sequences")});
    sequencesNode->setExpanded(true);

    auto* seqItem = new QTreeWidgetItem(QStringList{seq.name});
    seqItem->setData(0, Qt::UserRole, seq.id);
    sequencesNode->addChild(seqItem);

    projectNode->addChild(sequencesNode);
    projectNode->addChild(new QTreeWidgetItem(QStringList{QStringLiteral("Media")}));
    projectNode->addChild(new QTreeWidgetItem(QStringList{QStringLiteral("Bins")}));
    projectNode->addChild(new QTreeWidgetItem(QStringList{QStringLiteral("Exports")}));
    tree_->addTopLevelItem(projectNode);

    tree_->expandAll();
}

W_OBJECT_IMPL(ProjectPanel)
