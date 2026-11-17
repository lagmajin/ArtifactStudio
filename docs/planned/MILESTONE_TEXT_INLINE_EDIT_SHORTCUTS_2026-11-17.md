# M-TXT-INLINE-1: キャンバス直接テキスト編集とショートカットのローカルバインド化

**最終更新:** 2026-11-17

**ステータス:** Not Started

## 目的

テキストレイヤーを「選んでダイアログで打つ」から「キャンバス上で直接打つ」へ移行する。併せて、テキスト操作の固定キーを `ShortcutBindings` のローカル／モーダルバインドへ置き換える。

**対象外:** `wiggle()` / `loopOut()` / `loopIn()` / `smooth()` は 2026-11-17 の照合で Core に完全実装済みと確認したため、本マイルストーンでは新規実装しない。Expression セレクタの変数注入不足など、残課題は別案件で扱う。

## 前提（2026-11-17 実コード照合の確定事実）

### 既に実装済み

- `ToolType::Text` は `Artifact/include/Tool/ArtifactToolManager.ixx:19` に存在し、ツール名は `Artifact/src/Tool/ArtifactToolManager.cpp:43` で `"TextTool"`。
- キャンバス click / drag でテキストレイヤーを生成する経路は `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp:38542-38594` の `createTextLayerAtCanvas()`。drag なら段落ボックス、click なら Point レイアウトになる。mouse press :28969-29006、release :34502-34518 も配線済み。
- ダブルクリック編集は `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cpp:5353/5400` → `editTextLayerInline()`（`Artifact/src/Widgets/CompositionTextEditor.cpp:1300-1324`）。ダイアログは `ArtifactTextEditorDialog`（同 :295）、入力は `TextEditorInput`（:245-293）。Ctrl+Enter 確定 / Esc 取消は eventFilter（:623-642）。
- Undo は `Artifact/include/Undo/UndoManager.ixx` の `SetTextLayerTextCommand`（:801-824）、キーフレームなら `SetLayerPropertyKeyframesCommand`（:674-707）、スタイルは `SetLayerPropertyValueCommand`（:709-738）の `MacroUndoCommand`。確定時に `LayerChangedEvent`（`ArtifactEventTypes.ixx:501-506`）を発行する。
- TextAnimator（Range / Wiggly / Expression セレクタ）は Inspector・Timeline・VP・TextGizmo へ配線済み（M-TXT-ANIM-1 参照）。
- Expression 組み込み関数は `ArtifactCore/src/Script/Expression/ExpressionEvaluator.cpp:633-692` で登録、:1453 以降で実装、`ArtifactCore/src/Property/AbstractProperty.cpp:448-587` が `value` / `time` / `keyframes` を注入する。

### 未実装・問題点

- 真のキャンバス上キャレット／範囲選択／キー入力が無い。現在の編集は `ArtifactTextEditorDialog` と `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp:38959-39036` の `editTextAtViewport()`（`QInputDialog::getMultiLineText`、:39016-39018）によるモーダル入力。
- 未使用の下書き `TextOverlayFilter`（`CompositionTextEditor.cpp:158-243`、QPlainTextEdit ベース、Commit `Ctrl+Enter` / Cancel `Esc` を自前判定）が存在するが、このファイル内では一度も生成されていない。実使用はビルド対象外の `Artifact/composition_codex_original.cppm` のみ。
- 固定キーが残る: ツールバーは `Artifact/src/Widgets/ArtifactToolBar.cpp:465-470` で `QKeySequence(Qt::CTRL | Qt::Key_T)`、エディタは `CompositionTextEditor.cpp:223-228 / 634-642` で `Qt::Key_Return/Enter` と `Qt::Key_Escape` を直判定。AGENTS.md の「固定キー禁止・ローカルバインド必須」に反する。
- 既存ドキュメントが古い: `docs/spec/SPEC_TEXT_TOOL_REQUIREMENTS_2026-07-31.md:108-114` はキャンバス生成、VP 直接編集、ダブルクリック編集、Undo を未実装とする。`docs/analysis/GAP_AE_NUKE_2026-08-01.md:24/105-110` も Text Tool 20% / VP 編集 5% / Wiggle 0% と記載する。

