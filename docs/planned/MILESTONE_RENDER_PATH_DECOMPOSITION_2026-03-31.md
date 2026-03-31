# マイルストーン: Render Path Decomposition / Buffer Migration

> 2026-03-31 作成

## 目的

レンダーパスの責務を分解し、`QImage` 依存を段階的に減らしながら、内部表現を typed buffer に寄せる。

最終的には、

- I/O 境界: `QImage` / `RawImage`
- 内部表現: `ImageF32x4_RGBA` 系の typed buffer
- 描画経路: pass ごとに責務分離

を目指す。

---

## 背景

現在の描画経路は、UI 表示、CPU 合成、GPU upload、readback がかなり密結合している。

その結果:

- `QImage` が内部パスに入り込みやすい
- フォーマット差異が見えにくい
- readback / upload / composite の責務が混ざりやすい
- LOD / cache / proxy を導入しにくい

このマイルストーンは、将来の最適化や backend 差し替えの前提を作る。

---

## 方針

### 原則

1. `QImage` をいきなり全廃しない
2. `RawImage` は I/O 境界に置く
3. レンダーパス内部は typed buffer に寄せる
4. pass を小さく分けて責務を明確にする
5. 既存の見た目を壊さず段階移行する

### 想定する役割分担

- `QImage`
  - UI 表示
  - Qt 互換境界
  - 一時的な既存 API の受け口

- `RawImage`
  - デコード/エンコードの中間
  - ファイル I/O の境界
  - 素のバイナリ + 基本メタデータ

- typed buffer
  - render/composite/effect の内部表現
  - linear / HDR / half / float を扱う
  - stride / alpha mode / colorspace を明示する

---

## Phase 1: Internal Buffer Definition

### 目的

内部表現の型を 1 つ決め、レンダーパスの基準にする。

### 作業項目

- `ImageBuffer` / `FrameBuffer` / `LinearImageBuffer` のいずれかを定義する
- `width`, `height`, `stride`, `format`, `alphaMode`, `colorSpace` を持たせる
- `float32` / `half` / `unorm8` を区別できるようにする
- `QImage` への変換関数は出口側に閉じ込める

### 完了条件

- render 内部で `QImage` を直接持ち回らない
- 画像フォーマットの意味が明示される

---

## Phase 2: Pass Decomposition

### 目的

描画の責務を pass 単位に分解する。

### 作業項目

- `source / decode`
- `layout / transform`
- `raster / resolve`
- `composite`
- `post`
- `readback`

を独立した処理として切り出す。

### 完了条件

- 各 pass の入力と出力が明確になる
- どの pass が重いか測れる
- partial caching を差し込める

---

## Phase 3: QImage Boundary Reduction

### 目的

内部処理から `QImage` を追い出し、互換境界だけに残す。

### 作業項目

- `ArtifactImageLayer`
- `ArtifactVideoLayer`
- `ArtifactTextLayer`
- `ArtifactSvgLayer`

の内部処理を typed buffer 経由へ寄せる

- `drawLayerForCompositionView()` の QImage 依存を削る
- `GPUTextureCacheManager` の入力を typed buffer ベースに拡張する

### 完了条件

- render path の主要経路が `QImage` 非依存になる
- `QImage` は UI と旧互換の出入口のみになる

---

## Phase 4: Render Queue / Offline Integration

### 目的

レンダーキューとオフライン GPU 書き出しを、新しい内部表現に合わせる。

### 作業項目

- render queue の snapshot / clone 経路を typed buffer と整合させる
- offline render と preview render の責務を分ける
- video / audio / image sequence を同じ型変換ポリシーで扱う

### 完了条件

- render queue が preview と同じ見た目を安定して再現できる
- 出力フォーマットごとの分岐が最小化される

---

## Non-Goals

- いきなり完全な GPU only renderer にすること
- `QImage` を全箇所から一括で削除すること
- 一足飛びの backend 置換

---

## 成果物イメージ

- `ImageBuffer` / `FrameBuffer` の内部型
- `QImage` 変換ユーティリティ
- pass ごとの render helper
- render queue の typed buffer 対応
- proxy / LOD / cache の導線整備

