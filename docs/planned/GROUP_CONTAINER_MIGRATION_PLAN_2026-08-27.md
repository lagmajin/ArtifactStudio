# GroupContainer 移行計画

**作成日:** 2026-08-27  
**最終更新:** 2026-08-30
**ステータス:** Phase 0 / 1 実装済み、Phase 2 互換アダプタまで部分実装。独立Container実体とRender Boundary移行は未着手
**対象:** `ArtifactGroupLayer` から Composition 所有の独立 Container への移行

## Update 2026-08-30 — current implementation reconciliation

- `CompositionNode`、`ContainerNode`、`GroupContainerNode`、`CompositionNodeStore` は実装済み。NodeStoreはID重複、自己親、存在しない親、親子cycleを拒否し、parent IDとorderからchild順を再構築する。
- `ArtifactAbstractComposition` はlayer追加・階層変更・JSON保存／復元とNodeStoreを同期し、既存`childLayersOf()`を残している。`GroupContainerNode`はLayer非継承で、output mode、active child、enabled、opacity、blendをNode propertyとして往復する。
- `ArtifactGroupLayer` は描画・UI・既存プロジェクト互換の正規ownerのまま、NodeStoreとの双方向アダプタを持つ。これはPhase 2の互換層であり、独立したComposition兄弟Containerへ置換済みではない。
- GPU-native Render Boundary、Factory／UI／Undo／ExportのContainer実体化、旧`ArtifactGroupLayer`の廃止は未実装。ビルド・group JSON round-trip・Preview／Export parityはruntime未検証。

## 目的

`ArtifactGroupLayer` は通常の描画レイヤーではなく、子要素の階層管理とグループ合成境界を兼ねている。将来の Group / Precomp / Switch / Material 系コンテナ拡張に耐えられるよう、ContainerをLayer継承から分離する。

## 到達構造

```text
ArtifactAbstractComposition
└── CompositionNode
    ├── LayerNode
    │   └── ArtifactAbstractLayer
    └── ContainerNode
        └── GroupContainer
```

`GroupContainer` は `ArtifactAbstractLayer` を継承しない。レイヤーとコンテナはComposition上の兄弟ノードとする。

## 移行原則

- 既存 `ArtifactGroupLayer` は移行期間の互換アダプタとして維持する。
- 既存の `type: Group` JSONは読み込み可能な状態を維持する。
- Compositionを親子関係と表示順の唯一のsource of truthにする。
- コンテナの子はポインタではなく `LayerID` / `NodeID` で参照する。
- レンダリング用の合成境界はContainer本体から分離する。
- Factory、UI、Undo、Preview、Exportを一度に変更しない。

## フェーズ

### Phase 0: 契約とテスト

- `CompositionNode` のID、親ID、種別契約を定義。
- `ContainerNode` の子ID追加・削除・順序変更・重複拒否を定義。
- Layerを継承していないことをコンパイル可能なテストで固定。
- 不正な循環親子関係を拒否するテストを追加。

### Phase 1: Composition内のノードストア

- CompositionにNodeStoreを追加。
- 既存Layer配列との同期アダプタを追加。
- 既存 `childLayersOf()` の結果を変更せず、新APIを併設。
- Groupに依存する既存UIはこの段階では変更しない。

### Phase 2: GroupContainerの導入

- `GroupContainer` をLayer非継承の具体型として追加。
- 子要素はNodeIDで保持。
- `GroupOutputMode`、opacity、blend、mask方針をContainer側へ移行。
- 旧 `ArtifactGroupLayer` はGroupContainerへの変換アダプタとして維持。

### Phase 3: 合成境界の分離

- `ArtifactGroupLayer::draw()` にあるオフスクリーン処理をRender Boundaryへ移す。
- GPU経路を優先し、CPU readbackを新Containerの標準経路にしない。
- GroupのAll / Single / Shareの出力契約を回帰テストする。

### Phase 4: 周辺経路の移行

以下を順番に移行する。

1. Composition親子関係
2. Layer hierarchy UI
3. Factory
4. JSON保存・復元
5. Undo/Redo
6. Composition View
7. Render Queue
8. Export

旧JSONは読み込み時に `GroupContainer` へ変換し、必要な期間だけ旧形式へ書き戻せるようにする。

### Phase 5: 旧型の廃止

以下を確認してから `ArtifactGroupLayer` を削除する。

- ソース参照が互換読み込み部分以外でゼロ
- `isGroupLayer()` の意味を新Container判定へ移行済み
- Groupを含む既存プロジェクトのJSON round-tripが成立
- 親子関係、表示順、Undo/Redoが成立
- Preview / Render Queue / Exportの合成結果が一致
- 旧型を参照するプラグイン/API境界を整理済み

## 非目標

- Phase 1で既存Groupの描画品質を変更しない。
- Phase 1で全レイヤー型をNode化しない。
- Phase 1で `ArtifactGroupLayer` を削除しない。
- Render BoundaryとAdjustment GPU-native化を同じ変更に混ぜない。

## 最初の実装スライス

最初は独立したNode契約だけを追加する。

```text
CompositionNode
 ├── id
 ├── parentId
 └── kind

ContainerNode
 ├── children: NodeID[]
 ├── addChild
 ├── removeChild
 └── containsChild
```

このスライスが通った後に、CompositionのNodeStoreとGroup変換アダプタへ進む。
