# Milestone: CompositionCleanup (2026-07-14)

**ステータス:** In Progress

## Goal

Compositionの完成前に、意図しない数pxのずれ、余白の不統一、可読性不足を検出し、具体的な修正候補を安全に適用できるようにする。

UI表示名は `Composition Cleanup`、コード上の機能名は `CompositionCleanup` に統一する。

## Responsibility

`CompositionCleanup` は完成後または任意時点の静的診断を担当する。ドラッグ中の吸着を担当する既存Snap / Smart Guidesとは分離する。

```text
immutable composition snapshot
  -> CompositionCleanup analyzer (read-only)
  -> diagnostic + optional fix proposal
  -> Composition Editor overlay preview
  -> existing property/edit command
  -> single Undo/Redo entry
```

- AnalyzerはCompositionやLayerを直接変更しない。
- 修正は既存のTransform、Text、Color、Alignment経路だけを使う。
- 新規のグローバルsignal/slot接続や中央イベント配線は追加しない。
- ビューポート描画、export結果、保存データを診断だけで変更しない。

## MVP Checks

| ID | Check | Detection | Proposed action | Auto-fix policy |
|---|---|---|---|---|
| `cleanup.uneven-margin` | 余白が不均等 | 対応する外周またはグループ内余白の差が許容値を超える | `左へ4px移動`、`左右余白を24pxに統一` | Preview後に適用可 |
| `cleanup.clustered-elements` | 似た位置に要素が密集 | 可視bounds間距離が基準spacing未満 | `要素間隔を12pxへ拡張` | Preview後に適用可 |
| `cleanup.edge-risk` | 画面端ぎりぎり | 可視boundsがaction/title safeまたは設定marginを侵す | `内側へ8px移動` | Preview後に適用可 |
| `cleanup.low-title-contrast` | タイトルと背景のコントラスト不足 | テキストの代表色と直下背景sampleの比率が閾値未満 | 文字色候補、背景調整候補 | 候補選択のみ |
| `cleanup.multiple-focal-points` | 主役が複数 | 面積、中心距離、contrast、彩度等のsaliency上位が拮抗 | 対象をハイライトして説明 | 警告のみ |
| `cleanup.text-too-small` | 小さすぎる文字 | 最終出力px換算のglyph heightまたはfont sizeがpreset閾値未満 | `18pxへ変更` | Preview後に適用可 |
| `cleanup.near-center` | 重要要素が中央から微妙にずれる | 幾何中心またはvisual centroidが中央から小さな範囲だけ外れる | `左へ4px移動`等 | Preview後に適用可 |
| `cleanup.spacing-drift` | レイヤー間隔が1〜2pxずつ違う | 3個以上の同系列要素でgapのばらつきが許容値を超える | median gapへ均等分布 | Preview後に適用可 |

## Geometry Contract

診断はLayerのTransform矩形だけに依存しない。優先順位は次の通り。

1. mask / effect適用前の編集用visible alpha bounds
2. text layoutの実glyph bounds
3. shape / vectorの実path bounds
4. 利用できない場合のみtransformed layer bounds

PNGの透明余白を実コンテンツの余白として数えない。アルファbounds生成のために新しい`QImage`変換やGPU readbackを暗黙に追加せず、既存のCPU surfaceまたは明示的な解析snapshotを使う。

回転レイヤーはaxis-aligned bounding boxだけで過剰警告しないよう、oriented boundsまたは可視輪郭からComposition端までの最短距離を使う。

## Visual Center

中央判定は2種類を保持し、診断文に根拠を出す。

- Geometric center: visible boundsの中心
- Visual center: alphaと知覚輝度で重み付けしたcentroid

visual centroidは提案に使えるが、意図的な非対称レイアウトを壊す可能性があるため、初期版では一括自動適用しない。

## Contrast Sampling

タイトルのcontrastはComposition Background Colorだけで判定せず、対象テキスト直下の合成済み背景を複数点sampleする。動画やgradient背景では中央値と低contrast側percentileを併記する。

- text fill / stroke / shadowを含む最終的な可読性を評価する。
- scene-linear値を表示用transferへ変換した後の相対輝度で比較する。
- 背景sampleを取得できない経路では結果を`approximate`として明示する。
- 文字色や背景色の修正は既存の承認済みcolor picker / property経路を使い、`QColorDialog`を追加しない。

## Focal Point Heuristic

`主役が複数存在`は客観的エラーではない。次の特徴量からsaliency候補を説明付きで出すが、自動修正しない。

- visible area
- Composition centerへの近さ
- 周辺とのluminance / color contrast
- text hierarchy、layer role、selection metadata
- opacityと遮蔽後の可視率

人物検出や意味推論をMVPの必須条件にしない。同点に近い上位候補をComposition Editor上で番号表示し、ユーザーが意図を判断できるようにする。

## Fix Proposal Contract

各候補は少なくとも次を持つ。

```text
diagnosticId
severity
layerIds
frameTime
message
confidence
beforeValues
afterValues
previewGeometry
fixKind
```

