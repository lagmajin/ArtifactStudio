# Implementation Plan: Viewport Pane Manager Migration (M-VP-2)

更新日: 2026-06-28
関連:
- `docs/planned/IMPLEMENTATION_PLAN_MULTI_VIEWPORT_2026-06-27.md`
- `docs/planned/MILESTONE_MULTI_VIEWPORT_LAYOUT_2026-06-01.md`
- `docs/planned/MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md`

---

## 目的

現在の multi-viewport 試作は `ArtifactCompositionEditor.cppm` 内で `QSplitter + 複数 CompositionViewport` により

- `1 View`
- `2-Up`
- `4-Up`

を切り替えられる段階まで到達した。

ただしこの方式は、`CompositionViewport` が native window / render host 的な責務を持つため、Qt の child widget として分割したときに次の問題を抱えやすい。

- 空状態 overlay の click / focus / Z-order が不安定
- active pane 管理が widget focus 任せになりやすい
- overlay, gizmo, HUD, drag interaction の pane ごとの責務が散る
- splitter レイアウト都合が render / hit-test / input routing に漏れる
- 将来の `1+3`, `2x2 asymmetric`, camera assignment, synchronized zoom で状態管理が破綻しやすい

このメモの目的は、試作段階の `QSplitter` 方式から、DCC らしい `pane manager` 方式へ移行する設計方針を定義すること。

---

## 結論

本命構成は

- `1つの editor/viewer host`
- `1つの pane layout manager`
- `複数 pane の矩形・camera・view state を host が管理`
- `overlay / active pane / hit-test / HUD を host が一元管理`

とする。

`QSplitter` は最終形ではなく、短期試作または比較検証用に留める。

---

## 設計原則

### 1. pane は widget ではなく state と rect として扱う

pane の正体を「子 QWidget」ではなく、以下の state 集合として扱う。

- pane id
- viewport rect
- camera assignment
- zoom / pan / orientation
- compare mode / reference frame
- active / highlighted state
- per-pane overlay options

UI の矩形分割は `PaneLayoutState` が持ち、Qt layout には依存しない。

### 2. overlay は native viewport の子にしない

overlay / empty state / view label / orientation gizmo / HUD / pane border は

- editor host の sibling overlay
- または host 自身の owner-draw

で管理する。

`CompositionViewport` の子 QWidget として載せない。

### 3. active pane は focus ではなく明示 state

active pane は `mouse enter` や `mouse press` を pane rect に hit-test して決める。

- white border
- stronger gizmo emphasis
- shortcut target
- footer / toolbar binding target

は `activePaneId` を唯一の参照元にする。

### 4. camera / projection / view transform は pane ごとに独立

`4-Up` では少なくとも以下を pane ごとに独立させる。

- camera layer assignment
- ortho / persp
- zoom / pan
- view orientation
- fit / fill / 100%

共有してよいのは composition, playback frame, display option presets のみ。

### 5. render host と pane layout を分離する

将来的に

- 複数 native surfaces
- 1 native surface + multiple sub-viewports

のどちらにも進めるよう、pane manager は render backend 非依存にする。

---

## 推奨アーキテクチャ

### A. ArtifactCompositionEditor

責務:

- toolbar / footer / menu binding
- active pane への command dispatch
- composition 切替の上位 orchestration

持つもの:

- `ArtifactViewportPaneHost* paneHost_`
- `ArtifactViewportOverlayHost* overlayHost_` または owner-draw overlay layer

### B. ArtifactViewportPaneHost

新規の中心 widget。

責務:

- pane layout の保持
- pane rect の算出
- pane hit-test
- active pane の更新
- render request の dispatch
- resize 時の pane rect 再計算

持つもの:

- `std::array<PaneState, 4> panes_`
- `ViewportLayoutMode layoutMode_`
- `int activePaneId_`

### C. PaneState

```cpp
struct PaneState {
  int paneId = 0;
  QRect rect;
  CompositionRenderController* controller = nullptr;
  std::optional<CameraLayerID> cameraLayerId;
  float zoom = 1.0f;
  QPointF pan;
  ArtifactCore::ViewOrientationHotspot orientation =
      ArtifactCore::ViewOrientationHotspot::Front;
  bool visible = false;
};
```

### D. CompositionRenderController

責務は「1 pane 分の render/view state」に限定する。

controller 自体は pane-aware でよいが、layout を知らないようにする。

### E. Overlay Host

責務:

