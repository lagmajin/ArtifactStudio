# M-BLEND-1 Blend Mode Design

作成日: 2026-06-16
対象: `ArtifactCore/include/Layer/LayerBlend.ixx`, `ArtifactCore/src/Color/ColorBlendMode.cppm`,
      `ArtifactCore/include/Graphics/Shader/HLSL/Blend/`, `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx`,
      `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`,
      `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`,
      `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`,
      `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
位置づけ: BlendMode の **実装・接続・UI・契約** を 1 つの milestone に束ねる設計書。
実行方針メモ (2026-06-17 更新):
- この文書は設計の方向性は妥当だが、初手で `BlendOp.hlsl` 統合まで入ると既存 compute 契約を壊しやすい。
- 実装順は **Catalog -> 個別不足モード補完 -> Coverage -> UI 整理 -> shader 統合 -> stencil/silhouette 特殊経路** の順へ並べ替える。
- `ArtifactCore` だけでは完結しない。UI 露出と一部 call site 整理は `Artifact` 側を含む。
参照:
- `docs/technical/BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md`
- `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md`
- `docs/planned/MILESTONE_GPU_LAYER_BLEND_COMPUTE_2026-03-21.md`
- `docs/bugs/COMPUTE_BLEND_FAILURE_HYPOTHESES_2026-03-23.md`
- `docs/bugs/BUG_PLANE_LAYER_GPU_BLEND_ON_2026-05-16.md`
- `docs/analysis/COMPOSITION_EDITOR_GAP_ANALYSIS_2026-06-03.md`
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md`

---

## 1. 目的

`BlendMode` 周辺の現状は次の 3 つの歪みに同時にさらされている。

1. **定義は豊富だが、対応に偏りがある**。`LayerBlend.ixx` には 34 種が並ぶが、CPU 実装・GPU compute shader・UI 露出・テストの 4 軸でカバー率が揃っていない。
2. **接続と契約が不安定**。layer/accum/temp の format 差、straight/premult の混成、Porter-Duff over の欠落、stencil / silhouette / dissolve 系は未配線。
3. **AE parity の信用を直接左右する**。`MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md` の P0 系で「Track Matte / Mask / Blend の正確性」が常に上位に来る。

この milestone は、blend を「個別最適」ではなく **1 つの contract + 1 つの分類 + 1 つのテスト網** に束ねるための設計を固定する。

> 重要: 本 milestone は実際には `ArtifactCore` と `Artifact` の両方にまたがる。特に UI 露出と picker 導線は `Artifact/.../ArtifactLayerPanelWidget.cpp` 側に閉じる。サブモジュール（`ArtifactCore`, `ArtifactWidgets`）への直接編集は明示依頼時のみ。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 定義済み 34 モードのグループ

`BlendMode` を AE の規約に寄せ、次の 6 グループに分類する。

| グループ | モード | 用途 |
|---|---|---|
| **G0: Compositing** | `Normal`, `Dissolve`, `DancingDissolve` | source-over と確率 dissolve。Stencil/Silhouette は入れない（後述）。 |
| **G1: Porter-Duff / Stencil** | `StencilAlpha`, `StencilLuma`, `SilhouetteAlpha`, `SilhouetteLuma` | 描画経路が通常の blend と違う（mask 的に destination を更新する）。別 surface として扱う。 |
| **G2: Darken / Lighten** | `Darken`, `Multiply`, `ColorBurn`, `LinearBurn`, `ClassicColorBurn`, `Lighten`, `Screen`, `ColorDodge`, `LinearDodge`, `ClassicColorDodge` | 基本 10 種。 |
| **G3: Contrast** | `Overlay`, `SoftLight`, `HardLight`, `VividLight`, `LinearLight`, `PinLight`, `HardMix` | Overlay 系 7 種。 |
| **G4: Inversion** | `Difference`, `Exclusion`, `Subtract`, `Divide`, `Add`, `ClassicDifference` | 演算色が反転/差分系 6 種。 |
| **G5: HSL Component** | `Hue`, `Saturation`, `Color`, `Luminosity` | HSL ベース 4 種。 |

