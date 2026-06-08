# MILESTONE: Mask Feather Directional / Render FPS Safety - 2026-06-07

作成日: 2026-06-07  
対象: mask feather と render/export safety  
優先度: 🟠 高

---

## 目的

このマイルストーンは、次の 2 つの制作ギャップをまとめる。

1. Mask の feather を、単純な均一ぼかしから方向別・内外別に拡張する
2. Export 時の frame rate 不一致を、事前警告と初期値同期で減らす

どちらも「ユーザーが気づいた時には遅い」タイプのミスを減らすのが主題。

---

## 対象ギャップ

### 1. Mask Feather: 均一なぼかししかできない

不満:
- feather が縁全体に均一に掛かる
- 横だけ強く、縦は弱く、といった非対称なぼかしができない
- 内側と外側で feather 量を分けられない

改善:
- horizontal / vertical を個別に制御できるようにする
- inner feather / outer feather を別々に設定できるようにする
- 単一の feather 値は互換用の簡易モードとして残す

完了条件:
- 横・縦で別の feather が指定できる
- 内側 / 外側が別々に制御できる
- 既存の mask 表現と後方互換を保つ

---

### 2. Render / Export FPS Safety: 出力時に frame rate を間違えやすい

不満:
- comp が 30fps でも、出力モジュールの初期値が 24fps のままになることがある
- 気づかず書き出すと、意図しないコマ落ちや時間ずれが起きる

改善:
- 出力モジュールの frame rate 初期値を、コンポの frame rate に自動同期する
- 手動で変更した frame rate は目立つ色で表示する
- 書き出し前に frame rate mismatch を警告する

完了条件:
- export dialog の初期値が comp に一致する
- mismatch が見える
- うっかり誤設定で出力する確率が下がる

---

## 実装の読み替え

### Mask Feather

単なる blur の強さではなく、mask edge のどの方向にどれだけ効かせるかを分ける。

- horizontal / vertical を独立パラメータにする
- inner / outer を別半径として扱う
- 旧 feather は `uniform feather` として変換する

### Render FPS Safety

単なる出力設定の初期値問題ではなく、render/export 前の safety check として扱う。

- 既定値は comp に合わせる
- 変更済み値は強調する
- mismatch は preflight の warning へ流す

---

## 詳細実装スライス

### A. Mask Feather Directional

#### 入口
- Mask Path property
- Inspector の mask section
- Composition / layer overlay

#### 触るもの
- `MaskPath`
- `ArtifactAbstractLayer` の mask property wiring
- `ArtifactCompositionRenderController`
- mask editor / property editor

#### データ契約
- uniform feather
- horizontal feather
- vertical feather
- inner feather
- outer feather
- feather mode

#### 実装順
1. 既存 feather を uniform モードとして保持する
2. horizontal / vertical を追加する
3. inner / outer を追加する
4. render path の評価を 1 箇所にまとめる
5. inspector 表示を揃える

#### 失敗時の扱い
- 値が未指定なら uniform にフォールバックする
- 互換モードで意味が壊れる場合は警告を出す
- 方向別設定が render に反映されないケースは止める

#### Phase 1: direction split
- [ ] horizontal / vertical feather を追加する
- [ ] uniform feather からの変換を定義する
- [ ] inspector に 2 軸表示を出す

#### Phase 2: inner / outer split
- [ ] inner feather / outer feather を追加する
- [ ] 内外で別の半径を持てるようにする
- [ ] mask edge 評価の順序を固定する

#### Phase 3: preview / parity
- [ ] viewport overlay で差が読めるようにする
- [ ] existing mask shape と矛盾しないようにする
- [ ] undo / redo を通す

#### 具体的 UI 案
- `Feather X`
- `Feather Y`
- `Inner Feather`
- `Outer Feather`
- `Uniform` toggle

---

### B. Render / Export FPS Safety

#### 入口
- Export / Render dialog
- Render queue job settings
- Composition menu / output settings

#### 触るもの
- render output setting dialog
- render queue job metadata
- composition frame rate
- preflight warning surface
- output preset

#### データ契約
- composition frame rate
- output frame rate
- preset default frame rate
- user override state
- mismatch warning state

#### 実装順
1. 出力モジュール初期値を comp に同期する
2. 変更済み frame rate を目立たせる
3. mismatch を preflight warning に流す
4. export 前確認を追加する
5. job / preset の保存を揃える

#### 失敗時の扱い
- comp が未選択なら既定値へフォールバックする
- 変更済み値を自動で上書きしない
- mismatch が解消できない場合は警告を残す

#### Phase 1: default sync
- [ ] export dialog の frame rate 初期値を comp と同じにする
- [ ] 新規 job 作成時も comp 値を継承する
- [ ] mismatch しない限り同じ値を維持する

#### Phase 2: visual emphasis
- [ ] 手動変更された frame rate を目立つ色で表示する
- [ ] 変更済み preset と未変更 preset を見分けやすくする
- [ ] 出力設定欄に current comp rate を表示する

#### Phase 3: preflight warning
- [ ] export 前に frame rate mismatch を検出する
- [ ] warning から修正導線へ飛べるようにする
- [ ] render queue の job summary に mismatch を出す

#### 具体的 UI 案
- `Use Composition FPS` をデフォルト化する
- `Mismatch` badge を出す
- export dialog の frame rate 行を、変更時だけ強調色にする
- preflight で `composition: 30fps / output: 24fps` のように並べる

---

## 推奨実行順

1. Render / Export FPS Safety
2. Mask Feather Directional

理由:
- export の誤設定は即事故につながるため、先に潰したい
- mask feather は表現力改善として重要だが、既存動作の互換整理を伴うため後で丁寧に進めやすい

---

## 関連

- [`docs/planned/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md)
- [`docs/planned/MILESTONE_RENDER_OUTPUT_FEEL_REFINEMENT_2026-03-27.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RENDER_OUTPUT_FEEL_REFINEMENT_2026-03-27.md)
- [`docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md)
- [`docs/planned/MILESTONE_AE_STYLE_MASK_EDITING_GRAMMAR_2026-06-02.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE_STYLE_MASK_EDITING_GRAMMAR_2026-06-02.md)
- [`docs/planned/MILESTONE_UNIFIED_AUDIO_VIDEO_RENDER_OUTPUT_2026-03-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_UNIFIED_AUDIO_VIDEO_RENDER_OUTPUT_2026-03-28.md)

---

## 備考

- これは実装タスクの詳細設計ではなく、制作事故を減らすための追加マイルストーン。
- ビルドやテストは実施していない。
