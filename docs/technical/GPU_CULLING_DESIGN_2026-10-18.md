# GPUカリング設計

**最終更新:** 2026-10-18

**ステータス:** Draft（設計のみ・未実装・未計測）

## 1. 目的

Composition Viewport / Render Queue の描画パス削減ポイントを整理し、GPUカリングを入れるならどこから入れるかを固定する。実装・ビルド・実機計測は本書の範囲外。

## 2. 現状（確認済み事実）

- `Frustum / Cull / Occlusion / Visibility` の実装はなし。`PrimitiveRenderer3D` / `CompositionViewDrawing` / `RenderCommandBuffer` 側にカリング基盤なし。
- GPU土台はあり：`Diligent（D3D12優先/Vulkan fallback）` + `RenderCommandBuffer` + `PrimitiveRenderer2D/3D` + Compute（`RevealCS` / `SSGI`等）+ `SolidRectBatch` + `GPUTextureCache / FrameCache`。
- `Render/ClonerGpuPlacement`、`ARTIFACT_ENABLE_GPU_CLONER` などGPU配置の実験フラグ系統あり。
- Viewport監査（`docs/analysis/COMPOSITION_VIEWPORT_PERFORMANCE_AUDIT_2026-09-09.md`）の最有力ボトルネックはN層 `2N+1` 全画面GPUパス（`convertLayerToFloat + blendLayers + Flush`）。カリングより先にここが支配的。
- Diligentメモ（`docs/analysis/DILIGENT_PREVIEW_BOTTLENECK_MEMO_2026-08-27.md`）の指摘：層ごとFlush、Adjustment同期readback、Sprite都度生成がDiligent境界の確定コスト。

## 3. 方針

全面Occlusion / Hi-Zから入らない。効果の大きい順に以下とする。

1. Phase 0: CPU bounds/frustumカリング（本命）
2. Phase 1: LOD・不可視スキップの整理
3. Phase 2: Cloner/ParticleのみGPU indirectカリング（条件付き）
4. 対象外：Occlusion / Hi-Z（後回し）

理由：本アプリは2Dコンポ主体でαブレンド積層が中心。隠面除去の旨味が薄く、全画面convert+blend削減とsubmit自体の削減が先。

## 4. Phase 0: CPUカリング（推奨・最小）

### 対象

- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- レイヤーパイプラインの描画前判定

### 判定

- `transformedBounds vs visibleRect` でsubmit前に除外。
- 除外条件：非表示、不透明度0、範囲外、空ジオメトリ、効果なしの完全透明。
- カメラあり3Dは簡易frustum（6面ではなく `w>0` + NDC `|x|,|y|<=1` + depth `[0,1]`）から開始。`PrimitiveRenderer3D` 側の既存NDC診断ロジックと一致させる。

### 制約

- 画質・意味を変えない。Quality/Resolution/Effect/Maskを落として高速化に見せない。
- `revision / generation / dirty bit` で変更時のみ更新。毎フレーム全層の重い再計算を増やさない。

## 5. Phase 1: LOD・可視性整理

- ズームベースLODは `docs/technical/LOD_SYSTEM_FEASIBILITY_ANALYSIS_2026-03-28.md` の方針を再利用。新規独自仕様を作らない。
- pan/zoom中の継続再合成は現状全層再描画のため、camera-only fast path（既存合成結果の再利用）の可否をPhase 0計測後に判断する。
- 調整レイヤー同期readbackはカリングで消えない。別案件としてGPU化/省略経路の整理が必要。

## 6. Phase 2: GPUカリング（Cloner/Particle限定）

数千〜数十万instanceになる場合のみ検討。

- ComputeでAABBカリング → `IndirectDraw` / StructuredBufferへ集約。
- DiligentのIndirect + StructuredBufferで実現可能。D3D12/Vulkan両laneでbarrier（`TRANSITION` + `FinishFrame`影響）を再確認する。
- 少量instanceではCPUカリングのまま。しきい値は起動時設定で切替。

## 7. やらないこと

- Occlusion Query / Hi-Z / ソフトウェアラスタizerへのカリング新設。
- `Software`経路の新機能化（GPU経路優先の原則維持）。
- Diligent低レベルbackendのアプリ責務外の変更（色・α・blend契約は `docs/technical/GPU_COMPOSITING_SPECIFICATION_2026-07-01.md` 優先）。

## 8. 共通契約

- `docs/technical/HOT_PATH_RULES.md` 遵守：フレーム毎 `new / PSO生成 / WaitForIdle / 暗黙readback / 無条件ログ` 禁止。
- Cull用 `args / visible-list` は起動時確保の `ring / pool / arena` + bounded fallback（coalesce/defer/dropのいずれかを明示）。
- 設定は `ArtifactStartup.json` の `Render/*` でゲート（`JSON > env > 既定`）。ホットパスで毎フレーム読み直さない。例：`Render/CpuCulling`、`Render/GpuClonerCulling`。
- C++20 module衛生：`.ixx`に不要importを増やさない、`export import`の安易追加禁止、自己import禁止。
- 新規 `signal/slot`、`QImage`本流追加、`QtCSS`、`QColorDialog`、`QPainter`合成の新規追加は禁止。既存theme/token・typed-buffer経路を再利用。

## 9. 診断・受入

`HOT_PATH_RULES`の診断ルールに従い、最低限以下を取る。

- `accepted / culled / deferred` 数、queue depth、ring occupancy
- CPU frame time、GPU time、present time
- `layerToFloatConvertCount`、`blendDispatchCount`、`flushMs`
- upload/readback bytes、cache hit/miss

計測マトリクスはViewport監査の最小セットを再利用：Solid 1枚 / 静止画10 Normal / 静止画30 Mixed / Text 10 / 調整1 / カメラあり / matteあり。present-pacedとshader-bound切分けのためoffscreen/uncapped条件を含める。

## 10. 次の手順（実装時）

1. Phase 0のCPUカリング + 診断カウンタのみ実装。
2. 上記マトリクスでA/B計測し、`2N+1`削減効果を確認。
3. 効果がCloner/Particleに偏る場合のみPhase 2を設計レビューへ上げる。

## 関連文書

- `docs/analysis/COMPOSITION_VIEWPORT_PERFORMANCE_AUDIT_2026-09-09.md`
- `docs/analysis/DILIGENT_PREVIEW_BOTTLENECK_MEMO_2026-08-27.md`
- `docs/technical/GPU_COMPOSITING_SPECIFICATION_2026-07-01.md`
- `docs/technical/LOD_SYSTEM_FEASIBILITY_ANALYSIS_2026-03-28.md`
- `docs/technical/HOT_PATH_RULES.md`
- `docs/technical/STARTUP_FLAGS_CONTRACT_2026-09-29.md`
- `Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md`
