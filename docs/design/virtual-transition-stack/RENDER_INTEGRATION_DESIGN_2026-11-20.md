# Virtual Transition — 描画統合設計（提案）

**最終更新:** 2026-11-23
**ステータス:** P1/P2実装済み、P3画素合成統合は未完了（model API・3描画経路のdraw plan接続を追加。2026-11-22に対象target buildとtransition契約CTest 10/10 Passを確認）

## 1. 目的と範囲

[要件・マイルストーン](../../planned/MILESTONE_VIRTUAL_TRANSITION_STACK_2026-11-20.md) のうち、2026-11-20 時点で未実装の **A/B 一時合成・通常描画の抑制・stack insertion の画素順反映・Preview/final parity** について、実装前に設計決定と作業順を固定する。

- 対象: Artifact（composition model / render queue / preview controller / timeline UI）、ArtifactCore（crossfade kernel）。
- 今回の成果物: 本設計書、および既存 milestone / design README への相互リンクと決定事項の更新。
- 対象外: production code、テストコード、CMake、ビルド・テスト実行、画像キャプチャ。
- 制約: AGENTS.md（新規 signal/slot・ReactiveEvents・QtCSS・QImage/QPainter 本流禁止、独自コンテナ優先、ホットパス無確保）、HOT_PATH_RULES.md、親子 git ワークフロー。

## 2. 監査で確定した事実（ファイル:行は現 worktree）

| 領域 | 確定事実 |
|---|---|
| Model | `CompositionTimelineTransition` は値型で ID/kind/easing/source A/B LayerID/stackPosition/range/enabled を保持。runtime stack index は JSON 対象外。`Artifact/include/Composition/ArtifactAbstractComposition.ixx:227-276`、`Artifact/src/Composition/ArtifactAbstractComposition.cpp:4624-4667` |
| 評価 | 単数 API は最初の active のみ、複数 API は呼び出し側 `ArtifactArray` workspace へ全 active を格納。opacity 係数と `containsSourceLayer()` はあるが描画側は係数のみ利用。`ArtifactAbstractComposition.cpp:7445-7557`、`:4676-4720` |
| 挿入位置 | `insertionIndex` は model 側で計算されるが **どの render loop も消費していない**。layer 変更時の index cache 更新は実装済み。`:2301-2313`、`:2762,2817,2853,4228,4263,4279,4295,6136,7089-7091,7175,7322-7323,8277` |
| Preview | GPU preview のみ評価 API を呼ぶ。layer loop `:45651`、opacity `:45847-45864`、accum blend `:46144-46155 / :13926-14015`。direct fallback `:46788` は係数未適用（preview 内部で既に分岐）。`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp` |
| Render Queue | CPU のみ評価 API を呼ぶ（thread_local workspace `:4643-4651`）。GPU 2 経路（`:7437`、`:4990`）は評価 API を呼ばず `opacityOverride=1.0`。`Artifact/src/Render/ArtifactRenderQueueService.cpp` |
| GPU kernel | straight BGRA → premultiplied lerp → straight BGRA の compute shader / PSO / renderer facade / 単体テストは存在するが **production caller は 0**。`ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx:139-177`、`ArtifactCore/include/Graphics/Shader/Compute/LayerBlendPipeline.ixx:105-112`、`ArtifactCore/src/Graphics/LayerBlendPipeline.cpp:1011-1077`、`Artifact/include/Render/ArtifactIRenderer.ixx:813-817`、`Artifact/src/Render/ArtifactIRenderer.cpp:8184-8196` |
| 二重描画 | progress 0<p<1 では A/B とも opacity>0 となり通常 pass で両方描画される。`containsSourceLayer()` は未使用。`ArtifactCompositionRenderController.cpp:45847-45864`、`ArtifactRenderQueueService.cpp:4767-4772` |
| Undo | transition 用 UndoCommand は皆無。UI が setter を直接呼ぶ。`Artifact/include/Undo/UndoManager.ixx`（Transition 一致なし）、`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp:6211,6236-6349,6997,7019`、`Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp:7783-7789` |
| 複製 | render-queue snapshot は JSON 経由で transition を保持するが、Project の Composition 複製は layer のみコピーし transition を落とす。`Artifact/src/Render/ArtifactRenderQueueService.cpp:4527-4533`、`Artifact/src/Project/ArtifactProject.cpp:1819-1888` |
| Timeline UI | T マーカー / hover 強調 / 選択時バー / hit-test / offscreen ミラーは実装済み。新規 signal/slot なし（既存 `TimelineLayerSelectionRequestedEvent` 再利用）。専用詳細エディタは無く右クリック `QInputDialog` のみ。`ArtifactTimelineTrackPainterView.cpp:7128-7204,7482-7512,3743-3798,8626-8635`、`ArtifactTimelineWidget.cpp:9990-10020,11729-12104`、`ArtifactLayerPanelWidget.cpp:6174-6187,6948-7022` |
| テスト | `ArtifactVirtualTransitionStackContractTest` 7 件 + GPU kernel 1 件が登録済みだが未ビルド・未実行。描画順・抑制・parity の契約は未実装と明記。`tests/Artifact/ArtifactVirtualTransitionStackContractTest.cpp:41-455`、`tests/Artifact/ArtifactVirtualTransitionGpuCrossfadeTest.cpp:21-128`、`tests/Artifact/CMakeLists.txt:72-84` |

