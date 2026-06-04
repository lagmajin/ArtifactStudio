# 各ウィジェット 不足機能分析 — 2026-06-03

**分析手法**: 実ソースコード検証。各ウィジェットのファイルサイズ、TODO/FIXME/stub コメント、実装の深さを確認。

---

## 調査対象ウィジェット一覧

| ウィジェット | ファイル | 行数 | 状態 |
|-------------|---------|------|------|
| ArtifactCompositionEditor | 4580行 | ⭕ | 機能完備。Motion Path / Effect Hitbox / Pen Tool 実装済み |
| ArtifactCompositionRenderController | 8476行 | ⭕ | 機能完備。LOD / GPU/CPU両パス / debounce完備 |
| ArtifactTimelineWidget | 5413行 | ⭕ | 全体動作。EasingLab / キーフレーム編集完備 |
| ArtifactPropertyWidget | 2158行 | ⭕ | TODOなし。プロパティ表示・編集完備 |
| ArtifactPropertyEditor | 2657行 | ⭕ | ObjectReferencePropertyEditor 実装済み |
| ArtifactInspectorWidget | 2551行 | ⭕ | エフェクトラック管理完備 |
| ArtifactAssetBrowser | 2940行 | ⭕ | ファイルブラウズ・インポート完備 |
| ArtifactProjectManagerWidget | 4272行 | ⭕ | プロジェクト管理完備 |
| ArtifactMainWindow | 略 | ⭕ | Lazy Dock システム完備 |
| ArtifactToolBar / ToolOptionsBar | 略 | ⭕ | ツール切替完備 |
| AudioMixerWidget | 略 | ⭕ | オーディオミキサー完備 |
| SpectrumAnalyzerWidget | 略 | ⭕ | スペクトラム表示完備 |
| ArtifactProblemViewWidget | 715行 | ⭕ | 問題表示完備 |
| ArtifactSecondaryPreviewWindow | 410行 | ⭕ | セカンダリプレビュー完備 |
| ArtifactUndoHistoryWidget | 187行 | ⭕ | Undo/Redo表示完備 |
| ArtifactColorPaletteWidget | 314行 | ⭕ | カラーパレット完備 |
| EasingLabWidget | 390行 | ⭕ | イージングラボ完備 |
| Timeline系（LayerPanel/ScrubBar/Navigator） | 各200-1000行 | ⭕ | 全ファイルTODOなし |
| ArtifactCurveEditorWidget | 818行 | ⭕ | Value Graph描画完備、Speed Graph sampling 追加 |

## 🔴 空のシェル（機能がほぼないもの）

### 1. ArtifactQuickToolBox — 基本タブを実装
- **ファイル**: `Artifact/src/Widgets/ArtifactQuickToolBox.cppm` (55行)
- **状態**: `AnchorPointTool` と `AlignmentWidget` を直接タブ化し、`Transform` / `Calc` も案内パネルとして実体化
- **残課題**:
  - Transform / Calculator の専用機能はまだ薄い
  - 必要なら後続で専用 widget に差し替える

### 2. ArtifactSnapshotCompareWidget — UIは完成、RevisionService 連携を実装
- **ファイル**: `Artifact/src/Widgets/ArtifactSnapshotCompareWidget.cppm` (215行)
- **状態**: 
  - UI（ComboBox / ボタン / ListWidget / Splitter）は完全に組んである
  - `loadSnapshots()` は `ArtifactRevisionService::revisions()` を読み、selector/list を実データで埋める
  - `onCompare()` / `onDiff()` は `diffRevisions()` の `changes` を左右リストへ展開する
  - `onRestoreA()` / `onRestoreB()` は `restoreRevision()` へ接続済み
  - `onBranch()` は source snapshot を復元してから `commitCurrentProject()` で branch snapshot を作る
  - ボタンは `SnapshotActionButton` 経由で既存の実行経路に閉じているため、新しい signal/slot 配線は追加していない

## 🟡 部分的に未実装

### 3. ArtifactPropertyEditor — ObjectReference はピッカー連携済み
- **ファイル**: `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm` (2657行)
- **ObjectReferencePropertyEditor**:
  - `ArtifactObjectPickerDialog` を開き、選択結果を `commitValue()` に流す
  - 値表示は `None` / 数値 ID / レイヤー名付き表示のいずれかで更新される
  - クリアボタンは現在値を `-1` に戻す

