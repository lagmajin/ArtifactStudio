# ArtifactPr GPU Program Monitor Milestone

**最終更新:** 2026-08-31
**ステータス:** In Progress

## 目的

`ArtifactPr` の Program Monitor を、CPU の `ImageF32x4_RGBA` 合成結果を
Diligent texture として表示する preview-only GPU 経路へ段階移行する。
CPU の `SequenceCompositor` は export の正規経路および GPU 初期化失敗時の
明示 fallback として維持する。

このマイルストーンは動画 decode の GPU 化、NLE モデルから Artifact の
Composition/Layer モデルへの移行、FFmpeg export の置換を対象外とする。

## 現状と制約

- `ArtifactPr` は `ArtifactCore` を通じて D3D12 の Diligent 基礎ライブラリと
  `ImageF32x4RGBAWithCache` の texture upload を利用できる。
- swap chain と共有 GPU device の所有は `Artifact.Render.DiligentDeviceManager`
  にあり、現状は `ArtifactRender` target の一部である。
- `ArtifactRenderer` は外部 render job 用 executable であり、Program Monitor
  からリンクする renderer library ではない。
- `ArtifactCompositionRenderController` を直接利用すると AE 系 Composition/Layer
  モデルを ArtifactPr へ導入することになるため、再利用しない。
- 低レベル D3D12/Diligent の変更は推測で広げず、device 所有を一箇所に保つ。

## 完了条件

1. D3D12 と Vulkan のいずれかを明示診断付きで選択し、初期化失敗時は CPU
   Program Monitor へ復帰する。
2. `ImageF32x4_RGBA` の表示では `QImage` / `QPainter` を GPU 正常時の通常経路に
   用いない。
3. 同一 `RenderPlan` とフレーム番号で、GPU preview と CPU compositor の RGBA 値を
   比較できる。
4. resize、シーク、連続再生、device loss、空フレームでクラッシュせず、古い
   generation のフレームを表示しない。
5. export は GPU preview の成否に影響されず、既存 CPU output を維持する。

## 実装段階

### P0 — 共有 GPU foundation の分離

- `DiligentDeviceManager` と必要最小限の backend/config 依存を、
  Artifact 固有の Composition/Layer renderer と分離した static target へ移す。
- Artifact の既存 import 利用者が同じ module 名または明示的な置換先を使えるように
  し、device の二重生成を避ける。
- `ArtifactPr` はこの foundation と `ArtifactCore` の texture upload だけへ依存する。

**受入れ:** Artifact と ArtifactPr の双方で、同一プロセス内の device 所有数と
backend 選択を診断できる。

**進捗 (2026-08-31):** `ArtifactGpuFoundation` static target を追加し、
`Artifact.Render.Config` と `Artifact.Render.DiligentDeviceManager` の provider を
`ArtifactRender` から移した。ArtifactRender は foundation を public link し、
ArtifactPr は AE renderer 本体ではなく foundation を直接 link する。CMake configure
と runtime device 所有の検証は未実施。

### P1 — Program Monitor GPU present

- `ImageF32x4_RGBA` を `ImageF32x4RGBAWithCache` または同等の明示 upload API で
  `RGBA32_FLOAT` texture にする。
- フルスクリーン triangle/quad と alpha 正規化を行う最小 PSO を用意し、swap chain
  に present する。
- `SequenceCanvasWidget` は GPU widget を host し、GPU unavailable / device lost
  時だけ既存 QPainter 表示を使う。

**受入れ:** static image と video frame の Fit/100%/pan/zoom が CPU fallback と
同じ表示領域になり、resize 後も表示が回復する。

### P2 — generation・upload・fallback 契約

- `ProgramMonitorPanel::requestGeneration_` を upload request にも渡し、完了時に
  generation が一致しない texture を破棄する。
- upload 中のフレームは最新一枚だけを保ち、seek や停止が stale upload を待たない。
- backend、fallback reason、texture size、upload/present time を既存 diagnostics に
  記録する。

**受入れ:** 急速な seek、連続 clip edit、decoder 遅延で旧フレームへの巻き戻りがない。

### P3 — CPU/GPU parity と実機検証

- 固定 `RenderPlan` を CPU compositor と GPU preview に入力し、透明色、letterbox、
  opacity、transition、clip effect を比較する。
- D3D12/Vulkan、GPU unavailable、device loss、HD/4K、複数クリップで runtime を
  検証する。
- GPU failure は reason を残して CPU preview を継続する。

## 実装前の判断

P0 は Artifact の target 構成を変更する。`ArtifactRender` を ArtifactPr に丸ごと
リンクする方式は、巨大な AE renderer の再ビルドと不要なモデル依存を生むため採用
しない。foundation 抽出の対象 module、名称、配置は Artifact 側の既存利用者を含む
設計レビューで確定する。
