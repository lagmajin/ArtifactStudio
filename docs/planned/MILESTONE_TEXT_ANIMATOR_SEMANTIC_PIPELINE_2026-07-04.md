# MILESTONE: Text Animator Semantic Pipeline

> **Supporting slice:** [`MILESTONE_TEXT_LAYER_GPU_EDIT_ANIMATION_2026-07-16.md`](./MILESTONE_TEXT_LAYER_GPU_EDIT_ANIMATION_2026-07-16.md) の WP-5。AE系の問題を解消する selector／modifier 意味論の個別仕様を保持する。

**ステータス:** In Progress

**Date:** 2026-07-04

## Purpose

ArtifactStudio の Text Animator を AE 互換機能の追加に留めず、AE で曖昧になりやすい
対象単位、適用順、文字編集後の追従、complex script、診断性を明示的な契約で扱う。

既存の `TextLayoutContract` と shaping backend を維持しながら、現在の
`RangeSelector + WigglySelector + AnimatorProperties` tuple と
`GlyphItem` 直接変更を、意味論を持つ評価パイプラインへ段階移行する。

## Current Baseline

実装済み:

- Percentage / Index / Cluster / Line / Tag selector
- Range shape、ease、wiggly、regex entry
- position / scale / rotation / opacity / skew / tracking / z
- fill / stroke / blur
- animator stack、serialization、property keyframe
- selector overview、weight heatmap、logical / visual order 表示
- horizontal / vertical writing と layout metadata

未完成:

- Selector Stack と Modifier Stack の独立モデル
- source text span に対する regex / tag / script selector
- 文字編集後も意味的対象を保つ stable token
- selector weight の合成演算
- immutable evaluation result
- order / anchor grouping の評価接続
- selector 単位の cache / diagnostics

## Design Goals

1. selector は「どこへ、どれだけ効くか」だけを返す。
2. modifier は selector の実装や text order を知らない。
3. selector は glyph ではなく source token / layout unit を評価する。
4. grapheme cluster、emoji ZWJ、combining mark、ligature を途中で分割しない。
5. logical order、visual order、writing mode を推測せず明示する。
6. text edit 後の target recovery を index 一致だけに依存させない。
7. 評価結果を描画用 glyph へ直接蓄積せず、再評価可能な中間結果にする。
8. CPU / GPU renderer は同じ evaluated instance contract を消費する。

## Non-Goals

- AE の内部表現や不透明な例外挙動を完全再現すること
- 最初の段階で HarfBuzz、GPU instance、縦書きの全機能を完成させること
- Property Editor、Timeline、renderer を一括置換すること
- 既存プロジェクトの animator JSON を破壊すること

## Architecture

```text
SourceTextSnapshot
  -> TextTokenTable
  -> Shaping / TextLayoutContract
  -> SelectionDomain
  -> SelectorStack
  -> WeightComposition
  -> ModifierStack
  -> EvaluatedTextInstances
  -> CPU / GPU Renderer
```

### Responsibility Boundary

- `TextTokenTable`
  - source text と意味的 token identity を所有する
- `TextLayoutContract`
  - shaping、cluster、line、bidi、vertical metadata を所有する
- `SelectionDomain`
  - token / cluster / word / line / paragraph と glyph mapping を提供する
- `SelectorStack`
  - unit ごとの正規化 weight を生成する
- `WeightComposition`
  - 複数 selector の weight を合成する
- `ModifierStack`
  - 合成 weight から transform / appearance delta を生成する
- `EvaluatedTextInstances`
  - renderer が消費する immutable snapshot

## Core Contracts

### Stable Text Identity

`script:index:codepoint` は diagnostic ID として残せるが、stable identity には使わない。

```cpp
struct TextTokenId {
  uint64_t value = 0;
};

struct TextToken {
  TextTokenId id;
  int logicalStart = 0;
  int logicalLength = 0;
  QString sourceText;
  QString scriptTag;
  QStringList tags;
};

struct TextTokenTable {
  uint64_t sourceRevision = 0;
  QVector<TextToken> tokens;
};
```

token ID は保存済み document identity と edit transaction から継承する。
再読み込み時は source span、内容、前後 token、script を使う deterministic recovery を行う。
完全一致できない場合は新規 ID を発行し、曖昧な target recovery を黙って確定しない。

### Selection Domain

