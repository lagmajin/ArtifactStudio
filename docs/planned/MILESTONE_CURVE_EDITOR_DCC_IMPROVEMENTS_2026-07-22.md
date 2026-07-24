**ステータス:** In Progress

# M-CURVE-ED: カーブエディタ改善 — DCC いいとこどり計画

作成日: 2026-07-22
対象: `ArtifactCurveEditorWidget`（`Widget.CurveEditor`）+ `ArtifactTimelineWidget` のカーブエディタ統合（Tab 切替）
目的: Maya / Blender / AE のグラフエディタの良い部分を、既存の Blender ライクキー系（`InputOperator` / `KeyMap`）と矛盾しない形で取り込む。

## 現状サマリ（調査済み 2026-07-22）

### 動いているもの
- Tab / グローバルスイッチで timeline painter ⇔ curve editor page 切替（`timelineModeStack_`、設定永続化 `timelineGraphEditorActive`）
- Value / Speed グラフ切替（Speed は read-only）、nice-step グリッド、ベジェ描画、プレイヘッドスクラブ、カーソルアンカー付きズーム（Shift=X / Alt=Y / 無印=XY）
- キー移動（frame/value）とキー削除はプロパティ書き戻し + Undo 配線済み（`applyCurveEditorMove`、interactionStarted/Finished でスナップショット → `TimelineKeyframeSnapshotCommand`）
- トラックフォーカス（ダブルクリック）、Solo ピン、サマリラベル、プロパティパネル
- `CurveKey` はベジェハンドル + `brokenTangents` フラグ（Maya Break/Unify 相当の概念）を保持

### 重大なギャップ（正確性バグ）
- **タンジェント編集がプロパティに書き戻されない**。ハンドルドラッグと Auto/Flat/Linear ボタンはウィジェットローカルのみ。書き戻し関数 `applyCurveEditorTrackToProperty`（cp1/cp2 変換ロジック完備、`ArtifactTimelineWidget.cppm:3387`）は呼び出し元ゼロのデッドコード。表示カーブと評価カーブが乖離し、signature 変更で編集が消える。

## キー体系の整合ポリシー（Blender ライク系と矛盾しないために）

- 既存キー系: `InputOperator` / `KeyMap`（`ArtifactCore` `Input.Operator` モジュール、BUG_BLENDER_KEYMAP 修正済み）+ QShortcut（Tab、ツール V/H/Z/R/S）+ "Playback" KeyMap（Space/J/K/L/I/O/U）。
- カーブエディタの新規キーは **専用 KeyMap（"CurveEditor"）を InputOperator に登録する方式を正規とし**、タイムライン側 QShortcut と衝突しない割り当てにする。
- 割り当て案（Blender Graph Editor 準拠、衝突回避）:
  - `G` = grab（選択キー移動モード）、`B` = マーキー選択、`X` / `Delete` = キー削除
  - `Ctrl+Click` = キー挿入（Blender 方式）、`A` = 全選択/解除（現行は focusTrack(-1) → 選択全解除に変更）
  - `V` = tangent タイプメニュー（Auto/Flat/Linear/Step/Break/Unify）※ タイムラインのツール V（Selection）とはカーブエディタアクティブ時のみ有効としてコンテキスト分離
  - `F` = fit/フォーカス（現行維持）、`Home` = 全表示
  - `Ctrl+C` / `Ctrl+V` = キー コピー/ペースト（後段フェーズ）
- 既存の widget 内 `keyPressEvent` 直処理（Delete/F/A）も KeyMap へ段階移行する（本マイルストーンでは新規分のみ KeyMap 登録）。

## 修正・機能リスト

### P0: タンジェント書き戻し（正確性バグ）
- [x] **CE-1**: `interactionFinished` で、ウィジェットのトラックを `applyCurveEditorTrackToProperty` でプロパティへ書き戻してから after スナップショットを取る（Undo 粒度は現行どおり1インタラクション=1コマンド）。
  - 実装: `writeBackCurveEditorTangentEdits()`（`ArtifactTimelineWidget.cppm:3497`）を新設し、キャッシュと差分のあるトラックのみ書き戻す（キー移動は `applyCurveEditorMove` 済みのためスキップ）。`interactionFinished`（:6720）で after スナップショット取得前に呼出。2026-07-22 完了（ビルド未検証）。
- [x] **CE-2**: Auto/Flat/Linear ボタンも同様に書き戻し。ボタン操作は interactionStarted が発行されないため、before/apply/after/push の小ヘルパを用意して Undo 対象に含める。
  - 実装: `applyTangentEditWithUndo` ラムダ（:6138）を新設し、3ボタン（:6180-6201）から呼出。2026-07-22 完了（ビルド未検証）。
