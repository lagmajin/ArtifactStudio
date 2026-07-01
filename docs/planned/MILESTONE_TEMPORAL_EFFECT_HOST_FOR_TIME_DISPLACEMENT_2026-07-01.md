# Temporal Effect Host for Time Displacement (2026-07-01)

`Time Displacement` を `Layer Component` ではなく `Raster Effect` として実装するための、
effect host / context / temporal sampling contract の長期設計。

`Displacement Map` は既存の単フレーム `src -> dst` raster effect として成立するが、
`Time Displacement` は「別時刻のレイヤー結果を effect が参照する」必要があるため、
現状の effect host 契約をそのままでは使えない。

---

## Goal

- `Displacement Map` と `Time Displacement` をどちらも effect stack に置ける
- spatial effect と temporal effect を同じ effect host の枠内で扱える
- effect が composition / layer 実装を直接掘らず、host contract 経由で必要入力を要求できる
- editor preview / playback / RAM preview / render queue で同じ temporal contract を使える
- 将来の `Echo` / `Frame Blend` / `Posterize Time` / `Temporal Smear` / `Motion Trail` に流用できる

---

## Current Snapshot

2026-07-01 時点では、effect system に次の土台がある。

- `Artifact/include/Effects/ArtifactAbstractEffect.ixx`
  - `ArtifactAbstractEffect`
  - `EffectPipelineStage`
  - `applyConfigured(src, dst)`
- `Artifact/include/Effects/ArtifactEffectImplBase.ixx`
  - CPU / GPU impl base
- `Artifact/include/Effects/EffectContext.ixx`
  - `pDeviceContext`
  - `roi`
  - `isInteractive`
- `Artifact/include/Effects/EffectHostContract.ixx`
  - `EffectInputRequest`
  - `EffectCapabilityDescriptor`
  - `EffectDependencyDescriptor`
  - `LegacyEffectAdapter`
- `Artifact/src/Effects/DisplacementMap/DisplacementMapEffect.cppm`
  - 単フレーム raster effect として成立している

ただし現状の問題は次のとおり。

1. `EffectContext` に frame / fps / source sampler がない
2. `LegacyEffectAdapter::render()` は upstream の 1 枚だけを `applyConfigured()` に渡す
3. `EffectDependencyDescriptor` に temporal 語彙があるが、実行側で消費されていない
4. effect が「別時刻の source を要求する」正規ルートがない
5. temporal cache key が contract 化されていない

---

## Core Rule

今後は `Raster Effect` を次の 2 種に明確に分ける。

1. `Spatial Raster Effect`
   - 現フレームの入力画像だけで完結する
   - 例: `Displacement Map`, `Blur`, `Sharpen`, `Mosaic`, `Glow`

2. `Temporal Raster Effect`
   - 別フレーム、前フレーム、lookback buffer、あるいは source 再サンプルを要求する
   - 例: `Time Displacement`, `Echo`, `Frame Blend`, `Temporal Smear`

両者とも `EffectPipelineStage::Rasterizer` に属してよいが、
host contract 上の入力要求と cache 前提は区別する。

---

## Non-Goals

- `Time Displacement` を layer component として実装しない
- effect が layer / composition を dynamic_cast で直接掘る設計にしない
- `getThumbnailAtFrame()` をそのまま runtime temporal sampling API に昇格しない
- preview 用の ad-hoc 特例で temporal effect を実装しない

---

## Recommended Architecture

## A. `EffectContext` は runtime sampler を持つ

`Artifact/include/Effects/EffectContext.ixx` を、
単なる GPU context holder ではなく「effect 実行時の runtime context」に拡張する。

最低限ほしい情報:

- `compositionFrame`
- `layerFrame`
- `frameRate`
- `timeSeconds`
- `renderPurpose`
- `resolutionScale`
- `IEffectFrameSampler* sampler`

ここで重要なのは、effect に `ArtifactAbstractComposition*` や
`ArtifactAbstractLayer*` を直接渡すことではなく、
`sampler interface` を渡すこと。

候補:

```cpp
class IEffectFrameSampler {
public:
    virtual ~IEffectFrameSampler() = default;

    virtual bool sampleCurrentLayerFrame(
        std::int64_t compositionFrame,
        ArtifactCore::ImageF32x4RGBAWithCache& out) = 0;

    virtual bool sampleCurrentLayerFrameRelative(
        std::int64_t frameOffset,
        ArtifactCore::ImageF32x4RGBAWithCache& out) = 0;

    virtual bool sampleNamedInput(
        const QString& inputId,
        std::int64_t compositionFrame,
        ArtifactCore::ImageF32x4RGBAWithCache& out) = 0;
};
```

