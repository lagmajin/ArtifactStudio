# UI Layout Undo History Milestone

**作成日:** 2026-06-07  
**ステータス:** 🟡 進行中（Phase 0: ADS 永続化 完了 2026-06-15 / Phase 1-5 未着手）  
**関連コンポーネント:** Window Manager, Dock Layout, Tab State, UndoManager, ADS (Advanced Docking System)

---

## 概要

UI の開閉や分割、タブ移動などのレイアウト操作を undo / redo 可能にするためのマイルストーンです。

誤ってパネルを閉じた場合でも、`Ctrl+Z` で戻せるようにします。  
目的は「編集内容だけでなく、作業空間の状態も履歴として扱える」ことです。

---

## 背景

現状は編集系の undo はあっても、UI 状態は履歴に入っていません。

- タイムラインを閉じても undo で戻せない
- 分割レイアウトの変更が履歴化されない
- タブ移動や dock の再配置が取り消せない

これだと、操作ミスの復帰にメニュー操作が必要になり、作業を止めやすくなります。

---

## 目標

- panel close を undoable にする
- tab move を undoable にする
- split / merge / dock rearrange を undoable にする
- window visibility を履歴に載せる
- 既存の編集 undo と混ざっても破綻しないようにする

---

## 対象操作

- panel open / close
- dock show / hide
- tab reorder
- tab detach / reattach
- split add / remove
- layout preset apply

---

## Phase 構成

### Phase 0: ADS Layout Persistence（前提基盤）

undo を載せる前に、ADS のレイアウト状態（dock 配置、タブグループ、splitter、floating 位置）が永続化されていないと undo の意味が薄いため、まず永続化を整備した。

実装メモ (2026-06-15):
- `UiLayoutState`（`ArtifactCore/include/UI/LayoutState.ixx`）に `dockState`（QByteArray）フィールドを追加。構造 version を 2 へ。
- `toJson/fromJson/saveToSettings/loadFromSettings/saveToStore/loadFromStore` の全てに `dockState` の読み書きを追加。
- `ArtifactMainWindow`（`Artifact/include/Widgets/ArtifactMainWindow.ixx` / `src/Widgets/ArtifactMainWindow.cppm`）に `saveDockManagerState()` / `restoreDockManagerState()` を追加。内部で `CDockManager::saveState()` / `restoreState()` を呼ぶ。
- `AppMain.cppm` の起動時復元（`setStartupLayoutFrozen(false)` の直前）と終了時保存（`aboutToQuit`）に dockState の読み書きを接続。version 不一致時・restore 失敗時のリセット処理にも `dockState` キーの削除を追加。
- **復元タイミングの制約**: ADS は「全ての dock が DockManager に登録された後」でないと restore できない。そのため `setStartupLayoutFrozen(false)` の直前（全 dock 追加後）で呼ぶ。
- 古いレイアウト（dockState 無し）は `dockState.isEmpty()` でスキップされ、後方互換を保つ。

完了条件:

- ADS レイアウトがアプリ再起動後も復元される

### Phase 1: Layout State Snapshot

- 現在の window / dock / tab / splitter 状態を snapshot 化する
- undo に使える最小の state diff を定義する
- UI state と edit state を分離する

完了条件:

- layout state を保存・復元できる

### Phase 2: Undoable Layout Commands

- close panel を command 化する
- open panel を command 化する
- tab move を command 化する
- split operation を command 化する

完了条件:

- `Ctrl+Z` で UI の変更が戻る

### Phase 3: Window Menu Integration

- `Window` メニューの操作を undo history と同期する
- 再表示操作も履歴経由で扱う
- shortcut と menu の挙動を揃える

完了条件:

- menu からの操作も undo で巻き戻せる

### Phase 4: State Granularity Policy

- どこまでを 1 undo step にするかを固定する
- drag で連続変化する操作はまとめる
- transient hover や resize preview は履歴に入れない

完了条件:

- undo 粒度が荒すぎず細かすぎない

### Phase 5: Recovery and Safety

- layout が壊れた場合の復旧手段を用意する
- safe fallback layout を保持する
- undo stack が空でも再表示できる保険を持つ

完了条件:

- 失敗しても window を失わない

---

## リスクと留意点

- UI 状態を全部 undo 化すると履歴が肥大化しやすい
- ユーザーが「編集 undo」と「レイアウト undo」を混同する可能性がある
- 一部のレイアウト変更は複数 dock に跨るため、差分設計が必要

---

## 成功条件

- 閉じた panel を `Ctrl+Z` で戻せる
- タブ移動や分割操作も履歴に乗る
- 編集 undo と UI undo が同じ仕組みで扱える
- メニューから戻る操作を探さなくてよい

---

## 関連

- `docs/planned/MILESTONE_UNDO_AND_AUDIO_PIPELINE_COMPLETION_2026-03-25.md`
- `docs/planned/MILESTONE_UI_UX_UNIFICATION_2026-03-28.md`
- `docs/planned/MILESTONE_RESPONSIVE_LAYOUT_COMPOSITION_2026-06-05.md`