## 3. 未実装ギャップ（優先度付き）

| ID | ギャップ | 影響 | 優先度 |
|---|---|---|---|
| G1 | `insertionIndex` が描画に反映されない | stackPosition を編集しても画素順が変わらない（要件 3/4 違反） | P0 |
| G2 | A/B の一時 group 合成が無い / kernel が未接続 | crossfade が「両方を薄く描く」だけで完結しない | P0 |
| G3 | 通常 pass の A/B 二重描画 | 中間 progress で過剰な重ね合わせ | P0 |
| G4 | GPU final が評価 API を呼ばない / preview direct が係数を無視 | Preview/final および preview 内部の parity 不成立 | P0 |
| G5 | 同時 active transition の合成順未定義 | 異なる source の 2 組以上で結果が不安定 | P1 |
| G6 | transition の Undo 皆無 / Project 複製で消失 | 編集安全性・保存整合性 | P1 |
| G7 | Wipe/Slide が UI で選べるが評価・pass とも無し | 選択しても何も起きない（未対応 parameter の露出） | P1 |
| G8 | 専用 Transition Stack 詳細エディタが無い | 疑似 3D stack / A・B・位置の一覧編集が不可能 | P2 |
| G9 | 描画契約テスト・parity テスト欠如 | 上記の完了判定ができない | P0（各実装と同時） |

## 4. 設計決定

凡例: **確定** = 既存コード/文書の事実、**提案** = 本設計で採用したい方針、**要確認** = ユーザ判断が必要。

### D1. 描画計画（draw plan）の導入 — 提案

- 現行の per-frame 評価 workspace（`evaluateTimelineTransitionsAtFrame`）は維持し、そこから **各 render loop 共通の描画ステップ列** を組み立てる。
- ステップは値型で `Layer(layerIndex)` と `TransitionComposite(evaluationIndex)` の 2 種のみ。呼び出し側が事前予約した `ArtifactCore::ArtifactArray<Step>` に格納し、フレーム中は `removeAll()` で容量を再利用する（新規コンテナ・新規モジュールは追加しない）。
- P2 追加: active transition の source A/B は「本来の stack index の Layer step」を出さず、挿入位置の直前に Layer step を出す（移動）。これにより通常 pass での二重描画が起きず、`stackPosition` が画素順へ反映される。`TransitionComposite` marker は残し、P3 で専用合成に差し替える。256 layer 超は移動を行わない安全な no-op（固定容量配列、フレーム中確保なし）。
- 構造情報（source 解決済み ID / insertionIndex / 順序 key）は composition 変更時にキャッシュし、progress だけを毎フレーム評価する。指数的検索を frame loop へ戻さない。
- 配置先は render 側の共有ヘルパーとし、preview GPU / CPU final / GPU final の 3 経路から同じステップ列生成を呼ぶ。新規 signal/slot やイベントバスは使わず、既存の描画呼び出しと `CompositionChangedEvent` のキャッシュ更新のみを使う。

