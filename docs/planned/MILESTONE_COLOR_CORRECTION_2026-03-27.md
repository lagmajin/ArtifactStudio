# Color Correction / Grading Milestone

`Artifact` の color correction / grading 系を、CPU 参照実装を残しながら GPU 側へ寄せていくためのマイルストーン。

ここでいう color correction は、単なる `Solid` の色変更ではなく、露出・コントラスト・色相・彩度・白黒・カーブ・LUT などの調整を含む。

## Goal

- CPU 側の color correction は検証・比較用として残す
- GPU 側の color correction を通常経路として実装する
- Inspector から調整値を触った結果が timeline / preview にすぐ反映される
- 調整前後の差分を追いやすくする

## Color Management Basics

最低でも次の前提を常に意識する。

- 作業色空間
- 入力色空間
- 出力色空間
- ガンマ / リニア
- sRGB 表示

この milestone では、少なくとも「どの色空間で計算して、どの色空間で表示しているか」を説明できる状態を先に固定する。
UI / shader / cache のどこかで暗黙変換が混ざると、CPU / GPU の差分が追えなくなるため。

## Scope

- `ArtifactCore/src/Graphics/Shader/*`
- `ArtifactCore/include/Graphics/Shader/*`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/src/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/Menu/ArtifactInspectorWidget.cppm`
- `Artifact/src/Layer/*`

## Non-Goals

- フル機能の DaVinci Resolve 互換 grading panel
- ACES / OCIO の全面導入
- RAW 現像パイプラインの置き換え
- CPU 実装の即廃止

## Background

今のアプリには color 系の調整余地はあるが、調整 UI、GPU shader、preview、timeline の接続がまだ分散している。

この milestone の方針は、CPU 実装を「壊れた時の比較用 baseline」として残しつつ、GPU 側に同等の見た目を作ること。
`CPU effect は残す`、`GPU effect は正規経路` という方針は [`MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md`](MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md) と共通にする。

特に color correction は、見た目の差が effect 実装差なのか、色空間変換差なのかを切り分けないと評価できない。
そのため、`Working space` / `Input space` / `Output space` / `Display space(sRGB)` を別概念として扱う。
この文書は color UX と color management の milestone であり、GPU 実装の優先順位は [`MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md`](MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md) 側で固定する。

## Phases

### Phase 1: Basic Correction Controls

- 目的:
  - 代表的な調整を UI と property に揃える

- 作業項目:
  - exposure
  - contrast
  - saturation
  - temperature / tint
  - hue shift
  - black / white point

- 完了条件:
  - Inspector から主要な correction 値を触れる
  - CPU / GPU のどちらでも同じ入力を扱える

### Phase 2: GPU Grade Path

- 目的:
  - 調整値を shader へ流し込む

- 作業項目:
  - working color space / input space / output space の変換を固定する
  - linear / gamma の往復をどこで行うかを明示する
  - sRGB 表示までの最終変換を統一する
  - color matrix / lift-gamma-gain / tone mapping の GPU 化
  - effect parameter を shader constants に固定
  - preview / render / solo view で共通の出力を使う
  - CPU fallback を維持する

- 完了条件:
  - よく使う color correction が GPU で動く
  - CPU との差が視覚的に追える

### Phase 3: Curves / LUT

- 目的:
  - 調整の幅を広げる

- 作業項目:
  - RGB / luma curves
  - 1D / 3D LUT
  - pre/post correction の切替
  - effect stack との順序整合

- 完了条件:
  - カーブと LUT が GPU path に載る

### Phase 4: Diagnostics / Compare

- 目的:
  - CPU / GPU の差を見える化する

- 作業項目:
  - before/after compare
  - numeric readout
  - effect cost / shader cost 表示
  - working space / display space の違いを表示する
  - fallback 理由表示

- 完了条件:
  - 調整の結果とコストを見ながら作業できる

### Phase 5: Workflow Integration

- 目的:
  - 実制作で使える導線にまとめる

- 作業項目:
  - preset 保存 / 検索
  - batch apply
  - selected layer / group への適用
  - keyframe / timeline 連携

- 完了条件:
  - color correction が個別の調整で終わらず、制作フローに乗る

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

## Current Status

- GPU effect parity の一部として進めるのが自然
- まずは小さめの correction controls から入るのが低コスト
- CPU path は debug / compare 用に残す
