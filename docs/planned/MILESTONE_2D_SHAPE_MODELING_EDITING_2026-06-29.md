# MILESTONE: 2D Shape Modeling Editing

作成日: 2026-06-29  
ID: `M-LE-3`

## Goal

`ArtifactShapeLayer` を単なる primitive layer や AE 風 shape layer の延長ではなく、
「2D モデリング対象」として直接編集できる段階まで引き上げる。

この milestone では、shape を

- `primitive`
- `editable path`
- `operator stack`

の 3 層で扱い、viewport 上での頂点編集と modifier 的な編集を両立させる。

## Why Now

現状の repo には、すでに次の土台がある。

- `ArtifactShapeLayer` に `customPolygonPoints` / `customPathVertices` / open-close / tangent 情報がある
- shape operator (`Trim Paths`, `Offset Paths`, `Rounded Corners`, `Twist`, `Repeater` など) が実装済み
- `ArtifactRenderLayerWidgetv2` と overlay に shape path の直接編集経路がある
- `Composition Editor` には `Modal.Mask` / `Modal.Pen` という modal input routing の先行例がある

一方で、今の shape 編集は

- layer solo view と property editing に寄っている
- shape 専用の mode routing が弱い
- multi-vertex selection や segment insert など、モデリングらしい操作が未整理
- `shape.type` のような structural concern が property enum に寄りすぎている

ため、ここから先は「機能追加」よりも「編集モデルの昇格」として扱う方がよい。

