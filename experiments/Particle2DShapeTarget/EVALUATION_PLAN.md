**最終更新:** 2026-11-19

# Evaluation Plan

詳細と時系列の実施記録は[planned milestone](../../docs/planned/MILESTONE_PARTICLE_2D_SHAPE_TARGET_2026-11-18.md)を唯一の結果記録として扱う。本書はこのexperimentの対象と再実行可能な評価matrixを定義する。

## 対象・条件

- 2D Particle LayerのShape/Text target attraction、disabled parity、永続化、fixed-step deterministic seek。
- GPU offscreen capture: VulkanとD3D12、320×180、30 fps、seed `20261118`、frames 30/60/90/180/240、同一固定fixture LayerID。
- Fixtureは外側/内側ellipseのordered Shape Stack subtract ring、Arial Regular 400 `A8` stroked text、empty Shape、360粒子のpoint emitter。
- Baselineは同一fixtureのdisabled default。backend間はRGBA/alpha差分、粒子中心の同index位置を比較する。再現可能な粒子中心は0 px差、同じGPU draw contractはRGBA最大差0を期待する。disabled baselineの1-channel以下の少数画素差は位置・alphaの比較と分けて報告する。

## 評価matrix

| 評価 | 状態 | 証拠／制限 |
|---|---|---|
| Disabled default、欠損/self/empty target | 実施 | offscreen pixel parityとJSON persistence check |
| Shape visible fill、穴、target sampler | 実施 | Vulkan/D3D12 alpha、mask、particle centers。ordered stack fixtureのみ |
| Text static glyph fill/stroke | 実施 | `A8`、Arial。多言語/複数font backend未評価 |
| Continuous/direct seek | 実施 | frame 60、粒子ID/位置とPNG比較 |
| Backend comparison | 実施 | Vulkan/D3D12 fixed IDsのGPU offscreen capture。Composition View/Render Queue間のcross-backend比較は未評価 |
| Composition View / Render Queue | 部分実施 | `RenderQueueLayerImageIntegrationTest.ShapeTargetChangesParticleOutputThroughRenderQueue` がD3D12の`drawLayerForCompositionView()`とRender Queueで有効/無効画像の差20px超を確認。実アプリUI、Text target Queue、VulkanとのQueue比較は未評価 |
| Performance | 探索のみ | Debug、360 live particles、1 process内5 batches。run間confidence/threshold/Release比較未確定 |
| Memory | 部分実施 | CPU process working setのみ。GPU memory/cache allocation size未測定 |
| Shape edge cases | 未実施 | legacy merge、non-stack multi-content、stroke-only、opacity、animation |

画像品質の合否thresholdは候補最終比較前に固定できていない。現在の値は実装応答とbackend parityの探索記録であり、品質合格の主張には用いない。