### 4. ArtifactCurveEditorWidget — Speed Graph sampling を実装
- **ファイル**: `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm` (818行)
- Value Graph（値グラフ）の描画・編集・ハンドル操作は完備
- Speed Graph（速度グラフ）のサンプル関数 `sampleSpeedGraph()` を追加し、`setSpeedGraph(...)` で `CurveTrack` へ変換できる
- Menu の toggle Value/Speed は既存の経路のままなので、後続で実際の表示切替へつなげられる

### 5. Composition Editor — 選択レイヤー debug save を実装
- `ArtifactCompositionEditor.cppm`
  - F12 debug save は現在の選択レイヤーを優先して保存する
  - 選択レイヤーが画像化できない場合は composition screenshot にフォールバックする
  - 既存の Advanced Screenshot ダイアログはそのまま維持される

### 6. ApplicationSettingDialog — Importページは実装済み
- `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`
  - `ImportSettingPage::loadSettings()` / `saveSettings()` は実装済み

## ✅ 実装完了しているウィジェット（TODO/FIXMEなし）

| ウィジェット | 確認内容 |
|-------------|---------|
| ArtifactTimelineWidget (5413行) | レイヤー操作・キーフレーム・work area・scrub bar すべて完備 |
| Timeline系全ファイル | 14ファイル中、TODO/FIXMEゼロ |
| ArtifactLayerPanelWidget | レイヤーリスト表示・選択・ドラッグ完備 |
| ArtifactTimelineTrackPainterView | トラック描画・audio waveform完備 |
| ArtifactProblemViewWidget (715行) | 問題一覧・フィルタ・ソート完備 |
| AudioMixerWidget | オーディオミキシング完備 |
| SpectrumAnalyzerWidget | スペクトラム表示完備 |
| ArtifactColorPaletteWidget (314行) | カラーパレット管理・編集完備 |
| EasingLabWidget (390行) | イージングプリセット一覧・適用完備 |
| ArtifactUndoHistoryWidget (187行) | Undo/Redo履歴表示・操作完備 |

---

## 総合ギャップマップ

```
┌───────────────────────────────────────────────────────────────────┐
│                     UI ウィジェット 不足機能マップ                │
├───────────────────────────────────────────────────────────────────┤
│                                                                   │
│  P0（使えないウィジェットが存在する）                              │
│  ├─ ArtifactQuickToolBox: 基本タブを実装済み                       │
│  └─ ArtifactSnapshotCompareWidget: RevisionService 連携済み       │
│                                                                   │
│  P1（部分的に欠けている）                                          │
│  ├─ PropertyEditor: ObjectReference はピッカー連携済み          │
│  ├─ CurveEditor: Speed Graph sampling 追加                         │
│  ├─ Composition Editor: 選択レイヤー debug save 実装済み        │
│  └─ AppSettings: Import設定ページがスタブ                          │
│                                                                   │
│  P2（ないわけではないがUXに影響）                                  │
│  ├─ SecondaryPreviewWindow: 機能はあるがRAM preview連携のみ        │
│  │   （独立したフルスクリーン比較・モニタリング機能がない）        │
│  └─ Asset Browser: 機能はあるが左ペインのhierarchyが弱い          │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

## 補足：行数から見る実装バランス

```
Widget              行数      TODO数    状態
─────────────────────────────────────────────
RenderController     8476      0         ⭕ 最も巨大。CPU/GPU両パス完備
TimelineWidget       5413      0         ⭕ 
ProjectManager       4272      0         ⭕ 
CompositionEditor    4580      1(TODO)   ⭕ 
AssetBrowser         2940      0         ⭕ 
PropertyEditor       2657      0         ⭕ ObjectReference実装済み
InspectorWidget      2551      0         ⭕ 
PropertyWidget       2158      0         ⭕ 
ProblemView           715      0         ⭕ 
SecondaryPreview      410      0         ⭕ 
EasingLab             390      0         ⭕ 
ColorPalette          314      0         ⭕ 
SnapshotCompare       215      0         ⭕ RevisionService 連携済み
UndoHistory           187      0         ⭕ 
AlignmentWidget       146      0         ⭕ 
QuickToolBox           55      0         ⭕ 基本タブ実装済み
```

---

*分析日: 2026-06-03*
*調査対象: Artifact/src/Widgets/ 以下の全cppmファイル + Timeline/ サブディレクトリ*
