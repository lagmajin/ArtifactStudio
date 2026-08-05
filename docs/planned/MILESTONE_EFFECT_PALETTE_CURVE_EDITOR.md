# MILESTONE: Effect Browser + Curve Editor Completion

**日付**: 2026-08-04
**現状**: エフェクト適用は右クリックメニューのみ（カテゴリ別サブメニュー、検索なし、D&D なし）。Curve Editor はコード実装済みだが全13機能がビルド・ランタイム未検証。DopeSheet は読み取り専用の QListWidget。
**目標**: ドッカブル Effect Palette（検索+カテゴリツリー+D&D）、Curve Editor 全機能のビルド・ランタイム検証、DopeSheet 編集機能。

## Effect Browser 現状

| 機能 | 状態 |
|------|------|
| 効果適用 | ✅ 右クリックメニュー（階層サブメニュー） |
| 効果一覧 | ✅ `availableEffects()` が ~100 効果をフラットベクターで返す |
| カテゴリ分類 | ⚠️ `categoryForEffect()` が文字列ヒューリスティックで8カテゴリに分類 |
| 効果プリセット | ✅ `ArtifactEffectPreset` + 8デフォルトプリセット |
| 検索フィルタ | ❌ 不在 |
| カテゴリツリー | ❌ 不在（フラットサブメニューのみ） |
| ドラッグ＆ドロップ | ❌ 不在 |
| サムネイル | ❌ 不在 |
| お気に入り/最近使用 | ❌ 不在 |
| ダブルクリック適用 | ❌ 不在 |
| キーボード起動 | ❌ 不在 |

## Curve Editor 現状

| 機能 | コード | ビルド | ランタイム | Undo/Redo |
|------|--------|--------|-----------|-----------|
| CE-1: 接線書戻し | ✅ L3510 | ❌ | ❌ | ❌ |
| CE-2: Auto/Flat/Linear ボタン | ✅ L6461 | ❌ | ❌ | ❌ |
| CE-3: マーキー複数選択 | ✅ | ❌ | ❌ | — |
| CE-4: 複数キー平行移動 | ✅ | ❌ | ❌ | ❌ |
| CE-5: Ctrl+Click キー追加 | ✅ L3538 | ❌ | ❌ | ❌ |
| CE-6: 接線 Break/Unify | ✅ | ❌ | ❌ | — |
| CE-7: Step/Constant 補間 | ✅ | ❌ | ❌ | ❌ |
| CE-8: Frame/Value spinbox | ✅ | ✅ | ❌ | — |
| CE-9: バッファカーブ表示 | ✅ | ❌ | ❌ | — |
| CE-10: 正規化ビュー切替 | ✅ | ❌ | ❌ | — |
| CE-11: サイクル表示 | ✅ | ❌ | ❌ | — |
| CE-12: スナップ | ✅ | ❌ | ❌ | — |
| CE-13: キーコピペ | ✅ | ❌ | ❌ | ❌ |
| CE-14: 速度グラフ編集 | ❌ | — | — | — |

---

## Phase 1: Effect Palette ドッカブルパネル

### 1.1 ArtifactEffectPalette ウィジェット

```cpp
// 新規: Artifact/src/Widgets/Effects/ArtifactEffectPalette.cppm
class ArtifactEffectPalette : public QWidget {
public:
    explicit ArtifactEffectPalette(QWidget* parent = nullptr);

private:
    void buildCategoryTree();
    void filterEffects(const QString& query);
    void startDrag(const QModelIndex& index);
    
    QLineEdit* searchBox_;
    QTreeView* categoryTree_;
    QStandardItemModel* model_;
    
    struct CategoryNode {
        QString name;
        QIcon icon;
        std::vector<EffectInfo> effects;
    };
    std::vector<CategoryNode> categories_;
};
```

### 1.2 カテゴリツリー構築

既存 `categoryForEffect()` を `EffectCategory` enum にアップグレード（Distort Effects milestone の P5 で追加された enum を再利用）:

