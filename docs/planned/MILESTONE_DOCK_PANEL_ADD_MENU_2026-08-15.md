# Dock Panel Add Menu / Frequently Used Panels

**最終更新:** 2026-08-15
**ステータス:** Not Started

## Update 2026-08-15 — Phase 1 static audit

- `ArtifactMainWindow.cppm` の `ArtifactDockManager` が objectName を dock ID として登録し、空 ID／重複 ID を拒否することを確認。
- `dockWidgets()` の一覧、既存 dock の `show()`／`raise()`／`setAsCurrentTab()` 経路を確認し、追加メニューは既存 dock を再生成せず再表示・activate する責務で実装できる。
- 現行登録面と生成 factory の一覧を次の Phase 2 で descriptor 化する。UI 追加、build、runtime 検証は未実施。

## Update 2026-08-15 — Phase 2 minimal re-show menu

- 既存の View > Window Panels メニューに「パネルを追加／再表示」サブメニューを追加した。
- 登録済み dock のみを一覧し、選択時は既存 dock を表示して activate する。重複生成や表示名を永続 ID として保存する処理は追加していない。
- タイトルバー右上の専用 `+` ボタン、カテゴリ descriptor、最近使った順／お気に入りは次段階。

## Update 2026-08-15 — Phase 3 settings-backed panel lists

- View > Window Panels に最近使ったパネルとお気に入りのサブメニューを追加。
- `Workspace/RecentDockIds`（最大8件）と `Workspace/FavoriteDockIds` を Dock ID で保存し、存在しない ID は表示時に除外する。
- title-bar `+` の専用ホストとカテゴリ descriptor は未実装。既存 View メニューで先行提供する。

## Update 2026-08-15 — Phase 2 category grouping

- 「パネルを追加／再表示」を Project / Assets、Editing、Animation、Render / Diagnostics、Other に分類した。
- 既存の登録済み Dock のみを分類し、カテゴリに該当しないものは Other に残す。Dock 本体の責務や生成ライフサイクルは変更していない。

## 目的

ドック右上から、登録済みのパネル／ウィジェットを検索・追加・再表示できる導線を提供する。ユーザーが現在のワークスペースへよく使う面を段階的に追加できる状態を目指す。

## 背景

現在のドック右上にはオプション系の操作があるが、パネル追加と既存パネルの表示・配置管理が同じ場所に集まりやすい。`ArtifactWorkspaceWidget` / DockManager のレイアウト所有責務を維持したまま、追加操作だけを明示的な `+` 導線として分離する。

## スコープ

- ドックタイトルバー右上の「パネルを追加」入口
- 登録済みパネルのカテゴリ分けと表示名
- 最近使ったパネル、お気に入りパネル、全パネル一覧
- 選択したパネルのドックへの追加、既存タブの再表示、アクティベート
- パネル ID を使った状態管理。表示名を永続 ID にしない

### 初期候補パネル

Project View、Asset Browser、Inspector / Property Editor、Components、Effects、Timeline、Dope Sheet、Render Queue、Profiler、Debug Console。候補は実際の dock registry と照合して確定し、未登録の面をメニューだけに表示しない。

## UI 方針

- タイトルバー右上に小さな `+` ボタンを置く。
- `+` は「パネルを追加」に限定し、表示・配置・閉じる操作は既存のオプションメニューに残す。
- メニューは「最近使ったパネル」「お気に入り」「カテゴリ別の全パネル」で構成する。
- 未配置なら追加、配置済みなら既存パネルをアクティベートする。
- お気に入り登録はメニュー内で行う。常時表示バッジは追加しない。
- 狭いドックでもタイトルや既存操作を圧迫しない。必要なら検索付きポップアップへ切り替える。
- QtCSS、QColorDialog、新規のグローバル signal/slot 配線は追加しない。

## 責務境界

| 責務 | 所有者 |
|---|---|
| パネルの永続 ID、表示名、カテゴリ、生成可否 | Dock registry / panel descriptor |
| パネルを追加・再表示・アクティベート | `ArtifactWorkspaceWidget` / DockManager facade |
| 右上の追加メニュー表示 | Dock title-bar UI |
| 最近使った順・お気に入り状態 | Workspace layout/settings の既存保存境界 |
| 個別パネルの編集内容 | 各パネル本体 |

