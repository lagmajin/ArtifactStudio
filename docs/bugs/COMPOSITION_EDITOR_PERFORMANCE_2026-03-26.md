# コンポジットエディタ パフォーマンス問題レポート (2026-03-26)

## 症状

コンポジットエディタが重い。画像1枚でもラグがある。

---

## 再調査結果 (2026-03-26 再)

修正後も重い原因を再調査。readback は削除済みだが、以下の問題が残っている。

### ★★★ 1. QImage::cacheKey() ベースのテクスチャキャッシュが毎回ミス

**場所:** `PrimitiveRenderer2D.cppm:1254-1262`

```cpp
qint64 cacheKey = image.cacheKey();
auto it = impl_->m_spriteTexCache.find(cacheKey);
if (it != impl_->m_spriteTexCache.end()) { ... }
else {
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888); // ← 毎回!
    pDevice_->CreateTexture(...);  // ← GPU 転送!
}
```

`QImage::cacheKey()` はインスタンスベース。`toQImage()` や `currentFrameToQImage()` が
毎フレーム新しい QImage を返すと、キャッシュキーが毎回変わり、**毎フレーム GPU テクスチャ作成 + アップロード**。

3つのオーバーロード全てで同じ問題:
- `drawSprite()` (line 1081)
- `drawSpriteTransformed(QTransform)` (line 1254)
- `drawSpriteTransformed(QMatrix4x4)` (line 1486)

### ★★★ 2. サーフェスキャッシュキーも surface.cacheKey() に依存

**場所:** `CompositionRenderController.cppm:133,145,155,172` (`buildLayerSurfaceCacheKey()`)

サーフェスキャッシュのキーに `surface.cacheKey()` を使用。
ビデオ/テキストレイヤーが毎フレーム新しい QImage を生成すると、
キャッシュシグネチャが変わり、OpenCV のラスタライザーエフェクト処理が毎フレーム再実行される。

### ★★ 3. renderOneFrame() が31箇所から呼ばれている

| 問題 | 場所 |
|------|------|
| handleMouseMove 内で pen hover + gizmo hover が独立発火し、**1 mouseMove で2回** | line 1559, 1598 |
| レイヤーの `changed` シグナルが `composition->changed` と二重発火 | line 780, 691 |
| `setComposition` で同コンポジションの場合も再レンダリング | line 1026 |
| dead code: `renderTimer_` が作成されるが start() されない | line 829-832 |

### ★★ 4. レイヤー毎の GPU ブレンドサイクル

**場所:** `CompositionRenderController.cppm:2018-2035`

1レイヤーあたり:
1. `SetOverrideRTV(layerRTV)` → RTV 切替
2. `clear()` — layerRTV クリア
3. `drawLayerForCompositionView()` — 描画
4. `SetOverrideRTV(nullptr)` — RTV 復帰
5. `SetRenderTargets(0,nullptr,nullptr)` — バリア
6. `blendPipeline_->blend()` — Compute dispatch
7. `swapAccumAndTemp()` — ポインタスワップ

N レイヤーで N 回の Compute dispatch + N+1 回の clear。

### ★ 5. ビデオレイヤーが毎フレーム QImage アロケーション

**場所:** `drawLayerForCompositionView()` line 519

`videoLayer->currentFrameToQImage()` が毎フレーム新しい QImage を返す。
→ 仮説1 のキャッシュミスの原因。

---

## 対策案

### 対策1: テクスチャキャッシュをレイヤーID ベースに変更 (最重要)
```cpp
// 現在: qint64 cacheKey = image.cacheKey();
// 修正: qint64 cacheKey = qHash(layerId.toString());
```
→ 同じレイヤーなら同じキーで再利用。

### 対策2: サーフェスキャッシュキーから surface.cacheKey() を除去
`buildLayerSurfaceCacheKey()` で `surface.cacheKey()` の代わりに `layerId + sourceHash` を使用。

### 対策3: ビデオレイヤーの QImage キャッシュ
`currentFrameToQImage()` で同じフレームなら同じ QImage を返すようにする。

### 対策4: renderOneFrame() のコアレシング強化
`renderScheduled_` フラグで最初の呼び出し以降をスキップ。
`changed` シグナルの二重発火を抑制。

---

## 初回調査（修正前）

## ボトルネック分析

### ★★★ 1. GPU Readback ストール（最大のボトルネック）

**場所:** `CompositionRenderController.cppm:2087` → `ArtifactIRenderer.cppm:318-416`

補足メモ:
- `docs/bugs/GPU_READBACK_NOTES_2026-03-26.md`

