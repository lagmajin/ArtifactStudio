# MILESTONE: 3D空間オーディオレイヤー — 高性能設計

**最終更新:** 2026-08-23

## 1. 目的 / 非目的

目的: コンポジション内の3D空間に配置された音源を、アクティブカメラ＝リスナー基準でリアルタイム空間化し、既存 `Audio.Segment` / `Audio.Panner` / `Audio.Renderer` パイプラインにゼロコピーで統合する。将来のHRTF/オクルージョン拡張を性能劣化なしで差し込める土台とする。

非目的: 既存 `ArtifactAudioLayer` の2Dパン置換、DAW的ミキサー再設計、外部HRTFデータセットの同梱。

## 2. 制約（AGENTS.md準拠）

- Qt依存は `Artifact` 側のみ。`ArtifactCore` は純粋C++ + 既存 `Audio.*` のみ。
- `W_OBJECT` 追加時は `W_OBJECT_IMPL` 整合、`QApplication` 等は直接 `#include`。
- `module X;` purview に `#include` 禁止。`QImage` 新規禁止。`setStyleSheet` 禁止。
- C++20 modules の自己import・循環・`export import` 乱用を避ける。

## 3. モジュール構成

```
ArtifactCore/include/Audio/Spatial/
  SpatialParams.ixx          export module Audio.Spatial.Params
  SpatialMath.ixx            export module Audio.Spatial.Math       // 距離・コーン・ベクトル演算（inline, SIMD前提）
  SpatialRenderer.ixx/.cppm  export module Audio.Spatial.Renderer   // ブロック処理本体（オーディオスレッド）
  HrtfData.ixx               export module Audio.Spatial.HrtfData   // 将来: HRIRテーブル（Phase1ではスタブ）

Artifact/include/Layer/ArtifactSpatialAudioLayer.ixx
Artifact/src/Layer/ArtifactSpatialAudioLayer.cppm
Artifact/src/Audio/SpatialListenerResolver.cppm   // ActiveCamera解決をRenderControllerと共有
```

`Audio.Spatial.*` は `AUDIO_MODULES` に追加しない独立FILE_SETでも可。`Audio.Panner:12` の `PanningMode::Binaural` と `Audio.Segment:26` を再利用し、`Audio.Renderer:108` の `enqueue()` 直前で空間化する。

## 4. コア設計 — 高性能

### 4.1 スレッドモデル

```
[UI/Control Thread]  setSpatialParams() ──► atomic<ParamsSnapshot*> ──► [Audio Thread] processBlock()
                     ModulationRouter ──┘  (double-buffer + seqlock)      ▲ lock-free, wait-free read
```

- UIは `SpatialParams` をコピーして `publish()`。Audio側は `load()` でポインタをアトミック取得のみ。`std::shared_ptr` は使わず、2スロット + generationカウンタのseqlockでABA回避。確保済み `SpatialSnapshot` 2個を再利用し、publish時に非アクティブ側へ書き込み→ `store(release)`。
- `SpatialParams` はPOD（`float` 8個 + `uint32_t` 1個 + `bool` 2個 = 32Byte台、64Byteアライン）。`alignas(64)` でfalse sharing回避。

### 4.2 ブロック処理（Control-rate + Audio-rate分離）

- `processBlock(const AudioSegment& in, AudioSegment& out, int frames, float sampleRate, Vec3 srcPos, Vec3 listenerPos, Quat listenerRot)` は **ブロック先頭で1回だけ** 幾何計算:
  - `distance = length(src-listener)` — `rsqrt` 近似 + 1回Newton反復（誤差 <0.01%）
  - `azimuth/elevation = toSpherical( invListenerRot * (src-listener) )`
  - `attenuation = distanceAttenuation(distance, params)` — 分岐なしテーブル補間（Inverse/Linear/Exponentialを統一式 `pow` のLUT 256点 + 線形補間）
  - `coneGain = lerp(1, outerGain, smoothstep(inner, outer, angle))`
  - `gain = attenuation * coneGain`
