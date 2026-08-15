# マイルストーン: Render Path Decomposition / Buffer Migration

**最終更新:** 2026-08-15
**ステータス:** Typed buffer／cache／GPU upload 基盤は実装済み、QImage経路縮小・pass契約・runtime検証 pending

## 現行コード監査 (2026-08-15)

`ImageF32x4_RGBA` が内部の float RGBA 表現として存在し、`ImageF32x4_With_Cache` が CPU dirty state と GPU の shader-resource / unordered-access view の同期を担っている。`RawImage` は import 側にあり、`toQImage()` などの明示的な変換境界も確認できる。したがって typed buffer、cache、GPU upload の基盤は実装済みと判定する。

ただし、`ArtifactCompositionRenderController` では現在も QImage を surface、matte source、resolved source、preview/readback の中間値として保持する経路が残っている。QImage は UI・I/O だけに閉じておらず、layer 合成や matte 処理の内部にも入るため、QImage 経路の縮小と pass ごとの責務分離は未完了である。`QImage` の新規採用を増やす状態ではないが、既存経路の置換を完了した証拠はない。

**判定:** Phase 1（typed buffer と明示変換の基盤）は実装済み。Phase 2（render/composite pass の分解）、QImage の I/O 境界限定、format/alpha/colorspace 契約の全経路適用、GPU/CPU parity と runtime 受入れは pending。

## Update 2026-08-15

- `ImageF32x4_RGBA`／`ImageF32x4_With_Cache`、明示的な `toQImage()` 境界、GPU upload／surface cache の基盤を再確認。
- `CompositionRenderController` には setup／base／surface／mask／composite／post／overlay／flush／present の frame pass plan と診断要約があるため、pass 分解の足場は実装済み。
- ただし QImage は surface、matte、resolved source、preview/readback に残っており、主要 render path の typed buffer 化、format／alpha／colorspace 契約の全経路適用、GPU／CPU parity は未完了・未検証。

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

## Static Audit (2026-08-15)

現行ソースでは、`ImageF32x4_RGBA` と `ImageUploadBuffer` が typed buffer の中心として使われ、`GPUTextureCacheManager`、`ArtifactCompositionViewDrawing`、video/image/svg/text の current frame buffer、LOD、buffer cache、GPU upload まで接続されている。`RenderCommandBuffer`、`PrimitiveRenderer2D/3D`、offscreen renderer、post-process は描画責務をある程度分離している。最終コンポジションエフェクトにも `applyCompositionFinalEffectsToBuffer()` のtyped-buffer入口を追加し、QImage版のレイヤー rasterizer はtyped-buffer版へ委譲する構造に整理した。GPU Render Queue の通常フレーム出力と単発GPU出力の両方で `readbackToImageF32()` を経由し、crop／resize／最終エフェクトをQImage化前に適用する経路へ接続した。ソフトウェアコンポジターにも既存QImage処理を互換境界として利用する `composeToBuffer()` を追加し、Hue／Saturation／Color／Luminosity／Dissolve／DancingDissolve はOpenCVのfloat合成ループへ移行した。Stencil／Silhouette系はアルファプレーンを追加し、RGB合成を壊さずに出力アルファを更新する経路を実装した。

ただし、マイルストーンの完了とは判定しない。`ArtifactCompositionViewDrawing` には buffer から `QImage` へ戻す分岐、QImage surface fallback、renderer readback が残っており、内部主要経路が完全に QImage 非依存になったとは言えない。`RawImage` / `FrameBuffer` の統一契約、format/alpha/color-space/stride の共通メタデータ、source→decode→layout→raster→composite→post→readback の明示 pass graph、各 pass の timing/cache boundary は一貫した公開契約として確認できない。

Render queue/offline と preview の typed-buffer 同一化、各 layer の旧経路撤去、QImage 変換を出口に限定する保証、runtime での見た目一致も未検証である。現行コードには `ArtifactCompositionViewDrawing` の QImage surface／matte／effect 境界と renderer readback が残るため、QImage hot path の完全撤去とは判定できない。したがって Phase 1 は部分実装、Phase 2 は helper/renderer 分解として partial、Phase 3/4 は移行途中と判定する。