### 2.2 4 軸カバレッジ

| 軸 | 充足 | 不足 |
|---|---|---|
| `BlendMode` enum 定義 | 34 / 34 | — |
| CPU 実装 (`ColorBlendMode::blend`) | 一部（18 + 周辺） | Dissolve 系, Stencil 系, Classic 系, `DancingDissolve` など |
| GPU compute shader (`CS_Blend*.hlsl`) | 17 / 34 | Stencil/Silhouette 系, Dissolve 系, Classic 系 4 種, `DancingDissolve` |
| UI 露出 (`ArtifactLayerPanelWidget.cpp`) | 18 種コンボ | 残り 18 種は UI に出ていない |
| Porter-Duff over 統一 | 15 種修正済 (`COMPUTE_BLEND_FAILURE_HYPOTHESES_2026-03-23.md`) | Stencil/Silhouette 系, Dissolve 系, Classic 系 4 種 |
| Test (visual / harness) | smoke 数件 + 平面 + `Add` | グループ別 fixture が無い |

> 数値はマイルストーン作成時点の目視確認。実装着手時に `BlendModeCatalog::coverageReport()` を 1 度走らせ、確定値をこの表へ反映する。
>
> 2026-06-17 補足:
> - `Layer.Blend` には 34 モードが定義済みで、`BlendModeUtils::toString/fromString` も同ファイルに同居している。
> - `ColorBlendMode.cppm` は `LinearBurn / Divide / PinLight / VividLight / LinearLight / HardMix` まで CPU 実装済みだが、alpha はまだ簡略化されている。
> - `LayerBlendComputeShader.ixx` と `LayerBlendPipeline.cppm` は既存の個別 shader 群で稼働しているため、いきなり `BlendOp.hlsl` へ統合するより個別補完が安全。

### 2.3 既に走っている修正履歴

- 2026-04-08: 15 個の非 Normal shader を Porter-Duff over に統一
- 2026-05-16: `Bug_Plane_Layer_GPU_Blend_ON` で straight-to-premult contract へ揃え直し
- 2026-05-22: `RenderPipeline.LayerFloat(RGBA32F)` を追加し、compute blend 入力前に layer-to-float 変換を挟む構造に変更
- 2026-05-08: `Blend / Mask Contract` (`phase=blend-mask-smoke-v1`) を `FrameDebugSnapshot` に導入

つまり「shader 単位」では大きく前に進んでいる。残るのは **グループ単位の完成度** と **Stencil / Dissolve のような特殊経路**。

---

## 3. 設計の柱

### 3.1 Canonical Contract (RENDER_FORMAT_CONTRACT と一致)

内部合成 canonical は次で固定する。

- Format: `RGBA32F` linear
- Alpha model: **premultiplied**
- Channel order: `R, G, B, A`
- Color space: linear

blend shader の入力 contract:

- `SrcTex` = straight RGB を持つ `RGBA32F` (layer draw 後、layer-to-float 変換で unpremultiply 済み)
- `DstTex` = premultiplied accum
- 出力 `OutTex` = premultiplied accum (次の layer の `DstTex`)

`srcA = src.a * opacity` を基本とし、`srcRGB = src.rgb`（straight）で計算し、出力で `ComposeBlend(...)` により premult へ戻す。`BUG_PLANE_LAYER_GPU_BLEND_ON_2026-05-16.md` の修正を全モードで同じ規約に揃える。

### 3.2 BlendMode メタ情報レイヤ

`BlendMode` の隣に **`BlendModeInfo` 構造体** を導入し、レンダラ・UI・テストが同じ記述を見る。

