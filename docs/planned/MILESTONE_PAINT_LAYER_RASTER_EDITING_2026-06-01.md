> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_PAINT_LAYER_2026-06-16.md](MILESTONE_PAINT_LAYER_2026-06-16.md)

# マイルストーン: Paint Layer / Raster Editing Foundation

> 2026-06-01 作成
> Status: Planned

## 目的

`paintLayer` を、Photoshop の「普通に描けるピクセルレイヤー」に近い体験として導入する場合の設計土台を整理する。

このマイルストーンは、いきなり Photoshop 全機能を再現することではなく、

- composition に置ける
- brush / eraser で直接描ける
- timeline / inspector / undo と自然につながる
- software / Diligent の両方で破綻しにくい
- `QImage` を本流に増やさず、明示的な変換境界を守る

ところまでを対象にする。

## 背景

現状の Artifact では、

- `image layer` は既存の source asset を置く責務が強い
- `brush / clone stamp / eraser` は tool ownership の候補として整理されている
- renderer 側は `QImage` 依存を減らし、`ImageF32x4_RGBA` などの内部表現へ寄せたい

という前提がある。

そのため `paintLayer` は、

- 既存 image layer に雑に paint 機能を足す
- viewport の一時 overlay と永続ピクセル編集を混ぜる
- Qt widget paint と compositor paint を同一責務にする

といった形では入れない方が安全。

## 位置づけ

`paintLayer` は新しい tool ではなく、新しい layer type として扱う。

- `PaintLayer`
  - 永続ピクセル内容を持つレイヤー本体
- `BrushTool` / `EraserTool`
  - `PaintLayer` に stroke を適用する入力手段
- `Mask`
  - paint 内容そのものとは分けて扱う
- `Overlay.Composition`
  - ブラシカーソル、プレビュー円、stroke preview などの一時表示

つまり「描く対象」と「描く道具」と「一時表示」を分離する。

## 非目標

- clone stamp / heal / liquify まで最初から入れること
- PSD 完全互換の pixel layer stack を一気に再現すること
- adjustment layer / smart object / filter gallery を同時に設計すること
- 既存 `image layer` をそのまま `paintLayer` に置き換えること
- `QColorDialog` や QtCSS を足して見た目だけ先に作ること

## 原則

1. `paintLayer` は `image layer` の別名にしない
2. 永続編集対象は project-owned buffer で持つ
3. `QImage` は import/export と Qt 境界の明示変換に限る
4. viewport overlay と pixel commit は別フェーズで扱う
5. 新規 signal/slot を増やさず、既存 command / service / event route に寄せる
6. Diligent / DX12 低レベルへは直接広く触らず、まず renderer 境界で閉じる

## 想定モデル

### Layer model

- `PaintLayer`
  - name / visibility / opacity / blend / transform / time range を持つ
  - paint content buffer を持つ
  - source asset 必須ではない
  - 必要なら初期化時に blank / imported image から生成できる

### Buffer model

- canonical buffer:
  - `ImageF32x4_RGBA` または同等の内部表現
- optional cache:
  - preview 用 GPU texture
  - undo 用 stroke delta
- compatibility boundary:
  - `QImage` への明示変換

### Editing model

- pointer drag 中:
  - overlay preview
  - stroke point 蓄積
- stroke commit 時:
  - paint op を buffer に適用
  - layer invalidate
  - undo command を記録

## ArtifactCore 型定義ドラフト

この節では、`ArtifactCore` に切り出す時の最小インターフェースを定義する。  
ここでの狙いは、実装を先に固定することではなく、`ImageLayer` と混ざらない責務境界を明文化すること。

### 設計判断

- `PaintLayer` は `ImageLayer` の派生ではなく独立型にする
- canonical storage は `ImageF32x4_RGBA` に固定する
- `StrokeSample` は通常の project save には含めない
- `PaintPreviewState` は永続化しない
- `Clone Stamp` と `Healing Brush` は同じ stroke 基盤を共有する
- `TrackedPoint` は初期版では必須にしない

