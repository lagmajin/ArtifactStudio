# Milestone: Asset Browser Advanced Sort (M-AB-11)

**マイルストーンID**: M-AB-11
**作成日**: 2026-06-28
**優先度**: P1 (High)
**推定工数**: 1-2日
**カテゴリ**: Asset Browser / UX / Sorting
**状態**: Partial implementation（単一キー／natural name／固定複合 preset／sort key・方向の設定保存を実装、個別方向の multi-key／custom order／runtime 検証 pending）
**依存**: M-AB (Asset Browser base)

---

## 目的

アセットブラウザーに高度なソート機能を実装する。ユーザーは複数のキー（名前、種類、サイズ、日付、ステータスなど）で同時にソートできるようになり、大量のアセットを効率的に整理できる。

---

## 背景

### 現状
- 既存の`MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md` (Phase 2.4) で「サイズ/日付ソート」の概念が言及されている
- `ArtifactAssetBrowser.cppm`の`applyFilters()`でアイテムをフィルタリングしているが、ソートはシンプルな単一キー
- `AssetMenuItem`構造体には`name`, `type`, `fileSizeBytes`, `lastModified`などのソート可能なフィールドがある
- 現在の実装では、1つのフィールドでのみソート可能
- 名前ソートは `file2` が `file10` より前になる natural order を使用する。

### 2026-07-29 Implementation Loop

- `ArtifactAssetBrowser` の既存 name sort に数値 run-aware の natural comparator を追加。
- date／size／type sort が同値の場合も、natural name を安定した副次キーとして使用。
- natural name まで同値の場合は asset path を最終タイブレークにして、再読込時の並び揺れを抑制。
- `Type → Name`、`Date → Name`、`Size → Name` の複合 sort preset を既存 combo に追加し、既存の昇順／降順トグルを適用。
- 選択中の sort key と方向を `QSettings` に保存し、Asset Browser 再生成時に復元する。
- UI／既存 sort key／sequence frame の親子順序は変更せず、連番素材の表示順だけを改善。
- 個別方向、保存可能な preset、custom order、drag sort は未完了。複合 sort の固定候補は実装済み。

### 要件
- **Multi-key Sort**: 複数のフィールドで同時にソート（例: タイプ→名前→日付）
- **Sort Direction**: 各フィールドに対して昇順/降順を個別に設定
- **Sort Presets**: よく使うソート条件をプリセットとして保存
- **Custom Sort**: ユーザー定義のソート順序
- **Natural Sort**: 文字列を自然順序でソート（"file1", "file2", "file10"の順）
- **Sequence Sort**: シーケンスアセットをフレーム番号順にソート
- **Drag & Drop Sort**: 手動でアイテムを並べ替え

### ユースケース
1. 同じ種類のアセットを名前順に並べ、かつサイズ順に副次ソート
2. 最新のアセットを上位に表示（日付降順）
3. 大きなファイルを優先的に表示（サイズ降順）
4. シーケンスをフレーム番号順に並べる
5. カスタムな順序でアセットを並べる

---

## 対象ファイル一覧

| 区分 | ファイル | 変更内容 |
|---|---|---|
| **新規** | `ArtifactCore/include/Asset/AssetSortCriteria.ixx` | ソート条件データ構造 |
| **新規** | `ArtifactCore/src/Asset/AssetSortCriteria.cppm` | 実装 |
| **新規** | `ArtifactCore/include/Asset/AssetSorter.ixx` | ソート実行クラス |
| **新規** | `ArtifactCore/src/Asset/AssetSorter.cppm` | ソートロジック実装 |
| **新規** | `Artifact/src/Widgets/Asset/SortSettingsDialog.cppm` | ソート設定ダイアログ |
| **新規** | `Artifact/include/Widgets/Asset/SortSettingsDialog.ixx` | ヘッダー |
| **新規** | `Artifact/src/Widgets/Asset/SortPresetsManager.cppm` | ソートプリセット管理 |
| **新規** | `Artifact/include/Widgets/Asset/SortPresetsManager.ixx` | ヘッダー |
| **変更** | `Artifact/src/Asset/AssetMenuModel.cppm` | ソートロジック統合 |
| **変更** | `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` | ソートUI統合 |
| **変更** | `Artifact/include/Widgets/ArtifactAssetBrowser.ixx` | シグナル/スロット追加 |

---

## 変更詳細

### 1. AssetSortCriteria - ソート条件データ構造

**新規ファイル**: `ArtifactCore/include/Asset/AssetSortCriteria.ixx`

```cpp
module;
#include <string>
#include <vector>
#include <optional>

#include <QMetaType>

export module Asset.SortCriteria;

import std;

namespace ArtifactCore {

/**
 * @brief ソートフィールドを表す列挙型
 */
export enum class SortField {
    Name,           // ファイル名
    Type,           // ファイルタイプ（画像、動画、音声、シーケンス、フォルダなど）
    Size,           // ファイルサイズ
    ModifiedDate,   // 更新日時
    CreatedDate,    // 作成日時
    Status,         // ステータス（Imported, Missing, Unused, Favorite）
    SequenceFrame,  // シーケンスのフレーム番号
    CustomOrder     // ユーザー定義の順序
};

/**
 * @brief ソート方向を表す列挙型
 */
export enum class SortDirection {
    Ascending,      // 昇順
    Descending      // 降順
};

/**
 * @brief 単一のソート条件
 */
export struct SortCriterion {
    SortField field;
    SortDirection direction = SortDirection::Ascending;
    bool enabled = true;
    
    bool operator==(const SortCriterion& other) const;
};

/**
 * @brief 複数のソート条件のコレクション
 *優先度順にソートが適用される
 */
export struct SortCriteria {
    std::vector<SortCriterion> criteria;
    
    // 便利なファクトリメソッド
    static SortCriteria byName(SortDirection dir = SortDirection::Ascending);
    static SortCriteria byType(SortDirection dir = SortDirection::Ascending);
    static SortCriteria bySize(SortDirection dir = SortDirection::Descending);
    static SortCriteria byDate(SortDirection dir = SortDirection::Descending);
    static SortCriteria byStatus(SortDirection dir = SortDirection::Ascending);
    static SortCriteria naturalOrder();
    
    bool operator==(const SortCriteria& other) const;
};

// QMetaType登録用
} // namespace ArtifactCore

Q_DECLARE_METATYPE(ArtifactCore::SortField)
Q_DECLARE_METATYPE(ArtifactCore::SortDirection)
Q_DECLARE_METATYPE(ArtifactCore::SortCriterion)
Q_DECLARE_METATYPE(ArtifactCore::SortCriteria)
```

