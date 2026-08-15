# MILESTONE: After Effects エクスプレッション標準ライブラリ互換性

作成日: 2026-04-18
**最終更新:** 2026-08-15
優先度: 🔴 最高
推定工数: 3日

---

## 概要

既存の式評価エンジンに対して、After Effects 標準のエクスプレッション関数との互換レイヤーを実装する。

エンジン本体は実装済みだが、AE 互換のグローバル変数と一部の標準関数はまだ詰めが必要です。

---

## ✅ 現在の状況

✅ 式パーサー: 実装済み
✅ 評価エンジン: 実装済み
✅ 演算子優先順位: 基本実装あり
✅ 値型システム: Number/Array/Vector/String 対応
⚠️ AE標準グローバル変数: `thisComp`／`thisLayer` は一部 context 注入、全プロパティ契約は未完了
⚠️ AE標準ライブラリ関数: 基本数学・補間・乱数・音声・`valueAtTime`・loop系は実装、互換ギャップあり

## 現行コード監査 (2026-08-15)

`ExpressionEvaluator::registerStandardFunctions()` で sin/cos、linear/ease、clamp、vector、random/noise、wiggle/smooth、audio、`valueAtTime`、`loopIn/Out`、duration版が登録されている。loop系は `keyframes`／`time`／`value` context、cycle／pingpong／continue／offset、0／1キーフレームのフォールバック、duration／keyframe count を扱う実装がある。

旧表の「loop系未実装」および標準関数の未登録という判定は現状と一致しない。一方、`index`、AEの追加グローバル／レイヤー API、`posterizeTime`／`seedRandom`／`toWorld` 系、time remapとの完全な組み合わせ、AE互換の網羅受入は未確認である。基本stdlib基盤は実装済みだが、マイルストーン全体は未完了とする。

---

## 🎯 実装目標

After Effects 2025 のエクスプレッション言語との段階的な互換性確保

既存のAEエクスプレッションを順番に取り込めること。

---

## 📋 実装リスト

### Phase 1 (1日目)

| 関数名 | 状況 | 優先度 |
|--------|------|--------|
| `time` | ✅ 既存 | 💯 |
| `value` | ✅ 既存 | 💯 |
| `index` | ⚠️ 未実装 | 💯 |
| `thisLayer` | ⚠️ 部分実装 | 💯 |
| `thisComp` | ⚠️ 部分実装 | 💯 |
| `linear()` | ✅ 実装済み | 💯 |
| `ease()` | ✅ 実装済み | 💯 |
| `easeIn()` | ✅ 実装済み | 💯 |
| `easeOut()` | ✅ 実装済み | 💯 |
| `clamp()` | ✅ 既存 | 💯 |

### Phase 2 (2日目)

| 関数名 | 優先度 |
|--------|--------|
| `sin()` / `cos()` / `tan()` | 🔴 |
| `degToRad()` / `radToDeg()` | 🔴 |
| `length()` | 🔴 |
| `distance()` | 🔴 |
| `normalize()` | 🔴 |
| `lookAt()` | 🟠 |
| `random()` | 🟠 |
| `seedRandom()` | 🟠 |

### Phase 3 (3日目)

| 関数名 | 優先度 |
|--------|--------|
| `sourceRectAtTime()` | 🔴 |
| `loopOut()` | 🟠 |
| `loopIn()` | 🟠 |
| `posterizeTime()` | 🟠 |
| `wiggle()` | 🟠 |
| `smooth()` | 🟡 |

---

## 🛠 実装計画

1.  **`ExpressionEvaluator.cppm`** に互換レイヤークラスを追加
2.  グローバルスコープにAE互換関数を登録
3.  レイヤー参照システムを統合
4.  単体テストをAEの出力結果と比較
5.  既存の式を破壊しないことを確認

---

## ✅ 完了条件

- AEの公開エクスプレッションサンプル100個が全て同じ結果を返す
- `linear()` `ease()` の動作がpixel単位で一致
- 既存のプロジェクトファイルが全て破壊されない

---

## 💡 技術的メモ

既存のエンジンは非常に良く設計されており、互換レイヤーの追加は全く難しくありません。
このマイルストーンが完了すると、インターネット上に存在する殆ど全てのAfter Effectsエクスプレッションがそのまま動作するようになります。


---

## Static audit follow-up (2026-07-25)

ExpressionEvaluator::registerStandardFunctions() では math／vector、linear／ease、random／noise／wiggle、audio、valueAtTime／loop 系の登録が確認でき、	hisComp／	hisLayer も変数モデルと Copilot 候補が存在する。

一方、index の標準変数、lookAt、seedRandom、sourceRectAtTime、posterizeTime、smooth は互換実装として確認できない。loop 系は登録されているが placeholder であるため、標準ライブラリ互換の完了とは扱えない。AE サンプル 100 件との比較や pixel 単位一致も未検証であり、現状は Phase 1〜2 の一部と Phase 3 の入口のみ実装済みと記録する。
