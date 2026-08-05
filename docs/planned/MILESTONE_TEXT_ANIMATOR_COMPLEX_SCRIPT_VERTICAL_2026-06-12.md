> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_TEXT_ANIMATOR_SYSTEM_2026-03-25.md](MILESTONE_TEXT_ANIMATOR_SYSTEM_2026-03-25.md)

# MILESTONE: Text Animator Complex Script And Vertical Writing

## Static Audit (2026-07-25)

`TextLayoutContract`、`TextShapingRequest/Result`、Qt/HarfBuzz backend interface、script/bidi/cluster/line metadata、vertical writing metadata は実装側に存在する。`GlyphItem` に cluster、line、selector tag、stable token id があり、Text Gizmo／Inspector／debugState 側にも writing mode や selector 情報の表示経路がある。

- 実装済みまたは部分実装: horizontal/vertical の layout contract、script run、bidi run、grapheme相当の cluster span、line/vertical column、ruby、tate-chu-yoko、punctuation、bracket、kinsoku metadata、selector tag/regex と heatmap/debug 表示の入口。
- 未確認または未達: Hangul/Arabic/Hebrew/Thai/Indic/emoji ZWJ/ligature の実ランタイム検証、clusterを壊さない全selector／source編集同期、stable token の永続性、完全な glyph cluster map、縦書きのruby・kinsoku・tate-chu-yoko実描画、selector stack と modifier stack の独立した編集UI。
- `QtShapingBackend` と `HarfBuzzShapingBackend` のインターフェースは存在するが、全複雑文字をHarfBuzz本線で処理すること、backend差異の品質比較、full vertical rendering の完了は静的には証明できない。

判定: semantics／metadata の基盤と表示入口は大きく前進しているが、Success Criteria の複雑文字と縦書きの実動作保証は未検証。現時点は contract-first の部分実装として扱う。

> **Supporting slice:** [`MILESTONE_TEXT_LAYER_GPU_EDIT_ANIMATION_2026-07-16.md`](./MILESTONE_TEXT_LAYER_GPU_EDIT_ANIMATION_2026-07-16.md) の WP-5／WP-7。complex script／縦書き／多言語の個別仕様と履歴を保持する。

**Date**: 2026-06-12

---

## Purpose

`Text Animator` を、日本語だけでなく Hangul, Arabic, Hebrew, emoji ZWJ, ligature-heavy script を含む
複雑文字系へ広げつつ、縦書きまで含めて破綻しない設計へ整理する。

このマイルストーンは、単に対応言語を増やすことではなく、
**selector / shaping / layout / modifier / rendering の責務を分け直す** ことを目的にする。

---

## Problem

現状の `Artifact` では、`TextAnimatorEngine` 自体はかなり良いが、
text shaping と selection semantics はまだ `char` と `glyph` の間で揺れている。

今のままだと次の問題が起きやすい。

- Hangul は比較的通せても、Arabic / Hebrew の bidi と contextual shaping が不安定
- grapheme cluster と glyph cluster が一致しない script で selector が壊れやすい
- ligature, combining mark, emoji ZWJ sequence を文字単位編集として扱うと破綻する
- source text 編集後に animator target がずれる
- 縦書きを入れた瞬間に selector, anchor, line flow, punctuation rotation の意味が崩れる

---

## Goal

- complex script を `special case` ではなく `normal path` として扱う
- text selection を `char` ではなく `text token / cluster span` に寄せる
- `Selector Stack` と `Modifier Stack` を分離して理解しやすくする
- `stable glyph id` ではなく `stable text token id + shaped cluster mapping` にする
- 横書きと縦書きを同じ text model 上で扱う

---

## Non-Goals

- いきなり GPU text backend を全面置換すること
- HarfBuzz 専用 backend を最初から必須化すること
- AE と完全に同じ内部表現を再現すること
- 既存の text layer 全体を一括で作り直すこと

---

## Design Direction

### 1. Complex Script First

`CJK only shaped path` ではなく、
**cluster shaping が必要な script は全て shaped layout の本線へ流す**。

