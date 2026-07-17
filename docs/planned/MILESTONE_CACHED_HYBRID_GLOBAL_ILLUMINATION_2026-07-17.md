# Milestone: Cached Hybrid Global Illumination

**ステータス:** In Progress

**日付:** 2026-07-17

## 目的

ゲームエンジン相当の品質と速度のバランスを持つリアルタイムGIを、既存の
Diligent DX12 / Vulkanレンダーパスへ導入する。標準モードは `Balanced` とし、
画面外の安定した間接光をDDGI、接触部と近距離ディテールをSSGI、遠景をIBLで補う。

## 基本方針

- `DDGI + SSGI + IBL` を単一のキャッシュ型ハイブリッドGIとして扱う。
- DDGIは全プローブを毎フレーム更新せず、カメラ周辺と変化領域を優先して分散更新する。
- SSGIは低解像度で実行し、DDGIが苦手な近接ディテールを補う。
- Temporal accumulationとdepth/normal aware denoiseを両方式で共有する。
- RT非対応時は `SSGI + IBL` へ自動フォールバックする。
- CPU readback、`QImage`、Qt合成をGI本流へ持ち込まない。
- DiligentEngine自体は変更せず、Artifact / ArtifactCore側の公開APIで完結させる。

## 品質モード

| モード | 目標GPU時間 | DDGI | SSGI | 用途 |
|---|---:|---|---|---|
| Performance | 1.5 ms | 16–32 rays/probe、1/16更新 | 1/4解像度、4–8 steps | スクラブ、低性能GPU |
| Balanced | 3.0 ms | 32–64 rays/probe、1/8更新 | 1/2解像度、8–12 steps | 標準プレビュー |
| Quality | 6.0 ms | 64–128 rays/probe、1/4更新 | 1/2解像度、12–24 steps | 静止確認、最終品質 |

GPU時間が予算を超えた場合は、解像度変更より先にプローブ更新数とray stepを減らす。
予算を継続して下回る場合だけ段階的に品質を戻し、フレーム単位の振動を避ける。

## 現状

- `ArtifactIRenderer` に `Off / Auto / SSGI / DDGI / Hybrid` の選択契約を追加済み。
- GI設定、RT非対応時フォールバック、診断文字列を追加済み。
- RenderPipelineのdepth / normal / albedo / velocity / emission共通入力契約を追加済み。
- DDGI / SSGI / SurfelGI / VXGIの継承シェーダー資産は存在する。
- DiligentのRT capability、最小BLAS/TLAS、warmup TraceRaysは存在する。
- 実シーンBLAS/TLAS、SSGI dispatch、DDGI probe update、GI合成は未接続。

## 実装フェーズ

### Phase 1 — 共通契約と品質予算

- [x] GIモード、品質、フォールバック契約
- [x] 共通G-buffer入力ビュー
- [x] `Performance / Balanced / Quality` と目標GPU時間を設定へ反映
- [ ] 診断へ実測時間、履歴状態、更新予算を追加

### Phase 2 — SSGI fast path

- [ ] depth pyramidまたは階層depth入力を用意
- [x] 1/2・1/4解像度compute ray marchとGI出力SRVの最小経路
- [ ] normal/albedo aware rejectとIBL miss fallback
- [x] velocityを使ったtemporal reprojectionとping-pong履歴
- [x] depth/normal aware bilateral denoise
- [ ] denoise済みGIの既存linear color targetへの合成

### Phase 3 — 実シーンRTデータ

- [ ] Mesh GPU bufferから実BLASを構築
- [ ] mesh revisionでBLAS再構築を抑制
- [ ] transform dirty時だけTLASを更新
- [ ] material/light/emissionをRT hit pathへ接続

### Phase 4 — DDGI cache

- [ ] カメラ追従probe volumeとscrolling grid
- [ ] irradiance/depth/offset textureのGPU所有
- [ ] dirty regionとcamera distanceによるprobe scheduler
- [ ] hysteresis、relocation、classification、history reset

### Phase 5 — Hybrid合成と動的予算

- [ ] DDGIの低周波成分とSSGI近接成分を重複抑制して統合
- [ ] GPU profilerの実測値でray/probe予算を適応
- [ ] スクラブ開始時はPerformanceへ降格し、停止後に段階収束
- [ ] RT非対応・AS失敗・履歴無効時のSSGI fallback

## 完了条件

- Balancedで1080pのGI追加コストが代表シーン中央値3 ms以内。
- カメラ移動、ライト変更、動的メッシュで破綻や長時間の残像がない。
- RT非対応環境でもSSGI + IBLで同じ合成契約を維持する。
- GI無効時に既存レンダーパスの出力と性能を変えない。
- DX12とVulkanで同じArtifactCore契約を使用する。
- GPUリソース不足やRT初期化失敗を診断文字列から判別できる。

## 関連文書

- `docs/planned/MILESTONE_RAY_TRACING_DX_VULKAN_2026-05-16.md`
- `docs/planned/MILESTONE_RAYTRACING_EFFECTS_2026-03-25.md`
- `docs/planned/MILESTONE_3D_COMPOSITING_2026-07-08.md`
- `docs/planned/RENDER_BOUNDARY_CHANGE_SAFETY_CHECKLIST_2026-04-21.md`