## Scope

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
- `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- `Artifact/include/Layer/ArtifactShapeLayer.ixx`
- 必要なら `docs/WIDGET_MAP.md`

## Non-Goals

- Illustrator 級の完全な vector authoring system
- paint / brush / bitmap retouch との統合
- 3D mesh editing
- shape/mask/time-addressable animation を同時に再設計すること
- Diligent / DX12 backend の低レベル変更

## Product Direction

目指す体験は、AE の shape layer を土台にしつつ、
Blender / Illustrator / Spine の「編集対象としてのパス」に近い操作感を viewport に持ち込むこと。

重要なのは、shape をただの property 群としてではなく、
「現在選んでいる幾何」を直接触る対象として扱うこと。

## Editing Model

### 1. Primitive Layer

- `Rect`, `Ellipse`, `Star`, `Polygon`, `Line` などの primitive として生成
- 初期値編集は引き続き property / gizmo / solo view から可能
- ただし primitive は最終形ではなく、editable path への入口として扱う

### 2. Editable Path

- `Convert To Editable Path` で `customPathVertices` ベースの path へ昇格
- anchor / tangent / segment を直接編集する主対象
- open / close / smooth / corner / broken tangent を path レベルで扱う

### 3. Operator Stack

- shape operator を「AE 風 effect」ではなく 2D モデリング用 modifier stack として扱う
- base path を壊さず、非破壊で `Offset`, `Rounded`, `Twist`, `Repeater`, `Trim` などを積む
- stack order を viewport / inspector から読めるようにする

## Phase Plan

### Phase 1: Shape Edit Mode And Selection Grammar

目的:
- shape 専用の modal editing を composition editor に定義する

実装項目:
- `Shape Edit` mode を追加する
- shape layer 選択時に vertex / segment / tangent hit-test を優先できる routing を用意する
- current layer / current shape path / selected vertices の ownership を固定する
- gizmo / pan / playback scrub / mask mode との優先順位を明文化する

完了条件:
- shape layer 選択時に viewport から直接 shape editing へ入れる
- tool state が mask editing や transform gizmo と競合しにくい
- selected path / hovered segment / hovered tangent の状態が分かる

### Phase 2: Vertex Modeling Essentials

目的:
- 本格的な 2D モデリングとして最低限必要な path editing を揃える

実装項目:
- vertex select / marquee select / additive select
- vertex move / delete / duplicate
- segment 上 click で vertex insert
- open / close toggle
- multi-vertex translation

完了条件:
- simple polygon/path を viewport だけで組み替えられる
- segment insert が undo 可能
- path を壊さず open/close を切り替えられる

### Phase 3: Tangent And Topology Controls

目的:
- Bezier 編集を property 編集ではなく modeling 操作として完成させる

実装項目:
- `smooth / corner / broken` tangent mode
- mirrored tangent と independent tangent の切替
- handle only selection
- selected anchor から handle create
- tangent reset / flatten / align

完了条件:
- curve のニュアンスを viewport 上だけで詰められる
- anchor editing と handle editing の見た目が分離される
- tangent mode が undo/redo で壊れない

### Phase 4: Primitive To Editable Path Conversion

目的:
- primitive と editable path を対立させず、昇格可能な workflow にする

実装項目:
- `Convert To Editable Path`
- primitive parameter の初期形状を custom path へ焼き付け
- conversion 後の bounds / anchor / gradient / stroke の整合維持
- 既存 project load/save で custom path を自然に扱えることを確認

完了条件:
- rectangle / ellipse / star / polygon を path 編集へ自然に移行できる
- convert 後も描画と保存が破綻しない

### Phase 5: Modifier Stack UX

目的:
- shape operator を 2D モデリングの modifier として使いやすくする

実装項目:
- operator list の可視化
- reorder / toggle / remove
- path base と processed result の見分けを overlay に出す
- `Offset Paths`, `Rounded Corners`, `Pucker/Bloat`, `Twist`, `Repeater` を優先整理

完了条件:
- base path と processed path の区別が分かる
- stack order の変更が編集体験として理解しやすい
- non-destructive workflow が明快になる

## UX Rules

- shape editing は `Mask editing` の延長ではなく別 mode として扱う
- geometry editing と parameter editing を混ぜすぎない
- overlay は情報量を増やしすぎず、選択中の幾何だけを強く見せる
- property editor は補助面とし、主編集面は viewport とする
- operator は effect list ではなく modeling stack として見せる

## Technical Guardrails

- 新規 global signal routing は増やさない
- shell / controller / overlay の責務境界を維持する
- `QImage` を新しい hot path 編集状態に入れない
- `QPainter` / `QPainterPath` 依存の既存実装は当面利用してよいが、編集 state と raster path を混同しない
- `.ixx` 変更は最小限に留め、まず `.cppm` と既存 class 内で吸収できるか確認する

## Suggested Implementation Order

1. Phase 1 `Shape Edit Mode`
2. Phase 2 `Vertex Modeling Essentials`
3. Phase 4 `Convert To Editable Path`
4. Phase 3 `Tangent And Topology Controls`
5. Phase 5 `Modifier Stack UX`

理由:
- 先に mode routing を固めないと、頂点機能を増やすほど操作事故が増える
- conversion を早めに入れると、primitive layer と editable path の二重管理が整理しやすい
- operator stack polish は、base path editing が安定した後の方が判断しやすい

## Validation Checklist

- shape layer を選んだとき、viewport から shape edit mode に入れる
- vertex を追加・移動・削除・複数選択できる
- segment 上 insert が安定する
- tangent mode の違いが視覚的に分かる
- primitive から editable path に変換できる
- shape operator stack を壊さず並び替えられる
- save/load 後も custom path と operator が復元される

## Related Docs

- `docs/done/MILESTONE_LAYER_EDIT_2026-04-25.md`
- `docs/done/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md`
- `docs/COMPOSITION_EDITOR_CONTRACT.md`
- `docs/planned/SHAPE_LAYER_ANALYSIS_2026-04-17.md`

## Next Slice

最初の実装 slice は Phase 1 に絞る。

- `Shape Edit` mode の導入
- selected shape path / vertex selection state の ownership 整理
- vertex / segment / tangent overlay の hit-test 優先順位整理

この slice が通ると、その後の vertex insert や convert-to-editable-path を安全に足しやすくなる。

## Current Progress

- `Shape Edit` mode を `EditMode` と tool routing に追加済み
- `ArtifactCompositionEditor` の tool menu に `Shape modeling` の入口を追加済み
- `ArtifactRenderLayerWidgetV2` 側の mode readout でも `Shape` を表示できる状態
- `ArtifactToolService` で `Shape` を `ToolType::Shape` に結び直した
- `ArtifactRenderLayerWidgetV2` の shape context menu で polygon / path の hover summary と path vertex insert を出せるようにした
- `ArtifactCompositionEditor` の shape selection detail で open / closed を読めるようにした
- `ArtifactRenderLayerWidgetV2` で path segment hover を拾い、segment からの vertex insert に追従できるようにした
- `ArtifactRenderLayerWidgetV2` の tooltip / context menu で path segment hover を明示できるようにした
- `ArtifactRenderLayerWidgetV2` で path segment を番号付きで識別できるようにした
- `ArtifactRenderLayerWidgetV2` で path vertex duplication をできるようにした
- 次は shape layer 選択時の vertex / segment / tangent selection grammar を実装する