対象:

- Japanese
- Chinese
- Korean / Hangul
- Arabic
- Hebrew
- Thai
- Indic scripts
- emoji / emoji ZWJ sequence
- ligature-heavy Latin typography

### 2. Text Token Model

内部で直接 `GlyphItem` を編集対象にしない。
まず text を次の単位へ分解して保持する。

- source text run
- grapheme cluster
- glyph cluster
- word
- line
- paragraph

各単位に stable id を持たせる。

例:

- `TextTokenId`
- `ClusterSpanId`
- `LineId`

`GlyphItem` は最終描画の派生結果として扱う。

### 3. Selector Stack + Modifier Stack

`Animator` を一塊で増やすのではなく、次の 2 層に分ける。

- `Selector Stack`: どこに効くか
- `Modifier Stack`: 何を変えるか

Selector の例:

- range selector
- regex selector
- tag selector
- line selector
- word selector
- script selector
- direction selector
- vertical column selector

Modifier の例:

- transform
- opacity
- tracking
- fill / stroke
- blur
- noise
- field
- formula

### 4. Writing Mode As First-Class Data

縦書きを後付け option にせず、layout contract に明示的に持たせる。

必要な軸:

- writing mode: horizontal / vertical
- inline direction
- block flow direction
- bidi direction
- glyph orientation
- punctuation rotation policy
- tate-chu-yoko policy
- ruby policy
- kinsoku policy

---

## Proposed Data Contract

### Text Layout Contract

- `writingMode`
- `baseDirection`
- `bidiResolvedRuns`
- `graphemeClusters`
- `glyphClusters`
- `lineRuns`
- `SelectorUnits` に Cluster / Line を追加
- `tokenToGlyphMap`
- `clusterToGlyphMap`
- `rubyAttachments`
- `tateChuYokoRuns`
- `punctuationPlacementRuns`
- `bracketOrientationRuns`
- `kinsokuBoundaryInfo`
- `layoutRevision`

### Selector Contract

- `targetUnit`
- `selectionMode`
- `selectorOrder`
- `seed`
- `range`
- `regex`
- `tags`
- `scriptFilter`
- `writingModeFilter`

### Modifier Contract

- `position`
- `scale`
- `rotation`
- `opacity`
- `tracking`
- `color`
- `stroke`
- `blur`
- `noiseField`
- `formulaField`

---

## Language-Specific Notes

### Hangul

- syllable block と jamo sequence を壊さない
- grapheme cluster 単位 selector を優先する
- glyph 単位編集は debug / advanced mode に限定できるようにする

### Arabic

- contextual shaping を前提にする
- right-to-left を layout contract へ明示する
- joining を壊す cluster split を禁止する
- regex selector は source run に対して評価し、glyph には直接当てない

### Hebrew

- bidi 解決を先に行い、visual order と logical order を混同しない
- niqqud など combining mark を cluster としてまとめる

### Other Asian Scripts

- Thai, Indic, Khmer などは grapheme cluster と glyph cluster の分離を前提にする
- line break と cursor movement を script-aware にする

### Emoji

- ZWJ sequence, skin tone modifier, variation selector を単一 cluster として扱う
- selector が途中を切らない

---

## Vertical Writing Notes

縦書きは単なる 90 度回転ではない。
次を layout contract に含める必要がある。

- column progression
- line stacking direction
- vertical glyph orientation
- rotated Latin handling
- punctuation placement
- tate-chu-yoko
- ruby / emphasis mark compatibility
- kinsoku line-break policy

縦書きの selector では、少なくとも次を明示する。

- line
- column
- cluster
- glyph orientation

### Tate-Chu-Yoko

縦中横は単なる rotate ではなく、
**縦書き flow の中に短い横組み run を埋め込む** 契約として扱う。

必要な項目:

- `tateChuYokoPolicy`
- `maxInlineDigits`
- `allowedScriptKinds`
- `baselineAdjustment`
- `boxFitPolicy`

初期方針:

