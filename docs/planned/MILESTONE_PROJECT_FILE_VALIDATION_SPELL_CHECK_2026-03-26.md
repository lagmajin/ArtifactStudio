# Project File Validation / Spell Check Milestone

`ArtifactProject` が持つ project data に対して、名前の typo や表記ゆれ、禁止語、参照切れを検出するための validation milestone.

**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

`ArtifactProjectHealthChecker` は naming／spelling／missing asset／broken reference／circular reference／frame range を検査し、組み込み typo 辞書による spelling suggestion とカテゴリ別 repair action を生成する。`ArtifactProjectHealthDashboard`、Problem View、Project Importer、Project Manager から health report／repair 経路へ接続されているため、文書の「命名と文字列品質を追加する」という Phase 1〜2 の基盤は実装済みである。

一方、custom dictionary／ignore list、禁止語ポリシーの編集 UI、tags／notes／AI metadata の網羅、suggestion の一括適用と undo、runtime の誤検出率検証は確認できない。現状は project health validation と組み込み spelling suggestion が実装済み、ユーザー定義辞書・高度な品質ポリシー・運用検証が pending と判定する。

## Update 2026-08-15

- 現行コードでは `ArtifactProjectHealthChecker` の naming／spelling／missing asset／broken reference／circular reference／frame range 検査、組み込み修正候補、repair、Project Health Dashboard／Problem View／save・import validation 連携を確認できる。
- custom dictionary／ignore list、禁止語 policy UI、tags／notes／AI metadata の網羅、一括 suggestion 適用＋Undo、誤検出率の runtime 検証は未完了または未確認。

## Goal

- project / composition / layer / asset 名の typo を見つけやすくする
- tags / notes / ai metadata の表記ゆれを検出する
- custom dictionary と ignore list を使って誤検出を減らす
- `ArtifactProjectHealthChecker` と `ArtifactProjectHealthDashboard` に統合する

## Scope

- `Artifact/src/Project/ArtifactProjectHealthChecker.cppm`
- `Artifact/src/Widgets/ArtifactProjectHealthDashboard.cppm`
- `Artifact/src/Project/ArtifactProject.cppm`
- `Artifact/src/Project/ArtifactProjectManager.cppm`
- `Artifact/src/Project/ArtifactProjectImporter.cppm`

## Non-Goals

- 自由文の英作文を添削する一般的な文章校正
- コードコメントや shader 文字列の spell check
- 完全自動修正だけで品質を担保すること

## Background

Project file は code とは違って、ユーザーが入力する name / label / tag / note が多い。
ここでの "spell checker" は、英語辞書だけを当てる単純な機能ではなく、
project data の品質を保つための validation に寄せる。

既に `ArtifactProjectHealthChecker` は cycle / duplicate ID / broken reference / missing asset を見ている。
この milestone ではそこに、命名と文字列品質のチェックを追加し、
health dashboard から一目で追えるようにする。

## Phases

### Phase 1: Validation Categories

- project / composition / layer / asset の名前を対象にする
- tag / note / ai metadata の文字列を対象にする
- 既知の用語を許可する whitelist を設ける
- 参照切れや空名前を warning ではなく明確な issue として出す

### Phase 2: Dictionary and Ignore List

- project scope の custom dictionary を持つ
- ignore list を project file に保存できるようにする
- 固有名詞、技術用語、アセット名の誤検出を減らす
- locale に応じた基本辞書を切り替えられるようにする

### Phase 3: Dashboard Integration

- `ArtifactProjectHealthDashboard` で issue を category 別に見せる
- typo / naming / reference / metadata を severity で分ける
- scan result から該当項目へ移動できる導線を作る
- quick fix が可能な項目だけボタンで修正する

### Phase 4: Repair and Workflow

- rename suggestion を提示する
- 一括修正ではなく、個別修正を基本にする
- import / save / open 時に health check を走らせる
- project view から再スキャンを呼べるようにする

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4

## Current Status

- `ArtifactProjectHealthChecker` は既に存在する
- ただし spell / naming hygiene 専用の検査はまだない
- まずは project data に限定した validation として追加するのが最小で安全

## 2026-07-25 実装監査

既存の project health／validation は missing file、broken reference、duplicate／circular dependency、expression、performance 等を検出し、diagnostic engine／dashboard 基盤へ接続できる。一方、project／composition／layer／asset 名の typo・naming hygiene、tag／note／AI metadata、custom dictionary、ignore list、locale 辞書、rename suggestion、category別 dashboard／quick fix、import／save／open時の自動 scan は確認できない。したがって参照・整合性検証の基盤はあるが、Spell Check milestone の専用機能は未実装・未検証とする。
