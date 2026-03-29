# レンダリング性能改善 Milestone

**作成日:** 2026-03-28  
**ステータス:** 計画中  
**関連コンポーネント:** ArtifactIRenderer, CompositionRenderController, PrimitiveRenderer2D, PrimitiveRenderer3D

---

## 概要

レンダリングパイプラインの性能を改善し、60fps 維持とメモリ使用量削減を実現する。

---

## 発見された問題点

### ★★★ 問題 1: テクスチャキャッシュの非効率

**場所:** `Artifact/src/Render/PrimitiveRenderer2D.cppm:1254-1286`

**問題:**
```cpp
qint64 cacheKey = image.cacheKey();  // QImage のポインタベース
auto it = impl_->m_spriteTexCache.find(cacheKey);
if (it != impl_->m_spriteTexCache.end()) {
    // ヒット
} else {
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);  // 毎回変換
    pDevice_->CreateTexture(...);  // GPU 転送
}
```

**影響:**
- `QImage::cacheKey()` はインスタンスベースで、毎回変わる
- 毎フレーム 33MB(4K 画像) の変換 +GPU 転送
- メモリ帯域の浪費

**工数:** 4-6 時間

---

### ★★★ 問題 2: ギズモ描画の過剰 GPU 呼び出し

**場所:** `Artifact/src/Widgets/Render/TransformGizmo.cppm:117-180`

**問題:**
- 1 フレームで 241 回の GPU 描画呼び出し
- `drawCircle(filled)` が 128 回の太い線を描画
- 1 本の線で 7 回の GPU API 呼び出し

**内訳:**
| 要素 | 描画関数 | 呼び出し数 |
|------|---------|-----------|
| バウンディングボックス | `drawSolidLine` × 4 | 4 |
| 8 ハンドル (塗り) | `drawSolidRect` × 8 | 8 |
| 8 ハンドル (枠線) | `drawRectOutline` × 8 | 32 |
| 回転ハンドル (線) | `drawSolidLine` × 1 | 1 |
| 回転ハンドル (枠円) | `drawCircle(outline)` | 64 |
| 回転ハンドル (塗り円) | `drawCircle(filled)` | **~128** |
| アンカーポイント | `drawCrosshair` | 4 |
| **合計** | | **~241** |

**工数:** 3-4 時間

---

### ★★ 問題 3: CompositionRenderController のシグナルストーム

**場所:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

**問題:**
```cpp
// レイヤープロパティ変更時
opacity 変更
  → layer->changed() → renderOneFrame()      ← 呼び出し 1
  → composition->changed → renderOneFrame()   ← 呼び出し 2
  → renderRescheduleRequested_ = true → 3 回目  ← 呼び出し 3
```

**影響:**
- プロパティ 1 回の変更で 2〜3 回のフルレンダリング
- UI のラグ

**工数:** 2-3 時間

---

### ★★ 問題 4: 不要な readback

**場所:** `Artifact/src/Render/ArtifactIRenderer.cppm:354-416`

**問題:**
- 毎フレーム GPU→CPU の読み戻し
- `ctx->Flush()` + `fence->Wait()` で CPU ブロッキング
- フルパイプラインストール

**工数:** 3-4 時間

---

### ★ 問題 5: OpenCV CPU 処理のホットパス

