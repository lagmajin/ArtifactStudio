# Milestone: Expression System (2026-03-29)

**Status:** Not Started
**Goal:** After Effects 風のエクスプレッションでプロパティ値を動的に制御する。
キーフレーム以外のアニメーション制御を可能にする。

---

## コンセプト

```
// 例: サイン波で位置を揺らす
value = [position.x, position.y + Math.sin(time * 5) * 30];

// 例: 親レイヤーに追従
parent.position;

// 例: ランダム揺れ
wiggle(5, 20);

// 例: ピックウィップで接続された値
thisComp.layer("Control").effect("Slider")("Slider");
```

---

## 類似ツール

| ツール | 実装 |
|--------|------|
| **After Effects** | ExtendScript-based expression engine |
| **Blender** | Python driver expressions |
| **Nuke** | TCL + Python expressions |
| **Fusion** | Lua expressions |

---

## 現状

| 機能 | 状態 |
|------|------|
| エクスプレッション UI (「=」ボタン) | ⚠️ UI のみ、評価エンジンなし |
| TimeRemap の getSpeedAtTime | ✅ 実装済み |
| TextAnimatorEngine の式ベース weight | ✅ 実装済み |
| エクスプレッションパーサー | ❌ 未実装 |
| エクスプレッションランタイム | ❌ 未実装 |

---

## Architecture

```
ExpressionEngine (Core)
  ├── ExpressionParser        ← 式文字列を AST に変換
  │     ├── Tokenizer (字句解析)
  │     ├── Parser (構文解析)
  │     └── AST nodes (数式、関数呼び出し、プロパティ参照)
  ├── ExpressionEvaluator     ← AST を評価
  │     ├── Math functions (sin, cos, abs, clamp, etc.)
  │     ├── Animation functions (wiggle, loop, time, value)
  │     ├── Layer references (thisLayer, parent, comp)
  │     └── Property references (position, scale, rotation, opacity)
  ├── ExpressionContext        ← 評価コンテキスト
  │     ├── currentFrame
  │     ├── currentLayer
  │     ├── currentComp
  │     └── time (seconds)
  └── BuiltinFunctions         ← 組み込み関数ライブラリ
        ├── Math: sin, cos, tan, abs, min, max, clamp, pow, sqrt, random
        ├── String: length, substr, concat
        ├── Array: length, push, pop
        ├── Animation: wiggle, loopIn, loopOut, ease, linear
        └── Comp: comp("name").layer("name").property
```

---

## Milestone 1: Expression Parser (AST)

### Implementation
- Tokenizer: 数値、演算子、識別子、文字列、ドットアクセス
- Parser: 再帰下降パーサーで AST 構築
- AST ノード: Literal, BinaryOp, UnaryOp, FunctionCall, PropertyAccess, Assignment

### Grammar (subset):
```
expression  → assignment
assignment  → IDENTIFIER '=' expression | ternary
ternary     → logical ('?' expression ':' expression)?
logical     → comparison (('&&' | '||') comparison)*
comparison  → addition (('<' | '>' | '<=' | '>=' | '==' | '!=') addition)*
addition    → multiplication (('+' | '-') multiplication)*
multiplication → unary (('*' | '/') unary)*
unary       → ('-' | '!') unary | call
call        → primary ('(' arguments ')' | '.' IDENTIFIER | '[' expression ']')*
primary     → NUMBER | STRING | IDENTIFIER | '(' expression ')'
arguments   → expression (',' expression)*
```

### 見積: 8h

---

## Milestone 2: Expression Evaluator

### Implementation
- AST ウォーカーで各ノードを評価
- 型システム: float, vec2, vec3, color, string, array, layer, comp
- 組み込み関数の実装
- プロパティ参照の解決

### 組み込み関数 (最低限):
```cpp
// Math
float sin(float x);     float cos(float x);
float abs(float x);     float clamp(float x, float lo, float hi);
float min(float a, b);  float max(float a, b);
float pow(float a, b);  float sqrt(float x);
float random();         float random(float max);

// Animation
float time;             // 現在の秒数
float value;            // 現在のキーフレーム補間値
vec2 wiggle(float freq, float amp);
vec2 ease(float t, tMin, tMax, value1, value2);
vec2 linear(float t, tMin, tMax, value1, value2);

// Layer
layer.position;    layer.scale;    layer.rotation;    layer.opacity;
layer.inPoint;     layer.outPoint;
```

### 見積: 10h

---

## Milestone 3: Property Integration

### Implementation
- 各プロパティに「エクスプレッション有効」フラグを追加
- キーフレーム評価の代わりに式を評価
- ピックウィップでレイヤー/プロパティ参照を設定
- 式エディタの UI **(AE差別化: シンタックスハイライト、組み込み変数補完、エラー位置ハイライト、独自DSL検討)**

### 見積: 6h

