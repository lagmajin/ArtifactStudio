# Milestone: AE Style Mask Editing Grammar

> 2026-06-02 作成

**最終更新:** 2026-08-15

## 2026-08-15 現行コード照合

- `RotoMask` に頂点追加／削除、位置・入出タンジェント、頂点補間、feather、mask mode、キーフレーム時刻評価、JSON／clone／keyframe コピーの基盤がある。
- `RotoMaskEditor` に Draw／Select／Edit／Delete モード、頂点・タンジェント hit-test、ドラッグ編集、Delete、undo／redo、grid／zoom／pan、キーボードモード切替が実装されている。
- 一方、現行コード上はこの専用 editor が Timeline／Composition の通常マスク編集導線へ全面統合されていること、Shift 複数選択、辺上クリック追加、Alt による角／ベジェ切替、F／T／M／MM の AE ショートカット契約は確認できない。
- 既存 `MaskPath` 側にはマスク mode、頂点、タンジェント、キーフレーム、undo／JSON 経路があるが、RotoMaskEditor と通常マスク surface の責務統一は未完了。

**判定:** マスクデータモデルと独立した直接編集 widget は実装済み。AE 互換の操作文法、複数選択、通常 UI 統合、runtime 受入れは未完了。

After Effects のマスク編集で期待される「操作の文法」を、Artifact の Timeline / Mask surface に段階的に寄せるマイルストーン。

この文書は、頂点選択・追加・削除・変形・マスク全体移動・プロパティ呼び出し・マスクモード切替を、モーションデザイナーが迷わず扱えるようにするための上位枠とする。

---

## Goal

- 頂点編集の基本操作を AE に近い手触りにする
- `mask path` と `vertices` と `property` の文脈を分けて読めるようにする
- マスクを「ただの設定」ではなく、直接触れる編集対象として扱う
- 迷いなく `select / add / delete / move / toggle / inspect` へ進めるようにする

---

## Scope

### In

- 頂点だけ選択する操作
- 全頂点選択の案内
- 頂点追加 / 頂点削除
- 角 ⇔ ベジェ切替
- ハンドル片側だけ動かす操作
- マスク全体移動
- マスクだけ移動してレイヤーは動かさない操作
- `F / T / M / MM` 系のマスクプロパティショートカット
- `Mask Path` のキーフレーム導線
- `Add / Subtract / Intersect / None` のマスクモード切替

### Out

- 描画 backend の変更
- 新しい中央集権 signal/slot の導入
- QtCSS の追加
- 既存の編集哲学と矛盾する別系統 UI の追加

---

## Design Rules

1. 頂点編集は、まずクリックと Delete で成立すること
2. `Alt/Option` は変形の補助として一貫して使うこと
3. `M / MM / F / T` は AE の記憶に寄せて使うこと
4. 先に「触れる」こと、そのあとで「細かく調整する」こと
5. 文言は操作の結果を短く説明し、冗長なヘルプにしないこと

---

## Phases

### Phase 1: Vertex Interaction Core

- 頂点のクリック選択を整理する
- `Shift` による複数頂点選択を扱えるようにする
- `Delete` で頂点削除できるようにする
- ペンツールの辺上クリックで頂点追加できるようにする
- マスクパスの選択状態と頂点選択状態を明確に分ける

### Phase 2: Handle and Shape Conversion

- `Alt/Option` による角 ⇔ ベジェ切替を扱う
- ハンドル片側だけ動かす操作を整理する
- 頂点タイプの状態表示を整える

### Phase 3: Mask Surface Shortcuts

- `F` で feather
- `T` で opacity
- `M` で mask list
- `MM` で mask expansion など詳細
- マスクモード切替の導線を統一する

### Phase 4: Polishing and Behavior Consistency

- マスク全体移動とレイヤー移動の差を明確にする
- tooltip と inline hint を整理する
- selection / property path / undo の文脈ずれを減らす

---

## Execution Checklist

### Phase 1 Checklist

- [ ] 頂点選択と mask path 選択の境界を洗い出す
- [ ] `Shift` 複数選択の扱いを決める
- [ ] `Delete` 時の挙動を定義する
- [ ] 辺上クリックの追加操作を定義する

### Phase 2 Checklist

- [ ] `Alt/Option` の意味を全編集面で揃える
- [ ] 角 ⇔ ベジェ切替の状態を表示できるか確認する
- [ ] ハンドルの片側操作を文脈として壊さないようにする

### Phase 3 Checklist

- [ ] `F / T / M / MM` の割り当てを整理する
- [ ] mask properties の表示順を確認する
- [ ] mask mode の切替導線を揃える

### Phase 4 Checklist

- [ ] 頂点・マスク・レイヤーのドラッグ文脈が混ざらないか確認する
- [ ] tooltip が長すぎないか確認する
- [ ] undo/redo で選択文脈が破綻しないか確認する

---

## Success Criteria

- 頂点編集の基本操作が迷わず試せる
- AE で覚えた操作感が、少なくとも主要部分は通じる
- マスク編集が「設定画面」ではなく「直接操作」に見える
- 選択文脈と property 文脈が混ざりにくい
- 複数マスクや複数頂点でも挙動が読める

---

## Related Docs

- [`MILESTONE_MULTI_MASK_EDITING_SAFETY_2026-05-28.md`](./MILESTONE_MULTI_MASK_EDITING_SAFETY_2026-05-28.md)
- [`MILESTONE_MOTION_DESIGNER_EMPTY_STATE_GUIDANCE_2026-06-02.md`](./MILESTONE_MOTION_DESIGNER_EMPTY_STATE_GUIDANCE_2026-06-02.md)
- [`MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md`](./MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md)
- [`MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md`](./MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md)
- [`MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md`](./MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md)

---

## Next Step

Phase 1 の対象を `ArtifactLayerPanelWidget` / `ArtifactTimelineTrackPainterView` / mask selection 周辺に絞り、頂点選択と mask path 選択の区別を先に整理する。
そのあと `Delete` / `Shift` / 追加操作の導線を詰める。
