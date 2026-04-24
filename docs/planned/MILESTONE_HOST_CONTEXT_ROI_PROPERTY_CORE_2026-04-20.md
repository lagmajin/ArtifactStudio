# マイルストーン: Host / Context / ROI / Property Core

> 2026-04-20 作成

## 目的

ArtifactStudio のレンダリング・エフェクト・プロパティ基盤を、DCC / compositor 系ツールに近い疎結合な契約へ段階的に寄せる。

このマイルストーンは次の 4 本柱をまとめて扱う。

- Host-Plugin Interface Contract
- Evaluation / Render Context Registry
- Tiled ROI / Partial Evaluation
- Universal Property Registry

ただし、最初から挙動を変えることは目的にしない。

- 先に read-only な抽象層を置く
- 次に adapter で既存実装を包む
- 最後に必要な箇所だけ実際の評価経路を切り替える

---

## 現状整理

今のコードベースには、完全ではないが既に近い土台がある。

- `RenderContext` / `RenderROI`
  - [`Artifact/include/Render/ArtifactRenderContext.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderContext.ixx)
  - [`Artifact/include/Render/ArtifactRenderROI.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderROI.ixx)
- `AbstractProperty` / `PropertyGroup` / expression
  - [`ArtifactCore/include/Property/AbstractProperty.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Property/AbstractProperty.ixx)
  - [`ArtifactCore/include/Property/PropertyGroup.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Property/PropertyGroup.ixx)
  - [`ArtifactCore/src/Property/AbstractProperty.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Property/AbstractProperty.cppm)
- effect 基盤
  - [`Artifact/include/Effetcs/ArtifactAbstractEffect.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Effetcs/ArtifactAbstractEffect.ixx)
  - [`Artifact/include/Effetcs/EffectContext.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Effetcs/EffectContext.ixx)
- render queue / export
  - [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)

不足しているのは「概念そのもの」より、ホスト契約としての一貫性と、非 UI / 非 direct-call の入口である。

---

## 原則

1. 既存の layer / effect / property UI をすぐには壊さない
2. まずは Core 側に抽象を追加する
3. 既存 API は互換 adapter として当面残す
4. 実評価経路の変更は後半フェーズまで遅らせる
5. ROI / tile / dependency declaration は metadata から始める

---

## Phase 1: Render Context Registry

### 目的

既存の `RenderContext` を「本当に共通で参照される文脈」に昇格する。

### 追加するもの

- `RenderContextRegistry`
- `RenderPurpose`
  - `EditorInteractive`
  - `EditorPreview`
  - `FinalExport`
  - `ProxyBuild`
- `RenderContextSnapshot`
- context を受け渡す read-only access API

### まずやること

- 既存 `RenderContext` の意味を固定する
- `time / resolutionScale / interactive / colorDepth / colorSpace / roi` を必須項目として整理
- preview / export / cache build の context 生成箇所を 1 か所に寄せる

### この段階で何をしないか

- 実レンダリングのアルゴリズムは変えない
- 既存 widget 内部のローカル state はすぐには消さない

### 完了条件

- preview / export / proxy が共通の context factory を通る
- layer / effect 側が context を read-only に読める
- `interactive` や `resolutionScale` の意味がコード上で 1 つに揃う

### 挙動変更リスク

低い。
最初は wrapper / factory 追加だけで進められる。

---

## Phase 2: Universal Property Registry

### 目的

UI を経由しなくても、全プロパティを ID / path で検索・読取・書込・serialize できるようにする。

### 追加するもの

- `PropertyPath`
- `PropertyRegistry`
- `PropertyHandle`
- `PropertyOwnerDescriptor`
- `PropertySerializationBridge`

### まずやること

- `AbstractProperty` と `PropertyGroup` を列挙できる registry を追加
- layer / effect / composition の property owner を path で引けるようにする
- expression / keyframe / raw value を UI 非依存で読めるようにする

### 例

- `layer.<LayerID>.transform.position.x`
- `layer.<LayerID>.effect.<EffectID>.mix`
- `composition.<CompositionID>.settings.frameRate`

### この段階で何をしないか

- PropertyWidget を大改修しない
- 既存の editor row 生成コードは当面残す

### 完了条件

- Python / CLI / NetworkRPC から UI なしで property 参照ができる
- expression / keyframe / current value を path で取得できる
- existing property UI は registry の consumer に寄せ始められる

### 挙動変更リスク

低い。
既存 property モデルの前に index を置くだけなら安全。

---

## Phase 3: Host-Plugin Contract Adapter

### 目的

effect 実装と host の間に、直接バッファを握らない抽象境界を導入する。

### 追加するもの

- `EffectHostContext`
- `EffectInputRequest`
- `EffectOutputSurface`
- `EffectCapabilityDescriptor`
- `EffectDependencyDescriptor`
- `IEffectHostAdapter`

### 初期方針

最初から `DescribeDependencies()` を必須にしない。

まずは次の 2 層に分ける。

- legacy path
  - `apply(src, dst)` を使う既存 effect
- host-adapted path
  - `describeCapabilities()`
  - `describeDependencies()`
  - `render(hostContext, outputSurface)`

### この段階でやること

- 既存 `ArtifactAbstractEffect` を adapter で包めるようにする
- host 側が effect に ROI / time / purpose を渡せるようにする
- effect が「何を要求するか」を宣言する構造だけ先に足す

### この段階で何をしないか

- 全 effect を一気に書き換えない
- direct buffer path を即廃止しない

### 完了条件

- 新規 effect は host contract 経由でも作れる
- 旧 effect は adapter 経由で動く
- dependency declaration 用の型が安定する

### 挙動変更リスク

中程度。
ただし adapter 先行なら実動作はかなり維持しやすい。

---

## Phase 4: Dependency Declaration

### 目的

`render()` の前に「何が必要か」を host に返すフェーズを正式導入する。

### 追加するもの

- `DescribeDependencies(Time, ROI, Channels)`
- `DependencyGraphNode`
- `RequestedInputSurface`
- `RequestedChannelSet`

### やること

- effect / layer / precomp が必要入力を先に宣言できるようにする
- precomp / track matte / mask / upstream effect の依存を host が把握できるようにする
- render queue / editor preview の両方で dependency tree を共通化する

### 完了条件

- host が入力準備と評価順を決められる
- effect 側が「自分で upstream を取りに行く」形から離れ始める

### 挙動変更リスク

高い。
ここから先は実評価経路が変わり始める。

---

## Phase 5: ROI Metadata and Partial Invalidation

### 目的

いきなり tile renderer に飛ばず、まず ROI と invalidation を正確に記述できるようにする。

### 追加するもの

- `EffectROIHint`
- `LayerInvalidationRegion`
- `DirtyRegionAccumulator`
- `RenderDamageTracker`

### やること

- layer 変更時に dirty rect を積む
- effect ごとに ROI expansion の宣言を持てるようにする
- preview 再描画が full-frame 前提でなくても済む下地を作る

### 完了条件

- `何が変わったか` を領域として追跡できる
- precomp / effect / transform が ROI metadata を返せる

### 挙動変更リスク

低〜中。
metadata 導入だけなら比較的安全。

---

## Phase 6: Tiled ROI Engine

### 目的

空間分割された評価単位で、必要タイルだけ計算する。

### 追加するもの

- `TileKey`
- `TileGrid`
- `SparseTileSurface`
- `TileRenderScheduler`
- GPU dispatch clipping

### やること

- `ImageBuffer` を全面置換する前に tile-backed surface を並行導入する
- full-frame path と tile path を切り替え可能にする
- precomp / heavy effect / export で tile path の効果を検証する

### 完了条件

- 一部更新で full-frame の GPU / CPU work を避けられる
- precomp / mask / blur などで ROI 付き評価が機能する

### 挙動変更リスク

高い。
ここは性能改善の本丸だが、最も慎重に進めるべき段階。

---

## Phase 7: Network / Script / Headless Integration

### 目的

上の抽象を UI 外の入口へ接続する。

### 接続先

- `NetworkRPCServer`
- Python API
- command-line render
- future plugin sandbox

### やること

- property registry を RPC から叩けるようにする
- render context を headless render でも同じ型で渡す
- host contract を plugin / remote evaluation の共通境界にする

### 完了条件

- UI なしで property 操作と render kickoff が可能
- remote / local / interactive で共通 contract を使える

---

## 実装順の提案

1. Render Context Registry
2. Universal Property Registry
3. Host-Plugin Contract Adapter
4. Dependency Declaration
5. ROI Metadata and Partial Invalidation
6. Tiled ROI Engine
7. Network / Script / Headless Integration

---

## 無挙動変更で入れやすい範囲

次の 3 つは、既存動作を大きく変えずに先行導入しやすい。

- Phase 1: Render Context Registry
- Phase 2: Universal Property Registry
- Phase 3: Host-Plugin Contract Adapter

特に Phase 1 と Phase 2 は、まず read-only registry / adapter として入れるのがよい。

---

## リスク

- `RenderContext` は既に存在するため、二重定義ではなく正規化が必要
- `Property` は UI と密結合ではなく、むしろ owner 列挙の不足が本質
- tile / dependency declaration は最終的に挙動変更を伴う
- `NetworkRPCServer` はまだ骨組みであり、即戦力ではない

---

## 完了条件

- preview / export / proxy が共通 context contract で動く
- property を path / id で UI 外から操作できる
- effect / plugin に host contract が存在する
- ROI / invalidation / tile evaluation を段階的に使える
- headless / remote / plugin 経路に同じ基盤を流用できる

---

## 関連ファイル

- [`Artifact/include/Render/ArtifactRenderContext.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderContext.ixx)
- [`Artifact/include/Render/ArtifactRenderROI.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderROI.ixx)
- [`Artifact/include/Effetcs/ArtifactAbstractEffect.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Effetcs/ArtifactAbstractEffect.ixx)
- [`Artifact/include/Effetcs/EffectContext.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Effetcs/EffectContext.ixx)
- [`ArtifactCore/include/Property/AbstractProperty.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Property/AbstractProperty.ixx)
- [`ArtifactCore/include/Property/PropertyGroup.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Property/PropertyGroup.ixx)
- [`ArtifactCore/src/Property/AbstractProperty.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Property/AbstractProperty.cppm)
- [`ArtifactCore/NetworkRPCServer.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/NetworkRPCServer.ixx)

## Follow-up Notes

- `2026-04-22`
  - `RenderContext` の生成を `createEditorPreviewContext()` / `createFinalExportContext()` / `createProxyBuildContext()` に集約し、preview / export / proxy の入口を共通化した
  - `RenderContextRegistry::makeKey()` は目的名を文字列化するようにして、snapshots の見通しを少し上げた
  - `PropertyRegistry` に owner / handle の read-only query を追加し、`ArtifactPropertyWidget` の summary から snapshot owner の状態を読めるようにした
  - `PropertyRegistryReadOnlyAdapter::queryAllOwners()` を追加し、owner snapshot の列挙を UI 外から行いやすくした
  - `ArtifactInspectorWidget` の layer type signature を表示ロジックと揃え、NoLayer 以外の type 変化を拾いやすくした
  - `ArtifactProjectService::selectLayer()` で invalid / cleared / reselect の reason を明示し、selection boundary の log を追いやすくした
