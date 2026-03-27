# マイルストーン: テキストレイヤー コンポジットエディタ内編集

> 2026-03-27 作成

## 現状サマリー

`ArtifactTextLayer` は既に `QTextDocument` ベースで 1 枚の `QImage` にラスタライズできる。
また、`text / font / stroke / shadow / tracking / alignment` などの基本プロパティは揃っている。

一方で、コンポジットエディタ上での「直接編集」はまだ未完成で、現状はプロパティパネル経由での編集が中心。
`TextGizmo` もハードコードのデモ実装に近く、テキスト内容・カーソル・選択・IME・確定/取消の編集導線は layer データに結びついていない。

このマイルストーンは、**Text Animator ではなく Text Layer の inline edit** に絞って、コンポジットエディタ内での編集体験を成立させることを目的にする。

---

## Scope

- `Artifact/src/Layer/ArtifactTextLayer.cppm`
- `Artifact/include/Layer/ArtifactTextLayer.ixx`
- `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`
- `Artifact/include/Widgets/Render/ArtifactTextGizmo.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- 必要に応じて `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`

## Non-Goals

- AE 風 Text Animator の全面実装
- glyph 単位の per-character animation 完成
- テキストレイアウトエンジンの全面再設計
- プロパティパネル廃止

## Background

今の Text Layer は「描画はできるが、編集中の状態が弱い」構造になっている。
`updateImage()` により最終的なレンダリング結果は作れるが、編集 UI は外側のプロパティパネルに寄っている。

直接編集を入れるときは、描画結果そのものをいじるのではなく、`setText()` / `setLayerPropertyValue()` を通じて layer モデルを更新し、その結果を `updateImage()` で再ラスタライズする流れを守る。

---

## Phase 1: Edit Mode / Commit Flow

- 目的:
  - テキストレイヤーをダブルクリック等で編集モードに入れる
  - 編集開始 / 確定 / 取消 を明確にする

- 作業項目:
  - コンポジットエディタ上で text layer を選択した状態から edit mode へ入る導線
  - 編集中は transform gizmo と通常ヒットテストを抑制
  - Enter で commit, Escape で cancel
  - フォーカス喪失時の扱いを定義

- 完了条件:
  - 編集モードの開始/終了が一貫する
  - 誤ってレイヤー移動や選択変更に飛ばない

### Progress 2026-03-27

- `ArtifactCompositionEditor` の viewport から、Text Layer をダブルクリックしたときだけ簡易編集ダイアログを開く導線を追加
- 編集確定時は `ArtifactTextLayer::setText()` に流し込み、`changed()` を emit して再描画を促す
- まだ in-canvas caret / selection / IME は未実装で、Phase 1 の最小入り口だけ作成済み

### Progress 2026-03-27 (later)

- コンポジットエディタ内での編集対象が Text Layer かどうかをダブルクリック時に判定し、誤って通常操作へ落ちるケースを避ける方向に整理した
- inline edit の最終ゴールは、プロパティパネルと矛盾しない `setText()` / `changed()` / 再描画の最小ループを維持したまま、caret / selection / IME を段階追加すること
- 確定操作を `Ctrl+Enter` からも受けられるようにして、Text Layer 編集の commit flow を少し強めた
- 編集ダイアログ起動時に全文選択するようにして、置き換え入力の初動を軽くした
- `Escape` で明示的に cancel できるようにして、ダイアログの終了操作を commit / cancel の2系統に整理した

## Phase 2: In-Canvas Text Input

- 目的:
  - テキストをコンポジットエディタ内で直接打てるようにする

- 作業項目:
  - caret / selection の表示
  - 文字入力 / Backspace / Delete / Enter / Tab の扱い
  - IME 入力の受け口
  - clipboard copy / paste
  - undo / redo との接続

- 完了条件:
  - 新規テキストをその場で入力できる
  - 既存テキストの一部編集ができる
  - 日本語 IME を含む入力で破綻しない

## Phase 3: Box / Bounds Editing

- 目的:
  - テキストの配置領域を画面上で追えるようにする

- 作業項目:
  - text box bounds の表示
  - wrap / alignment の視覚化
  - 位置・サイズハンドルとの同期
  - anchor / center の表示補助
  - `TextGizmo` の仮ハンドルを実データに接続

- 完了条件:
  - テキスト領域の大きさが画面上で分かる
  - box edit と layer transform の関係が追える

## Phase 4: Inspector / Property Sync

- 目的:
  - inline edit と property panel が同じ状態を参照するようにする

- 作業項目:
  - property editor の text / font / stroke / shadow / alignment と同期
  - 編集中の current text / selection 状態表示
  - edit mode 中の property change を安全に反映
  - 編集対象レイヤーの強調表示

- 完了条件:
  - プロパティパネルからの変更と in-canvas edit が矛盾しない
  - 現在どの text layer を編集しているか分かる

## Phase 5: Polish / UX

- 目的:
  - 実運用で使える編集感に寄せる

- 作業項目:
  - caret のズーム依存補正
  - selection / cursor の見やすさ調整
  - commit 直前のプレビュー表示
  - ダブルクリック / ショートカット / ストロークの再編集導線

- 完了条件:
  - 文字編集が遅く感じない
  - テキストの編集状態が常に把握できる

---

## Recommended Order

1. Phase 1: Edit Mode / Commit Flow
2. Phase 2: In-Canvas Text Input
3. Phase 3: Box / Bounds Editing
4. Phase 4: Inspector / Property Sync
5. Phase 5: Polish / UX

---

## Validation Checklist

- [ ] text layer をコンポジットエディタ内で編集開始できる
- [ ] Enter / Escape で commit / cancel できる
- [ ] IME を含む文字入力が破綻しない
- [ ] 編集中に transform gizmo が邪魔しない
- [ ] property editor と inline edit の結果が一致する
- [ ] テキスト領域の bounds が画面上で分かる
