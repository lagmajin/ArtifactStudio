# Expression Engine Audit & Completion (2026-08-08)

**最終更新:** 2026-08-15
**状態:** コア評価・Copilot基本UIは実装済み。レイヤー実値参照、Marker API、Pick Whip、追加AE関数、runtime受入れが未完了。

## 第一稿からの主な訂正

| 項目 | 第一稿 | 第二稿（実態） |
|------|--------|--------------|
| `time` 変数注入 | 「不十分」 | ✅ `evaluateValue()`内でLine 490に `setVariable("time", ...)` あり |
| `value` 変数注入 | 「previewValue という表示値」 | ✅ `evaluateValue()`内でLine 489に `qvariantToExpressionValue(baseValue, type)` でキーフレーム補間後の値を注入 |
| `frameRate` 注入 | 未記載 | ✅ Line 488, 491 |
| `keyframes` 配列 | 未記載 | ✅ Line 496-504。`loopIn`/`loopOut`/`smooth` 用に全キーフレームを注入 |
| `thisComp.layer("name")` | 「部分的に動く」 | ✅ Evaluator で MethodCall→`thisComp.layer()` を解析し、レイヤー名/インデックス検索を実装済み（ExpressionEvaluator.cppm:363-396） |
| AST キャッシュ | 未記載 | ✅ `propertyExpressionAstCache().getOrParseAST()` (Line 507) |
| 自動補完 | 未記載 | ✅ `rootSuggestions()`, `thisCompSuggestions()`, `thisLayerSuggestions()` |
| エクスプレッション有効/無効トグル | 「UIにない」 | ✅ `AbstractProperty::setExpression()` / `hasExpression()`。PropertyWidget 上のトグル UI 有無は未確認だが API は完備 |

## 現状監査

### 2026-08-15 実装監査

- 前回監査後も、通常の `AbstractProperty::evaluateValue()` が注入する変数は `value` / `time` / `frameRate` / `keyframes` までで、レイヤー `index` は確認できない。
- `ExpressionCopilotWidget` の `buildLayerObject()` は `name` / `index` / `comp` のみを構築しており、実レイヤーの `transform` や `marker` はまだ解決しない。Copilot の `thisLayer.index` 補完はあるが、実評価の index 注入とは別物。
- Pick Whip は完全な Property Editor ドロップ連携ではないが、Copilot 内に参照リストと `application/x-artifact-expression-reference` のドラッグ生成が存在する。エディタ側の受け取り・式挿入までの一貫した経路は未確認。
- エラー表示用の `QTextEdit::ExtraSelection` は実装されている。追加 AE 関数（`posterizeTime`、`seedRandom`、座標変換等）は Expression evaluator の組込関数として確認できない。
- 今回はコード監査のみで、ビルド・ランタイム受入れは実施していない。

## Update 2026-08-15

- `AbstractProperty::evaluateValue()` の `value`／`time`／`frameRate`／`keyframes` 注入、AST cache、`thisComp.layer()` 解決、loop 系関数、Copilot 補完・エラー表示を再確認。
- 実レイヤーの transform／marker 解決、通常評価への `index` 注入、Property Editor まで通る Pick Whip、`posterizeTime`／座標変換等の追加 AE 関数は未完了または未確認。
- コア evaluator／parser は実装済みだが、製品統合と runtime 受入れは pending。ビルド・実行は未実施。

## Update 2026-08-15 (transform preview slice)

- Expression Copilot の preview context に、composition 内各レイヤーの `transform.position`、`transform.scale`、`transform.rotation`、`opacity` の現在時刻 snapshot を追加した。
- `thisComp.layer("Name").transform.position.x` などの Copilot preview 評価で実プロパティ値を参照できるようにした。
- Composition の既存 `ArtifactInOutPoints` から marker snapshot（time／index／duration／comment／chapter）を Copilot preview の `thisComp.marker.keys` に渡すようにした。
- 通常の `AbstractProperty::evaluateValue()` に同じ layer context を注入する処理、`marker.key()`／`nearestKey()` と layer marker、追加 AE 関数、runtime 受入れは引き続き pending。

