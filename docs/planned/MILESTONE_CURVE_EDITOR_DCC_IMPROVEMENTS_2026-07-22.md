**ステータス:** CE-1〜CE-14 実装済み（CE-14 は key move による retime / speed 書き戻しのみ）、runtime／実機検証が未完了

**最終更新:** 2026-10-31

2026-07-25: CE-8〜CE-13 のソース実装を再確認。常設数値入力、Buffer／Normalize／Cycle／Snap、Curve Copy/Paste まで実装済み。ビルド・ランタイム検証は未実施。

2026-10-31: CE-14 の契約を定義して実装。Speed グラフは key move のみ編集可（挿入・削除・copy/paste・clone・tangent・数値 value 入力は derived モードで拒否）。speed→source value の逆変換は `curveSpeedAt()` / `solveCurveKeyValueForSpeed()` に一本化。あわせて `focusTrack()` から選択変更を外し（timeline refresh が毎回 primary key を 0 に戻していた）、`setTracks()` で無効な選択 index を剪定し、derived track のドラッグ値を 0 でクランプ。**検証:** `ArtifactCurveEditorStandaloneTest` を Debug ビルドし、直接実行と CTest の両方で 85/85 pass。`ArtifactTimelineWidget.cpp` / `ArtifactCurveEditorWidget.cpp` / `ArtifactCurveEditorWidget.ixx` はアプリ側プロジェクトでも単一 TU コンパイル成功。アプリを起動しての Speed モード実機操作は未実行。

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
- 2026-08-15 の現行コード照合では、タンジェント編集の書き戻し経路は解消済み。`interactionFinished` で `writeBackCurveEditorStructureDiffs` と `writeBackCurveEditorTangentEdits` を呼び、`applyCurveEditorTrackToProperty` が cp1/cp2 と interpolation を property へ反映し、同一操作の Undo snapshot に含める。
- 残る確認課題は、Speed グラフの編集可否、任意 property の keying-set 制限、複雑な tangent／frame変更後の runtime 評価と再読込である。

## キー体系の整合ポリシー（Blender ライク系と矛盾しないために）

- 既存キー系: `InputOperator` / `KeyMap`（`ArtifactCore` `Input.Operator` モジュール、BUG_BLENDER_KEYMAP 修正済み）+ QShortcut（Tab、ツール V/H/Z/R/S）+ "Playback" KeyMap（Space/J/K/L/I/O/U）。
- カーブエディタの新規キーは **専用 KeyMap（"CurveEditor"）を InputOperator に登録する方式を正規とし**、タイムライン側 QShortcut と衝突しない割り当てにする。
- 割り当て案（Blender Graph Editor 準拠、衝突回避）:
  - `G` = grab（選択キー移動モード）、`B` = マーキー選択、`X` / `Delete` = キー削除
  - `Ctrl+Click` = キー挿入（Blender 方式）、`A` = 全選択/解除、`Shift+drag` = マーキー（既存選択に追加。置換は Esc 後）
  - `V` = tangent タイプメニュー（Auto/Flat/Linear/Step/Break/Unify）※ タイムラインのツール V（Selection）とはカーブエディタアクティブ時のみ有効としてコンテキスト分離
  - `F` = 選択キーにフレームイン（未選択時は可視キー全体）。`frameSelection()` を公開 API とし、正規化表示では y を -1.2..1.2 に固定する。
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
- [x] **CE-8**: 選択キーの frame/value 数値エントリ欄（Maya Stats 相当。既存 `promptSetSelectedKeyValue` をパネル化）。
  - 進捗: Curve Editor上部に選択キーのFrame／Value入力欄を追加。既存の keyMoved / interactionStarted / interactionFinished 経路へ接続済み。2026-07-25（ビルド未検証）。
- 完了条件: tangent タイプの往復が Undo 対応で動作。数値入力でキーが確定更新される。

