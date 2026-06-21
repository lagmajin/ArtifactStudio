# MILESTONE: Timeline Selection Sets

日付: 2026-06-22

After Effects の `Control Groups` に近い、タイムライン操作の時短導線を追加する。

## Goal

選択中のレイヤー群やタイムライン上の位置を保存・復帰できるようにし、毎回探し直さなくてもよい状態を作る。

## Non-Goals

- 新しい global signal-slot 経路を増やさない
- 選択状態の完全な履歴管理に広げない
- Diligent / D3D12 の低レベル render path には触らない
- 既存の bookmark / navigator を全面置換しない

## Core Concept

- `Selection Set`
  - 現在のレイヤー選択を保存して再選択する
- `Focus Set`
  - 現在の作業対象や現在位置に戻る
- `Jump Set`
  - よく使うタイムライン位置へ即ジャンプする

## Phase 1: Selection Set

目的: 選択中レイヤー群を composition 単位で保存・復帰する。

- 現在選択を保存する
- 名前付きで呼び出せる
- composition ごとに分離する
- 既存 selection manager を再利用する

## Phase 2: Focus / Jump Set

目的: 選択だけでなく、時間位置も一緒に戻せるようにする。

- current frame を保存する
- work area と組み合わせて戻せる
- bookmark 系 UI と競合しない導線にする

## Phase 3: Workflow Polish

目的: 反復操作の往復を短くする。

- 既存セットに追加 / 差し替え
- ショートカット割り当て
- command palette から呼び出し

