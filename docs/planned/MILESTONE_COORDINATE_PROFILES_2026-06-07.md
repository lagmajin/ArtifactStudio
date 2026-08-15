# MILESTONE: Coordinate Profiles

日付: 2026-06-07

**最終更新:** 2026-08-15
**Status:** CoordinateProfile／Resolver と基本変換は Core 実装済み、Artifact UI／project persistence／Unity・Tile 編集統合は未確認

座標系や単位を用途ごとに切り替え、Pixel / Percent / Normalized 0-1 / Unity Units / Tile/Grid Units を同じ編集体験の中で扱えるようにする。

## 2026-07-25 実装監査

Core に `CoordinateUnit`／`CoordinateValue`／`CoordinateSpace`／`CoordinateProfile`／`CoordinateResolver` と JSON 変換、px／%／normalized／grid／safe-area の基本変換、式解釈の実装を確認した。ただし Artifact 側の既存編集 UI・property・guide・responsive variant がこの resolver を利用する経路や、profile のプロジェクト保存、Y 軸を含む式の共通評価は確認できない。Unity Units／Tile Grid の実編集統合も未確認であり、Phase 1〜2 は Core 基盤、Phase 3 は未実装とする。

## Goal

`x = 0.5w`、`y = safe.top + 32px`、`tileX = 3` のような表現を、プロジェクトごとの座標プロファイルに基づいて自然に解釈できるようにする。

## Non-Goals

- 既存の全レイヤーを一度に新座標系へ移行しない
- Diligent / D3D12 backend や低レベル render path には触らない
- 新規の global signal-slot 経路を増やさない
- `QImage` / QtCSS / `QColorDialog` の採用を増やさない

## Core Concept

- `CoordinateProfile`
  - id
  - name
  - base unit
  - display unit
  - conversion rules
- `CoordinateValue`
  - numeric value
  - unit tag
  - expression string
- `CoordinateResolver`
  - 実際の変換と解釈を行う
- `CoordinateSpace`
  - canvas / safe area / tile grid / unity-like world などの文脈

## Why It Matters

- ピクセルと相対座標を同じ UI で扱いやすくなる
- safe area や grid に沿った配置がしやすくなる
- Unity Units や Tile/Grid Units 風のレイアウトに寄せやすくなる
- 内容物の bounds と組み合わせると、整列や自動配置が強くなる
- 表示単位と内部単位を分けられる

## Phase 1: Profile Definition

目的: 座標の解釈ルールをプロジェクト単位で保持する。

- base unit を定義する
- display unit を切り替えられるようにする
- profile ごとに変換ルールを持てるようにする
- まずは `px`、`%`、`normalized`、`grid` を扱う

確認観点:

- プロジェクトごとに単位の見せ方を変えられる
- 既存プロジェクトが壊れない
- 単位タグ付きの値を保存できる

## Phase 2: Resolver

目的: 値の解釈を共通化する。

- `0.5w`
- `safe.top + 32px`
- `tileX = 3`
- `tileY = 5`

確認観点:

- 同じ式が同じ文脈で同じ結果になる
- canvas サイズ変更で再計算できる
- safe area や grid を参照しても破綻しない

## Phase 3: UI Integration

目的: 編集 UI で単位を自然に扱えるようにする。

- 数値欄で単位を選べる
- 表示単位と入力単位を分けられる
- profile 切り替え時に既存値の解釈を維持する
- bounds と連携して視覚的に分かりやすくする

確認観点:

- ピクセルと相対値を混ぜても扱える
- safe area を前提にした位置指定ができる
- grid / tile 系の編集にも使える

## Integration Notes

- `Content Bounds System` と組み合わせると、見た目基準の配置がやりやすい
- `Sandbox Edits` と組み合わせると、座標変化の比較がしやすい
- まずは resolver と profile を作り、UI は後から追従させる

## 2026-08-15 現行実装監査

- Core の `CoordinateUnit`、`CoordinateValue`、`CoordinateSpace`、`CoordinateProfile`、`CoordinateResolver` と JSON 変換、px／%／normalized／grid／safe-area の基本式評価を確認した。
- 既存のレイヤー編集 UI、property、guide、responsive variant がこの resolver を共通利用する経路は確認できない。
- profile のプロジェクト保存、canvas resize 時の再評価、Y 軸を含む式の共通契約、Unity Units／Tile Grid の実編集導線は未確認。
- したがって Phase 1〜2 は Core 基盤、Phase 3 の UI／project 統合は未完了と判定する。