### 低レベル型

```cpp
export enum class PaintToolKind {
    Brush,
    Eraser,
    CloneStamp,
    HealingBrush
};

export enum class PaintRepairMode {
    Clone,
    Heal,
    PatchBlend,
    TemporalHeal
};

export enum class SourceAnchorMode {
    FixedPoint,
    OffsetFromTarget,
    TrackedPoint
};

export struct StrokeSample {
    QPointF position;
    float pressure = 1.0f;
    float tiltX = 0.0f;
    float tiltY = 0.0f;
    float velocity = 0.0f;
    double timestamp = 0.0;
    bool isFirst = false;
    bool isLast = false;
};

export struct SourceAnchor {
    SourceAnchorMode mode = SourceAnchorMode::FixedPoint;
    QPointF sourcePosition;
    QPointF targetPosition;
    QPointF offset;
    int sourceFrame = -1;
    float confidence = 1.0f;
    bool locked = false;
};

export struct RepairOperation {
    PaintRepairMode mode = PaintRepairMode::Clone;
    float sampleRadius = 24.0f;
    float blendStrength = 1.0f;
    float colorMatchStrength = 0.0f;
    float edgeFeather = 0.5f;
    float patchSearchRadius = 48.0f;
    float seamPenalty = 0.0f;
    float temporalWeight = 0.0f;
};

export struct PaintStroke {
    QUuid strokeId;
    PaintToolKind toolKind = PaintToolKind::Brush;
    QVector<StrokeSample> samples;
    std::optional<SourceAnchor> sourceAnchor;
    std::optional<RepairOperation> repair;
    float radius = 24.0f;
    float hardness = 1.0f;
    float flow = 1.0f;
    float opacity = 1.0f;
    double startTime = 0.0;
    double endTime = 0.0;
    QRectF affectedRect;
};

export struct PaintLayerSourceRef {
    enum class Kind {
        Empty,
        ImportedImage,
        ImportedVideoFrame,
        DuplicatedLayer
    };

    Kind kind = Kind::Empty;
    QString sourceAssetId;
    QString sourceLayerId;
    int sourceFrame = -1;
};

export struct PaintLayerBufferDesc {
    QSize canvasSize;
    bool premultipliedAlpha = true;
    bool linearColorSpace = true;
};

export struct PaintPreviewState {
    QPointF cursorPosition;
    QPointF sourcePreviewPosition;
    float brushRadius = 24.0f;
    float previewOpacity = 1.0f;
    QRectF overlayBounds;
};

export struct PaintUndoRecord {
    QUuid undoId;
    QUuid strokeId;
    QRectF affectedRect;
    int layerRevisionBefore = 0;
    int layerRevisionAfter = 0;
};
```

### `PaintLayer` interface

```cpp
export class PaintLayer {
public:
    PaintLayer();
    explicit PaintLayer(const PaintLayerBufferDesc& desc);

    QUuid layerId() const;
    QString name() const;
    void setName(const QString& name);

    bool isVisible() const;
    void setVisible(bool visible);

    float opacity() const;
    void setOpacity(float opacity);

    BlendMode blendMode() const;
    void setBlendMode(BlendMode mode);

    Transform2D transform() const;
    void setTransform(const Transform2D& transform);

    TimeRange timeRange() const;
    void setTimeRange(const TimeRange& range);

    QSize canvasSize() const;
    void resizeCanvas(const QSize& size);

    const ImageF32x4_RGBA& contentBuffer() const;
    ImageF32x4_RGBA& contentBuffer();

    const PaintLayerSourceRef& sourceReference() const;
    void setSourceReference(const PaintLayerSourceRef& ref);

    int revision() const;
    QRectF dirtyRect() const;
    void clearDirtyRect();

    void appendStroke(const PaintStroke& stroke);
    const QVector<PaintStroke>& strokeHistory() const;

    void applyStroke(const PaintStroke& stroke);
    PaintUndoRecord captureUndoForStroke(const PaintStroke& stroke) const;
    void restoreFromUndo(const PaintUndoRecord& record);

private:
    QUuid id_;
    QString name_;
    bool visible_ = true;
    float opacity_ = 1.0f;
    BlendMode blendMode_ = BlendMode::Normal;
    Transform2D transform_;
    TimeRange timeRange_;
    PaintLayerBufferDesc bufferDesc_;
    ImageF32x4_RGBA contentBuffer_;
    PaintLayerSourceRef sourceRef_;
    QVector<PaintStroke> strokeHistory_;
    QRectF dirtyRect_;
    int revision_ = 0;
};
```

