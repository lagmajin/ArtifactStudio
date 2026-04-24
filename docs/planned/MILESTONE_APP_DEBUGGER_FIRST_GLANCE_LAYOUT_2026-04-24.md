# Phase 5 Execution: App Debugger First-Glance Layout

> 2026-04-24 作成

## 目的

[`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
の見やすさ改善を、実際に「最初に見る場所が分かる」レイアウトへ落とし込むための実行メモ。

この段階では、情報量を減らすよりも、視線の迷子を減らすことを優先する。

---

## 方針

1. 最上段に `now / warning / next` の要約を置く
2. 異常があるときだけ、その異常を先頭に出す
3. 同じ重要度のカードを横並びにしすぎない
4. raw text は常時前面ではなく、必要時だけ開く

---

## 現状の課題

- どこを見るべきかが 1 秒で分かりにくい
- `Frame / State / Trace / Resource` が同じ強さで並びやすい
- 成功時と失敗時の差が弱く、視線誘導が働きにくい
- 長い text dump が主役になってしまう
- 重要な警告が summary の下に沈みやすい

---

## 実装タスク

### 1. First-Glance Summary を作る

やること:

- `now` を 1 行で示す
- `warning` があれば最初に出す
- `next` の行動候補を 1 つだけ出す

完了条件:

- 画面を開いてすぐ、どこを見るか分かる

### 2. Layout Priority を固定する

やること:

- 上段: summary / warning strip
- 中段: primary frame / playback / render view
- 下段: details / raw text / export

完了条件:

- 重要情報が上、補助情報が下に自然に分かれる

### 3. Danger-First Ordering を入れる

やること:

- failed pass
- missing resource
- fallback path
- stale cache
- playback stall

これらを、通常の詳細より前に出す。

完了条件:

- 異常時に「まずここを見る」が自然に分かる

### 4. Raw Text を補助化する

やること:

- 長い dump は折りたたみ
- summary first の要約を先に出す
- copy しやすい report と分ける

完了条件:

- text dump が主役を奪わない

---

## レイアウト方針

- 左側: navigate / filter / category
- 中央: いま見るべき主表示
- 右側: details / raw / compare
- 上部: state summary / warning strip

補助ルール:

- 1 画面 1 主役を基本にする
- 異常時は summary を警告色に寄せる
- 正常時は静かに、異常時だけ強くする

---

## 表示ルール

- `now` は短く、現在値だけを見せる
- `warning` は色と位置の両方で目立たせる
- `next` は 1 個に絞る
- `Frame` は時間軸、`State` は状態軸として扱う

---

## 実装メモ

- `AppDebuggerWidget` の top strip を追加する
- `Frame / State / Trace` の順序を再調整する
- `warning` の badge tone を統一する
- `raw text` は default collapse にする
- `compare` は補助情報として右側に寄せる

---

## 完了条件

- 画面を見て 3 秒以内に見る場所が分かる
- 失敗時に異常が先に目に入る
- 成功時は余計な強調がなく静かに見える
- raw text を見なくても、まず判断できる

---

## リスク

- summary を強くしすぎると詳細が隠れる
- 1 画面 1 主役を強くしすぎると比較がしづらくなる
- 折りたたみを増やしすぎると、逆に探索性が下がる

---

## 参照

- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE1_EXECUTION_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE1_EXECUTION_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE2_EXECUTION_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE2_EXECUTION_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE3_EXECUTION_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE3_EXECUTION_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE4_EXECUTION_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE4_EXECUTION_2026-04-24.md)
