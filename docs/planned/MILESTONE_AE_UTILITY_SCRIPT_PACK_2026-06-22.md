# Milestone: AE Utility Script Pack

**最終更新:** 2026-08-15

> 状態: Partial (three utility scripts, C++ bridges, and Script menu entries implemented; Undo, confirmation, input UI, and shared registry remain incomplete or unverified)

## Static Audit (2026-07-25)

AE Utility Pack の3本（`quick_rename_layers.py`、`clean_layers.py`、`trim_comp_to_content.py`）と、対応する `ArtifactPythonAPI` の C++ bridge、Script menu の `AE Utility Pack` サブメニューを確認した。各ファイルは menu から直接実行でき、既存の selection／composition service 経路を利用する。

- 実装済み: Quick Rename の prefix/base/suffix/連番、Clean Layers の parent/effects/markers/expressions/labels、Trim の selected/all/visible、padding、work area同期、locked layer考慮。Command Palette 側にも一部日常操作が存在する。
- 未確認: 小 dialog／Command Palette からの入力編集、破壊的操作の統一 confirm、各操作の Undo 経路、失敗理由の構造化表示、入力スキーマのUI検証、menu と command palette の共通 registry。
- `ArtifactPythonAPI::registerProjectAPI` には placeholder の project_new/open/save/info も残っているため、AE Utility Pack の3本が動作することを Python API 全体の完成根拠にはできない。

判定: 提案された3 utility の bridge／script／menu entry は部分的に実装済み。再利用可能な制作補助としての最小導線はあるが、Undo／confirm／UI入力／registry統合を含む完成条件は未達または未検証。

## 2026-07-29 実装ループ: 破壊的 utility の確認導線

## 2026-08-15 現行コード照合

- 3本の script／C++ bridge／Script menu entry と、Clean／Trim の確認ダイアログを確認した。
- 現行の残課題は Undo の統一、入力編集 UI、失敗理由の構造化、Script menu と Command Palette の共通 registry、runtime 検証。
- `ArtifactPythonAPI` 全体には placeholder API も残るため、この3本の実装を Python surface 全体の完成とは扱わない。

**判定:** 最小導線は実装済みだが、再利用可能な制作補助パックとしては `Partial` を維持する。

- ✅ Script メニューから `Clean Layers` / `Trim Comp to Content` を実行する前に確認ダイアログを表示するようにした。
- ✅ `Quick Rename Layers` は非破壊のため確認なしで既存経路を実行する。
- ⏳ Undo、入力編集 UI、共通 registry、runtime 検証は未完了。マイルストーン全体は `Partial` のままとする。

> 2026-06-22 draft

## 目的

After Effects 由来の定型作業を、Artifact 側で少ない操作にまとめる。
この milestone は、AE の人気スクリプトである `Quick Rename Layers`、`Clean Layers`、`Trim Comp to Content` を
そのまま模倣するのではなく、Artifact の責務境界に合わせて小さな制作補助機能として再構成する。

## 対象候補

### 1. Quick Rename Layers

- 選択レイヤーを一括で命名する
- prefix / base name / suffix / 連番の組み合わせを扱う
- 名前の整理と検索性の向上を目的にする

### 2. Clean Layers

- 選択レイヤーを検証用の初期状態に戻す
- effects / expressions / markers などを選択的に消す
- 破壊的操作になりすぎないよう、カテゴリ単位で切り替えられるようにする

### 3. Trim Comp to Content

- comp の尺を、実際のレイヤー内容に合わせて詰める
- 対象範囲、余白、基準レイヤーを切り替えられるようにする
- work area の同期を同時に扱うかは実装時に決める

## 期待する価値

- 命名の統一がしやすくなる
- テスト前や再実行前の掃除がしやすくなる
- 不要な尺を削ってレビューしやすくなる
- いずれも AE 風の「日常の小技」を支える導線として効く

## Menu Entry

- 入口は `Script` メニュー配下の `AE Utility Pack`
- 各項目は `macros/ae_utility_pack/` 内の個別スクリプトとして置く
- メニューからは直接実行できるようにし、編集したいときはファイルをそのまま開けるようにする

## Input Schema

### Quick Rename Layers

- `baseName`
- `prefix`
- `suffix`
- `startIndex`
- `padding`
- `renameSelectedOnly`
- `preserveLockedLayers`

### Clean Layers

- `clearParent`
- `clearEffects`
- `clearMarkers`
- `clearExpressions`
- `clearLabels`
- `preserveLockedLayers`

### Trim Comp to Content

- `trimMode`
  - `selectedLayers`
  - `allLayers`
  - `visibleLayers`
- `paddingFrames`
- `syncWorkArea`
- `respectLockedLayers`

### Quick Rename Layers

- `prefix`
- `baseName`
- `suffix`
- `startIndex`
- `padding`
- `renameSelectedOnly`

## Artifact 側の置き場所

- `Quick Rename Layers`
  - project / layer 操作に近い command として扱う
- `Clean Layers`
  - development / diagnostic helper に寄せる
- `Trim Comp to Content`
  - composition utility として扱う

## 実装メモ

- まずは scriptable surface の完成を待たず、既存 command 入口に載せる
- UI は小さな dialog か command palette 起点で十分
- 破壊的操作は確認フローを付ける
- 新しい global signal / slot は増やさない

## 依存候補

- `docs/planned/MILESTONE_UI_SCRIPTABILITY_AND_ADAPTIVE_SURFACE_2026-06-07.md`
- `docs/planned/MILESTONE_SHORTCUT_IMPROVEMENTS_2026-06-02.md`
- `docs/planned/MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md`

## Next Step

この milestone を進めるなら、次は各コマンドの

- 入力スキーマ
- 対象選択ルール
- Undo / confirm policy
- UI 入口

を 1 枚ずつ固める。

## Progress

- `Quick Rename Layers` は `artifact.rename_selected_layers(...)` の C++ bridge と
  `Artifact/scripts/macros/ae_utility_pack/quick_rename_layers.py` で先行実装済み
- `Clean Layers` は `artifact.clean_selected_layers(...)` の C++ bridge と
  `Artifact/scripts/macros/ae_utility_pack/clean_layers.py` で先行実装済み
- `Clean Layers` の現行スコープは `parent` / `effects` / `markers` /
  `expressions` / `labels` の除去
- `Trim Comp to Content` は `artifact.trim_comp_to_content(...)` の C++ bridge と
  `Artifact/scripts/macros/ae_utility_pack/trim_comp_to_content.py` で先行実装済み
- 現行の `Trim Comp to Content` は `trimMode` / `paddingFrames` / `syncWorkArea` /
  `respectLockedLayers` を受け、selected / all / visible を切り替えつつ
  `frameRange` / `workAreaRange` を更新する
- 2026-07-10: Composition Editor の Command Palette に日常向け Batch 操作を追加
  - Rename Selected Layers
  - Duplicate Selected Layers
  - Sequence Layers End-to-End
  - Match Duration to First Layer
  - Trim Composition to Selection