### 保存方針

- `PaintLayer` の永続化対象
  - `name`
  - `visible`
  - `opacity`
  - `blendMode`
  - `transform`
  - `timeRange`
  - `canvasSize`
  - `contentBuffer`
  - `sourceReference`
  - `strokeHistory` は原則メタ情報のみ
- `PaintLayer` の非永続対象
  - `PaintPreviewState`
  - hover 中の cursor 情報
  - drag 中の未確定 path
  - 完全な raw sample stream

### Undo 方針

- undo の最小単位は 1 stroke
- `PaintUndoRecord` は buffer 全コピーを常に持たず、まずは affected rect と revision を持つ
- 差分保存が難しい場合は checkpoint snapshot を別経路で積む
- preview 中は undo を積まない

### 初期版の最小到達点

1. `PaintLayer` を composition に追加できる
2. `Brush` と `Eraser` が同じ stroke path を使える
3. `CloneStamp` が `SourceAnchor` を使って動く
4. `Heal` は `RepairOperation` の mode だけ先に確保する
5. project save/load で `contentBuffer` が失われない

## Serialization Draft

`PaintLayer` は、layer metadata と pixel payload を分けて保存する。  
この分離により、将来 `StrokeSample` を保存するかどうかを切り替えても、基本の project format を壊さずに済む。

### 保存単位

- metadata
  - layer ID
  - display name
  - transform / opacity / blend / visibility
  - source reference
  - buffer description
  - revision / dirty tracking
- payload
  - canonical `ImageF32x4_RGBA`
  - optional thumbnail or preview cache
- history
  - default は stroke metadata のみ
  - raw `StrokeSample` は非推奨

### JSON Draft

```json
{
  "type": "paint",
  "id": "6f0db9d2-4c84-4eb0-a6cf-2f4d38f4f9d0",
  "name": "Retouch 01",
  "visible": true,
  "opacity": 1.0,
  "blendMode": "normal",
  "transform": {
    "tx": 0.0,
    "ty": 0.0,
    "rotation": 0.0,
    "scaleX": 1.0,
    "scaleY": 1.0
  },
  "timeRange": {
    "start": 0.0,
    "end": 0.0
  },
  "canvasSize": {
    "width": 1920,
    "height": 1080
  },
  "sourceReference": {
    "kind": "ImportedImage",
    "sourceAssetId": "asset-1234",
    "sourceLayerId": "",
    "sourceFrame": -1
  },
  "buffer": {
    "encoding": "rgba32f",
    "linearColorSpace": true,
    "premultipliedAlpha": true
  },
  "revision": 12,
  "dirtyRect": {
    "x": 128.0,
    "y": 64.0,
    "width": 240.0,
    "height": 180.0
  },
  "strokes": [
    {
      "strokeId": "c7a6a9d0-8c49-4db2-b5ef-3d2f1b1c6d61",
      "toolKind": "CloneStamp",
      "radius": 24.0,
      "hardness": 1.0,
      "flow": 1.0,
      "opacity": 1.0,
      "repair": {
        "mode": "Clone",
        "sampleRadius": 24.0,
        "blendStrength": 1.0,
        "colorMatchStrength": 0.0,
        "edgeFeather": 0.5,
        "patchSearchRadius": 48.0,
        "seamPenalty": 0.0,
        "temporalWeight": 0.0
      }
    }
  ]
}
```