```cpp
struct BlendModeInfo {
    const char*  uiName;            // "Color Dodge"
    const char*  groupName;         // "G2_DarkenLighten"
    bool         requiresHSL;       // Hue/Sat/Color/Luminosity
    bool         isClassic;         // Classic* 系
    bool         isStochastic;      // Dissolve / DancingDissolve
    bool         isStencil;         // Stencil*/Silhouette*
    BlendKind    kind;              // Compositing | DarkenLighten | Contrast | Inversion | HSL | Stencil
    ColorContract srcContract;      // Straight | Premultiplied
    ColorContract dstContract;      // Premultiplied
};
```

- `ArtifactCore/include/Layer/BlendModeInfo.ixx` を新規追加
- 34 モード全件分のテーブルを `constexpr` で持つ
- `toInfo(BlendMode)` の単一取得関数を追加
- UI 側のコンボボックス・分類ヘッダ・ツールチップは **このテーブルを single source of truth** として読む

> 禁止事項に抵触しないか: 新規 signal/slot 接続も、新規 QImage 使用も、QtCSS の追加もしない。`QString` を返す `uiName` のみは Qt 依存でよいが、Core 側は `const char*` にとどめる。

### 3.3 3 レンダリング経路

実装上、blend は 3 つの経路に分けて扱う。

1. **Standard Compute Path** (G0/G2/G3/G4/G5 の大半)
   - `CS_Blend<Mode>.hlsl` + Porter-Duff over
   - `LayerBlendPipeline::blend(ctx, srcSRV, dstSRV, outUAV, mode, opacity)`
   - 出力 premult 戻しを含む
2. **Stochastic Path** (Dissolve / DancingDissolve)
   - ノイズ入力 (`BlueNoiseTex` or ハッシュベース) を追加 binding
   - `threshold = noise + (1 - opacity)` の式
   - フレーム間で `BlueNoiseTex` の offset を 1px ずつ動かす
3. **Stencil / Silhouette Path** (Stencil/Silhouette 4 種)
   - 通常 blend では表現不可。`DstTex` を「更新」する代わりに、accum のうち source の mask 領域を書き換える
   - SRV 入力: `SrcTex` + `MaskTex` (alpha または luma)
   - UAV 出力: 書き換え領域のみ
   - Porter-Duff over の代わりに `BlendOp` を **discriminating** にする（後述の `BlendOp` 整理）

### 3.4 BlendOp の整理

現状は「`CS_Blend<Mode>.hlsl` を 1 モード 1 ファイル」展開しているが、Porter-Duff 統一後は **数式コアとモード分岐** に分けて整理できる。

- `BlendCommon.hlsli` に共通ヘルパ: `rgb2hsl`, `hsl2rgb`, `ComposeBlend`, `PorterDuffOver`, `LerpByOpacity`, `StochasticThreshold`
- `BlendOp.hlsl` に **モード → コア関数** の dispatch テーブル
- 個別 `CS_Blend*.hlsl` は **「モード固有の RGB 計算式」だけ** に縮める

これにより、

- 新モード追加時の boilerplate 削減
- 共通バグ修正が多数ファイルに散らばらない
- `BlendModeInfo::requiresHSL` のような属性で HSL パス最適化がかけやすい

### 3.5 UI 露出戦略

`ArtifactLayerPanelWidget.cpp` の現状は 18 種コンボ。これを **全 34 種に拡大** するのではなく、AE と同じく **2 階層メニュー** で扱う。

- コンボボックス上に出るのは "Normal" / "Add" / "Screen" / "Multiply" / "Overlay" などの代表 10 種
- 残りはコンボ下部に **"More..."** を置き、`ArtifactBlendModePickerDialog` を新設
  - グループは 6 セクション表示 (G0〜G5)
  - Classic 系は Classic セクションに隔離
  - Stencil / Silhouette はグループ名に "(stencil)" サフィックス

UI 側の text 取得は `BlendModeInfo::uiName` を使う。重複定義を禁止する。

### 3.6 テスト・診断

- `BlendModeCatalog::coverageReport()`
  - CPU / GPU / UI / Test の 4 軸マトリクスを文字列で返す
  - 34 種 × 4 軸の表を 1 行で出す