**新規ファイル**: `ArtifactCore/src/Asset/AssetSortCriteria.cppm`

```cpp
module;
#include <vector>
#include <algorithm>

import Asset.SortCriteria;

namespace ArtifactCore {

bool SortCriterion::operator==(const SortCriterion& other) const {
    return field == other.field && 
           direction == other.direction && 
           enabled == other.enabled;
}

SortCriteria SortCriteria::byName(SortDirection dir) {
    return SortCriteria{.criteria = {{SortField::Name, dir, true}}};
}

SortCriteria SortCriteria::byType(SortDirection dir) {
    return SortCriteria{.criteria = {{SortField::Type, dir, true}}};
}

SortCriteria SortCriteria::bySize(SortDirection dir) {
    return SortCriteria{.criteria = {{SortField::Size, dir, true}}};
}

SortCriteria SortCriteria::byDate(SortDirection dir) {
    return SortCriteria{.criteria = {{SortField::ModifiedDate, dir, true}}};
}

SortCriteria SortCriteria::byStatus(SortDirection dir) {
    return SortCriteria{.criteria = {{SortField::Status, dir, true}}};
}

SortCriteria SortCriteria::naturalOrder() {
    return SortCriteria{.criteria = {
        {SortField::Type, SortDirection::Ascending, true},
        {SortField::Name, SortDirection::Ascending, true}
    }};
}

bool SortCriteria::operator==(const SortCriteria& other) const {
    if (criteria.size() != other.criteria.size()) return false;
    for (size_t i = 0; i < criteria.size(); ++i) {
        if (!(criteria[i] == other.criteria[i])) return false;
    }
    return true;
}

} // namespace ArtifactCore
```

### 2. AssetSorter - ソート実行クラス

**新規ファイル**: `ArtifactCore/include/Asset/AssetSorter.ixx`

```cpp
module;
#include <vector>
#include <functional>

#include <QStringList>

export module Asset.Sorter;

import Asset.SortCriteria;
import AssetMenuModel;

namespace ArtifactCore {

/**
 * @brief AssetMenuItemのリストをソートするクラス
 */
export class AssetSorter {
public:
    // ソートを実行
    static void sort(QList<Artifact::AssetMenuItem>& items, 
                     const SortCriteria& criteria);
    
    // 比較関数を生成
    static std::function<bool(const Artifact::AssetMenuItem&, 
                              const Artifact::AssetMenuItem&)> 
    createComparator(const SortCriteria& criteria);
    
    // 単一フィールドの比較関数
    static int compareByField(const Artifact::AssetMenuItem& a, 
                              const Artifact::AssetMenuItem& b,
                              SortField field);
    
    // 自然順序ソート用
    static int naturalCompare(const QString& a, const QString& b);
    
    // シーケンス用ソート
    static int sequenceCompare(const Artifact::AssetMenuItem& a, 
                              const Artifact::AssetMenuItem& b);
    
private:
    AssetSorter() = delete;
    ~AssetSorter() = delete;
};

} // namespace ArtifactCore
```

**新規ファイル**: `ArtifactCore/src/Asset/AssetSorter.cppm`

