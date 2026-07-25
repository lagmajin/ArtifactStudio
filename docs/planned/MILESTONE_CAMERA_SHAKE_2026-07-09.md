**ステータス:** In Progress（ワークストリーム A: 3D カメラシェイク実装済み / B: 未着手）

# M-CS-1: Camera Shake System - 設計マイルストーン

カメラが揺れる演出（3D 空間でのカメラシェイク / 画面全体を揺らす 2D シェイク）を追加するための設計マイルストーン。
3D カメラシェイクと trauma モデルは実装済み。2D スクリーンシェイクは未着手。既存の `jitter` は TAA の AA ジッタと 2D フィルムウィーブ揺らぎ (`FilmJitterEffect`) のみ。

## 設計判断（コンポーネントか、エフェクトか）

結論: **既存の `LayerComponentSystem` / `LayerModifier` にも `ArtifactAbstractEffect`（2D 画像フィルタ）にも乗せない。両者は別系統として実装する。**

- **カメラコンポーネントとして付けない理由**
  - `LayerComponentSystem` (`Artifact/include/Layer/ArtifactLayerComponentSystem.ixx`) / `LayerModifier` (`Artifact/include/Layer/ArtifactLayerModifier.ixx`) は cloner / particle / crowd / physics 系の sim フレームワーク。phase/scope モデルと sim コンテキストに依存し、カメラの projection/lens 状態とは噛み合わない。
  - `ArtifactCameraLayer` 自体もカメラ専用プロパティは全部自前の Pimpl (`ArtifactCameraLayer::Impl`) に持っており、汎用コンポーネント経路は通していない。シェイクをここに乗せると余計な複雑さになる。
- **2D エフェクトとして丸め込まない理由**
  - `ArtifactAbstractEffect` は `ImageF32x4RGBA` をチェーン処理する 2D 画像フィルタ（`Rasterizer` ステージ）。「画面全体をピクセルオフセットで揺らす」なら作れるが、それは 3D 空間でカメラ自身が揺れるのとは別物。2D エフェクト化すると DOF / 3D ジオメトリの正しい投影が崩れる。
- **採る形**
  - **3D カメラシェイク** = `ArtifactCameraLayer` に transient offset プロパティを持たせ、`viewMatrix()` の出口で加算（階層 transform を汚さない）。
  - **2D ポスト揺れ** = `Distortion.cppm` の `makeOffset` を使った `Rasterizer` エフェクトとして、既存パイプラインに乗せる。

## ワークストリーム A: 3D カメラシェイク

### M-CS-A1: カメラレイヤーへの transient シェイク状態追加

**状態:** 完了

- 目標: `viewMatrix()` に揺れオフセットを差し込める最小基盤を作る。
- 対象:
  - `Artifact/include/Layer/ArtifactCameraLayer.ixx`
  - `Artifact/src/Layer/ArtifactCameraLayer.cppm`（`Impl` に `shakeOffset_` / `shakeRotation_` / `shakeSeed_` 追加）
- 完了条件:
  - `viewMatrix()` = `getGlobalTransform4x4().inverted()` にシェイクオフセット（位置+回転）を加算し、zero のときは既存と完全に一致
  - 階層 transform（`transform3D()` / `getGlobalTransform4x4()`）は変更しない
  - シリアライゼーションは意図的に「保存しない / transient」であることを明記

### M-CS-A2: 揺れ生成モデル（trauma ベース）

**状態:** 完了

- 目標: AE 互換で直感的な揺れパラメータを定義する。
- パラメータ候補:
  - `trauma`（0..1、加算減衰）、`traumaDecay`、`shakeFrequency`
  - `positionAmplitude`、`rotationAmplitude`（deg）、`seed`
  - ノイズ: `PerlinNoise` / `ValueNoise` による時間変化（既存 `Distortion.cppm` の `makeTurbulentDisplace` / `makeNoiseDisplace` を参照）
- 完了条件:
  - `addTrauma(amount)` で trauma を加算し、毎フレーム `trauma *= decay` で減衰
  - 実オフセット = `trauma^2 * amplitude * noise(t)`（二乗で小揺れが強調されにくくなる classic trauma モデル）
  - カメラ以外のレイヤーに影響しない

### M-CS-A3: Inspector / キーフレーム連携

**状態:** 完了（Camera Shake プロパティ群とランタイム trigger を実装済み）

- 目標: シェイクパラメータを UI から操作可能にする。
- 対象: `ArtifactCameraLayer::getLayerPropertyGroups()` / `setLayerPropertyValue()`
- 完了条件:
  - "Camera Shake" プロパティグループを追加（trauma / amplitude / frequency / seed）
  - `addTrauma` はアクション／エクスプレッションから呼べる入口を用意（UI ボタン or timeline trigger）
  - キーフレーム化は「trauma をキー」する程度に留め、オフセット自体はランタイム生成