## Phase 計画

| Phase | 内容 | 状態 | 主な対象 |
| --- | --- | --- | --- |
| P1 | キャンバス上インライン編集（キャレット／選択／入力） | Not Started | `CompositionTextEditor.cpp`、`ArtifactCompositionRenderController.cpp` |
| P2 | ショートカットの `ShortcutBindings` / `Modal.Text` 化 | Not Started（ArtifactCore 変更を伴う） | `ShortcutBindings.ixx/.cpp`、`InputOperator`、`ArtifactToolBar.cpp`、エディタ |
| P3 | 既存ドキュメントの照合更新・runtime 受入 | Not Started | `docs/`、受入観点 |

## P1 — キャンバス上インライン編集

### 現状

キャンバス上にテキストを生成する導線と、ダブルクリックでダイアログを開く導線はある。しかし文字を打つたびにダイアログが開き、VP 上の見た目と入力位置が分離する。`TextOverlayFilter` の下書きはあるが未接続。

### 実装

- `TextOverlayFilter` を復活させ、VP 上のテキストレイヤー位置へ配置する。既存の `editTextLayerInline()` と `editTextAtViewport()` の呼び出しを、ダイアログ生成ではなくオーバーレイ生成へ差し替える。
- キャレット移動、範囲選択、全選択、IME preedit のガードは既存 `TextEditorInput` / `TextOverlayFilter` の挙動を踏襲する。
- 確定時は既存の `SetTextLayerTextCommand` または `SetLayerPropertyKeyframesCommand` を push し、`LayerChangedEvent` を発行して dirty と再描画を通知する。新規 signal/slot は追加しない。
- 段階的な移行として、まずダブルクリック導線をオーバーレイ化し、Text ツール選択中のクリック／drag 生成は既存 `createTextLayerAtCanvas()` を維持する。
- 文字種は ASCII / CJK / 絵文字 / 合字 / 縦書きを対象に、フォント指定は既存 `makeTextFont()` と Core `FontManager` を使う。

### 検証

- VP 上でダブルクリック → キャレットが出る → 文字を打つ → 確定でレイヤーへ反映 → 1回の Undo/Redo で往復する。
- Esc 相当の取消でレイヤーが変化しない。
- IME 変換中の確定キーが誤ってコミットされない。
- キーフレーム済み `text.value` を編集した場合、キーフレームが保持されたまま更新される。

## P2 — ショートカットのローカルバインド化

### 現状

`ShortcutBindings`（`ArtifactCore/include/UI/ShortcutBindings.ixx`）は `ShortcutId` を :15-191、`resetToDefaults()` を `ArtifactCore/src/UI/ShortcutBindings.cpp:902` 以降、`matches(const QKeyEvent*, ShortcutId)` を同ヘッダ :209 に持つ。コンテキスト文字列は `InputOperator`（`ArtifactCore/include/UI/InputOperator.ixx`）が管理し、`Modal.Transform` / `Modal.Import` 等の前例がある。

### 実装

- `ShortcutId` にテキスト編集用の ID を追加する（例: `TextEditCommit` / `TextEditCancel` / `TextTool`）。序数は末尾追加とし、既存の永続化 ordinal を変更しない。
- 既定キー、表示名、永続化キー、所有コンテキストを `resetToDefaults()` に定義する。所有コンテキストは `Modal.Text` とし、フォーカス中のテキストオーバーレイだけが解決する。
- `CompositionTextEditor.cpp` の `Qt::Key_Return` / `Qt::Key_Escape` 直判定と、`ArtifactToolBar.cpp:465-470` の `Ctrl+T` ハードコードを `ShortcutBindings::matches()` 経由へ置き換える。
- G / R / S / X / Y / Z、Shift / Ctrl、Esc、Enter は Blender 系の予約操作として衝突を確認する。既存の他ツール固定キー（V / H / Z / C / Y / Q 等）は今回の対象外とし、無関係な一括変更は行わない。
- 設定画面（`ArtifactContextShortcutProvider` / `ApplicationSettingDialog`）への表示、保存／再読込、空バインドによる無効化、同一コンテキスト内の競合検出、別コンテキストで同じキーを使えることを確認する。