`Time Displacement` はこの sampler に対して
「現在ピクセルの displacement 値から計算した target frame を要求する」。

---

## B. `EffectInputRequest` を temporal-aware にする

現状の `EffectInputRequest` は `requiresPreviousFrame` と `temporalLookback` が中心で、
`Time Displacement` に必要な「任意フレーム参照」を表現しきれない。

追加したい語彙:

- `sourceId`
- `sourceKind`
- `sampleMode`
- `relativeFrameOffset`
- `absoluteFrame`
- `allowPerPixelTemporalSampling`
- `requiresFrameCache`

例:

```cpp
enum class EffectSourceKind {
    UpstreamColor,
    OriginalLayerSource,
    PreviousStageColor,
    AuxiliaryMap,
    TemporalSample
};

enum class EffectTemporalSampleMode {
    None,
    PreviousFrame,
    RelativeFrameOffset,
    AbsoluteFrame,
    PerPixelDisplacedFrame
};
```

`Displacement Map` は `UpstreamColor + AuxiliaryMap` 程度で済むが、
`Time Displacement` は
`UpstreamColor + TemporalSample(PerPixelDisplacedFrame)`
を要求する。

---

## C. `LegacyEffectAdapter` の次段として `EffectInputBundle` を導入する

今の `ArtifactAbstractEffect::applyConfigured(src, dst)` は、
single input の spatial effect には十分だが、
temporal effect には不足する。

長期的には次の bundle へ寄せる。

```cpp
struct EffectInputSurface {
    QString inputId;
    ArtifactCore::ImageF32x4RGBAWithCache* image = nullptr;
    QRectF roi;
    std::int64_t compositionFrame = 0;
};

struct EffectInputBundle {
    std::vector<EffectInputSurface> surfaces;
};
```

そして新しい effect base か host adapter 側に、

```cpp
virtual void renderBundle(
    const EffectContext& context,
    const EffectInputBundle& inputs,
    ArtifactCore::ImageF32x4RGBAWithCache& dst);
```

を持たせる。

短期互換のため、

- legacy effect は `applyConfigured(src, dst)` 維持
- host 側が `EffectInputBundle` から primary input を抜いて legacy path へ流す

とする。

---

## D. Temporal sampling は host 側で解決する

effect 自身が frame acquisition や cache 管理を持たない。

責務分担:

- effect:
  - dependency descriptor を返す
  - temporal parameter を解釈する
  - bundle から必要画像を読む

- host:
  - dependency を解決する
  - frame cache を引く
  - sampler を使って必要 frame を収集する
  - output surface を用意する

この分離を守ると、

- offline render
- interactive preview
- RAM preview
- future network render

で同じ effect 実装を再利用できる。

---

## E. Temporal cache key を contract 化する

`Time Displacement` は複数の frame を頻繁に引くため、
cache key が曖昧だと preview と render の再現性が崩れる。

最低限必要な key:

- `layerId`
- `compositionFrame`
- `resolution`
- `renderPurpose`
- `variant / time remap state`
- `upstream signature`

推奨:

```cpp
struct EffectFrameCacheKey {
    QString layerId;
    std::int64_t compositionFrame = 0;
    QSize pixelSize;
    RenderPurpose purpose = RenderPurpose::EditorInteractive;
    std::uint64_t sourceSignature = 0;
};
```

`Time Displacement` は frame lookback を大量に引くので、
host 側 cache がないと実用速度に届きにくい。

---

## F. Temporal effect は capability で明示する

`EffectCapabilityDescriptor` に次を追加する。

- `supportsTemporalProcessing`
- `supportsPerPixelTemporalSampling`
- `requiresDeterministicFrameCache`

`Time Displacement` は少なくとも:

- `supportsTemporalProcessing = true`
- `supportsPerPixelTemporalSampling = true`

が必要。

---

## Time Displacement Placement

`Time Displacement` は `Rasterizer` stage に置く。

理由:

- geometry / layer transform ではない
- instance topology も変えない
- 最終的に「画像サンプルをどの時刻から引くか」を変える raster effect だから