### D2. A/B 一時合成と kernel 接続 — 提案

- preview GPU: 既存 `RenderPipeline` の事前確保済み float ターゲットを拡張し、transition 用に **A / B / 出力の 3 枚を 1 スロット** として composition/リサイズ時にコールドパスで確保する（`Artifact/include/Render/ArtifactRenderLayerPipeline.ixx:72-319,192-206,270-280` のプール方針を踏襲）。
- A/B を既存の `drawGpuLayerToIntermediate` でそれぞれ別ターゲットへ描画 → `ArtifactIRenderer::crossfadeLayers(..., progress, region)` → 出力 SRV を `blendGpuLayerIntoAccum` へ 1 回だけ投入。入力と出力のエイリアス禁止は kernel 側が既に検証（`:1026-1029`）。
- CPU final: 新規の `QImage` / `QPainter` / `CompositionMode` は使わず、`ImageF32x4_RGBA` 上の専用 crossfade + 既存 `SoftwareRender::blendSurface` で合成し、既存の `QImage` 境界は最終出力側だけに留める。
- GPU final: まず thread_local workspace で評価 API を呼び（CPU final と同じ）、既存の直接 render target 描画に対し **挿入位置まで通常描画 → A/B 一時合成を 1 回描画 → 残りを通常描画** の順を取る。
- プール枯渇時は one-shot resource を生成せず、上限（初期値 8 同時 active）を超える分は `drop`（その transition を一時スキップ）として記録する。上限と根拠は実装時に計測して見直す。

### D3. 二重描画抑制と opacity 係数 — 決定（2026-11-21 ユーザ承認）

- 各 layer loop の先頭で `containsSourceLayer(workspace, layer->idView())` が真なら **通常 pass を `continue`**（`ArtifactAbstractComposition.cpp:4712-4720` を利用）。
- 一時合成へ入る A/B には `opacityMultiplierForLayer()` を適用しない（crossfade が alpha を補間するため二重減衰になる）。また、通常 pass の係数は「transition に属さない layer」に限って従来どおり適用する。
- `Cut` / `Crossfade` / `Dissolve` は現行どおり opacity 係数で等価（Dissolve は Crossfade と同一結果）だが、parity 確保のため **3 種とも D2 の一時合成経路へ統一** する。Dissolve のノイズ等は本件の範囲外。
- 一時合成の外側 composite に使う blend mode / opacity は **source A の値を継承し、opacity は 1.0**（A/B 内部は premultiplied lerp）。隣接 layer との前後関係は D4 の順序規則に従う。

### D4. 挿入位置と同時 active の順序 — 提案

- `stackPosition = -1` は source A の現在 index、0 以上は `[0, layerCount-1]` へ clamp（現行計算を維持）。
- layer loop を 1 回通し、index `i` に到達した時点で `insertionIndex == i` の transition 合成を **全て** 先に描画してから layer `i` を描画する。A/B 自身は D3 でスキップ済みなので二重描画しない。
- 同一 `insertionIndex` の複数 transition は **transition の安定 ID の辞書順** で描画し、順序を決定的にする。model は source 共有の inclusive overlap を拒否済みのため、競合は主に独立 source 間（`:7093-7106`）。
- source 未解決・index 負・layerCount 0 の評価は現行どおりスキップし、その frame は transition なしとして描画する（安全な no-op）。

### D5. 対応 kind の扱い — 決定（2026-11-21 ユーザ承認: UI から未実装 kind を隠す）

