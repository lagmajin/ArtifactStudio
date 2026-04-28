# MILESTONE: Text Layer UX Refresh

**Date**: 2026-04-27
**Status**: In Progress
**Priority**: High
**Related**: `docs\MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md`, `ArtifactCore\docs\MILESTONE_TEXT_SYSTEM_2026-03-12.md`

---

## 概要

テキストレイヤーは描画自体は可能だが、コンポジットエディタ上の操作感がまだ After Effects 系の期待値に届いていない。
特に現状は live editor 側で `ArtifactTextGizmo` が使われておらず、実際の操作は `TransformGizmo` が担っているため、
テキスト特有の UX 改善は active gizmo 側へ直接入れる必要がある。

---

## 現状整理

| 項目 | 現状 |
|---|---|
| live gizmo | `Artifact\src\Widgets\Render\TransformGizmo.cppm` |
| unused text gizmo | `Artifact\src\Widgets\Render\ArtifactTextGizmo.cppm` |
| paragraph box property | `text.maxWidth`, `text.boxHeight`, `text.wrapMode` |
| 現在の問題 | scale handle が generic layer scale として働き、text box 編集にならない |
| 期待 UX | text layer の side/corner handle が paragraph box 編集として動く |

---

## フェーズ

### Phase 1: Paragraph Box Resize on Active Gizmo
**目標**: `TransformGizmo` 上で text layer の scale handle を paragraph box 編集に変える。

- [x] active gizmo が `TransformGizmo` であることを確認
- [x] `ArtifactTextGizmo` を触っても live UX が改善しないことを確認
- [x] text layer の side/corner drag を local-space paragraph box resize に切り替え
- [x] left/top drag 時に opposite edge を維持する位置補正を追加
- [x] undo に `text.maxWidth` / `text.boxHeight` を含める
- [x] handle visual の text-specific affordance 調整

### Phase 2: Point Text / Box Text Behavior
**目標**: point text と paragraph text の変換ルールを整理する。

- [ ] point text 横ドラッグ時の paragraph 化ポリシー明確化
- [ ] auto-height box の扱いを UX として明文化
- [ ] double-click / mode toggle の導線整理

### Phase 3: Direct Text Editing Feel
**目標**: on-canvas 編集の分かりやすさを上げる。

- [x] box edge / baseline / paragraph bounds の視覚化（paragraph box guide を active gizmo に追加）
- [ ] text tool 選択時の affordance 強化
- [x] text tool options bar の font / size / bold / italic / underline を selected text layer に live 反映
- [x] alignment / vertical alignment / wrap の live 適用

### Phase 4: Animator / Timeline Bridge
**目標**: text animator 系の UI を text-layer UX と矛盾なく接続する。

- [ ] dedicated animator panel
- [ ] animator property track の timeline 連携
- [ ] text editing UX と animator UX の役割分離

---

## Phase 1 実装メモ

- world-space bbox ベースの generic scale ではなく、text layer のみ local-space で handle drag を解釈する
- `textEffectMargin` 分を差し引いた content width / height を `text.maxWidth` / `text.boxHeight` に反映する
- left/top/corner drag は、反対側の edge / corner が world 上で動かないように `transform.position` を補正する
- generic scale undo だけでは text box 編集を戻せないため、undo snapshot に paragraph box size を含める
- text layer 選択時は paragraph box の内側ガイドを描画し、generic scale ではなく text box 編集であることを視覚的に示す
- MainWindow に tool options bar を実際に配置し、selected text layer と font / style 状態を双方向同期する
- tool options bar から horizontal / vertical alignment と wrap mode も直接変更できるようにする

---

## 成功条件

1. text layer の横ハンドルが layer scale ではなく paragraph width を変える
2. text layer の縦ハンドルが paragraph height を変える
3. left/top/corner drag でも box の反対側が暴れない
4. undo / redo で text box サイズ変更が戻る
5. 既存の非 text layer scale 操作はそのまま維持される
