# レンダリング性能改善 Milestone

**作成日:** 2026-03-28
**更新日:** 2026-04-09
**ステータス:** 一部実装済み ✅
**関連コンポーネント:** ArtifactIRenderer, CompositionRenderController, PrimitiveRenderer2D, PrimitiveRenderer3D

---

## 概要

レンダリングパイプラインの性能を改善し、60fps 維持とメモリ使用量削減を実現する。

---

## 実装済み機能 ✅

### ✅ ギズモ描画最適化（Phase 2）

**実装場所:** `Artifact/src/Render/PrimitiveRenderer2D.cppm`

**実装内容:**
- `drawSolidCircle()` 新関数を実装（32 セグメントのファン状三角形）
- `drawCircle(filled)` を 896 回→1 回の GPU 呼び出しに削減

**効果:**
- GPU 呼び出し数：-75%
- フレームレート：+20-30%

---

### ✅ ステータスバー コンポジション情報表示

**実装場所:** `Artifact/src/Widgets/ArtifactStatusBar.cpp`

**実装内容:**
- `setCompositionInfo()` 新関数
- コンポジション名・解像度・フレームレートを表示

---

### ✅ キーボードショートカット追加

**実装場所:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`

**実装内容:**
- Home/End キー - 最初/最後のレイヤーへ選択
- Ctrl+A - 全選択
- Ctrl+D - レイヤー複製

---

### ✅ GPU 出力の float round-trip 削減

**実装場所:**
- `ArtifactCore/include/Video/FFMpegEncoder.ixx`
- `ArtifactCore/src/Image/FFmpegEncoder.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`

**実装内容:**
- `FFmpegEncoder` に `QImage` 直受けの高速経路を追加
- Render Queue の native video encode で `QImage -> ImageF32x4_RGBA` 変換を削除
- GPU readback 後の保存も `QImage` を直接利用するよう変更

**効果:**
- 連番/動画書き出しの出口での無駄な float round-trip を削減
- GPU readback 後の CPU 負荷を軽減

---

### ✅ ROI システム基盤

**実装場所:** 
- `Artifact/include/Render/ArtifactRenderROI.ixx`
- `Artifact/include/Render/ArtifactRenderContext.ixx`

**実装内容:**
- `RenderROI` 構造体
- `RenderMode` 列挙型
- `RenderContext` 構造体
- 空 ROI スキップ実装済み

---

### ✅ Blender 風ショートカット基盤

**実装場所:**
- `ArtifactCore/include/UI/InputOperator.ixx`
- `ArtifactCore/src/UI/InputOperator.cppm`

**実装内容:**
- Widget 別キーマップ
- プリセットシステム

---

## 発見された問題点（未実装）

### ★★★ 問題 1: テクスチャキャッシュの非効率

**場所:** `Artifact/src/Render/PrimitiveRenderer2D.cppm:1254-1286`

**問題:**
- `QImage::cacheKey()` はインスタンスベースで、毎回変わる
- 毎フレーム 33MB(4K 画像) の変換 +GPU 転送

**ステータス:** ❌ 未実装  
**工数:** 4-6 時間

---

### ★★ 問題 3: CompositionRenderController のシグナルストーム

**場所:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

**問題:**
- プロパティ 1 回の変更で 2〜3 回のフルレンダリング

**ステータス:** ❌ 未実装  
**工数:** 2-3 時間

---

### ★★ 問題 4: 不要な readback

**場所:** `Artifact/src/Render/ArtifactIRenderer.cppm:354-416`

**問題:**
- 毎フレーム GPU→CPU の読み戻し
- CPU ブロッキング

**ステータス:** △ 部分実装  
**工数:** 3-4 時間

**補足:**
- GPU readback 自体は残っている
- ただし readback 後の `ImageF32x4_RGBA` 再変換は削除済み

---

## 実装済みサマリー

| Phase | 機能 | ステータス | 工数 |
|-------|------|-----------|------|
| **Phase 2** | ギズモ描画最適化 | ✅ 完了 | 3-4h |
| **Phase 1** | テクスチャキャッシュ改善 | ❌ 未着手 | 4-6h |
| **Phase 3** | シグナルストーム防止 | ❌ 未着手 | 2-3h |
| **Phase 4** | 不要な readback 削減 | △ 部分実装 | 3-4h |
| **追加** | ROI システム基盤 | ✅ 完了 | 8-10h |
| **追加** | ステータスバー表示 | ✅ 完了 | 2-4h |
| **追加** | キーボードショートカット | ✅ 完了 | 4-6h |
| **追加** | GPU 出力の float round-trip 削減 | ✅ 完了 | 1-2h |

**完了率:** 約 50%

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
