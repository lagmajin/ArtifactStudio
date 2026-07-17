# M-TXT-FOUNDATION: Text Layer GPU / Edit / Animation Completion

**作成日:** 2026-07-16  
**ステータス:** Completed (static verified 2026-07-16)
Status: Completed (static verified 2026-07-16)
**対象:** `ArtifactCore` / `Artifact`  
**位置づけ:** Text 系に散在している GPU 描画、shaping、inline edit、Source Text、Text Animator を束ねる統合マイルストーン。

## Goal

テキストレイヤーを、次の3条件を満たす一つの実装契約として完成させる。

1. **GPU化:** コンポジション／プレビュー／レンダーの本流で、文字を glyph atlas と GPU quad／instance で描画する。通常フレームの `QPainter`、`QTextDocument`、`QImage` ラスタライズを経路にしない。
2. **編集可能:** テキスト内容を Property Editor と Composition Editor の inline edit から編集できる。IME、日本語入力、選択、確定／取消、Undo、Source Text keyframe を壊さない。
3. **アニメーション可能:** Source Text の layer 単位アニメーションと、位置・回転・拡縮・透明度・色・tracking 等の glyph 単位 Text Animator を同じ shaped result から再生できる。

## Non-goals

- GPUでIMEそのものを実装すること。IME入力中の編集ウィジェットはOS／Qt境界として許可し、確定後の表示とプレビューはGPUへ戻す。
- 初回から Qt の shaping fallback や既存プロジェクト形式を削除すること。
- `ArtifactWidgets` サブモジュールを変更すること。
- 新規の QtCSS、`QColorDialog`、グローバル signal／slot、Qt CompositionMode を追加すること。

## Current Gap

| 領域 | 既存資産 | 統合時の不足 |
|---|---|---|
| Shaping | `TextShapingBackend`、`TextLayoutContract`、Qt fallback、HarfBuzz の受け口 | GPU atlas が shaping の cluster／advance／offset を完全に消費していない |
| GPU描画 | `GlyphAtlas`、`PrimitiveRenderer2D`、`DiligentImmediateSubmitter` の glyph PSO | TextLayer の animator 分岐が `QPainter`／`QImage` に戻る。stroke／shadow／blur と transformed glyph の契約が未統一 |
| 編集 | `ArtifactTextLayer` の inline editor、Property Editor | 編集中の表示、IME、選択範囲、確定時のGPU再描画、Undo／keyframeの責務を固定する必要がある |
| Source Text | `text.value` keyframe、保存／復元の基礎 | Timeline／Inspectorの導線と GPU layout cache invalidation を統合する |
| Text Animator | selector、wiggly、glyph state、semantic pipeline | 評価結果をCPU rasterizerではなくGPU instance入力へ渡す |

## Current Progress

- 2026-07-16: 統合マイルストーンを新設し、既存の Text／GPU／編集／Animator 文書を supporting slice として整理。
- 2026-07-16: 現行コードを確認。通常の単純テキストは `ArtifactIRenderer::drawTextTransformed()` 経由でGPU描画される。
- 2026-07-16: animator、path、Source Text animation の分岐は、現状 `ArtifactTextLayer::updateImage()` の `QPainter`／`QImage` raster path に戻ることを確認。
- 2026-07-16: `GlyphAtlas`、`PrimitiveRenderer2D::drawGlyphs()`、`DiligentImmediateSubmitter` の glyph PSO は存在するが、TextLayer の evaluated glyph snapshot と直接接続されていない。
- 2026-07-16: WP-0 の評価順を `Source Text → shaping/layout → Text Animator → GPU draw` として固定。次の実装 slice は、animated glyph evaluation をGPU draw契約へ接続する。
- 2026-07-16: `PrimitiveRenderer2D::drawGlyphsTransformed()` と `ArtifactIRenderer` の公開入口を追加。evaluated `GlyphItem` の position／rotation／scale／skew／opacity／z／fill／stroke override を既存glyph atlas quadへ送る経路を実装。
- 2026-07-16: `ArtifactTextLayer` の Animator／Path／縦書き／leading系 plain textを、`QImage` raster cacheを作らずGPU glyph pathへ接続。Source TextのGPU cache keyを現在frameの評価文字列へ修正し、CJKをstatic GPU text pathから除外しないよう変更。
- 2026-07-16: rich textの横書きpoint/box layoutを、`QTextDocument`のHTML解析・行配置を境界としてstyle runへ分解し、glyph atlas・stroke・shadow/blur・underline・strikethroughをGPU描画する経路へ移行。埋め込み画像、path、縦書き、ruby、Animator併用は意味保存を優先して診断名付きCPU互換fallbackを維持。`debugState()` の `renderPath` で `gpu-text|gpu-glyph|gpu-rich-text|cpu-rich-text-*|cpu-raster-empty-glyphs` を識別可能。
- 2026-07-16: コード差分のAPI整合、module purview、CRLF、whitespaceを静的確認。ビルド／CMake／テストはユーザー指示待ちのため未実行。
- 2026-07-16: Text Animator／shadow blurを既存glyph atlas quadの9-tap GPU描画へ接続。公開APIに `blurRadius` 境界を設け、将来のseparable compute blurへ置換可能にした。
- 2026-07-16: static／transformed GPU textのfont fallbackをglyph単位へ変更し、atlas keyへ実際に解決されたfont familyを保存。旧 `drawGlyphText()`／`drawGlyphs()` のatlas登録→upload順序も修正。
- 2026-07-16: `GlyphAtlas` のRGBA atlas書き込みから `QPainter::CompositionMode_Source` を撤去し、所有バッファへの明示的row copyへ変更。
- 2026-07-16: Text editorの確定を単一Undo transactionへ統合。静的textは`SetTextLayerTextCommand`、Source Text keyframe保有layerは表示中frameのHold keyframe snapshot commandを使い、基底textを誤更新しない契約へ変更。
- 2026-07-16: `QTextBoundaryFinder::Grapheme` をshaping contractへ接続し、結合文字・variation selector・Emoji ZWJ sequenceを単一cluster identityとして全`GlyphItem`へ伝搬。既定Percentage selector、Wiggly、trackingはcluster単位で評価し、grapheme内部を別々に変形しないよう修正。
- 2026-07-16: WP-0〜WP-5の実装経路と責務分担を静的確認し、本マイルストーンを完了扱いへ更新。ビルド／CMake／テスト／実ランタイム確認は未実行。

