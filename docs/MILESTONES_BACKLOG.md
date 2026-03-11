# Milestones Backlog

空いている時間に進めやすいよう、分野別に小さめのマイルストーンへ分割したバックログ。

## UI / UX

### M-UI-1 Timeline Finish
- playhead、不感帯、余白、行揃え、ホイール、ドラッグ挙動の最終整理

### M-UI-2 Dock / Tab Polish
- アクティブタブ装飾
- スプリッター幅
- 空パネルや初期レイアウトの見直し

### M-UI-3 Inspector Usability
- effect / property の見つけやすさ
- 空状態の整理
- 選択同期とラベル整理

## Timeline / Layer

### M-TL-1 Layer Basic Operations
- 追加、削除、複製、rename、親子、並び替え

### M-TL-2 Layer View Sync
- 左ツリー展開と右トラック行の同期
- 1レイヤー1クリップの維持

### M-TL-3 Work Area / Range Unification
- in / out
- work area
- seek
- render 範囲の一本化

## Render

### M-IR-1 ArtifactIRender API Cleanup
- viewport / canvas / pan / zoom の整理
- primitive API の責務固定

### M-IR-2 ArtifactIRender Software Backend
- Qt painter fallback の強化
- overlay / gizmo 用 2D 描画

### M-IR-3 ArtifactIRender Backend Parity
- software と Diligent の primitive 差分を縮める

### M-RD-1 Software Render Pipeline
- コンポ作成
- Solid 追加
- preview
- effect
- 静止画シーケンス

### M-RD-2 Render Queue Hardening
- job 編集
- 範囲指定
- 失敗理由表示
- 履歴と再実行

### M-RD-3 Dual Backend Parity
- software と Diligent の見た目差分を減らす

## Effects

### M-FX-1 Inspector Effect Stack Bridge
- Inspector から effect 追加、削除、順序変更

### M-FX-2 Solid Color Effects
- Color Wheels
- Curves
- Grader を Solid に通す

### M-FX-3 Creative Effects Bridge
- Halftone
- Posterize
- Pixelate
- Mirror などを接続

## Audio

### M-AU-1 Composition Audio Mixer
- mute / solo / volume / layer 同期

### M-AU-2 Playback Sync
- 再生位置と音の同期

### M-AU-3 Audio Visualization
- waveform
- meter
- 簡易可視化

## Project / Asset

### M-PV-1 Project View Basic Operations
- Project View selection と current composition の同期
- rename / delete / double-click
- 基本検索と filter

### M-PV-2 Project View Asset Presentation
- thumbnail
- type icon
- size / duration / fps / missing 状態

### M-PV-3 Project View Organization
- folder / bin 整理
- expand / collapse
- unused / tag / virtual view

### M-AS-1 Asset Import Flow
- 読み込み
- 再リンク
- メタ表示
- 未使用管理

### M-AS-2 Composition / Project Organization
- project tree
- 検索
- 並び
- タグ

### M-AS-3 Save / Load Integrity
- 保存再読込で composition / layer / effect が落ちない

### M-AS-4 Asset System Integration
- `AssetBrowser` と `Project View` の同期
- import / metadata / relink / missing / unused の統合
- 詳細は `Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md`

## Core / Architecture

### M-AR-1 Service Boundary Cleanup
- UI 直参照を減らして service 経由へ統一

### M-AR-2 import std Rollout
- 安全な module から順に C++23 / `import std;` 化

### M-AR-3 Serialization Cleanup
- layer / composition / effect の JSON 保存整理

## Test / Validation

### M-QA-1 Software Test Windows
- current composition / current layer 追従の検証窓を強化

### M-QA-2 Manual Regression Checklist
- タイムライン、render、audio、dock の確認表

### M-QA-3 Crash / Diagnostics
- recovery
- ログ
- 診断導線

## Good Small Tasks

- `M-AR-2 import std Rollout`
- `M-UI-2 Dock / Tab Polish`
- `M-QA-1 Software Test Windows`
- `M-FX-2 Solid Color Effects`
- `M-AS-3 Save / Load Integrity`