### P3: 表示・効率（後段）
- [x] **CE-9**: バッファカーブ（編集前カーブの薄い表示、Maya Buffer Curve Snapshot 相当）。
- [x] **CE-10**: 正規化表示トグル（値域の違うトラックを -1..1 に重ねる）。
- [x] **CE-11**: サイクル表示（pre/post を点線、Maya Infinity / AE LoopOut 相当。`roving` フラグと整合）。
- [x] **CE-12**: スナップ（整数フレーム/値/隣接キー/グリッド）。
- [x] **CE-13**: キーの Copy/Paste（同トラック・別トラック間）。
- [x] **CE-14**: Speed グラフの編集可（key move のみ。2026-10-31 実装、standalone 85/85 pass、実機操作は未確認）。
  - 契約: Speed グラフの各 key は「隣接する非 Constant 区間の平均絶対スロープ × fps」(`curveSpeedAt()`)。
  - 縦ドラッグは `solveCurveKeyValueForSpeed()` で元プロパティの key value を逆算する（前後 key・frame・constant は固定、現在値に最も近い解を選ぶ）。
  - 横ドラッグは元 keyframe のフレーム移動。移動後の frame 配置で同じ target speed を解き直すため、水平ドラッグは speed を保ったまま retime する。
  - 挿入・削除・copy/paste・clone・tangent 操作・数値 value 入力は `ArtifactCurveEditorWidget::setDerivedGraphMode(true)` で拒否。内蔵 frame/value エディタは derived モードで非表示。
  - 書き戻しは `applyCurveEditorSpeedMove()`（`ArtifactTimelineWidget.cpp`）が `applyCurveEditorMove()` の衝突検査・Undo スナップショット経路を再利用する。interactionFinished の track 一括書き戻しは Speed モードでは実行しない（derived 数値を property に書かない）。
  - 既知の制限: 複数 key を同時に speed ドラッグした場合、各 key は個別に現在の property 状態へ解を当てる。等スパンの隣接区間では target speed に対して解が plateau になり、現在値が既に target を満たす場合は現在値を維持する。到達不能な target（負値、隣接が許す最小平均未満）は編集を拒否する。

## 非目標（スコープ外）
- タイムライン painter 側のキーフレーム編集（dope sheet）の変更。
- `InputOperator` / KeyMap コアの変更（既存修正済みの仕組みをそのまま利用）。
- Speed グラフの書き戻し仕様変更（CE-14 で別途扱う）。

## 検証方法
- タンジェント編集後に再生・エクスポートで表示カーブと評価が一致すること（CE-1/2）。
- Undo/Redo でタンジェント・挿入・移動が往復すること。
- タイムライン表示（Tab 往復）でカーブとドープシートのキー位置が一致すること。
- 新規キーが "CurveEditor" KeyMap 経由で動作し、タイムライン側 QShortcut と両立すること。

## テスト＆実装ループ（2026-10-31 拡充）

対象: `tests/Artifact/CurveEditorStandaloneTest.cpp`（`ArtifactCurveEditorStandaloneTest`、CTest 登録済み）。

1. `cmake --build build --target ArtifactCurveEditorStandaloneTest --config Debug` でハーネスだけをビルドする（`ArtifactAppRuntime` はリンクしない）。
2. `ctest --test-dir build -C Debug -R ArtifactCurveEditorStandaloneTest --output-on-failure` で契約 85 本を実行する。
3. `build/tests/Artifact/.../ArtifactCurveEditorStandaloneTest --ui` で 16 シナリオを切り替えて目視確認する（1〜9 キー、F frame、N normalize、B buffer、A all、Esc clear）。
4. 失敗時は `ARTIFACT_CURVE_EDITOR_CAPTURE_DIR` の PNG（シナリオ別 + contact sheet + `op_*` の before/after）を見て、ピクセル契約とロジック契約のどちらが落ちたかを切り分ける。
5. 意図した描画変更なら `ArtifactCurveEditorStandaloneTest --update-baselines` で `tests/Artifact/baselines/curve-editor/`（シナリオ 16 枚 + `operations/` の before/after 36 枚）を更新する。比較は `--baseline-dir` / `ARTIFACT_CURVE_EDITOR_BASELINE_DIR` で差し替え可能。失敗時は `baseline_*_{expected,actual,diff}.png` が capture dir に出る。判定は「チャネル差 > 12 の画素が 0.2% 以下、平均絶対差 0.5 以下」。

