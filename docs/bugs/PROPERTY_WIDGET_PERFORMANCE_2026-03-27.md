# プロパティウィジェット パフォーマンス問題レポート (2026-03-27)

## 症状

プロパティウィジェット（インスペクター）が重い。レイヤー選択切り替え時にラグがある。

---

## ボトルネック分析

### ★★★ 1. 選択変更の度に全ウィジェットを破棄・再作成

**場所:** `ArtifactPropertyWidget.cppm:717-723`

```cpp
void ArtifactPropertyWidget::Impl::rebuildUI() {
    propertyEditors.clear();  // hash をクリア
    QLayoutItem* child;
    while ((child = mainLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();  // ← 全ウィジェットを破棄
        }
    }
    // ... 全てを再作成 ...
}
```

**影響:**
- TextLayer (18 基本 + 18 テキスト + 10 エフェクト) = **~46 プロパティ**
- 各プロパティ行に 7-10 個の子ウィジェット (ラベル、エディタ、5ボタン)
- **1回の選択変更で ~400 ウィジェット作成/破棄**
- ウィジェットプール、キャッシュ、再利用なし

### ★★★ 2. アイコンが毎回ディスクから読み込み

**場所:** `ArtifactPropertyEditor.cppm:923-927`

```cpp
QIcon keyIcon = loadPropertyIcon("MaterialVS/yellow/keyframe.svg");
QIcon prevIcon = loadPropertyIcon("MaterialVS/neutral/arrow_left.svg");
QIcon nextIcon = loadPropertyIcon("MaterialVS/neutral/arrow_right.svg");
QIcon resIcon = loadPropertyIcon("MaterialVS/neutral/undo.svg");
QIcon exprIcon = loadPropertyIcon("MaterialVS/blue/code.svg");
```

`loadPropertyIcon()` は `resolveIconResourcePath()` → `loadSvgAsIcon()` でファイル I/O を毎回実行。
46 プロパティ × 5 アイコン = **230 回のファイル I/O / リビルド**。
キャッシュなし。

### ★★ 3. シグナルストーム: 変更 → リビルドのカスケード

**場所:** `ArtifactPropertyWidget.cppm:575-595`

```
プロパティ編集
  → setLayerPropertyValue()
  → notifyLayerMutation()
  → layer->changed() シグナル
  → scheduleRebuild(80ms)           ← リビルド1回目
  → notifyProjectIfTimelinePropertyChanged()
  → projectChanged()
  → scheduleRebuild(0ms)            ← リビルド2回目（即座）
```

1つのプロパティ編集で **2回のリビルド** がトリガーされる。

### ★★ 4. プロパティ行あたり ~8 のシグナル接続

**場所:** `ArtifactPropertyEditor.cppm:901-1067`

各 `ArtifactPropertyEditorRowWidget` コンストラクタ:
- Float: spinBox(3) + slider(1) + reset/expr/keyframe/prev/next(5) = **8 シグナル**
- Int: 同上 **8 シグナル**
- Bool: toggled(1) + 5 = **6 シグナル**
- String/Color/Enum: **6 シグナル**

46 プロパティ × ~6-8 = **~150 シグナル接続 / リビルド**。
全てが次のリビルドで切断・破棄される。

### ★ 5. getLayerPropertyGroups() が2回呼ばれる

**場所:** `ArtifactPropertyWidget.cppm:773, 874`

```cpp
const auto layerGroups = currentLayer->getLayerPropertyGroups();  // 1回目
// ... サマリーセクションで使用 ...
for (const auto& groupDef : layerGroups) {  // 2回目のイテレーション
```

2回のイテレーション（サマリー + 詳細）でプロパティグループを処理。
`persistentLayerProperty()` のキャッシュでヒープ割り当ては回避されるが、
各プロパティで 4 回の setter 呼び出しが発生。

### ★ 6. エフェクトプロパティが2回コピーされる

**場所:** `ArtifactPropertyWidget.cppm:840-843, 931-934`