- まずは 2 桁から 4 桁程度の数字と短い ASCII を対象にする
- selector は tate-chu-yoko run 全体に当てる
- run の途中を cluster 単位で分割しない

### Punctuation Placement

句読点は glyph rotation だけで済ませず、
**縦書き専用の配置 policy** として持つ。

必要な項目:

- `punctuationPlacementPolicy`
- `hangablePunctuation`
- `compressionPolicy`
- `lineStartLineEndAdjustment`

初期方針:

- `、。` は vertical punctuation set として別扱いする
- line end / line start での押し込みとぶら下げ可否を policy 化する

### Bracket Orientation

括弧は縦書き用の形状 / 向き / ペア解決が必要なので、
文字そのものではなく **paired punctuation run** として扱う。

必要な項目:

- `bracketOrientationPolicy`
- `pairedBracketMap`
- `mirroredGlyphPolicy`
- `verticalAlternateGlyphPolicy`

初期方針:

- 開き括弧 / 閉じ括弧の対応を logical order で保持する
- shaping 後の見た目ではなく、source token 側で pair を追う

### Ruby

ルビは decoration ではなく、
base text と別の inline run を持つ注釈 layer として扱う。

必要な項目:

- `rubyBaseRange`
- `rubyText`
- `rubyAlignment`
- `rubyOffsetPolicy`
- `rubyCollisionPolicy`

初期方針:

- base cluster span に対して ruby attachment を持つ
- selector は既定で base text に当て、ruby は opt-in で別 target にする
- text animator が base と ruby を一緒に壊さないよう、target unit を分ける

### Kinsoku

禁則は wrap 後の見た目修正ではなく、
**line break candidate の段階で判定する rule set** として扱う。

必要な項目:

- `kinsokuPolicy`
- `prohibitedLineStart`
- `prohibitedLineEnd`
- `hangingPunctuationPolicy`
- `compressionFallbackPolicy`

初期方針:

- 行頭禁則と行末禁則を rule table として持つ
- break candidate を作る段階で invalid break を落とす
- どうしても収まらない場合の fallback を明示する

---

## Japanese Composition Extras

縦書き対応では、次の 5 つを最初から視野に入れる。

- `縦中横`: 短い横組み run
- `句読点`: `、。` の専用配置
- `括弧`: 縦書き向け alternate glyph / orientation
- `ルビ`: base text に対する注釈 run
- `禁則`: line break candidate の制御

これらは別機能ではなく、すべて `TextLayoutContract` の一部として扱う。

---

## UI Direction

### Selector Debugger

AE の弱点を避けるため、まず debug surface を入れる。

- weight heatmap
- selected unit kind: glyph / grapheme / cluster / word / line
- logical order / visual order 表示
- selector order / seed 表示
- writing mode badge
- RTL / vertical badge

### Inspector

- current selection target unit を常時表示
- current writing mode を表示
- script-sensitive warning を出す
- cluster-breaking selection は disabled にする

### Timeline

- text animator track に `RTL`, `Vertical`, `Cluster`, `Regex`, `Seeded` などの badge を出せるようにする

---

## Performance Direction

高速化は必要だが、意味論より先にやらない。

優先順:

1. layout cache
2. shaped run cache
3. cluster-to-glyph map cache
4. glyph atlas
5. GPU instance rendering

理由:

- selector semantics が固まる前に GPU instance へ進むと、壊れた挙動を高速化するだけになりやすい

---

## Recommended Phases

### Phase 1: Complex Script Safe Contract

- `GlyphItem` 直編集をやめるための中間 contract を作る
- grapheme cluster / glyph cluster / line run を定義する
- selection が cluster を壊す場合は止める
- RTL と vertical を data として持てるようにする

完了条件:

- Hangul / Arabic / Hebrew / emoji で selection semantics が破綻しにくい
- writing mode を property として持てる

### Phase 2: Selector Debugger

- weight heatmap
- logical / visual order 表示
- unit kind 表示
- seed / order / writing mode 表示

完了条件:

- どの unit に何が効いているかを UI で説明できる

### Phase 3: Selector Stack + Modifier Stack