- 既存 `DebugRenderHarness` の `FrameDebugSnapshot.resources` に
  `label=Blend Coverage` を追加
  - note に `phase=blend-catalog-v1 coverage=<json>` を入れる
- Group 別 smoke fixture を 6 件固定
  - G0: Normal + opacity 50%
  - G1: StencilAlpha + luma mask
  - G2: Multiply + Screen + ColorDodge
  - G3: Overlay + HardLight + HardMix
  - G4: Add + Difference + Divide
  - G5: Hue (image, solid)

各 fixture は **平面 + 矩形** の最小構成で、PNG 比較ではなく **min/max probe + reason 文字列** で確認する。

### 3.7 不変条件 (Guardrails)

- 内部中間バッファに `RGBA8_sRGB` を増やさない
- `BlendOp` を持つレイヤーは `SrcTex` を straight で読む
- `DstTex` を読むレイヤーは premult 前提
- CPU fallback (`ColorBlendMode::blend`) は常に同じ contract で動く
- Stencil / Silhouette は Standard Compute Path に混ぜない
- `BlendModeInfo` テーブルを 34 種分必ず埋める。穴があれば `coverageReport()` で fail

---

## 4. フェーズ計画

### Phase 1: Catalog 導入 (P0, 1〜2 セッション)

- `ArtifactCore/include/Layer/BlendModeInfo.ixx` 新規
- `BlendModeInfo` 構造体 + 34 モード分の `constexpr` テーブル
- `toInfo(BlendMode)` 追加
- `BlendModeCatalog::coverageReport()` を `ArtifactCore` 側で実装
- `ArtifactLayerPanelWidget.cpp` のコンボは当面 18 種のまま、表示名のみ `uiName` 参照へ切替

**Done criteria:**
- 34 モードすべてに `BlendModeInfo` がある
- `coverageReport()` が固定文字列で返る
- 既存 UI 表示名が壊れない

### Phase 1.5: 現状追認と実装台帳化 (P0, 0.5 セッション)

- `BlendModeUtils::toString/fromString` の重複定義を catalog へ寄せる準備をする
- CPU / GPU / UI / Test の 4 軸で「既存対応済み / 未対応 / 契約未確認」を埋める
- `Classic*`, `Dissolve*`, `Stencil*`, `Silhouette*` を **特殊群** として台帳化する

**Done criteria:**
- 「未実装」ではなく「未配線」「未検証」「別経路必要」の区別が付く
- 次フェーズで 1 モードずつ進められる台帳になる

### Phase 2: 不足モードの個別補完 (P0, 2〜4 セッション)

新規 GPU shader / CPU 実装 / テスト:

- **不足している基本**: `LinearBurn`, `LinearDodge`, `Divide`, `VividLight`, `LinearLight`, `PinLight`, `HardMix`
- **Classic 系**: `ClassicColorBurn`, `ClassicColorDodge`, `ClassicDifference`
- **Stochastic** はまだ統合しない。`Dissolve`, `DancingDissolve` は次フェーズへ送る
- 1 種ずつ `BlendModeInfo` / CPU / GPU / smoke を同時追加する

**Done criteria:**
- G2/G3/G4 の不足分が GPU で描画できる
- CPU fallback が全対象を返せる
- `Add/Multiply/Screen/Overlay` の既存結果を壊さない

### Phase 3: Coverage Harness 統合 (P1, 1 セッション)

- `BlendModeCatalog::coverageReport()` を `DebugRenderHarness` / `FrameDebugSnapshot` に接続
- 6 グループ smoke のうち、まず G2/G3/G4/G5 を固定 fixture 化
- PNG 比較ではなく probe + reason 文字列中心で結果を残す

**Done criteria:**
- export された snapshot に blend coverage が含まれる
- 不足モード追加時に回帰確認の場がある

### Phase 4: UI 整理 (P1, 1 セッション)

- `ArtifactLayerPanelWidget.cpp` の表示名を `BlendModeInfo` 参照に寄せる
- 既存 18 種コンボは維持し、追加モードはまず内部対応のみでもよい
- picker dialog はこのフェーズで着手してよいが、標準コンボを壊さない

