module;
#include <QTreeWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QFont>
#include <QStringList>
#include <wobjectdefs.h>

export module ArtifactPr.ProjectPanel;

import ArtifactPr.EditorEngine;

export class ProjectPanel : public QWidget
{
    W_OBJECT(ProjectPanel)
public:
    explicit ProjectPanel(QWidget* parent = nullptr);
    void refreshProjectTree(const ArtifactPr::DemoSequence& seq);

private:
    QTreeWidget* tree_ = nullptr;
};
