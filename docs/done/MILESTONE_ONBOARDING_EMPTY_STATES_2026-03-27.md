# Onboarding / Empty States (2026-03-27)

**Status:** Complete (static implementation verified; runtime and localization/accessibility verification pending).

## Goal

初回起動や空プロジェクト時に、何をすればよいかが分かる状態を作る。

## Scope

- empty project / empty selection / empty asset / empty timeline の案内
- first action の導線
- hint / tip / shortcut の最小表示
- recent project / import / create composition の入口

## DoD

- 空状態が単なる blank screen にならない
- 初回ユーザーが次の操作を見つけやすい
- 既存ユーザーの邪魔をしない

## Notes

`Deferred UI Initialization` と相性がよい。
lazy load した後でも、空状態の案内が自然に出るようにする。

## Implementation verification (2026-07-30)

- WelcomeWidget provides the empty-project first-action surface with recent projects and New / Import / Open entry points.
- Asset Browser distinguishes no folder, empty folder, and no search/filter matches.
- Layer Panel distinguishes no composition from a composition with no layers.
- Render Layer Widget and surface information use an explicit `No layer selected` state.
- Remaining verification is limited to runtime presentation, localization, and accessibility review.