- animator を selector と modifier に分解する
- regex selector / tag selector / script selector を追加できる形にする
- field / noise / formula modifier を標準化する

完了条件:

- animator 数が増えても構造が追いやすい

### Phase 4: Vertical Writing

- writing mode horizontal / vertical
- punctuation rotation policy
- rotated Latin policy
- vertical selector visualization
- tate-chu-yoko
- ruby attachment
- kinsoku-aware line break
- bracket and punctuation placement policy

完了条件:

- 縦書き text layer が layout / selector / modifier の各段で矛盾しない
- `縦中横 / 句読点 / 括弧 / ルビ / 禁則` の入口を同じ contract で扱える

### Phase 5: GPU-Oriented Optimization

- layout cache
- shaped run cache
- glyph atlas
- GPU instance

完了条件:

- complex script / vertical text を含む animator preview が軽くなる

---

## Initial Implementation Slice

最初の 1 スライスは小さく始める。

1. `TextLayoutContract` を追加する
2. `targetUnit` を glyph / grapheme / cluster / word / line で持つ
3. Inspector に unit badge を出す
4. weight heatmap debug view を出す
5. `writingMode` を horizontal / vertical で持つ

この段階では、まだ full vertical rendering を完成させなくてよい。
まずは **壊れない text semantics** を先に固定する。

### Current Progress

- `TextLayoutContract` を追加済み
- `writingMode` を text layer surface へ追加済み
- vertical path で `tate-chu-yoko` / `ruby` / punctuation / bracket metadata を少しずつ返し始めた
- `、。` は upright 寄り、ASCII bracket は rotate 寄りの fallback を入れ始めた
- selector / modifier / rendering の分離を前提に、縦書きの metadata contract を先に固める方針を維持している
- Inspector に dynamic text unit badge と line-aware selection target の入口を出し始めた
- Canvas 側に selector weight heatmap の入口を出し始めた
- heatmap を glyph/cluster/line ベースへ少し寄せた
- heatmap に logical / visual / unit label を足し始めた
- heatmap に cluster / line boundary marker を足し始めた
- heatmap の boundary marker に番号を振り始めた
- Inspector に selector overview を出し始めた
- selector overview に writing mode を足し始めた
- debugState に selector overview を足し始めた
- selector overview に logical / visual order を足し始めた
- Inspector の個別 selector 詳細を selector overview 寄りに整理し始めた
- selector overview を key/value 形式へ寄せ始めた
- debugState の selector ラベルを selectorOverview に揃えた
- selector overview の order を source / visual に分け始めた
- debugState の selectorOverview を括弧なしにした
- selector overview を compact key=value 形式へ寄せた
- selector overview に unit も入れた
- selector overview の target を atom 化した
- shaping backend の script tag 分類を広げた
- selector に tag 単位を追加した
- selector overview に tag summary を出した
- selector の regex pattern を受け始めた
- stable token id を glyph に載せた
- Inspector に selector token を出した
- selector overview に token を入れた
- selector overview に tag も戻した

---

## Success Criteria

- Hangul, Arabic, Hebrew, emoji を含む text animator が意味論的に破綻しにくい
- grapheme cluster と glyph cluster を UI 上で区別して扱える
- selector の対象単位が常に明示される
- 縦書きを property と layout contract で扱える

## Execution Slice

- まずは [`MILESTONE_TEXT_LAYOUT_CONTRACT_2026-06-12.md`](../../ArtifactCore/docs/MILESTONE_TEXT_LAYOUT_CONTRACT_2026-06-12.md) を共通契約の起点にする
- 次に [`MILESTONE_TEXT_SHAPING_BACKEND_HARFBUZZ_2026-06-12.md`](../../ArtifactCore/docs/MILESTONE_TEXT_SHAPING_BACKEND_HARFBUZZ_2026-06-12.md) で shaping backend を差し替え可能にする
- その後に vertical writing の metadata を `TextLayoutContract` へ流し込む
- 最後に selector / inspector / debug surface を unit aware にする
- GPU 最適化へ進む前に text semantics が固定される