```cpp
module;
#include <vector>
#include <algorithm>
#include <functional>
#include <string>

#include <QString>
#include <QDateTime>
#include <QCollator>
#include <QRegularExpression>

import Asset.Sorter;
import Asset.SortCriteria;
import AssetMenuModel;

namespace ArtifactCore {

// 自然順序ソート用の正規表現
static const QRegularExpression kNaturalSortRx(
    QStringLiteral(R"((\d+)|(\D+)))"));

static int naturalCompareStrings(const QString& a, const QString& b) {
    // 自然順序ソートを実装
    // 数字を数値として比較、文字列を通常通り比較
    
    QRegularExpressionMatchIterator itA = kNaturalSortRx.globalMatch(a);
    QRegularExpressionMatchIterator itB = kNaturalSortRx.globalMatch(b);
    
    while (itA.hasNext() && itB.hasNext()) {
        auto matchA = itA.next();
        auto matchB = itB.next();
        
        QString strA = matchA.captured(0);
        QString strB = matchB.captured(0);
        
        // 数字の場合
        if (!matchA.captured(2).isEmpty() && !matchB.captured(2).isEmpty()) {
            bool okA, okB;
            int numA = strA.toInt(&okA);
            int numB = strB.toInt(&okB);
            
            if (okA && okB) {
                if (numA != numB) {
                    return numA - numB;
                }
            } else {
                // 数字としてパースできなかった場合
                int cmp = QString::compare(strA, strB);
                if (cmp != 0) return cmp;
            }
        } else {
            // 文字列の場合
            int cmp = QString::compare(strA, strB, Qt::CaseInsensitive);
            if (cmp != 0) return cmp;
        }
    }
    
    // 片方だけマッチが終わった場合
    if (itA.hasNext()) return 1;
    if (itB.hasNext()) return -1;
    
    return 0;
}

int AssetSorter::naturalCompare(const QString& a, const QString& b) {
    return naturalCompareStrings(a, b);
}

int AssetSorter::compareByField(
    const Artifact::AssetMenuItem& a, 
    const Artifact::AssetMenuItem& b,
    SortField field) {
    
    switch (field) {
        case SortField::Name: {
            // フォルダを先に表示
            if (a.isFolder != b.isFolder) {
                return a.isFolder ? -1 : 1;
            }
            
            // 自然順序ソート
            QString nameA = QString::fromUtf8(a.name.c_str());
            QString nameB = QString::fromUtf8(b.name.c_str());
            return naturalCompare(nameA, nameB);
        }
        
        case SortField::Type: {
            QString typeA = QString::fromUtf8(a.type.c_str());
            QString typeB = QString::fromUtf8(b.type.c_str());
            return QString::compare(typeA, typeB, Qt::CaseInsensitive);
        }
        
        case SortField::Size: {
            if (a.fileSizeBytes != b.fileSizeBytes) {
                return a.fileSizeBytes < b.fileSizeBytes ? -1 : 1;
            }
            return 0;
        }
        
        case SortField::ModifiedDate: {
            if (a.lastModified != b.lastModified) {
                return a.lastModified < b.lastModified ? -1 : 1;
            }
            return 0;
        }
        
        case SortField::CreatedDate: {
            // 作成日時はAssetMenuItemに現在ないため、更新日時を使用
            if (a.lastModified != b.lastModified) {
                return a.lastModified < b.lastModified ? -1 : 1;
            }
            return 0;
        }
        
        case SortField::Status: {
            // ステータスを比較（Favorites > Imported > Unused > Missing）
            auto getStatusValue = [](const Artifact::AssetMenuItem& item) {
                QString type = QString::fromUtf8(item.type.c_str());
                if (type.contains("Favorite")) return 0;
                if (type.contains("Imported")) return 1;
                if (type.contains("Unused")) return 2;
                if (type.contains("Missing")) return 3;
                return 4;
            };
            
            int statusA = getStatusValue(a);
            int statusB = getStatusValue(b);
            if (statusA != statusB) {
                return statusA - statusB;
            }
            return 0;
        }
        
        case SortField::SequenceFrame: {
            // シーケンスの場合はフレーム番号でソート
            if (a.isSequence && b.isSequence) {
                if (a.sequenceStartFrame != b.sequenceStartFrame) {
                    return a.sequenceStartFrame - b.sequenceStartFrame;
                }
            }
            return 0;
        }
        
        case SortField::CustomOrder: {
            // カスタム順序（将来の拡張用）
            return 0;
        }
    }
    
    return 0;
}

void AssetSorter::sort(QList<Artifact::AssetMenuItem>& items, 
                       const SortCriteria& criteria) {
    auto comparator = createComparator(criteria);
    std::sort(items.begin(), items.end(), comparator);
}

std::function<bool(const Artifact::AssetMenuItem&, 
                   const Artifact::AssetMenuItem&)> 
AssetSorter::createComparator(const SortCriteria& criteria) {
    return [criteria](const Artifact::AssetMenuItem& a, 
                     const Artifact::AssetMenuItem& b) {
        for (const auto& criterion : criteria.criteria) {
            if (!criterion.enabled) continue;
            
            int cmp = compareByField(a, b, criterion.field);
            
            // 降順の場合は比較結果を反転
            if (criterion.direction == SortDirection::Descending) {
                cmp = -cmp;
            }
            
            if (cmp != 0) {
                return cmp < 0;
            }
        }
        return false;
    };
}

} // namespace ArtifactCore
```

### 3. SortSettingsDialog - ソート設定ダイアログ

**新規ファイル**: `Artifact/include/Widgets/Asset/SortSettingsDialog.ixx`

```cpp
module;
#include <QDialog>
#include <memory>

#include <QList>

export module Widgets.Asset.SortSettingsDialog;

import Asset.SortCriteria;

namespace Artifact {

class SortSettingsDialogImpl;

/**
 * @brief ソート設定を行うダイアログ
 *複数のソートキーを優先度順に設定可能
 */
export class SortSettingsDialog : public QDialog {
    Q_OBJECT
    W_OBJECT(SortSettingsDialog)

public:
    explicit SortSettingsDialog(QWidget* parent = nullptr);
    ~SortSettingsDialog();
    
    // ソート条件を設定/取得
    void setSortCriteria(const ArtifactCore::SortCriteria& criteria);
    ArtifactCore::SortCriteria getSortCriteria() const;
    
    // プリセットを設定/取得
    static void savePreset(const QString& name, 
                         const ArtifactCore::SortCriteria& criteria);
    static ArtifactCore::SortCriteria loadPreset(const QString& name);
    static QStringList getPresetNames();
    static void removePreset(const QString& name);
    
signals:
    void sortCriteriaChanged(const ArtifactCore::SortCriteria& criteria);
    
public slots:
    void accept() override;
    
private:
    void setupUi();
    void addCriterionRow();
    void updateCriterionRow(int row, const ArtifactCore::SortCriterion& criterion);
    ArtifactCore::SortCriterion getCriterionFromRow(int row) const;
    void loadPresets();
    
    std::unique_ptr<SortSettingsDialogImpl> impl_;
};

} // namespace Artifact
```

**新規ファイル**: `Artifact/src/Widgets/Asset/SortSettingsDialog.cppm`