**Done criteria:**
- UI 表示名の single source of truth ができる
- 現行 UX を壊さずに追加モードの導線を増やせる

### Phase 5: BlendOp 統合 (P1, 2〜3 セッション)

- `BlendCommon.hlsli` を整備（既存ファイルへ追記）
- `BlendOp.hlsl` を新規追加し、各モードの **RGB 計算式だけ** を dispatch
- `LayerBlendComputeShader.ixx` の 17 shader 群を 1 つの `BlendOp.hlsl` へ統合
- Porter-Duff over / `ComposeBlend` / `LerpByOpacity` を共通関数化

**Done criteria:**
- 17 個の個別 `CS_Blend*.hlsl` が `BlendOp.hlsl` のテーブル呼び出しになる
- Visual smoke (5 種: Normal/Add/Multiply/Screen/Overlay) が phase 1 と同等
- 残課題 (Dissolve / Stencil / Classic 系) は Phase 3 以降

> 実行上の注意:
> - このフェーズは **初手でやらない**。既存 shader 群は過去の Porter-Duff / premult 修正の積み重ねなので、回帰面で最後に回す。
> - 統合前に coverage harness がない状態で入るのは禁止。

### Phase 6: Stochastic モード (P1, 1〜2 セッション)

- `Dissolve`, `DancingDissolve` を noise 入力 + frame offset で追加
- preview / render / export で同じノイズ系列を使う契約を定義
- ここは `BlendOp` 統合後でも個別 path でもよいが、必ず deterministic test を先に置く

**Done criteria:**
- G0 の Dissolve が GPU でも stochastic に動く

### Phase 7: Stencil / Silhouette (P0, 1〜2 セッション)

- 専用 Compute Path を `LayerBlendPipeline::blendStencil(...)` として追加
- `StencilAlpha`, `StencilLuma`, `SilhouetteAlpha`, `SilhouetteLuma` の 4 種
- Mask texture (alpha or luma) を SRV で受ける
- DstTex の **書き換え領域のみ** を更新
- UI 露出は Phase 5 とまとめて実施

**Done criteria:**
- 4 種すべてで visual smoke 通過
- DstTex の非マスク領域が変化しない (probe で確認)

### Phase 8: UI 完全露出 (P1, 1 セッション)

- `ArtifactBlendModePickerDialog` 新規
- コンボボックス下に "More..." 追加
- 6 セクション + Classic 隔離 + Stencil サフィックス
- 全モードのキーバインド (Photoshop 互換 short name) をツールチップに

**Done criteria:**
- 34 モードすべてに UI 導線がある
- コンボボックスの標準 10 種と dialog の見た目が統一されている

## 4.1 実装順の推奨

設計順ではなく、実装は次の順を推奨する。

1. `Phase 1: Catalog 導入`
2. `Phase 1.5: 現状追認と実装台帳化`
3. `Phase 2: 不足モードの個別補完`
4. `Phase 3: Coverage Harness 統合`
5. `Phase 4: UI 整理`
6. `Phase 5: BlendOp 統合`
7. `Phase 6: Stochastic モード`
8. `Phase 7: Stencil / Silhouette`
9. `Phase 8: UI 完全露出`

この順なら、既存 compute path を壊さずに前へ進められる。

---

