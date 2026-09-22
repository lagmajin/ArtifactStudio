# TGFX-inspired Render Reuse / Damage Scheduling

**最終更新:** 2026-09-22
**ステータス:** In Progress

## 目的

Tencent TGFX を依存ライブラリとして導入せず、TGFX 2.1 系で確認できる
DisplayList、dirty region、tiled rendering、frame-based resource expiration の設計を、
ArtifactStudio の既存 Diligent / RenderGraph / GPU texture cache 境界へ移植する。

Donor reference: Tencent TGFX (`https://github.com/Tencent/tgfx`), BSD-3-Clause,
consulted for resource expiration, dirty-region accumulation, viewport culling, and
bounded tile refinement. No donor code copied.

## 守る境界

- `ArtifactIRenderer -> Diligent -> D3D12 / Vulkan` を維持する。
- TGFX の Canvas / Surface / Context / backend 実装を持ち込まない。
- `QImage`、CPU readback、GPU upload をホットパスへ新規に暗黙追加しない。
- resource expiration と tile refinement は bounded とし、枯渇時に one-shot resource を生成しない。
- D3D12 / Vulkan で同じ公開契約を使い、backend 固有処理を上位へ漏らさない。

## 現状

- `RenderCommandBuffer` は描画 packet をフレーム単位で記録する。
- `GPUTextureCacheManager` は owner / generation / byte budget / entry budget / LRU を持つ。
- `ArtifactRenderROI` に `DirtyRegionAccumulator`、`RenderDamageTracker`、`TileGrid` が存在する。
- Composition View は ROI による画面外 layer skip と composition-space GPU cache の基礎を持つ。
- damage tracker は layer invalidation を受け取るが、描画後の消費・診断・tile scheduling へ未接続である。

## 実装段階

### TGFX-RR1: Frame-based GPU resource expiration

- cache entry に最終使用 frame を保持する。
- `beginFrame(frameIndex)` で cache のフレーム境界を明示する。
- 未使用期間が閾値を超えた entry を1フレーム当たり固定数だけ破棄する。
- expiration、budget eviction、明示 invalidation を診断上区別する。

完了条件:

- cache hit で最終使用 frame が更新される。
- expiration scan は1フレーム当たりの最大破棄数を超えない。
- 既定値0でexpirationを無効化できる。
- device reset後に古いframe ageを引き継がない。

### TGFX-RR2: Damage lifecycle

- layer変更時に旧boundsと新boundsの和をdamageとして記録する。
- full-frame effect、composition構造変更、camera/3D変更はfull redrawへ昇格する。
- 描画成功後にだけdamageを消費する。
- dirty rect、full-redraw理由、dirty tile数を遅延評価の診断へ出す。

完了条件:

- layer単位invalidatonが直後のbase invalidationで消失しない。
- transform変更で旧位置に残像が残らない契約を持つ。
- 失敗・早期returnしたframeでdamageを誤消費しない。

### TGFX-RR3: Bounded tile refinement

- 大型2D compositionに限定して`TileGrid`から可視dirty tileを列挙する。
- current visible frameを最優先し、1フレーム当たりのrefine数に上限を持たせる。
- zoom変更時は利用可能な近傍LODを表示し、stale generationはpublishしない。
- mask / matte / adjustment / 3D / full-frame effectは初期対象外とする。

完了条件:

- pan / zoom中に画面外tileを生成しない。
- queue depth、refined、deferred、dropped、stale rejectionを計測できる。
- queue上限超過時はcoalesceまたはdeferし、無制限確保しない。

### TGFX-RR4: Retained layer draw records

- layer content revision、draw bounds、resource generationを持つ再利用可能な描画記録を定義する。
- content不変時はpacket再構築を避け、transform / opacityだけを再評価する。
- raw GPU pointerの寿命をcache handle / generationで検証する。

完了条件:

- content revision不変の静止layerでpacket再構築数が減る。
- device reset、cache eviction、layer削除後にstale resourceをsubmitしない。

## 実装順

1. TGFX-RR1（cache寿命と診断）
2. TGFX-RR2（damageの正しい蓄積・消費）
3. TGFX-RR3（限定的tile refinement）
4. TGFX-RR4（retained draw record）

## 対象ファイル

- `Artifact/include/Render/GPUTextureCacheManager.ixx`
- `Artifact/src/Render/GPUTextureCacheManager.cppm`
- `Artifact/include/Render/ArtifactRenderROI.ixx`
- `Artifact/include/Render/RenderCommandBuffer.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

## 検証

静的確認:

- module purview内に`#include`を追加しない。
- texture cache APIがDiligent backend型以外のD3D12/Vulkan詳細を公開しない。
- expiration処理がboundedであり、通常frameに同期waitやresource生成を追加しない。
- 変更文書の日付監査を通す。

実行確認（ユーザー許可後）:

- D3D12とVulkanで静止画／連番画像／シェイプの表示が一致する。
- cache hit / expired eviction / budget evictionが期待通り増える。
- pan、zoom、scrub、長時間idle後の再描画でstale textureや欠落がない。
- dirty regionとtile refinement導入後、局所編集でfull composite回数が減る。

## 現在の状態

- TGFX-RR1: 実装済み。`beginFrame()`、frame age、bounded expiration、診断値を追加した。ビルド／D3D12・Vulkan実機確認待ち。
- TGFX-RR2: lifecycle foundation実装済み。旧／新boundsの和、full-frame昇格、present成功後の消費、debug snapshotを追加した。実際のpartial recompose接続は未実装。
- TGFX-RR3: allocation-freeなcoverage countと、可視tileを最大8件だけ選び残りをdeferする固定容量schedulerを実装済み。tile render targetへの実描画接続は未実装。
- TGFX-RR4: 既存`RenderCommandBuffer`との責務整理中。未実装。
