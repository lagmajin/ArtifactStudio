# Composition Viewport パフォーマンス監査メモ

**最終更新:** 2026-09-09

**状態:** 静的ソーストレース完了。実機プロファイル・ビルド・ランタイム検証は未実施。

## 目的

Composition Viewport（VP）の描画、入力、キャッシュ、GPU 合成経路を静的に確認し、フレーム時間や操作応答性に影響しそうな現行コードを優先度順に整理する。

対象の中心は以下。

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

本メモは静的解析に基づく候補整理であり、ボトルネックの最終確定には実機計測が必要である。

## 結論

現時点の最有力候補は、レイヤー数に比例する全画面 GPU 変換・ブレンドと、pan / zoom 中にも GPU パイプライン全体を再合成する構造である。

次点として、調整レイヤーの同期 GPU readback、アクティブカメラ使用時の前フレーム評価、render hot path 内の診断文字列・設定参照・一時確保がある。

## 優先度: 高

### 1. レイヤーごとの全画面 GPU 変換・ブレンド

GPU パイプラインでは、各表示レイヤーについて概ね以下を繰り返す。

1. 中間 render target へ描画
2. `renderer_->flush()`
3. `convertLayerToFloat()`
4. 必要に応じて pointwise effect / track matte
5. `blendLayers()`
6. accum/temp ping-pong target を交換

主な箇所:

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:11214`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:11232`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:11463`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:11800`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:35891`

影響:

- レイヤー数に比例して全画面 compute / blend pass が増える。
- レイヤーごとの flush と resource transition が command submission の粒度を細かくする。
- 1080p / 4K、非 Normal blend、mask / matte / pointwise effect の組み合わせで負荷が増えやすい。

確認方法:

- 1、10、30レイヤーの Normal-only composition で `layerToFloatConvertCount`、`blendDispatchCount`、`flushMs`、GPU frame time を比較する。
- 同じ構成で Normal と Mixed blend を比較する。
- present-paced と shader-bound を分離するため、可能なら offscreen / uncapped 条件でも比較する。

### 2. pan / zoom 中も基本的に全レイヤーを再合成

操作中は低解像度化されるが、GPU blend path 自体は継続して実行される。

主な箇所:

- 操作中の downsample: `ArtifactCompositionRenderController.cppm:33876`
- 操作中の継続再描画: `ArtifactCompositionRenderController.cppm:34662`
- Composition-space GPU cache の実験設定: `ArtifactCompositionRenderController.cppm:34948`
- cache eligibility が `!pipelineEnabled` に限定: `ArtifactCompositionRenderController.cppm:34957`

現状:

- `effectivePreviewDownsample` によって操作中の描画ピクセル数は抑えられる。
- 一方、既存の合成結果を pan / zoom だけで再利用する camera-only fast path は通常の GPU pipeline には適用されていない。
- Composition-space GPU cache は既定無効で、対象も effect / mask / 3D / adjustment / non-Normal blend を含まない限定的な fallback path である。

影響:

- レイヤー内容が不変でも、pan / zoom イベント中に全レイヤー再描画へ入りやすい。
- CPU raster / texture upload があるレイヤーでは downsample だけでは追従性を十分に改善できない可能性がある。

確認方法:

- 静止画のみ、Text 混在、effect 混在の3 composition で pan / zoom 中の `layerMs`、`surfaceUploadLayers`、`cpuRasterLayers` を比較する。
- settled frame と interaction frame の GPU pass 数を比較する。

### 3. 調整レイヤーの同期 GPU→CPU readback

調整レイヤーの fallback では、現在の描画ターゲットを `QImage` として同期取得し、その画像へ effect を適用する。

主な箇所:

- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:2318`
- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:2340`

現状:

- viewport interaction / draft 条件では、残りの CPU adjustment effect を省略して readback を避ける経路がある。
- settled / full-quality frame では同期 readback が残る。

影響:

- GPU copy、flush、fence wait、Map を伴う同期点になり得る。
- 調整レイヤーの枚数に応じてフレーム時間が段階的に悪化する可能性がある。
- 操作停止直後の full-quality 復帰時に引っ掛かりとして現れる可能性がある。

確認方法:

- 調整レイヤー 0 / 1 / 3 枚で settled frame の CPU/GPU時間と fence wait を比較する。
- interaction 中と停止直後を別区間として記録する。

### 4. カメラ使用時の前フレーム評価

アクティブカメラがあり現在フレームが1以上の場合、毎描画で composition を前フレームへ移動し、行列取得後に現在フレームへ戻している。

主な箇所:

- `ArtifactCompositionRenderController.cppm:34233`
- `ArtifactCompositionRenderController.cppm:34235`
- `ArtifactCompositionRenderController.cppm:34239`

現状の流れ:

```text
composition frame N
  -> goToFrame(N - 1)
  -> previous camera matrices を取得
  -> goToFrame(N)
