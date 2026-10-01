# GroupContainer 移行計画

**作成日:** 2026-08-27  
**最終更新:** 2026-10-01
**ステータス:** Phase 0 / 1 実装済み、Phase 2 は独立GroupContainerの作成・保存・Timeline表示・switch列まで部分実装。展開状態の永続化は追加実装。Render Boundary移行は未着手
**対象:** `ArtifactGroupLayer` から Composition 所有の独立 Container への移行

## Update 2026-08-30 — current implementation reconciliation

- `CompositionNode`、`ContainerNode`、`GroupContainerNode`、`CompositionNodeStore` は実装済み。NodeStoreはID重複、自己親、存在しない親、親子cycleを拒否し、parent IDとorderからchild順を再構築する。
- `ArtifactAbstractComposition` はlayer追加・階層変更・JSON保存／復元とNodeStoreを同期し、既存`childLayersOf()`を残している。`GroupContainerNode`はLayer非継承で、output mode、active child、enabled、opacity、blendをNode propertyとして往復する。
- `ArtifactGroupLayer` は描画・UI・既存プロジェクト互換の正規ownerのまま、NodeStoreとの双方向アダプタを持つ。これはPhase 2の互換層であり、独立したComposition兄弟Containerへ置換済みではない。
- GPU-native Render Boundary、Factory／UI／Undo／ExportのContainer実体化、旧`ArtifactGroupLayer`の廃止は未実装。ビルド・group JSON round-trip・Preview／Export parityはruntime未検証。

## Update 2026-09-16 — standalone creation and Timeline surface

- 選択レイヤーからLayer非継承の`GroupContainerNode`を直接作成するComposition APIとUndo経路を追加した。子の所属はNodeStoreのparentIdで保持し、既存レンダー経路の`parentLayerId`は変更しない。
- Timelineの行descriptorにNode IDとContainer行種別を追加し、独立Containerの名称、子数、展開／折り畳み、選択、子レイヤー行を表示する。右クリックから名称変更、子レイヤー選択、グループ解除をUndo対応で実行できる。
- Timeline右ペインには子レイヤーのin/out和集合をContainerバーとして表示する。Containerバーはレイヤー操作対象にしない。
- `compositionNodes`へのJSON保存／復元経路を利用するためContainerと子parentIdは永続化される。ビルド、runtime表示、保存後再起動の往復確認は未実施。
- Render Boundary、ネストContainer、Inspector、Export移行は引き続き未実装。

## Update 2026-10-01 — 展開状態の永続化と開示Trianglesのヒット領域

### 実施内容

- `ArtifactAbstractComposition::groupContainerExpanded()` / `setGroupContainerExpanded()` を追加し、`GroupContainerNode` の `properties["expanded"]` を正式な保存先とした。生成時に `expanded=true` を書いていたが読み戻す経路が無く、再起動で常に全展開に戻っていた。
- 左ペインの行生成は `expandedByGroupKey` のセッション内上書きを優先し、未上書きのときだけノードから読む。
- Container行のクリック処理を、開示Triangleのヒット矩形内だけに限定した。従来はContainer行のどこをクリックしても折り畳み／展開がtoggleされ、コンテナ行の選択が事実上不可能になっていた。レイヤー行の16px規律（`ArtifactLayerPanelWidget.cppm` の `disclosureRect`）と一致させている。
- `SetGroupContainerExpandedCommand` を追加し、展開／折り畳みをUndo可能にした。あわせて `RemoveGroupContainerCommand` はundo時に `expanded` を復元する（`createGroupContainer` は `expanded=true` でseedするため、そのままだとUngroup→Redoでグループが勝手に開く）。

### Container switch（2026-10-01 追加）

**設計判断: 子上方向のみ（Aggregate + Write-through）**

- Containerは `ArtifactAbstractLayer` を継承せず独自の switch 状態を持たない。`properties` に switch を保存するとCoreの `hasParent()` / `childLayersOf()` / `shouldEvaluateLayer()` と状態モデルが二重化する。
- そのため Container switch は**子の集約値であり、書き込みは子への伝播**とした。逆方向（子の個別操作を Container に反映して状態に書き戻す）は採用しない。
- 集約値はCoreの単一責務として `ArtifactAbstractComposition::groupContainerSwitchStates()` に集約した。全4 switch を1回の子走査で返すため、paint がセルごとに Core を叩かない。状態は `On`（全子オン）/ `Mixed`（一部オン）/ `Off`（全子オフ）。空グループは `Off`。
- 書き込みは既存の `SetLayerVisibilityCommand` / `SetLayerLockCommand` / `SetLayerSoloCommand` / `SetLayerShyCommand` を `MacroUndoCommand` で束ねて実行する。新しい signal/slot や新しい永続化先は追加していない。
- ロック済み子は非lock switchの対象から除外する（レイヤー行の既存ルールと同じ）。全子ロック時はロック拒否の通知を出し、lock switch のみ有効。
- クリック時のトグルは集約値で決める: `Off` なら全子オン、それ以外は全子オフ。
- Audio列(3)とPick Whip列(5)は Container レベルの意味を持たないため無効のまま描画する。

### 左ペインの現状と残るギャップ

Container行は「集約行」として成立し、switch列はレイヤー行と揃った。残る未実装点:

| 項目 | 状態 |
| --- | --- |
| Container switch列（Visible / Lock / Solo / Shy） | 実装済み。集約表示 + 子への書き戻し、Macro Undo、ロック拒否の適用 |
| ドラッグによるContainerへのparent link付け | 不可。すべてのdrop経路が `visibleRows[i].layer != nullptr` を要求し、Container行は `layer == nullptr`。ContainerはNodeStoreの親なので、別のNodeStore `setParent` 経路を新設する必要がある |
| 入れ子Container | 未実装。`appendNode` は container の子に固定depth 1で渡す |
| .Render Boundary | 未実装。`createGroupContainer` は `parentLayerId_` を触らないため `hasParent()` / `childLayersOf()` / `shouldEvaluateLayer()` から所属が見えない。これはswitch実装の前提条件でもある |
| 子からContainerへの状態逆反映 | 採用しない（子上方向のみで確定） |

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