ただし host 契約上は `Temporal Raster Effect` として扱う。

---

## Displacement Map Placement

`Displacement Map` は現在どおり `Rasterizer` stage の
`Spatial Raster Effect` として扱う。

これは temporal host 拡張の最初の利用者ではなく、
既存 spatial path の互換検証対象とする。

検証項目:

- bundle host 導入後も既存 `Displacement Map` が regression しない
- `primary input + optional auxiliary map` で表現できる

---

## Suggested Runtime Flow

1. render pipeline が effect stack を評価順に並べる
2. 各 effect の `capabilities()` / `describeDependencies()` を読む
3. host が `EffectInputBundle` を構成する
4. spatial effect は current frame input をそのまま処理
5. temporal effect は sampler / frame cache から追加 frame を収集
6. effect が bundle を読んで output を書く
7. cacheable result は `(layerId, frame, purpose, resolution, signature)` で保存

---

## File-Level Impact

最小で触る候補:

- `Artifact/include/Effects/EffectContext.ixx`
  - frame / fps / sampler 追加
- `Artifact/include/Effects/EffectHostContract.ixx`
  - temporal request 拡張
  - `EffectInputBundle` 追加
  - legacy adapter 更新
- `Artifact/include/Effects/ArtifactAbstractEffect.ixx`
  - bundle-aware render hook を追加するか検討
- `Artifact/src/Effects/ArtifactAbstractEffect.cppm`
  - bundle / legacy 両対応の dispatch
- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`
  - effect host への context / bundle 供給
- `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm`
  - preview 用 temporal cache 接続
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - render / playback 経路の host 接続

`Time Displacement` 実体の候補:

- `Artifact/include/Effects/Time/TimeDisplacementEffect.ixx`
- `Artifact/src/Effects/Time/TimeDisplacementEffect.cppm`

---

## Proposed Phases

### Phase 1: Contract Extension

- `EffectContext` に frame / fps / sampler を追加
- `EffectInputRequest` に temporal sample mode を追加
- `EffectCapabilityDescriptor` に temporal capability を追加

完了条件:

- spatial effect を壊さず contract が拡張される

### Phase 2: Host Bundle

- `EffectInputBundle` を導入
- `LegacyEffectAdapter` を bundle aware にする
- render / preview で bundle path を通せるようにする

完了条件:

- existing raster effect が新 host 経由で動く

### Phase 3: Frame Sampler and Cache

- `IEffectFrameSampler` 導入
- frame cache key を固定
- preview / playback / render で共通の sampling entry を持つ

完了条件:

- host が別フレーム画像を effect 用に収集できる

### Phase 4: Time Displacement CPU MVP

- CPU 限定で `Time Displacement` を実装
- per-pixel displaced frame sampling を host contract 上で実現

完了条件:

- effect stack から `Time Displacement` を追加できる
- preview と playback で同じ結果が出る

### Phase 5: Optimization and GPU

- temporal cache hit rate 改善
- bounded lookback policy
- GPU / compute path の検討

完了条件:

- practical playback speed に近づく

---

## Guardrails

- `Time Displacement` を layer component に逃がさない
- effect が composition / layer 実体を dynamic_cast で掘らない
- `getThumbnailAtFrame()` を runtime host API の代用にしない
- temporal effect の cache key に `frame / purpose / resolution` を必ず入れる
- preview 専用の特例 path を authoritative host から分岐させすぎない

---

## Immediate Recommendation

いま次に着手するなら、順番はこれが安全。

1. `EffectContext.ixx` を temporal-aware にする
2. `EffectHostContract.ixx` に `EffectInputBundle` と `IEffectFrameSampler` を追加
3. `LegacyEffectAdapter` を bundle aware にする
4. 既存 `Displacement Map` を新 host で regression check
5. その後 `Time Displacement` CPU MVP を追加

---

## Related Files

- `Artifact/include/Effects/ArtifactAbstractEffect.ixx`
- `Artifact/src/Effects/ArtifactAbstractEffect.cppm`
- `Artifact/include/Effects/EffectContext.ixx`
- `Artifact/include/Effects/EffectHostContract.ixx`
- `Artifact/src/Effects/DisplacementMap/DisplacementMapEffect.cppm`
- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`
- `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `docs/EFFECT_SYSTEM_SPECIFICATION.md`
- `docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`