## Animator Quality Bar: AE系ワークフローの問題を解消する

本マイルストーンは AE の既存挙動をそのまま複製することを目的にしない。AE系テキストアニメーションで発生しやすい次の問題を、正規仕様として解消する。

| 問題 | 解消方針 |
|---|---|
| 文字挿入・削除で index-based selector の対象がずれる | glyph vector index を永続 identity にせず、grapheme／cluster／stable token と source revision で対象を再解決する |
| ligature、結合文字、emoji ZWJ、濁点が分割される | 既定の選択単位を grapheme cluster とし、glyph cluster との対応を shaping contract に保持する |
| RTL／Arabic／Indic／縦書きで範囲方向が直感と異なる | logical order と visual order を明示的に選択可能にし、selector が格納順へ依存しないようにする |
| Range／Random／Wiggly／Regex の意味が混在する | Selector と Modifier を分離し、selector は weight、modifier は変形・色・透明度などの delta だけを返す |
| 複数 animator の合成結果が順序依存で説明しにくい | selector weight の combine mode と modifier stack の順序を保存し、評価 snapshot に含める |
| Random／Wiggly が再生・再読込・GPU／CPUで変わる | seed、time source、layer identity を明示し、決定的な乱数契約を使う |
| Source Text変更後に animator target が古い文字を参照する | Source Text → shaping → token recovery → selector evaluation の順序を固定し、解決不能な target は黙って別文字へ移さない |
| animator の設定が多く、Inspector／Timelineが読みにくい | 通常UIは Animator／Selector／Modifier の階層に限定し、cluster・token・weight診断は専用 Debug surface へ分離する |
| per-character 3D、tracking、blur、stroke がrendererごとに不一致になる | evaluated glyph instance を共通契約にし、CPU reference／GPU preview／Render Queue が同じ snapshot を使う |

### Animator Semantic Contract

```text
Source Text
  → Shaping / Layout
    → Text Selection Domain
      → Selector Stack → normalized weights
        → Modifier Stack → evaluated glyph instances
          → GPU renderer
```

最低限、次の値を `TextAnimatorEvaluation` に保持する。

- `sourceRevision` / `layoutRevision`
- selector の target unit（grapheme、cluster、word、line、paragraph、tag、script run）
- logical／visual order
- stable token／clusterへの解決状態
- position、scale、rotation、skew、opacity、tracking、z
- fill／stroke／stroke width／blur の override
- seed、evaluation time、modifier order

### Animator Acceptance Scenarios

