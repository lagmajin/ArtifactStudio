# Onboarding / Empty States (2026-03-27)

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