- 完了条件: ハンドルドラッグ・tangent ボタン適用後、プロパティの cp1/cp2/interpolation が更新され、再描画・再生で一致。Undo/Redo で往復できる。

### P1: 選択体系（Maya/Blender の基本）
- [x] **CE-3**: マーキー（矩形）選択 + 複数キー選択状態の保持（`selectedTrack_/selectedKey_` 単一 → 集合へ拡張）。
  - 実装: `selectedKeys_` セット追加（`ArtifactCurveEditorWidget.cppm`）。Shift+drag=マーキー、Shift+click=トグル選択、`A`=全選択トグル、`Esc`=選択解除+全表示。選択描画は `isKeySelected` 経由で複数対応。2026-07-22 完了（ビルド未検証）。
- [x] **CE-4**: 複数キーの平行移動（`G` grab またはドラッグ）。
  - 実装: `draggedKeys_` スナップショットで選択キー全てを同一デルタ移動（ドラッグ）。リリース時に影響トラックをソートし選択を再構築。既存 `keyMoved` シグナルをキー毎に発行するため書き戻し経路は変更不要。2026-07-22 完了（ビルド未検証）。
- [x] **CE-5**: `Ctrl+Click` でキー挿入（クリック位置の frame/value、補間は隣接から継承）。
  - 実装: `insertKeyAt()`（選択トラック優先、なければクリック位置に最も近いカーブのトラック）。ハンドルは隣接キーから25%ルールで自動設定。書き戻しは新設 `writeBackCurveEditorStructureDiffs()`（`ArtifactTimelineWidget.cppm`）が挿入/削除をフレーム集合差分で検出して `interactionFinished` で同期。複数削除（Delete/X）も同経路で Undo 対応。2026-07-22 完了（ビルド未検証）。
- 完了条件: 矩形で複数キーを選択し一括移動できる。挿入したキーがプロパティに Undo 付きで追加される。

### P2: タンジェント操作拡充 + 数値入力
- [x] **CE-6**: Break / Unify Tangents（`brokenTangents` フラグ活用、`V` メニューまたは `B` はマーキーと競合するため `V` 側へ）。
  - 実装: Curve EditorのBreak/Unify操作とTimelineのUndo付きボタン経路を確認。2026-07-24 完了（ビルド未検証）。
- [x] **CE-7**: Step（Constant）切替（InterpolationType::Constant への往復）。
  - 実装: Curve EditorのConstant切替とTimelineのStepボタン／Undo経路を確認。2026-07-24 完了（ビルド未検証）。
- [ ] **CE-8**: 選択キーの frame/value 数値エントリ欄（Maya Stats 相当。既存 `promptSetSelectedKeyValue` をパネル化）。
  - 進捗: Timelineの `Value...` / `Frame...` ボタンと既存ダイアログは接続済み。常設の数値欄とキーボード導線は未実装。
- 完了条件: tangent タイプの往復が Undo 対応で動作。数値入力でキーが確定更新される。

### P3: 表示・効率（後段）
- [ ] **CE-9**: バッファカーブ（編集前カーブの薄い表示、Maya Buffer Curve Snapshot 相当）。
- [ ] **CE-10**: 正規化表示トグル（値域の違うトラックを -1..1 に重ねる）。
- [ ] **CE-11**: サイクル表示（pre/post を点線、Maya Infinity / AE LoopOut 相当。`roving` フラグと整合）。
- [ ] **CE-12**: スナップ（整数フレーム/値/隣接キー/グリッド）。
- [ ] **CE-13**: キーの Copy/Paste（同トラック・別トラック間）。
- [ ] **CE-14**: Speed グラフの編集可（read-only 解除。別途設計）。

## 非目標（スコープ外）
- タイムライン painter 側のキーフレーム編集（dope sheet）の変更。
- `InputOperator` / KeyMap コアの変更（既存修正済みの仕組みをそのまま利用）。
- Speed グラフの書き戻し仕様変更（CE-14 で別途扱う）。

## 検証方法
- タンジェント編集後に再生・エクスポートで表示カーブと評価が一致すること（CE-1/2）。
- Undo/Redo でタンジェント・挿入・移動が往復すること。
- タイムライン表示（Tab 往復）でカーブとドープシートのキー位置が一致すること。
- 新規キーが "CurveEditor" KeyMap 経由で動作し、タイムライン側 QShortcut と両立すること。