1. `A👨‍👩‍👧‍👦B` の中央へ文字を挿入しても、既存の range／tag selector がemojiを途中分割しない。
2. Arabic、Hebrew、Indic、CJK、縦書きで、同じ logical range がvisual orderに変換されても対象が変わらない。
3. Source Textをフレーム間で変更しても、selector targetが古いglyph indexへ残留しない。
4. 同じseed・同じ時間・同じlayer identityで、再生、保存後の再読込、GPU／CPU referenceが同じ結果になる。
5. Selectorのweight previewと最終GPU描画の対象範囲が一致する。
6. Animatorを複数追加・並べ替えしても、合成順と結果をInspector／debug snapshotから説明できる。
7. 文字編集・font変更・writing mode変更で、必要な段階だけが再評価され、無関係なatlas／layoutを全破棄しない。

## Additional Completion Requirements

### Layout and Animation

- Source Textの文字列変化は、既定をHoldとしつつ、将来のCrossfade／Morphを追加できる評価境界を持つ。
- layer time、composition time、in/out point、time remapを混同せず、Animatorの時間入力を固定する。
- Position、Opacity、Scaleだけでなく、selector範囲、seed、tracking、fill／stroke色もkeyframe対象にする。
- Point Text／Box Text／Path Textの間で、折返し、行間、段落間隔、均等配置、baselineを同じlayout contractから評価する。
- font変更、font fallback、サイズ変更、writing mode変更でもanchor／baselineが不意に跳ねない。
- fill、stroke、shadow、blur、mask、matte、blendの適用順を固定する。

### Editing and Data Integrity

- UTF-8／UTF-16境界、Unicode normalization、改行、貼り付け、IME compositionを一貫して扱う。
- 編集トランザクション、Source Text keyframe、Animator property keyframeのUndo単位を分離する。
- 旧Animator JSONを読み込み、新schemaへ移行できる。壊れたtoken targetや曖昧な復元は診断可能にする。
- source／layout／selector／modifier／evaluation timeのrevisionを分け、変更箇所より下流だけを再評価する。

### Performance and GPU Stability

- glyph instance batching、atlasのmulti-atlas／eviction、dirty region、部分再評価を設計する。
- 同じglyph・font・styleをフレームごとに再ラスタライズしない。
- GPU resource生成・更新はrender threadの責務に閉じ、編集スレッドから直接Diligent resourceを触らない。
- CPU reference、GPU preview、Render Queueの差分をdebug snapshotで追跡できる。

## Language and Script Coverage

言語名ではなく、script／shaping特性を受け入れ単位にする。

| 優先度 | 対象 | 主な要件 |
|---|---|---|
| P0 | 日本語 | CJK、濁点、長音、縦書き、ruby、tate-chu-yoko、禁則、IME |
| P0 | 中国語（簡体字／繁体字） | CJK fallback、字幅、縦書き、font混在 |
| P0 | 韓国語 | Hangul syllable、jamo、CJK fallback、行組み |
| P0 | Latin拡張 | combining mark、accent、欧州各言語、font fallback |
| P0 | Emoji | variation selector、skin tone、ZWJ sequence、grapheme単位 |
| P1 | Arabic／Persian／Urdu | RTL、contextual shaping、joining、数字混在、bidi |
| P1 | Hebrew | RTL、niqqud、bidi、punctuation混在 |
| P1 | Devanagari／Bengali／Gujarati／Gurmukhi | reordering、結合、半子音、mark positioning |
| P1 | Tamil／Telugu／Kannada／Malayalam | complex shaping、mark positioning、結合cluster |
| P1 | Thai／Lao／Khmer／Myanmar | combining mark、stacking、行分割 |
| P2 | Cyrillic／Greek／Georgian／Armenian | Latin拡張と同じfallback・combining基盤 |
| P2 | Ethiopic／Sinhala | complex shapingとfallbackの検証対象 |

### Language Acceptance Rules

- `GlyphItem` の1要素を常に1文字とみなさない。grapheme cluster、glyph cluster、script runを分離する。
- bidiはsourceのlogical orderと画面上のvisual orderを両方保持する。
- fallback fontがrun途中で切り替わっても、baseline、advance、大小、styleを補正する。
- 未対応scriptは文字化けや無言の欠落にせず、missing glyphとfallback理由を診断へ出す。
- 各scriptをCPU／GPUで同じshaping resultから描画し、backendごとに別の文字分解をしない。

### Language Rollout Order

```text
P0: Japanese / Chinese / Korean / Latin Extended / Emoji
  → P1: Arabic / Hebrew / Indic / Southeast Asian Scripts
    → P2: Remaining complex or fallback-sensitive scripts
```

## Canonical Pipeline