## Update 2026-08-16

- Evaluator の MethodCall に `marker.key(index)` と `marker.nearestKey(time)` を追加した。
- Composition marker snapshot の `thisComp.marker.keys` と組み合わせて、Copilot preview で marker object を取得できる。
- Layer marker snapshot、通常評価への context 注入、追加 AE 関数、runtime 受入れは引き続き pending。

### コアエンジン（ArtifactCore）— 完全実装済み ✅

| モジュール | ファイル | 行数 |
|-----------|---------|------|
| ExpressionParser | `ExpressionParser.cppm` | 806行 |
| ExpressionEvaluator | `ExpressionEvaluator.cppm` | 1393行 |
| ExpressionValue | `ExpressionValue.ixx` | 複数ファイル |

### Parser — 全演算子 + 多言語対応 ✅

数値/文字列/ベクトル/配列/オブジェクト、全二項・単項演算子、三項（JS `a?b:c` / Python `b if a else c` 両対応）、関数呼出、メソッド呼出、プロパティアクセス（`.`）、配列アクセス（`[]`）、言語スタイル切替（Flexible/Python/Ruby/VB）。

### Evaluator — プロダクションレベル ✅

変数管理、再帰深度制限(100)、評価バジェット(10000回)、メモ化キャッシュ、キャンセル、時間評価モード4種（FrameLocked/SubframeSampled/AdaptiveStep/FixedMicrostep）、適応ステッピング（速度依存＋半ステップ誤差推定）。

### 組込関数 33個 ✅

sin, cos, tan, degToRad, radToDeg, sqrt, pow, abs, floor, ceil, round, min, max, clamp, length, distance, normalize, dot, cross, linear, ease, easeIn, easeOut, timeToFrames, framesToTime, random, randomSeeded, noise (Perlin), wiggle (fractal Perlin 4 octaves), smooth, sum, average + オーディオ5種 + AE Loop 4種 (loopIn/loopOut/loopInDuration/loopOutDuration with cycle/pingpong/offset/continue)。

### Property 統合 — ほぼ完了 ✅

`AbstractProperty::evaluateValue()` (Line 398-523):
- `time`, `value`, `frameRate`, `keyframes` 変数自動注入済み
- エンベロープ評価後にエクスプレッション評価（正しい順序）
- ASTキャッシュ (`propertyExpressionAstCache()`) で再パース回避
- エラー時は `baseValue` にフォールバック
- `qvariantToExpressionValue` / `expressionValueToQVariant` で型変換

### ExpressionCopilotWidget — UI 基本実装済み ✅

- 入力エリア (`QPlainTextEdit`)
- 自動補完: `thisComp`/`thisLayer`/`time`/`value`/`index`/全関数
- `thisComp.*` 補完: width, height, name, numLayers, layers, layer("...")
- `thisLayer.*` 補完: name, index, comp
- `buildCompositionObject()`, `buildLayerObject()` でオブジェクト構築
- `variantToExpressionValue()` で QVariant↔ExpressionValue 変換

---

## 残る実装ギャップ（5項目のみ）

### Gap 1: レイヤーの transform プロパティが解決不可能 🔴

**問題**: `thisComp.layer("MyLayer").position.x` はパースできるが評価時にエラーになる。

`buildLayerObject()` (ExpressionCopilotWidget.cppm:192-202) が作るレイヤーオブジェクトには `name`, `index`, `comp` のみで `transform` プロパティがない。

**修正**: `buildLayerObject()` に `transform` サブオブジェクトを追加。position.x/y、rotation、scale.x/y、opacity を現在時刻の値として埋め込む。

**ファイル**: `Artifact/src/Widgets/ArtifactExpressionCopilotWidget.cppm`
**コスト**: 中

---

### Gap 2: `index` 変数が注入されていない 🔴

**問題**: Copilot のプレビューでは `thisLayer.index` を注入しているが、通常の `AbstractProperty::evaluateValue()` 側への `index` 注入は確認できない。

