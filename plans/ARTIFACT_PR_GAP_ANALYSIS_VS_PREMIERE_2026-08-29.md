# ArtifactPr × Premiere Pro ギャップ分析レポート

作成日: 2026-08-29
最終更新: 2026-08-29

---

## 概要

ArtifactPr は `ArtifactCoreNLE` / `ArtifactCoreVideo` / `ArtifactCoreCommand` を共有する
Premiere Pro 風の動画編集試作アプリ (`ArtifactPr/README.md` に "Pr-like editor prototype"
と明記)。NLE コアには機能があっても UI/配線が未着手の領域、Premiere の中核機能そのもの
がない領域が多数残っている。Premiere と並ぶ実用エディタに到達するには、まだ相当の差がある。

---

## 強み (Premiere に対し同等以上)

- **Serializable Undo/Redo**: `ArtifactPrEditorEngine.ixx` の `*StateCommand` が JSON
  で完全永続化 (`EditCommand.cppm`)。
- **オーディオ経路の共通化**: `ArtifactCoreAudio` 共有で `AudioPreviewMixer` /
  `SequenceAudioRenderer` を実装。
- **Premiere 風ショートカット体系**: `PrShortcut` / `ShortcutHelpDialog` 専用モジュール。
- **drop-frame 対応 TimeBase**: `ArtifactCore/include/NLE/Core.ixx` の `TimeBase` で
  `timecodeString` / `frameFromTimecode` が標準対応。
- **プレビュー/書き出し分離**: `RenderQualityPreset { Draft, Preview, Full }`。
- **共有 NLE ストアへの布石**: `ArtifactPrEditorEngine` は `import NLE.Core;` で
  コア側の ID 体系・サービス群を直接参照可能。

---

## Premiere 機能カバレッジ

| Premiere 機能 | ArtifactPr 現状 | 状態 |
|---|---|---|
| ネストシーケンス | `TrackKind::NestedSequence` は NLE コア定義済み、UI 配線なし | ❌ 未着手 |
| マルチカム編集 | なし | ❌ 未着手 |
| Adjustment Layer | `TrackKind::Adjustment` は定義済み、未使用 | ❌ 未着手 |
| 字幕 / キャプション | `TrackKind::Subtitle` は定義済み、未使用 | ❌ 未着手 |
| トラックマット (Alpha/Luma) | なし | ❌ 未着手 |
| トランジション 100+ 種 | **4 種のみ** (`Crossfade/DipToBlack/WipeLeft/WipeRight`) | ⚠️ 大幅不足。NLE 17 種 / Video 16 種実装済、`plans/transition-effects-expansion-2026-07-09.md` M2 で write-through ミラー計画段階 |
| Lumetri / カラーグレーディング | `PrColorPickerDialog` のみ | ⚠️ 大幅不足 |
| Essential Graphics / テキストタイトル | なし | ❌ 未着手 |
| オーディオエフェクト | EQ 3 帯 (low/mid/high) のみ | ⚠️ 大幅不足 |
| Trim: Ripple | 実装 | ✅ |
| Trim: Roll | UI での露出は限定的 | ⚠️ |
| Trim: Slip | UI 未着手 | ❌ |
| Trim: Slide | UI 未着手 (`WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §4 で計画) | ❌ |
| Clip Linking (V/A, Selection, Move) | `LinkingService` 定義済、未接続 | ⚠️ 計画のみ |
| Conform (メディア再リンク) | `ConformService` 定義済、未接続 | ⚠️ 計画のみ |
| プロキシ生成 | `MediaItem::proxyPath` フィールドのみ、生成 UI なし | ⚠️ |
| マルチカメラ同期 | なし | ❌ |
| Dynamic Link (AE / Ps) | なし | ❌ |
| チームプロジェクト | なし | ❌ |
| バックグラウンドレンダリング | `RenderQualityPreset` のみ、レンダーキュー独立実行なし | ⚠️ |
| プロ用コーデック (ProRes RAW / CineForm / BRAW) | `ArtifactCoreVideo` の FFmpeg 設定文字列依存 | ⚠️ |
| オーディオミキサー詳細 (5.1ch, バスセンド等) | 基本プレビューのみ | ⚠️ |
| プロジェクトパネル詳細 (ビン、プロキシ作成) | `MediaPanel` 基本のみ | ⚠️ |
| カラー / Vectorscope / Waveform スコープ | なし | ❌ |

凡例: ✅ 実装 / ⚠️ 部分実装 / ❌ 未実装

---

## NLE コアに既に存在し ArtifactPr で未活用なもの

`ArtifactCore/include/NLE/Core.ixx` で定義済みだが、`ArtifactPr/include/ArtifactPrEditorEngine.ixx`
の `DemoClip / DemoTrack / DemoSequence` には取り込まれていない:

- `LinkingService` (`createLinkGroup`, `addClipToLinkGroup`, `propagateTrimLink/MoveLink/SelectionLink`)
- `NLEEditHistory` (ArtifactPr 側は `QUndoStack` 直結)
- `ConformService` (`conformSequence`, `conformAll`)
- `TrimMode { Source, Ripple, Roll, Slip, Slide }`
- ID 体系 (`SequenceId / TrackId / ClipId / MarkerId / TransitionId / SourceId`) と
  `SourceRef` (`proxyAvailable`, `useProxy`, `online` を含む)
- `NLEProjectStore::createTransition`, `SequenceEditor::insertTransition/setTransitionKind`

→ `plans/transition-effects-expansion-2026-07-09.md` の M2 (`ArtifactPr/src/NLETransitionMirror.cppm`)
で write-through 経由で初めて NLE コアへ書き込まれる計画であり、現時点では
**ArtifactPr の編集操作と NLE ストアが分離されたまま**。

---

## 開発優先方針との整合

ルート `AGENTS.md` の **2026-07-27 開発優先方針** で動画対応は後回し、静止画・連番・
シェイプ・合成・3D の完成度向上が優先。ArtifactPr は「`Artifact` プロジェクトとは別方向に
育てる試作品」と位置付けられ、Premiere と実用比較する段階ではない。

ただし `ArtifactCoreNLE` の設計は Premiere の中核データモデル (Track/Clip/Source/Transition/
Marker/Link/Trim/Conform) を既に内包しているため、コアが揃っている分だけ将来性は高い。

---

## 📎 関連ファイル

- `ArtifactPr/CMakeLists.txt`
- `ArtifactPr/include/ArtifactPrEditorEngine.ixx`
- `ArtifactPr/include/ArtifactPrMainWindow.ixx`
- `ArtifactPr/README.md`
- `ArtifactCore/include/NLE/Core.ixx`
- `ArtifactCore/include/Video/TransitionFactory.ixx` (`ArtifactCore/include/Video/Transitions/*`)
- `plans/transition-effects-expansion-2026-07-09.md`
- `docs/analysis/WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md`
- `plans/AFTER_EFFECTS_GAP_ANALYSIS.md` (比較: AE 比較レポートのテンプレ参考)