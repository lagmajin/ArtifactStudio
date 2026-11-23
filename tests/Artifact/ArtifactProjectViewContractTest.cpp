// Project View UI contract test.
//
// Covers the surfaces that actually drive the Project View panel:
//   - ArtifactProjectModel      : project tree -> rows / roles / header labels
//   - ProjectFilterProxyModel   : search / type / unused filtering and sorting
//   - ArtifactProjectView       : expand & collapse, presentation mode, hit
//                                 testing, column width, sorting, selection
//   - ArtifactProjectManagerWidget : the panel that hosts the view
//
// The suite is hermetic: it never touches the real project service, the asset
// database, or any file outside a QTemporaryDir. It runs headless through the
// offscreen QPA platform.

#include <gtest/gtest.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QModelIndex>
#include <QRect>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>

import Artifact.Project;
import Artifact.Project.Items;
import Artifact.Project.Model;
import Artifact.Project.Roles;
import Artifact.Widgets.ProjectFilterProxyModel;
import Artifact.Widgets.ProjectManagerWidget;

namespace {

QApplication& testApplication()
{
    if (qgetenv("QT_QPA_PLATFORM").isEmpty()) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    static int argc = 1;
    static char applicationName[] = "ArtifactProjectViewContractTest";
    static char* argv[] = {applicationName, nullptr};
    static QApplication instance(argc, argv);
    return instance;
}

QJsonObject folderObject(const QString& name, const QJsonArray& children = QJsonArray())
{
    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("folder"));
    object.insert(QStringLiteral("name"), name);
    if (!children.isEmpty()) {
        object.insert(QStringLiteral("children"), children);
    }
    return object;
}

QJsonObject footageObject(const QString& name, const QString& filePath)
{
    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("footage"));
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("filePath"), filePath);
    return object;
}

int itemTypeRole()
{
    return Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType);
}

QString displayName(const QAbstractItemModel* model, const QModelIndex& index)
{
    return model->data(index.siblingAtColumn(0), Qt::DisplayRole).toString();
}

} // namespace

class ArtifactProjectViewContractTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        testApplication();
        ASSERT_TRUE(tempDir_.isValid());

        existingClipPath_ = QDir::cleanPath(tempDir_.filePath(QStringLiteral("existing_clip.png")));
        QFile existingClip(existingClipPath_);
        ASSERT_TRUE(existingClip.open(QIODevice::WriteOnly));
        existingClip.write("artifact-project-view-contract-test");
        existingClip.close();

        missingClipPath_ = QDir::cleanPath(tempDir_.filePath(QStringLiteral("missing_clip.mov")));

        project_ = Artifact::makeShared<Artifact::ArtifactProject>(
            QStringLiteral("ProjectViewContractProject"));

        QJsonObject shots = folderObject(QStringLiteral("Shots"));
        shots.insert(QStringLiteral("tags"), QJsonArray{QStringLiteral("hero")});
        shots.insert(QStringLiteral("children"),
                     QJsonArray{footageObject(QStringLiteral("existing_clip.png"), existingClipPath_),
                                footageObject(QStringLiteral("missing_clip.mov"), missingClipPath_)});

        const QJsonArray items{shots,
                               folderObject(QStringLiteral("Assets"))};

        ASSERT_TRUE(project_->addProjectItemsFromJson(items));

        model_ = std::make_unique<Artifact::ArtifactProjectModel>();
        model_->setProject(project_);

        proxy_ = std::make_unique<Artifact::ProjectFilterProxyModel>();
        proxy_->setSourceModel(model_.get());
        proxy_->setRecursiveFilteringEnabled(true);
        proxy_->setAdvancedFilter(QString(), QStringLiteral("All"), false);
    }

    // Row 0 of the tree: the "Shots" folder.
    QModelIndex shotsIndex() const { return model_->index(0, 0, QModelIndex()); }
    // Child rows of the "Shots" folder.
    QModelIndex childIndex(int row) const { return model_->index(row, 0, shotsIndex()); }

    QTemporaryDir tempDir_;
    QString existingClipPath_;
    QString missingClipPath_;
    Artifact::ArtifactProjectPtr project_;
    std::unique_ptr<Artifact::ArtifactProjectModel> model_;
    std::unique_ptr<Artifact::ProjectFilterProxyModel> proxy_;
};

