# グループレイヤー昇格の検討

**最終更新:** 2026-08-27

## 背景

グループレイヤー（`ArtifactGroupLayer`）は「色々なレイヤーをまとめる」レイヤーとして導入されたが、レイヤー機能としては責務が重すぎる、という問題意識から、上位の仕組みへ昇格できるか検討した。

## 現状の事実（ソース確認済み）

### 責務の所在

| 責務 | 実際の所在 | 出典 |
|------|-----------|------|
| 親子関係の正 | `ArtifactAbstractLayer::parentLayerId_` + `ArtifactAbstractComposition::childLayersOf()` | `ArtifactAbstractLayer.cppm:807`, `ArtifactAbstractComposition.cppm:3536-3549` |
| Group 自身の子ベクタ | `GroupImpl::children`（Composition 未接続時のレガシー置き場） | `ArtifactGroupLayer.cppm:33-35` |
| 評価制御フック | `ArtifactAbstractLayer` の仮想関数（`hasExclusiveChildSelection` / `selectedChildIdForEvaluation` / `childEvaluationGain`） | `ArtifactAbstractLayer.ixx:467-469` |
| 合成境界（本質的な重さ） | `ArtifactGroupLayer::draw()` に直書きされたオフスクリーン合成 | `ArtifactGroupLayer.cppm:206-400` |
| 出力モード（All/Single/Share） | `GroupOutputMode` と `setOutputMode` 等 | `ArtifactGroupLayer.ixx:29-33`, `ArtifactGroupLayer.cppm:583-590` |

### 重要な発見

1. **「まとめる」機能は既に Composition 側が正で持っている。**
   - `childLayersOf(parentId)` は任意の親 ID で子を返す汎用 API であり、Group 専用ではない。
   - `ArtifactGroupLayer::children()` は Composition 接続時は `childLayersOf()` を優先し、`std::vector children` はフォールバックに過ぎない（`ArtifactGroupLayer.cppm:510-521`）。

2. **Group を「レイヤー」に留めている唯一の本質は合成境界。**
   - 子を一時テクスチャに描いて 1 枚にし、その 1 枚に対してグループ全体のブレンドモード・不透明度・マスクを適用する責務。
   - これが `draw()` にインライン展開されており、Group の「重さ」の本体。

3. **コンテナ共通の上位抽象は存在しない。**
   - `ArtifactGroupLayer` / `ArtifactSwitchLayer` / `ArtifactMaterialContainerLayer` / `ArtifactParametricCompositionLayer` / `ArtifactCompositionLayer` は、いずれも `ArtifactAbstractLayer`（または `ArtifactAbstract2DLayer`）直下の独立リーフ。
   - 子管理 API と合成境界は Group が自前実装しており、他コンテナと共有されていない。

## 昇格の方向性

### 案 A：合成境界ごと捨てて薄くする（純粋な階層ノード化）

- Group をレイヤー種別から外し、「階層フォルダ」ノードに格下げ。
- 子管理は Composition に完全委譲。
- 利点：実装が最も薄くなる。二重構造（`children` ベクタと Composition 親子関係）が解消する。
- 欠点：グループ全体へのブレンドモード／不透明度／マスクの一括適用が失われる。
  - AE でいう「グループ（プリコンポ）的な合成境界」が成立しなくなる。

### 案 B：合成境界は残しつつ共通サービスへ切り出す

- 子管理 API を共通のコンテナ抽象（例：`ArtifactAbstractContainerLayer`）に集約。
- 合成境界（オフスクリーン合成）はコンテナ系が再利用できるサービスへ切り出し。
- 利点：既存機能（一括ブレンド／マスク／All/Single/Share）を維持したまま、Group を「レイヤーの一種」から「コンテナの合成境界」へ昇格できる。
- 欠点：変更範囲が大きい。コンテナ抽象の新設は `.ixx` 追加＝全体再スキャン要因。

## 実装上の注意点

- **モジュール循環参照**：`Artifact.Layer.Group` は既に `Artifact.Composition.Abstract` を import している。コンテナ抽象を置く場所を誤ると循環参照になりやすい。
  - 前方宣言で済ませられる型は import を避け、GMF（global module fragment）に置く。
  - 合成境界の切り出し先も `Artifact.Composition` 系に寄せるか、独立モジュールにするか慎重に選ぶ必要がある。
- **`isGroupLayer()` の互換**：27 箇所で分岐に使用されている（`ArtifactLayerPanelWidget.cppm`, `ArtifactHierarchyModel.cppm`, `ArtifactCompositionEditor.cppm` など）。真偽判定の意味を変える場合は影響範囲の確認が必須。
- **`.ixx` 新規追加は最小限に**：AGENTS ルール上、新規 `.ixx` は全体再スキャン要因。既存ファイルへの追記・編集で代替可能か先に検討する。
- **レンダラー優先順位**：GPU/Diligent 経路を優先。Group の合成境界を触る場合は GPU 経路での挙動を壊さないことが前提。

## 結論

「色々なレイヤーをまとめる」という機能そのものは既に Composition の親子関係として汎用化されており、Group が二重に保持している状態が重さの主因。

- 単に「まとめる」だけなら、Group はレイヤーである必要がない（案 A で十分）。
- 「グループ全体への一括ブレンド／マスク」を残すなら、合成境界だけを共通サービスへ切り出す（案 B）。

いずれにせよ、これは新規機能の追加ではなく、**「コンテナ責務の重複整理」と「合成境界の再利用化」**が中心となる。

## 未決定事項

- どちらの案を採るか（ユーザー判断待ち）。
- 案 B の場合のコンテナ抽象の配置場所とモジュール分割。
- 案 A の場合の「グループ全体へのブレンド／マスク」喪失を許容するか。
