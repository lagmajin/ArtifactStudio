# 画像レイヤー 1 枚が重い原因仮説 (2026-03-25)

## 症状

単純な画像 1 枚をコンポジットエディタに表示するだけで動作が重い。

---

## 調査更新 (2026-03-27)

**状態:** 仮説として挙げられた問題は**事実確認され、未修正**。

### 事実確認された問題

| # | 問題 | 深刻度 | 状態 | 場所 |
|---|------|--------|------|------|
| 1 | テクスチャキャッシュが毎フレームミス (QImage::cacheKey()) | ★★★ | **未修正** | `PrimitiveRenderer2D.cppm:1254` |
| 2 | QImage::convertToFormat が毎回ヒープアロケーション | ★★★ | **未修正** | `PrimitiveRenderer2D.cppm:1262` |
| 3 | 1 レイヤーあたり 10+ 回の GPU API 呼び出し | ★★ | **未修正** | `CompositionRenderController.cppm:1362-1407` |
| 4 | getGlobalTransform() の毎フレーム計算 | ★ | **未修正** | `drawLayerForCompositionView()` |

### 実装状況

記載された対策案 (キャッシュをファイルパスベースに変更，テクスチャ作成を 1 回に，レンダリングパイプラインの簡素化，getGlobalTransform のキャッシュ) は**全て未実装**。

---

## パイプライン概要

```
renderOneFrame()
  ├─ 背景クリア/描画
  ├─ グリッド描画
  ├─ for each layer:
  │    ├─ setOverrideRTV(layerRTV)
  │    ├─ clear()
  │    ├─ drawSpriteTransformed()  ← QImage → GPU テクスチャ
  │    │    ├─ convertToFormat(RGBA8888)  ← CPU 変換
  │    │    ├─ CreateTexture()            ← GPU 転送
  │    │    ├─ MapBuffer ×2 (vertex + CB)
  │    │    ├─ SetPipelineState
  │    │    ├─ CommitShaderResources
  │    │    ├─ SetVertexBuffers / SetIndexBuffer
  │    │    └─ DrawIndexed
  │    ├─ setOverrideRTV(nullptr)
  │    ├─ blend()                    ← Compute dispatch
  │    └─ swapAccumAndTemp()
  └─ drawSprite(accum)              ← 最終出力
```

1 枚の画像のために **3 テクスチャ** (layer/accum/temp) と **4 描画パス** が発生。

---

## 仮説

### ★ 仮説 1: テクスチャキャッシュが毎フレームミス（最も有力）

**場所:** `PrimitiveRenderer2D.cppm:1272-1306`

```cpp
qint64 cacheKey = image.cacheKey();  // QImage のポインタベースのキー
auto it = impl_->m_spriteTexCache.find(cacheKey);
if (it != impl_->m_spriteTexCache.end()) {
    // ヒット → 再利用
} else {
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);  // ← 毎回変換
    pDevice_->CreateTexture(texDesc, &initData, &pTexture);              // ← GPU 転送
}
```

`QImage::cacheKey()` は QImage オブジェクトのアドレス/世代ベース。
`toQImage()` が毎回同じ `QImage*` を返していても `cacheKey()` が変わる場合がある。
またはプリフェッチ完了前 → `QImage()` → 完了後 `*cache_` でキーが変化。

**結果:** 毎フレーム `convertToFormat` + `CreateTexture` が実行。

### ★ 仮説 2: QImage::convertToFormat が毎回ヒープアロケーション

**場所:** `PrimitiveRenderer2D.cppm:1280`

```cpp
const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
```

キャッシュミス時に毎回 `RGBA8888` 変換。
4K 画像 (3840×2160×4 = 33MB) の場合、毎フレーム 33MB のメモリアロケーション +
ピクセル変換処理が CPU で実行される。

### 仮説 3: 1 レイヤーあたり 10+ 回の GPU API 呼び出し

**場所:** `CompositionRenderController.cppm:1382-1402`