```cpp
module;
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QHeaderView>
#include <QToolButton>
#include <QMessageBox>
#include <QSettings>
#include <QMenu>

import Widgets.Asset.SortSettingsDialog;
import Asset.SortCriteria;

namespace Artifact {

class SortSettingsDialogImpl {
public:
    QTableWidget* criteriaTable_ = nullptr;
    QComboBox* fieldCombo_ = nullptr;
    QComboBox* directionCombo_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* removeButton_ = nullptr;
    QPushButton* upButton_ = nullptr;
    QPushButton* downButton_ = nullptr;
    QComboBox* presetCombo_ = nullptr;
    QPushButton* savePresetButton_ = nullptr;
    QPushButton* deletePresetButton_ = nullptr;
    
    QList<ArtifactCore::SortCriterion> criteria_;
};

SortSettingsDialog::SortSettingsDialog(QWidget* parent)
    : QDialog(parent), impl_(std::make_unique<Impl>()) {
    setWindowTitle(tr("Sort Settings"));
    setMinimumSize(500, 400);
    
    setupUi();
}

SortSettingsDialog::~SortSettingsDialog() = default;

void SortSettingsDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(11, 11, 11, 11);
    
    // プリセット
    auto* presetLayout = new QHBoxLayout();
    presetLayout->setSpacing(4);
    
    presetLayout->addWidget(new QLabel(tr("Preset:"), this));
    
    impl_->presetCombo_ = new QComboBox(this);
    presetLayout->addWidget(impl_->presetCombo_);
    
    impl_->savePresetButton_ = new QPushButton(tr("Save"), this);
    connect(impl_->savePresetButton_, &QPushButton::clicked, [this]() {
        QString name = QInputDialog::getText(
            this, tr("Save Preset"), tr("Preset name:"));
        if (!name.isEmpty()) {
            savePreset(name, getSortCriteria());
            loadPresets();
        }
    });
    presetLayout->addWidget(impl_->savePresetButton_);
    
    impl_->deletePresetButton_ = new QPushButton(tr("Delete"), this);
    connect(impl_->deletePresetButton_, &QPushButton::clicked, [this]() {
        QString name = impl_->presetCombo_->currentText();
        if (!name.isEmpty()) {
            QMessageBox::StandardButton result = QMessageBox::question(
                this, tr("Delete Preset"),
                tr("Delete preset '%1'?").arg(name));
            if (result == QMessageBox::Yes) {
                removePreset(name);
                loadPresets();
            }
        }
    });
    presetLayout->addWidget(impl_->deletePresetButton_);
    
    connect(impl_->presetCombo_, &QComboBox::currentTextChanged, [this](const QString& text) {
        if (!text.isEmpty()) {
            auto criteria = loadPreset(text);
            setSortCriteria(criteria);
        }
    });
    
    layout->addLayout(presetLayout);
    
    // クライテリアテーブル
    impl_->criteriaTable_ = new QTableWidget(this);
    impl_->criteriaTable_->setColumnCount(3);
    impl_->criteriaTable_->setHorizontalHeaderLabels({
        tr("Field"), tr("Direction"), tr("Enabled")
    });
    impl_->criteriaTable_->horizontalHeader()->setStretchLastSection(false);
    impl_->criteriaTable_->verticalHeader()->setVisible(false);
    impl_->criteriaTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    impl_->criteriaTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    
    layout->addWidget(impl_->criteriaTable_);
    
    // 追加コントロール
    auto* addLayout = new QHBoxLayout();
    addLayout->setSpacing(4);
    
    addLayout->addWidget(new QLabel(tr("Add:"), this));
    
    impl_->fieldCombo_ = new QComboBox(this);
    impl_->fieldCombo_->addItems({
        tr("Name"),
        tr("Type"),
        tr("Size"),
        tr("Modified Date"),
        tr("Created Date"),
        tr("Status"),
        tr("Sequence Frame"),
        tr("Custom Order")
    });
    addLayout->addWidget(impl_->fieldCombo_);
    
    impl_->directionCombo_ = new QComboBox(this);
    impl_->directionCombo_->addItems({
        tr("Ascending"),
        tr("Descending")
    });
    addLayout->addWidget(impl_->directionCombo_);
    
    impl_->addButton_ = new QPushButton(tr("Add"), this);
    connect(impl_->addButton_, &QPushButton::clicked, this, &SortSettingsDialog::addCriterionRow);
    addLayout->addWidget(impl_->addButton_);
    
    layout->addLayout(addLayout);
    
    // 操作ボタン
    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(4);
    
    impl_->upButton_ = new QPushButton(tr("Up"), this);
    connect(impl_->upButton_, &QPushButton::clicked, [this]() {
        int row = impl_->criteriaTable_->currentRow();
        if (row > 0) {
            impl_->criteriaTable_->insertRow(row - 1);
            for (int col = 0; col < impl_->criteriaTable_->columnCount(); ++col) {
                auto* item = impl_->criteriaTable_->takeItem(row, col);
                impl_->criteriaTable_->setItem(row - 1, col, item);
            }
            impl_->criteriaTable_->removeRow(row + 1);
            impl_->criteriaTable_->selectRow(row - 1);
        }
    });
    actionLayout->addWidget(impl_->upButton_);
    
    impl_->downButton_ = new QPushButton(tr("Down"), this);
    connect(impl_->downButton_, &QPushButton::clicked, [this]() {
        int row = impl_->criteriaTable_->currentRow();
        if (row < impl_->criteriaTable_->rowCount() - 1) {
            impl_->criteriaTable_->insertRow(row + 2);
            for (int col = 0; col < impl_->criteriaTable_->columnCount(); ++col) {
                auto* item = impl_->criteriaTable_->takeItem(row, col);
                impl_->criteriaTable_->setItem(row + 2, col, item);
            }
            impl_->criteriaTable_->removeRow(row);
            impl_->criteriaTable_->selectRow(row + 1);
        }
    });
    actionLayout->addWidget(impl_->downButton_);
    
    impl_->removeButton_ = new QPushButton(tr("Remove"), this);
    connect(impl_->removeButton_, &QPushButton::clicked, [this]() {
        int row = impl_->criteriaTable_->currentRow();
        if (row >= 0) {
            impl_->criteriaTable_->removeRow(row);
        }
    });
    actionLayout->addWidget(impl_->removeButton_);
    
    actionLayout->addStretch();
    layout->addLayout(actionLayout);
    
    // OK/Cancelボタン
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    auto* cancelButton = new QPushButton(tr("Cancel"), this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);
    
    auto* okButton = new QPushButton(tr("OK"), this);
    okButton->setDefault(true);
    connect(okButton, &QPushButton::clicked, this, &SortSettingsDialog::accept);
    buttonLayout->addWidget(okButton);
    
    layout->addLayout(buttonLayout);
    
    // 初期状態
    addCriterionRow();
    loadPresets();
}

void SortSettingsDialog::loadPresets() {
    impl_->presetCombo_->clear();
    impl_->presetCombo_->addItem(tr("Custom"));
    impl_->presetCombo_->addItems(getPresetNames());
}

void SortSettingsDialog::addCriterionRow() {
    ArtifactCore::SortCriterion criterion;
    criterion.field = static_cast<ArtifactCore::SortField>(
        impl_->fieldCombo_->currentIndex());
    criterion.direction = static_cast<ArtifactCore::SortDirection>(
        impl_->directionCombo_->currentIndex());
    criterion.enabled = true;
    
    int row = impl_->criteriaTable_->rowCount();
    impl_->criteriaTable_->insertRow(row);
    updateCriterionRow(row, criterion);
    impl_->criteriaTable_->selectRow(row);
    
    // 最初の行を追加した場合は、ダブルクリックで編集可能に
    if (row == 0) {
        impl_->criteriaTable_->setEditTriggers(QAbstractItemView::DoubleClicked);
    }
}

void SortSettingsDialog::updateCriterionRow(int row, const ArtifactCore::SortCriterion& criterion) {
    // Field
    auto* fieldItem = new QTableWidgetItem();
    fieldItem->setText(impl_->fieldCombo_->itemText(static_cast<int>(criterion.field)));
    fieldItem->setData(Qt::UserRole, static_cast<int>(criterion.field));
    impl_->criteriaTable_->setItem(row, 0, fieldItem);
    
    // Direction
    auto* directionItem = new QTableWidgetItem();
    directionItem->setText(impl_->directionCombo_->itemText(static_cast<int>(criterion.direction)));
    directionItem->setData(Qt::UserRole, static_cast<int>(criterion.direction));
    impl_->criteriaTable_->setItem(row, 1, directionItem);
    
    // Enabled
    auto* enabledItem = new QTableWidgetItem();
    enabledItem->setCheckState(criterion.enabled ? Qt::Checked : Qt::Unchecked);
    enabledItem->setTextAlignment(Qt::AlignCenter);
    enabledItem->setData(Qt::UserRole, criterion.enabled);
    impl_->criteriaTable_->setItem(row, 2, enabledItem);
}

ArtifactCore::SortCriterion SortSettingsDialog::getCriterionFromRow(int row) const {
    ArtifactCore::SortCriterion criterion;
    
    auto* fieldItem = impl_->criteriaTable_->item(row, 0);
    if (fieldItem) {
        criterion.field = static_cast<ArtifactCore::SortField>(
            fieldItem->data(Qt::UserRole).toInt());
    }
    
    auto* directionItem = impl_->criteriaTable_->item(row, 1);
    if (directionItem) {
        criterion.direction = static_cast<ArtifactCore::SortDirection>(
            directionItem->data(Qt::UserRole).toInt());
    }
    
    auto* enabledItem = impl_->criteriaTable_->item(row, 2);
    if (enabledItem) {
        criterion.enabled = enabledItem->checkState() == Qt::Checked;
    }
    
    return criterion;
}

void SortSettingsDialog::setSortCriteria(const ArtifactCore::SortCriteria& criteria) {
    impl_->criteriaTable_->setRowCount(0);
    
    for (const auto& criterion : criteria.criteria) {
        int row = impl_->criteriaTable_->rowCount();
        impl_->criteriaTable_->insertRow(row);
        updateCriterionRow(row, criterion);
    }
    
    if (impl_->criteriaTable_->rowCount() == 0) {
        addCriterionRow();
    }
}

ArtifactCore::SortCriteria SortSettingsDialog::getSortCriteria() const {
    ArtifactCore::SortCriteria criteria;
    
    for (int row = 0; row < impl_->criteriaTable_->rowCount(); ++row) {
        criteria.criteria.append(getCriterionFromRow(row));
    }
    
    return criteria;
}

void SortSettingsDialog::accept() {
    emit sortCriteriaChanged(getSortCriteria());
    QDialog::accept();
}

// 静的メソッド
void SortSettingsDialog::savePreset(const QString& name, 
                                  const ArtifactCore::SortCriteria& criteria) {
    QSettings settings("Artifact", "AssetBrowser");
    settings.beginGroup("SortPresets");
    settings.beginGroup(name);
    
    // シリアライズ
    settings.setValue("count", static_cast<int>(criteria.criteria.size()));
    for (int i = 0; i < criteria.criteria.size(); ++i) {
        const auto& c = criteria.criteria[i];
        settings.beginGroup(QString::number(i));
        settings.setValue("field", static_cast<int>(c.field));
        settings.setValue("direction", static_cast<int>(c.direction));
        settings.setValue("enabled", c.enabled);
        settings.endGroup();
    }
    
    settings.endGroup();
    settings.endGroup();
}

ArtifactCore::SortCriteria SortSettingsDialog::loadPreset(const QString& name) {
    ArtifactCore::SortCriteria criteria;
    
    QSettings settings("Artifact", "AssetBrowser");
    settings.beginGroup("SortPresets");
    settings.beginGroup(name);
    
    int count = settings.value("count", 0).toInt();
    for (int i = 0; i < count; ++i) {
        settings.beginGroup(QString::number(i));
        ArtifactCore::SortCriterion c;
        c.field = static_cast<ArtifactCore::SortField>(
            settings.value("field", 0).toInt());
        c.direction = static_cast<ArtifactCore::SortDirection>(
            settings.value("direction", 0).toInt());
        c.enabled = settings.value("enabled", true).toBool();
        criteria.criteria.append(c);
        settings.endGroup();
    }
    
    settings.endGroup();
    settings.endGroup();
    
    return criteria;
}

QStringList SortSettingsDialog::getPresetNames() {
    QSettings settings("Artifact", "AssetBrowser");
    settings.beginGroup("SortPresets");
    QStringList names = settings.childGroups();
    settings.endGroup();
    return names;
}

void SortSettingsDialog::removePreset(const QString& name) {
    QSettings settings("Artifact", "AssetBrowser");
    settings.beginGroup("SortPresets");
    settings.remove(name);
    settings.endGroup();
}

} // namespace Artifact
```