- `Wipe` / `Slide` は UI で選択可能なのに評価・GPU pass が無く、選択しても無変化。要件「未対応 parameter を UI だけに表示しない」に反する。
- 推奨: 専用 pass ができるまで追加メニュー／kind 一覧から外し、model 側は未知 kind を安全な no-op として扱う契約テストを残す（保存データ互換は維持）。
- 代替案: Wipe/Slide も一時合成経路で Crossfade と同じ結果にフォールバック（見た目は欺瞞となるため非推奨）。

### D6. Undo / 複製 — 提案

- transition の追加・削除・range/kind/easing/name/source/enable/stackPosition 変更を、既存 `MacroUndoCommand` または新規 `TimelineTransitionSnapshotCommand` で包む。スナップショットは `QVector<CompositionTimelineTransition>` の before/after を取り、適用は既存 `setTimelineTransitions()`（検証込み）経由にする。
- `ArtifactProject.cpp:1819-1888` の Composition 複製へ transition コピーを追加する（新規 ID を割り当て、source は複製先 layer 名/ID へ解決）。
- `duplicateTimelineTransition()` の range 重複再検証漏れを塞ぐ（`:7379-7398`）。

### D7. Preview / final parity — 提案

- 3 経路（preview GPU・CPU final・GPU final）が **同じ `evaluateTimelineTransitionsAtFrame()` 結果と同じ draw plan** を使うことを契約化する。
- preview direct fallback（`:46788`）は transition 係数未適用のまま残さず、D1 の draw plan 経路へ統合するか、未対応を明示するフラグを付ける。
- parity は「同一 frame hash または定義済み tolerance」で比較し、CPU/GPU を個別識別する。未確認の parity は主張しない（milestone テストグループ C に従う）。

### D8. ホットパス / リソース規律 — 提案

- workspace・一時ターゲット・ステップ列は `CompositionChangedEvent` / リサイズ / transition 数の変化時にコールドパスで予約。frame 中の `new`、GPU texture/PSO/UAV 生成、readback、文字列整形ログを追加しない。
- `ArtifactArray` の `removeAll()` は容量を維持（`:147`）。同時 active 上限と pool miss / drop 数は診断カテゴリで遅延評価し、閾値は基準 capture 取得前に決めない。
- 新規 `.ixx` / `.cppm` は可能な限り避け、既存実装ファイルへの追記で済ませる。追加する場合は CMake 再スキャン要因であることを作業依頼時に明示する。

## 5. 実装順序（提案）

| Phase | 内容 | 完了条件 |
|---|---|---|
| P0 設計確定 | 本設計の D5（kind 方針）と D3（blend 継承）の承認、D4 の順序規則の確定 | ユーザ承認が文書に反映 |
| P1 評価→描画計画 | D1 の draw plan 生成と 3 経路への接続（まず opacity 係数のまま） | 既存 7 件が壊れず、insertionIndex がステップ列に現れる contractual test が通る |
| P2 挿入・抑制 | D3/D4 の layer skip・挿入描画 | stackPosition 変更で画素順が変わり、中間 progress で A/B が二重描画されない |
| P3 kernel 接続 | D2 の preview GPU → CPU final → GPU final の順に接続 | kernel 単体でなく render loop 経由の crossfade contract が通る |
| P4 parity | D7 の direct fallback 統合と同一 frame 比較 | preview/final の差が定義済み tolerance 内（未達は数値で記録） |
| P5 安全性 | D6 の Undo・複製・重複検証、D5 の UI 整理 | Undo/Redo と save/reload で全字段が復元 |
| P6 詳細 UI | D8 制約下で Transition Stack 詳細エディタ（G8） | owner-draw・ja/en/zh/zh-TW・新規配線なし・viewport 変更なし |
| P7 受入 | テストグループ C/D の追加、連続フレーム・alpha 境界・odd dimensions・load 計測 | milestone の受入条件を Pass/Fail/N/A で更新 |

各 Phase は `docs/planned/MILESTONE_VIRTUAL_TRANSITION_STACK_2026-11-20.md` の Phase 0〜5 と対応し、既存の「未実装」記述を消すのは **実装とテスト実行が完了した後** とする。

## 6. 追加する契約テスト（案）