**2026-10-31 実行結果:** Debug ビルド成功（`build/tests/Artifact/Debug/ArtifactCurveEditorStandaloneTest.exe` 再生成）、直接実行 85/85 pass、`ctest -R ArtifactCurveEditorStandaloneTest` 1/1 pass。アプリ側は `MSBuild /t:ClCompile /p:SelectedFiles=...` で .ixx / カーブエディタ実装 / タイムライン実装の 3 TU がコンパイル成功。初回実行で落ちたテストはすべてテスト側の想定違い（Ctrl は press 時点で insert ジェスチャになる、キードラッグ開始で buffer が自動表示される、選択トラックは `QColor::lighter(130)` で彩度も下がる、`focusTrack()` は選択を変えないので paste 先は対象トラックの key をクリックして決める）で、実装契約を変えたのは `focusTrack()` の選択非干渉化のみ。

**追加契約（第2便）:** `FocusTrackDoesNotClobberTheSelection`、`SetTracksPrunesStaleSelection`、`DerivedGraphModeClampsSpeedToZero`、`DerivedGraphModeCanBeTurnedOff`、`XKeyDeletesEverySelectedKey`、`PasteOnTheSameTrackAnchorsOnTheSelection`、`SolveSpeedHandlesDecimalFrameRates`、`SolveSpeedHandlesDecreasingNeighbours`、`SolveSpeedIsIdempotentForUnequalSpans`。

**追加契約（第3便）:** `KeyDragPublishesInteractionAndMoveEvents`、`TangentEditPublishesOneInteraction`、`RejectedEditsPublishNoInteractionEvents`、`PlayheadScrubPublishesFrameChanges`（EventBus の Undo 契約）、`SelectAllOnlySelectsVisibleTracks`、`FitToContentIgnoresHiddenTracks`、`MarqueeSkipsHiddenTracks`、`InfinityExtensionStaysFlatForConstantEdges`。あわせて constant 端点が Infinity プレビューへ傾きを漏らさないよう描画を修正。

**追加契約（第4便）:** `SpeedSolverRoundTripsOnRandomCurves`（300 ケース、解けたら `curveSpeedAt` が target に戻る）、`RandomizedEditSequenceKeepsInvariants`（固定 seed 300 操作、キー frame の単調性・選択 index と primary の整合・選択数 ≤ 総キー数を毎回検査）。

**追加契約（第5便・操作→キャプチャ→画素）:** `ClickPaintsTheSelectionHighlightOnTheClickedKey`、`KeyDragRepaintsTheDiamondAtTheNewPosition`、`DeleteRemovesThePaintedKey`、`InsertPaintsANewDiamondAtTheClickPoint`、`PlayheadScrubRepaintsAtTheNewFrame`、`NormalizedToggleRepaintsKeyPositions`、`DerivedModeStopsPaintingTangentHandles`、`BufferCurveIsPaintedAtThePreEditPosition`、`MarqueeIsPaintedOnlyWhileDragging`、`PastePaintsSelectedKeysAtTheDestination`。各テストは before/after を `op_<name>_{before,after}.png` に保存する。この追加で **buffer curve が塗りだけでストロークされていない実バグ**を検出し修正した（area fill が pen を `Qt::NoPen` にしたまま buffer は infinity ブロックを飛ばすため、`drawPath` が線を描いていなかった）。

**追加契約（第6便・基準画像回帰）:** `RenderScenariosMatchStoredBaselines`（16 シナリオを `tests/Artifact/baselines/curve-editor/` と比較、`--update-baselines` で更新、失敗時は expected/actual/diff を保存）、`ImageDiffSeparatesNoiseFromRealChanges`（比較器の単体契約）、`MultiKeyDragRepaintsEverySelectedKey`、`SelectAllHighlightsEveryKeyAndEscapeClearsIt`。負例検証として、ベースラインのコピー 1 枚に矩形を描いて `--baseline-dir` で流し、`differingFraction=0.025` で FAILED になることを確認した。