## ワークストリーム B: 2D ポストシェイク（画面揺れ）

### M-CS-B1: ScreenShakeEffect の新規追加

- 目標: 最終フレームをアニメーションオフセットで揺らす 2D エフェクトを作る。
- 対象:
  - `Artifact/src/Effects/Rasterizer/` 配下に新規エフェクト（`ScreenShakeEffect`）
  - ベース: `ArtifactAbstractEffect`（`Artifact/include/Effects/ArtifactAbstractEffect.ixx`）、`ComputeMode::CPU`
  - オフセット生成: `ArtifactCore/src/ImageProcessing/Distortion.cppm` の `makeOffset` を利用
- 完了条件:
  - `applyConfigured(src, dst)` で `ImageF32x4RGBAWithCache` をオフセット変換
  - `EffectPipelineStage::Rasterizer` として `buildRasterizedSurfaceBuffer`（`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`）のチェーンに乗る
  - エフェクト登録: `Artifact/src/Service/ArtifactEffectService.cppm` + `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` カタログ

### M-CS-B2: 2D シェイク用パラメータ

- パラメータ: `amplitudeX` / `amplitudeY`、`frequency`、`decay`、`seed`、`wrap`（端の扱い: clamp / wrap / mirror）
- 完了条件:
  - `makeOffset` の変位マップを時間でアニメーション
  - 端処理を `wrap` 以外選択可

## 両者の使い分け・統合指針

- **3D カメラシェイク（A）**: 3D レイヤー / パース / DOF がある本物の空間揺れ。カメラ自体が揺れるので、被写体の相対位置も揺れる。
- **2D ポスト揺れ（B）**: 2D のみのコンポジションや、最終出力に対する「画面がガクッと揺れる」演出。3D 情報不要で安価。
- 両方を同時に掛けることは推奨しない（二重揺れで視覚ノイズ増）。同一コンポジション内ではどちらか一方を選ぶ運用とする。
- カメラレイヤーは `isNullLayer()` = true の親レイヤーなので、A のシェイクは「カメラレイヤー自身」ではなく「そのカメラで描画される 3D シーン」に適用される点に注意（renderer 側の `set3DCameraMatrices` 経由）。

## 依存・未確認事項

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `renderOneFrameImpl()` で `activeCamera` を解決している箇所（`viewMatrix()` を呼ぶ位置）を特定し、A のオフセットがそこに反映されることを確認すること。
- 3D シェイクの減衰更新をどのクロックで回すか（preview playback clock / 専用タイマー）は別途確定。
- 2D エフェクトは `Rasterizer` ステージのみ対象。最終合成段 (`Artifact/src/Render/ArtifactFinalPostProcess.cppm`) への昇格は別マイルストーン。
- `LayerComponentSystem` 経由は今回採用しない（上記判断による）。将来的に sim コンポーネント化する場合も、カメラ projection 系とは別モジュールとする。

## 次のステップ

1. A 優先で `ArtifactCameraLayer::viewMatrix()` に transient オフセット差し込み口を確認・実装
2. B は独立して `ScreenShakeEffect` を `Rasterizer` パイプラインに乗せる
3. A/B のパラメータUI を Inspector に追加

## 2026-07-25 Screen Shake 実装監査

- `ScreenShakeEffect` は実装済みで、Rasterizer / CPU effect として登録され、Inspector の effect catalog から選択できる。
- amplitudeX / amplitudeY、frequency、decay、seed、wrapMode（clamp / wrap / mirror）を持ち、`Distortion` の displacement と bilinear sampling を使って 2D surface を変位させる。
- 未確認事項は、実際の layer effect rack での時間変化、端処理の見た目、長時間再生時の性能、3D camera shake との二重適用を避ける運用である。
- したがって Workstream B は「実装済み・runtime 確認待ち」と更新する。

## 2026-07-25 Runtime 統合監査

- 3D camera shake は `CompositionRenderController` の active camera 解決後に composition frame / delta を渡して `advanceShake()` を呼び、camera の `viewMatrix()` 経由で 3D render path に到達する。
- 2D Screen Shake は Rasterizer effect として `buildRasterizedSurfaceBuffer` / `applyRasterizerEffectsAndMasksToSurface` の effect chain から `applyConfigured()` に到達し、effect context の `timeSeconds` を使う。
- Inspector catalog、effect service factory、Rasterizer stage 登録も確認できる。
- したがってソース上の runtime 経路は接続済み。ただし build / runtime 実行は未実施のため、実際の揺れ量、frame scrub、wrap 境界、3D/2D 同時適用時の見た目は未検証。