### 保存ルール

- `contentBuffer` は project 内包を基本にする
- 外部ファイル退避をする場合は、`sourceReference` と別に payload path を持つ
- undo snapshot は常時保存しない
- `PaintPreviewState` は保存しない
- history は stroke metadata のみに抑える

### `ImageF32x4_RGBA` 境界

- import 時
  - `QImage` / `cv::Mat` / file path から `ImageF32x4_RGBA` に明示変換する
- export 時
  - `ImageF32x4_RGBA` から `QImage` / `cv::Mat` / file path へ明示変換する
- hot path
  - `QImage` を見ない
  - implicit conversion を置かない

## Suggested File Split

`ArtifactCore` に落とす場合の、分割候補は次の通り。

### Core module

- `ArtifactCore/include/Paint/PaintLayer.ixx`
  - `PaintLayer` 本体
  - buffer desc
  - source ref
  - undo record

- `ArtifactCore/include/Paint/PaintStroke.ixx`
  - `PaintToolKind`
  - `StrokeSample`
  - `PaintStroke`
  - `SourceAnchor`
  - `RepairOperation`

- `ArtifactCore/src/Paint/PaintLayer.cppm`
  - apply / restore / dirty tracking
  - serialization helpers

- `ArtifactCore/src/Paint/PaintStroke.cppm`
  - sample normalization
  - stroke packing helpers

### App integration

- `Artifact/include/Layer/ArtifactPaintLayer.ixx`
  - `PaintLayer` の app 側ラッパー
  - composition への配置

- `Artifact/src/Layer/ArtifactPaintLayer.cppm`
  - renderer 連携
  - current buffer 供給

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - overlay / stroke commit routing

- `Artifact/src/Widgets/ArtifactToolOptionsBar.cppm`
  - brush / clone / heal / eraser のパラメータ surface

### 実装順

1. `PaintStroke` / `SourceAnchor` / `RepairOperation`
2. `PaintLayer` の buffer ownership
3. JSON save/load
4. `CloneStamp` の適用
5. `HealingBrush` の mode 追加
6. overlay preview
7. undo snapshot 最適化

## フェーズ

### Phase 1: Boundary Audit

#### 目的

既存の image layer / tool / renderer / undo 経路のどこに `paintLayer` を差し込むべきかを確定する。

#### 作業項目

- `image layer` の current responsibility を棚卸しする
- composition viewport の tool routing を確認する
- overlay 描画と永続描画の境界を確認する
- `QImage` が hot path に残っている箇所を洗い出す
- undo に乗せられる最小単位を決める

#### 完了条件

- `PaintLayer` の責務が `image layer` と混ざらず説明できる
- stroke preview と stroke commit の境界が明文化される
- 最初に触るべきファイル群が特定できる

### Phase 2: PaintLayer Core Representation

#### 目的

`PaintLayer` を layer system に追加するための最小モデルを用意する。

#### 作業項目

- 新しい layer type を定義する
- blank layer 作成 API を用意する
- pixel buffer ownership を layer に持たせる
- bounds / width / height / resolution policy を定義する
- persistence の保存形式を決める

#### 完了条件

- empty `PaintLayer` を composition に追加できる設計になっている
- layer save/load で pixel content の保存方針が定義される
- source なし layer と imported-from-image layer の両方を扱える

### Phase 3: Brush Stroke Pipeline

#### 目的

ブラシ入力から buffer 更新までの最小編集経路を成立させる。

#### 作業項目

- `BrushTool` の ownership を app/tool 側で固定する
- stroke sample 収集モデルを決める
- brush dab 合成ルールを定義する
- opacity / flow / hardness の最小セットを決める
- `EraserTool` を「別レイヤー種別」ではなく別 apply mode として扱う

