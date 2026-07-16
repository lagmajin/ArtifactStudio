# マイルストーン: Professional Soft Body / Cloth System

**ステータス:** In Progress

> 2026-07-11 作成  
> 矩形Shape向けの試作格子を、制作で再現・ベイク・レンダーできる変形システムへ発展させる。

## 目的

「プレビューで一度たわむ」だけではなく、再生fps・シーク・オフラインレンダーで同じ結果を出し、
布・旗・ゼリー・柔らかいロゴに使えるソフトボディを提供する。

## 現在の土台

- `Physics.SoftBody` はVerlet積分、距離拘束、風、自己衝突、破断、簡易リメッシュ、面積保持を持つ。
- `Physics.System` はレイヤーIDごとのsolverを管理する。
- 矩形Shapeは格子点をquad描画へ反映できる。
- この段階では画像/動画のUV変形、シーク復元、外部Collider world、キャッシュは未実装である。

## 非交渉の契約

1. simulationは固定タイムステップで進め、表示fpsに依存させない。
2. Preview、Bake、Renderは同じsnapshot contractを使う。
3. スクラブはlive solverを逆再生しない。cache snapshotを復元するか、明示的にresetする。
4. レイヤー自身のboundsを自己Colliderにしない。Colliderは別レイヤーまたは明示的なworld sourceである。
5. Image/Video変形はCPU `QImage` 合成ではなく、UV付きGPU gridで行う。

## 実装フェーズ

| Phase | 内容 | 完了条件 |
|---|---|---|
| P1 | Fixed-step solverと安定化 | 24/30/60fpsで同じ入力が同じstateへ収束 |
| P2 | `SoftBodySettings`、reset、snapshot、diagnostics | 設定・seed・frame stateを保存/復元できる |
| P3 | Composition simulation clockとframe cache | seek後にcacheから正しいstateを返せる |
| P4 | Collider world | Plane/Box/Circle/Polygon、layer間filter、接触イベント |
| P5 | GPU deformation grid | Shape/Image/VideoをUV付き頂点bufferで変形し、previewとrenderを共有 |
| P6 | Authoring UX | pin、weight、wind、pressure、tear、quality preset、diagnostics |
| P7 | Bake/export | 指定範囲をcacheまたはdeformation animationへbakeしundo対応 |

## 品質プリセット

| Preset | Step | Substeps | 主用途 |
|---|---:|---:|---|
| Draft | 1/60 s | 2 | 操作中の確認 |
| Preview | 1/120 s | 8 | 通常プレビュー |
| Final | 1/240 s | 16 | オフラインレンダー/Bake |

## P1 実装済み（2026-07-11）

- solverに固定タイムステップaccumulator（既定1/120秒）とsubstep上限を追加。
- playbackがsolverを駆動する経路を追加。
- 矩形Shapeの格子描画とInspectorの有効化プロパティを追加。

## P2 着手済み（2026-07-11）

- `SoftBodySnapshot` に点、拘束、体積拘束、風、格子状態、accumulatorを保存する。
- `PhysicsSystem` はレイヤーごとに直近480フレームのin-memory snapshotを保持する。
- Compositionは逆方向または大きなシークで、全ソフトボディの同一フレームsnapshotを原子的に復元する。

## 連続体材質 backend（2026-07-11）

- `Physics.Mpm2D` はfixed-corotated弾性、Young率、Poisson比、塑性、破断を既に持つ。
- 肉・フォーム・硬いゴム・木材は、まずこのMPM backendを材質solverとして使う。
- `PhysicsSystem` にmaterial solver登録と材質presetを追加した。FEMはUV/頂点を厳密に保つmesh backendとして後段に追加する。
- MPMも固定step accumulator化し、表示fpsと独立して材質計算を進める。
- LayerのInspectorから材質simulationとFlesh/Foam/Hard Rubber/Wood presetを選択し、bounds由来の粒子gridを初期化できる。
- MPMの破断粒子数を `MaterialFractureEvent` としてCompositionへ渡し、既存のlayer fracture/shard/debrisへ連携する。
- MPMもフレームsnapshotを保存・復元し、soft bodyと同じシーク契約に揃える。
- MPMへPlane/Box/Circle collision proxyを追加。次段で既存Box2D colliderとcomposition collision sourceをこの契約へ変換する。
- Compositionは有効なcollision layerのboundsを材質レイヤーのlocal spaceへ変換し、MPM Box proxyとして再登録する。再生フレームでもこの同期を行う。
- Circle colliderはMPM Circle proxyへ変換する。回転・非等方scaleによる楕円は外接半径で安定に近似する。

## 次の実装順

1. `SoftBodySettings` と品質presetをCore API化する。
2. Composition単位のsimulation clockとcache miss時のreset policyを追加する。
3. UV付き動的grid render packetを追加し、Image/Videoに拡張する。
4. cache/bakeを実装してから、破断・fracture eventと接続する。

## 関連

- `docs/planned/MILESTONE_LAYER_PHYSICS_COMPONENT_2026-06-13.md`
- `docs/planned/MILESTONE_LAYER_COMPONENT_PIPELINE_2026-07-01.md`
- `docs/planned/MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md`
