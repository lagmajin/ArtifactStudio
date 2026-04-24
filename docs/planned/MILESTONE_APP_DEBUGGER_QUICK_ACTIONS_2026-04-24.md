# Phase 9 Execution: App Debugger Quick Actions

> 2026-04-24 作成

## 目的

[`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
の改善を、調査中にすぐ使える `quick actions` に落とし込むための実行メモ。

この段階では、画面を読むだけでなく、読む前後の操作を短くする。

---

## 方針

1. よく使う操作を 1 クリックで触れるようにする
2. 調査の流れを壊さない位置に置く
3. copy / pin / compare / filter を近接配置する
4. 破壊的な操作は quick actions に混ぜない

---

## 現状の課題

- 読む前に何度もタブや詳細を行き来してしまう
- copy / export / pin / compare が別々の場所に散りやすい
- 調査の途中で操作導線を見失いやすい
- 似たボタンが多いと、使うたびに迷う
- 重要操作が深い場所にあると、見やすさ改善が活きにくい

---

## 実装タスク

### 1. Quick Copy を置く

やること:

- current report の copy
- selected frame の copy
- short summary の copy

をひとまとめにする。

完了条件:

- 共有用 text をすぐ持ち出せる

### 2. Quick Pin を置く

やること:

- current frame の pin
- current pass の pin
- current resource の pin

を近い場所に置く。

完了条件:

- 調査対象をすぐ固定できる

### 3. Quick Compare を置く

やること:

- baseline を current にする
- target を current にする
- compare を切り替える

を最短で行えるようにする。

完了条件:

- compare の開始が迷わずできる

### 4. Quick Filter を置く

やること:

- failed
- warning
- fallback
- missing
- selected

の絞り込みを素早く切り替えられるようにする。

完了条件:

- 調査対象を 1 操作で狭められる

---

## レイアウト方針

- 上部: summary / legend / quick actions
- 左側: filter / pin list
- 中央: focused evidence
- 右側: copy / compare / export

補助ルール:

- quick actions は summary の近くに置く
- 似た操作はまとまったグループにする
- destructive は quick に入れない

---

## 表示ルール

- `copy` は即共有できることを示す
- `pin` は固定対象を示す
- `compare` は差分を示す
- `filter` は読む範囲を絞る

---

## 実装メモ

- `AppDebuggerWidget` の上部に action strip を置く
- `Frame Debug View` に copy / pin の導線を足す
- `compare` は current focus と別グループにする
- `filter` は toggle chip で扱う
- `copy` は summary-first をコピー対象にする

---

## 完了条件

- よく使う操作に迷わない
- 調査の途中で手が止まりにくい
- copy / pin / compare / filter が近い
- 画面を見る流れを壊さない

---

## リスク

- quick actions を増やしすぎると騒がしくなる
- 重要操作と補助操作の差がぼやける
- 画面上部がボタンで詰まりすぎる

---

## 参照

- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_REPORT_SHARE_BUNDLE_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_REPORT_SHARE_BUNDLE_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_LEGEND_SEMANTIC_KEY_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_LEGEND_SEMANTIC_KEY_2026-04-24.md)