TEST_F(ArtifactProjectViewContractTest, ProjectModelExposesProjectTree)
{
    ASSERT_EQ(model_->rowCount(QModelIndex()), 2);
    ASSERT_EQ(model_->columnCount(QModelIndex()), 6);

    EXPECT_EQ(displayName(model_.get(), model_->index(0, 0, QModelIndex())), QStringLiteral("Shots"));
    EXPECT_EQ(displayName(model_.get(), model_->index(1, 0, QModelIndex())), QStringLiteral("Assets"));

    EXPECT_EQ(model_->data(model_->index(0, 0, QModelIndex()), itemTypeRole()).toInt(),
              static_cast<int>(Artifact::eProjectItemType::Folder));
    EXPECT_EQ(model_->data(model_->index(1, 0, QModelIndex()), itemTypeRole()).toInt(),
              static_cast<int>(Artifact::eProjectItemType::Folder));

    ASSERT_EQ(model_->rowCount(shotsIndex()), 2);
    EXPECT_EQ(displayName(model_.get(), childIndex(0)), QStringLiteral("existing_clip.png"));
    EXPECT_EQ(displayName(model_.get(), childIndex(1)), QStringLiteral("missing_clip.mov"));
    EXPECT_EQ(model_->data(childIndex(0), itemTypeRole()).toInt(),
              static_cast<int>(Artifact::eProjectItemType::Footage));

    EXPECT_EQ(model_->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(), QStringLiteral("Name"));
    EXPECT_EQ(model_->headerData(1, Qt::Horizontal, Qt::DisplayRole).toString(), QStringLiteral("Type"));
}

TEST_F(ArtifactProjectViewContractTest, FilterKeepsOnlyMatchingItemTypes)
{
    proxy_->setAdvancedFilter(QStringLiteral("type:folder"), QStringLiteral("All"), false);

    // Both folders match, the footage rows below them do not.
    EXPECT_EQ(proxy_->rowCount(QModelIndex()), 2);
    EXPECT_EQ(proxy_->rowCount(proxy_->index(0, 0, QModelIndex())), 0);
    EXPECT_EQ(proxy_->visibleRowCount(), 2);

    proxy_->setAdvancedFilter(QStringLiteral("type:footage"), QStringLiteral("All"), false);

    // Only the folder that owns matching footage rows survives at the top
    // level, and both footage rows below it stay visible.
    EXPECT_EQ(proxy_->rowCount(QModelIndex()), 1);
    EXPECT_EQ(proxy_->rowCount(proxy_->index(0, 0, QModelIndex())), 2);
    EXPECT_EQ(proxy_->visibleRowCount(), 2);
}

TEST_F(ArtifactProjectViewContractTest, FilterMatchesPlainSearchTerm)
{
    proxy_->setAdvancedFilter(QStringLiteral("existing_clip"), QStringLiteral("All"), false);

    EXPECT_EQ(proxy_->rowCount(QModelIndex()), 1);
    EXPECT_EQ(proxy_->rowCount(proxy_->index(0, 0, QModelIndex())), 1);
    EXPECT_EQ(displayName(proxy_.get(), proxy_->index(0, 0, proxy_->index(0, 0, QModelIndex()))),
              QStringLiteral("existing_clip.png"));
}