#### 完了条件

- 1本の stroke を `PaintLayer` に反映できる
- brush と eraser が同じ stroke engine を共有できる
- pointer move 中に毎回 full recomposite を強制しない方針がある

### Phase 4: Composition Editor Integration

#### 目的

composition editor 上で `PaintLayer` を違和感なく編集できる入口を作る。

#### 作業項目

- brush cursor / radius preview の overlay 表示
- active tool と active layer の条件整理
- non-paint layer 選択時の fallback behavior 定義
- pan/zoom 中の stroke accidental input を防ぐ
- tablet pressure が無い場合の挙動を決める

#### 完了条件

- viewport 上で paint mode に入れる
- overlay 表示が renderer 境界で完結する
- transform 操作と paint 操作のモード衝突が整理される

### Phase 5: Render Path and Cache

#### 目的

`PaintLayer` の表示と invalidation を既存 compositor に安全に繋ぐ。

#### 作業項目

- buffer から preview texture への upload 境界を決める
- stroke 中の dirty rect 更新方針を定める
- software fallback の描画経路を定義する
- Diligent 側には texture consumer として渡す
- full layer reupload を避ける余地を設計に残す

#### 完了条件

- `PaintLayer` が既存 blend / opacity / transform と一緒に描画される
- stroke 1回ごとの invalidate 粒度が説明できる
- software と Diligent の責務差が整理される

### Phase 6: Undo / Persistence / Assetization

#### 目的

描いた内容を作業として成立させる。

#### 作業項目

- stroke 単位 undo を導入する
- autosave / project save に pixel content を載せる
- external file と internal embedded data の方針を決める
- thumbnail / layer preview との接続を整理する
- imported image から `PaintLayer` を作る導線を定義する

#### 完了条件

- 1 stroke = 1 undo の最小体験が成立する
- プロジェクト再読込後に paint 内容が失われない
- asset browser との関係が破綻しない

### Phase 7: Expansion Track

#### 目的

初期版の先に広げる余地を整理する。

#### 候補

- clone stamp
- selection-aware paint
- layer mask 連携
- symmetry / spacing / jitter
- filter brush
- per-stroke blend mode
- tile / wrap canvas

#### 完了条件

- 初期版を壊さずに拡張できる責務分離がある

## UI 方針

- `PaintLayer` 作成は menu / toolbar command から入る
- 色選択は `FloatColorPicker` または既存承認済み picker を使う
- ブラシ設定は Inspector か tool option surface に寄せる
- 新規 dock をいきなり増やさず、まず既存 surface に収める

## 技術上の注意

- `QImage` をレイヤー内部の canonical storage にしない
- 暗黙の CPU download / GPU upload をしない
- stroke 中の毎フレーム project-wide refresh を避ける
- Diligent backend の低レベル変更は renderer boundary の外へ広げない
- mask / matte / blend と paint の責務を最初から分離する

## 依存・関連

- [MILESTONE_PHOTOSHOP_LIKE_IMAGE_EDITING_2026-04-11.md](/X:/Dev/ArtifactStudio/docs/planned/MILESTONE_PHOTOSHOP_LIKE_IMAGE_EDITING_2026-04-11.md)
- [MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md](/X:/Dev/ArtifactStudio/docs/planned/MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md)
- [MILESTONE_VECTOR_LAYER_IMPORT_2026-03-25.md](/X:/Dev/ArtifactStudio/docs/planned/MILESTONE_VECTOR_LAYER_IMPORT_2026-03-25.md)
- [MILESTONE_ARTIFACT_IRENDER_2026-03-12.md](/X:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md)
- [MILESTONE_OIIO_IMAGE_PIPELINE_MIGRATION_2026-03-30.md](/X:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_OIIO_IMAGE_PIPELINE_MIGRATION_2026-03-30.md)
- [MILESTONE_VIDEO_QIMAGE_RETIREMENT_2026-04-15.md](/X:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_VIDEO_QIMAGE_RETIREMENT_2026-04-15.md)