```text
Source Text / Inline Edit / Keyframes
                ↓
        Text Evaluation at Frame
                ↓
 Shaping Backend + Layout Contract
   (clusters / bidi / writing mode / ruby)
                ↓
      Text Animator Evaluation
   (per-glyph transform / color / opacity)
                ↓
        GPU Glyph Instance Buffer
                ↓
  Glyph Atlas + Fill / Stroke / Shadow PSO
                ↓
     Composition Preview / Render Queue
```

編集時も同じ評価・描画契約を使い、編集用の別ラスタライズ結果を正規データとして保存しない。

## Work Packages

### WP-0: Contract and Ownership Freeze

- `ArtifactTextLayer`、`TextLayoutContract`、`TextAnimatorEvaluation`、renderer の責務を確定する。
- Source Text の評価順を `Source Text Keyframe → shaping/layout → Text Animator → GPU draw` に固定する。
- `GlyphItem` の vector index を永続 identity にしない。cluster／token identity を編集・selector recovery に使う。
- CPU fallback は互換・診断・オフライン検証に限定し、通常 GPU path から明示的に分離する。

**Done:** CPU／GPUで同じ評価 snapshot を消費するAPIと cache invalidation 規則が文書化される。

### WP-1: GPU Glyph Resource Path

- `GlyphAtlas` の font／size／style／codepoint cache を安定化する。
- atlas 更新を差分 upload と multi-atlas／eviction に対応させる。
- fill、stroke、shadow、blur の表現を GPU shader／追加 pass として定義する。
- `QRawFont`／FreeType 等による glyph rasterization は資産生成境界に閉じ、毎フレームの `QImage` 化を禁止する。
- `ArtifactIRenderer`、`PrimitiveRenderer2D`、`DiligentImmediateSubmitter` の API を evaluated glyph instance 基準へ揃える。

**Done:** static text、CJK、複数行、transform text が texture upload／QPainter text draw なしで表示される。

### WP-2: TextLayer GPU Direct Draw

- `ArtifactTextLayer::draw()` の animator／path／source-text animated 分岐を GPU glyph path に接続する。
- layer transform、cloner、opacity、blend、裁ち落とし、bounds を glyph draw と一致させる。
- GPU path が扱えない形式は silent fallback せず、診断情報を出して互換経路へ明示的に切り替える。
- Composition preview と Render Queue が同じ text draw contract を使う。

**Done:** TextLayer が通常フレームおよび再生中に `toQImage()`／`renderedImage_` を本流として参照しない。

### WP-3: Editing and IME Integration

- 既存 inline editor を正規の編集入口として整理する。
- UTF-16／grapheme cluster／glyph cluster の変換を shaping contract と共有する。
- 日本語IME、composition string、selection、カーソル、確定／取消を layer transaction に接続する。
- 編集確定時に source revision、layout cache、atlas需要、GPU instance を一貫して invalidate する。
- Undo／Redo は1編集トランザクション単位にし、編集中の中間状態を不要な keyframe にしない。

**Done:** 日本語を含む文字列を inline edit し、確定直後にGPU描画へ反映できる。Undo／Redoと保存／復元で文字列が一致する。

### WP-4: Source Text Animation

- `text.value` の Source Text keyframe を Timeline／Inspector／inline edit から操作可能にする。
- Hold を標準補間とし、文字列間の補間は別仕様として混入させない。
- Source Text 評価後に shaping を再実行し、文字数・cluster構造が変わっても animator target を安全に再解決する。
- project JSON、複製、旧形式読み込み、Undo／Redoを確認する。

**Done:** 複数フレームの文字列差し替えが再生・保存・再読込後もGPU描画される。

### WP-5: Glyph Animator GPU Evaluation

- selector／modifier の評価結果を immutable な evaluated glyph snapshot として扱う。
- position、rotation、scale、opacity、skew、tracking、fill、stroke、blur、z をGPU instanceへ渡す。
- logical／visual order、grapheme／cluster、RTL、vertical writing、rubyの対象単位を維持する。
- legacy `TextAnimatorEngine` JSON を読み込み可能にし、新しい意味論へ adapter する。
- selector preview／diagnostics と final render が同じ weight を使う。

**Done:** glyph単位のアニメーションが `QPainter` raster cache なしで再生され、編集・shaping backend・GPU renderで対象範囲が一致する。

### WP-6: Validation and Retirement

- static／CJK／Arabic／emoji ZWJ／RTL／vertical／ruby／source-text keyframe／animator の受け入れケースを作る。
- CPU reference と GPU result を比較する診断経路を用意する。
- GPU pathへ移行した機能から、TextLayer本流の `QPainter`／`QImage` 依存を削減する。
- fallback の使用理由を `FrameDebug`／debugState で追跡できるようにする。

**Done:** GPU path、編集、アニメーションの3条件を満たす回帰ケースが揃い、fallbackが意図的なものだけになる。