`evaluateValue()` (Line 484-504) は `time`, `value`, `frameRate`, `keyframes` を注入するが、`index` は未注入。

**修正**: `evaluateValue()` に `evaluator->setVariable("index", ExpressionValue(layerIndex))` 追加。AbstractProperty にレイヤーインデックスを知る仕組み（プロパティオーナーレイヤーから取得）が必要。

**ファイル**: `ArtifactCore/src/Property/AbstractProperty.cppm`（1行追加）
**コスト**: 低

---

### Gap 3: レイヤーオブジェクトに marker がない 🟡

**問題**: `thisComp.marker.key(1).time` / `thisLayer.marker.nearestKey(time).index` が未実装。

AE のマーカーはコンポジションとレイヤーの両方に存在する。`ArtifactMarker` / In/Out/Chapter マーカーはタイムラインに既存。

**修正**: `buildCompositionObject()` と `buildLayerObject()` に `marker` 配列追加。マーカーオブジェクト構造: `{ time, index, duration, comment, chapter }`。`key(n)` / `nearestKey(t)` メソッドは Evaluator の MethodCall 分岐で実装。

**ファイル**: `Artifact/src/Widgets/ArtifactExpressionCopilotWidget.cppm`
**コスト**: 低

---

### Gap 4: ピックウィップ + 追加のエクスプレッション機能 🟡

**問題**:
- プロパティ間の参照をドラッグで作成するピックウィップ UI がない
- エラー位置情報は Copilot の `QTextEdit::ExtraSelection` で赤い波線・背景として表示済み。100ms デバウンスの有無と runtime 受入れは未確認
- 以下の AE 関数が欠落: `posterizeTime`, `seedRandom`, `toWorld`/`toComp`/`fromWorld`/`fromComp`, `lookAt`

**修正**:
- ピックウィップ: `ArtifactExpressionCopilotWidget` にボタン追加 → QDrag → PropertyEditor 行にドロップ → プロパティパス生成
- エラーハイライト: `QPlainTextEdit::ExtraSelection` でエラー位置に赤波線。100ms デバウンスでリアルタイム検証
- 新規組込関数: 6〜8関数。Evaluator に追加（50行程度）

**ファイル**: `Artifact/src/Widgets/ArtifactExpressionCopilotWidget.cppm`、`ExpressionEvaluator.cppm`
**コスト**: 中

---

## Phase 一覧（訂正版）

| Phase | 内容 | コスト | 優先度 |
|-------|------|--------|--------|
| 1 | `thisComp.layer("X").position.x` を評価可能にする | 中 | 🔴 |
| 2 | `index` 変数注入 | 低 | 🔴 |
| 3 | エラーハイライト + 欠落関数 + Marker API | 中 | 🟡 |
| 4 | Pick Whip UI | 中 | 🟡 |

Phase 1+2 で AE 互換のエクスプレッション基本機能が完成。Phase 3+4 は UX 磨き込み。

## 変更対象ファイル一覧

| ファイル | Phase | 変更量 |
|----------|-------|--------|
| `Artifact/src/Widgets/ArtifactExpressionCopilotWidget.cppm` | 1, 3, 4 | 中〜大 |
| `ArtifactCore/src/Property/AbstractProperty.cppm` | 2 | 1行 |
| `ArtifactCore/src/Script/Expression/ExpressionEvaluator.cppm` | 3 | 50行（新規関数） |
| `ArtifactCore/include/Script/Expression/ExpressionEvaluator.ixx` | 3 | 10行（宣言追加） |

## 結論

**Expression エンジンはコア部分が実装済みで、製品統合は未完了である。** `ExpressionParser` / `ExpressionEvaluator`、標準関数、時間評価、ASTキャッシュ、`thisComp.layer()`、Copilotの補完・検証・エラーハイライトは確認できた。残る主な差分は、実レイヤーの transform/marker 解決、プロパティ評価側の `index`、Pick Whip、追加AE関数、および runtime 受入れである。