```cpp
// サマリー:
propGroup.addProperty(std::make_shared<AbstractProperty>(property));  // コピー1
// 詳細:
propGroup.addProperty(std::make_shared<AbstractProperty>(p));          // コピー2
```

`effect->getProperties()` が値型の `vector<AbstractProperty>` を返すため、
各プロパティが `make_shared` で2回コピーされる。
5 プロパティ × 2 = **10 不要なコピー**。

### ★ 7. フレーム変更で全プロパティエディタを更新

**場所:** `ArtifactPropertyWidget.cppm:422-433, 657-688`

```cpp
// フレーム変更時:
for (auto it = propertyEditors.begin(); it != propertyEditors.end(); ++it) {
    const QVariant animatedValue = propertyPtr->interpolateValue(now);  // 補間
    row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(now));           // O(n) キーフレーム走査
}
```

スクラブ中、毎フレーム全プロパティエディタに対して補間 + キーフレーム走査。

### ★ 8. スタイルシート再計算

**場所:** `ArtifactPropertyWidget.cppm:447-561`

114行の CSS 文字列を全体に適用。ウィジェット追加/削除の度に
Qt がスタイル継承ツリー全体を再計算。

---

## 影響の量化

| 操作 | ウィジェット数 | シグナル接続 | ファイル I/O | 推定時間 |
|------|-------------|------------|------------|---------|
| TextLayer 選択 | ~400 | ~150 | ~230 | 30-80ms |
| Solid2D 選択 | ~200 | ~80 | ~120 | 15-40ms |
| VideoLayer 選択 | ~280 | ~110 | ~170 | 20-50ms |

---

## 対策案

### 対策1: ウィジェット再利用プール（最重要）

選択変更時に全破棄せず、既存ウィジェットを再利用:
```cpp
// rebuildUI() の代わり:
for (auto& group : layerGroups) {
    for (auto& prop : group.properties()) {
        if (propertyEditors.contains(prop.path())) {
            // 既存エディタを更新
            propertyEditors[prop.path()]->updateFromProperty(prop);
        } else {
            // 新規作成のみ
            auto* row = new ArtifactPropertyEditorRowWidget(prop);
            propertyEditors.insert(prop.path(), row);
        }
    }
}
// 余剰エディタを非表示 or 削除
```

### 対策2: アイコンキャッシュ

```cpp
// グローバルキャッシュ
static QHash<QString, QIcon> g_iconCache;
QIcon loadPropertyIconCached(const QString& path) {
    if (!g_iconCache.contains(path)) {
        g_iconCache[path] = loadPropertyIcon(path);
    }
    return g_iconCache[path];
}
```

### 対策3: changed() のデバウンス

`changed()` シグナルの発火を 1 フレームに1回に制限:
```cpp
// notifyLayerMutation 内:
if (debounceTimer.elapsed() < 16) return;  // 60fps 相当
```

### 対策4: getLayerPropertyGroups() の結果キャッシュ

```cpp
mutable std::optional<std::vector<PropertyGroup>> cachedGroups_;
// 一度構築したらフレーム内で再利用
```

### 対策5: アニメーション中の値更新をスキップ

エディタがフォーカス中のプロパティは更新をスキップ（既存の `hasFocus()` チェックを拡張）。

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` | 717-723 | rebuildUI — 全ウィジェット破棄 |
| `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` | 575-595 | setLayer — changed() → scheduleRebuild |
| `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` | 440-445 | projectChanged → scheduleRebuild(0) |
| `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` | 422-433 | frameChanged → updatePropertyValues |
| `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` | 657-688 | updatePropertyValues — 全エディタ更新 |
| `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` | 773,874 | getLayerPropertyGroups — 2回呼び出し |
| `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm` | 901-1067 | RowWidget コンストラクタ |
| `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm` | 923-927 | アイコンディスク読み込み |
| `Artifact/src/Layer/ArtifactAbstractLayer.cppm` | 52-59 | notifyLayerMutation — changed() 発火 |
| `Artifact/src/Layer/ArtifactAbstractLayer.cppm` | 1008-1027 | persistentLayerProperty — キャッシュ |
