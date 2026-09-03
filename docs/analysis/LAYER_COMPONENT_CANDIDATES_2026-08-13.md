# レイヤーコンポーネント 追加候補

**最終更新:** 2026-09-03

レイヤーコンポーネントシステム（`ArtifactLayerComponentSystem.ixx`）に追加すべきコンポーネント候補を、既存のフェーズ構造（Source → Drive → Generate → Arrange → Intent → Dynamics → Topology → Emit → RenderExtraction）に沿って整理したもの。

> 進捗 (2026-08-24): (1) Source 相に `builtin.source-solid-fill` / `builtin.source-image` / `builtin.source-noise` の descriptor 登録と `resolveLayerSourceOverride()` 実装、(2) `sourceComponentSettingsSnapshot()` による descriptor `settings` のライブ反映（平面=幅/高さ/fill、画像=パス/連番/色管理、ノイズ=全 ProceduralTextureSettings+色マップ）、(3) `sequence-player` を `clonerSequenceEnabled_` に連動（従来は常時 `enabled=false` 固定）の3点を対応。詳細は `Insight.md` の 2026-08-24 項目を参照。ノイズ Fill 方式は `MILESTONE_SOLID_LAYER_NOISE_FILL_2026-08-18.md` を Superseded とし独立レイヤーへ移行済み。

> 注意: 本稿は「既存実装との重複・実現可否をコードで検証した結果」ではない。各候補の実装状況は別途ソース確認が必要（未検証と明記する）。

## 分類

1. 空のフェーズを埋めるもの（構造的に必要）
2. 既に存在するが component 化されていないもの
3. 差別化のための新規

---

## 1. 空のフェーズを埋めるもの

### Source 相（現在 component ゼロ）

レイヤーの「ソース供給」を component 化する。footage / sequence / procedural / solid を統一して Source 相に乗せる。現状はソースがレイヤー直持ちで、Source 相が空のままなので component パイプラインに乗せられない。

- 静止画 / 連番 / 動画 / プロシージャル / ソリッドを統一ソース component に。
- ソースの時間解決・シーク・キャッシュ責務を component 側へ。

### RenderExtraction 相（現在 component ゼロ）

マテリアル / シェーディング / レンダーパス抽出を component 化する。

- toon / emission / AOV / stylization。
- 3D の PBR とカラーの最終変換をこの相に統合。

---

## 2. 既に存在するが component 化されていないもの

### Soft-body / Cloth / Rope（Dynamics / Topology）

`softBody.enabled` は property group に存在するが descriptor 未登録。フラクチャと同様に component 化すべき。

- `PhysicsSystem` / `SoftBodySolver` は実装済みなので接続が中心。

### Force Field（Composition scope）

重力・乱流・磁場・渦を composition 横断の component にする。

- 粒子側の `ForceField` は存在するが、composition 横断の component としては無い。
- 現在 Composition scope は collision と fluid のみ。ここを拡張。

### Deformer（Topology）

bend / twist / taper / skew を component 化する。MoGraph 相当。

- 3D のデフォーマ未実装（`THREED_LAYER_FEATURE_GAP`）と合致。

### Constraint（Drive）

look-at / follow-path / parent / distance を component 化する。

- カメラ POI 未実装の代替にもなる。

---

## 3. 差別化のための新規

### Audio Reactive（Drive）

AudioRMS / spectrum で駆動する component。

- audio 解析と式関数（`AudioRMS`）が既にあるので component 化は自然。
- AE の有償プラグイン相当を内製できる。

### Data Driven（Drive）

CSV / JSON で position / color / scale を駆動する component。

- `DataAssetFile`（CSV）が既にある。

### Boolean / Mesh Merge（Topology）

ブール演算、メッシュ統合。

- SDF レイヤーと相性が良い。

### Morph / Blend Shape（Topology）

モーフ変形。

---

## 優先度の見立て

- **差別化軸（前回の改善マップの結論）に効く**: Audio Reactive、Force Field の component 化。
- **整合性回復軸で構造上先**: Source 相と RenderExtraction 相を埋めること。
- **比較的軽い（既存部品の登録・接続）**: Soft-body、Deformer。

---

## 関連する既存検証

- 整合性検証: `LAYER_COMPONENT_PIPELINE_INTEGRITY_2026-08-13.md`
- 改善マップ: `AE_PAIN_POINT_IMPROVEMENT_MAP_2026-08-13.md`