**毎フレーム**以下の処理が走る:
1. ステージングテクスチャ作成
2. `ctx->CopyTexture()` — GPU→CPU コピー
3. `ctx->Flush()` + `fence->Wait()` — **CPU ブロッキング**
4. ステージングテクスチャをマップ
5. 新規 QImage アロケーション (全フレームバッファ分)
6. 行ごとに memcpy

→ フルパイプラインストール。レンダリング完了を待ってからCPU側にコピー。

**対策:**
- 再生中/Idle 時のみ or 変更時のみ readback
- キャッシュした QImage を変更がなければ再利用
- readback の条件を `needsReadback` フラグで制御

### ★★★ 2. シグナルストーム / 二重レンダリング

**場所:** `CompositionRenderController.cppm:675, 780, 783, 1066`

レイヤープロパティ変更時のカスケード:
```
opacity 変更
  → layer->changed() → renderOneFrame()      ← 呼び出し1
  → composition->changed → renderOneFrame()   ← 呼び出し2
  → renderRescheduleRequested_ = true → 3回目  ← 呼び出し3
```

→ プロパティ1回の変更で **2〜3回のフルレンダリング**。

**対策:**
- `layer->changed` の接続を削除し `composition->changed` のみ使用
- または `changed` シグナルにバッチング (changed_within_frame) を導入
- `renderRescheduleRequested_` のガード強化

### ★★ 3. renderOneFrame() が46箇所から呼ばれている

**場所:** `CompositionRenderController.cppm` 全体

| トリガー | 問題 |
|---------|------|
| `layerCreated` (line 780 + 783) | **2回連続呼び出し** |
| `setComposition` (line 1027+1085+1088) | **3回呼び出し** |
| `frameChanged` (line 849) | 再生中**毎フレーム**呼び出し |
| mouseMove (line 1510+1547+1588+1594) | 1 mouseMove 内で**最大4回** |

**対策:**
- 重複呼び出し箇所を削除
- mouseMove 内の呼び出しを1回に統合
- フレームレートリミッター導入 (16ms 間隔でコアレシング)

### ★★ 4. OpenCV CPU 処理がホットパスに

**場所:** `CompositionRenderController.cppm:335-367`

マスク/エフェクト付きレイヤー毎フレーム:
1. `QImage → cv::Mat` コピー
2. `uint8 → float32` 変換
3. エフェクト適用
4. `cv::Mat → QImage` コピー

**対策:**
- マスク/エフェクトが変更されない限り結果をキャッシュ
- 変更検出フラグ (`maskDirty_`, `effectDirty_`) を導入

### ★ 5. 二重テクスチャキャッシュ

`PrimitiveRenderer2D::m_spriteTexCache` と `GPUTextureCacheManager` が独立してテクスチャを作成。

**問題点:**
- 同じ画像が両方のキャッシュに存在
- LRU フレームカウンターが `drawSpriteTransformed` 呼び出し毎にインクリメント（正しくはフレーム毎）
- `pruneCache()` が不正確にエントリを削除

**対策:**
- テクスチャキャッシュを1つに統一
- フレームカウンターをレンダーループ毎に1回だけインクリメント

### ★ 5.5 QImage アロケーションがホットパス

| 場所 | アロケーション | 頻度 |
|------|---------------|------|
| `ArtifactIRenderer.cppm:409` | フルフレームバッファ | 毎フレーム |
| `CompositionRenderController.cppm:460,481` | Solid2D surface | Per layer/frame |
| `PrimitiveRenderer2D.cppm:1280` | RGBA8888 変換 | キャッシュミス時 |
| `CompositionRenderController.cppm:335` | cv::Mat 変換 | Per layer/frame |

**対策:**
- フレームバッファ QImage をメンバとして保持（再利用）
- RGBA8888 変換を `loadFromPath()` 時に1回のみ

---

## 推奨対応順

| 順序 | 対応 | 効果 | 見積 |
|---|---|---|---|
| 1 | GPU readback の条件付き無効化 | 最大 | 2h |
| 2 | シグナル二重発火の抑制 | 大 | 2h |
| 3 | renderOneFrame() 重複呼び出し削除 | 大 | 1h |
| 4 | フレームバッファ QImage 再利用 | 中 | 1h |
| 5 | OpenCV 処理結果のキャッシュ | 中 | 2h |
| 6 | テクスチャキャッシュ統一 | 中 | 3h |

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 675 | composition->changed → renderOneFrame |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 780,783 | layerCreated → 二重呼び出し |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 849 | frameChanged → 毎フレーム |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 1027,1085,1088 | setComposition → 三重呼び出し |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 1636-1682 | renderOneFrame スロットリング |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 2087 | readbackToImage 呼び出し |
| `Artifact/src/Render/ArtifactIRenderer.cppm` | 318-416 | readbackToImage 実装 (GPU ストール) |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 67 | m_spriteTexCache |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 335-367 | OpenCV 変換 |