```cpp
enum class TextSelectionUnit {
  Token,
  GraphemeCluster,
  GlyphCluster,
  Word,
  Line,
  Paragraph,
  Tag,
  ScriptRun,
  RubyBase,
  RubyText,
  TateChuYokoRun,
};

enum class TextOrder {
  Logical,
  Visual,
};

struct TextSelectionDomain {
  TextSelectionUnit unit;
  TextOrder order;
  QVector<TextTokenId> unitTokens;
  QVector<QVector<int>> unitToGlyphs;
};
```

通常UIでは `GlyphCluster` を advanced / diagnostic 扱いにし、
編集可能な既定単位は `GraphemeCluster` とする。

### Selector

```cpp
enum class SelectorKind {
  Range,
  Regex,
  Tag,
  Script,
  Direction,
  Random,
  Field,
};

enum class WeightCombineMode {
  Replace,
  Add,
  Subtract,
  Multiply,
  Min,
  Max,
};

struct SelectorResult {
  TextSelectionUnit unit;
  TextOrder order;
  QVector<float> weights;
  QString diagnosticLabel;
};
```

Selector は `SelectorResult` を返し、glyph state を変更しない。
全 weight は原則 `0..1`。範囲外値を必要とする modifier は modifier 側の
strength / remap curve で表現する。

### Regex Selector

regex は source text に対して評価する。

1. regex match を logical source span として取得
2. span と交差する token / grapheme cluster を求める
3. cluster の途中だけが match しても cluster 全体へ正規化
4. logical-to-visual map で表示順へ投影
5. invalid regex は weight 0 と structured diagnostic を返す

token ID、cluster ID、index を連結した diagnostic string は検索対象にしない。

### Modifier

```cpp
enum class TextModifierKind {
  Transform,
  Opacity,
  Tracking,
  Fill,
  Stroke,
  Blur,
  Noise,
  Formula,
};

struct TextModifierDelta {
  QPointF position;
  float scale = 1.0f;
  float rotation = 0.0f;
  float opacity = 1.0f;
  float tracking = 0.0f;
  float blur = 0.0f;
};
```

Modifier は base layout と合成 weight から delta を返す。
tracking の累積方向は `TextOrder` と writing mode から決め、
glyph vector の格納順に依存させない。

### Evaluation Result

```cpp
struct EvaluatedGlyphInstance {
  int glyphIndex = -1;
  QPointF position;
  float scale = 1.0f;
  float rotation = 0.0f;
  float opacity = 1.0f;
  float skew = 0.0f;
  float z = 0.0f;
  float blur = 0.0f;
};

struct TextAnimatorEvaluation {
  uint64_t sourceRevision = 0;
  uint64_t layoutRevision = 0;
  QVector<EvaluatedGlyphInstance> instances;
  QVector<SelectorResult> selectorResults;
};
```

base `GlyphItem` は変更せず、評価結果を別snapshotとして返す。
diagnostic build / editor preview では selector result を保持し、
final render では破棄可能にする。

## Stack Model

Animatorは次の構造を持つ。

```text
Animator
  Selectors
    Range
    Regex
    Field
  Weight Composition
    Multiply
  Modifiers
    Transform
    Fill
```

- Selector順序は明示し、保存する
- selectorごとに enable / invert / strength / combine mode を持つ
- Modifierは複数追加可能
- Animator同士の適用順も保存する
- legacy tupleは selector 1個 + modifier group 1個として読み替える

## Serialization Compatibility

- 現行 `text.animators[]` JSON はそのまま読み込む
- legacy `range` は Range Selector node へ変換する
- legacy `wiggly` は Random Selector nodeへ変換する
- legacy `properties` は必要な Modifier node群へ変換する
- 新形式には `schemaVersion` を付ける
- 旧形式へのlossless保存が可能な間は旧形式を維持する
- 新機能を使った時だけ新schemaへ昇格する

## UI Contract

通常の Inspector:

```text
Animator 1
  Selectors
    Range
    Regex
  Modifiers
    Transform
    Fill
```

- target unit と order はselector headerに短く表示
- diagnostic文字列をText groupへ常時大量表示しない
- script / token / logical-visual mapping はSelector Debuggerへ集約
- invalid regex、lost token target、cluster coercionだけは該当selector直下へ表示
- TimelineにはAnimator単位の開閉とkeyframe laneを出し、診断badgeを常設しすぎない
- 新しいグローバル signal / slot 経路は追加せず、既存property/service経路を使う

## Cache Contract

cache key:

- source revision
- layout revision
- selector stack revision
- modifier stack revision
- evaluation time
- writing mode