- draw plan: `insertionIndex` の変更で step 列の順序が変わること / 同一 index の ID 辞書順。
- suppression: progress 0.5 で source A/B が通常 pass に現れないこと。
- insert: 3+ layer + 中間 layer で、stackPosition=A/B/明示 index の順が期待どおり入れ替わること。
- simultaneous: source を共有しない 2 組を同一 frame で評価し、両方の結果が残ること。
- kernel integration: render loop 経由で progress 0/0.5/1 の straight alpha 出力が得られること（kernel 単体テストの代用にしない）。
- parity: 同一 fixture/frame で preview GPU と CPU/GPU final を個別比較、差分を数値で記録。
- load: steady-state で workspace 成長・GPU resource 生成が 0 であること（観測手段は実装時に選定）。
- UI: 既存の marker/bar テストに `ja`, `en`, `zh`, `zh-TW` を追加（新しいロケールが設定可能ならそれも）。

## 7. 要確認事項

1. **D2**: 同時 active 上限の初期値 8 でよいか（根拠は未計測）。P2/P3 の実装時に計測して見直す。
2. **D4**: 安定 ID のバイト順を順序 key として用いる前提でよいか。UUID 由来 ID は辞書順が成立するが、人間が付けた任意 ID（`first` 等）は QUuid::fromString が null UUID になり、実装上は登録順が tie-break になる。

## 8. P1 実装状況（2026-11-21）

- `CompositionTimelineTransitionDrawStep`（Layer / TransitionComposite）と `buildTimelineTransitionDrawPlan()` を `Artifact.Composition.Abstract` に追加。評価 workspace と step workspace は呼び出し側所有、フレーム内では `removeAll()` で容量再利用。
- 順序は `(insertionIndex, stable transition id bytes)`。同時 active が 64 件超の場合のみ model 登録順へフォールバック（固定容量 `std::array<int,64>`、フレーム中の確保なし）。安定 ID のバイト列は transition が composition へ入る時点でキャッシュし、評価はコピーのみ。
- 3 描画経路を draw plan 消費へ変更: preview GPU（`ArtifactCompositionRenderController.cpp`）、CPU final・GPU final 2 経路（`ArtifactRenderQueueService.cpp`）。`TransitionComposite` step は P2/P3 まで no-op のため、現時点の画素出力は従来と同一。
- 契約テスト `DrawPlanPlacesTransitionCompositeAtInsertionIndex` を追加。`stackPosition` 変更で composite step が目的の layer 直前に移動することを検査する。
- P2 実装（2026-11-21）: draw plan が active transition の source A/B を insertionIndex 直前に移動する Layer step として出し、自然位置の Layer step を抑制した。4 描画ループはそのまま step 順で描画するだけのため、各 source は 1 フレームに 1 回だけ描画される。契約テストの期待 step 列を更新。256 layer 超・64 transition 超は登録順/移動なしのフォールバック。
- ビルド/テスト実行（2026-11-21、ユーザ明示許可）: `x64-Debug-Agent` (Ninja/MSVC 14.52, `ARTIFACT_BUILD_TESTS=ON`) で `ArtifactVirtualTransitionStackContractTest` をビルド成功。実行結果は 8 件中 7 件 Pass（新規 `DrawPlanPlacesTransitionCompositeAtInsertionIndex` を含む）、`StableSourceIdsSurviveRenameAndLegacyNamesMustBeUnique` は layer stack index の期待値不一致で Fail、`TimelineShowsCompactMarkerUntilTransitionIsSelected` は offscreen 実行でハング。GPU crossfade kernel 単体テスト 1 件は Pass。
- この時点で未実装だった項目: P3 の crossfade kernel 描画ループ接続、P4 parity、P5 Undo / UI 整理。Fail / ハングの2件は後続の2026-11-22修正・検証で解消した。UI / Undo の現状は本書 §10 を参照。P3画素合成統合とP4 parityは引き続き未完了。
- ビルドを通すための既存コード修正（今回の機能変更とは別）: Boost UUID `data()` 呼び出し、`ArtifactShapeLayer::evaluatePathAt()` の PImpl 定義後の配置、`ArtifactPythonAPI.cpp` の QString→std::string 変換、`ArtifactLayerPanelWidget.cpp` の画像 filter 固定文字列。一時プレースホルダ `tests/Artifact/ArtifactLayerGroupingUiContractTest.cpp` は CMake 参照切れ回避のため残している（Git 追跡対象の変更ではない）。

