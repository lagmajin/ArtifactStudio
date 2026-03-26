# Project File Validation / Spell Check Milestone

`ArtifactProject` が持つ project data に対して、名前の typo や表記ゆれ、禁止語、参照切れを検出するための validation milestone.

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
