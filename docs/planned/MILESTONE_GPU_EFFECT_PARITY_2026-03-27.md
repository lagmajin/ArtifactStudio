# GPU Effect Parity Milestone

CPU 側の effect 実装を検証・開発用の参照実装として残しつつ、同等の見た目と挙動を GPU 側で実装していくためのマイルストーン。

この文書では、CPU path を捨てずに保守しながら、GPU path を正規経路として育てる方針を定義する。

## Goal

- CPU effect は reference / debug / validation 用として残す
- GPU effect は通常描画の正規経路として実装する
- 同じ effect 名でも CPU と GPU の見た目差分を減らす
- GPU path の失敗時に CPU path へ安全に戻せる

## Scope

- `ArtifactCore/src/Graphics/Shader/*`
- `ArtifactCore/include/Graphics/Shader/*`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/src/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Layer/*`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/Menu/ArtifactInspectorWidget.cppm`

## Non-Goals

- CPU effect を即座に廃止すること
- すべての effect を同時に GPU 化すること
- shader editor / node graph の全面実装
- 既存の debug / test widget を壊すこと

## Background

今のアプリでは、CPU effect は開発・検証の基準として有用だが、実運用では composition editor / layer view / render queue の体感を GPU path に寄せていく必要がある。

ここで重要なのは、CPU path を「古い実装」として消すことではない。
むしろ、CPU path を仕様確認用の reference として維持し、GPU path が同じ結果を再現できているかを比較できる状態を作ることが目的になる。

## Phases

### Phase 1: CPU Reference Preservation

- 目的:
  - CPU effect を検証用の参照実装として明確に残す
  - GPU path と比較できる前提を固定する

- 作業項目:
  - CPU effect の debug / test 経路を整理する
  - effect ごとの CPU 出力を再現しやすいようにする
  - reference 用の render mode を UI から切り替えられるようにする
  - GPU で壊れたときの fallback を明示する

- 完了条件:
  - CPU effect が開発用の baseline として残る
  - CPU / GPU の切替理由を説明できる

### Phase 2: GPU Equivalent Effect Bridge

- 目的:
  - 代表 effect を GPU path へ移植する
  - CPU と同じ入力から同じ出力に近づける

- 作業項目:
  - solid / blur / overlay / color / transform 系の GPU 実装を揃える
  - effect parameter を shader constant / structured buffer に載せる
  - layer / mask / blend との接続点を固定する
  - GPU に無い effect は CPU fallback を残す

- 完了条件:
  - 少なくとも主要 effect が GPU で実用になる
  - CPU との比較で差分が小さい

### Phase 3: Parity Diagnostics

- 目的:
  - CPU と GPU の差分を見える化する

- 作業項目:
  - side-by-side compare
  - diff heatmap / overlay
  - effect cost / upload cost / shader time の表示
  - 失敗した effect 名と fallback 理由の表示

- 完了条件:
  - どの effect が GPU 側で崩れているかすぐ分かる
  - CPU fallback を使った理由が追跡できる

### Phase 4: Effect Coverage Expansion

- 目的:
  - GPU で扱う effect の範囲を広げる

- 作業項目:
  - creative effect pack
  - text / image / solid / vector / matte への拡張
  - generator / effector の GPU 化
  - inspector からの GPU effect 編集導線

- 完了条件:
  - effect タイプごとに GPU path が増えている
  - CPU path は参照として維持されている

### Phase 5: Production Cutover with CPU Fallback

- 目的:
  - 通常運用は GPU、検証は CPU の形に安定させる

- 作業項目:
  - render queue / preview / solo view で GPU を既定にする
  - CPU fallback の条件を限定する
  - diagnostic mode でのみ CPU path を強く使う

- 完了条件:
  - GPU effect が通常経路として定着する
  - CPU path は壊れたときの比較・検証用に残る

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

## Current Status

- CPU effect はまだ参照実装として価値がある
- GPU path は composition / render / preview 側の高速化と見た目 parity の両方に効く
- まずは CPU を残したまま GPU equivalent を増やす方針が安全

## Color Pipeline Priority

GPU effect parity の中では、次の順で color 系を扱うのが自然。

1. `Exposure` / `Contrast`
2. `Lift/Gamma/Gain`
3. `Hue/Saturation`
4. `Curves`
5. `Color Balance`
6. `Color Space` / `LUT`

理由は、前段の調整ほど shader への落とし込みが単純で、後段ほど色空間の前提と診断が必要になるから。
`Color Correction / Grading` の具体的な UX と色管理前提は [`MILESTONE_COLOR_CORRECTION_2026-03-27.md`](MILESTONE_COLOR_CORRECTION_2026-03-27.md) に分離し、この文書では GPU parity の優先順を固定する。

### Suggested GPU First Steps

- `Exposure` と `Contrast` を最初の GPU 対象にする
- 次に `Lift/Gamma/Gain` を載せる
- `Hue/Saturation` と `Curves` で見た目 parity を詰める
- `Color Space` と `LUT` は working / input / output / display の整理後に入れる