## 10. Transition Stack UI / Undo 実装状況（2026-11-23）

- Layer Panel の既存 Transition Stack メニューから開く編集ダイアログを追加し、name、source A/B、kind、easing、frame range、stack position、enabled を編集する。
- ダイアログ上部にQPainter owner-drawのA/B stack previewを置き、ソースカード・遷移矢印・kind・easing・range長・挿入位置を表示する。2026-11-23に Windows Qt platform pluginでアプリwindowを不可視化し、localhost MCPのQt仮想入力とwidget captureで実表示を確認した。OS入力注入やデスクトップcaptureは使用しない。
- 参照切れの source A/B はダイアログ上で明示し、代替レイヤーを選ぶまでApplyを受け付けない。別プロパティだけを編集してsourceが暗黙に差し替わることを防ぐ。
- Frame range input は signed 64-bit として検証し、`FramePosition` の `int64_t` assignment を通して保存する。`int` constructor による暗黙 narrowing を避ける。
- 作成・変更・複製・削除・全削除を `setTimelineTransitions()` ベースの in-memory snapshot Undo command に接続。新規公開signal/slot/event配線は追加していない。
- Wipe/Slide は追加／種類変更メニューから除外。Dissolve を追加し、編集UI文言は ja/en/zh/zh-TW を用意。
- 確認済み（2026-11-23）: Artifact app／Stack targetのbuild、および関連CTest 2/2（Core 20/20、Stack 10/10）。hidden UI MCPでdialog表示・stack図・Apply・名前変更後の反映を確認し、transition追加のCtrl+Z/Ctrl+Shift+Zによるmarker除去／復元を確認した。未確認: 編集値変更そのもののUndo/Redo、Undo履歴の永続化、共同編集へのsnapshot operation dispatch。編集UIの操作完了後に値は反映されたが、編集値の履歴復元はまだ確認していない。
- 既存CTestログ確認（2026-11-22 11:41）: `ArtifactCoreTransitionRenderContractTest` 20/20 と `ArtifactVirtualTransitionStackContractTest` 10/10 Pass。この記録は上記編集ダイアログ／Undo変更より前であり、それらの検証証跡には使わない。
- 2026-11-23 hidden UI実操作: `run_ui_offline.py` の仮想Qt入力でComposition作成、Solid 2枚追加、2 layer選択、Crossfade追加、編集dialogのname変更とApply、Stack一覧での変更名反映まで確認した。GPU Composition captureは成功したが、白一色のsourceなのでpixel blendの目視証拠とはしない。menuを開いたままUndoした後の一覧は古いmenu snapshotだったため、編集値Undo/Redoとプロジェクト保存・再読込は依然未確証。Render Queueの関連pixel/duplicateテストは2/2、Core/Stack CTestは2/2 Pass（20/20と10/10）。
- 2026-11-23 locale matrix: `ja/en/zh/zh-TW`ごとに同じ仮想Qt入力経路で編集dialogを開いてcaptureし、主要field/buttonの表示範囲内配置を確認。`zh`/`zh-TW`ではStack preview見出しの一部文字列が英語で、Source名は試験fixtureの日本語名だった。Pixel parity判定は行わず、`temp/transition-ui-*`のcaptureはGit対象外。

## 9. 参考

- 要件・決定・受入: `../../planned/MILESTONE_VIRTUAL_TRANSITION_STACK_2026-11-20.md`
- 仕様画像と出典: `./README.md`
- 実装規約: `../../../AGENTS.md`、`../../technical/HOT_PATH_RULES.md`
- 監査で参照した主要コード: 本設計 §2 のファイル:行を参照。