**追加契約（第7便・フレームインと形状）:** `FrameSelectionFitsTheSelectedKey`、`FrameSelectionUsesEverySelectedKey`、`FrameSelectionFallsBackToVisibleKeys`、`FKeyFramesTheSelection`（`frameSelection()` API と F キー）、`FlatTangentsRepaintTheCurveShape`（flat tangent 後に frame 15 のカーブが直線より上へ描き直されることを列画素で検証）。F は従来の「選択トラックにフォーカス」から「選択キーにフレームイン（未選択時は全可視キー）」へ変更。

**追加契約（第8便・描画再計算）:** `HandleDragMovesThePaintedHandlePin`（ハンドル点が旧位置から消え新位置に描かれる）、`DeleteMiddleKeyRecomputesThePaintedCurve`（中間キー削除で frame 25 のカーブが大きく下へ再描画）、`InsertKeyRecomputesThePaintedCurve`（挿入した 45/25 キーにより frame 42 のカーブが下へ）、`FocusTrackHidesAndRestoresTheOtherCurve`（フォーカスで他トラックのカーブが消え、解除で戻る）。あわせて `writeCapturePair()` が操作の before/after も `baselines/curve-editor/operations/` に保存・比較するようにし、基準画像の**欠落時**は空ディレクトリを `--baseline-dir` にして 16 件の `missing baseline` と 36 件の `missing operation baseline` で FAILED になることを確認した。

**追加契約（第9便・貼り付け位置）:** `PasteWithoutSelectionAnchorsOnThePlayhead`（コピー後に選択を外し、再生ヘッド 45 で Ctrl+V → 45/75/105 に貼られ、source track が選択先になる）。あわせて `CopyPasteAcrossTracksAnchorsOnTheSelection` の「選択なし paste」節を新仕様へ更新。実装では Ctrl+V の外側ガードから「選択トラック必須」を外し、`pasteCopiedKeys()` が「選択トラック → コピー元トラック」の順で宛先を決め、選択キーが無ければ `currentFrame_` をアンカーにする。`pasteCopiedKeys()` が失敗したときに `interactionFinished` を発行しない漏れも修正した。

**追加契約（第10便・マーキー追加選択）:** `ShiftMarqueeAddsToTheExistingSelection`（既存選択 + 箱で囲んだキーが両方残る）、`EscapeThenMarqueeReplacesTheSelection`（Esc 後は箱のキーだけ）。未使用だった `marqueeAdditive_` を有効化し、Shift は他の選択操作と同じく追加修飾として扱う。

契約グループ:

総数 85 本。

- Speed math: `curveSpeedAt` の区間平均・Constant 除外・端点、`solveCurveKeyValueForSpeed` の端点/内部解・到達不能・等スパン plateau・方向維持・小数 fps・減少方向・冪等性。
- 当たり判定: 0 長ハンドルでキー移動、非 0 ハンドルでタンジェント、正規化ビューのクリック/マーキー/ドラッグ。
- 編集規則: derived モードの構造編集拒否・キー移動許可・0 クランプ・解除、read-only の tangent 拒否、copy/paste（別トラック/同一トラック/未選択時は再生ヘッド）、clone、Backspace/X 削除、Ctrl スナップ、reorder 後の選択再構築、内蔵数値エディタ、focus の選択非干渉、setTracks の選択剪定。
- 表示・ナビゲーション: buffer toggle、正規化 API、double-click focus、wheel 軸分離、pan、visible トラックのみの A/fit/marquee、constant 端点の infinity。
- EventBus: ドラッグ/タンジェントの Started/Finished/Moved/Selected、拒否操作の無発行、scrub の frame 通知。
- 操作→キャプチャ→画素: click/drag/delete/insert/scrub/normalized/derived/buffer/marquee/paste/handle drag/focus の before/after PNG と、期待位置の色検証。複数キー同時ドラッグ、A 全選択ハイライト、Esc 解除、キー削除/挿入後のカーブ再計算も含む。
- 基準画像回帰: 16 シナリオ + 操作 before/after 36 枚と `tests/Artifact/baselines/curve-editor/` の比較、比較器単体契約、`--update-baselines` 更新経路。`writeCapturePair()` が操作画像も保存・比較する。
- フレームイン/形状: 選択キーへの `frameSelection()` と F、flat tangent 後のカーブ位置変化。
- Render: 16 シナリオの PNG、背景/グリッド/カーブ/プレイヘッド/定数ステップ/マーキー/buffer/選択ハイライトのピクセル契約。

