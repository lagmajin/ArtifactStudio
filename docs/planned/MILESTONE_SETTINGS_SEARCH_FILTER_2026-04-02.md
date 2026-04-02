# MILESTONE_SETTINGS_SEARCH_FILTER_2026-04-02

## 概要

環境設定ダイアログ (`ApplicationSettingDialog`) に JetBrains IDE 風の検索バーを実装し、検索クエリに一致する設定項目のみを表示する機能を実装する。

## 背景

現在の `ApplicationSettingDialog` は左側にカテゴリリスト、右側に `QStackedWidget` でページを切り替える構造。
各ページ内の設定項目を探すには、カテゴリを手動で切り替える必要があり、設定項目が増えるほど目的の設定にたどり着くのが困難になっている。

ショートカットページ (`ShortcutSettingPage`) には既にフィルタ機能があるが、これはショートカット専用の実装であり他ページでは使えない。

## 現在のカテゴリ一覧

| カテゴリ | ページクラス | 設定項目数(概算) |
|---------|-------------|-----------------|
| General | `GeneralSettingPage` | 3 (Theme, Auto-Save, Startup) |
| Import | `ImportSettingPage` | 9 (Frame Rate, Color Space, etc.) |
| Preview | `PreviewSettingPage` | 9 (Quality, Cache, GPU, etc.) |
| Memory & Performance | `MemoryAndCpuSettingPage` | 3 (Worker Threads, Cache) |
| Shortcuts | `ShortcutSettingPage` | 可変 (フィルタ実装済み) |
| Plugins | `PluginSettingPage` | 可変 |
| AI | `AISettingPage` | 3 (Provider, Model Path) |

## 設計方針

### UI構成

```
┌─────────────────────────────────────────────────┐
│ [🔍 Search settings...]                         │ ← 検索バー (QLineEdit)
├──────────┬──────────────────────────────────────┤
│ General  │                                      │
│ Import   │         設定ページ表示領域            │
│ Preview  │                                      │
│ ...      │                                      │
├──────────┴──────────────────────────────────────┤
│              [OK] [Cancel] [Apply]              │
└─────────────────────────────────────────────────┘
```

### 検索モード

1. **通常モード**: 検索バーが空 → 従来通りカテゴリ選択でページ切り替え
2. **検索モード**: 文字列を入力 → 全ページ横断検索し、一致項目をフラットリストで表示

### 検索対象

各ページから「検索可能な設定項目」のメタデータを取得し、以下のフィールドでマッチング:
- 設定項目のラベル名
- 設定項目の説明（あれば）
- 所属カテゴリ名

### ページ側の変更

各設定ページに `ISettingPage` インターフェースを拡張し、検索用のメタデータ提供を追加:

```cpp
struct SettingItemInfo {
    QString label;          // 設定項目の表示名
    QString description;    // 説明（オプション）
    QString category;       // 親カテゴリ名
    QWidget* widget;        // 対応するUIウィジェット（ハイライト用）
};

class ISettingPage {
public:
    virtual ~ISettingPage() = default;
    virtual void loadSettings() = 0;
    virtual void saveSettings() = 0;
    virtual QList<SettingItemInfo> searchableItems() const = 0;  // 追加
};
```

## 実装マイルストーン

### Phase 1: 基盤整備 (優先度: 高)

- [ ] 1.1 `ISettingPage` インターフェースに `searchableItems()` を追加
- [ ] 1.2 `SettingItemInfo` 構造体を定義
- [ ] 1.3 既存ページに `searchableItems()` を実装
  - [ ] `GeneralSettingPage`
  - [ ] `ImportSettingPage`
  - [ ] `PreviewSettingPage`
  - [ ] `MemoryAndCpuSettingPage`
  - [ ] `ShortcutSettingPage`
  - [ ] `PluginSettingPage`
  - [ ] `AISettingPage`

### Phase 2: 検索バーUI (優先度: 高)

- [ ] 2.1 `ApplicationSettingDialog` に検索バー (`QLineEdit`) を追加
  - 位置: ダイアログ上部、カテゴリリストと設定ページの上
  - プレースホルダー: "Search settings..."
  - クリアボタン付き (`QLineEdit::ClearButton`)
- [ ] 2.2 検索結果表示用のフラットリストウィジェット (`QListWidget`) を追加
  - 検索モード時にカテゴリリストの代わりに表示
  - 各項目にカテゴリ名をサブテキストとして表示
- [ ] 2.3 検索モード切り替えロジック
  - 検索バーが空 → 通常モード（カテゴリリスト表示）
  - 検索バーに文字あり → 検索モード（結果リスト表示）

### Phase 3: 検索ロジック (優先度: 高)

- [ ] 3.1 全ページから `searchableItems()` を収集
- [ ] 3.2 部分一致検索（大文字小文字区別なし）
- [ ] 3.3 検索結果をリアルタイム更新 (`QLineEdit::textChanged` シグナル使用)
- [ ] 3.4 検索結果クリック時に該当ページへジャンプ + 項目ハイライト

### Phase 4: UX改善 (優先度: 中)

- [ ] 4.1 キーボードショートカット
  - `Ctrl+F` で検索バーにフォーカス
  - `Escape` で検索クリア
- [ ] 4.2 検索結果がない場合のメッセージ表示 ("No settings found")
- [ ] 4.3 一致項目のハイライト機能（該当ページに移動後、対応ウィジェットを点滅または枠表示）
- [ ] 4.4 検索履歴（オプション）

### Phase 5: テスト・調整 (優先度: 中)

- [ ] 5.1 手動テスト: 各ページの設定項目が正しく検索できるか
- [ ] 5.2 パフォーマンス確認（設定項目が増えても遅くならないか）
- [ ] 5.3 UIの見た目調整（既存テーマとの整合性）

## 影響範囲

### 変更ファイル

| ファイル | 変更内容 |
|---------|---------|
| `ApplicationSettingDialog.ixx` | `ISettingPage` インターフェース拡張、`SettingItemInfo` 追加 |
| `ApplicationSettingDialog.cppm` | 各ページに `searchableItems()` 実装、検索バーUI追加、検索ロジック追加 |

### 依存関係

- Qt: `QLineEdit`, `QListWidget`, `QVBoxLayout`
- 既存の `ISettingPage` 継承クラスすべて

## 備考

- `ShortcutSettingPage` は既に独自のフィルタ機能を持つが、これはページ内部のショートカット一覧用のもの。ページ横断検索とは別に残す。
- サブモジュール (`ArtifactWidgets`, `DiligentEngine`) は変更しない。
