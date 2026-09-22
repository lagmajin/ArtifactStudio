# Motion Modulation Phase 0 — 現行基盤監査と評価契約

**最終更新:** 2026-09-22

## 1. 監査結果（既存実装の正とする点）

### 1.1 Property / Animation

- 正規基盤は `Property.Abstract` (`ArtifactCore/include/Property/AbstractProperty.ixx`, 実装 `ArtifactCore/src/Property/AbstractProperty.cppm`)。
  - `EnvelopeTrack` / `EnvelopePreset` は Float/Integer のみ。`evaluateValue()` の順序は `externalOverride → interpolateValue → envelopes → expression → ModulationRouter::targetValue(targetId, base)`。
  - Router 呼び出しは caller が `router + stablePath` を明示的に渡した時のみ。Router は property を mutate しない。
- `Animation.Value` (`ArtifactCore/include/Animation/AnimatableValue.ixx`): `AnimatableValueT::at()` は整数 frame クランプ＋`lower_bound`＋線形 t。`AnimationLayerStackT` は Add/Override のみ。`toJson/fromJson` は float のみ。
- `AnimatableTransform3D` (`Animation.Transform3D`): チャンネル property は `persistentLayerProperty()` で共有インスタンスをキャッシュ。正規パスは `transform.position.x/y/z`, `transform.rotation(.x/y/z)`, `transform.scale.x/y/z`, `transform.anchor.x/y/z`, `layer.opacity` (0-1)。
- 評価順（`ArtifactAbstractLayer::opacity()` 約10829行付近）: `impl_->opacity_ → layer.opacity keyframe → animationLayers_.evaluateWithBase() → modulationRouter_.processAtFrame() + targetValue() → LayerEffectEnvelope → clamp 0-1`。
- `getLayerPropertyGroups()` は約25の override あり。Timeline 左ペイン標準は `Transform` のみ、PropertyWidget 通常表示は Components/Cloner 等を露出させない（AGENTS.md 制約を維持）。
- Color は base layer に存在しない。`component.collision.displayColor:Color`、3D 系 `baseColor/emissionColor` のみ。`QColor↔FloatColor` 変換は読み取り境界のみ。

### 1.2 既存 Modulation（2026-08-29 マイルストーン実装済み）

- 正規 Core は `Audio.Modulation.Router` + `Audio.Modulation.Modulator`（`ArtifactCore/include/Audio/Modulation/`）。
  - Source: `LfoSource` (Sine/Triangle/Saw/Square/SampleAndHold)、`AdsrSource`、`RandomSource` (XorShift32 + seed + rate + smoothing)、`MacroSource` (0-1、時間不進行)。
  - `ModulationAssignment::forPropertyPath()` が stable path + FNV-1a targetId を保持。Add/Multiply 合成可。重複追加は depth/enabled 更新。
  - `processAtFrame(frame, frameRate)` は同一 frame 冪等、逆行 seek は reset+replay。`sourceDefinitions()/restoreSources()` は設定のみ往復し runtime phase は捨てる。source ID 0 は予約。
  - 評価順は `base → keyframe/envelope/expression → mod Add → mod Multiply`。保存は `modulation.sources/assignments` (effect + layer JSON)。Undo は `Effect/LayerModulationSnapshotCommand` + `ArtifactEffectService::setEffectModulationSnapshot()` 経由のみ。
  - 現状の接続先は `layer.opacity` (`layer.<id>.layer.opacity`) と composition/layer effect (`effect.<id>.<prop>`) のみ。Transform/particle/fluid は未接続。
- 本マイルストーン（2026-09-22）はこの Router を置き換えない。Phase 1 の Constant/Noise/Steps/Math は Router の `IModulatorSource` 追加または Router 前段の純関数として足す。

### 1.3 Playback / Preview / RenderQueue

- Transport の唯一所有者は `ArtifactPlaybackService` (`Artifact/include/Service/ArtifactPlaybackService.ixx`)。UI から Composition/Engine/Controller への直接 Transport 操作は禁止。
- Preview (`ArtifactPreviewCompositionPipeline` + `ArtifactCompositionRenderWidget::renderOneFrame`): `setCurrentFrame → composition->goToFrame → layer->goToFrame + draw(renderer) + flush`。snapshot 構造体なし、共有 composition を直接進める。
- RenderQueue (`ArtifactRenderQueueService::FrameRenderSnapshot`): `toJson/fromJson` による isolated clone 上で `goToFrame + evaluateLayerComponentSimulation` し、GPU (`draw + readbackToImageF32/Image`) / CPU (`QPainter + SoftwareCompositor`) に分岐。
- `layer->goToFrame` は `currentFrame_` 設定 + `modulationRouter_.processAtFrame()` を行う。両経路ともこの点を共有しているため、変調の決定的再現の鍵は Router の frame 決定的動作にある。

### 1.4 Particle / Fluid 入力点