## 5. 既存マイルストーンとの関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_GPU_LAYER_BLEND_COMPUTE_2026-03-21.md` | 本 milestone が上位。Phase 1〜3 で個別 shader 補完を完了し、Phase 2 で `BlendOp` 統合に置き換える。 |
| `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` | `CompositionRenderController` に low-level call site を増やさない方針を踏襲。Stencil は Pipeline 側 helper として実装し、controller を触らない。 |
| `MILESTONE_GPU_MASK_COMPUTE_PIPELINE_2026-04-03.md` | Stencil/Silhouette は mask 寄りなので、Phase 4 で参照する。 |
| `MILESTONE_TRACK_MATTE_DRAG_LINK_UX_2026-06-01.md` | 完走後、Stencil モードと Track Matte の UX を 1 つの surface に並べる可能性あり。今回は触らない。 |
| `MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md` | P0 の "Track Matte / Mask / Compositing Correctness" を直接支える。 |
| `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` | 「18/38 程度しか埋まっていない」指摘を解消する。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **`BlendOp` 統合時の性能劣化と回帰**。dispatch テーブル化で分岐が増えるだけでなく、既存の Porter-Duff 修正差分を失う危険がある。Group 単位で PSO を分け、coverage harness 導入後にのみ着手する。
2. **Dissolve のフレーム安定性**。DancingDissolve は frame offset が累積するため、preview / render / export で **同じノイズ系列を使う** 必要があり、frame 番号の扱い契約を `MILESTONE_EXPRESSION_SUBFRAME_TIMESTEP_POLICY_2026-06-07.md` と整合させる。
3. **Stencil の MaskTex 取得**。LayerMask との結合点が未確定。`MaskCutoutInput` を再利用する方向で要相談。
4. **Classic 系の数式差**。`ClassicColorBurn = 1 - (1-dst)/src`、`ColorBurn` との違いは scale 1.0 vs scale 255 だが、HLSL 内部で 0..1 化したときの差分をテストで確認する。

### 6.2 契約上の未解決

- `outAlpha = src.a + dst.a * (1 - src.a)` を全モードで共通化するが、`Add` だけは単純加算。`Add` の Porter-Duff 適用について `src.a` をどう扱うか (a) 単純加算 (b) source-over を採用するかは Phase 2 で決定する。
- `LinearBurn` と `ColorBurn` の UI 上での並び順。AE では Darkening グループで Color/Linear/Classic の順。`BlendModeInfo` テーブルはこの順を尊重する。
- Stencil / Silhouette は G1 だが、内部的には描画経路を別 PSO にする。`BlendModeInfo::isStencil` を見て controller 側で dispatch する。

### 6.3 サブモジュール境界

- `ArtifactCore/include/Layer/BlendModeInfo.ixx` を追加 → 親 repo 側で `ArtifactCore` submodule を bump する手順
- `ArtifactCore/include/Graphics/Shader/HLSL/Blend/BlendCommon.hlsli` の更新 → 同上
- `ArtifactWidgets` は触らない（picker dialog は `Artifact/src/Widgets/` 配下に置く）
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7.1 最初の着手パッチ候補

最初の着手は次の最小単位を推奨する。

- `ArtifactCore/include/Layer/BlendModeInfo.ixx` を追加
- `Layer.Blend` の `BlendModeUtils::toString()` を `BlendModeInfo::uiName` 参照へ置換
- `coverageReport()` はまず固定 table を返すだけでよい
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` は表示名の取得元だけ差し替える

この単位なら shader 契約に触れず、catalog を先に根付かせられる。

---

## 7. Done Criteria (全体)

- `BlendMode` 34 種すべてに `BlendModeInfo` がある
- `BlendModeCatalog::coverageReport()` が 4 軸カバレッジを返す
- G2/G3/G4/G5 が GPU compute + Porter-Duff over で動作
- Dissolve / DancingDissolve が GPU で stochastic に動作
- Stencil / Silhouette 4 種が専用 compute path で動作
- 6 グループ × smoke fixture が `DebugRenderHarness` から 1 クリックで回せる
- 34 種すべてに UI 導線 (標準 10 + picker dialog 24)
- `BUG_PLANE_LAYER_GPU_BLEND_ON_2026-05-16.md` の再現条件が再発しない
- 新規 `QImage` 使用 / `setStyleSheet` / 新規 signal-slot が増えていない
- `ArtifactCore` 側の submodule bump が親 repo の commit と整合している

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`MILESTONE_GPU_LAYER_BLEND_COMPUTE_2026-03-21.md` の上位設計として再整理。