### 検証

- `Modal.Text` 以外のフォーカスではコミット／取消キーが発火しない。
- バインドを空にすると操作が無効化される。
- テキスト編集オーバーレイ表示中だけ `Modal.Text` がアクティブになる。

## P3 — ドキュメント更新・受入

- `docs/spec/SPEC_TEXT_TOOL_REQUIREMENTS_2026-07-31.md:108-114` の 5.2 を、キャンバス生成とダブルクリック編集が実装済みである点へ更新する。残る VP 直接編集の項目は本マイルストーンの P1 完了後に更新する。
- `docs/analysis/GAP_AE_NUKE_2026-08-01.md` のテキスト欄と Wiggle / loopOut の 0% 表記を、Expression 組み込み関数が実装済みである点へ修正する。
- `MILESTONE_TEXT_ANIMATOR_ADD_WORKFLOW_2026-09-21.md` の P3（プリセット拡充）は本マイルストーンの対象外として分離する。
- runtime 受入（ビルド、module hygiene、D3D12 / Vulkan、Undo 往復、CJK / 絵文字、ショートカット設定の往復）はユーザーの明示指示後に実施する。

## 受入条件

- VP 上で直接テキストを入力・確定でき、1操作＝1 Undo で往復する。
- 確定／取消と Text ツール起動が `ShortcutBindings` のバインドで解決され、固定キーが残らない。
- 新規 signal/slot、QtCSS、QColorDialog、QImage 化、QPainter 合成を追加しない。
- 色選択は既存の承認済み picker、ツールアイコンは `Artifact/App/Icon/Studio/` のオリジナル SVG を使う。
- 既存の TextAnimator 評価経路と Expression 組み込み関数を壊さない。

## 検証状況

- 2026-11-17: 実コード照合のみ実施。ビルド・テスト・CMake は未実行（ユーザー指示待ち）。
- 2026-11-17: 子リポ（Artifact / ArtifactCore）の編集と ArtifactCore の ShortcutId 追加はユーザーから明示的に許可済み。
- 2026-11-17: ブランチ確認。親と `Artifact` は `codex/2026-11-13-dev-r2`、`ArtifactCore` と `ArtifactWidgets` は `codex/2026-11-13-dev` で不一致。コミット前に親子を同じ開発ブランチへ揃えること。

## コミット／プッシュ前の確認

AGENTS.md の親子リポジトリ workflow に従う。子を先にコミット／プッシュし、親の gitlink を更新してから親をプッシュする。ブランチ名は親子で統一する（上記の不一致を解消してから着手）。`ShortcutId` 追加は `ArtifactCore` の変更を伴うため、同子リポを先にプッシュしてから親の gitlink を更新する。

## 関連文書

- `docs/spec/SPEC_TEXT_TOOL_REQUIREMENTS_2026-07-31.md`
- `docs/planned/MILESTONE_TEXT_ANIMATOR_ADD_WORKFLOW_2026-09-21.md`
- `docs/planned/MILESTONE_EXPRESSION_LOOPOUT_RUNTIME_2026-06-02.md`
- `docs/analysis/GAP_AE_NUKE_2026-08-01.md`
- `docs/analysis/AE_PAIN_POINT_IMPROVEMENT_MAP_2026-08-13.md`
- `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md`
- `docs/WIDGET_MAP.md`
- `Insight.md`（2026-11-17 の項目）