### 4. SortPresetsManager - ソートプリセット管理

**新規ファイル**: `Artifact/include/Widgets/Asset/SortPresetsManager.ixx`

```cpp
module;
#include <QString>
#include <QStringList>
#include <vector>

#include <memory>

export module Widgets.Asset.SortPresetsManager;

import Asset.SortCriteria;

namespace Artifact {

class SortPresetsManagerImpl;

/**
 * @brief ソートプリセットを管理するシングルトンクラス
 *プリセットの保存、読み込み、削除を一元管理
 */
export class SortPresetsManager {
public:
    static SortPresetsManager& instance();
    
    // プリセット操作
    void savePreset(const QString& name, const ArtifactCore::SortCriteria& criteria);
    ArtifactCore::SortCriteria loadPreset(const QString& name) const;
    QStringList getPresetNames() const;
    void removePreset(const QString& name);
    void renamePreset(const QString& oldName, const QString& newName);
    
    // 組み込みプリセット
    static const ArtifactCore::SortCriteria& getNaturalOrderPreset();
    static const ArtifactCore::SortCriteria& getByNamePreset();
    static const ArtifactCore::SortCriteria& getByTypePreset();
    static const ArtifactCore::SortCriteria& getBySizePreset();
    static const ArtifactCore::SortCriteria& getByDatePreset();
    static const ArtifactCore::SortCriteria& getByStatusPreset();
    
    // 通知
    void setPresetsChangedCallback(std::function<void()> callback);
    
private:
    SortPresetsManager();
    ~SortPresetsManager();
    
    SortPresetsManager(const SortPresetsManager&) = delete;
    SortPresetsManager& operator=(const SortPresetsManager&) = delete;
    
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Artifact
```