**場所:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:335-367`

**問題:**
- マスク/エフェクト付きレイヤー毎フレーム
- `QImage → cv::Mat` コピー
- `uint8 → float32` 変換
- エフェクト適用
- `cv::Mat → QImage` コピー

**工数:** 4-6 時間

---

## 優先度別実装計画

### P0（必須）

| 項目 | 工数 | 優先度 | 効果 |
|------|------|--------|------|
| **テクスチャキャッシュ改善** | 4-6h | 🔴 高 | フレームレート向上 |
| **ギズモ描画最適化** | 3-4h | 🔴 高 | GPU 負荷削減 |

### P1（重要）

| 項目 | 工数 | 優先度 | 効果 |
|------|------|--------|------|
| **シグナルストーム防止** | 2-3h | 🟡 中 | UI レイテンシ改善 |
| **不要な readback 削除** | 3-4h | 🟡 中 | CPU 負荷削減 |

### P2（推奨）

| 項目 | 工数 | 優先度 | 効果 |
|------|------|--------|------|
| **OpenCV 処理キャッシュ** | 4-6h | 🟢 低 | CPU 負荷削減 |

**合計工数:** 16-23 時間

---

## Phase 構成

### Phase 1: テクスチャキャッシュ改善

- 目的:
  - 毎フレームの GPU テクスチャ作成を防止

- 作業項目:
  - レイヤー ID ベースのキャッシュキー
  - 永続的テクスチャキャッシュ
  - LRU による自動削除

- 完了条件:
  - 同じ画像でキャッシュヒット率 90% 以上
  - GPU 転送が初回のみ

- 実装案:
  ```cpp
  // 修正前
  qint64 cacheKey = image.cacheKey();  // インスタンスベース
  
  // 修正後
  qint64 cacheKey = qHash(layerId.toString() + ":" + 
                          QString::number(frameNumber));
  
  // またはレイヤー ID のみ
  qint64 cacheKey = qHash(layer->id().toString());
  ```

### Phase 2: ギズモ描画最適化

- 目的:
  - GPU 呼び出し回数を削減

- 作業項目:
  - `drawCircle(filled)` を `drawSolidCircle` に置換
  - `drawRectOutline` を 1 回描画に統合
  - ギズモのバッチ描画

- 完了条件:
  - ギズモ描画が 241 回 → 20 回に削減
  - 60fps 維持

- 実装案:
  ```cpp
  // 修正前：128 回の描画
  void drawCircle(float x, float y, float radius, ..., bool fill) {
      if (fill) {
          for (float r = 0.5f; r <= radius; r += 1.0f) {
              drawCircle(x, y, r, color, 1.5f, false);  // 128 回
          }
      }
  }
  
  // 修正後：1 回の描画
  void drawSolidCircle(float cx, float cy, float radius, ...) {
      constexpr int segments = 32;
      TriangleVertex vertices[segments * 3];
      // ファン状の三角形を 1 回で描画
      drawTriangles(vertices, segments * 3);
  }
  ```

### Phase 3: シグナルストーム防止

- 目的:
  - 不要な再レンダリングを防止

- 作業項目:
  - `changed()` シグナルのデバウンス
  - 16ms 間隔でマージ
  - 優先度ベースのスケジューリング

- 完了条件:
  - プロパティ変更で 1 回のみのレンダリング
  - UI の応答性向上

### Phase 4: 不要な readback 削除

- 目的:
  - CPU ブロッキングを防止

- 作業項目:
  - readback の条件付き無効化
  - プレビュー時のみ有効
  - キャッシュの活用

- 完了条件:
  - 通常動作で readback 発生なし
  - プレビュー時は正しく機能

---

## 技術的課題

### 1. キャッシュの一貫性

**課題:**
- レイヤーが変更された時にキャッシュを無効化

**解決案:**
- レイヤー変更フラグ
- バージョン管理
- 明示的無効化 API

### 2. メモリ使用量

**課題:**
- テクスチャキャッシュのメモリ増加

**解決案:**
- LRU による自動削除
- メモリ制限（例：512MB）
- 使用頻度の低いエントリから削除

### 3. ギズモの視認性

**課題:**
- 最適化でギズモが見えにくくならないか

**解決案:**
- 視認性テスト
- ユーザー設定可能な品質
- フォールバック経路

---

## 期待される効果

### 性能向上

| 指標 | 現在 | 改善後 | 向上率 |
|------|------|--------|--------|
| **フレームレート** | 30-45fps | 60fps | +33-100% |
| **GPU 呼び出し/帧** | 2000+ | 500 | -75% |
| **メモリ帯域** | 1GB/s | 200MB/s | -80% |
| **CPU 使用率** | 40% | 15% | -62% |

### ユーザー体験

- スクラブ中のカクつき消除
- プロパティ変更の即時反映
- 長時間編集でも安定

---

## 関連ドキュメント

- `docs/bugs/COMPOSITION_EDITOR_PERFORMANCE_2026-03-26.md` - 性能問題
- `docs/bugs/GIZMO_DRAWING_PERFORMANCE_2026-03-26.md` - ギズモ描画
- `docs/planned/MILESTONES_BACKLOG.md` - 全体バックログ

---

## 実装順序の推奨

1. **Phase 1: テクスチャキャッシュ** - 効果大、実装容易
2. **Phase 2: ギズモ最適化** - 効果大、実装容易
3. **Phase 3: シグナルストーム** - 中程度の効果
4. **Phase 4: readback 削除** - 中程度の効果

---

**文書終了**
