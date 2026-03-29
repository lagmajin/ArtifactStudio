# UI/UX 統一改善 Milestone

**作成日:** 2026-03-28  
**ステータス:** 計画中  
**関連コンポーネント:** ArtifactPropertyWidget, ArtifactInspectorWidget, ArtifactTimelineWidget, ArtifactProjectManagerWidget

---

## 概要

UI/UX の統一性を改善し、操作性と視認性を向上させる。

---

## 発見された問題点

### ★★★ 問題 1: プロパティウィジェットのパフォーマンス

**場所:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm:717-723`

**問題:**
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
- 選択変更の度に全ウィジェットを破棄・再作成
- TextLayer で 46 プロパティ × 7-10 子ウィジェット = **~400 ウィジェット**
- 1 回の選択変更で 30-80ms のラグ

**工数:** 6-8 時間

---

### ★★★ 問題 2: アイコンの毎回ディスク読み込み

**場所:** `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm:923-927`

**問題:**
```cpp
QIcon keyIcon = loadPropertyIcon("MaterialVS/yellow/keyframe.svg");
QIcon prevIcon = loadPropertyIcon("MaterialVS/neutral/arrow_left.svg");
QIcon nextIcon = loadPropertyIcon("MaterialVS/neutral/arrow_right.svg");
QIcon resIcon = loadPropertyIcon("MaterialVS/neutral/undo.svg");
QIcon exprIcon = loadPropertyIcon("MaterialVS/blue/code.svg");
```

**影響:**
- 46 プロパティ × 5 アイコン = **230 回のファイル I/O**
- キャッシュなし
- 起動時と選択変更時に発生

**工数:** 2-3 時間

---

### ★★ 問題 3: タイムラインのドラッグ＆ドロップ不具合

**場所:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`

**問題:**
- 左ペインで D&D が機能しない
- `QDrag::exec()` がブロック
- スクロール領域との競合

**工数:** 4-6 時間

---

### ★★ 問題 4: キーボードショートカットの不足

**場所:** 各ウィジェット

**未実装:**
- `Ctrl+C/V` - コピー/ペースト
- `Ctrl+Z/Y` - アンドゥ/リドウ
- `Delete` - 削除
- `Ctrl+D` - 複製
- `G` - グループ化

**工数:** 4-6 時間

---

### ★ 問題 5: ダークテーマの不完全さ

**場所:** 全体

**問題:**
- 一部ウィジェットでライトテーマが混在
- スタイルシートの統一性不足
- ハイコントラスト対応なし

**工数:** 6-8 時間

---

### ★ 問題 6: ツールチップの不足

**場所:** 全体

**問題:**
- ボタンにツールチップがない
- 機能の説明がない
- ショートカットキーの表示なし

**工数:** 3-4 時間

---

## 優先度別実装計画

### P0（必須）

| 項目 | 工数 | 優先度 | 効果 |
|------|------|--------|------|
| **プロパティウィジェット最適化** | 6-8h | 🔴 高 | 応答性向上 |
| **アイコンキャッシュ** | 2-3h | 🔴 高 | 起動時間短縮 |

### P1（重要）

| 項目 | 工数 | 優先度 | 効果 |
|------|------|--------|------|
| **D&D 改善** | 4-6h | 🟡 中 | 操作性向上 |
| **ショートカット実装** | 4-6h | 🟡 中 | 作業効率向上 |

### P2（推奨）

| 項目 | 工数 | 優先度 | 効果 |
|------|------|--------|------|
| **ダークテーマ統一** | 6-8h | 🟢 低 | 視認性向上 |
| **ツールチップ拡充** | 3-4h | 🟢 低 | 学習コスト低減 |

**合計工数:** 25-35 時間

---

## Phase 構成

### Phase 1: プロパティウィジェット最適化

- 目的:
  - 選択変更時のラグを解消

- 作業項目:
  - ウィジェットプールの実装
  - 既存ウィジェットの再利用
  - 差分更新のみ実行

- 完了条件:
  - 選択変更が 30-80ms → 5-10ms に
  - メモリ使用量 30% 削減

- 実装案:
  ```cpp
  // 修正前：全破棄
  while ((child = mainLayout->takeAt(0)) != nullptr) {
      child->widget()->deleteLater();
  }
  
  // 修正後：プールから取得
  for (const auto& group : layerGroups) {
      for (const auto& prop : group.properties()) {
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
  ```

### Phase 2: アイコンキャッシュ

- 目的:
  - ファイル I/O を削減

- 作業項目:
  - グローバルアイコンキャッシュ
  - 遅延読み込み
  - プリフェッチ

- 完了条件:
  - 2 回目以降の読み込みがキャッシュから
  - 起動時間が 20% 短縮

- 実装案:
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

### Phase 3: D&D 改善

- 目的:
  - タイムラインの操作性向上

- 作業項目:
  - `QScrollArea` のイベントフィルター改善
  - ドラッグ開始の閾値調整
  - ビジュアルフィードバック

- 完了条件:
  - レイヤーの並び替えがスムーズに
  - ドロップ位置が明確

### Phase 4: ショートカット実装

- 目的:
  - 作業効率向上

- 作業項目:
  - 主要ショートカットの実装
  - ショートカット設定 UI
  - チートシート

- 完了条件:
  - 主要操作がキーボードで可能
  - カスタマイズ可能

---

## 技術的課題

### 1. ウィジェットの状態管理

**課題:**
- プールされたウィジェットの状態を正しく管理

**解決案:**
- 状態オブジェクトの分離
- リセット/リストア機構
- バージョン管理

### 2. ショートカットの競合

**課題:**
- 複数のコンテキストで同じショートカット

**解決案:**
- コンテキストベースのバインディング
- 優先度管理
- ユーザーカスタマイズ

### 3. テーマの一貫性

**課題:**
- サードパーティウィジェットとの統一

**解決案:**
- スタイルシートの包括的適用
- カスタムウィジェットへの置換
- プラグイン API の提供

---

## 期待される効果

### 性能向上

| 指標 | 現在 | 改善後 | 向上率 |
|------|------|--------|--------|
| **プロパティ表示** | 30-80ms | 5-10ms | -83% |
| **アイコン読み込み** | 230 回 I/O | 0 回 (2 回目) | -100% |
| **起動時間** | 5 秒 | 4 秒 | -20% |

### ユーザー体験

- プロパティ変更がサクサク
- キーボード操作で効率的
- 視認性が向上

---

## 関連ドキュメント

- `docs/bugs/PROPERTY_WIDGET_PERFORMANCE_2026-03-27.md` - プロパティ性能
- `docs/bugs/TIMELINE_DRAG_DROP_NOT_WORKING_2026-03-27.md` - D&D 不具合
- `docs/planned/MILESTONES_BACKLOG.md` - 全体バックログ

---

## 実装順序の推奨

1. **Phase 1: プロパティ最適化** - 効果大
2. **Phase 2: アイコンキャッシュ** - 実装容易
3. **Phase 3: D&D 改善** - 操作性向上
4. **Phase 4: ショートカット** - 作業効率

---

**文書終了**