### 5. AssetMenuModel への統合

**ファイル**: `Artifact/src/Asset/AssetMenuModel.cppm`

```cpp
// Impl クラスにソート機能追加
class AssetMenuModel::Impl {
    // ... 既存フィールド
    
    ArtifactCore::SortCriteria sortCriteria_;
    QList<AssetMenuItem> originalItems_;  // ソート前の元データ
    
public:
    void applySort();
};

// ソートを適用
void AssetMenuModel::Impl::applySort() {
    if (originalItems_.isEmpty()) return;
    
    // 元データからコピー
    QList<AssetMenuItem> items = originalItems_;
    
    // ソートを実行
    ArtifactCore::AssetSorter::sort(items, sortCriteria_);
    
    // 表示を更新
    // 実装は省略 - Modelのデータを更新
}

// アイテムを設定（ソート前のデータを保存）
void AssetMenuModel::setItems(const QList<AssetMenuItem>& items) {
    impl_->originalItems_ = items;
    impl_->applySort();
}

// アイテムを追加
void AssetMenuModel::addItem(const AssetMenuItem& item) {
    impl_->originalItems_.append(item);
    impl_->applySort();
}

// ソート条件を設定
void AssetMenuModel::setSortCriteria(const ArtifactCore::SortCriteria& criteria) {
    impl_->sortCriteria_ = criteria;
    impl_->applySort();
}

// 現在のソート条件を取得
ArtifactCore::SortCriteria AssetMenuModel::sortCriteria() const {
    return impl_->sortCriteria_;
}
```

### 6. ArtifactAssetBrowser への統合

**ファイル**: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

```cpp
// Impl クラスにソート機能追加
class ArtifactAssetBrowser::Impl {
    // ... 既存フィールド
    
    // ソート設定
    QAction* sortSettingsAction_ = nullptr;
    QAction* sortByNameAction_ = nullptr;
    QAction* sortByTypeAction_ = nullptr;
    QAction* sortBySizeAction_ = nullptr;
    QAction* sortByDateAction_ = nullptr;
    QAction* sortByStatusAction_ = nullptr;
    QMenu* sortMenu_ = nullptr;
    
    SortSettingsDialog* sortSettingsDialog_ = nullptr;
};

// ソートメニューをセットアップ
void ArtifactAssetBrowser::Impl::setupSortMenu() {
    // トールバーまたはメニューにソートメニューを追加
    sortMenu_ = new QMenu(tr("Sort"), toolbar_);
    
    // プリセットソート
    sortByNameAction_ = sortMenu_->addAction(tr("By Name"));
    connect(sortByNameAction_, &QAction::triggered, [this]() {
        assetModel_->setSortCriteria(
            ArtifactCore::SortCriteria::byName());
    });
    
    sortByTypeAction_ = sortMenu_->addAction(tr("By Type"));
    connect(sortByTypeAction_, &QAction::triggered, [this]() {
        assetModel_->setSortCriteria(
            ArtifactCore::SortCriteria::byType());
    });
    
    sortBySizeAction_ = sortMenu_->addAction(tr("By Size"));
    connect(sortBySizeAction_, &QAction::triggered, [this]() {
        assetModel_->setSortCriteria(
            ArtifactCore::SortCriteria::bySize());
    });
    
    sortByDateAction_ = sortMenu_->addAction(tr("By Date"));
    connect(sortByDateAction_, &QAction::triggered, [this]() {
        assetModel_->setSortCriteria(
            ArtifactCore::SortCriteria::byDate());
    });
    
    sortByStatusAction_ = sortMenu_->addAction(tr("By Status"));
    connect(sortByStatusAction_, &QAction::triggered, [this]() {
        assetModel_->setSortCriteria(
            ArtifactCore::SortCriteria::byStatus());
    });
    
    sortMenu_->addSeparator();
    
    // 自然順序
    auto* naturalOrderAction = sortMenu_->addAction(tr("Natural Order"));
    connect(naturalOrderAction, &QAction::triggered, [this]() {
        assetModel_->setSortCriteria(
            ArtifactCore::SortCriteria::naturalOrder());
    });
    
    sortMenu_->addSeparator();
    
    // カスタムソート
    sortSettingsAction_ = sortMenu_->addAction(tr("Custom Sort..."));
    connect(sortSettingsAction_, &QAction::triggered, [this]() {
        if (!sortSettingsDialog_) {
            sortSettingsDialog_ = new SortSettingsDialog(self_);
            connect(sortSettingsDialog_, &SortSettingsDialog::sortCriteriaChanged,
                    [this](const ArtifactCore::SortCriteria& criteria) {
                assetModel_->setSortCriteria(criteria);
            });
        }
        sortSettingsDialog_->setSortCriteria(assetModel_->sortCriteria());
        sortSettingsDialog_->show();
    });
    
    // トールバーにメニューを追加
    auto* sortButton = new QToolButton(toolbar_);
    sortButton->setMenu(sortMenu_);
    sortButton->setText(tr("Sort"));
    sortButton->setPopupMode(QToolButton::InstantPopup);
    toolbar_->addWidget(sortButton);
}

// インポート時や初期化時に呼び出す
void ArtifactAssetBrowser::Impl::initializeSort() {
    // 既存のソート状態を復元
    QSettings settings("Artifact", "AssetBrowser");
    QString presetName = settings.value("lastSortPreset", "Natural Order").toString();
    
    if (presetName == "Natural Order") {
        assetModel_->setSortCriteria(ArtifactCore::SortCriteria::naturalOrder());
    } else if (presetName == "By Name") {
        assetModel_->setSortCriteria(ArtifactCore::SortCriteria::byName());
    } else if (presetName == "By Type") {
        assetModel_->setSortCriteria(ArtifactCore::SortCriteria::byType());
    } else if (presetName == "By Size") {
        assetModel_->setSortCriteria(ArtifactCore::SortCriteria::bySize());
    } else if (presetName == "By Date") {
        assetModel_->setSortCriteria(ArtifactCore::SortCriteria::byDate());
    } else if (presetName == "By Status") {
        assetModel_->setSortCriteria(ArtifactCore::SortCriteria::byStatus());
    } else {
        // カスタムプリセット
        auto criteria = SortSettingsDialog::loadPreset(presetName);
        assetModel_->setSortCriteria(criteria);
    }
}

// ソート条件が変更された時の処理
void ArtifactAssetBrowser::Impl::onSortCriteriaChanged() {
    // 現在のソート条件を保存
    auto criteria = assetModel_->sortCriteria();
    
    // プリセット名を決定
    QString presetName;
    if (criteria == ArtifactCore::SortCriteria::naturalOrder()) {
        presetName = "Natural Order";
    } else if (criteria == ArtifactCore::SortCriteria::byName()) {
        presetName = "By Name";
    } else if (criteria == ArtifactCore::SortCriteria::byType()) {
        presetName = "By Type";
    } else if (criteria == ArtifactCore::SortCriteria::bySize()) {
        presetName = "By Size";
    } else if (criteria == ArtifactCore::SortCriteria::byDate()) {
        presetName = "By Date";
    } else if (criteria == ArtifactCore::SortCriteria::byStatus()) {
        presetName = "By Status";
    } else {
        presetName = "Custom";
    }
    
    QSettings settings("Artifact", "AssetBrowser");
    settings.setValue("lastSortPreset", presetName);
}
```

