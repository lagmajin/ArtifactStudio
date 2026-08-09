# Expression Engine Audit & Completion (2026-08-08)

**最終更新:** 2026-08-08（第二稿：ソース再確認後）
**状態:** 監査完了。コアエンジンは95%実装済み。残りはレイヤープロパティ参照の深追い、UI改善、追加組込関数。

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

**問題**: 自動補完には `index` が表示されるが、`evaluateValue()` では注入されていない。

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

### Gap 4: ピックウィップ + エラーハイライト 🟡

**問題**:
- プロパティ間の参照をドラッグで作成するピックウィップ UI がない
- エラー位置情報（Parser: `getErrorPosition()` / `getErrorLength()`）は返るが、UI で赤波線表示していない
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

**Expression エンジンは95%完成している。** Gap 1+2（合計 中＋低 コスト）で AE 互換の全基本機能が揃う。最初の想定より完成度が劇的に高かった。
