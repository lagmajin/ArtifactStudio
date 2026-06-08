# UI Layout Undo History Milestone

**作成日:** 2026-06-07  
**ステータス:** 計画中  
**関連コンポーネント:** Window Manager, Dock Layout, Tab State, UndoManager

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
