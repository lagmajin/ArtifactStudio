> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_UI_THEME_MIGRATION.md](MILESTONE_UI_THEME_MIGRATION.md)

# QSS Reduction / Style Ownership Milestone (2026-03-31)

`QSS` を UI の主責務から外し、`Artifact` の見た目を theme tokens + 共通 widget + owner-draw で管理するための実行マイルストーン。
`MILESTONE_UI_THEME_SYSTEM_2026-03-30.md` の Phase 3 を、実施順の粒度に落として進める。

## Goal

- `QSS` の用途を「例外的な微調整」に限定する
- 背景、枠線、選択、ホバー、アクセントを theme / palette / 共通部品で持つ
- widget ごとの stylesheet 依存を減らし、見た目変更の責務を集約する
- 既存の操作系を壊さずに、段階的に style ownership を移す

## Scope

- `Artifact/src/Widgets`
- `Artifact/src/Widgets/Render`
- `Artifact/src/Widgets/Timeline`
- `Artifact/src/Widgets/Dialog`
- `Artifact/src/Widgets/Dock`
- `Artifact/src/Widgets/Menu`
- `Artifact/src/Widgets/PropertyEditor`
- `ArtifactWidgets/src`
- `Artifact/src/AppMain.cppm`

## Non-Goals

- 全 widget を一括で `QSS` ゼロにする
- OS ネイティブ外観の厳密な再現
- 操作系の大改造
- 既存の細かなスタイルを全部消すこと自体を目的にする

## Phases

### Phase 1: QSS Inventory

- `setStyleSheet()` の使用箇所を棚卸しする
- inline style / global stylesheet / widget-local stylesheet を分類する
- まず残すべき `QSS` と消せる `QSS` を分ける

### Phase 2: Theme Ownership

- 背景、surface、border、selection、accent を theme token から供給する
- `QPalette` をベースにできるところは palette へ移す
- 主要 widget は共通の `Theme` API を参照するようにする

### Phase 3: Widget Replacement

- button / tab / badge / panel / header を共通部品へ寄せる
- `Dock`, `Inspector`, `Render Queue`, `Timeline` の見た目を揃える
- 必要最小限の `QSS` だけを残す

### Phase 4: Residual CSS Cleanup

- 残存 `QSS` を用途別に整理する
- 例外ルールを文書化する
- theme 変更で壊れやすい箇所を減らす

## First Targets

- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`

## Validation Checklist

- 主要画面の色と階層感が theme で説明できる
- `setStyleSheet()` を追加しなくても見た目を整えられる
- 既存の widget で hover / selection / disabled の見え方が揃う
- 例外的な `QSS` が増殖しない

## Static Audit (2026-07-25)

対象の `Artifact`／`ArtifactWidgets` 実装ソースを検索したところ、バックアップ・資料ファイルを除く範囲で実際の `setStyleSheet()` は `DockStyleManager.cppm` の `setStyleSheet(QString())` だけだった。これは DockManager の既存 stylesheet を空にする後処理であり、QSS を見た目の定義として追加している箇所ではない。CommonStyle、QPalette、theme token、owner-draw への移行は、Property／Inspector を含む主要 UI で確認できる。

一方、これは静的な残存数の確認であり、全画面の hover／selection／disabled の統一や、theme 切替時の再描画、例外的 stylesheet の責務文書化を証明しない。`QStyleSheetStyle` の実行時混入、サブモジュール全体、生成物・バックアップを含む完全なインベントリも未確認。したがって Phase 1〜2 は大きく前進、Phase 3〜4 と完了条件は未検証として、ステータスは未完了のままとする。

確認対象:

- `Artifact/src/Widgets/Dock/DockStyleManager.cppm`
- `Artifact/src/Widgets/CommonStyle.cppm`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/PropertyEditor/`
- `ArtifactWidgets/src/Common/HeadPanel.cppm`
