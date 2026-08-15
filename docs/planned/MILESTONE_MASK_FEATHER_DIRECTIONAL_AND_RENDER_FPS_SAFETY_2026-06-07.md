# MILESTONE: Mask Feather Directional / Render FPS Safety - 2026-06-07

**ステータス:** Mask directional feather 実装済み、FPS safety 統合・runtime/export検証 pending

**最終更新:** 2026-08-15

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
- [x] horizontal / vertical feather を追加する（MaskPath に既存: featherHorizontal/Vertical）
- [x] uniform feather からの変換を定義する（rasterizeToAlpha で 0 のとき uniform にフォールバック）
- [x] inspector に 2 軸表示を出す（ArtifactAbstractLayer プロパティ公開済み、PropertyWidget ラベル解決済み）

#### Phase 2: inner / outer split
- [x] inner feather / outer feather を追加する（MaskPath に既存: featherInner/Outer）
- [x] 内外で別の半径を持てるようにする（rasterizeToAlpha で dilate/erode + max 合成）
- [x] mask edge 評価の順序を固定する

#### Phase 3: preview / parity
- [x] viewport overlay で差が読めるようにする（タイムライン表示: ArtifactTimelineKeyframeModel に H/V/Inner/Outer ラベル解決を追加 2026-06-15）
- [x] existing mask shape と矛盾しないようにする（既存 feather は uniform として維持、0 のときフォールバック）
- [x] undo / redo を通す（プロパティ経路は既存 Undo 基盤を使用）
- [x] mask 永続化: ArtifactAbstractLayer::toJson/fromJsonProperties に masks 配列を追加（vertices, feather×5, keyframes 含む全属性）。これまで mask はプロジェクトファイルに保存されなかった（2026-06-15 追記）

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

- [`docs/done/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`](X:/Dev/ArtifactStudio/docs/done/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md)
- [`docs/planned/MILESTONE_RENDER_OUTPUT_FEEL_REFINEMENT_2026-03-27.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RENDER_OUTPUT_FEEL_REFINEMENT_2026-03-27.md)
- [`docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md)
- [`docs/planned/MILESTONE_AE_STYLE_MASK_EDITING_GRAMMAR_2026-06-02.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE_STYLE_MASK_EDITING_GRAMMAR_2026-06-02.md)
- [`docs/planned/MILESTONE_UNIFIED_AUDIO_VIDEO_RENDER_OUTPUT_2026-03-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_UNIFIED_AUDIO_VIDEO_RENDER_OUTPUT_2026-03-28.md)

---

## 備考

- これは実装タスクの詳細設計ではなく、制作事故を減らすための追加マイルストーン。
- ビルドやテストは実施していない。

---

## Static audit follow-up (2026-07-25)

`MaskPath` の feather 評価経路、render performance monitor、batch/output settings、Render Preflight 関連を現行ソースで照合した。ビルド・実機出力・FPS実測は未実施。

| 領域 | 現状 | 判定 |
|---|---|---|
| Directional mask feather | `MaskPath` に uniform／horizontal／vertical／inner／outer feather があり、保存・補間・CPU mask評価で非対称値を使用する。 | 実装済み／runtime確認待ち |
| Feather compatibility | directional値が未指定のとき uniform値へフォールバックする経路を確認した。 | 実装済み／網羅確認待ち |
| Export FPS default sync | composition frame rate と render context／creation defaults の接続はあるが、全 export dialog／job作成での自動同期は未確認。 | 部分実装 |
| FPS mismatch visibility / preflight | Render performance monitor と preflight基盤は存在するが、`composition: Xfps / output: Yfps` の専用 mismatch warning／強調表示は未確認。 | 部分実装／統合待ち |
| Render FPS safety | current／average FPS、frame budget、target FPS の計測基盤はあるが、目標値に対する出力安全ゲートの実測は未実施。 | 部分実装／検証待ち |

### 現在の判定

Mask directional feather は主要実装が存在する一方、Render/Export FPS safety は設定同期と mismatch warning の統合が未確定。全体は「部分実装／runtime・export確認待ち」とする。

## 現行コード監査 (2026-08-15)

`MaskPath` は uniform／horizontal／vertical／inner／outer feather を保持・補間・JSON 復元し、CPU 評価では方向別の X/Y blur と内外 feather、未指定時の uniform fallback を実装している。`LayerMask` の合成経路も確認できるため、directional feather の core 実装は現行コードでも成立している。

一方、FPS 側は render queue／preview の frame budget・performance 計測と composition／stream の frame rate 情報までは存在するが、全 export job の FPS 自動同期、専用 mismatch warning、failure を防ぐ出力ゲート、実測による安全性は確認できない。したがって export/runtime 部分を完了扱いにはしない。

判定: **directional feather は実装済み。FPS safety は計測・設定素材までで、export 統合と runtime 検証が pending。**
