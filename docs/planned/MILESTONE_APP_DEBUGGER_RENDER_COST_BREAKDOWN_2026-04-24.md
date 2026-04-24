# Phase 12 Execution: App Debugger Render Cost Breakdown

> 2026-04-24 作成

## 目的

[`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
の改善を、描画コストの内訳を見て原因を切り分ける `render cost breakdown` に落とし込むための実行メモ。

この段階では、単純な GPU/CPU 総時間だけでなく、何が積み上がっているかを先に見えるようにする。

---

## 方針

1. `draw call` を最優先で見る
2. `PSO / SRB` 切り替えを次に見る
3. `buffer update` の頻度をその次に見る
4. `CPU/GPU` 総時間は最後に置く

---

## 優先順位

### A. Draw Call 数

- 平面 1 枚に対して何 call 出ているか
- 枠線、ギズモ、補助表示、マスク境界で増えていないか
- まずここが一番説明力が高い

### B. PSO / SRB 切り替え数

- 2D 描画なのに毎回切り替えていないか
- state change が多いと CPU 側の司令コストが増える
- draw call が少なくても重くなる原因になる

### C. Buffer 更新回数

- 定数バッファを primitive 単位で更新していないか
- dynamic buffer 更新が細かすぎないか
- CPU 側の細切れ更新をあぶり出す

### D. CPU / GPU 時間

- CPU 15ms / GPU 2ms なら司令側が主犯
- CPU 2ms / GPU 15ms なら ROI やピクセル処理側が主犯
- ただしこれは最後の判定として使う

---

## 現状の課題

- 総時間だけでは、何が効いているか分かりにくい
- 平面レイヤーなのに重いとき、原因が隠れやすい
- state change と buffer update が見えないと、無駄な細分化に気づきにくい
- `overlay` / `present` が重いときに、実際の内訳が分からない
- CPU/GPU の総時間だけだと、司令側か描画側かの切り分け止まりになる

---

## 実装タスク

### 1. Draw Call Summary を置く

やること:

- total draw call
- per-layer draw call
- overlay draw call
- gizmo / helper draw call

を出す。

完了条件:

- 平面 1 枚あたりの call 数が分かる

### 2. PSO / SRB Switch Summary を置く

やること:

- PSO switch count
- SRB switch count
- pipeline bind churn

を出す。

完了条件:

- state change の多さが見える

### 3. Buffer Update Summary を置く

やること:

- uniform / constant buffer update count
- dynamic buffer update count
- per-frame update granularity

を出す。

完了条件:

- 細切れ更新が見える

### 4. CPU / GPU Time Summary を補助にする

やること:

- CPU total
- GPU total
- CPU/GPU ratio

を最後に出す。

完了条件:

- 最後にどちら側が主犯か分かる

---

## レイアウト方針

- 上部: draw calls / pso-srb / buffer updates / cpu-gpu summary
- 中央: layer breakdown / pass breakdown
- 右側: warning / hints / next action
- 下部: raw timing table

補助ルール:

- A/B/C を上に出す
- D はまとめとして下げる
- 総時間より内訳を先に出す

---

## 表示ルール

- `draw calls` は最優先指標
- `pso/srb` は state change の指標
- `buffer updates` は細切れ更新の指標
- `cpu/gpu` は最終判定の指標

---

## 実装メモ

- `AppDebuggerWidget` の summary に A/B/C を寄せる
- `Frame Debug View` に cost breakdown を入れる
- `overlay / present` の内訳を別行で見せる
- `duration` だけでなく `count` を表示する
- `next action` に `reduce calls` / `reduce switches` / `coalesce updates` を出す

---

## 完了条件

- 平面レイヤー 1 枚あたりの負荷の内訳が分かる
- state change と buffer update の多さが見える
- CPU/GPU 総時間だけでなく原因候補を先に見られる
- どこを減らすべきかが読み取れる

---

## リスク

- 指標を増やしすぎると見づらくなる
- 数値が多いと、逆に総覧性が落ちる
- A/B/C/D の順番を崩すと原因特定が遅くなる

---

## 参照

- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`MILESTONE_APP_DEBUGGER_AUTO_FOCUS_SMART_RANKING_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_AUTO_FOCUS_SMART_RANKING_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_QUICK_ACTIONS_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_QUICK_ACTIONS_2026-04-24.md)