```

影響:

- `goToFrame()` がレイヤー評価、property animation、component simulation 等へ波及する場合、1描画中に追加の composition 評価が発生する。
- 前フレーム行列は velocity / motion blur 用だが、現状の位置では motion blur が不要な場合も評価される可能性がある。

改善候補:

- timeline motion blur、camera motion blur、velocity channel のいずれかが実際に必要な場合だけ previous-frame sampling を行う。
- composition 全体の可変 frame position を往復せず、camera transform の時刻指定評価で取得できるか責務を確認する。

確認方法:

- カメラなし、カメラあり・motion blur off、カメラあり・motion blur on の frame evaluation 回数と CPU時間を比較する。

## 優先度: 中

### 5. render hot path 内の診断文字列・設定参照・一時確保

毎フレームまたは定期的に、描画そのものではない診断処理が実行される。

主な箇所:

- `QSettings` の生成・参照: `ArtifactCompositionRenderController.cppm:34952`
- pass plan の文字列化: `ArtifactCompositionRenderController.cppm:35192`
- 30フレームごとの診断用 RenderGraph 構築・compile: `ArtifactCompositionRenderController.cppm:35198`
- `RenderCostCaptureGuard` の `make_unique`: `ArtifactCompositionRenderController.cppm:33728`
- レイヤー配列の `std::vector` コピー: `ArtifactCompositionRenderController.cppm:37181`
- `lastRenderPathSummary_` の大きな `QString::arg()` 連鎖: `ArtifactCompositionRenderController.cppm:39153`
- visibility summary の再構築: `ArtifactCompositionRenderController.cppm:39271`

影響:

- `QString` / `QStringList` / `std::string` / container の一時確保が steady-state render に入る。
- 診断用 RenderGraph の構築と compile が周期的な CPU spike になる可能性がある。
- GPU負荷が低い composition では、これらのCPUコストが相対的に目立ちやすい。

改善候補:

- `QSettings` 値を controller のコールドパスで読み、設定変更時だけ更新する。
- summary 生成を表示要求または診断 category 有効時だけ遅延実行する。
- RenderGraph compile は plan 変更時または明示 capture 時に限定する。
- `RenderCostCaptureGuard` はスタック配置にする。
- overlay API が range / span 相当を受け取れるならレイヤー配列コピーをなくす。

### 6. CPU raster・QImage・matte fallback

主な箇所:

- cache signature 構築: `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:521`
- Text の27フィールド署名: `ArtifactCompositionViewDrawing.cppm:726`
- scene light の非3D surface 全画素処理: `ArtifactCompositionViewDrawing.cppm:1669`
- matte source の `QImage` fallback: `ArtifactCompositionRenderController.cppm:35728`
- matte resize / upload: `ArtifactCompositionRenderController.cppm:11567`

現状:

- cache signature は以前の無条件生成から改善され、cache 利用時に限定されている。
- ただし cache hit 判定のため、effect property、layer type、各レイヤー固有 property の走査と文字列生成はフレームごとに残る。
- scene light がある非3D layer は GPU texture cache の利用対象から外れ、float/QImage surface 上で全画素 lift を行う経路がある。
- Stretch matte と GPU intermediate を使える場合は GPU path があるが、それ以外は QImage 化、resize、upload へ落ちる。

影響:

- Text、静止画、matte、Light が多い composition で CPU allocation とメモリ帯域が増える。
- 高解像度 surface では全画素処理と upload が支配的になり得る。

改善候補:

- cache identity を layer revision / dirty serial ベースへ寄せ、毎フレームの長い文字列構築を避ける。
- non-3D light lift を GPU pointwise pass へ統合する。
- matte fit と upload の結果を revision / target size / fit mode でキャッシュする。

## 追加の軽度候補

以下は単体では上位候補より小さいが、レイヤー数や mask vertex 数によって累積する可能性がある。

- `timelineTransitionProgressAtFrame()` をレイヤーループ内で毎回評価している: `ArtifactCompositionRenderController.cppm:36061`
- mask overlay の頂点ごとに `selectedMaskVertices_` を線形検索する: `ArtifactCompositionRenderController.cppm:37529`
- overlay 用にレイヤー配列を毎フレームコピーする: `ArtifactCompositionRenderController.cppm:37181`

## 既に改善されている箇所

### Motion Path のサンプルキャッシュ

過去の調査では Motion Path 表示時の `getGlobalTransformAt()` 多数呼び出しが重大候補だったが、現在は選択レイヤーごとの cache がある。

- cache 定義: `ArtifactCompositionRenderController.cppm:13387`
- cache hit 判定: `ArtifactCompositionRenderController.cppm:19456`

したがって、現時点では Motion Path よりも GPU layer blend、camera previous-frame evaluation、診断処理を先に調べる。

### Render dirty による idle 停止

render tick は常時描画ではなく、dirty がなく interaction 中でもない場合に停止する。

- `ArtifactCompositionRenderController.cppm:16066`

このため、idle 時の常時60fps描画そのものは主要問題ではない。操作中や再生中、dirty が頻発する条件を重点的に測定する。

## 推奨着手順

### Phase 1: 低リスクな CPU hot-path 整理

1. `QSettings` の per-frame 参照をコールドパスへ移す。
2. 診断 summary と診断用 RenderGraph compile を遅延実行する。
3. `RenderCostCaptureGuard` の heap allocation をなくす。
4. overlay 用のレイヤー配列コピーをなくす。
5. transition progress をフレームごとに1回だけ評価する。

期待:

- 描画結果や GPU resource contract を変えずに CPU stutter と allocation を減らせる。

### Phase 2: カメラ前フレーム評価の条件化

1. previous-frame camera matrices が必要な機能条件を整理する。
2. motion blur / velocity が不要なら `goToFrame(N - 1)` を省略する。
3. 可能なら composition mutation を伴わない時刻指定評価へ移す。

### Phase 3: interaction reuse

1. pan / zoom だけの変更と composition content invalidation を分離する。
2. settled composition result を texture として保持する。
3. interaction 中は保持 texture の表示変換だけを更新する。
4. 操作終了後に full-quality frame を再構築する。

### Phase 4: 同期 readback 撤去

1. 調整レイヤーの背面を GPU texture として保持する。
2. GPU対応 effect は texture 上で処理する。
3. CPU effect fallback が必要な場合も同期 readback を render submit lane の恒常的な停止点にしない。

### Phase 5: レイヤー合成パス削減

1. Normal-only composition の fast path を測定する。
2. layer-to-float conversion と blend ping-pong の統合可能性を検討する。
3. resource transition と D3D12 / Vulkan backend の整合を維持したまま flush 粒度を見直す。

## 最小計測マトリクス

以下の composition を同一解像度・同一 backend で比較する。

1. Solid 1枚
2. 静止画 10枚、Normal blend
3. 静止画 30枚、Mixed blend
4. Text 10枚
5. 調整レイヤー 1枚
6. カメラあり、motion blur off
7. カメラあり、motion blur on
8. matte あり（Stretch GPU path）
9. matte あり（QImage fallback 条件）
10. Light あり + 大きな非3Dレイヤー

最低限記録する値:

- frame total
- setup / base / layer / mask / composite / post / overlay
- `flushMs`
- `presentMs`
- GPU frame time
- `surfaceUploadLayers`
- `cpuRasterLayers`
- `layerToFloatConvertCount`
- `blendDispatchCount`
- adjustment readback count / wait time
- composition `goToFrame()` 呼び出し回数
- hot-path allocation count

## 現在の working tree について

調査時点の未コミット `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` の差分は Light 作成ダイアログへの接続であり、確認した範囲では VP の毎フレーム描画性能を悪化させる変更ではない。

既存の未コミット変更には触れておらず、本メモ以外の実装変更は行っていない。

## 関連文書

- `Artifact/docs/MILESTONE_COMPOSITION_VIEW_FAST_PATH_2026-03-25.md`
- `docs/analysis/DILIGENT_PREVIEW_BOTTLENECK_MEMO_2026-08-27.md`
- `docs/analysis/RENDER_PERF_HOTPATH_INVESTIGATION_2026-07-08.md`
- `docs/analysis/PREVIEW_CACHE_SYSTEM_AUDIT_2026-08-08.md`
- `Artifact/docs/planned/MILESTONE_MULTI_FRAME_PREVIEW_RENDERING_2026-06-29.md`
- `Artifact/docs/MILESTONE_PREVIEW_FREEZE_STOP_RESPONSIVENESS_2026-06-05.md`