---

## Milestone 4: Built-in Presets

### 実装済みプリセット:
- `wiggle(freq, amp)` — ランダム揺れ
- `loopIn()` / `loopOut()` — ループ
- `bounce` — 弾み
- `overshoot` — オーバーシュート
- `smooth` — スムーズ補間
- `snap` — スナップ

### 見積: 4h

---

## Future: AE互換機能拡張 (After Effects Compatibility Extensions)

### Phase 2: 高度な数学・変換関数 (Advanced Math & Conversion)

**優先度: 高**
- `degrees()` / `radians()` - 度数↔ラジアン変換
- `linear()` with more parameters
- `easeIn()` / `easeOut()` with custom curves
- `posterizeTime()` - 時間を量子化

### Phase 3: 時間・アニメーション関数 (Time & Animation)

**優先度: 高**
- `loopIn()` / `loopOut()` / `loopInDuration()` - ループ制御
- `valueAtTime()` - 指定時間の値を取得
- `velocityAtTime()` / `speedAtTime()` - 速度/加速度
- `nearestKey()` / `key()` - キーフレーム操作

### Phase 4: 空間・変換関数 (Spatial & Transform)

**優先度: 高**
- `fromWorld()` / `toWorld()` - ワールド座標変換
- `fromComp()` / `toComp()` - コンポジション座標変換
- `lookAt()` - 方向ベクトル計算

### Phase 5: 色関数 (Color Functions)

**優先度: 中**
- `rgbToHsl()` / `hslToRgb()` - 色空間変換
- `hexToRgb()` / `rgbToHex()` - 16進数色変換
- `temperatureToRgb()` - 色温度変換

### Phase 6: 文字列関数 (String Functions)

**優先度: 中**
- `substring()` / `indexOf()` - 文字列操作
- `text.style()` - テキストスタイル取得
- `text.sourceText` - テキストレイヤーのソース

### Phase 7: レイヤー・プロパティ関数 (Layer & Property)

**優先度: 中**
- `hasParent` - 親レイヤー存在チェック
- `parent` - 親レイヤー参照
- `index` - レイヤーインデックス
- `name` - レイヤー名

### Phase 8: 物理・シミュレーション関数 (Physics & Simulation)

**優先度: 低**
- `inertia()` - 慣性ベースの動き
- `bounce` - バウンス効果
- `elastic()` - ゴムのような動き

### Phase 9: パス・シェイプ関数 (Path & Shape)

**優先度: 低**
- `createPath()` - パス作成
- `points()` / `inTangents()` / `outTangents()` - パスデータアクセス
- `pathCount()` - パスの数

### 実装順序推奨 (Implementation Priority)

| Phase | 機能カテゴリ | 見積 | 理由 |
|-------|--------------|------|------|
| **Phase 2** | 高度な数学関数 | 6-8h | アニメーション制作の基礎 |
| **Phase 3** | 時間・アニメーション | 8-10h | アニメーション制御に必須 |
| **Phase 4** | 空間・変換 | 6-8h | レイヤー操作に重要 |
| **Phase 5** | 色関数 | 4-6h | 視覚効果制作で便利 |
| **Phase 6** | 文字列関数 | 4-6h | テキストアニメーション |
| **Phase 7** | レイヤープロパティ | 4-6h | レイヤー操作の質向上 |
| **Phase 8** | 物理シミュレーション | 6-8h | 特殊効果・高度な使い手向け |
| **Phase 9** | パス・シェイプ | 8-12h | シェイプアニメーション |

**総見積 (将来拡張): ~50-70h**

---

## Recommended Order

| 順序 | マイルストーン | 見積 |
|---|---|---|
| 1 | **M1 Parser** | 8h |
| 2 | **M2 Evaluator** | 10h |
| 3 | **M3 Property Integration** | 6h |
| 4 | **M4 Presets** | 4h |

**総見積: ~28h**

---

## Deliverables

| ファイル | 内容 |
|---------|------|
| `ArtifactCore/include/Expression/ExpressionParser.ixx` | パーサー |
| `ArtifactCore/src/Expression/ExpressionParser.cppm` | 実装 |
| `ArtifactCore/include/Expression/ExpressionEvaluator.ixx` | 評価エンジン |
| `ArtifactCore/src/Expression/ExpressionEvaluator.cppm` | 実装 |
| `ArtifactCore/include/Expression/ExpressionContext.ixx` | 評価コンテキスト |
| `ArtifactCore/include/Expression/BuiltinFunctions.ixx` | 組み込み関数 |

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm` | 1044-1047 | 「=」ボタン (スタブ) |
| `ArtifactCore/src/Text/TextAnimator.cppm` | 13-88 | 式ベース weight 計算 (参考) |
| `ArtifactCore/src/Time/TimeRemap.cppm` | 152 | getSpeedAtTime (参考) |