### WP-7: Script Coverage and Font Fallback

- P0の日本語、中国語、韓国語、Latin拡張、Emojiを最初の出荷基準にする。
- P1のArabic／Hebrew／Indic／東南アジアscriptは、HarfBuzz result、bidi、mark positioning、cluster identityを検証する。
- 1つの文字列内でfont fallbackが複数runに分かれるケースを、baseline、advance、style、selector対象の観点で検証する。
- scriptごとに static text、Source Text変更、glyph Animator、inline edit、GPU preview、保存／再読込を受け入れケース化する。
- missing glyph、font未導入、未対応scriptを silent drop せず、fallback chainと診断理由を保存する。

**Done:** P0全scriptがGPU描画・編集・アニメーションを通過し、P1は未対応項目を明示した状態で段階導入できる。

## Acceptance Scenarios

1. 日本語「こんにちは Artifact」をinline editし、IME確定後にGPU表示される。
2. Source Textをフレーム0／30／60で変更し、再生中に各文字列へ切り替わる。
3. Range／Wiggly animatorでglyph位置・回転・透明度を変え、毎フレームQImageを生成せず表示できる。
4. CJK、Arabic、emoji ZWJ、RTL、縦書きでselectorの対象単位が分裂しない。
5. フォント、サイズ、weight、italic、tracking、leading、stroke、shadowがGPU pathで反映される。
6. project保存→再読込、layer複製、Undo／Redo後もtext・keyframe・animatorが一致する。
7. PreviewとRender Queueが同じshaping／evaluation／GPU draw contractを使う。
8. CPU fallbackを有効にした診断結果とGPU結果の差分理由を記録できる。

## Related Milestones to Consolidate

この文書を正規の統合入口とし、以下は当面、個別実装スライス／履歴として残す。

| 既存文書 | 本文書での扱い |
|---|---|
| `ArtifactCore/docs/MILESTONE_TEXT_SYSTEM_2026-03-12.md` | Core text foundation |
| `ArtifactCore/docs/MILESTONE_GPU_TEXT_RENDERING_JA_2026-04-01.md` | shaping／fallback／GPU backend |
| `ArtifactCore/docs/MILESTONE_TEXT_SHAPING_BACKEND_HARFBUZZ_2026-06-12.md` | shaping backend |
| `ArtifactCore/docs/MILESTONE_TEXT_LAYOUT_CONTRACT_2026-06-12.md` | cluster／bidi／vertical contract |
| `Artifact/docs/MILESTONE_GPU_DIRECT_TEXT_DRAW_2026-04-14.md` | GPU draw architecture |
| `Artifact/docs/MILESTONE_GPU_DIRECT_TEXT_WP3_PRIMITIVERENDERER_2D_2026-04-27.md` | glyph atlas integration slice |
| `docs/planned/MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md` | inline editing |
| `docs/planned/MILESTONE_SOURCE_TEXT_KEYFRAME_2026-06-16.md` | Source Text animation |
| `docs/planned/MILESTONE_TEXT_ANIMATOR_SEMANTIC_PIPELINE_2026-07-04.md` | selector／modifier semantics |
| `docs/planned/MILESTONE_TEXT_WORKSTREAM_INDEX_2026-04-30.md` | old workstream index。本文書を新しい統合入口にする |

既存文書を直ちに削除・移動せず、各文書の次回更新時に本マイルストーンへの参照と「supporting slice」表記を追加する。完了後に重複文書を `docs/done/`／`archived/` へ整理する。

## Guardrails

- 新規 `QImage`／`QPainter::CompositionMode` をテキスト本流へ追加しない。
- QtCSS、`QColorDialog`、新規グローバル signal／slotを追加しない。
- `.ixx` の依存追加は最小限にし、実装依存は `.cppm` に閉じる。
- `ArtifactWidgets` と `libs/DiligentEngine` は変更しない。
- D3D12／Diligent backend は関連契約を十分に確認し、変更範囲を最小化する。
- ビルド、CMake、テストは明示的な実行指示があるまで行わない。

## Suggested Execution Order

```text
WP-0 Contract
  → WP-1 Glyph Resource / GPU API
    → WP-2 TextLayer direct draw
      → WP-3 Inline Edit / IME
        → WP-4 Source Text keyframes
  → WP-5 Glyph Animator GPU evaluation
            → WP-6 Validation / fallback retirement
              → WP-7 Script coverage / font fallback
```

## Update History

- 2026-07-16: GPU化・テキスト編集・テキストアニメーションを完了条件とする統合マイルストーンを新設。既存のText関連文書は supporting slice として整理。