ハーネスに含めないもの: `QInputDialog` を開く `promptSetSelectedKeyFrame()` の対話結果（内蔵スピンボックス側で frame/value 編集は検証済み）、GPU curve snapshot の見た目一致（別経路）。

## 2026-07-25 実装監査

- CE-1〜CE-13 は、タンジェント書き戻し、複数選択／移動／挿入、数値入力、Buffer／Normalize／Cycle／Snap、Copy/Paste を含め、計画書の実装記録とソース確認が一致する。
- CE-8 の数値入力は実装済みだったため、チェック欄を `[x]` に修正した。
- CE-14（Speed グラフの編集）は非目標ではなく後続項目として未実装である。
- ビルド、runtime、Undo／Redo の実機動作、評価カーブとの一致は未検証のため、マイルストーン全体は `In Progress` のままとする。

## 2026-08-15 現行コード監査

- `ArtifactCurveEditorWidget` に Value／Speed 表示、Bezier handle、複数キー選択・移動・挿入・削除、tangent 操作、数値入力、Buffer／Normalize／Cycle／Snap／Copy-Paste の経路が存在する。
- `ArtifactTimelineWidget` 側には curve editor の snapshot、tangent／構造差分の書き戻し、Property keyframe への Undo 経路があり、旧記述の「タンジェントがローカルだけ」は現行コードには適用しない。
- Speed グラフは引き続き read-only で、速度カーブを編集して元の keyframe へ書き戻す契約は未実装。
- ビルド／runtime／Undo-Redo／再生・export での評価カーブ一致はこの監査では実行していない。

判定（2026-08-15 時点）: **通常の Value カーブ編集と主要 DCC 操作は実装済み。Speed 編集、実機検証、回帰検証は pending。**

### CE-14 契約（2026-10-31 更新）

2026-08-15 の調査メモは「速度1点から元 keyframe へ一意に逆変換できない」と結論していたが、2026-10-31 に境界条件を固定して一意化した。

- 速度の定義: key i の速度 = 隣接する有効区間（`keys[i-1].constant` と `keys[i].constant` を除外）の `|Δvalue| / Δframe * fps` の平均。`curveSpeedAt()` が唯一の定義元。
- 逆変換: 前後 key の value / frame / constant を固定し、key i の value だけを未知数として解く。区間は 1〜2 本なので区分線形方程式になり、解析解の候補（前後より外側2本、内側2本）を評価して target を満たすもののうち現在値に最も近いものを採用する。
- 唯一性が崩れるのは「前後区間のフレーム幅が等しく、かつ target が現在の平均と異なる」場合。この場合は解なしとして編集を拒否する（値のジャンプを作らない）。target が現在の平均と等しい場合は現在値を維持する。
- 端点 key は有効区間が1本なので一意。値の増減方向は現在の符号を維持する。
- 時間移動は「移動後の frame 配置で同じ target speed を解き直す」と定義する。したがって水平ドラッグは speed を保った retime、垂直ドラッグは frame を固定した速度変更になる。
- Undo は既存の `CurveEditorInteractionStarted/Finished` スナップショット経路（1 interaction = 1 command）をそのまま使う。
- 実装: `curveSpeedAt()` / `solveCurveKeyValueForSpeed()`（`Widget.CurveEditor`、standalone で契約テスト済み）、`applyCurveEditorSpeedMove()`（タイムライン）、`ArtifactCurveEditorWidget::setDerivedGraphMode()`（構造編集・tangent 編集の拒否）。
