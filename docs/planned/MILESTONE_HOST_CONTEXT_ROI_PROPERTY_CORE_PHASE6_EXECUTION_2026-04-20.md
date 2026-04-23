# Phase 6 実行メモ: Tiled ROI Engine

> 2026-04-20 作成

## 目的

[`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md) の Phase 6 を、full-frame path を残しながら段階導入するための実行メモ。

この段階では render の本流をすぐ置き換えず、tile-backed surface を並走させる。

---

## 方針

1. tile path は opt-in で始める
2. full-frame path は残す
3. GPU dispatch の clip は ROI から導く
4. tile surface は sparse backing で扱う

---

## 現状の土台

- [`Artifact/include/Render/ArtifactRenderROI.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderROI.ixx)
- [`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactCompositionViewDrawing.cppm)
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)
- [`ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Graphics/LayerBlendPipeline.cppm)

---

## 実装タスク

### 1. tile-backed surface の型を追加する

追加候補:

- `TileKey`
- `TileGrid`
- `SparseTileSurface`
- `TileRenderScheduler`

責務:

- 画像全体を 1 枚の巨大バッファとして固定しない
- ROI 単位で必要なタイルだけを確保する
- 局所更新を sparse に保持する

### 2. full-frame path と並走させる

やること:

- tile path を新しい render option として追加する
- 既存の full-frame path をデフォルトで残す
- どちらの path でも同じ出力になることを優先する

### 3. GPU dispatch を ROI で制限する

やること:

- compute dispatch / copy / blend のサイズを ROI から決める
- 画像サイズ起点の固定 dispatch を避ける
- mask / blur / heavy effect で効く範囲を先に絞る

### 4. precomp / heavy effect で検証する

やること:

- heavy effect や precomp のように ROI 効果が大きい箇所で試す
- まずは export ではなく preview で効果を確認する
- 失敗時は full-frame に戻せるようにする

---

## 実装順

1. `TileKey` / `TileGrid` / `SparseTileSurface`
2. tile / full-frame の二経路化
3. GPU dispatch clipping
4. precomp / heavy effect で検証

---

## 完了条件

- 一部更新で full-frame の work を避けられる
- tile path と full-frame path を切り替えられる
- 既存の描画結果が変わらない

---

## 変更しないこと

- full-frame path の即廃止
- 既存 UI の見た目
- 既存の layer / effect 実装の大改修

---

## リスク

- tile path を急ぎすぎると、既存の cache / blend / effect と噛み合わない
- sparse backing の参照漏れで描画欠けが起きる
- GPU dispatch を ROI に寄せる際に、余白分の安全域を忘れると見切れが出る

---

## 次の Phase への橋渡し

Phase 6 が終わると、Phase 7 で network / script / headless から同じ contract を使いやすくなる。

---

## 関連文書

- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md)
- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE5_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE5_EXECUTION_2026-04-20.md)
