# Milestone: AE-Style Simple Rain Effect

> 2026-05-31

**最終更新:** 2026-08-15
**現状:** CPU effect／Inspector factory／静止画・連番の通常stack経路は実装済み、GPU／preset packaging／runtime受入れ待ち

## 2026-08-15 現行コード監査

`ArtifactParticleLayer`／粒子レンダー基盤、および AI description 側の `rain` preset 名は確認できる。一方、`RainEffect`、雨 streak／density／wind／splash の専用パラメータ、composition render への雨 preset の接続は現行コードから確認できなかった。`ArtifactCore::RainModel` は AcousticSystem 配下のモデルであり、映像用の雨描画実装とは別責務である。

判定（旧監査時点）: **Phase 1〜5 は未着手。** 現行コード照合は下記 Update 2026-08-15 を正とする。

## Update 2026-08-15 — 現行コード照合

`SimpleRainEffect` のCPU実装を確認した。密度、streak length、speed、wind、opacity、depth、splash、evolution、seedを持ち、決定的hashから雨滴を生成して `ImageF32x4_RGBA` 上で合成する。`ArtifactEffectService` のfactory／catalogにも登録され、通常のRasterizer effect stackから利用できる。`Preset` プロパティから `Light`／`Heavy` を適用でき、個別編集時は `Custom` に戻る。

したがって Phase 1〜2 の最小実装と、light/heavy の最小プリセット適用は完了相当。GPU equivalent、プリセットの専用UI／配布パッケージ、zoom／composition boundsのruntime受入れ、preview／render queue parity、GPU／CPU性能比較は未完了または未検証。

## Purpose

`After Effects` っぽい見た目の簡易雨を、既存の particle / effect / overlay 基盤の上で最小構成から実現する。

ここでの狙いは、物理的に正確な降雨シミュレーションではなく、`streak / density / direction / splash / depth feel` を短い工数で出して、映像演出として十分に使える状態へ寄せること。

## Why This Exists

- 既存の particle 系と 2D 描画基盤があるため、雨は新しい大きなシステムを作らずとも表現できる可能性が高い
- `AE っぽい簡易雨` は、制作現場で使うときの「ちょっと足したい」需要に合う
- 流体や高精度天候モデルより先に、見た目の満足度を小さく早く得られる

## Current Findings

### 1. Existing Particle Path Can Likely Carry The First Slice

- `ArtifactParticleLayer` と particle renderer の既存経路がある
- 雨の最初の版は、粒子の方向・速度・寿命・透過度の組み合わせでかなり近づけられる

### 2. Rain Is Mostly A Screen-Space Visual Problem

- 雨は物体の物理より、画面上の motion streak と density の問題が大きい
- camera zoom / canvas size / composition bounds に応じた見え方調整が必要

### 3. The First Version Should Be Preset-Driven

- 雨専用の preset を先に作るのが安全
- いきなり汎用 weather system にしないほうが、早く成果が出る

## In Scope

- 雨 streak の particle preset
- 密度調整
- 斜め下方向の流れ
- 速度レンジの調整
- 軽い splash / hit の補助表現
- 画面サイズと zoom に対する見え方補正
- 可能なら簡単な wind bias

## Out of Scope

- 本格的な気象シミュレーション
- 雨雲、雷、湿度、反射の完全モデル
- 3D 空間での複雑な軌跡計算
- 既存 particle 系の全面再設計
- 一般向け weather editor の大規模 UI

## Recommended Start Order

### Phase 1: Rain Look Preset

- 雨 streak を作る
- 斜め下方向の流れを固定する
- 透過度と長さを調整する
- 背景上で読みやすい密度に寄せる

### Phase 2: Screen-Space Stability

- canvas size に応じた見え方補正を入れる
- zoom で見たときに streak が潰れないようにする
- composition bounds 外への出方を整理する

### Phase 3: Rain Depth Feel

- 手前の雨を少し太く速くする
- 奥の雨を細く薄くする
- ごく少量の parallax を足して奥行きを作る

### Phase 4: Splash / Impact Accent

- 画面下端や接触面で小さい splash を出す
- 雨が「ただ流れる」だけでなく、少しだけ当たる感じを付ける

### Phase 5: Preset Packaging

- 既存の particle / effect 導線から呼び出しやすくする
- 雨 preset を再利用しやすい名前と設定群にまとめる

## Suggested Visual Recipe

1. `streak length`
   - 細長い雨粒を主役にする

2. `direction`
   - ほぼ一定の斜め下
   - 風があるときだけ少し揺らす

3. `density`
   - 画面全体を薄く覆う程度から始める
   - 強雨は別 preset に分ける

4. `opacity`
   - 真っ白にしすぎず、背景に馴染む程度

5. `splash`
   - 量は少なめ
   - 下端や接地面でだけ出す

6. `blur feel`
   - 物理 blur ではなく、速度と streak 長で表現する

## Success Criteria

- 1 つの preset で `light rain` と `heavy rain` の最小差分を出せる
- composition 上で雨が「見える」だけでなく「雰囲気を作れる」
- zoom やサイズ変更で破綻しにくい
- 既存 particle path を壊さずに導入できる

## Likely Touch Points

- [ArtifactParticleLayer.cppm](X:/Dev/ArtifactStudio/Artifact/src/Layer/ArtifactParticleLayer.cppm)
- [ArtifactIRenderer.cppm](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactIRenderer.cppm)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
- [ArtifactCompositionEditor.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm)
- [ArtifactRenderLayerWidgetv2.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm)

## Related

- [MILESTONE_VFX_PARTICLE_FLUID_2026-03-30.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_VFX_PARTICLE_FLUID_2026-03-30.md)
- [MILESTONE_PARTICLE_RENDER_PATH_STABILIZATION_2026-04-21.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_PARTICLE_RENDER_PATH_STABILIZATION_2026-04-21.md)
- [MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md)
