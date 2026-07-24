**ステータス:** Not Started

# M-PREFRACTURE: シンプルな事前破砕（Pre-Fracture）導入 — 設計メモとギャップ修正

作成日: 2026-07-22
目的: レイヤーへの「シンプルな事前破砕」を、既存コンポーネント（Collision / Cloner / Layout / Crowd / Particle Emitter / Fluid）と矛盾しない形で提供する。

## 結論

**新規システムは不要。既存の fracture インフラが約9割揃っており、`fracture.preGenerate` + `fracture.triggerFrame` が正規レシピとして既に機能する。残るギャップ G1〜G3 を埋めるだけでよい。**

## 既存資産（実装済み）

| 資産 | 場所 | 状態 |
|---|---|---|
| 破砕生成器（Radial/Grid/Voronoi/Hybrid、6プリセット Glass/Concrete/Stone/Metal/Wood/Dust、seed=0 で決定論的） | `ArtifactCore/include/Geometry/Fracture.ixx`（`FractureEffect`） | ✅ |
| プロパティ一式（`fracture.enabled` / `fracture.preGenerate` / `fracture.triggerFrame` / preset / shardCount / damping / gravity / impactSensitivity） | `Artifact/src/Layer/ArtifactAbstractLayer.cppm:6083-6178`、JSON 永続化 :3974, :4524 | ✅ |
| 事前生成経路（preGenerate 有効時に shard 生成・状態へ種込み） | `drawFractureOverlay` 内 :2624-2650（lazy、`shards.empty()` ガードで1回のみ） | ✅ |
| トリガーフレーム（指定フレームで impact 発火、巻き戻しで re-arm） | :2388-2403 | ✅ |
| 描画（`syncFragmentDataset` → fragments → レンダー） | 全11レイヤー型が `drawFractureOverlay` を draw 末尾で呼出済 | ✅ |
| コンポーネント登録（Topology フェーズ・InstanceSet・order 600） | `Artifact/include/Layer/ArtifactLayerComponentSystem.ixx:676` | ✅ |

## 正規レシピ（シンプル事前破砕の使い方）

1. `fracture.enabled = true`
2. `fracture.preGenerate = true`（衝撃前に決定論的な shard ジオメトリを用意）
3. 任意で `fracture.triggerFrame = N`（指定フレームで破砕開始。巻き戻しで自動 re-arm）
4. `fracture.preset` / `fracture.shardCount` で形状調整

collision コンポーネントは不要（G1 対応が前提）。

## ギャップ修正リスト

### G1: fracture の collision 必須依存を緩和【矛盾の本丸】

- 現状: `makeFractureComponentDescriptor` が `requiredTypeIds = {artifact.component.collision}`（`ArtifactLayerComponentSystem.ixx:685`）。validation（:480-542）は依存先が無効だと **error**。事前破砕は collision を使わないのに、collision 無効でエラー表示になる。
- 修正: `requiredTypeIds` から collision を外すか、「preGenerate==false かつ triggerFrame<0 の impact 駆動時のみ必須」に条件化。phase 順（Dynamics 500 → Topology 600）は正しいので変更しない。
- 完了条件: preGenerate + triggerFrame 構成で collision 無効でも validation error が出ない。impact 駆動（collision 経由 :1718, :2106）は従来どおり動作。

### G2: preset / shardCount 変更時に事前生成が更新されない

- 現状: setter（`ArtifactAbstractLayer.cppm:7807-7810` preset、:7819-7822 shardCount）が `resetFractureState()` を呼ばず、lazy 生成は `shards.empty()` の時だけ → 変更しても破砕形状が更新されない。
- 修正: 両 setter で `resetFractureState()` を呼ぶ（`fracture.preGenerate` setter :7793-7801 と同じ既存パターン）。
- 完了条件: preset / shardCount 変更後の次フレーム描画で新しい破砕形状が反映される。

### G3: UI 露出を Components 専用面の導線に揃える

- 背景: AGENTS ルール「コンポーネント由来グループは通常 Property Widget に出さず、Components 専用面を正規の編集導線とする」。`Fracture` グループ（:6083）は `builtin.fracture` コンポーネント由来。
- 修正: Collision / Cloner 等と同様に Components 専用面の編集導線に揃える。通常プロパティ面への新規露出は行わない。
- 完了条件: Fracture 設定が Components 専用面から編集でき、通常 Property Widget の表示方針が AGENTS ルールと一致。

## やらないこと（非目標）

- Rasterizer エフェクトとしての破砕追加 → Topology コンポーネントと二重系統になり、描画順・キャッシュ・評価フェーズが衝突する。
- `VoronoiEffect`（Effects/Rasterizer）の流用 → 模様生成エフェクトであり破砕シミュレーションではない。

## 補足（許容判断）

- 事前生成は描画パス内 lazy（:2624）。seed=0 で決定論的、`shards.empty()` ガードで1回のみのためシンプル用途では許容。将来 shard 数を大きくする場合は、生成をプロパティ変更時/評価時に移す余地あり。
- `applyFractureImpact` 後に `prefractureResult_` が invalid / shard 数不一致なら再生成する経路（:2949-2960）は既存。G2 は preGenerate 駆動時の対称性を揃えるもの。

## 検証方法

- preGenerate 有効レイヤーでコンポジション先頭フレームから破砕形状が決定論的に再現されること（seed=0）。
- triggerFrame 往復スクラブで re-arm → 再発火が安定すること。
- preset / shardCount 変更が即座に形状へ反映されること（G2 後）。
- `validateLayerComponents()` で collision 無効 + preGenerate 構成が error 非表示になること（G1 後）。