- empty composition card
- pane border
- active pane highlight
- 3D orientation label
- pane camera label (`Top`, `Left`, `Custom`, etc.)
- compare HUD / debug HUD

overlay は pane host の rect 情報を読むだけにする。

---

## レイアウトモデル

```cpp
enum class ViewportLayoutMode {
  Single,
  TwoUpHorizontal,
  FourUp,
  OneLargeThreeSmall
};
```

MVP では以下だけでよい。

- `Single`
- `TwoUpHorizontal`
- `FourUp`

`FourUp` の既定 camera assignment:

- pane 0: Perspective
- pane 1: Top
- pane 2: Front
- pane 3: Right

これは Maya 的な期待値に寄せるための default であり、固定ではない。

---

## 入力ルーティング

### mouse

1. pane host が cursor 位置を pane rect に hit-test
2. `activePaneId` を更新
3. 対象 pane controller にイベントを変換して送る

### keyboard

- zoom in/out
- fit/fill/100
- view orientation shortcut
- compare mode toggle

は常に active pane に送る。

### drag and drop

drop 位置の pane を先に確定し、その pane の camera / transform / insertion context を使う。

---

## Overlay / Empty State 方針

空状態 UI は pane 0 の render widget 子ではなく、pane host 座標系に置く。

ルール:

- composition がない: host 全体に 1 枚の empty state
- composition はあるが layer がない: active pane または primary pane 上に guidance
- pane 単位の label / border は host owner-draw

こうすることで native child surface の Z-order 問題を避ける。

---

## QSplitter 方式の評価

### 良い点

- 実装が速い
- 2-Up / 4-Up の見た目確認が簡単
- pane resize を Qt に任せられる

### 悪い点

- native viewport と child overlay の相性が悪い
- pane state が widget tree に漏れる
- active pane / command target の責務が曖昧
- asymmetric layout や camera presets を入れ始めると責務が Editor に逆流する
- 将来 1 surface 描画に切り替えづらい

結論として、`QSplitter` は「正しさ検証用プロトタイプ」には向くが、「最終的な DCC viewport shell」には向かない。

---

## 実装段階

### Phase 1: state 分離

目的:

- 現行 `ArtifactCompositionEditor.cppm` から pane state を抽出

作業:

- `ViewportLayoutMode`
- `PaneState`
- `activePaneId`
- pane rect 計算 helper

を `Impl` 直下に切り出す。

この段階では描画はまだ `QSplitter` でもよい。

### Phase 2: overlay host 分離

目的:

- empty state / HUD / pane border を viewport 子から追い出す

作業:

- `EmptyCompositionOverlayWidget` を host overlay 化
- active pane border / pane labels を owner-draw 化
- orientation widget の pane anchor 化

### Phase 3: input routing 集約

目的:

- active pane 更新と command dispatch を host 側へ集約

作業:

- mouse hit-test を host で実施
- toolbar/footer は active pane の state を読む
- shortcuts は active pane target に送る

### Phase 4: QSplitter 依存除去

目的:

- pane rect を host の算出値だけで決める

作業:

- `QSplitter` を撤去
- host `resizeEvent` で pane rect 再計算
- child viewport が必要なら `setGeometry(rect)` で直接配置

### Phase 5: single-surface option 準備

目的:

- 将来 1 native surface に複数 viewport を描ける形へ寄せる

作業:

- pane controller と render surface binding を分離
- render command を pane rect 指定で流せるよう整理

---

## 最小実装ファイル案

新規:

- `Artifact/include/Widgets/Render/ArtifactViewportPaneHost.ixx`
- `Artifact/src/Widgets/Render/ArtifactViewportPaneHost.cppm`

変更:

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/include/Widgets/Render/ArtifactCompositionEditor.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`

必要なら後続:

- `ArtifactCore/include/Viewport/ViewportLayoutState.ixx`
- `ArtifactCore/src/Viewport/ViewportLayoutState.cppm`

---

## 非目標

この段階では以下はやらない。

- pane ごとの別 composition 表示
- pane ごとの別 playback frame
- multi-window / multi-monitor pane detach
- pane ごとの完全独立 tool mode
- Diligent backend の広い再設計

---

## 判断

今後の multi-viewport は

- 「Qt splitter で view を並べる」

ではなく

- 「Composition Editor が複数 pane を持つ viewer shell になる」

方向で進める。

試作 `QSplitter` 版は、UI の必要性検証とショートカット確認には有効だった。
ただし本実装は `pane manager + overlay host + active pane routing` を中核に据えるべきである。
