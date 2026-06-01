# MILESTONE: Text Animator Next Gen - Parity Execution Slice

作成日: 2026-05-31
対象: `M-TXT-1 Text Animator Next Gen`
参照:
- [MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md)
- [MILESTONE_TEXT_ANIMATOR_NEXT_GEN_EXECUTION_2026-04-30.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_TEXT_ANIMATOR_NEXT_GEN_EXECUTION_2026-04-30.md)
- [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md)

## Purpose

既存の `Text Animator Next Gen` を、現行 repo の AE parity 優先度に合わせて再開しやすい execution slice に絞る。

ここでは新規表現の追加より、`UI / timeline / preset` のつながりを先に完成させる。

## Why This Is Ready Now

- `TextAnimatorEngine` 自体の core はかなり揃っている
- parity gap でも不足は `engine missing` ではなく `timeline 統合 / UX coherence`
- 既存 milestone でも execution order が `Animator UI -> Timeline Exposure -> Preset Browser` で整理済み

## Recommended Start Order

### 1. Animator UI Finish

- animator 一覧の見通しを改善する
- add / remove / rename の導線を揃える
- selector と effector の関係を inspector 上で追いやすくする

狙い:
- 何が動いているかを timeline 前に理解できる状態にする

### 2. Timeline Exposure

- 少なくとも 1 つの animator property を timeline に確実に出す
- keyframe の追加 / 編集 / 削除が最小経路で成立するようにする
- 再生中に animator の時間変化が追える状態にする

狙い:
- AE parity で重要な「触れる text animator」ではなく「時間変化する text animator」にする

### 3. Preset Browser

- built-in preset を最小単位で保存 / 復元できるようにする
- inspector から preset を呼べるようにする

狙い:
- 実務で毎回ゼロから組まなくて済む状態にする

## In Scope

- inspector 導線
- timeline 公開
- preset 保存 / 復元

## Out of Scope

- Text on Path
- per-character 3D
- GPU text rendering backend work
- 大規模な selection model の再設計

## Success Criteria

- inspector だけで animator 構造が読める
- 少なくとも 1 つの animator property が timeline に出る
- preset を作って再利用できる
- `TextAnimatorEngine` を壊さずに実務導線が前進する