- Generic layer の Components 面は `component.particleEmitter.{count,speed,lifetime}` と `component.fluid.{grid,viscosity,diffusion,buoyancy,...}` の enabled トグルのみ公開。内部値は `setComponent*PropertyValue()` で範囲 clamp され `fluidRuntime_.invalidate*` を起こす。
- 専用 `ArtifactParticleLayer` は `particle.emitter.* / particle.particle.* / particle.simulation.* / particle.effectors.*` を持つが、既定で `setAnimatable(true)` なし（Timeline keyframe 対象外）。
- Timeline の keyframe 対象は `isAnimatable()` で絞られ、実質 `layer.opacity/transform.*` のみ。`particle.*`/`component.*` は force-enable hack を除き静的。
- Per-frame 供給: smoke は `setViscosity/Diffusion/...` 毎 frame 再設定＋`addDensity/addVelocity`、generic particle は count/speed/lifetime から spawn＋積分、専用 layer は `goToFrame(max(1,frame))` 決定的 forward sim。

## 2. 評価契約（Phase 0 固定）

### 2.1 評価単位

- 時間の唯一所有者は `ArtifactPlaybackService`。変調は `frame:int64` + `frameRate:float` を入力とする control-rate 評価とし、既存 `ModulationRouter::processAtFrame(frame, frameRate)` を正規 clock とする。
- Subframe（`RationalTime`）補間は `AbstractProperty::interpolateValue()` の責務に残す。変調は subframe 補間後の base 値へ Add/Multiply で重ねる。変調 source 自身は subframe 内挿しない（Phase 5/6 まで持ち越さない）。
- Seed 付き source（Noise/Random/Steps/Operators）は `eventID/cycle/frame + seed → XorShift32` の決定的導出とし、毎 frame の非決定的 RNG 呼び出しを禁止。`reset()` で seed へ戻り、backward seek は reset+replay。

### 2.2 値の型・範囲・補間

- Phase 1 の Router 契約は `float` のみ（既存 `targetValue(targetId, base:float)` を維持）。Vector/Color は Phase 1 では各チャンネルへ同一 binding を複数適用する形に留め、新規の汎用 Variant API は作らない（未解決事項の型表現は Phase 6 まで open）。
- 範囲 clamp は所有側の既存 min/max を適用後に維持する（`AbstractProperty::evaluateValue` の現行動作）。Modulation amount=0 は `targetSums=0, targetMultipliers=1` により base へ完全に戻ることを不変条件とする。
- 補間は既存 `InterpolationType`（`Math.Interpolate`）を再利用。新規カーブ種別は Phase 2 の `AutomationClip` 側で curvature として持つ。

### 2.3 Seed・品質段階

- Seed は `uint32_t`、0 は予約（既存 Router 規則を継承）。保存時は full uint32 を double で JSON 保存（既存規則）。
- 品質段階は Phase 0 では導入しない。GPU 非対応ノードの fallback 判定は Phase 6 の受入基準（明示的診断）へ先送り。

### 2.4 UI 表示値 vs レンダー評価値の分離

- UI（Property Editor/Timeline）は keyframe base 値を表示し、変調結果は「適用後プレビュー値」として読み取り専用表示に留める。UI からの書き込みは keyframe/base へのみ行い、変調適用後値への直接書き戻しを禁止。
- Render（Preview/RenderQueue）は `goToFrame()` 後の `targetValue()` 適用済み値のみを受け取る。レンダー中に Router の再解析・source 追加・assignment 変更を行わない。Audio Follower の解析結果も frame snapshot として渡す（Phase 4）。

## 3. GPU / ソフトウェア境界（文書化）

- 両経路とも `layer->goToFrame(frame)` で Router を同一 frame へ進めた後の評価済み値を使う。GPU は `ArtifactIRenderer::draw + readbackToImage(F32)`、CPU は `SoftwareCompositor` で合成するが、入力値は同一 snapshot。
- Interchange は `ImageF32x4_RGBA`。`QImage` 新規採用禁止、`QPainter` 新規合成禁止の既存規則を維持。変調由来の値は float/channel で渡し、画像変換を介さない。
- Phase 1 の CPU/GPU snapshot 一致条件（同一 Seed・同一 frame・同一入力で一致）は、`processAtFrame` の冪等性＋`MacroSource` の時間不進行性により担保する。Audio-rate `process(n)` と control-rate `processAtFrame` の混用はしない。

## 4. Phase 1 への接続方針（既存を壊さないため）

- `IModulatorSource` の追加のみで足りるもの（Constant/Noise/Steps）は `Modulator.ixx` に純粋追加し、`ModulatorSourceType` と `sourceDefinitions()/restoreSources()` の往復を拡張する。既存 LFO/ADSR/Random/Macro の ID・JSON schema を変更しない。
- Math/Remap/Clamp は source ではなく binding 後段の純関数（`amount/offset/range/blend`）として実装し、Router の Add/Multiply 順序を変えない。
- 初期 destination はマイルストーン通り Transform/Opacity/Color/Particle emission/Fluid strength に限定するが、実装順は Transform/Opacity 最小版から始め、Particle/Fluid は `setComponent*PropertyValue()` の clamp/invalidate 経路を経由させる（直接メンバ書き換え禁止）。
- 新規 signal/slot、QtCSS、`QColorDialog`、`QImage`、`QPainter` 合成の導入はなし。ホットパスは事前確保＋immutable snapshot 原則（`HOT_PATH_RULES.md`）に従う。

