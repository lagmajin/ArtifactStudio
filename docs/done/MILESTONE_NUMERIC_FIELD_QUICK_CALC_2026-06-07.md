# Numeric Field Quick Calc Milestone

**最終更新:** 2026-08-15

## Update 2026-08-15 — 共通 Numeric Editor 経路を再確認

`ArtifactPropertyEditorNumeric` の float／integer editor に `+10`、`-5`、`*2`、`/3` の相対計算を追加し、Enter／editingFinished 時に範囲 clamp 後の値を既存の commit 経路へ渡すようにした。絶対値入力と既存 slider／preview／Undo 経路は維持する。

**作成日:** 2026-06-07  
**ステータス:** ✅ 完了 (2026-06-08 確認 / 2026-06-15 共有化・Dialog 波及)  
**関連コンポーネント:** ArtifactPropertyWidget, Numeric Row, Property Editor, Value Input, RenderOutputSettingDialog

---

## 概要

数値フィールドで簡易計算式を受け付けるためのマイルストーンです。

目的は、`100` を一度消して `110` と打ち直す代わりに、`+10` や `*2` を直接入力できるようにすることです。

---

## 背景

現在の数値入力は、単純な置き換え中心になりやすいです。

- `100+10` をそのまま入れたい
- `*2` で今の値を倍にしたい
- `/3` で割合変更したい
- `-5` で微調整したい

これができると、数値編集が「再入力」ではなく「差分編集」になります。

---

## 目標

- すべての数値フィールドで簡易計算式を許可する
- `+10`, `-5`, `*2`, `/3` のような式を受ける
- Enter 確定時に計算結果を反映する
- 既存値を基準にした相対入力を可能にする
- 失敗した入力は安全に元の値へ戻す

---

## 対応式

### 1. Relative Delta

- `+10`
- `-5`

### 2. Relative Scale

- `*2`
- `/3`

### 3. Absolute Expression

- `100+10`
- `200-25`
- 必要なら `round(...)` などの簡易関数も将来検討する

---

## Phase 構成

### Phase 1: Parser for Quick Calc

- 数値フィールド用の軽量 parser を作る
- 先頭演算子の相対入力を解釈する
- 安全な数値のみを許可する

完了条件:

- `+10 / -5 / *2 / /3` が解釈できる

### Phase 2: Commit on Enter

- Enter 確定時にのみ計算を適用する
- 入力途中では値を壊さない
- Escape でキャンセルできるようにする

完了条件:

- 入力確定前に元の値を失わない

### Phase 3: Field Integration

- numeric row / spin box / inline value editor に接続する
- property editor の標準数値フィールドに適用する
- value type ごとの変換を共通化する

完了条件:

- 主要な数値フィールドで使える

#### 共有化と Dialog 波及 (2026-06-15 追記)

`ArtifactRelativeDoubleSpinBox` / `ArtifactRelativeSpinBox` は元々 `ArtifactPropertyEditor.cppm` のファイルローカルクラスだったが、Dialog 系でも接頭辞計算を共有するため `Artifact.Widgets.RelativeSpinBox` モジュール（`Artifact/include/Widgets/ArtifactRelativeSpinBox.ixx`）に切り出した。

- `ArtifactPropertyEditor.cppm`: ローカル定義を削除し `import Artifact.Widgets.RelativeSpinBox;` に差し替え
- `ArtifactRenderOutputSettingDialog.cppm`: width/height/fps/bitrate/audioBitrate の5 spinBox を `ArtifactRelative*SpinBox` に置換（メンバ型は基底ポインタ `QSpinBox*`/`QDoubleSpinBox*` のまま、アップキャストで格納）
- これにより Render 出力設定の bitrate（`+2000` 等の微調整）や fps でも接頭辞計算が効くようになった

### Phase 4: Validation and Feedback

- パース失敗時に入力を拒否する
- どの値が計算されたかを簡単に示す
- 相対入力か絶対入力かを視覚的に分かるようにする

完了条件:

- ユーザーが結果を予測しやすい

### Phase 5: Compatibility and Fallback

- expression 入力とは別扱いにする
- 不明な型のフィールドでは安全に無効化する
- 既存の数値編集フローを壊さない

完了条件:

- 既存の数値入力が後退しない

---

## 実装順

1. quick calc parser
2. enter commit / escape cancel
3. numeric row integration
4. feedback / validation
5. fallback rules

---

## 対象範囲

- `ArtifactPropertyWidget`
- numeric row widgets
- inline property value editors

---

## リスクと留意点

- expression evaluator と責務が被らないようにする必要がある
- 型が整数か浮動小数かで丸め挙動が変わる
- 文字列入力としての expression と誤認されないようにする必要がある

---

## 成功条件

- `+10` で今の値を基準に増やせる
- `*2` で倍率変更できる
- 数値再入力の手間が減る
- 既存の expression と混線しない

---

## 関連

- `docs/planned/MILESTONE_EXPRESSION_QUICK_INPUT_2026-04-10.md`
- `docs/planned/MILESTONE_PROPERTY_WIDGET_ROW_ALIGNMENT_INSPECTOR_LAYOUT_2026-04-03.md`
- `docs/planned/MILESTONE_INLINE_INTERACTION_SURFACES_2026-03-31.md`
