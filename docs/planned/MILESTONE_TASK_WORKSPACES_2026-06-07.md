# MILESTONE: Task Workspaces

日付: 2026-06-07

Status: Design Note

作業内容ごとに UI を切り替え、今やるべきことだけを見せるコンテキスト中心のワークスペース構想。

## Goal

`Import Workspace`、`Layout Workspace`、`Animation Workspace`、`Text/Caption Workspace`、`Export Workspace`、`Debug Workspace` のような目的別 surface を切り替えて、編集面を最適化する。

## Current Implementation

- 実装済みの中心は `WorkspaceMode` の切替
- 現在の選択肢は `Default / Import / Layout / Animation / VFX / Compositing / Text / Export / Debug / Audio`
- レイアウトの保存/復元は `ArtifactWorkspaceManager` が担当する
- これらはまだ「作業内容で UI 全体を再構成する」段階ではなく、主にモード切替とレイアウト永続化の段階

## Non-Goals

- 既存 UI を完全に廃止しない
- すべての機能を各 workspace に重複実装しない
- Diligent / D3D12 backend を広く変更しない
- 新規の global signal-slot 経路を増やさない

## Core Concept

- workspace は単なるタブではなく、目的ごとの表示・操作の集約である
- どの workspace でも同じデータを扱うが、見せ方と主導線を変える
- 迷いを減らし、今の作業に必要な UI だけを前面に出す

## Typical Workspaces

- `Import Workspace`
- `Layout Workspace`
- `Animation Workspace`
- `Text/Caption Workspace`
- `Export Workspace`
- `Debug Workspace`

## Why It Matters

- 初心者にも何を触ればいいか分かりやすい
- ベテランも作業に必要な UI だけを集中的に使える
- 機能が増えても画面が散らかりにくい
- `Task-Based Workspaces` は `Multi-Format Preview` や `Export Matrix` のような機能を目的別に束ねやすい

## Phase 1: Workspace Definition

目的: 各 workspace の役割を定義する。

- workspace id
- display name
- primary tools
- visible panels
- default shortcuts

確認観点:

- workspace ごとの責務が分かる
- 共通機能は再利用できる
- 既存 UI を壊さない

## Phase 2: Context Switching

目的: 作業文脈に応じて UI を切り替える。

- import 中は import 導線を前面に出す
- layout 中は bounds / guides / collision を前面に出す
- animation 中は motion / preview を前面に出す
- export 中は matrix / preset / queue を前面に出す
- debug 中は warnings / diagnostics / fallback を前面に出す

確認観点:

- 切り替え後に迷いにくい
- 文脈に合わない UI が減る
- 目的ごとに必要な情報が見える

## Phase 3: Workspace Persistence

目的: 前回の作業文脈を復元できるようにする。

- last used workspace
- per-workspace layout
- panel visibility
- recent context

確認観点:

- 作業の続きから入りやすい
- workspace 切り替えが負担にならない
- 既存プロジェクトの状態と分離できる

## Integration Notes

- `Content Bounds System` は Layout Workspace と相性が良い
- `Motion Tokens` は Animation Workspace の中核になる
- `Export Matrix` は Export Workspace に自然に載る
- `Smart Fallbacks` や `Loop Seam Checker` は Debug Workspace に寄せやすい

## Source Alignment Notes

- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Widgets/ArtifactToolBar.cppm`
- `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`
- `Artifact/src/Core/ArtifactWorkspaceManager.cppm`

これらの実装に合わせるなら、今は「Task Workspace をどう作るか」より先に、
「既存の `WorkspaceMode` と `WorkspaceManager` をどう整理して拡張するか」が正しい出発点。