- 数値修正は`左へ4px移動`のように方向と差分を表示する。
- `Apply`前にbefore / afterをghost overlayで比較できる。
- staleな候補を適用しない。生成後に対象LayerまたはCompositionが変わった場合は再解析する。
- no-op候補をUndo履歴へ追加しない。
- 単一候補は1 Undo、一括適用は全変更をまとめた1 Undoとする。
- 初期版の一括適用は高confidenceなgeometry修正だけに限定する。

## UI

### Cleanup panel

- `Analyze Current Frame`
- category / severity filter
- 問題、対象Layer、具体的な差分、confidence
- `Preview` / `Apply` / `Ignore`
- `Apply Safe Fixes`

常時バッジをComposition Editorへ追加しない。解析要求時または結果表示中だけoverlayを出す。

### Existing UI reuse

- `AlignmentWidget`: align / distributeの編集ロジックを再利用
- Safe Area overlay: edge-riskの根拠表示を再利用
- `LayoutSnapshotCommand`: geometry修正のUndo/Redo境界を再利用
- Problem View: export preflight等へ警告を露出する場合のdiagnostic文法を再利用

Composition Cleanupの制作向け候補を開発用Frame Diagnosticsへ混在させない。

## Defaults

初期値はComposition解像度に対する比率から算出し、UIではpxへ丸めて表示する。

- center near-miss band: 長辺の`0.1%〜1.0%`
- spacing drift tolerance: `max(1px, 長辺の0.05%)`
- edge margin: 既存Safe Area設定を優先、未設定時は短辺の`5%`
- minimum text size: 出力preset別。初期汎用値は最終出力`18px`

固定pxだけで4Kと小型Compositionを同じ判定にしない。

## Phases

### Phase 1: Deterministic geometry checks

- [x] uneven margin（Compositionの半分以上を占める要素）
- [x] edge risk
- [x] near center
- [x] spacing drift（横方向の同列）
- [ ] immutable analysis resultとstable diagnostic ID

### Phase 2: Preview and command application

- [x] 既存Info Overlayによる候補プレビュー
- [x] 既存render-path ghost previewによる候補bounds表示
- [x] concrete delta表示
- [x] stale proposal検証
- [x] existing command pathによるApply / Undo / Redo

### Phase 3: Text and contrast

- [x] 最小font size警告（18px、Inspectorへ誘導）
- [ ] final output px換算
- [ ] composited background sampling
- [x] Composition Background Colorを使う近似contrast警告
- [ ] contrast修正候補とapproximate状態のUI

### Phase 4: Heuristic composition review

- [x] clustered elements（近接中心の複数ペア、警告のみ）
- [x] multiple focal points（large / near-centerの警告のみ）
- [ ] confidenceと説明可能な根拠
- [ ] export preflightへの任意接続

## Definition of Done

- 同一snapshotから同一順序・同一IDの診断が生成される。
- 1〜2pxのspacing driftを検出し、具体的な均等配置候補を提示できる。
- 透明PNGの外形ではなくvisible alpha boundsで余白を測れる。
- 具体的な移動量をApply前にComposition Editor上で確認できる。
- Apply後の全変更を1回のUndoで復元できる。
- 主役競合や低confidence候補は自動修正されない。
- 診断だけではComposition、保存データ、render/export結果が変化しない。
- QtCSS、`QColorDialog`、`QPainter`合成、新規グローバルsignal/slotを追加しない。

## Likely Implementation Areas

実装開始前に子リポジトリ変更の明示承認と、現在のbranch整合を確認する。

- Core analysis contract / immutable result
- Composition snapshot adapter
- Composition Editor cleanup panel and overlay
- existing Alignment / Safe Area / Undo command adapters
- optional Problem View / render preflight adapter

公開module interfaceの変更は最小化し、実装依存は`.cppm`側へ閉じる。新規moduleが必要な場合はCMakeのmodule登録ルールと循環依存を先に確認する。

## Related Documents

- `docs/planned/MILESTONE_COMPOSITION_EDITOR_FIGMA_LIKE_OVERLAY_2026-04-21.md`
- `docs/planned/MILESTONE_COMPOSITION_EDITOR_FIGMA_LIKE_OVERLAY_PHASE1_2026-04-21.md`
- `docs/analysis/MOTION_GRAPHICS_AD_PRODUCTION_THINKING_MEMO_2026-05-28.md`
- `docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md`
- `docs/DOC_LIFECYCLE.md`

## 2026-07-25 実装監査

- `ArtifactCompositionEditor.cppm` に `analyzeCompositionCleanup()`、各 MVP diagnostic ID、候補 preview、具体的 delta 表示、Apply 経路が存在する。
- geometry／edge／center／spacing／cluster／focal point／minimum text size／近似 contrast の検出と、既存編集経路を用いた候補適用・Undo 境界はコード上で確認できる。
- immutable analysis result と stable diagnostic ID の明示的な契約は未確認である。現在の解析結果は editor 実装内の候補構造体に閉じている。
- final output px 換算、合成済み背景の複数点 sampling、approximate 状態を含む contrast 修正候補、confidence／説明可能な根拠、export preflight 接続は未完了である。
- よって本マイルストーンは `In Progress` のままとし、MVP の editor 実装だけで Definition of Done 全体を満たしたとは判定しない。