```cpp
void ArtifactEffectPalette::buildCategoryTree() {
    auto effects = ArtifactEffectService::instance()->availableEffects();
    
    // カテゴリ別にグループ化
    std::map<EffectCategory, std::vector<EffectInfo>> grouped;
    for (auto& effect : effects) {
        grouped[effect.category].push_back(effect);
    }
    
    // ツリーモデル構築
    model_->clear();
    
    // カテゴリ順序（ユーザーに見せる優先度）
    static const std::vector<std::pair<EffectCategory, QString>> categoryOrder = {
        {EffectCategory::Color, "カラー補正"},
        {EffectCategory::Blur, "ブラー"},
        {EffectCategory::Distort, "ディストーション"},
        {EffectCategory::Stylize, "スタイライズ"},
        {EffectCategory::Generate, "生成"},
        {EffectCategory::Keying, "キーイング"},
        {EffectCategory::Matte, "マット"},
        {EffectCategory::Noise, "ノイズ"},
        {EffectCategory::Transition, "トランジション"},
        {EffectCategory::Light, "ライト"},
        {EffectCategory::Time, "時間"},
        {EffectCategory::Audio, "オーディオ"},
        {EffectCategory::Utility, "ユーティリティ"},
    };
    
    for (auto& [cat, displayName] : categoryOrder) {
        auto it = grouped.find(cat);
        if (it == grouped.end() || it->second.empty()) continue;
        
        auto* catItem = new QStandardItem(displayName);
        catItem->setFlags(catItem->flags() & ~Qt::ItemIsDragEnabled);
        
        for (auto& effect : it->second) {
            auto* item = new QStandardItem(effect.displayName);
            item->setData(effect.id, Qt::UserRole);     // 効果ID
            item->setData(effect.description, Qt::UserRole + 1); // 説明
            item->setIcon(QIcon(effect.iconPath));
            item->setFlags(Qt::ItemIsSelectable |
                           Qt::ItemIsDragEnabled |
                           Qt::ItemIsEnabled);
            catItem->appendRow(item);
        }
        
        model_->appendRow(catItem);
    }
}
```

### 1.3 検索フィルタ

```cpp
void ArtifactEffectPalette::filterEffects(const QString& query) {
    if (query.isEmpty()) {
        categoryTree_->setModel(model_);  // 全表示
        return;
    }
    
    // 検索結果のみのフラットリスト
    auto* filterModel = new QStandardItemModel();
    
    for (int c = 0; c < model_->rowCount(); ++c) {
        auto* catItem = model_->item(c);
        for (int e = 0; e < catItem->rowCount(); ++e) {
            auto* effectItem = catItem->child(e);
            QString name = effectItem->text();
            QString desc = effectItem->data(Qt::UserRole + 1).toString();
            
            if (name.contains(query, Qt::CaseInsensitive) ||
                desc.contains(query, Qt::CaseInsensitive)) {
                // カテゴリ名をプレフィックスに
                auto* clone = effectItem->clone();
                clone->setText(catItem->text() + " › " + name);
                filterModel->appendRow(clone);
            }
        }
    }
    
    categoryTree_->setModel(filterModel);
}
```

### 1.4 ドラッグ＆ドロップ

```cpp
void ArtifactEffectPalette::startDrag(const QModelIndex& index) {
    QString effectId = index.data(Qt::UserRole).toString();
    if (effectId.isEmpty()) return;
    
    auto* mimeData = new QMimeData();
    mimeData->setData("application/x-artifact-effect-add", effectId.toUtf8());
    
    auto* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(createDragPixmap(index));
    drag->exec(Qt::CopyAction);
}
```

### 1.5 LayerPanel のドロップ受付

```cpp
// ArtifactLayerPanelWidget::dropEvent に追加（~5行）
void ArtifactLayerPanelWidget::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasFormat("application/x-artifact-effect-add")) {
        QString effectId = QString::fromUtf8(
            event->mimeData()->data("application/x-artifact-effect-add"));
        
        // ドロップ位置のレイヤーに効果を追加
        auto* targetLayer = layerAtPosition(event->pos());
        if (targetLayer) {
            ArtifactEffectService::instance()->addEffectToLayer(
                targetLayer->id(), effectId);
            event->acceptProposedAction();
            return;
        }
    }
    
    // 既存の D&D 処理（レイヤー並べ替え等）
    QWidget::dropEvent(event);
}
```

### 1.6 ダブルクリック適用