TEST_F(ArtifactProjectViewContractTest, FilterMatchesTagTerm)
{
    // "hero" is a tag of the Shots folder, not of its children.
    proxy_->setAdvancedFilter(QStringLiteral("tag:hero"), QStringLiteral("All"), false);

    EXPECT_EQ(proxy_->rowCount(QModelIndex()), 1);
    EXPECT_EQ(proxy_->rowCount(proxy_->index(0, 0, QModelIndex())), 0);
    EXPECT_EQ(proxy_->visibleRowCount(), 1);
}

TEST_F(ArtifactProjectViewContractTest, FilterMarksMissingAndUnusedAssets)
{
    proxy_->setUnusedAssetPaths(QSet<QString>{existingClipPath_});

    const QModelIndex shotsProxy = proxy_->index(0, 0, QModelIndex());
    const QModelIndex existingProxy = proxy_->index(0, 0, shotsProxy);
    const QModelIndex missingProxy = proxy_->index(1, 0, shotsProxy);

    ASSERT_TRUE(existingProxy.isValid());
    ASSERT_TRUE(missingProxy.isValid());

    EXPECT_EQ(proxy_->data(existingProxy, Qt::DisplayRole).toString(),
              QStringLiteral("[Unused] existing_clip.png"));
    EXPECT_EQ(proxy_->data(missingProxy, Qt::DisplayRole).toString(),
              QStringLiteral("[Missing] missing_clip.mov"));

    const QVariant missingColor = proxy_->data(missingProxy, Qt::ForegroundRole);
    ASSERT_TRUE(missingColor.isValid());
    EXPECT_EQ(missingColor.value<QColor>(), QColor(220, 105, 105));
}

TEST_F(ArtifactProjectViewContractTest, ViewExpandsAndCollapsesFolder)
{
    Artifact::ArtifactProjectView view;
    view.resize(900, 600);
    view.setModel(proxy_.get());

    const QModelIndex shotsProxy = proxy_->index(0, 0, QModelIndex());
    const QModelIndex childProxy = proxy_->index(0, 0, shotsProxy);
    ASSERT_TRUE(shotsProxy.isValid());
    ASSERT_TRUE(childProxy.isValid());

    // Collapsed by default: the child row has no geometry.
    EXPECT_FALSE(view.visualRect(shotsProxy).isEmpty());
    EXPECT_TRUE(view.visualRect(childProxy).isEmpty());

    view.expand(shotsProxy);
    EXPECT_FALSE(view.visualRect(childProxy).isEmpty());
    EXPECT_LT(view.visualRect(shotsProxy).top(), view.visualRect(childProxy).top());

    view.collapse(shotsProxy);
    EXPECT_TRUE(view.visualRect(childProxy).isEmpty());
}

TEST_F(ArtifactProjectViewContractTest, ViewHitTestingMatchesVisualRect)
{
    Artifact::ArtifactProjectView view;
    view.resize(900, 600);
    view.setModel(proxy_.get());

    const QModelIndex shotsProxy = proxy_->index(0, 0, QModelIndex());
    ASSERT_TRUE(shotsProxy.isValid());

    const QRect rect = view.visualRect(shotsProxy);
    ASSERT_FALSE(rect.isEmpty());
    EXPECT_EQ(view.indexAt(rect.center()), shotsProxy);

    // The header band is not a selectable row.
    const QPoint aboveFirstRow(rect.left() + 8, rect.top() - 1);
    EXPECT_FALSE(view.indexAt(aboveFirstRow).isValid());
}

