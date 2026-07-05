# M-ANIM-HEADLESS-1 Dope Sheet / Squash & Stretch / Bitmap Brush Engine (UI統合なし)

作成日: 2026-07-04  
ステータス: Draft  
方針: **UI 統合はこの milestone では行わない**。`Timeline` / `Composition Editor` / `Property Panel` への常設導線は後続 slice に分離し、今回は **core / service / model / serialization / diagnostics / test seam** のみを整備する。

参照:
- [docs/planned/MILESTONE_TIMELINE_INDEX_2026-04-22.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_INDEX_2026-04-22.md)
- [docs/done/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md](/x:/Dev/ArtifactStudio/docs/done/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md)
- [docs/planned/MILESTONE_PAINT_LAYER_2026-06-16.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_PAINT_LAYER_2026-06-16.md)
- [docs/planned/MILESTONE_FRAME_BY_FRAME_ANIMATION_2026-07-03.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_FRAME_BY_FRAME_ANIMATION_2026-07-03.md)
- [docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md](/x:/Dev/ArtifactStudio/docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md)
- [docs/analysis/REPORT_AE_GAP_UPDATE_2026-07-03.md](/x:/Dev/ArtifactStudio/docs/analysis/REPORT_AE_GAP_UPDATE_2026-07-03.md)

---

## 1. 目的

以下の 3 系統を、互いに UI 依存で詰まらないように独立した下層機能として先行整備する。

1. `Dope Sheet`
2. `Squash & Stretch`
3. `Bitmap Brush Engine`

この段階では:

- timeline 右ペインの常設拡張はしない
- composition viewport への常設ブラシ UI は出さない
- property panel の新規 row も増やさない

代わりに:

- データモデル
- 操作 service
- bake / apply API
- undo 単位
- persistence
- diagnostics

を先に閉じる。

---

## 2. なぜ UI 統合なしで先に進めるか

### 2.1 Dope Sheet

既存 timeline は `keyframe edit surface` の土台があるが、`Dope Sheet` に必要な **複数 property を横断した bulk move / scale / offset** は、まず model/service として整理しないと right-pane UI と密結合になりやすい。

### 2.2 Squash & Stretch

自然な `つぶれ / 伸び` は見た目のツールより先に、

- velocity / acceleration の計測
- transform keyframe の補助生成
- original keyframe との差分保持

が必要。これは UI より先に animation service として切り出せる。

### 2.3 Bitmap Brush Engine

TVPaint 系のブラシは UI よりも、

- stroke data
- dab evaluation
- texture sampling
- pressure / tilt / rotation
- raster target format

の整備が本体。ここを先に確定しないと paint layer UI を作っても作り直しになる。

---

## 3. スコープ

### In Scope

- keyframe の横断操作を行う dope-sheet core model
- keyframe batch offset / scale / ripple の service
- squash & stretch 補助キー生成 service
- brush stroke / brush preset / dab engine / raster apply core
- JSON serialize / deserialize
- undo command 単位の設計
- diagnostics / report text / snapshot vocab

### Out of Scope

- Dope Sheet 専用 dock / widget
- Xsheet view
- viewport 上の本格 brush HUD
- stylus settings dialog
- brush preset browser UI
- squash & stretch の viewport gizmo
- timeline 上の専用 row / lane 可視化

---

## 4. Workstream A: Dope Sheet Core

### 4.1 Goal

`Dope Sheet` 相当の編集を支える **UI 非依存 keyframe aggregate model** を用意する。

必要要件:

- 全 property keyframe を flatten して列挙できる
- 複数 property / 複数 layer をまとめて選択できる
- 選択 keyframe 群に対して offset / scale / duplicate / trim を適用できる
- source of truth は既存 `AbstractProperty` の keyframe を維持する

### 4.2 新規 core

候補:

- `ArtifactCore/include/Animation/DopeSheetKeyframe.ixx`
- `ArtifactCore/include/Animation/DopeSheetSelection.ixx`
- `ArtifactCore/include/Animation/DopeSheetService.ixx`
- `ArtifactCore/src/Animation/DopeSheetService.cppm`

### 4.3 Data Model

```cpp
struct DopeSheetKeyframeRef {
    QString compositionId;
    QString layerId;
    QString propertyPath;
    qint64 frame;
    QVariant value;
    int interpolation = 0;
    bool roving = false;
};

struct DopeSheetSelection {
    QVector<DopeSheetKeyframeRef> refs;
    qint64 anchorFrame = 0;
};
```

### 4.4 Core Operations

- `collectKeyframes(composition, filters)`
- `offsetSelection(selection, deltaFrames)`
- `scaleSelection(selection, pivotFrame, scaleFactor)`
- `duplicateSelection(selection, insertFrame)`
- `deleteSelection(selection)`
- `summarizeSelection(selection)`  
  例: layer 数 / property 数 / frame span / collision 数

### 4.5 Guardrails

- keyframe の truth は複製しない。apply 時に property へ戻す
- UI 座標や row geometry を model に持ち込まない
- layer lock / read-only property / hidden lane は UI ではなく service 側で skip policy を持つ

### 4.6 Done 条件

- `Timeline` を使わずとも keyframe batch shift / scale が API として動く
- 複数 property の同時オフセットが 1 undo 単位で適用できる
- report text で selection summary が読める

---

## 5. Workstream B: Squash & Stretch Core

### 5.1 Goal

Moho / Blender 的な squash & stretch を、**transform keyframe の補助生成 service** として整備する。

### 5.2 初期スライス

最初は次に限定する:

- 対象は 2D layer の transform
- 入力は position / scale keyframe
- 出力は scaleX / scaleY の補助 keyframe
- bake 専用。ライブ modifier にはしない

### 5.3 新規 core

候補:

- `ArtifactCore/include/Animation/SquashStretchProfile.ixx`
- `ArtifactCore/include/Animation/SquashStretchService.ixx`
- `ArtifactCore/src/Animation/SquashStretchService.cppm`

### 5.4 Data Model

```cpp
struct SquashStretchSettings {
    float stretchGain = 0.35f;
    float squashGain = 0.25f;
    float maxStretch = 1.8f;
    float maxSquash = 0.7f;
    float preserveVolume = 1.0f;
    int sampleRadiusFrames = 1;
    bool bakeToScaleKeys = true;
};

struct SquashStretchSample {
    qint64 frame = 0;
    QPointF velocity;
    float speed = 0.0f;
    float stretchX = 1.0f;
    float stretchY = 1.0f;
};
```

### 5.5 Core Operations

- `analyzeMotion(layer, frameRange, settings)`
- `previewStretchSamples(...)`
- `bakeStretchKeys(layer, samples, settings)`
- `clearBakedStretchKeys(layer, tag)`

### 5.6 Rules

- 元の `position` keyframe の意味は変えない
- bake した `scale` keyframe は metadata/tag で識別する
- hand-authored scale keyframe と衝突する場合の policy は Phase 1 で固定する  
  推奨: `Replace tagged only`, manual key は壊さない

### 5.7 Done 条件

- position keyframe 群から速度ベースの squash/stretch sample が取れる
- scale keyframe を bake / rollback できる
- UI なしでも CLI 的/サービス的に適用結果を検証できる

---

## 6. Workstream C: Bitmap Brush Engine Core

### 6.1 Goal

TVPaint 系ブラシの本体になる **dab/stroke engine** を paint layer UI とは独立に整備する。

### 6.2 初期スライス

Phase 1 は以下だけ:

- bitmap tip / grayscale alpha tip
- pressure
- tilt
- rotation
- spacing
- opacity / flow
- texture stamp

### 6.3 新規 core

候補:

- `ArtifactCore/include/Paint/BitmapBrushPreset.ixx`
- `ArtifactCore/include/Paint/BitmapBrushStroke.ixx`
- `ArtifactCore/include/Paint/BitmapBrushEngine.ixx`
- `ArtifactCore/src/Paint/BitmapBrushEngine.cppm`

### 6.4 Data Model

```cpp
struct BitmapBrushPreset {
    QString id;
    QString name;
    float radius = 12.0f;
    float spacing = 0.12f;
    float opacity = 1.0f;
    float flow = 1.0f;
    float rotationJitter = 0.0f;
    float scatter = 0.0f;
    bool usePressureRadius = true;
    bool usePressureOpacity = true;
    bool useTiltRotation = true;
    QImage tipPreview;
};

struct BitmapBrushPoint {
    QPointF position;
    float pressure = 1.0f;
    float tiltX = 0.0f;
    float tiltY = 0.0f;
    float rotation = 0.0f;
    qint64 timeUsec = 0;
};
```

### 6.5 Raster Contract

- canonical target は `ImageF32x4_RGBA` / `ImageF32x4RGBAWithCache`
- `QImage` は **brush preset import / preview / file boundary** に限定
- hot path の stroke apply では `QPainter` 合成へ逃がさない

### 6.6 Core Operations

- `stampDab(target, point, preset, color)`
- `strokeToDabs(points, preset)`
- `applyStroke(target, stroke)`
- `makeStrokePreview(points, preset)`  
  これは debug/report 用で render hot path ではない

### 6.7 Done 条件

- pressure / tilt / rotation を含む stroke から raster 結果が得られる
- tip texture を切り替えても apply API が変わらない
- PaintLayer 未統合でも engine 単体テストが可能

---

## 7. 実装順

### Phase 1

- `Dope Sheet Core`
- 既存 keyframe model / clipboard / undo との接続

### Phase 2

- `Squash & Stretch Core`
- transform bake / rollback metadata

### Phase 3

- `Bitmap Brush Engine Core`
- dab/stroke apply + preset serialization

### Phase 4

- diagnostics / report text / snapshot vocabulary 整理
- minimal test harness

---

## 8. Diagnostics / Test Seam

UI 統合なしでも進めるため、各 workstream は最初から診断導線を持つ。

### Dope Sheet

- keyframe count
- layer count
- property count
- frame span
- collision count

### Squash & Stretch

- sampled frame count
- max speed
- baked key count
- skipped manual key count

### Brush Engine

- point count
- generated dab count
- average spacing
- max radius
- texture tip id

---

## 9. 後続 UI Slice への受け渡し

この milestone 完了後、UI は別文書で受ける。

### 後続候補

1. `Dope Sheet View`  
   timeline right pane の派生 mode、または独立 panel

2. `Squash & Stretch Tool UI`  
   preset / strength / bake/clear ボタン

3. `Paint Surface UI`  
   brush cursor / preset list / layer binding / frame-by-frame paint

---

## 10. まとめ

この slice の主眼は「先に UI を作らない」ことにある。

- `Dope Sheet` は keyframe batch editing service として成立させる
- `Squash & Stretch` は transform bake service として成立させる
- `Bitmap Brush Engine` は raster stroke core として成立させる

これにより、後続 UI は:

- timeline に載せる
- dock に分ける
- paint layer と結びつける

のどれを選んでも、下層を作り直さずに進められる。