```cpp
void ArtifactEffectPalette::onDoubleClicked(const QModelIndex& index) {
    QString effectId = index.data(Qt::UserRole).toString();
    if (effectId.isEmpty()) return;
    
    auto* activeLayer = ArtifactCompositionEditor::instance()->activeLayer();
    if (activeLayer) {
        ArtifactEffectService::instance()->addEffectToLayer(
            activeLayer->id(), effectId);
    }
}
```

### 1.7 完了条件

- [ ] Effect Palette が Dock 領域に表示され、カテゴリツリーが全 ~100 効果を表示
- [ ] 検索ボックスで効果名・説明によるフィルタリングが動作
- [ ] 効果をドラッグして LayerPanel にドロップ→効果追加
- [ ] ダブルクリックでアクティブレイヤーに効果即適用
- [ ] 効果追加後、Inspector に効果パラメータが表示される

---

## Phase 2: Curve Editor ビルド・ランタイム検証

### 2.1 現在のブロッカー

CE-1〜CE-13 はコードが書かれているが、**一度もビルドされていない**。特に:

- **CE-1 (`writeBackCurveEditorTangentEdits`)**: 曲線エディタで編集した接線をプロパティに書き戻すパイプライン。`ArtifactTimelineWidget.cppm:3510`。呼出元→書戻し先のプロパティパスが正しいかが最大のリスク
- **CE-5 (`writeBackCurveEditorStructureDiffs`)**: Ctrl+Click で追加したキーの write-back。時系列の一貫性検証が必要
- **CE-4 (複数キー平行移動)**: `draggedKeys_` スナップショットの undo 復元が正しいか

### 2.2 検証手順（Step-by-step）

1. **ビルド**: `cmake --build build_j_vs18 --target Artifact` で全 CE コードがコンパイルエラーなく通ることを確認
2. **接線編集**: キーフレームを選択→接線ハンドルをドラッグ→プロパティ値が更新される→Ctrl+Z→復元
3. **キー追加**: Ctrl+Click でカーブ上に新規キー追加→プロパティにキーフレームが追加される→Undo
4. **複数選択**: マーキーで複数キー選択→Shift+Click 追加→一括移動
5. **補間切替**: 選択キー→Flat/Linear/Auto ボタン→カーブ形状変化→Undo
6. **スナップ**: Ctrl+ドラッグでグリッドにスナップ
7. **コピペ**: キー選択→Ctrl+C→別トラック→Ctrl+V→値が正しく複製
8. **正規化**: 正規化ビュー切替→全カーブが 0-1 範囲に正規化→切替解除で元に戻る

### 2.3 完了条件

- [ ] CE-1〜CE-13 がビルド成功
- [ ] 接線編集・キー追加・削除・移動の基本操作が Undo/Redo 対応
- [ ] 編集後のカーブとプロパティ評価値が一致
- [ ] エッジケース: キーが1つだけのカーブ編集、全キー削除

---

## Phase 3: DopeSheet 編集機能 + Curve Editor 統合

### 3.1 DopeSheet 編集機能

現状の読み取り専用 `QListWidget` を編集可能な `QTableView` に置き換え:

| 機能 | 実装 |
|------|------|
| キー選択 | single, Ctrl+click multi, Shift+click range |
| キー移動 | 水平（時間）+ 垂直（値）ドラッグ |
| キー削除 | Delete キー |
| 時間スケール | Alt+ドラッグでキー間隔を伸縮 |
| 補間色表示 | キーの補間タイプを色で表現（Linear=灰, Bezier=青, Hold=赤 等） |
| チャンネルグループ | position を展開して X/Y を個別表示 |
| サマリー行 | 全チャンネルを1行にまとめたサマリー表示 |

```cpp
// 新規: Artifact/src/Widgets/Timeline/ArtifactDopeSheetEditor.cppm
class ArtifactDopeSheetEditor : public QWidget {
public:
    void setKeyframeModel(ArtifactTimelineKeyframeModel* model);
    
    // 編集モード
    void moveSelectedKeys(int deltaFrames, float deltaValue);
    void deleteSelectedKeys();
    void scaleSelectedKeys(float timeScale, int64_t anchorFrame);

private:
    QTableView* tableView_;
    ArtifactTimelineKeyframeModel* model_;
    DopeSheetDelegate* delegate_;  // カスタム描画
};
```

### 3.2 Curve Editor ↔ DopeSheet シームレス切替