---

## タスク分割 (優先度付き)

### 優先度レベル
- **P0 (Critical)**: コア機能、なければ動作しない
- **P1 (High)**: 主要機能、ないと使い勝手が悪い
- **P2 (Medium)**: 便利機能、あってもなくても動作する
- **P3 (Low)**: 見栄え/UX向上、なくても機能する

### Phase 1: ソート基盤 (0.5日) - **P0**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `SortField`/`SortDirection` 列挙型定義 | P0 | 0.5h | なし | ✅ |
| `SortCriterion`/`SortCriteria` 構造体定義 | P0 | 1h | 上記 | ✅ |
| `AssetSorter` クラス設計 | P0 | 1h | 上記 | ✅ |
| 比較関数実装 | P0 | 2h | 上記 | ✅ |
| 自然順序ソート実装 | P0 | 2h | 上記 | ✅ |

### Phase 2: ソートUI (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `SortSettingsDialog` クラス設計 | P1 | 1h | Phase 1 | ✅ |
| `SortSettingsDialog::Impl` 実装 | P1 | 3h | 上記 | ✅ |
| テーブル表示と編集 | P1 | 2h | 上記 | ✅ |
| アップ/ダウンボタン | P1 | 1h | 上記 | ✅ |
| プリセット保存/読み込み | P1 | 2h | 上記 | ✅ |

### Phase 3: プリセット管理 (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `SortPresetsManager` クラス設計 | P1 | 1h | Phase 2 | ✅ |
| `SortPresetsManager::Impl` 実装 | P1 | 2h | 上記 | ✅ |
| 組み込みプリセット定義 | P1 | 1h | 上記 | ✅ |
| 通知機能 | P2 | 0.5h | 上記 | ✅ |

### Phase 4: AssetMenuModel 統合 (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `AssetMenuModel::Impl` にソート機能追加 | P1 | 2h | Phase 1 | ❌ |
| `applySort()` 実装 | P1 | 2h | 上記 | ❌ |
| ソート前データの保持 | P1 | 1h | 上記 | ❌ |
| ソート条件の設定/取得 | P1 | 1h | 上記 | ❌ |

### Phase 5: AssetBrowser UI統合 (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| ソートメニューのセットアップ | P1 | 2h | Phase 4 | ❌ (UIスレッド) |
| プリセットソートアクション | P1 | 1h | 上記 | ❌ (UIスレッド) |
| カスタムソートダイアログ | P1 | 1h | 上記 | ❌ (UIスレッド) |
| ソート状態の保存/復元 | P2 | 1h | 上記 | ❌ (UIスレッド) |

### 並行作業可能性
- **Phase 1 (Core)**: 独立して並行可能
- **Phase 2 (UI)**: Phase 1完了後、独立して並行可能
- **Phase 3 (Presets)**: Phase 2完了後、独立して並行可能
- **Phase 4 (Model)**: Phase 1完了後、独立して並行可能
- **Phase 5 (UI Integration)**: UIスレッド依存のため、基本的には直列実施

---

## テスト戦略

### 1. 自動テスト (Unit Tests)

#### P0 - 必須テスト (全てパスすること)
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| `compareByField` (Name) | 自動 | 正しくソート | 1h |
| `compareByField` (Type) | 自動 | 正しくソート | 1h |
| `compareByField` (Size) | 自動 | 正しくソート | 1h |
| `compareByField` (ModifiedDate) | 自動 | 正しくソート | 1h |
| `compareByField` (Status) | 自動 | 正しくソート | 1h |
| `compareByField` (SequenceFrame) | 自動 | 正しくソート | 1h |
| `naturalCompare` | 自動 | 自然順序でソート | 2h |
| `createComparator` | 自動 | 複数キーでソート | 2h |

#### P1 - 主要テスト (可能な限りパス)
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| `SortCriteria` シリアライゼーション | 自動 | 正しく保存/復元 | 1h |
| `SortSettingsDialog::setSortCriteria` | 自動 | UIが正しく更新 | 1h |
| `SortSettingsDialog::getSortCriteria` | 自動 | 正しい条件を返す | 1h |
| `SortPresetsManager::savePreset` | 自動 | プリセットが保存 | 1h |
| `SortPresetsManager::loadPreset` | 自動 | プリセットが読み込み | 1h |