追加メニューは個別パネルの内部状態や編集責務を持たない。Components、Effects、Properties などの責務分離も維持する。

## 実装フェーズ

### Phase 1: Registry 契約の確認

- [ ] 現行の dock 登録箇所、dock ID、表示名、生成ライフサイクルを一覧化
- [ ] `ArtifactWorkspaceWidget` / DockManager facade の再表示・activate API を確認
- [ ] 未登録パネル、重複表示名、表示名依存の検索を洗い出す

### Phase 2: 追加メニューの最小導線

- [ ] ドックタイトルバー右上に `+` 入口を追加
- [ ] 登録済みパネルだけをカテゴリ別に表示
- [ ] 未配置パネルの追加と配置済みパネルのアクティベートを同じ選択操作で扱う
- [ ] 既存のオプションメニューとの重複項目を整理

### Phase 3: 最近使ったパネルとお気に入り

- [ ] パネル ID ベースで最近使った順を保存
- [ ] パネル ID ベースのお気に入りを保存
- [ ] 保存・復元時に存在しない ID を安全に無視
- [ ] 初期状態、壊れた設定、重複追加時の挙動を定義

### Phase 4: 密度とアクセシビリティの調整

- [ ] 狭い幅でのタイトルバー崩れを確認
- [ ] キーボード操作、フォーカス、Esc による閉じる操作を確認
- [ ] tooltip / accessible name を設定
- [ ] ダーク／ライトテーマの既存 token と視認性を確認

## 受け入れ条件

- [ ] 右上の追加入口から、登録済みパネルを一覧できる
- [ ] 未配置パネルを現在のワークスペースへ追加できる
- [ ] 配置済みパネルを重複生成せずアクティベートできる
- [ ] 最近使ったパネルとお気に入りを安定したパネル ID で保持できる
- [ ] ワークスペース保存・復元後も既存パネルの状態が壊れない
- [ ] Project / Asset / Inspector / Components / Effects / Timeline の責務境界を侵食しない
- [ ] 既存の close / float / tab 操作を壊さない
- [ ] QtCSS、QColorDialog、新規グローバル signal/slot、常時表示バッジを導入しない
- [ ] 実装後、追加・再表示・保存復元・狭幅表示を runtime 確認する

## 非スコープ

- 新しいパネル本体の実装
- ドックレイアウトエンジンや Qt ADS の置き換え
- ワークスペースプリセット全体の再設計
- パネル内部のプロパティ構成変更
- サブモジュールや Qt ADS 本体の変更

## 関連文書

- `docs/WIDGET_MAP.md`
- `docs/planned/MILESTONE_TOP_LEVEL_WIDGET_ARCHITECTURE_2026-07-13.md`
- `docs/planned/MILESTONE_WORKSPACE_MANAGER_2026-03-29.md`
- `docs/planned/MILESTONE_WORKSPACE_PRESETS_2026-04-10.md`

## 検証方針

ビルド、CMake、テスト、runtime 確認は実装後にユーザー許可を得て実施する。本マイルストーン作成時点ではコード変更と検証は行わない。

## Update 2026-08-15 — Phase 4 accessibility metadata

- 最近使用／お気に入り／追加・再表示 submenu に accessible name／description を設定。
- 個別 Dock action に表示・activate の tooltip を設定し、既存のメニューキーボード操作を維持した。

## Update 2026-08-15 — top chrome `+` entry

- `ArtifactMenuBar` の右上 corner widget に `+` ボタンを追加。
- ボタンのポップアップは表示時に登録済み Dock を再列挙し、選択時は既存 MainWindow の表示／activate API を呼ぶ。
- ADS 内部 title bar の private API には依存せず、既存の MainWindow chrome 内で実装した。

## Update 2026-08-15 — top chrome menu parity

- `+` メニューにも最近使用／お気に入りの submenu を追加し、表示時に `QSettings` と登録済み Dock を再読込するようにした。
- `View > Window Panels` と同じ ID ベースの表示・activate／最近使用更新経路を使い、未登録 Dock は除外する。
