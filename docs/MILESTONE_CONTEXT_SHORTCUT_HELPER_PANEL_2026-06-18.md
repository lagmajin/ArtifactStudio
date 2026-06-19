# マイルストーン: Context Shortcut Helper Panel

> 2026-06-18 作成

## 目的

現在のフォーカス/ワークスペースモードで使えるショートカット一覧を表示するドックパネルを追加する。

---

## 背景

ArtifactStudio は多数のショートカットを持つが、ユーザーは自分の今いる場面で何が使えるかを忘れがち。コンテキストに応じたショートカットヘルパーがあれば、操作を覚えやすくなり、作業効率が上がる。

---

## 概念

- **Context**: 現在の `WorkspaceMode` + フォーカスウィジェット（タイムライン/ビューアー/レイヤーパネルなど）
- **Shortcut Entry**: `{ context, category, action, shortcut, description }`
- **リアルタイム更新**: フォーカス変更やワークスペース切り替えで表示内容を更新

---

## フェーズ設計

### Phase 1: Data source

**目的:** ショートカット情報を収集・提供するクラスを実装する。

**作業項目:**
- `Artifact/src/Widgets/ArtifactContextShortcutProvider.cppm` 作成
  - `ArtifactContextShortcutEntry` 構造体
  - `WorkspaceMode` とフォーカスウィジェットに応じたリスト返却
  - 既存 `ArtifactTimelineKeyBinding` などから情報を収集
  - アプリケーションメニューからショートカット情報を抽出（可能な範囲）

**完了条件:**
- コンテキストに応じたショートカットリストを取得可能

**完成度:**
- [ ] Provider クラス作成
- [ ] Entry 構造体定義
- [ ] タイムラインショートカット収集
- [ ] ワークスペース連携

---

### Phase 2: Widget

**目的:** ショートカット一覧を表示する UI を実装する。

**作業項目:**
- `Artifact/src/Widgets/ArtifactContextShortcutHelperWidget.cppm` 作成
  - `QTreeWidget` または `QTableView` ベース
  - カテゴリ別/コンテキスト別グループ化
  - 検索フィルタ
  - ショートカットキー表示は既存のリッチフォーマットを参考に
- `W_OBJECT` / `W_OBJECT_IMPL` 追加

**完了条件:**
- UI にショートカット一覧が表示される
- 検索で絞り込める

**完成度:**
- [ ] Widget クラス作成
- [ ] 一覧表示
- [ ] カテゴリ分類
- [ ] 検索フィルタ

---

### Phase 3: Dock registration

**目的:** メインパネルとして登録する。

**作業項目:**
- `Artifact/src/AppMain.cppm`
  - `#import` 追加
  - `addDockedWidgetTabbed` で「Shortcut Helper」として登録

**完了条件:**
- アプリ起動後、「Shortcut Helper」パネルがドッキング可能

**完成度:**
- [ ] AppMain に登録
- [ ] レイアウト確認

---

## Non-Goals

- ショートカットの編集機能
- ユーザー独自のショートカット登録
- チュートリアル/ガイド機能
- 全ショートカットの完全網羅（主要なものから順次拡充）

---

## 技術方針

- 既存のキーバインディング情報を優先して再利用
- 新規にキー情報をハードコードしないように努める
- `QApplication::focusWidget()` または EventBus のフォーカスイベントでコンテキスト更新
- `setStyleSheet()` は使わない

---

## 進捗サマリー

| Phase | 状態 |
|---|---|
| Phase 1 | 未着手 |
| Phase 2 | 未着手 |
| Phase 3 | 未着手 |

**総合完成度:** 0%
