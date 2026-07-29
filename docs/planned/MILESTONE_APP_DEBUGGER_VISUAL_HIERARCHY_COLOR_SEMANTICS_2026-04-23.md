# Milestone: App Debugger Visual Hierarchy / Color Semantics (2026-04-23)

**Status:** Planning
**Parent:** `MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17`
**Goal:** 内蔵デバッガを「情報は多いが読みにくい」状態から、色とレイアウトで優先度が伝わる画面へ寄せる。

---

## ねらい

今の App Debugger は将来性が高い一方で、情報の置き方が均等すぎると人間には読みづらい。
とくに `overlay` / `present` / `render path` / `resource` / `playback` のような要素が同じ密度で並ぶと、
「どこを見ればいいか」が分かりにくくなる。

このマイルストーンでは、情報量を減らすのではなく、
見る順番・重要度・異常の見え方を整える。

---

## 現状の課題

- どのタブ / セクションを先に見ればよいか分かりにくい
- 正常時と異常時の見え方の差が弱い
- `overlay` / `present` / `pass` / `resource` の意味が色だけでは区別しにくい
- 重要な警告が他の情報に埋もれやすい
- 詳細情報が最初から同じ強さで出ていて、視線誘導が弱い

---

## 改善方針

### Phase 1: 情報階層の固定

- 上部に `Current State` の短い要約を置く
- 中央に `いま見るべきカテゴリ` を 1 つだけ強く出す
- 下部に詳細ログや補助情報を寄せる
- 正常時は静か、異常時だけ前に出る構成にする

### Phase 2: 色セマンティクスの固定

- `normal` は neutral
- `info` は blue
- `warning` は amber
- `error` は red
- `success` は green

同じ意味は同じ色に寄せ、同じ色に複数の意味を混ぜない。

### Phase 3: セクションごとの視線誘導

- `Trace` は時系列の読解に集中させる
- `State` は現在値を大きく、補足を小さくする
- `Frame` は pass / resource / present を段階表示にする
- `Diagnostics` は要約を先に、全文は後に出す

### Phase 4: 異常系の前景化

- failed pass
- missing resource
- fallback path
- stale cache
- playback / render の不整合

これらは一覧の中で埋もれないように、カードや badge で前に出す。

---

## レイアウト方針

- 左側: navigation / filter / category
- 中央: 現在の主表示
- 右側: details / inspector / raw text
- 上部: current state summary
- 異常時: summary を警告色にして固定表示

補助ルール:

- 1 画面に同じ強度の枠を並べすぎない
- 1 セクション 1 役割を基本にする
- 詳細は折りたたみ可能にする

---

## 表示ルール

- `overlay` と `present` は同じ「描画後段」でも、別色と別ラベルで区別する
- `pass` と `resource` は「工程」と「対象」で色分けする
- `frame` 系は時間軸、`state` 系は状態軸として扱う
- `debug note` は補助テキストに落とし、主見出しを汚さない

---

## 実装メモ

- `AppDebuggerWidget` のタブ順と見出しを整理する
- `Frame Debug` と `State` のサマリを最上段へ寄せる
- `warning` / `error` の badge tone を固定する
- `overlay` / `present` / `fallback` の文言テンプレートを揃える
- 長い text dump は折りたたみから展開する

---

## 完了条件

- 画面を見た瞬間に、どこが要注意か分かる
- 正常時と異常時で見え方が変わる
- `overlay` / `present` / `pass` / `resource` の違いが読み取りやすい
- 情報量を増やしても、視線の迷子が減る
- App Debugger が「読むための画面」として成立する

---

## リスク

- 色を増やしすぎると逆に意味がぼやける
- レイアウトを分けすぎると、今度は情報が散る
- 階層を強くしすぎると、必要な詳細が見つけにくくなる
- 既存の diagnostics 表示との整合を崩す可能性がある

---

## 参照

- [`MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md)
- [`MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md)
- [`MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md)
- [`MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_FOCUS_PIN_FILTER_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_REPORT_SHARE_BUNDLE_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_REPORT_SHARE_BUNDLE_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_LEGEND_SEMANTIC_KEY_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_LEGEND_SEMANTIC_KEY_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_QUICK_ACTIONS_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_QUICK_ACTIONS_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_AUTO_FOCUS_SMART_RANKING_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_AUTO_FOCUS_SMART_RANKING_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_SESSION_HISTORY_COMPARISON_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_SESSION_HISTORY_COMPARISON_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_RENDER_COST_BREAKDOWN_2026-04-24.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_RENDER_COST_BREAKDOWN_2026-04-24.md)

---

## 次の一手

Phase 1-4 は summary / color / ordering / diagnostics bridge の基礎面として扱い、Phase 5-12 はその上に乗る派生 slice として進める。
個別の詳細は各 phase 実行メモに置き、ここでは視線誘導の原則だけを保つ。

---

## Static audit follow-up (2026-07-25)

現行の AppDebugger、FrameDebug、Harness の summary/palette/layout 実装を確認した。ビルド・実機 UI 評価は未実施。

| Phase | 現状 | 判定 |
|---|---|---|
| 1. Information Hierarchy | Frame Debug に goal/now/warning/next の top summary があり、Harness も summary/failure/report の段階表示を持つ。AppDebugger 全タブの主役統一は未確認。 | 部分実装 |
| 2. Color Semantics | QPalette ベースの neutral/warning/error 表示、failure palette、summary surface がある。normal/info/warning/error/success の共通 token と全 surface の同一意味付けは未確認。 | 部分実装 |
| 3. Section Guidance | Frame/Resource/Pipeline/Diff/Trace の専用面と summary/detail の分離がある。Trace/State/Frame/Diagnostics の最終視線誘導は未確認。 | 部分実装 |
| 4. Danger-First | failed pass、missing/failed resource、fallback、cache health、playback 状態を前景化する表示がある。全異常種別の優先順位統一は未確認。 | 部分実装／統合待ち |

### 現在の判定

情報階層と異常表示の実装は進んでいるが、全 App Debugger surface の共通 semantic token と runtime layout 評価が残る。マイルストーンは「部分実装／確認待ち」とする。

## 2026-07-29 実装マーク

- Phase 1 の current-state / frame summary / warning / next の階層、Phase 2 の QPalette による neutral / warning / error surface はコード上実装済みとしてマークする。
- Phase 4 の failed pass、resource failure、fallback、stale cache、playback warning の前景化も既存 summary / attention surface で確認済みとする。
- 全 surface の semantic token 統一、overlay / present / pass / resource の最終色設計、runtime layout 評価は pending のため、マイルストーン全体は `Partial implementation / verification pending` を維持する。
