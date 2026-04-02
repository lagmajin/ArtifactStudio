# UI Theme System / Studio Skin Milestone (2026-03-30)

`QSS` に責務を寄せすぎず、`Artifact` 全体の見た目を意味ベースのテーマ値で管理するためのマイルストーン。
`Maya / Blender / Modo / DaVinci Resolve` のような制作ツールにある「暗めの作業面」「明確な選択状態」「大きめの情報面」という方向性を参考にしつつ、
アプリ独自の配色・余白・階層を持つ `studio skin` を作る。

## Goal

- 全体背景、パネル、カード、選択、ホバー、区切り線を共通の theme で管理できるようにする
- `QSS` は最小限にして、見た目の核は自前描画または共通ウィジェットへ寄せる
- 「背景色だけ変えたい」ではなく、制作ツールとしての画面階層を揃える
- UI を大量に壊さず、既存 widget へ段階導入できるようにする

## Scope

- `Artifact/src/Widgets`
- `Artifact/src/Widgets/Render`
- `Artifact/src/Widgets/Timeline`
- `Artifact/src/Widgets/Dialog`
- `Artifact/src/Widgets/Dock`
- `Artifact/src/Widgets/Menu`
- `Artifact/src/Widgets/PropertyEditor`
- `Artifact/src/Widgets/PropertyEditor/*`
- `docs/WIDGET_MAP.md`

## Non-Goals

- 完全な CSS テーマエンジン化
- OS ネイティブ見た目の厳密再現
- 全 widget を一括で作り直す大規模リライト
- 既存の操作系を壊す配色だけの変更

## Design Principles

- 背景は 1 色ではなく、`app / workspace / surface / elevated` の階層で扱う
- コントラストは `primary`, `secondary`, `muted`, `accent` の意味で管理する
- ボタンや tab は `QSS` ではなく共通部品で揃える
- timeline / inspector / render queue は同じ階層感を持たせる
- 余白と角丸は控えめにし、情報密度を保つ

## Phases

### Phase 1: Theme Tokens And Palette Core

- 色定義を意味ベースで一元化する
- `background`, `panel`, `panelElevated`, `border`, `borderStrong`, `textPrimary`, `textSecondary`, `accent`, `accentSoft`, `danger`, `warning`, `success` を整理する
- `QPalette` と `QSS` の責務を分離する
- light/dark の切替余地を残す

### Phase 2: Core Surface Widgets

- パネル、セクション見出し、フラットボタン、切替チップ、ステータスバッジを共通部品化する
- `Dock` / `Inspector` / `Render Queue` / `Timeline` の表情を揃える
- 選択状態とホバー状態の視認性を統一する

### Phase 3: QSS Reduction And Style Ownership

- 既存 `QSS` を削減し、widget ごとの責務を明確にする
- 背景、枠線、強調色を theme class から供給する
- 例外的に必要な `QSS` だけを残す

### Phase 4: Workspace Mood And Editor Polish

- composition editor / render queue / property editor の見た目を揃える
- 密度の高い制作 UI として、視線誘導を最適化する
- スペーシング、見出し、分割線、空状態の文言を整える

## Recommended Order

1. `Phase 1: Theme Tokens And Palette Core`
2. `Phase 2: Core Surface Widgets`
3. `Phase 3: QSS Reduction And Style Ownership`
4. `Phase 4: Workspace Mood And Editor Polish`

## Current Status

- 既に一部 widget は独自描画に寄っている
- ただし theme の意味定義はまだ弱く、`QSS` と個別実装が混在している
- まずは背景色を起点に、共通 surface と accent の設計を固めるのが最短
- `applyDCCTheme()` が現在テーマを保持し、Dock / TimeCode / Render Center はその accent / text / surface を参照し始めている
- `StatusBar` / `Undo History` / `Project Health Dashboard` / `Performance Profiler` / `Secondary Preview` も palette ベースに寄せ始めている
- `ProjectManager` / `Inspector` / `Console` の直書き `QSS` をさらに削減し、検索欄・ログ一覧・プレビュー・ステータス表示を palette / paintEvent へ寄せ始めている

## Validation Checklist

- 背景色・パネル色・選択色が UI 全体で揃って見える
- `QSS` を大きくいじらなくても主要 widget の見た目を変えられる
- `Render Queue` と `Inspector` の階層感が一致する
- 画面遷移時に「別アプリに見える」差が出ない
- theme 変更で既存操作系が壊れない