現状の `QStackedWidget` ページ切替を、単一の unified モードに:

```cpp
// TimelineWidget の toolbar にトグルボタンを追加
QToolButton* curveDopeSheetToggle;
QAction* curveMode = toggle->addAction("Curve Editor");
QAction* dopeSheetMode = toggle->addAction("Dope Sheet");

// キーボードショートカット
// ` (バッククォート): Curve Editor / Dope Sheet 切替
// Ctrl+`: タイムラインに戻る
```

### 3.3 完了条件

- [ ] DopeSheet でキー選択・移動・削除が動作
- [ ] DopeSheet ↔ Curve Editor が1クリックで切替
- [ ] 両方で選択状態が同期される（DopeSheet で選択→Curve Editor でも選択）
- [ ] DopeSheet で移動したキーが Timeline と Curve Editor に即反映

---

## Phase 4: 効果のお気に入り・最近使用 + プリセットブラウザ

### 4.1 お気に入り・最近使用

```cpp
// ArtifactEffectService に追加
class ArtifactEffectService {
    // ...既存...
    
    void markFavorite(const QString& effectId);
    void unmarkFavorite(const QString& effectId);
    QStringList favoriteEffectIds() const;
    bool isFavorite(const QString& effectId) const;
    
    void recordEffectUsed(const QString& effectId);
    QStringList recentEffectIds(int maxCount = 10) const;
    
    // 永続化
    void saveFavorites();
    void loadFavorites();
};
```

Effect Palette のトップに「お気に入り」セクションと「最近使用」セクションを追加。

### 4.2 プリセットブラウザ

`ArtifactEffectPresetCollection` を拡張し、プリセットのサムネイル付きブラウザを Effect Palette 内に表示:

```cpp
// Effect Palette 下部にプリセットプレビュー
void ArtifactEffectPalette::showPresetsForEffect(const QString& effectId) {
    auto presets = ArtifactEffectPresetCollection::presetsForEffect(effectId);
    
    presetList_->clear();
    for (auto& preset : presets) {
        auto* item = new QListWidgetItem();
        item->setIcon(QPixmap::fromImage(preset.thumbnail()));
        item->setText(preset.name());
        item->setToolTip(preset.description());
        item->setData(Qt::UserRole, preset.id());
        presetList_->addItem(item);
    }
}
```

### 4.3 完了条件

- [ ] お気に入り登録した効果が Palette 上部に常時表示
- [ ] 最近使用した10効果が履歴表示
- [ ] 効果選択時に下部にプリセット一覧が表示
- [ ] プリセットダブルクリックで効果をプリセット値で適用

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | 新規 `Artifact/src/Widgets/Effects/ArtifactEffectPalette.cppm` | パレットウィジェット本体 |
| P1 | 新規 `Artifact/include/Widgets/Effects/ArtifactEffectPalette.ixx` | インターフェース |
| P1 | `Artifact/src/Widgets/Layer/ArtifactLayerPanelWidget.cppm` | dropEvent 追加 (~5行) |
| P1 | `Artifact/src/AppMain.cppm` | パレットを Dock に登録 |
| P2 | `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cppm` | CE コードのビルドデバッグ |
| P2 | `Artifact/src/Widgets/Timeline/ArtifactCurveEditorWidget.cppm` | 接線・キー操作の Undo/Redo 修正 |
| P3 | 新規 `Artifact/src/Widgets/Timeline/ArtifactDopeSheetEditor.cppm` | 編集可能 DopeSheet |
| P3 | `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cppm` | unified モード切替 |
| P4 | `Artifact/src/Service/ArtifactEffectService.cppm` | お気に入り・履歴管理 |
| P4 | `Artifact/src/Widgets/Effects/ArtifactEffectPalette.cppm` | お気に入り・履歴・プリセット表示 |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P1: Effect Palette | **P0** | 中 | 最大のUXギャップ。データ層は完成。新規ウィジェット |
| P2: Curve Editor 検証 | **P0** | 大 | コードは書かれている。ビルド+ランタイムデバッグが主 |
| P3: DopeSheet 編集 | **P1** | 中 | 読み取り専用→編集可能への移行 |
| P4: お気に入り+プリセット | **P2** | 小 | 薄い追加機能 |