#### P2 - 統合テスト
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| AssetMenuModelソート | 手動 | アイテムが正しくソート | 2h |
| 自然順序ソート | 手動 | 自然順序で表示 | 1h |
| 複数キーソート | 手動 | 複数キーでソート | 2h |
| プリセットの切り替え | 手動 | 即座に反映 | 1h |

### 2. 手動テスト (Manual Tests)

#### UXテスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| ソートメニューの操作性 | 直感的で分かりやすい | 1h |
| カスタムソートダイアログ | 分かりやすい | 2h |
| アップ/ダウンボタン | 直感的 | 0.5h |
| プリセットの保存/読み込み | 分かりやすい | 1h |
| ソート状態の永続化 | 起動後も維持 | 0.5h |

#### 視覚的テスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| ソート後のアイテム表示 | 正しく並んでいる | 1h |
| 自然順序ソートの表示 | 期待通り | 1h |
| 複数キーソートの表示 | 期待通り | 1h |
| ソートメニューの表示 | 分かりやすい | 0.5h |

### 3. テスト実行計画

#### Phase 1: Unit Tests (Sorter)
```
日数: 1日
対象: AssetSorter クラス
方法: Google Test / Catch2
実行: CIパイプラインで自動実行
```

#### Phase 2: UI Tests (Dialog)
```
日数: 0.5日
対象: SortSettingsDialog
方法: 手動テスト
実行: QAチーム
```

#### Phase 3: Integration Tests
```
日数: 1日
対象: AssetMenuModel + AssetBrowser
方法: 手動テスト
実行: QAチーム
```

### 4. テスト完了基準
- [ ] P0全てのテストがパス
- [ ] P1の80%以上のテストがパス
- [ ] 重大なバグなし
- [ ] 全てのソートフィールドが正しく動作
- [ ] 自然順序ソートが正しく動作
- [ ] 複数キーソートが正しく動作
- [ ] プリセットが正しく保存/読み込み
- [ ] 全てのテスト項目がパス

---

## 成果物

1. **コア機能**: 高度なソート機能
2. **API**: `AssetSorter`, `AssetSortCriteria` クラス
3. **UIコンポーネント**: `SortSettingsDialog`
4. **管理クラス**: `SortPresetsManager`
5. **統合**: AssetMenuModel + AssetBrowser への統合
6. **プリセット**: 組み込みおよびカスタムプリセット

---

## 依存関係

### 必要な前提条件
- **M-AB (Asset Browser 基盤)** - 基本的なAsset Browser機能
- **`AssetMenuItem`** - アイテム構造
- **`AssetMenuModel`** - アイテム表示モデル

### 連携する機能
- `AssetSorter` - ソートロジック
- `AssetSortCriteria` - ソート条件
- `SortSettingsDialog` - 設定UI
- `SortPresetsManager` - プリセット管理
- `AssetMenuModel` - アイテムモデル
- `ArtifactAssetBrowser` - ホストアプリケーション

### 影響を受ける機能
- アセットリストの表示順序
- フィルタリングとの連携

---

## リスクと対策

### リスク1: ソートパフォーマンス
**内容**: 多数のアセットがある場合のソートパフォーマンス
**対策**:
- 確認があった場合のみソートを実行
- インクリメンタルソートを検討（将来的な拡張）
- ソートは非同期で実行（UIブロックを防ぐ）

### リスク2: 自然順序ソートの複雑さ
**内容**: 自然順序ソートの実装の複雑さ
**対策**:
- 既存のライブラリを利用（QtのQCollator等）
- 正規表現を使用したシンプルな実装
- テストを充実させる

### リスク3: 複数キーソートの優先度
**内容**: 複数キーソートの優先度管理
**対策**:
- UIで明確に優先度を表示
- 各行をドラッグ&ドロップで並べ替え可能に
- 柔軟な並べ替えを提供

### リスク4: プリセットの互換性
**内容**: 古いバージョンとのプリセット互換性
**対策**:
- バージョン管理を導入
- 古いバージョンのプリセットを自動変換
- 互換性のある保存形式

---

## テスト項目

- [ ] 各ソートフィールドで正しくソート
- [ ] 自然順序ソートが正しく動作
- [ ] 複数キーでソートが正しく動作
- [ ] プリセットの保存/読み込み
- [ ] プリセットの削除
- [ ]カスタムソートダイアログの操作
- [ ] アップ/ダウンボタンの動作
- [ ] ソート状態の永続化
- [ ] 性能テスト（1000+アイテム）

---

## 完了基準

- [ ] 全てのソートフィールドが正しく動作
- [ ] 自然順序ソートが正しく動作
- [ ] 複数キーソートが正しく動作
- [ ] プリセットが正しく保存/読み込み
- [ ] UIが直感的で使いやすい
- [ ] パフォーマンスに問題なし
- [ ] 全てのテスト項目がパス
- [ ] UXテストで問題なし
- [ ] ドキュメントが更新

---

## 関連文書

- `Artifact/src/Asset/AssetMenuModel.cppm` - アイテムモデル
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` - Asset Browser
- `Artifact/include/Asset/AssetMenuModel.ixx` - アイテム構造
- `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md` - 元のロードマップ
- `docs/planned/MILESTONES_BACKLOG.md` - マイルストーンバックログ

---

## メモ

- 高度なソート機能はユーザーの作業効率を大幅に向上させる
- Finder/Explorerのソート機能と類似のUXを目指す
- 将来的には、カスタムコンパレータを実装可能にする
- タッチデバイス対応も検討
- 並べ替えアニメーションの追加（将来的な拡張）

## 2026-07-25 実装監査

- `ArtifactAssetBrowser` に Name／Date／Size／Type の単一キー選択、昇順／降順切替、シーケンスの frame 順ソートを確認できる。
- 一方、複数キーを優先順位付きで適用する `AssetSortCriteria`／`AssetSorter`、各キー別方向、ソートプリセット、自然順、カスタム順、専用設定ダイアログは確認できない。
- 手動 Drag & Drop sort、ソート条件のプロジェクト単位永続化、タグ・ステータスとの複合ソートも未完了である。
- よって既存の単一ソートは実装済みだが、Advanced Sort 全体は Partial／未完了と判定する。
