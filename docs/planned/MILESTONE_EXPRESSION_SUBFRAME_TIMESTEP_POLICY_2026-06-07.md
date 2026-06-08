# Expression Subframe / Timestep Policy Milestone

**作成日:** 2026-06-07  
**ステータス:** 計画中  
**関連コンポーネント:** ExpressionEvaluator, ExpressionContext, Property Integration, Time System

---

## 概要

Expression の評価をフレーム境界だけに固定せず、サブフレーム評価と可変時間ステップを選べるようにするためのマイルストーンです。

目的は「time を使う式が 30fps と 60fps で別物になりにくい」ことです。  
特に物理演算、バネ、摩擦、慣性、緩和系の表現で、フレームレート依存を減らします。

---

## 背景

現状の expression は評価自体は可能でも、時間の刻み方がフレーム依存だと挙動が変わります。

- 30fps と 60fps で同じ式でも見た目が変わる
- `time` が実質フレーム単位の proxy になりやすい
- 物理系の式を作ると、速度や減衰が frame rate に引きずられる

この milestone は、式の文法を増やすのではなく、**評価の時間解像度と積分方法を選べるようにする**ことを狙います。

---

## 目標

- サブフレーム評価を許可する
- 評価モードを式単位またはプロパティ単位で選べるようにする
- 物理系式には内部可変時間ステップを使えるようにする
- パフォーマンスとのトレードオフをユーザーに見せる
- フレームレート差による挙動差を抑える

---

## 評価モデル

### 1. Frame Locked

- 現状に近い
- 1 フレームごとの評価
- 軽い
- 既存互換を保ちやすい

### 2. Subframe Sampled

- フレーム内の任意時刻で評価する
- 補間や微小変化の取得に使う
- 表示は滑らかになるが、呼び出し回数は増える

### 3. Adaptive Step

- 物理系や再帰式のために内部ステップを細かく刻む
- 変化量が大きい時だけ細かく分割する
- 安定性を優先するが重くなる

### 4. Fixed Microstep

- ユーザー指定の固定微小刻み
- 予測しやすい
- ただしコストは一定で高くなりやすい

---

## Phase 構成

### Phase 1: Time Evaluation Contract

- expression evaluator に評価モードを持たせる
- `frame locked / subframe / adaptive / fixed microstep` を定義する
- `time` の意味を「評価時刻」として固定する
- 評価モードの既定値を決める

完了条件:

- 評価モードが式評価の前提として参照できる
- 既存の expression が壊れない

### Phase 2: Subframe Sampling

- 任意時刻で expression を評価できる
- property integration 側からサブフレーム時刻を渡せる
- `valueAtTime` 系と整合する

完了条件:

- 1 フレームの中間時刻でも式が評価できる

### Phase 3: Adaptive Physics Step

- 物理系式用に内部ステップを分割できる
- 高速移動や強い力があるときだけ刻みを細かくする
- 収束条件と最大ステップ数を設定する

完了条件:

- バネや慣性の動きが frame rate 依存になりにくい

### Phase 4: User Tradeoff Controls

- パフォーマンス優先 / 安定性優先 / 自動 の選択を出す
- サブフレームを使うほど重くなることを明示する
- expression ごとの推奨モードを提示する

完了条件:

- ユーザーがコストと滑らかさを選べる

### Phase 5: Profiling and Diagnostics

- 評価回数
- サブフレーム使用率
- ステップ分割回数
- 最悪ケースの重さ

を可視化する

完了条件:

- どの式が重いかを追える

---

## 実装順

1. 評価モードの enum / policy 定義
2. subframe sampling の導線追加
3. adaptive microstep の実装
4. UI でのモード選択
5. diagnostics / profiling

---

## 対象範囲

- `ExpressionEvaluator`
- `ExpressionContext`
- property integration
- expression editor / copilot
- diagnostics / profiler

---

## リスクと留意点

- 毎回サブフレーム評価すると重くなる
- 既存のキーフレーム補間との責務境界を曖昧にしない必要がある
- 物理系の安定化は式だけでは足りず、積分器の設計が必要
- 再帰や loop 系と組み合わせると計算量が急増しやすい

---

## 成功条件

- 同じ expression が 30fps / 60fps で極端に別挙動になりにくい
- 物理系は必要に応じて subframe を使える
- ユーザーが性能と精度のバランスを選べる
- expression の時間解像度が明文化される

---

## 関連

- `docs/planned/MILESTONE_EXPRESSION_SYSTEM_2026-03-29.md`
- `docs/planned/MILESTONE_SCRUB_EXPRESSION_CACHE_REUSE_2026-06-07.md`