TEST_F(ArtifactProjectViewContractTest, ViewSwitchesBetweenListAndTilePresentation)
{
    Artifact::ArtifactProjectView view;
    view.resize(900, 600);
    view.setModel(proxy_.get());

    const QModelIndex shotsProxy = proxy_->index(0, 0, QModelIndex());
    ASSERT_TRUE(shotsProxy.isValid());

    view.setPresentationMode(Artifact::ArtifactProjectView::PresentationMode::List);
    EXPECT_EQ(view.presentationMode(), Artifact::ArtifactProjectView::PresentationMode::List);
    const QRect listRect = view.visualRect(shotsProxy);
    ASSERT_FALSE(listRect.isEmpty());

    view.setPresentationMode(Artifact::ArtifactProjectView::PresentationMode::Tile);
    EXPECT_EQ(view.presentationMode(), Artifact::ArtifactProjectView::PresentationMode::Tile);
    const QRect tileRect = view.visualRect(shotsProxy);
    ASSERT_FALSE(tileRect.isEmpty());

    EXPECT_LT(tileRect.width(), listRect.width());
    EXPECT_GT(tileRect.height(), listRect.height());
    EXPECT_EQ(view.indexAt(tileRect.center()), shotsProxy);
}

TEST_F(ArtifactProjectViewContractTest, ViewColumnWidthChangesRowWidth)
{
    Artifact::ArtifactProjectView view;
    // Keep the viewport narrower than the summed column widths so the row
    // width follows the column layout instead of the viewport.
    view.resize(320, 600);
    view.setModel(proxy_.get());

    const QModelIndex shotsProxy = proxy_->index(0, 0, QModelIndex());
    ASSERT_TRUE(shotsProxy.isValid());
    ASSERT_GT(view.visualRect(shotsProxy).width(), 0);

    view.setColumnWidth(0, 400);
    const int widened = view.visualRect(shotsProxy).width();
    view.setColumnWidth(0, 200);
    const int narrowed = view.visualRect(shotsProxy).width();

    EXPECT_EQ(widened - narrowed, 200);

    // A column narrower than the 40px minimum is clamped, not applied as is.
    view.setColumnWidth(0, 10);
    EXPECT_LT(view.visualRect(shotsProxy).width(), narrowed);
}

TEST_F(ArtifactProjectViewContractTest, ViewSortingIsIgnoredUntilEnabled)
{
    Artifact::ArtifactProjectView view;
    view.resize(900, 600);
    view.setModel(proxy_.get());

    // Source order is insertion order: Shots, then Assets.
    EXPECT_EQ(displayName(proxy_.get(), proxy_->index(0, 0, QModelIndex())), QStringLiteral("Shots"));

    view.setSortingEnabled(false);
    view.sortByColumn(0, Qt::AscendingOrder);
    EXPECT_EQ(displayName(proxy_.get(), proxy_->index(0, 0, QModelIndex())), QStringLiteral("Shots"));

    view.setSortingEnabled(true);
    view.sortByColumn(0, Qt::AscendingOrder);
    EXPECT_EQ(displayName(proxy_.get(), proxy_->index(0, 0, QModelIndex())), QStringLiteral("Assets"));
}

TEST_F(ArtifactProjectViewContractTest, ViewSelectionFollowsCurrentIndex)
{
    Artifact::ArtifactProjectView view;
    view.resize(900, 600);
    view.setModel(proxy_.get());

    const QModelIndex assetsProxy = proxy_->index(1, 0, QModelIndex());
    ASSERT_TRUE(assetsProxy.isValid());

    view.setCurrentIndex(assetsProxy);

    ASSERT_NE(view.selectionModel(), nullptr);
    EXPECT_EQ(view.currentIndex(), assetsProxy);
    EXPECT_EQ(view.selectionModel()->selectedRows(0).size(), 1);
    EXPECT_EQ(view.selectionModel()->selectedRows(0).first(), assetsProxy);
}

TEST_F(ArtifactProjectViewContractTest, ManagerWidgetExposesProjectView)
{
    Artifact::ArtifactProjectManagerWidget panel;
    panel.resize(900, 600);

    ASSERT_NE(panel.projectView(), nullptr);
    panel.projectView()->setPresentationMode(Artifact::ArtifactProjectView::PresentationMode::Tile);
    EXPECT_EQ(panel.projectView()->presentationMode(),
              Artifact::ArtifactProjectView::PresentationMode::Tile);
}