## 最初にやるなら

1. `Phase 1` の棚卸しで責務境界を確定する
2. blank `PaintLayer` を layer type としてだけ先に通す
3. hard round brush + eraser の 2 つに絞って stroke engine を作る
4. undo と save が成立してから clone stamp 以降へ進む

## Minimal CloneStamp API

`CloneStamp` を先に実装する場合、最初に必要なのは「ストロークを受け取る」「ソースを指定する」「buffer に焼く」の 3 点だけ。  
Healing や patch search はこの段階では入れない。

### 要件

- source は明示指定できる
- source が未設定なら apply を拒否する
- brush radius / opacity / hardness だけで動く
- sample path は stroke として保持する
- target layer への commit は 1 stroke 単位
- preview と commit を分ける

### 最小 public API

```cpp
export class PaintLayer;

export class CloneStampSession {
public:
    CloneStampSession();

    void setTargetLayer(PaintLayer* layer);
    PaintLayer* targetLayer() const;

    void beginStroke(const QPointF& targetPos,
                     const SourceAnchor& anchor,
                     float radius,
                     float opacity,
                     float hardness);

    void appendSample(const StrokeSample& sample);
    void updateSourceAnchor(const SourceAnchor& anchor);

    bool canCommit() const;
    PaintUndoRecord commit();
    void cancel();

    bool isActive() const;
    PaintPreviewState previewState() const;

private:
    PaintLayer* targetLayer_ = nullptr;
    std::optional<PaintStroke> currentStroke_;
    PaintPreviewState previewState_;
};
```

### 最小 apply contract

`CloneStampSession::commit()` は次を保証する。

- `currentStroke_` が存在しない場合は失敗扱い
- `targetLayer_` が null の場合は失敗扱い
- `SourceAnchor` が無効なら失敗扱い
- 成功時は `PaintLayer::applyStroke()` を呼ぶ
- 戻り値として `PaintUndoRecord` を返す
- commit 後は session を idle に戻す

### 失敗条件

- source anchor の mode が未定義
- source position が canvas 外
- target layer が null
- stroke sample が 0 個
- layer canvas size と stroke 座標系が一致しない

### Preview contract

preview は commit とは別に扱う。

- cursor 移動中は `previewState_` だけ更新
- `sourcePreviewPosition` はあくまで表示用
- `overlayBounds` は dirty rect ではない
- preview で buffer を変更しない

### 実装順

1. `CloneStampSession` の状態遷移だけ作る
2. `PaintLayer` に `applyStroke()` の空実装を置く
3. hard round brush の dab 合成を入れる
4. source offset のコピーを入れる
5. undo record を返す
6. preview overlay を接続する

## 成功条件

- `paintLayer` を「ただの image layer 改造版」として扱わずに済む
- viewport tool と layer persistence が自然につながる
- 既存 render path を壊さずに最小の pixel editing を追加できる
- 将来の Photoshop-like expansion に耐える境界が残る

## 2026-07-25 現状確認

Paint Layer の基礎実装は別途進んでおり、`ArtifactPaintLayer` が composition に置ける独立 layer type として存在する。永続バッファは `ImageF32x4_RGBA`、フレーム別管理、ブラシ／消しゴム stroke、直近stroke undo、dirty通知、JSONプロパティ保存を備える。`ArtifactBrushTool`、ブラシ／消しゴムのツールバー・オプションUI、Onion Skin overlay も接続されている。

未完了または未検証なのは、Timeline／InspectorでのPaint Layer専用編集、strokeのUndoManager／プロジェクト履歴統合、pressure／tilt等の入力情報、GPU texture cache更新、複数フレームの保存復元、ソフトウェア／Diligent両経路の性能と表示一致である。clone／heal等の高度修復ツールは本マイルストーンの範囲外。判定は「Raster Editing Foundationは実装済み、製品導線と統合検証が残る」とする。