- 続く `frames` サンプルは `gain` と `PanningGain` を **線形ランプ** で適用（ジッパーノイズ防止、1ブロックで `gainPrev → gainCurr` を `1/frames` 刻み）。ランプ係数は `1/frames` を事前計算し `fma` で畳み込み。
- パン適用は `AudioPanner::calculateGain(azimuth,elevation)` → `PanningGain{vector<float>}` を毎ブロック1回取得し、`applyPanning` 相当を **SoA + SIMD** でインライン展開（下記）。

### 4.3 SIMD / キャッシュ最適化

- `AudioSegment:30` は `QVector<QVector<float>>`（AoS）。ブロック処理内では生ポインタ `float *L = out.channelData[0].data()` を取得し、16Byteアラインを `assume_aligned<16>`。残りチャンネルも同様。
- ステレオ2chのゲイン適用は SSE/AVX で `L[i] *= gainL_ramped[i]` を8サンプル単位で処理。`gainL_ramped` はスカラーの線形補間だが、ベクトルレジスタに `gainBase + i*step` を `set_ps` で展開しストア。`QVector` の暗黙共有コピーは `processBlock` 入口で `detach` 済みを前提（呼び出し側で `out = in` 後に `out.channelData[i].data()` がユニークであることを `isDetached()` でassert）。
- 分岐除去: `coneInner==360` の全指向性は `coneGain` 計算を `if (inner>=360) coneGain=1` の1分岐でスキップ。以降のサンプルループには分岐を持ち込まない。
- キャッシュ: `SpatialSnapshot` は64Byte、`PanningGain` は最大8ch×4Byte=32Byte。ブロック128～512サンプル（512×4×2=4KB）がL1に収まる。

### 4.4 アロケーションゼロ保証

- `processBlock` 内で `std::vector` / `QString` / `QVector::resize` 禁止。`out` は呼び出し側が `setFrameCount(frames)` 済み。
- `PanningGain` の `vector<float>` は毎ブロック確保するとNG。`SpatialRenderer` 内に `alignas(16) float gainBuf_[8]` を保持し、`span<float>` で返す内部API `calcGainsFast()` を追加（既存 `AudioPanner` は互換維持のため残す）。
- ドップラー（Phase2）はリサンプラのリングバッファを `SpatialRenderer` が `2*maxBlockSize` で事前確保。Phase1では `doppler=false` 固定でコストゼロ。

### 4.5 HRTF拡張のフック（性能劣化なし）

- `PanningMode::Binaural` 選択時のみ `HrtfData` のHRIR畳み込み（partitioned convolution, uniform 128tap, FFT 256点）を有効化。Phase1では `StereoBalance/EqualPower` のみでHRTF分岐はコンパイル時 `if constexpr` で除去。
- HRIRテーブルは `HrtfData.ixx` に `constexpr float kHrir[azimuthSteps][2][128]` として埋め込み、初回ロードで `mmap` 的に確保。畳み込みは `SpatialRenderer` の別エントリ `processHrtfBlock()` に分離し、通常パスの命令キャッシュを汚さない。

## 5. レイヤー設計

`ArtifactSpatialAudioLayer : ArtifactAbstractLayer` — `ArtifactAudioLayer:51` と同等の `Impl*` Pimpl（`Impl*` 生ポインタ、`delete` はcppm側）。`is3D=true`, `hasAudio()=true`, `shouldIncludeInFinalRender()=false`, `draw()` は `is3D` ギズモのみ。

```
Impl {
  SpatialParams spatial_;
  SpatialSnapshot snapshots_[2]; atomic<uint64_t> seq_;
  QString sourcePath_; // ArtifactAudioLayerと同様にAudioSegmentソースを保持
  AudioSegment cachedSegment_;
}
```

Transformは `ArtifactAbstractLayer` の3D Transformを流用（`getGlobalTransform4x4()` から `translation` 抽出）。独立 `spatial.position` は持たず、二重管理を回避。指向性 `cone` の向きはレイヤーの `+Z`（front）を `globalRot * (0,0,-1)` で取得。