invalidate:

- text edit: token recovery以降を無効化
- font / paragraph / box change: layout以降を無効化
- selector change: selector以降を無効化
- modifier change: modifier以降を無効化
- renderer backend change: evaluated resultは再利用可能

## Migration Phases

### Phase 0: Contract Lock

- type名、unit、order、combine mode、revision契約を確定
- legacy JSON mappingを確定
- `SelectorOrder` / `AnchorPointGrouping` の未接続状態を明記

完了条件:

- glyph vector indexに依存しない評価APIが定義される
- legacy projectを無変更で読める設計になる

### Phase 1: Pure Selector Evaluation

- Range Selectorをpure evaluatorへ移す
- source span上のRegex Selectorを実装
- weight compositionを実装
- 現行apply pathへadapterで結果を渡す

完了条件:

- selector評価がglyphを変更しない
- regexがsource textへ作用する
- heatmapとrenderが同じweightを使う

### Phase 2: Stable Token Recovery

- TextTokenTableとsource revisionを導入
- edit transactionでIDを継承
- reload recoveryとambiguous diagnosticを導入

完了条件:

- 前方挿入だけで後続targetが全てずれない
- recovery不能時に黙って別文字へ作用しない

### Phase 3: Modifier Stack

- transform / opacity / tracking / fill / stroke / blurをnode化
- immutable `TextAnimatorEvaluation` を返す
- legacy tuple adapterを維持

完了条件:

- Selector StackとModifier Stackを独立に並べ替えられる
- base glyph layoutを再生成せずmodifierを再評価できる

### Phase 4: UI Migration

- Property EditorをAnimator / Selectors / Modifiers階層へ整理
- Selector Debuggerへ診断項目を移す
- Timeline laneとproperty keyframe pathを新schemaへ対応

完了条件:

- 作用対象、順序、合成方法をInspectorから説明できる
- 通常Text propertyが診断文字列で埋まらない

### Phase 5: Renderer and Performance

- CPU rendererをevaluated instance contractへ移行
- GPU instance bufferを同じcontractへ接続
- selector / modifier cacheを導入

完了条件:

- CPU / GPUで同じselector resultを使用する
- semantic contractを変えずにGPU化できる

## Acceptance Scenarios

1. `A👨‍👩‍👧‍👦B` のemoji途中をselectorが分割しない。
2. Arabicのlogical rangeがvisual orderでも同じ語を対象にする。
3. combining mark付き文字を一単位として移動・着色する。
4. 先頭へ文字を挿入しても既存tag/token targetが可能な限り維持される。
5. regex `\\d+` がsource textの数字列を選び、token ID文字列を検索しない。
6. 縦書きのtrackingがglyph格納順ではなくcolumn flowへ従う。
7. rubyは既定でbase selectorへ追従せず、明示targetで選択できる。
8. selector heatmapとfinal renderのweightが一致する。
9. legacy animator projectが見た目を変えず読み込める。
10. invalid regexと曖昧なtoken recoveryがUIとdebug outputで説明される。

## Guardrails

- grapheme clusterを壊す通常selectorを追加しない
- `GlyphItem` indexを永続identityとして保存しない
- diagnostic表示と編集UIを同じproperty列へ無制限に混在させない
- selector semantics確定前にGPU専用表現を正規モデルにしない
- Qt composition / `QPainter` fallbackを新しい本流として増やさない
- QtCSS、`QColorDialog`、新規グローバルsignal/slotを導入しない

## First Implementation Slice

子リポジトリ編集の承認後、最初の変更は次に限定する。

1. `SelectorEvaluationContext` と `SelectorResult` をCoreへ追加
2. 現行Range Selectorをpure evaluatorへ移す
3. Regex Selectorをsource span評価へ変更
4. legacy `applyAnimatorStack()` はadapterとして維持
5. heatmapを同じ`SelectorResult`へ接続

このスライスではstable token、UI再編、GPU pathは変更しない。

## Current Progress

- 2026-07-04:
  - `SelectorEvaluationContext` / `SelectorResult` を追加
  - Range Selector をglyph変更から分離して評価できる入口を追加
  - Regex Selector をsource text上のlogical span評価へ変更
  - regex matchをcluster全体へcoerceする初期処理を追加
  - legacy `applyAnimatorStack()` を維持したsource-aware overloadを追加
  - render pathとselector heatmapを同じpure selector評価へ接続
  - stable token、selector composition、immutable evaluationは未着手