## 5. Phase 0 完了条件の照合

- [x] 変調評価は `processAtFrame + targetValue(targetId, base)` の snapshot 契約で任意パラメータから呼び出せる（Float/Integer は既実装、Transform 拡張は Phase 1）。
- [x] GPU/ソフトウェア経路が同じ評価済み値を受け取る境界を本書 §3 に文書化。
- [x] `ArtifactPlaybackService` を時間の唯一所有者として維持（§2.1）。
- [x] UI 表示値とレンダー評価値の分離を §2.4 に定義。

## 6. 次の作業（Phase 1 最小版）— 2026-09-22 実装済み

1. [x] `Modulator.ixx` へ `ConstantSource` / `NoiseSource(seed)` / `StepsSource` を `IModulatorSource` として追加（`ModulatorSourceType` 拡張＋往復対応）。LFO/ADSR/Random/Macro は既存のまま。
2. [x] binding 後段の `Math/Remap/Clamp` 純関数（`remapClamp`）＋`ModulationBinding{amount,offset,range,clamp}`＋`applyModulationBinding` を `Router.ixx` に追加。amount=0・offset=0 は base へ完全に戻る。
3. [x] `layer.opacity`（既存）に続き Transform 5ch（`transform.position.x/y`, `transform.rotation`, `transform.scale.x/y`）への `targetValue` 適用を `getLocalTransform()` へ拡張。順序は keyframe 評価→変調→dynamics→physics→layout。割当なし時は `empty()` ガードでホットパス無負荷。
4. [x] 既存 `AudioModulationRouterTest` のローカル Constant を公式 `ConstantSource` に置換し、往復・決定性・binding の回帰ケース追加。
5. [ ] ビルド・テストはユーザー明示指示後に実施（本ターンでは未実施）。

### Phase 1 実装メモ（既存互換）

- `ModulatorSourceType` は Lfo/Adsr/Random/Macro=0-3 を維持し Constant/Noise/Steps=4-6 を追加。effect/layer JSON・Undo 検証の型上限 `>3` → `>6` に更新し、`constantValue` / `stepCount` フィールド追加。旧プロジェクト（0-3）はそのまま読める。
- `NoiseSource` は smoothstep 補間の seeded noise、`StepsSource` は (seed,index) ハッシュの最大32ステップ。どちらも `reset()+replay` で決定的、`process()` 無確保。
- Transform 適用時の QString 構築は割当存在時のみ（bounded・小容量）。共通ケース（変調なし）は `empty()` で回避した旨をレビュー記録とする。
- UI（Property Editor の source 追加導線）は Phase 1 では変更なし。変調割当は既存の snapshot API・Undo 経路のまま利用する。

## 7. Phase 2 実装記録（2026-09-22）

所有者決定: **パターンは Composition 共有、インスタンスは Layer 所有**。Phase 3 の Alias（同一パターン複数参照）がそのまま載る形にした。`AutomationClipPattern`（points/interp/seed/安定id）と配置（offset/stretch/loop/timePolicy/weight）を分離した。

- Core 型は新規モジュールを作らず `Animation.Value`（`AnimatableValue.ixx`）へ追記。評価は無確保・ステートレスな純関数（`evaluateAutomationClipPattern` / `applyAutomationClipInstance` / `mapAutomationClipLoop`）。`curvature` は保存・往復のみで評価未適用（予約）。
- 評価対象は Phase 2 では float 6ch のみ（`isAutomationClipEvaluatedPath` で明示）。順序は keyframe→modulation→clips→dynamics（Transform）/ envelope（Opacity）。
- 保存: Composition JSON `automationClips`、Layer JSON `automationClipInstances`。id は uint32・0予約（source と同一規則）。
- Undo: `LayerAutomationClipInstancesCommand`（before/after＋生成パターン任意同梱、factory 登録済み）。
- 最小 UI 導線: Timeline カーブ More メニュー「Convert Selection to Automation Clip」（選択グループ毎にパターン化・非破壊・Undo 付き）、Property 行右クリック「Automation Clip」サブメニュー（割当・再利用・全解除、Undo 付き）。
- 未実施: ビルド・テスト実行（明示指示待ち）。`tests/ArtifactCore/AutomationClipTest.cpp` を新規登録済み（CMake のみ編集）。

## 8. 参照

- `docs/planned/MILESTONE_BITWIG_INSPIRED_MOTION_MODULATION_2026-09-22.md`
- `docs/planned/MILESTONE_PROPERTY_MODULATION_2026-08-29.md`
- `docs/technical/HOT_PATH_RULES.md`
- `ArtifactCore/include/Audio/Modulation/Router.ixx`, `Modulator.ixx`
- `ArtifactCore/include/Property/AbstractProperty.ixx`, `ArtifactCore/include/Animation/AnimatableValue.ixx`
