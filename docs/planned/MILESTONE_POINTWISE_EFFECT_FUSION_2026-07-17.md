# Milestone: Pointwise Effect Fusion Compiler

**ステータス:** In Progress

**日付:** 2026-07-17

## 目的

同一ピクセルだけを処理する連続エフェクトを、effect stack ごとの複数 render pass ではなく、
生成済みの単一 HLSL pixel / compute shader に融合する。中間 target、SRV bind、draw / dispatch を
減らし、プレビューと最終レンダーの両方で色処理を低コストかつ一貫して実行できるようにする。

DXC は生成済み HLSL を最適化・コンパイルする役割に限定する。Artifact 側が effect stack から
小さな中間表現（IR）を作り、融合可能な区間を HLSL と binding layout に変換する。

## 対象

### 融合対象（Pointwise）

- Exposure
- Gamma
- Contrast
- Levels
- Saturation
- Tint
- LUT（3D texture または既存LUT resourceのsample）
- Blend（背景入力が同一passで SRV として読める場合）
- Alpha 処理（straight / premultiplied の相互変換を含む）
- Posterize
- Threshold

### 融合境界

以下は別 pass とし、Pointwise 区間をそこで終了する。

- Blur、Glow、Drop Shadow 等の近傍サンプル処理
- Displacement、Morphology、Keyer matte expansion
- Temporal history、motion vector、depth、normal を使う処理
- GPU readback、CPU effect、Qt / `QImage` 境界
- 背景が同一 render pass で SRV 化できない blend

## 設計原則

- 入出力は linear RGBA を基本とし、display transform / export encoding の前に適用する。
- alpha mode を stack の暗黙状態にせず、`Straight` / `Premultiplied` を IR と resource contract に含める。
- dynamic parameter は constant buffer / structured buffer に置き、値の変更だけで HLSL / PSO を再生成しない。
- node type、接続順、static specialization、target format、alpha mode、backend を compile key に含める。
- LUT、入力、背景は SRV binding とし、同一 texture を input と output に同時bindしない。
- DX12 / Vulkan で同一IRと生成規則を使い、DiligentEngine の fork は変更しない。
- generated shader は診断可能な名前、source map、node一覧を保持する。

## アーキテクチャ

```text
Effect Stack
  → PointwiseSegmenter
  → Pointwise IR (ordered nodes + alpha/resource contract)
  → HLSL Generator + binding layout
  → DXC / Diligent PSO creation
  → Pipeline cache
  → single-pass linear RGBA output
```

`PointwiseSegmenter` は連続する融合可能 node を最大区間へまとめる。非融合 node の前後では
resource transition と intermediate target を明示的に確定し、元の effect 評価順は変えない。

## フェーズ

## 実装状況

- `ArtifactCore/include/Render/PointwiseEffectFusion.ixx` に、独立したIR node、segmenter、
  compile key、generated HLSL pixel shader emitterを実装済み。
- Exposure / Gamma / Contrast / Levels / Saturation / Tint / LUT / Blend / Alpha / Posterize /
  Threshold のHLSL生成を実装済み。
- render pipeline、effect stack、DXC / Diligent PSO cacheとの接続は意図的に未実施。

### Phase 1 — IR と融合境界

- `PointwiseNodeKind`、input / output alpha mode、texture input、constant parameter の最小IRを定義する。
- 既存 effect metadata に `Pointwise` / `Neighborhood` / `Temporal` / `CpuBoundary` の分類を追加する。
- stack を走査して最大の pointwise segment を返す segmenter を実装する。
- 非融合 node 前後の target / resource state を既存render pipeline契約へ合わせる。

**Done when:** 任意の effect stack について、融合区間とpass境界を診断文字列で説明できる。

### Phase 2 — 基本色ノードの生成

- Exposure、Gamma、Contrast、Levels、Saturation、Tint をIR nodeとして実装する。
- ordered node list から単一 HLSL entry point、constant buffer layout、SRV binding layout を生成する。
- generated sourceをDXC / Diligentでコンパイルし、PSOを作る。
- static graph変更時だけPSOを再生成し、パラメータ編集時はbuffer更新だけにする。

**Done when:** 6種の連続色補正が1 draw / dispatchとなり、非融合実行と画が一致する。

### Phase 3 — Alpha / LUT / Blend

- straight / premultiplied alpha の変換nodeと、alpha-safeな色補正規則を実装する。
- LUTをSRVとしてbindし、LUT asset revision時だけbinding / cacheを更新する。
- 背景SRVを安全に読める合成パスで、通常 alpha、add、multiply、screen 等のblendを融合する。
- read/write aliasが起こる構成では融合を拒否して既存passへfallbackする。

**Done when:** LUT、alpha処理、対応blendを含むstackが、正しいalphaとレイヤー順で単一pass化できる。

### Phase 4 — Stylize Nodes とキャッシュ

- Posterize、Threshold を追加する。
- compile key、generated source、PSO、binding layout のメモリ上cacheを実装する。
- cache miss、compile時間、fusion前後のpass数、fallback理由をdiagnosticsへ出す。
- 無効cache、shader compile失敗、resource不足時に既存の非融合pathへ戻す。

**Done when:** 同じstackを繰り返し描画してもshader compileが発生せず、失敗時も出力を失わない。

### Phase 5 — 検証と適用拡大

- 各node単体、代表的な混合stack、alpha edge caseのgolden image比較を用意する。
- preview / final renderで同じIRを共有し、format差だけを明示する。
- 融合前後のpass数、GPU時間、VRAM使用量を計測する。
- 近傍・temporal effectとの境界を確認し、誤融合を防ぐ回帰ケースを追加する。

**Done when:** 代表的な色補正stackで画の差分が許容範囲内かつ、intermediate pass数が削減される。

## Compile Key

```text
backend + targetFormat + alphaMode + orderedNodeKinds + staticSpecializations
  + inputBindingShape + blendMode + generatorVersion
```

`Exposure` 値、色、levels値、LUT intensityなどの動的値は key に含めない。constant buffer更新だけで
即時反映する。nodeの追加・削除・並べ替え、alpha mode、texture binding数、static branchだけが再生成条件になる。

## 非スコープ

- effect graph UI の全面刷新
- 全エフェクトの融合
- CPU / Qt合成の追加
- DiligentEngineのshader compiler変更
- AI生成LUTや自動グレーディング

## 完了条件

- 対象nodeを連続適用した場合、融合後は1 passで実行される。
- パラメータ操作中にHLSL再コンパイルやPSO再生成が起きない。
- 非融合pathとの色、alpha、effect順が一致する。
- resource alias、shader compile失敗、対応外nodeでは安全に非融合へfallbackする。
- DX12 / Vulkanで同一IR、同一のeffect順、同一のfallback理由を使う。

## 関連文書

- `docs/planned/MILESTONE_RENDER_PATH_DECOMPOSITION_2026-03-31.md`
- `docs/planned/MILESTONE_RENDER_FORMAT_EXPANSION_2026-06-16.md`
- `docs/planned/MILESTONE_OCIO_INTEGRATION_2026-06-16.md`
- `docs/planned/RENDER_BOUNDARY_CHANGE_SAFETY_CHECKLIST_2026-04-21.md`