PropertyGroups:
- `Spatial > Distance` — model/min/max/rolloff（Float/Integer、animatable=true、Modulationターゲット `modulationTargetId("layer/{id}/spatial.rolloff")`）
- `Spatial > Directivity` — inner/outer/outerGain
- `Spatial > Motion` — doppler/dopplerFactor（Phase2まで disabled）

`setLayerPropertyValue()` は `spatial_.*` 更新 → `publishSnapshot()` → `changed()` のみ。サンプルレートは `getAudio()` の引数 `sampleRate` を `SpatialRenderer::setSampleRate()` に伝播。

Listener解決: `SpatialListenerResolver::resolve(const Composition&)` が `ArtifactCompositionRenderController.cppm:31645` と同一ロジック（`isActiveCamera() && cameraPriority()` 最大）で `Vec3 listenerPos + Quat listenerRot` を返す。環境マップ `ArtifactEnvironmentMapLayer.ixx:20` の `intensity` は将来 `Audio.Reverb` の send 係数に流用予定だがPhase1では未接続。

## 6. 評価パイプライン

```
Composition::allLayerRef()
  → for each SpatialAudioLayer: getAudio(tmp, start, frames, sr) // デコード済みモノラル
  → SpatialRenderer::processBlock(tmp, spatialOut, listener)
  → AudioMixer / AudioRenderer::enqueue(spatialOut) // 既存ミックスに合流
```

`getAudio()` は `AudioSegment:30` の `channelData` を書き換えるのみで、`AudioRenderer:108` の `enqueue` 直前のコピーを回避するため `AudioSegment& out` を参照で受け取るオーバーロードを追加（既存 `enqueue(const AudioSegment&)` は残す）。

## 7. 永続化

`toJson()` に `spatial.distanceModel/min/max/rolloff/coneInner/coneOuter/coneOuterGain/doppler` を追加。`fromJsonProperties()` で旧キー `distanceModel` 欠落時はデフォルト `Inverse`。`QJsonObject` の `type = LayerType::SpatialAudio`（新値、Factoryに登録）。

## 8. 性能目標

- 128サンプルブロック、32同時音源、48kHz、ステレオ出力で `processBlock` 合計 <0.4ms（i7-12700目安）。`distanceAttenuation` LUT + ランプ適用のみで <0.05ms/ブロック。
- `metadata()` 的なstatループは持たない（ImageSequenceSourceの教訓）。`missingFrameCount` 的集計は `SpatialParams` に不要。
- `underflowCount/overflowCount`（`AudioRenderer:111`）で監視。`setLevelCallback` に空間化後ピークを流す。

## 9. ドナー参照

- 距離モデル: OpenAL Soft 1.22 `AL_DISTANCE_MODEL`、Steam Audio 4.5 attenuation curves
- HRTF: Resonance Audio / Oculus Audio HRIR partitioning、VBAPは `AudioPanner:15` 既存
- スレッドモデル: REAPER ReaPlug の lock-free param queue、JUCE `AudioProcessorValueTreeState` の seqlock

## 10. 実装フェーズ

- Phase 1（本マイルストーン）: `Audio.Spatial.*` Core + `ArtifactSpatialAudioLayer` 最小（距離+コーン+EqualPowerパン）、Listener解決、Property/JSON、Modulation接続。HRTF/dopplerはスタブ。
- Phase 2: HRTF partitioned convolution + doppler resampler（`Audio.DSP.DelayLine` 再利用）。
- Phase 3: オクルージョン（raycast vs `Mesh`）、環境リバーブ送り（`Audio.Reverb` + EnvironmentMap intensity）。

## 11. 検証

- 単体: `Audio.Spatial.Math` の距離/コーン計算を `ArtifactStudio_TestSequences` 的な固定ベクトルfixtureでGTest（許容誤差 1e-5）。`SpatialRenderer` は `QVector` アラインメントとランプ連続性をブロック境界でassert。
- 結合: 3DコンポにSpatialAudioLayer 1個 + Camera移動キーフレームで `AudioRenderer::bufferedFrames()` が途切れないこと、`tryFrameAt` 的な負キャッシュ相当の無音フォールバックが働くこと。