画像 1 枚表示の毎フレームの API 呼び出し:
1. `setOverrideRTV(layerRTV)` — RT 切替
2. `clear()` — クリアパス
3. `drawSpriteTransformed()` 内部:
   - `MapBuffer` × 2 (vertex + constant buffer)
   - `SetRenderTargets`
   - `SetPipelineState`
   - `CommitShaderResources`
   - `SetVertexBuffers`
   - `SetIndexBuffer`
   - `DrawIndexed`
4. `setOverrideRTV(nullptr)` — RT 復帰
5. `blend()` — Compute dispatch
6. `swapAccumAndTemp()` — テクスチャ入れ替え

各呼び出しで `RESOURCE_STATE_TRANSITION_MODE_TRANSITION` を使用。
D3D12 では各 transition がバリアコマンドとして発行される。

### 仮説 4: 描画パスの無駄なオーバーヘッド

画像 1 枚のために:
- 背景クリア/描画
- グリッド描画（表示中の場合）
- layer テクスチャへのレンダリング
- コンピュートブレンド（`blendPipeline_->blend()`）
- accum テクスチャからスワップチェーンへの最終描画（`drawSprite`）

→ フォールバックパス（GPU ブレンド無し）の方が軽い可能性がある。

### 仮説 5: getGlobalTransform() の毎フレーム計算

**場所:** `drawLayerForCompositionView()` 内

```cpp
renderer->drawSpriteTransformed(0, 0, w, h, layer->getGlobalTransform(), img, opacity);
```

`getGlobalTransform()` は親チェーンを辿って行列を合成。
アニメーションがなければ同じ値だが毎フレーム再計算。
キャッシュされていない。

### 仮説 6: プリフェッチ未完了時のフォールバック

**場所:** `ArtifactImageLayer.cppm:172-176`

```cpp
if (!impl_->prefetchDone_ && impl_->prefetchFuture_.isRunning()) {
    return QImage();  // 空画像 → このフレームは何も描画されない
}
```

プリフェッチ完了後は `*impl_->cache_` を返すが、
この時点で `cacheKey()` が変わる可能性 → 仮説 1 のキャッシュミス。

---

## 対策案

### 対策 1: キャッシュをファイルパスベースに変更
```cpp
// 現在：qint64 cacheKey = image.cacheKey();
// 修正：qint64 cacheKey = qHash(layer->sourcePath());
```
→ 同じ画像ファイルなら同じキーで再利用。

### 対策 2: テクスチャ作成を 1 回のみに
`loadFromPath()` 完了時にテクスチャを事前作成し、`draw()` では SRV のみ再利用。

### 対策 3: レンダリングパイプラインの簡素化
GPU ブレンド不要の場合（画像 1 枚、opacity=1、Normal ブレンド）:
- layer テクスチャに直接描画 → スワップチェーンにコピー
- `blend()` / `swapAccumAndTemp()` をスキップ

### 対策 4: getGlobalTransform のキャッシュ
行列が変更された時のみ再計算。`AnimatableTransform3D` に変更フラグを追加。

---

## 確認方法

1. `m_spriteTexCache` のヒット率をログ出力
2. Perfetto / GPUView で `CreateTexture` の呼び出し頻度を確認
3. フォールバックパス（GPU ブレンド無し）と比較してパフォーマンス差を測定
4. 画像解像度を変えて（100x100 vs 4K）パフォーマンス差を確認

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Layer/ArtifactImageLayer.cppm` | 46-85 | loadFromPath (プリフェッチ) |
| `Artifact/src/Layer/ArtifactImageLayer.cppm` | 154-199 | toQImage (キャッシュ取得) |
| `Artifact/src/Layer/ArtifactImageLayer.cppm` | 135-152 | draw (描画呼び出し) |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 1261-1396 | drawSpriteTransformed |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 1272-1306 | テクスチャキャッシュ (問題箇所) |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 1362-1407 | GPU ブレンドパス |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 1410-1446 | フォールバックパス |
