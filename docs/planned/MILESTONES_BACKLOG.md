# Milestones Backlog

**最終更新:** 2026-08-15

### Still Image / Production Readiness
- **M-IMG-1** Still Image Layer Production Readiness
  - 静止画の import、色解釈、transform、crop、mask、blend、effect、保存／再読込、preview／Render Queue の実制作経路を一つの受入条件で閉じる
  - OIIO、Asset System、Static Layer GPU Cache 等の既存計画を再実装せず、`ArtifactImageLayer` の end-to-end 完成責任を持つ
  - 詳細: `docs/planned/MILESTONE_STILL_IMAGE_LAYER_PRODUCTION_READINESS_2026-08-08.md`

## Next Walk TODO: Still Image and Production Workflow

- **WALK-IMG-1** 部分実装（2026-08-10 project path／Asset ID の relocation 復旧を修正）
  - project root、relative path、Asset ID、absolute fallback、relink の優先順位を AssetManager／project 保存境界で統一する
  - 静止画・連番画像・動画で共通利用できる relocation 契約を先に定義する
  - `setCurrentProjectPath()` と `setCurrentProjectRootPath()` の trim／absolute 化と root 同期、project item の `filePathRelative`／`sequencePathsRelative` 併記、composition layer の `*.sourcePathRelative`／`*.sequencePathsRelative` 併記、source registry の `pathRelative` 併記と既存絶対 path fallback は実装済み。Asset ID は registry 復元時に相対候補を優先して再登録する。
- **WALK-IMG-2** 部分実装（2026-08-10 静止画 crop の Preview／Render Queue 整合を修正）
  - source、decode、color、CPU buffer、GPU upload、cache、save/reload の状態を一画面で確認できる診断導線を追加する
  - 既存の Project Health / App Debugger の責務と重複しない範囲で設計する
  - `ArtifactImageLayer::toQImage()` の cached source crop を Render Queue／thumbnail 側にも適用し、fallback の二重 crop を避ける修正は完了。既存の `FallbackDiagnosticsPanel` を App Debugger の `Fallbacks` tab へ接続し、履歴確認の導線を追加した。さらに event message を Message 列へ表示し、fallback 理由まで確認できるようにした。Selected Image Source resource に source／decode buffer／version／color interpretation を載せ、App Debugger の次アクションにも反映した。GPU texture cache の pending upload count／bytes、layer／asset owner 単位の entry／pending 状態、共有 source key の binding 状態も診断するようにした。surface-cache と direct composite が同じ画像で切り替わるため、共有 source key 未接続は欠落と断定せず `candidate-missing` と表示する。
  - Composition Controller と共有 Composition View Drawing の current-frame-buffer 早期描画、3D card buffer 取得は crop 有効時に共有せず、`toQImage()`／通常経路へ戻して Preview／Render Queue 側の crop 抜けを防いだ。
  - image surface-cache key に source crop の全状態を含め、crop 編集後に古い合成面を再利用しないようにした。
  - matte source の surface-cache key に type／blend／fit／opacity／invert も含め、matte 設定だけを変更した場合の古い合成面再利用を防いだ。
  - LayerMask の add／set／remove／clear に revision を持たせ、surface-cache key に反映して stale mask surface を防いだ。
  - decode worker からの `FallbackTracker::record()` と Diagnostics panel の履歴読み取りが競合しないよう、tracker の履歴・policy・warning 設定を内部 mutex で保護する修正を実装済み。
  - 非同期 readback も `RGBA16_FLOAT`／`RGBA32_FLOAT` を source format のまま staging し、linear float を sRGB へ変換してから QImage 化するよう同期 readback と整合させた。F32 composition pipeline での非同期 readback の形式取り違えを防ぐ。
  - `LayerMatteReference::opacity` を CPU の matte factor と GPU track-matte の opacity に反映し、保存値だけが表示結果へ反映されない状態を修正した。複数 matte の blendMode／stackMode 統合は別途検証対象として残す。
  - `LayerMatteReference::fitMode` の Stretch／Fit／Fill／Original を共通の source 整形へ実装し、CPU matte と GPU matte source の両方で常に Stretch される状態を修正した。同一 source layer を異なる fitMode で参照できるよう、GPU cache には未整形 source を保持して適用時に fit する。
  - CPU matte evaluator が欠落 source を飛ばした際に後続 source と参照設定がずれる edge case を修正し、構築成功した source と active reference を同じ配列で評価するようにした。
  - CPU surface matte の Add／Subtract／Intersect／Difference を各 `LayerMatteReference::blendMode` から評価するようにした。GPU track-matte の複数 source／per-source blend は shader 入力構造が別のため、引き続き未統合として扱う。
  - GPU track-matte の RenderPipeline に 3 枚の source texture を追加し、3 source 以内・同一 blend mode／opacity の Add／Intersect／Subtract は既存 shader の一括 stack pass で処理するようにした。Difference、異なる opacity／blend の組み合わせは既存経路へフォールバックする。
  - Render Queue の共有 `drawLayerForCompositionView` に current-frame matte source map を渡し、matte を含む layer では未適用の surface／static cache を再利用しないようにした。これにより Render Queue GPU の matte helper が未接続だった経路を接続した。
  - Software Render Queue でも layer surface に既存の rasterizer effect／mask 処理を通し、current-frame matte source map と matte evaluator を適用するようにした。GPU／Software で mask／matte が丸ごと抜ける経路を修正した。
  - Software／shared GPU surface の matte 適用を `fitMode`／`opacity`／per-reference `blendMode` 対応の共通 evaluator に切り替え、GPU／Software で設定値を捨てて Core 既定 Add stack に落とす差を縮小した。
  - matte source が解決済みの layer では GPU texture cache も無効化し、surface cache bypass 中に空の cache handle を経由する余計な fallback 分岐を除いた。
- **WALK-IMG-3** ✅ Asset Browser の検索範囲切替（2026-08-13 実装）
  - Current Folder / Project Assets / Missing / Unused を切り替えられる検索導線を追加する
  - 現在フォルダ検索と Project View のプロジェクト検索の責務を整理する
  - Current Folder / Project Assets / Missing / Unused を Asset Browser 内の検索スコープとして追加し、設定を保存・復元する。Project Assets は asset root 以下を再帰検索する。
- **WALK-IMG-4** 部分実装（2026-08-15 比較ハーネス追加）
  - transform、crop、mask、matte、blend、effect、色解釈について、Preview / Software Preview / Render Queue の結果を比較できる受入導線を作る
  - `STILL_IMAGE_LAYER_ACCEPTANCE_MATRIX` の未実行項目を Pass / Fail / N/A へ進める
  - `ArtifactSoftwareRenderTestWidget` に Preview／Render Queue の保存フレーム読み込み（`P` / `Q`）と RGBA 画素差比較（`D`）を追加した。同一サイズを確認し、差分画素数・平均 RGBA 差・最大差を表示する。
  - 実素材・同一 project/frame/output transform での受入実行と、Software Preview を含む三経路の判定は未実施。
- **WALK-IMG-5** 部分実装（2026-08-15 現行経路を再確認・共通 finalize 経路を実装）
  - Composition Menu と Project View の設定編集を同じ設定適用、Undo、dirty 更新経路へ揃える
  - 新規イベント配線を増やさず既存サービスを再利用する
  - 両経路の解像度変更は `ChangeCompositionResolutionCommand` を維持し、設定確定後の dirty 通知と current composition の playback range／FPS 同期を `ArtifactProjectService::finalizeCompositionSettingsChange()` に統合した。設定フォーム、解像度 remap 判定、各種 composition property の入力処理は引き続き各 UI 側に残る。
- **WALK-IMG-6** 部分実装（2026-08-15 現行 UI／queue 経路を再確認・追加導線を整理）
  - Full / Work Area / Current Frame / Selected Layers と出力プリセットを整理し、現行の複数追加アクションを選びやすくする
  - 既存 command ID、shortcut、queue behavior との互換性を確認する
  - 現行 UI には Composition／Work Area／Custom／Selected Frames／Current Frame、Full／ROI／Custom Crop、All／Selected／Solo／Visible／Custom Layers、Composition／Half／Third／Quarter／Custom Resolution、および Preset 導線が存在する。Current Frame は内部 mode `4` を維持したまま表示名と accessible description を明示した。Composition Menu の6つの追加 action は「レンダーキューに追加」submenu に整理し、既存 action／shortcut／handler を維持した。command／queue behavior の runtime 受入確認は残る。

## Next Walk TODO: Timeline and Property Editing

- **WALK-TL-1** ✅ Timeline keyframe marker cache の再構築条件整理（2026-08-10 静的確認）
  - `ArtifactTimelineTrackPainterView` の keyframe marker / connection segment 収集を、paintEvent ごとではなく composition、track、selection、visible range の変更時だけ再構築する
  - playhead 移動だけでは marker geometry を再計算しない
- **WALK-TL-2** ✅ Expression AST の評価キャッシュ（2026-08-10 静的確認）
  - `AbstractProperty::evaluateValue` の式文字列 parse を毎回行わず、既存の `ScriptContext::getOrParseAST` または同等のキャッシュを利用する
  - 式変更時の invalidation と評価コンテキストの境界を確認する
- **WALK-TL-3** ✅ Timeline 検索用 property 名のキャッシュ（2026-08-10 実装）
  - `ArtifactLayerPanelWidget` の検索時にレイヤーごとの `getLayerPropertyGroups()` を毎回再構築しない
  - property group 構造が変わった時だけ検索用文字列を更新する
- **WALK-TL-4** 部分実装（2026-08-15 波形生成のレイアウト同期ブロックを遅延化）
  - `CachedAudioWaveform` が composition／layer／source signature 単位でフル長ピークを保持し、trim／zoom で再利用する経路は実装済み
  - `buildAudioWaveformForLayer()` のキャッシュミス時は `updateLayout()` 内で生成せず、次の UI event loop tick に一度だけ遅延実行し、完了後に track を再構築する。別スレッドから layer を読む unsafe な暫定実装は避けているため、decode／生成そのものの worker 化と runtime 負荷確認は未完了
- **WALK-TL-5** 部分実装（2026-08-15 現行 PropertyEditor を再確認・Reset Undo を補強）
  - 数値行の slider、reset/default、keyframe、expression affordance を `PropertyEditor` row の共通機能として揃える
  - 旧 Knob 系を再拡張せず、現行 `Artifact.Widgets.PropertyEditor` を正規経路にする
  - 数値 row の slider／spinbox、Slider before value の表示設定、default 値の reset、keyframe の追加／削除／anchor／color label、expression ボタンと Copilot 導線を確認した。Reset は `SetLayerPropertyValueCommand` と既存 `SetLayerPropertyKeyframesCommand` を `MacroUndoCommand` にまとめ、値とキーフレームを1回の Undo で戻すようにした。残課題は複数選択・通常編集の全経路 Undo・runtime 受入と、全 property 種別での表示整合確認。
- **WALK-TL-6** ✅ Project open/save の非ブロッキング化（2026-08-13 静的確認）
  - 既存の async load/save 経路を File Menu、recent project、recovery、auto-save から統一利用する
  - 進行表示、編集中の変更との整合性、temp→rename の atomic 保存を確認する
  - File Menu の通常保存／別名保存／最近のプロジェクト再オープンは `loadFromFileAsync()` / `saveToFileAsync()` を使用済み。進行表示と atomic 保存の runtime 確認は未実施。

## Next Walk TODO: Composition, Render Queue, and Diagnostics

- **WALK-CE-1** ✅ Project Health 診断から対象箇所へジャンプ（2026-08-10 実装）
  - 診断行に保持されている composition ID、layer ID、asset path を使い、ダブルクリックまたは Inspect 操作で Project View / Composition / Asset Browser の対象へ移動する
  - `Double-click to inspect` の案内と実際の操作を一致させる
- **WALK-CE-2** ✅ Frame Debug の next action を動的化（2026-08-10 Overview / State / Frame に静的実装）
  - `goal / now / warning / next` の `next` を固定文言にせず、stale resource、cache miss、invalid texture、mask fallback などの最初の原因に応じて変える
  - resource inspector や compare / step への既存導線を再利用する
- **WALK-RQ-1** 部分実装（2026-08-15 Render Queue 履歴 metadata／行アクションを実装）
  - 現在のテキスト履歴に job ID、frame range、failure stage、error message、retry action を保持する
  - 履歴の行から Retry Job / Retry Failed Frames / Reveal Output を実行できるようにする
  - `ArtifactRenderQueueManagerWidget` に Retry Job、Retry Detected Failed Frames、Reveal in Explorer、Open File、Copy Path の導線があり、service 側にも `resetJobForRerun()`／`rerenderAllDetectedFailedFrames()`／`requeueCompletedHistoryAt()` がある。履歴への service event は job ID、frame range、failure stage、retry action を metadata として付加し、実行中の履歴行には Retry Job／Reveal Output の context action を追加した。既存履歴の構造化復元と stable job ID の service 公開、runtime 受入は残る。
- **WALK-RQ-2** 部分実装（2026-08-15 renderer readback の UI 非同期化・失敗段階表示を追加）
  - `readbackToImage()` と保存処理を UI 操作に同期接続せず、既存の async readback と完了通知を利用する
  - 出力中の progress、cancel、失敗段階を表示する
  - `ArtifactIRenderer::readbackToImageAsync()` を Composition Editor の通常 Screenshot、Renderer 由来の Advanced Screenshot、Viewport Render Output に接続した。保存中はキャンセル可能な進捗ダイアログを表示し、readback 完了後に UI スレッドで保存・成功／失敗を通知する。readback 失敗は `Stage: readback`、保存失敗は `Stage: encode/write` と表示する。Whole Window／multi-channel は既存同期経路を維持し、実機受入は未完了。
- **WALK-CE-3** 部分実装（2026-08-15 Four-Up 遅延 start の stale callback 防止・計測を追加）
  - レイアウト変更時に未表示ペインまで renderer/controller を同期生成せず、表示されたペインから段階的に初期化する
  - 初回表示時の体感と renderer setup の重複を計測する
  - `ArtifactCompositionEditor` は Four-Up 切替時にアクティブな二次 controller のみへ composition を反映し、`QTimer::singleShot` で各 controller の `start()`／初期 Fit を段階実行する。layout generation を持たせ、切替後に古い deferred callback が hidden pane を起動しないようにした。pane start latency も debug log へ出す。一方、4ペインの view／controller 自体は editor 初期化時に生成されるため、未表示ペインの完全な遅延生成と runtime 計測は未完了。

### Color / Professional Media
- **M-PRO-MEDIA-1** Professional Media Materials Support
  - EXR/HDR、広色域、高ビット深度、log素材のメタデータ保持と明示的な解釈経路
  - Phase 1〜3の統合基盤を実装済み。入力色空間・transfer・HDR/log判定を `RawImage` に保持し、`SourceInterpretOverride` と ImageLayer の明示 working-space 変換へ接続済み。Phase 4〜5 と runtime 検証は継続中
  - 詳細: `docs/planned/MILESTONE_PROFESSIONAL_MEDIA_MATERIALS_2026-07-16.md`


### Layer Component / Simulation
- **M-LC-1** 部分実装（2026-08-15 現行コンポーネント基盤を再確認・dependency validation を強化）
  - `cloner / layout / crowd / physics / fracture / emit` を phase-based に矛盾なく接続する
  - preview fallback と authoritative simulation を分離し、将来の crowd / rigid / soft-body / pyro / bake に耐える土台を作る
  - 詳細: `docs/planned/MILESTONE_LAYER_COMPONENT_PIPELINE_2026-07-01.md`
  - superseded 先の `docs/planned/MILESTONE_LAYER_COMPONENT_SYSTEM_2026-04-18.md` で、descriptor／phase／scope、validation、runtime snapshot、保存・復元、Components 専用編集面を確認済み。`LayerComponentHost::validate()` に空 dependency type と依存循環の検出を追加した。legacy `component.*` と descriptor stack の併存、component 間依存解決、全機能の runtime phase parity、preset／拡張 API／旧 API 互換は未完了・未検証。

### Composition / Workflow
- **M-LCW-1** ✅ MVP 実装済み（2026-08-15 現行コード再確認）
  - 素材・平面・簡単なマスク・入退場 Envelope を一回の確定で作成する新規ダイアログ
  - 透明度とエフェクト強度の同時 / 先行 / 遅延プリセットを提供する
  - 既存の `CreateSolidLayerSettingDialog` は維持し、作成オーケストレーターだけを新設する
  - 詳細: `docs/done/MILESTONE_QUICK_LAYER_CREATION_DIALOG_2026-07-10.md`
  - QuickLayerCreationDialog の素材／平面作成、長方形／楕円マスク、入退場 Envelope、配置モード、自動選択、一括 Undo を確認した。角丸／全体マスク、追加プリセット、runtime／build 受入は継続課題。

- **M-PRECOMP-2** 部分完了（2026-08-15 現行コード再確認、runtime/build 未確認）
  - `PreCompose` の「呼べる」状態から、`unprecompose()` を含む実務 finish line まで閉じる
  - layer restore、time/range integrity、undo/redo、`Master Properties` 前提の責務境界を固める
  - 詳細: `docs/done/MILESTONE_PRECOMPOSE_WORKFLOW_COMPLETION_2026-07-09.md`
  - `ArtifactProjectService` の precompose／unprecompose UndoCommand、source mapping、layer order／transform／time 復元、keepComposition 両経路、nested time 変換まで source-level で確認済み。runtime／build による最終受入のみ保留。

- **M-LC-2** 部分実装（2026-08-15 現行 stack 経路を再確認）
  - `single cloner` 前提から、複数 generator と独立 field stack を持つ構造へ段階移行する
  - `component.cloner.*` 互換を維持しつつ、`generators[] / modifiers[] / fields[]` の内部モデルへ寄せる
  - 詳細: `docs/planned/MILESTONE_GENERATOR_MODIFIER_FIELD_STACK_2026-07-01.md`
  - legacy cloner の互換 descriptor 化、複数 generator の評価、modifier／field stack の保存・再読込と共通影響適用を確認した。generator／modifier 単位の field binding・mask／blend／remap、全 modifier の descriptor 化、複数 generator の merge／weight 契約、runtime parity は未完了・未検証。

- **M-LC-3** 部分実装（2026-08-15 現行 authoring 経路を再確認）
  - 先行実装済みの live radial field を、viewport direct manipulation と field stack 操作まで引き上げる
  - center / radius drag、active field 選択、strength / blend / invert、shape 拡張の順で進める
  - 詳細: `docs/planned/MILESTONE_LIVE_FIELD_AUTHORING_UX_2026-07-04.md`
  - radial／box／linear の viewport handle、active／hover、stack reorder、strength／blend／invert、modifier／clone／text／shape への influence 接続を確認した。noise／solid の composition evaluator／保存復元／作成メニューを追加した。color channel、shape vertex attribute、実機受入は未完了・未検証。

### Core / Type System
- **M-CORE-5** 部分実装（2026-08-15 現行 Core を再確認、全面移行は未完了）
  - `std` / `Qt` の境界を薄くしつつ、用途ごとの自前コレクションへ段階移行する
  - `Array<T>`, `String`, `Ptr<T>`, `Ref<T>`, `Owned<T>` の最小 API と採用順を整理する
  - 詳細: `docs/planned/MILESTONE_CUSTOM_COLLECTIONS_DESIGN_2026-07-04.md`
  - `ArtifactArray`／`ArtifactString`／`ArtifactPtr`／`ArtifactSet`／`ArtifactQueue` 等の基盤型は存在するが、`std::vector`／`std::string`／smart pointer 等の既存利用箇所全体は未移行。実装対象は `ArtifactCore` 子リポジトリ側のため、今回は親リポジトリから変更していない。

- **M-CORE-6** 部分実装（2026-08-15 現行 Core を再確認、repository-wide 移行は未完了）
  - 素のテンプレート露出を避け、`LayerList`, `EffectRegistry`, `PropertyBag` のような自己文書化型へ包む
  - `add()`, `remove()`, `contains()`, `get()` を揃え、暗黙変換を避ける
  - 詳細: `docs/planned/MILESTONE_DOMAIN_TYPE_WRAPPERS_2026-07-04.md`
  - `ArtifactArray`／`ArtifactPtr`／`ArtifactHashMap` と `LayerList`／`EffectStack`／`PropertyBag` の基盤は確認済み。一方で raw `std`／Qt コンテナの既存公開・内部利用が残るため、全体移行は未完了。対象は `ArtifactCore` 子リポジトリ側。

- **M-CORE-7** 方針再整理待ち（2026-08-15 現行コード／開発ルールと照合）
  - `vector`, `mutex`, `string`, `shared_ptr` などの置換優先度と例外を整理する
  - 置換しない標準型も明文化して、混在を減らす
  - 詳細: `docs/planned/MILESTONE_STD_TO_QT_MIGRATION_2026-07-04.md`
  - 現行ルールでは標準型の全面的な Qt 置換は採用せず、Core wrapper と ABI／module／hot path の責務単位で再設計する。親側からの横断置換は未実施。

- **M-CORE-8** Core Keyframe / Property Update Hardening ✅ (static verified 2026-07-12)
  - `AbstractProperty` / `AnimatableValueT` / `Core.KeyFrame` の多重実装を整理し、時刻比較・値検証・評価の堅牢性を上げる
  - まずは現状挙動を固定する回帰テストから入り、`RationalTime` の正規化比較とスレッド安全な評価へ段階移行する
  - 詳細: `docs/done/MILESTONE_CORE_KEYFRAME_ROBUSTNESS_2026-07-10.md`

空いている時間に進めやすいよう、分野別に小さめのマイルストーンへ分割したバックログ。

## Completed Milestones (2026-04-14 verified)

以下は実装確認済みの完了マイルストーン。詳細は各マイルストーン文書を参照。

### Diagnostics / Profiling
- **M-DIAG-1** Audio Engine Profiler ✅ (2026-04-15)
  - `AudioEngineProfiler` lock-free singleton, callback timing, fill-loop timing, buffer level
  - `ProfilerPanelWidget` に "Audio Engine" セクション + Reset ボタン追加 (Ctrl+Shift+D)
  - 主要ファイル: `ArtifactCore/include/Utils/PerformanceProfiler.ixx`, `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`

- **M-DIAG-2** EventBus Debugger ✅ (2026-04-15)
  - `EventBus`: PublishHook, type-name registry, `forEachRegisteredType`, `registerTypeNameRaw`
  - `EventBusDebugger`: attach/detach, fire log (ring buffer), subscriber snapshot, frequency snapshot
  - `EventBusDebuggerWidget`: 3-tab UI — Fire Log / Subscribers / Frequency (Ctrl+Shift+E)
  - 主要ファイル: `ArtifactCore/include/Event/EventBusDebugger.ixx`, `Artifact/src/Widgets/Diagnostics/EventBusDebuggerWidget.cppm`

- **M-DIAG-3** Lightweight Tracer / Frame Timeline
  - crash stack / scope tracer / frame timeline / thread trace を超軽量でまとめる
  - `Render / Decode / UI / Event` を frame ごとに並べる
  - 主要ファイル: `ArtifactCore/include/Diagnostics/*`, `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
  - 詳細: `docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md`
  - 実行メモは親文書へ統合済み

- **M-DIAG-4** Live Frame Pipeline / Resource Watcher / State Diff Tracker
  - Pass DAG / RT・Texture・Buffer lifetime / barrier hazard を常時追う
  - 任意 resource のライブ inspector と pixel inspect を持つ
  - 前フレームとの差分から壊れ始めた瞬間を自動検出する
  - 主要ファイル: `ArtifactCore/include/Render/*`, `Artifact/src/Widgets/Diagnostics/*`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - 詳細: `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md`
  - 実行メモは親文書へ統合済み

### Project View / Asset System
- **M-PV-1** Project View Basic Operations ✅ (verified 2026-04-14)
  - selection center/quick actions/sync chip/inline rename 実装済み
  - 主要ファイル: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
  - マイルストーン文書 `MILESTONE_PROJECT_VIEW_INTERACTION_POLISH` は内容完了につき `docs/done/` へ移動済み (2026-06-23)

- **M-PV-2** Project View Asset Presentation ✅ (verified 2026-04-14)
  - selection summary/detail、HoverThumbnailPopup 実装済み
  - 主要ファイル: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`

- **M-AS-4** Asset System Integration ✅ (verified 2026-04-14)
  - sync chip両方向に配置済み、Asset Browser↔Project View 往復同期
  - 主要ファイル: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`, `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

### UI / Property Editor
- **M-UI-23** Property Widget Row Alignment ✅ (verified 2026-04-14)
  - Phase 1-2完了、row bg/hover/keyframe chromeをowner-draw化
  - 主要ファイル: `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`

- **M-ACC-1** 部分実装（2026-08-15 現行コード再確認、Phase 3/runtime 未完了）
  - 主要 Asset Browser 操作（検索、表示切替、フォルダ移動、種類／状態フィルター）の Accessible Name / Description を静的実装済み（2026-07-30）
  - 左利き向け補助、片手操作補助、視認性補助、障碍者向け補助をまとめて整理する
  - Phase 1: 利き手設定の土台、ヒット領域調整、主要 widget からの参照
  - 詳細: `docs/planned/MILESTONE_ACCESSIBILITY_AND_LEFT_HANDED_UI_2026-06-28.md`
  - 主要ウィジェットの Accessible Name／Description、large target、コンテキストメニュー位置補正、片手入力設定は実装済み。全体 RTL／左右反転、viewport magnifier の実表示、スクリーンリーダー／キーボードの runtime 受入は未完了・未検証。

- **M-UI-27** ✅ 実装済み（2026-08-15 現行コード再確認）
  - `main / accent / background` などのデザイントークンに対して、比較演算子 + 値でルールを GUI 編集できるようにする
  - ルール違反時の warning / block と、選択色を最寄りパレット色へ補正する導線をまとめる
  - `ArtifactColorSciencePanel` か `PropertyEditor` のどちらに寄せるかは責務確認後に確定する
  - `ArtifactColorSciencePanel` に target／operator／value／scope／enforce の編集表、追加／削除、既定 constraint palette、最近傍色 snap を確認済み。詳細: `docs/done/MILESTONE_COLOR_CONSTRAINT_RULES_2026-06-07.md`
  - 詳細: `docs/planned/MILESTONE_COLOR_CONSTRAINT_RULES_2026-06-07.md`

- **M-UI-3** Inspector Usability ✅ (verified 2026-04-14)
  - キーボードショートカット/ステータスバー/レイヤーラベルカラー/整列分布機能
  - 主要ファイル: `Artifact/src/Widgets/ArtifactAlignmentWidget.cppm`, `Artifact/src/Widgets/ArtifactStatusBar.cpp`

- **M-UI-5** Contents Viewer Expansion ✅ (verified 2026-04-14)
  - テキストレイヤーインライン編集実装済み、Ctrl+Enter commit shortcutあり
  - 主要ファイル: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

### Layer Editing
- **M-LE-1** Layer Solo View 編集機能強化（平面・シェイプ） ✅ 全 Phase 完了 (2026-06-26)
  - Phase 1: シェイプ固有ビューポートハンドル（角丸・星内半径） ✅ 実装済み
  - Phase 2: グラデーションフィル（シェイプ・平面レイヤー + プロパティピッカー） ✅ 実装済み (2026-06-26)
  - Phase 3: ストロークスタイル（破線・端点・接合・配置） ✅ 完了 (2026-06-23)
  - Phase 4: ギズモ XYWH 数値 HUD オーバーレイ ✅ 完了 (2026-06-26)
  - Phase 5: シェイプ頂点ベジェカーブ編集 ✅ 完了 (2026-06-26)
  - 詳細: `docs/done/MILESTONE_LAYER_EDIT_2026-04-25.md`
  - 主要ファイル: `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`, `Artifact/src/Layer/ArtifactShapeLayer.cppm`, `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`, `Artifact/src/Widgets/Render/TransformGizmo.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`

- **M-LE-2** Layer Transform からの Crop / Pan 導線と Source Reframe 透明化 ✅ (2026-06-24 完了)
  - `Layer Transform` 直下に `Add Crop / Pan` を出し、既存の `Source Reframe` を再利用する
  - `SourceCrop` の crop 外側を透明として扱い、元サイズのレイヤー寸法を維持する
  - 詳細: `docs/done/MILESTONE_LAYER_SOURCE_REFRAME_NLE_2026-06-24.md`
  - 主要ファイル: `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`, `Artifact/src/Layer/ArtifactImageLayer.cppm`, `Artifact/src/Layer/ArtifactVideoLayer.cppm`, `Artifact/src/Layer/ArtifactSourceCrop.cppm`

- **M-LE-3** ✅ 実装済み（2026-08-15 現行コード再確認、runtime/build 未確認）
  - `ArtifactShapeLayer` を primitive layer から `editable path + modifier stack` を持つ 2D モデリング対象へ昇格させる
  - `Shape Edit` mode、vertex/segment/tangent editing、`Convert To Editable Path`、shape operator の modifier UX を段階導入する
  - 詳細: `docs/done/MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md`
  - custom path の頂点／segment／tangent 編集、選択・hover 表示、Undo、primitive から editable path への導線、shape operator stack の実装を確認済み。runtime/build 受入のみ未確認。

### Composition Editor / Cache
- **M-CE-1** Composition Editor Cache System ✅ (verified 2026-04-14)
  - Surface cache / render key suppression / ROIシステム実装済み
  - 主要ファイル: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- **M-CE-2** ✅ 実装済み（Phase 1〜5 source/static verified 2026-08-15、runtime性能確認待ち）
  - `ArtifactCompositionViewDrawing` の layer/source/effect/mask/resolution cache signature と source revision を GPU cache key に反映済み
  - `GPUTextureCacheManager` の F32/QImage 再利用、予算・最大エントリ数・LRU eviction、owner/key/device invalidate、hit/miss/upload/eviction 診断を実装済み
  - Image Sequence は static cache 対象外としてフレーム単位の描画経路を維持
  - 残りは runtime 性能確認
  - 主要ファイル: `Artifact/docs/MILESTONE_STATIC_LAYER_GPU_CACHE_2026-03-26.md`

- **M-RAM-3** Preview Range Policy and Priority ✅ (verified 2026-06-26)
  - `ramPreviewPriorityReason()` / `ramPreviewPriorityState()` / `orderedRamPreviewFramesForRange()` in `ArtifactPlaybackService.cppm`
  - immediate / near / directional / safety-backfill / work-area / out-of-range の priority reason 実装済み
  - 再生方向バイアス、一時停止中 warmup、work area 判定まで完了

- **M-CE-CRIT-1** Critical Render / Media Stability Program ✅ (verified 2026-06-26)
  - マイルストーン文書 `docs/done/` へ移動済み

### Debug / Regression Surface
- **M-CE-CRIT-2** Debug Render Harness ✅ 既存 regression surface
  - particle-only / video-only / blend-only / overlay-only / mixed-media の最小再現 surface は実装済み
  - `AppMain` から独立 dock として開け、`AppDebuggerWidget` からも同じ frame snapshot vocabulary を読める
  - 既存の regression surface として `M-CE-CRIT-1` の診断・回帰確認に使う
  - 詳細: `docs/planned/MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md`

### M-DIAG-6 Harness Engineering / Goal-First Loop（部分実装、2026-08-15 更新）
- `Debug Render Harness` と `App Debugger` の report vocabulary を goal-first に揃える
- `goal / expected / actual / next action` を共通の作業単位にする
- 詳細: `docs/planned/MILESTONE_HARNESS_ENGINEERING_2026-05-12.md`
- 実行メモは親文書へ統合済み
- Debug Render Harness の report Summary に `goal / expected / actual / nextAction` と `ok / failed / pending / degraded` を追加済み。App Debugger との完全テンプレート統合、runtime smoke checklist は未完了。
- App Debugger の Capture Details にも `goal / expected / actual / nextAction` を追加し、Harness と同じ text-first 語彙で比較状況を表示する。status taxonomy の完全統合、runtime smoke checklist は未完了。

### Workstream Containers

以下は個別機能ではなく、複数 milestone を束ねる進行管理の器。  
active milestone の重複名としては扱わない。

- `M-OPS-1` Active Implementation Triad
- `M-OPS-2` Continuation Sprint
- 詳細: `docs/planned/MILESTONE_ACTIVE_IMPLEMENTATION_TRIAD_2026-05-12.md`
- 詳細: `docs/planned/MILESTONE_CONTINUATION_SPRINT_2026-05-20.md`

### Render Execution / Isolation
- **M-RE-1** External Renderer Design ✅ Phase 1 closeout
  - 内蔵レンダラは維持しつつ、オフラインレンダリングだけ別プロセスへ切り出す
  - job snapshot / CLI / progress / diagnostics の設計を先に固める
  - 詳細: `docs/planned/MILESTONE_EXTERNAL_RENDERER_DESIGN_2026-04-22.md`
  - Phase 1 completion: `docs/done/MILESTONE_EXTERNAL_RENDERER_DESIGN_PHASE1_2026-06-25.md`

### Creative Effects / Exploratory Backlog
- **M-FX-FORM-1** ✅ 実装済み（2026-08-15 現行コード再確認、runtime/build 未確認）
  - Trapcode Form 風の grid / point-cloud particle layer を、既存 `ParticleSystem` / `ClonerGenerator` とは独立した generator layer として設計する
  - 既存資産との統合は renderer reuse に留め、`ParticleRenderData` / `ArtifactIRenderer::drawParticles()` だけを共有する
  - 詳細: `docs/planned/MILESTONE_FORM_GRID_PARTICLE_LAYER_2026-06-26.md`
  - `ArtifactFormParticleLayer`、Grid2D／Grid3D／LayerMap、deterministic noise／twist／falloff、base／animated cache、ParticleRenderData 変換、作成 UI／Inspector／保存復元を確認済み。runtime/build 受入のみ未確認。

- **M-FX-EXP-1** New Image Effect Exploration
  - 既存の `blur / glow / chromatic aberration` から少し離れた、Artifact らしい画像エフェクト案を保管する
  - まずは look-dev 用の発想メモとして扱い、制作体験に効くものから実装候補へ昇格させる
  - `Temporal Fossil`
  - `Pigment Separation`
  - `Surface Memory`
  - `Depth Melt`
  - `Edge Echo`
  - `Light Pressure`
  - `LuminescenceCaustics`
  - `QuantumGlitch / WavefunctionCollapse`
  - `DynamicFluidVortex`
  - `ReactionDiffusionStylizer`
  - `VectorFlowGlitch`
  - `AnisotropicFlowBlur`
  - `ReactionDiffusionBlur`
  - `ApertureShapeBlur`
  - `Glow Variants Pack`
  - `Chromatic Relief`
  - `Signal Collapse`
  - `Ink Delay`
  - `Atmospheric Slicing`
  - 実装候補メモ:
    - `Temporal Fossil`: 過去フレームの輪郭や色を薄く堆積させる。モーションブラーではなく時間の層を見せる方向
    - `Pigment Separation`: RGB 分離ではなく、顔料やインクのにじみとして色がほどける方向
    - `Surface Memory`: 素材表面に前の像の痕跡が焼き付く。キャンバス、金属、ガラスなど質感差を活かしやすい
    - `LuminescenceCaustics`: 輪郭やハイライトから集光の網目を生成する。液体金属、氷、クリスタル、魔法オーラ向け
    - `QuantumGlitch / WavefunctionCollapse`: タイルと隣接ルールで画像を再構成する。破壊ではなく自己構成の抽象コラージュ向け
    - `DynamicFluidVortex`: 流体速度場で画像を移流させる。インク、水流、渦、粘性のある歪み向け
    - `ReactionDiffusionStylizer`: 反応拡散で有機的パターンを生成する。キリン柄、シマウマ、指紋、サンゴ向け
    - `VectorFlowGlitch`: 輪郭や流れに沿って引き裂く。構造テンソルや動き場に追従する知的グリッチ向け
    - `AnisotropicFlowBlur`: 構造テンソルで方向場を取り、流れに沿ってだけぼかす。髪、木目、水流、美肌向け
    - `ReactionDiffusionBlur`: 拡散しながら模様が育つ。溶けるトランジションや細胞分裂風のブラー向け
    - `ApertureShapeBlur`: 任意マスクを PSF にするレンズボケ。ハート型玉ボケや汚れたレンズ向け
    - `Glow Variants Pack`: 輪郭発光、色収差、残光、液体感などを分けた発光亜種群
  - Core library 候補:
    - `Temporal Fossil` は frame history / accumulation 基盤を持てるなら `ArtifactCore` 側に置く価値が高い
    - `Pigment Separation` は CPU reference と GPU backend の両方を作りやすく、creative effect pack に馴染みやすい
    - `Surface Memory` は texture/history/mask を跨ぐので、effect host contract が固まってから `ArtifactCore` 候補として再評価する
    - `LuminescenceCaustics` は `Final Effect` または shared bus 寄りの投影表現に昇格しやすいので、最初は stylized rasterizer として検証する
    - `QuantumGlitch / WavefunctionCollapse` は `Mosaic` / `AutoMosaic` / tile 系資産と相性が良いので、まずは rasterizer として評価する
    - `DynamicFluidVortex` は `FluidSolver2D` / `FluidVisualizer` / `FluidForce` と相性が良いので、まずは fluid solver 連携の effect として評価する
    - `ReactionDiffusionStylizer` は `FluidSolver2D` の低解像度格子思想を流用しやすいので、まずは stylized rasterizer として評価する
    - `VectorFlowGlitch` は `StructureTensor` / `Distortion` / `ChromaSpread` と相性が良いので、まずは edge-aware rasterizer として評価する
    - `AnisotropicFlowBlur` は `StructureTensor` の方向場をそのまま使えるので、まずは edge-preserving blur として評価する
    - `ReactionDiffusionBlur` は `FluidSolver2D` / `AnisotropicFlowBlur` / `Distortion` と相性が良いので、まずは transition-oriented blur として評価する
    - `ApertureShapeBlur` は FFT 系の基盤と aperture UI と相性が良いので、まずは PSF-driven blur として評価する
    - `Glow Variants Pack` は `Glow` / `DirectionalGlow` / `ChromaSpreadGlow` / `Halation` と相性が良いので、まずは glow family として評価する
  - 関連:
    - `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md` の `C-GFX-1 Creative Effect Base`
    - `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md` の `C-GFX-2 Creative Effect Pack`

### Export / Review
- **M-FE-2** Export / Review / Share ✅ Phase 1完了 (2026-06-23)
  - Copy Path追加、Reveal/Open/Historyは既存、マイルストーン文書は `docs/done/` へ移動済み

### AI / Tooling
- **M-AI-1** MCP/Tool Bridge ✅ Phase 1完了 (verified 2026-04-14)
  - McpBridge::handleRequest() / AIContext 実装済み
  - 主要ファイル: `ArtifactCore/include/AI/McpBridge.ixx`

- **M-AI-2** AI Command Sandbox ✅ (verified 2026-04-14)
  - CommandSandbox.ixx（674行）で policy/execution/timeout すべて実装済み
  - 主要ファイル: `ArtifactCore/include/AI/CommandSandbox.ixx`

- **M-CMD-1** 部分実装（2026-08-15 現行コード再確認）
  - AI / MCP / DSL / Python から low-level API を直叩きさせず、Command IR を正規の automation 入口にする
  - Primitive Command と Macro Command の二層を前提に、validation / transaction / Undo 単位を固定する
  - 詳細: `docs/planned/MILESTONE_COMMAND_IR_AUTOMATION_FOUNDATION_2026-06-28.md`
  - `CommandIR` の request/result/vocabulary、validation、App 側 `CommandIRExecutor`、WorkspaceAutomation 経由の execute facade、Undo 接続を確認済み。keyframe command の mutation 前 payload validation を追加した。全 command の all-or-nothing rollback、独立 Resolver、preview/explain、UI／automation 経路の完全統一、runtime 検証は未完了。

### Asset Browser
- **M-AB** Asset Browser Improvement (Unity風) ✅ (verified 2026-04-14)
  - Icon/List切替実装済み（viewModeButton）、Name/Date/Size/Typeソート、Status filter
  - 主要ファイル: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

- **M-AB-SEQ-2** ✅ 実装完了（2026-08-15 現行コード再確認、runtime検証待ち）
  - sequenceの展開表示、欠番／読込失敗のsequence単位診断、preview/import/relink導線、bounded cache、時刻依存フレーム切替を実装済み
  - 残りは実素材での保存／再読込、欠番、キャッシュ、実機性能の検証
  - 詳細: `docs/planned/MILESTONE_IMAGE_SEQUENCE_WORKFLOW_COMPLETION_2026-07-27.md`

## Widget Ownership Guardrails

このバックログで milestone を触るときは、まず次の責務境界を確認する。

- `ArtifactContentsViewer`: 内容閲覧 / compare / recent sources / mode routing
- `ArtifactAssetBrowser`: ファイル探索 / サムネイル / favorites / project bridge
- `ArtifactCompositionEditor`: composition 編集 / viewport 操作 / playback
- `ArtifactTimelineWidget`: タイムライン全体の orchestration
- `ArtifactLayerPanelWidget`: タイムライン左ペインの行操作
- `ArtifactPropertyWidget` / `PropertyEditor`: property row の編集
- `ArtifactInspectorWidget`: summary / selection / effect stack の窓口

境界が曖昧な場合は、`docs/WIDGET_MAP.md` を先に更新してから milestone を触る。

## Application

### M-ARCH-1 Host / Context / ROI / Property Core（部分実装、2026-08-15 更新）
- render context / property registry / effect host contract / ROI partial evaluation を段階導入する
- まずは無挙動変更で入れやすい read-only registry / adapter を優先する
- 詳細は `docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`
- AE 1.0 向けの必須/重要/後回し仕分けと 6 か月順は `docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md`
- Month 1 の実行順は `docs/planned/MILESTONE_AE1_0_MONTH1_EXECUTION_2026-04-20.md`
- 実行メモは親文書へ統合済み
- `RenderContextRegistry`／purpose snapshot、`RenderROI`／effect ROI hint、`globalPropertyRegistry`／read-only adapter、`EffectHostContext`／legacy adapter の基盤を確認済み。全 property owner の単一 registry 化、全 effect 共通 capability／dependency 契約、tiled runtime、plugin adapter の統一受入は未完了。

### M-WKR-1 Background Utility Worker Process（部分実装、2026-08-15 更新）
- サムネイル / waveform / proxy / メタデータ抽出 / preflight / autosave / log collection などの雑用を、専用 worker process に段階分離する
- まずは共通 job contract と in-process runtime を作り、その後 protocol と外部プロセスへ進める
- 詳細は `docs/planned/MILESTONE_BACKGROUND_UTILITY_WORKER_PROCESS_2026-04-22.md`
- Phase 1 の実装表は同文書内の `実装表 A` を参照
- Phase 2-5 は `job contract -> scheduler -> facade -> protocol -> dedicated worker process` の順で進める
- `AsyncAssetReadScheduler` と個別の thumbnail／waveform／sequence 非同期経路は存在するが、汎用 `UtilityJobRequest/Result/Progress`、共通 task registry／dedupe／retry、全対象 facade、専用 utility process は未実装。新規 module／CMake 登録を伴うため、今回はコード変更していない。

### M-CORE-4 Module Hygiene / Build Stabilization
- module boundary / Qt type / STL numeric helper / API compatibility をまとめて安定化する
- いま出ている `SessionLedger` / `Property` / `LayerMatte` / `ArtifactRenderROI` / `Acoustic` 系の compile break を代表例として扱う
- 詳細は `docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_2026-04-21.md`
- 実行メモは親文書へ統合済み

### M-APP-1 Application Cross-Cutting Improvement（部分実装、2026-08-15 更新）
- menu / toolbar / shortcut / view / diagnostics / workflow を横断で揃える
- central widget の横幅不足と下部パネルの高さ不足を layout issue として追跡
- 詳細は `docs/planned/MILESTONE_APP_CROSS_CUTTING_IMPROVEMENT_2026-03-27.md`
- 横断設定、Project Health／Validation／Session Ledger、主要 menu／toolbar／shortcut、workflow bridge、Frame Debug／Profiler／Fallback／Render Queue／Playback 診断は実装済みまたは進行中。全 action の command owner、selection／current／active state の正本統一、status／console／inline feedback 語彙、runtime 受入は未完了。

### M-APP-2 Deferred UI Initialization / Lazy Load（部分実装、2026-08-15 更新）
- icon / thumbnail / viewer / dock の eager load を減らして初回体感を軽くする
- 詳細は `docs/planned/MILESTONE_DEFERRED_UI_INITIALIZATION_2026-03-27.md`
- `ArtifactMainWindow` の placeholder／factory／first-show materialization、lazy dock の計測、Asset Browser の非同期 thumbnail warmup、Property／Viewer の初回遅延を確認済み。全体 icon cache、Playback／Render backend の全面遅延、hidden refresh 抑制、lazy 後の全 panel 再同期、first paint 性能受入は未完了。

### M-APP-3 Frame Debug View / Simple RenderDoc-like
- 1 フレームを固定して pass / resource / attachment / compare / step を追える内蔵フレームデバッグビューを作る
- 詳細は `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`
- **部分完了（2026-08-15 現行コード再確認）**: `FrameDebugSnapshot`／capture／bundle のデータ契約、App Debugger／Frame Debug View／Pipeline／Resource Inspector の表示面、pass・resource・attachment・compare 情報、履歴と JSON serialization を確認。全 render path の capture 接続、前後フレームの実比較・scrub・step の一体 workflow、bundle export／runtime 検証は未完了。

### M-APP-3a Frame Debug Goal-First Summary
- `FrameDebugViewWidget` の上部サマリを `goal / frame / warning / next` に固定する
- harness report と同じ語彙でフレーム単位の判断を読めるようにする
- **部分完了（2026-08-15 現行コード再確認）**: FrameDebugView の上部要約を `goal / frame / warning / next` に統一し、Harness の `goal / expected / actual / nextAction` report と共通の判断先行構造を確認。compare／pin 操作の統一、saved bundle の再参照、runtime 表示確認は未完了。
- 詳細: `docs/planned/MILESTONE_FRAME_DEBUG_GOAL_FIRST_SUMMARY_2026-05-12.md`
- 実行メモは親文書へ統合済み

### M-APP-4 App Debugger Visual Hierarchy / Color Semantics
- App Debugger の情報階層、色の意味、異常時の見え方を整えて、人間が読みやすい diagnostics surface に寄せる
- 詳細は `docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`
- 実行メモは親文書へ統合済み
- Phase 5-12 は first-glance / focus / report / legend / quick actions / auto focus / session history / render cost の派生 slice として進める
- **部分完了（2026-08-15 実装）**: Overview に `neutral / info / warning / error / success` の semantic legend を追加。既存の QPalette による異常時強調、summary/detail 階層と合わせて読解導線を補強した。全診断面の色 token 統一と runtime 視認性確認は未完了。

### M-APP-4a App Debugger Goal-First Summary
- `AppDebuggerWidget` の上部サマリを `goal / now / warning / next` で固定する
- harness report と同じ語彙で作業面を読めるようにする
- **部分完了（2026-08-15 実装）**: Overview／State summary に `GOAL / NOW / WARNING / NEXT` の順序を追加し、失敗フレーム時は goal を `inspect the failed frame` に切り替える。copy／pin／compare／filter の横断配置と Harness との完全な語彙統一、runtime 表示確認は未完了。
- 詳細: `docs/planned/MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md`
- 実行メモは親文書へ統合済み

### M-APP-5 Project Health / Problem View Wiring
- `ArtifactProjectHealthChecker` と `ArtifactProblemViewWidget` / `ArtifactProjectHealthDashboard` の語彙を揃える
- `DiagnosticEngine` と smoke gate の failure vocabulary を合わせる
- 詳細は `docs/MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md`
- 実行メモは親文書へ統合済み
- **部分完了（2026-08-15 実装）**: Project Health Service の summary を Problem View／Dashboard と同じ `goal / now / warning / next` 文法へ統一。HealthChecker→ProjectDiagnostic 変換、DiagnosticEngine の検証経路、Problem View の一覧・severity 表示は確認済み。smoke gate の完全な failure vocabulary 統一、runtime 更新確認は未完了。

### M-APP-6 App Surface Cohesion
- Project / Asset / Timeline / Composition / Contents Viewer / Inspector / Debugger の current / recent / selection / status を揃える
- empty state と summary strip の文法をアプリ全体で統一する
- 詳細は `docs/planned/MILESTONE_APP_SURFACE_COHESION_2026-05-13.md`

### M-APP-7 App Diagnostic Cohesion
- Project Health / Problem View / App Debugger / Frame Debug View / harness report の diagnostics 文法を揃える
- warning / error / next action を surface 横断で統一する
- 派生 slice は同じ文法で読む
- **部分完了（2026-08-15 実装）**: App Debugger の Diagnostics summary を raw counter 列から `goal / now / warning / next` 形式へ変更し、Project Health／Problem View／Frame Debug の判断先行表示と揃えた。Harness の `goal / expected / actual / nextAction` report、色・copy／compare／pin 導線、全 surface の runtime 表示統一は未完了。
- 詳細は `docs/planned/MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`
- 実行メモは親文書へ統合済み

### M-IR-8 ImmediateContext Boundary / De-direct
- `DiligentEngine` の `ImmediateContext` / `IDeviceContext` を layer / widget / controller から直接触らない構造へ寄せる
- `ArtifactIRenderer` / `RenderCommandBuffer` / `DiligentImmediateSubmitter` を正式な描画境界として固定する
- 詳細は `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md`
- **部分完了（2026-08-15 現行コード再監査）**: 主流の 2D／3D 描画は packet／submitter／renderer façade 経由へ集約済み。`CompositionRenderController` の viewport／post-process／readback、render queue、effect/pass、layer に `IDeviceContext`／`immediateContext()` の直接依存が残るため、context access narrowing と全上位入口の統合は未完了。D3D12／Vulkan の runtime 検証なし。

### M-IR-9 Render Boundary Safety Gate
- 境界変更を壊れにくい順序で進めるための安全ゲート
- いったん置いておく対象と再開順を固定する
- 詳細は `docs/planned/MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md`
- **部分完了（2026-08-15 現行コード再確認）**: Trace／FrameDebug／render snapshot／fallback の観測点、危険な変更を避ける safety rules、façade→controller→particle→access narrowing の再開順を確認済み。低レベル依存の縮小、backend／render-target 復帰、snapshot 並列の runtime 受入れは pending。

### M-DIAG-5 Startup Thread Churn / Worker Burst Trace
- 起動直後 / 初回コンポ表示時の worker thread burst を trace で可視化する
- `sharedBackgroundThreadPool()`、video/image/svg prefetch、render scheduler、playback worker の寄与を切り分ける
- 詳細は `docs/planned/MILESTONE_STARTUP_THREAD_CHURN_TRACE_2026-04-21.md`
- **部分実装（2026-08-15）**: `ArtifactRenderScheduler` の worker task に `startup/render-scheduler/*` の `TraceDomain::Render` scope と thread id を追加。render lane の burst 相関が Trace Timeline で可能になった。Decode／Asset／Playback／Project／AI lane の統一タグ、専用 startup hotspot 表示、pool 統合と runtime 計測は未完了。

## AI / Tooling

### M-AI-0 AI Tooling Expansion
- AI の読み取り、提案、安全な write tool、自動化を一本化するマスター方針
- まずは `AIContext` / description catalog / inspection tool を強化し、その後に safe write tool と creative assist を広げる
- creative assist は `keyframe suggestion` と `color grading suggestion` を先行させる
- 詳細は `docs/planned/MILESTONE_AI_TOOLING_EXPANSION_2026-04-21.md`
- 推奨順は `read -> safe write -> keyframe suggestion -> color grading suggestion -> automation`
- **部分実装（2026-08-15）**: 既存の `workspaceSnapshot`／composition／selection／render queue read surface に、`get_project_overview`、`get_active_composition`、`get_selected_layers`、`get_render_queue_summary` の read-only inspection 名を追加。DescriptionRegistry と WorkspaceAutomation から共通利用できる。提案／safe write／creative assist／runtime検証は未完了。

### M-AI-2 Safe Write Tools
- AI の提案を確認付きで編集へ反映する安全な write surface
- `ArtifactProjectService` / `ArtifactEffectService` / render queue service を再利用する
- **部分完了（2026-08-15 現行コード再確認）**: CommandIR の dry-run／execution plan／risk／undo 契約、WorkspaceAutomation の主要 destructive gate／confirmation、SafeWriteAuditLog の in-memory・JSON 保存経路を確認。全 write 操作の同一 plan 化、UI／AI payload 整合、runtime 運用検証は未完了。
- 詳細は `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_2026-04-21.md`
- 実装メモは親文書へ統合済み

### M-AI-6 Workflow Automation
- `WorkspaceAutomation` を中心に project / composition / selection / render queue を束ねる
- 詳細は `docs/planned/MILESTONE_AI_WORKFLOW_AUTOMATION_2026-04-21.md`
- 実装メモは親文書へ統合済み
- **部分完了（2026-08-15 現行コード再確認）**: 統合 workspace snapshot、project／composition／selection／render queue の read/write schema、bulk keyframe／property、project item rename／move、variation export、SafeWrite／Undo 経路を確認。batch import／relink／render、queue pause／resume／再実行、共通 execution plan／rollback、UI／runtime 検証は未完了。
- **部分完了（2026-08-15 実装）**: `WorkspaceAutomation.batchRelinkFootageByPath()` を追加し、既存の個別 relink service を再利用した batch relink の結果 schema（requested／succeeded／failed／failures）を提供。batch import／render の共通 execution plan／rollback、長時間 queue 運用、UI／runtime検証は未完了。

### M-AI-2 AI Command Sandbox / CLI Execution
- AI 縺ｫ縺ｯ shell string 縺ｧ縺ｪ縺上↑繧峨〒縺・、program + argv 繧帝攝縺励※謇薙∴繧・
- allowlist / timeout / working directory / output cap 繧定ｨ倬鹸縺励※縲∝ｧ・ｭｷ螟夜Κ繧ｳ繝槭Φ繝峨ｒ縺ｾ縺・☆繧・
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_AI_COMMAND_SANDBOX_2026-04-10.md`
- **部分完了（2026-08-15 現行コード再確認）**: CommandSandbox の非 shell argv 実行、program／environment allowlist、working directory、timeout、output cap、dry-run、構造化結果、AI schema を確認。Preferences での policy 永続化、workspace root enforcement、Cloud／MCP の別 QProcess 経路との policy 統合、runtime検証は未完了。
- **実装着手判定（2026-08-15）**: 残課題は `ArtifactCore/include/AI/CommandSandbox.ixx` の policy／root 境界が中心で、親側の安全な代替実装は確認できないため、子リポジトリ変更なしで保留。次段階は policy 永続化と workspace root enforcement の設計確定。

### M-AI-1 MCP / Tool Bridge Foundation
- `DescriptionRegistry` / `AIToolExecutor` / `AIContext` を使って AI tool schema を安定化する
- local / cloud / 将来の MCP bridge から共通で使える tool 境界を切る
- 詳細は `docs/planned/MILESTONE_AI_MCP_TOOL_BRIDGE_2026-04-10.md`
- 実行メモは親文書へ統合済み
- **部分完了（2026-08-15 現行コード再確認）**: registry 由来 schema、ToolBridge の parse／validation／trace、McpBridge／McpTransport の initialize／tools/list／tools/call／ping／stdio frame、Cloud UI preview／approval、`--mcp-server` 自己 host、AIClient tool loop を確認。外部 MCP 互換性、全 provider の runtime 契約、CommandSandbox allowlist 統合、会話履歴への完全再注入は未完了。

### M-AI-2 Cloud UI Compact View / Settings Split
- Cloud AI の詳細設定を dialog 側へ寄せ、常時表示を減らす
- `ArtifactAICloudWidget` を compact view と advanced panel に分ける
- 詳細は `Artifact/docs/MILESTONE_AI_CLOUD_UI_2026-04-09.md`
- **部分完了（2026-08-15 現行コード再確認）**: `ArtifactAICloudWidget` の compact header／provider・model・prompt 面と、`More` 配下の approval／Tools／MCP／Transport advanced panel を確認。設定 persistence／provider migration、CommandSandbox policy 統合、外部接続の runtime 検証は未完了。
- **部分完了（2026-08-15 実装）**: provider 別の model selection を `QSettings` の `AICloud/model/<provider>` に保存・復元する経路を追加。compact／advanced の責務分離は維持。provider migration、API key 安全境界、外部接続 runtime 検証は未完了。

### M-AI-3 AI Assisted Keyframe Generation ⭐ **新規追加**
- 軌跡解析と自動キーフレーム生成でアニメーション作成を支援
- `AIKeyframeGenerator` で動きのパターンを学習し、スムーズなキーフレーム提案を返す
- `EasingLabWidget` とタイムライン keyframe 表示を使って比較・適用できるようにする
- **機能:** 軌跡データからのキーフレーム提案、タイムライン統合、既存 undo path での適用
- **見積:** 45-60h
- **詳細:** `docs/planned/MILESTONE_AI_KEYFRAME_SUGGESTION_2026-04-21.md`
- **部分完了（2026-08-15 現行コード再確認）**: `KeyframePatternGenerator` の trajectory 再サンプリング、Timeline の既存 keyframe／Undo 適用、KeyPatternDialog／EasingLab の preview 基盤を確認。AI suggestion 専用 contract、選択レイヤーからの自動抽出、候補比較 lane、品質 feedback、runtime 検証は未完了。

### M-AI-4 AI Color Grading Suggestion ⭐ **新規追加**
- シーン分析と自動カラーグレーディング提案で色調整を支援
- `AIColorAnalyzer` / `ColorGradingSuggester` で画像を解析し、LUTやパラメータの候補を提案
- `ArtifactColorSciencePanel` と `ArtifactColorGradingEngine` を提案経路に載せる
- **機能:** 画像分析からの色調整提案、LUT/preset 統合、既存 grading 経路での適用
- **見積:** 60-75h
- **詳細:** `docs/planned/MILESTONE_AI_COLOR_GRADING_SUGGESTION_2026-04-21.md`
- **部分完了（2026-08-15 実装）**: `ArtifactColorGradingEngine` に read-only の `AIColorAnalysisResult`／`analyzeSamples()` を追加し、単一フレーム相当の有限サンプルから平均輝度・輝度レンジ・彩度・色温度偏り・暗部／ハイライト比率を収集できるようにした。既存の `suggestGrading()`／`applySuggestion()` と接続する Phase 1 の契約を確保。AI解析コンテキスト、候補比較UI、preview／Undo、runtime検証は未完了。

### M-AI-5 AI Basic Assistant ⭐ **新規追加**
- 基本的なAIアシスタント機能で質問応答とプロジェクト情報提供
- `AIBasicAssistant` でクエリに応答し、MCP経由で外部AIと連携
- **機能:** 質問応答、ドキュメント/コード検索、UI統合
- **見積:** 35-50h
- **詳細:** `docs/planned/MILESTONE_AI_BASIC_ASSISTANT_2026-04-11.md`
- **統合済み（2026-08-15 現行コード整理）**: 旧 `AIBasicAssistant` 計画は `docs/planned/MILESTONE_LOCAL_AI_CHAT_2026-04-01.md` に統合。AIChatWidget／AIClient／LlamaLocalAgent／MCP／WorkspaceAutomation の静的実装は確認済み。実モデル受入れ、モデル配布、検索契約、runtime検証は統合先で継続。

## Feature Expansion Support

### Priority Execution Trio
- `M-FE-9 Motion Tracking Workflow`
- `M-AS-4b Vector / SVG Layer Import`
- `M-RD-5 Animated Image Export`
- 詳細は `docs/planned/MILESTONE_FEATURE_EXPANSION_FOCUS_TRIO_2026-03-28.md`

### M-FE-1 Onboarding / Empty States ✅ (2026-07-30)
- ✅ WelcomeWidget: empty project の案内画面（最近開いたプロジェクト一覧、New / Import / Open ボタン）
- ✅ Playhead Phase 1: `currentFrame_` を単一権威に統一、全 UI への fan-out を `setCurrentFrameForAll()` に集約
- ✅ empty selection: Render Layer Widget / surface info が `No layer selected` を表示
- ✅ empty asset: Asset Browser が未選択フォルダ・空フォルダ・検索結果なしを状態別に案内
- ✅ empty timeline: Layer Panel が composition 未選択時とレイヤー未作成時を状態別に案内
- 残件: 初回導線の実機表示確認と翻訳・アクセシビリティ確認
- マイルストーン文書は `docs/done/` へ移動済み

### M-FE-2 Export / Review / Share ✅ (2026-06-23)
- Phase 1 Result Surface 完了: Copy Path追加、Reveal/Open/Historyは既存
- マイルストーン文書は `docs/done/` へ移動済み

### M-FE-3 Automation Helpers
- command palette / batch / preset / macro entry を増やす
- ✅ レイヤーエディタ用コマンドパレット (Ctrl+F)
- 詳細は `docs/planned/MILESTONE_AUTOMATION_HELPERS_2026-03-27.md`
- **部分完了（2026-08-15 現行コード再確認）**: Command Palette の Recipe／pin／usage・recent 永続化、MRU JSON 復元の ID 正規化、WorkspaceAutomation／CommandIR の batch rename・move・relink・render queue surface、preset 経路を確認。外部 macro／script hook、失敗時 rollback、全 action の再起動後 Recipe、runtime 受入れは未完了。

### M-FE-4 Workspace / Layout / Session
- workspace 保存 / 読み込み、dock layout preset、window state 復元
- 詳細は `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
- **部分完了（2026-08-15 現行コード再確認）**: `ArtifactWorkspaceManager` の session／preset JSON、dock layout／window state 保存・復元、構造不備 JSON の default-layout fallback、Main Window／AppMain の shutdown 保存経路を確認。複数 workspace の切替、破損 session の復旧UI、runtime受入れは未完了。

### M-FE-5 Templates / Presets / Starter Kits
- project / composition / layer / effect の preset と starter project
- 詳細は `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
- **部分完了（2026-08-15 現行コード再確認）**: `ArtifactPresetManager`、effect／render／FX preset catalog、workspace layout preset の保存・復元を確認。project／composition／layer／effect を束ねる starter kit 契約、適用UI、runtime受入れは未完了。

### M-FE-6 Batch / Macro / Script Entry
- batch rename / relink / export、macro、script hook
- 詳細は `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
- **部分完了（2026-08-15 現行コード再確認）**: `ArtifactInteractiveShell` の `--script`／`source`、top-level／nested の canonical path による再帰 source 検出、`ArtifactPythonHookManager`、Command Palette Recipe、`ArtifactBatchRenderer` を確認。外部 script sandbox／権限、macro rollback、長時間実行状態、runtime受入れは未完了。
- **AE差別化:** インクリメンタルサーチ、メタデータ（解像度/fps/デュレーション）でフィルタ可能
- 詳細は `docs/planned/MILESTONE_SEARCH_COLLECTIONS_SMART_ORGANIZATION_2026-03-28.md`

### M-UI-21 Asset Browser Navigator / Search / Presentation Surface
- Asset Browser を Unity 風のナビゲータとして整理し、search / breadcrumb / favorites / grid-list / thumbnail slider / workflow bridge を段階導入する
- 既存の search / thumbnail / unused / DnD を土台にして、探索と presentation を揃える
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_NAVIGATOR_SEARCH_PRESENTATION_2026-04-03.md`
- 実行メモは親文書へ統合済み
- **部分完了（2026-08-15 現行コード再確認）**: navigator／breadcrumb／history／Favorites、incremental flat search、bounded search history completer、grid／list／thumbnail／status、folder context、DnD／unused／workflow bridge、view/filter/current-directory の保存・復元を確認。検索履歴切替、D&D preview ghost、runtime performance／UX受入れは未完了。

### M-TL-10 Timeline Feature Implementation / Interaction Surface
- Timeline の layer / clip / keyframe / search / visual language / owner-draw を一つの実行計画として束ねる
- 既存の timeline milestone を置き換えず、順序と責務をまとめる
- 詳細は `docs/planned/MILESTONE_TIMELINE_FEATURE_IMPLEMENTATION_2026-04-03.md`
- 実行メモは親文書へ統合済み
- **部分完了（2026-08-15 現行コード再確認）**: current／selected／playhead同期、layer state、keyframe add／remove／paste／navigation、ripple edit、常設検索／Hit status、owner-draw painter の marker／selection／drag／interpolation／roving を確認。Inspector／Undo完全一致、旧scene縮退、密集marker規則、大量layer runtime受入れは未完了。

### M-TL-11 Timeline Right Pane Full Owner-Draw
- `ArtifactTimelineWidget` の右ペインを `ArtifactTimelineTrackPainterView` 正規経路へ固定し、`TimelineTrackView / TimelineScene / ClipItem` を退役させる
- clip / keyframe / playhead / selection / input の責務を painter 側へ寄せ、右ペインを完全 owner-draw surface にする
- 詳細は `docs/planned/MILESTONE_TIMELINE_RIGHT_PANE_FULL_OWNER_DRAW_2026-04-08.md`
- **部分完了（2026-08-15 現行コード再確認）**: 右ペインは painter を直接配置する正規経路で、clip／marker／selection／playhead／seek／drag／resize／scroll／zoom を painter 側が担当。旧 scene／ClipItem 実装は削除済み。資料整理、旧 shortcut／同期回帰、大量レイヤー性能の runtime 受入れは未完了。

### M-TL-12 DAW-Style Input Surface
- timeline / inspector を DAW 風に、real-time input と step input の 2 系統で扱えるようにする
- playback 中の live capture と停止中の 1-frame step entry を同じ property / keyframe model に書き込む
- 詳細は `docs/planned/MILESTONE_DAW_STYLE_INPUT_SURFACE_2026-04-08.md`
- 進捗: Core 側の `InputSurfaceManager` と `InputSurfaceStateChangedEvent` を実装済み
- **部分完了（2026-08-15 現行コード再確認）**: `InputSurfaceManager` の Off／RealTime／StepEntry／LiveCapture、armed／pending／quantize、capture開始・確定・キャンセルを確認。Timeline／Inspector／Transport の共通表示、property／keyframe end-to-end書き込み、recording UI、Undo／Redo、runtime受入れは未完了。

### M-TL-13 Timeline Scrub Bar Frame Cache Overlay
- `ArtifactTimelineScrubBar` 上に AE 風の cache range 可視化を追加し、frame cache / RAM preview の有効範囲を緑の帯で見せる
- 現在フレームの赤い進捗表示と共存させ、playback / scrub / seek の状態を読み取りやすくする
- 詳細は `docs/planned/MILESTONE_TIMELINE_SCRUBBAR_FRAME_CACHE_OVERLAY_2026-04-10.md`
- timeline index では補助線扱い。単独で追うより `docs/planned/MILESTONE_TIMELINE_INDEX_2026-04-22.md` を先に読む。
- 状態: Cache Range Contract / Overlay Rendering / empty-state diagnostics の静的実装済み。runtime 回帰・大量フレーム負荷・overlay style 設定化は未完了
- **部分完了（2026-08-15 現行コード再確認）**: ready／failed／on-disk frame state と分断された cache range を ScrubBar に渡し、RAM緑・disk青・failed赤と current frame を同一 surface に描画。空状態／Accessible Description／keyboard seek も確認。warm／stale回帰、style設定化、大量フレーム性能の runtime 受入れは未完了。

### M-FE-9 Motion Tracking Workflow
- tracker editor / overlay / stabilize / bake を制作導線としてまとめる
- 詳細は `docs/planned/MILESTONE_MOTION_TRACKING_SYSTEM_2026-03-25.md`
- Phase 1 実装順は `docs/planned/MILESTONE_MOTION_TRACKING_PHASE1_EXECUTION_2026-03-28.md`
- **部分完了（2026-08-15 現行コード再確認）**: Core の MotionTracker／保存復元／複数 tracking mode／homography／camera solve／非同期進捗・キャンセル、Artifact の tracker tool／overlay／stabilizer を確認。tracker editor、layer／mask／cameraへの統一 bake、長時間処理のUI／Undo／runtime受入れは未完了。

### M-FE-10 Animation Dynamics Core
- Physics2D とは別に、animation 用の spring / damping / follow-through を Core に置く
- 詳細は `docs/planned/MILESTONE_ANIMATION_DYNAMICS_CORE_2026-03-28.md`
- **部分完了（2026-08-15 現行コード再確認）**: `Animation.Dynamics` の 1D／2D／3D solver、Spring／LagFollow、preset、overshoot clamp、velocity accumulator、seek reset と、主要 2D transform の Position／Rotation／Scale 接続、Property／JSON永続化を確認。全チャンネル adapter、専用UI、自動テスト、runtime回帰は未完了。

### M-FE-11 Virtual Pointer Core
- モーショングラフィック向けの仮想マウス演出を Core の再生可能データとして定義する
- 詳細は `docs/planned/MILESTONE_VIRTUAL_POINTER_CORE_2026-03-28.md`
- **部分完了（2026-08-15 現行コード再確認）**: `VirtualPointerTrack`／frame／event／style、JSON保存復元、recordFrame、stateAtFrame／stateAtTime、positionAtFrame／eventFrames、composition／layer／property binding を確認。実 playback service、録画入力、layer mutation／keyframe変換、編集UI、runtime受入れは未完了。

## UI / UX

### M-UI-1 Timeline Finish
- playhead、不感帯、余白、行揃え、ホイール、ドラッグ挙動の最終整理
- **部分完了（Update 2026-08-15）**: 現行コードで playhead の hit radius／ドラッグ、中央ボタン pan、Shift 横スクロール、Ctrl ズーム抑制、トラック／ヘッダーの seek、row painter の mouse drag／wheel、Timeline／ScrubBar 間の offset 同期を確認。playhead overlay の hit radius を入力経路間で共通定数化した。残りは不感帯・余白・行揃えの実機回帰、入力経路の組み合わせ検証、テーマ別の視認性確認。

### M-UI-2 Dock / Tab Polish
- アクティブタブ装飾
- スプリッター幅
- 空パネルや初期レイアウトの見直し
- **部分完了（Update 2026-08-15）**: `ArtifactMainWindow` の ADS dock 状態保存／復元、既定 dock layout、splitter 幅 API、`ArtifactNativeDockSurface` の左右／中央 tab surface、Timeline／Debugger 等の splitter stretch・handle 幅を確認。NativeDockSurface の tab palette に theme token の accent／surface／text を設定し、アクティブタブの意味付けを追加。Lazy dock の共通 placeholder に panel 名と初期化案内を追加。タブの非伸長・長名省略・スクロールも共通設定した。残りは初期レイアウトの実機回帰と密度調整。

### M-UI-11 UI Theme System / Studio Skin
- `QSS` に責務を寄せすぎず、背景 / surface / accent / selection を意味ベースで統一する
- `Maya / Blender / Modo / DaVinci` 系の制作 UI を参考にしつつ、Artifact 独自の studio skin を作る
- 詳細は `docs/planned/MILESTONE_UI_THEME_MIGRATION.md`（旧ロールアウト文書は統合済み）
- **部分完了（Update 2026-08-15）**: `currentDCCTheme()` の theme token、`ArtifactCommonStyle`、QPalette／owner-draw による Property／Inspector／Dock の統一、QSS の現行ソース 1 箇所（QADS 組み込み stylesheet クリア）まで確認。`ApplicationSettingDialog::saveSettings()` から `applyDCCTheme()` を呼び出す runtime 切替入口も追加。残りは Dark／Light／High-contrast の全 surface 再描画、hover／selection／disabled 回帰、Property row の keyframe／expression 表示、QADS 例外の実機確認。

### M-UI-14 QSS Reduction / Style Ownership
- `QSS` を主責務から外し、theme tokens / palette / 共通 widget / owner-draw へ移す
- 詳細は `docs/planned/MILESTONE_QSS_REDUCTION_2026-03-31.md`
- **部分完了（Update 2026-08-15）**: 統合先 `MILESTONE_UI_THEME_MIGRATION.md` の静的監査と一致。production source の新規 QSS 追加はなく、残存呼び出しは QADS の組み込み stylesheet クリアのみ（旧 `.bak`／`whole_file.txt` は除外）。実行時テーマ切替入口は追加済みだが、全 widget の状態表示回帰は未確認。

### M-UI-15 Inline Interaction Surfaces
- property / viewport / timeline / layer panel の inline 展開を共通化する
- color picker / gradient editor / scrub input / expression / waveform / blend mode をその場で扱えるようにする
- 詳細は `docs/planned/MILESTONE_INLINE_INTERACTION_SURFACES_2026-03-31.md`
- **部分完了（Update 2026-08-15）**: text layer の inline edit、viewport overlay／gizmo、Property Editor row／knob、layer name／parent／blend mode、Timeline scrub／keyframe、Expression Copilot を確認。Layer Panel の layer name／parent／blend mode inline editor に Accessible Name／Description を追加。共通 container の focus／commit／cancel 契約、property-row の color／gradient／pick-whip、viewport scrub、timeline waveform／expression、mask preview の横断統一は未完了または未検証。

### M-UI-16 UI EventBus Adoption
- UI 層の広域更新を `ArtifactCore::EventBus` に寄せ、Project / Timeline / Inspector / Render Queue / Asset Browser の fan-out を抑える
- Qt signal は高頻度入力と局所 UI に限定し、state change だけ bus 化する
- 詳細は `docs/planned/MILESTONE_UI_EVENT_BUS_ADOPTION_2026-04-01.md`
- **部分完了（Update 2026-08-15）**: Project Manager／Inspector／MainWindow／Asset Browser／Timeline／Playback Control／Property／Diagnostics 等の typed subscribe／publish と subscription lifetime、EventBus debugger 統計を確認。Timeline の queued dispatch、selection refresh coalesce、CompositionChanged の 1 tick coalesce を追加確認。Render Queue／Composition Editor 全体の更新契約、widget 単位 debounce policy、Qt signal fan-out の棚卸し、recovery 通知の統一は未完了または未検証。

### M-UI-17 Console Widget Enhancement
- `ArtifactDebugConsoleWidget` をログ診断のハブとして強化する
- search / filter / export / stats / event log integration / theme ownership をまとめる
- 詳細は `docs/planned/MILESTONE_CONSOLE_WIDGET_ENHANCEMENT_2026-03-31.md`
- **部分完了（Update 2026-08-15）**: theme／palette、regex・time・context・category filter、severity toggle、filter preset 保存復元、visible log の copy／save、FrameDebug snapshot、診断 dock 導線を確認。`updateStatus()` の表示件数／総数／severity 内訳／paused queue 統計も確認。統計 dashboard、JSON／CSV／XML export、百万件 virtualized list、command interface、runtime 大量ログ性能は未完了または未検証。

### M-RQ-1 Render Queue GPU Backend Selection / Fallback
- Render Queue から GPU backend を選べるようにし、CPU backend と fallback を並行運用できる状態にする
- backend contract / GPU encode path / UI diagnostics を段階導入する
- 詳細は `docs/planned/MILESTONE_RENDER_QUEUE_2026-03-22.md`（旧 GPU backend 文書は統合済み）
- **部分完了（Update 2026-08-15）**: job の auto／cpu／gpu backend、Render Queue UI の選択・表示、GPU 初期化／readback、multi-channel 制約、FFmpeg の native GPU／pipe-hw／pipe-vulkan／native／pipe fallback、失敗理由ログ、JSON 保存復元を確認。実機 capability probe、GPU→CPU fallback、zero-copy frame bridge、GPU 使用率、品質／速度は未検証。

### M-RD-13 Multi-Frame Rendering (MFR) for Render Queue
- Render Queue の export を複数フレーム並列で進められるようにし、直列 render の待ち時間を埋める
- まずは export-only で導入し、live preview は対象外にする
- 詳細は `docs/planned/MILESTONE_MULTI_FRAME_RENDERING_2026-04-09.md`
- **部分完了（Update 2026-08-15）**: Render Queue の MFR 分岐／`maxInFlightFrames_`、複数 worker、retry／backoff、frame progress、cancel／continue-on-error、FrameCache の count／memory 制限・eviction・prefetch cancel を確認。immutable snapshot の全経路適用、GPU を含む frame-safe scheduler、実測 memory upper bound、明示的 opt-in／直列 fallback UI、cancel／resume 後の出力整合は未検証。

### M-APP-4 Session Ledger / Recovery Workspace
- project / render job / failed task / recovery point を一つの作業台帳にまとめる
- crash 後復帰、長時間 render、未保存作業の回収導線を統合する
- 詳細は `docs/planned/MILESTONE_SESSION_LEDGER_RECOVERY_WORKSPACE_2026-04-09.md`
- **部分完了（Update 2026-08-15）**: `ArtifactCore::SessionLedger`、Render Queue からの session ledger 接続、project／render／failed task／crash の記録入口、AutoSave recovery point、revision ledger、queue checkpoint を確認。Recovery Workspace UI、台帳の永続化／再起動復元、recent／failed／recoverable 切替、相互参照導線は未完了または未検証。

### M-UI-18 Property Widget Update / Cleanup / Theme Ownership
- `ArtifactPropertyWidget` / `PropertyEditor` / `Inspector` の責務を整理し、property UI の見た目と構造を揃える
- `QSS` 依存を減らし、theme / palette / widget ownership を property pane に反映する
- **部分完了（Update 2026-08-15）**: section／search／row chrome の palette 化、theme token surface、keyframe／expression／auto-key 導線、rebuild debounce／revision／frame cache、component editor との責務境界を確認。`PropertyEditor` の `ClipboardManager` 経由の property value copy／paste と、Object Reference editor の expression reference MIME による数値 layer reference の drag-and-drop を追加確認。legacy Knob、非数値 reference／実画面 theme 回帰は未完了または未検証。
- 詳細は `docs/planned/MILESTONE_PROPERTY_WIDGET_UPDATE_CLEANUP_THEME_2026-04-02.md`

### M-UI-19 QSS Exorcism / Property Theme Ownership
- property / inspector / dock / queue 周辺の `QSS` を段階的に追放し、theme token と owner-draw に寄せる
- `M-UI-14` と `M-UI-18` をつなぐ実行 milestone
- **部分完了（Update 2026-08-15）**: Property／Inspector／Queue の主要 path は palette／theme token／owner-draw 化し、現行 `Artifact/src` の `setStyleSheet()` は QADS 組み込み stylesheet クリア 1 箇所のみ。`DockStyleManager` の palette change 再適用も追加した。全 surface の実行時切替、残存 token の共通化は未完了または未検証。
- 詳細は `docs/planned/MILESTONE_UI_THEME_MIGRATION.md`（旧文書は統合済み）

### M-UI-23 Property Widget Row Alignment / Inspector Layout
- `ArtifactPropertyWidget` の行揃え、keyframe / reset / badge / value column の位置を揃え、インスペクタらしい整列レイアウトへ段階移行する
- `PropertyEditor` row widget に layout 責務を寄せ、見た目の整いを構造の統一へつなげる
- **部分完了（Update 2026-08-15）**: row の label／editor／aux action 共通配置、固定寸法、keyframe／navigation／reset／expression action order、owner-draw chrome、`alignPropertyRowLabels()`、value-column 切替、section builder、refresh 抑制を確認。section／channel／transform／effect の label 整列幅も共通定数化し、`ArtifactPropertyEditor` の concrete row label 幅を共通値 132px に統一した。pick-whip／reference link、Inspector header 完全統一、全 editor 種別の runtime baseline、ad-hoc row 完全撤去は未完了または未検証。
- 詳細は `docs/planned/MILESTONE_PROPERTY_WIDGET_ROW_ALIGNMENT_INSPECTOR_LAYOUT_2026-04-03.md`
- 実行メモは親文書へ統合済み

### M-UI-24 Visual Density Monitor
- 画面の詰まり具合を density / heatmap / warning で読む診断 surface
- 完了記録は `docs/done/MILESTONE_VISUAL_DENSITY_MONITOR_2026-06-03.md`
- 実行メモは `docs/done/MILESTONE_VISUAL_DENSITY_MONITOR_PHASE1_EXECUTION_2026-06-03.md` で、canonical completion は done 側

### M-APP-5 Render Preflight / Output Safety Check
- render queue / export dialog / debugger / problem view に出力前検査を共通文法で流す
- 完了記録は `docs/done/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`
- **完了確認（Update 2026-08-15）**: Queue Service の preflight／error block、Output dialog の summary／details、Problem View／AppDebugger の warning／error 語彙を現行コードと完了記録で再確認。runtime 全組み合わせは別途未検証だが、マイルストーンの静的完了条件は満たしている。

### M-CLIP-1 Keyframe Copy & Paste
- timeline から keyframe を copy / paste できるようにする
- 完了記録は `docs/done/MILESTONE_KEYFRAME_COPY_PASTE_2026-06-16.md`
- **実装確認（Update 2026-08-15）**: Timeline widget／Track Painter の選択キーフレーム copy、playhead基準 paste、cut、`ClipboardManager` のJSON記録、複数property／layerの参照復元、既存keyframe merge、選択更新、Undo snapshot、Animation menu／shortcut／context menu接続を確認。異なるcomposition間の全型変換、実ファイルruntime受入、外部clipboard互換は未完了または未検証。

### M-UI-22 QSS Decommission / CommonStyle Path ✅ 完了
- 完了: `docs/done/MILESTONE_UI_THEME_SYSTEM_2026-03-30.md`

### M-SC-2 Shortcut Context Map / Blender-Like Keymap Routing
- `InputOperator` の context 解決順と widget / region 単位の分割を固定し、Blender 風の「場所とモードで意味が変わる」ショートカット routing を明文化する
- `ArtifactCompositionRenderWidget` / `ArtifactTimelineWidget` / `ArtifactLayerPanelWidget` / `ArtifactAssetBrowser` / `ArtifactInspectorWidget` を先行対象にする
- 詳細は `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md`
- 実行メモは親文書へ統合済み
- **部分完了（Update 2026-08-15）**: `InputOperator` の Modal > Widget.Mode > Widget > Workspace > Global 解決、widget keymap 登録、Timeline／Playback の専用 map、context 別 help／JSON import-export／reset、Timeline の操作 binding を確認。`KeyMap::addBinding()` に同一キー競合の拒否と同一 action の旧 binding 撤去を追加した。region 単位登録、競合のユーザー向け revert UI、Blender／Default／Custom preset の完全運用は未完了または未検証。

### Composition Editor Suggested Order
- `M-UI-7 Composition Editor Mask / Roto Editing`
- `M-UI-15 Inline Interaction Surfaces`
- `M-UI-6 Composition Motion Path Overlay`
- `M-FE-7 Review / Compare / Annotation`
- `M-TL-4 Timeline TrackView Owner-Draw Migration`
- `M-TL-8 Timeline QGraphicsScene Elimination`
- CompositionEditor の内部同期は signal 直結ではなく deferred event を正規経路にする
- 順序の目安:
  1. `M-UI-15 Inline Interaction Surfaces`
  2. `M-UI-6 Composition Motion Path Overlay`
  3. `M-UI-7 Composition Editor Mask / Roto Editing`
  4. `M-UI-5 Contents Viewer Expansion` の inline edit 連携
  5. `M-TL-4` / `M-TL-8` の painter 化が終わったら viewport/overlay 連携を深める
  6. `M-FE-7` で review / annotation の脇道を足す
- `M-UI-7` の内部では、geometry editing と mask parameter の time-addressable 化を分ける
- time-addressable 化の first slice は `docs/planned/MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md` に寄せる
- Phase 1 実行メモは親文書へ統合済み

### M-UI-12 Composition Notes / Scratchpad
- コンポジション / レイヤー / フレームに紐づく軽量メモを残せるようにする
- review / annotation より前段の、制作中の書きなぐりメモを扱う
- 詳細は `docs/done/MILESTONE_COMPOSITION_NOTES_SCRATCHPAD_2026-03-30.md`
- **部分完了確認（Update 2026-08-15）**: Composition note の編集・EventBus 同期・project snapshot 永続化、Inspector／Markdown Note Editor の導線、WorkspaceAutomation の get／set composition note を確認。Layer note、frame／marker note、note 検索、jump 導線、runtime 再起動確認は未完了または未検証。

### M-UI-3 Inspector Usability
- effect / property の見つけやすさ
- 空状態の整理
- 選択同期とラベル整理
- **AE差別化:** 全プロパティ一覧表示（P/S/R/T/Aショートカット不要）、ネストグループのフラット化、複数レイヤー一括編集、Blender風数値入力（スクロール変更）、数値スクラブ（ドラッグ変更＋Ctrl/Shift精度調整）、インラインキーフレーム操作（プロパティ横ミニタイムライン）、Expressionエディタ強化（シンタックスハイライト/補完/視覚的エラー表示）
- ✅ キーボードショートカット追加 (Home/End/Ctrl+A/Ctrl+D)
- ✅ ステータスバーコンポジション情報表示
- ✅ レイヤーラベルカラー機能
- ✅ レイヤー整列・分布機能
- **完了確認（Update 2026-08-15）**: 完了記録と現行 `ArtifactInspectorWidget` を再確認。effect／property 検索、空状態、選択／focus 同期、component／effect property filter、複数選択 effect rack、keyframe／expression 導線、palette／accessibility を確認。残る高度な数値 scrub 等は後続 row／inline milestone の責務として分離済み。

### M-UI-8 Animation Dynamics UI Surface
- Physics2D とは別に、animation 用の spring / damping / follow-through を Inspector / Layer Panel から触れるようにする
- 詳細は `docs/planned/MILESTONE_ANIMATION_DYNAMICS_UI_2026-03-28.md`
- **部分完了（Update 2026-08-15）**: Layer Property の Motion Dynamics group（enabled／mode／stiffness／damping／mass／lag tau／overshoot clamp／limit）、JSON 永続化、`Animation.Dynamics` transform 評価接続を確認。Layer Panel の context menu に Smooth／Bouncy／Heavy／Floaty／Rigid preset 適用と Reset を追加した。Timeline／Layer Panel badge、copy、Undo 付き適用、runtime 検証は未完了または未確認。

### M-UI-6 Composition Motion Path Overlay ✅ (verified 2026-06-26)
- 全 Phase 実装済み: path+dot+frame rect overlay, click-to-seek, hover, cache, toggle (Ctrl+Alt+M), interpolation色分け, Shift+click追加/Alt+click削除+Undo
- 主要コード: `ArtifactCompositionRenderController.cppm:9409-9581`、`buildMotionPathSamples()`、`hitTestMotionPathSample()`
- 詳細は `docs/planned/MILESTONE_COMPOSITION_MOTION_PATH_OVERLAY_2026-03-28.md`

### M-UI-7 Composition Editor Mask / Roto Editing ✅ (verified 2026-06-26)
- マイルストーン文書 `docs/done/` に移動済み、各サブ milestone 完了
- 実装内容: `EditMode::Mask` entry → path creation → vertex move/delete → bezier handle edit (Ctrl+drag) → undo/redo (`MaskEditCommand`)
- 主要ファイル: `ArtifactRenderLayerWidgetv2.cppm` (`drawMaskOverlay()`, mouse handlers), `ArtifactCompositionRenderController.cppm` (pending mask, segment insert, handle set)
- 残ポリッシュ項目: segment insert UX未配線、inspector detail △、context menu未対応、`RotoMaskEditor` (Core側 standalone) は未接続

### M-UI-7a Mask Keyframe Foundation ✅ (verified 2026-06-26)
- `mask.opacity`, `mask.feather` など scalar parameters の property exposure 完了
- マイルストーン文書 `docs/done/` へ移動済み
- 詳細: `docs/done/MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md`
- Phase 1 実行メモは親文書へ統合済み

### M-UI-4 Menu-to-App Command Routing
- File / Composition / Edit / View / Layer / Render / Help の menu を app service / command に正しく接続する
- 詳細は `docs/planned/MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md`
- **部分完了（Update 2026-08-15）**: `ArtifactMenuBar` の主要 menu surface と main window／timeline／inspector への action routing、Command Palette の menu action discovery／StandardActionRegistry／MRU 実行を確認。未接続 action の網羅監査、command ID の全経路統一、runtime 操作回帰は未完了または未検証。

### M-UI-4b Toolbar / App Integration
- `ArtifactToolBar` を app command surface として整理し、menu / shortcut / workspace state と揃える
- Qt の新規 signal / slot は増やさず、既存 service / event / 明示 refresh で同期する
- 詳細は `docs/planned/MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md`

### M-UI-4b Toolbar / App Integration ✅ (verified 2026-06-26)
- tool selection group (Select/Hand/Zoom/Move/Rotate等), Zoom In/Out/100%/Fit, Guide toggle, More overflow button
- `ArtifactToolBar.cppm` 全アクション app command surface に配線済み

### M-UI-5 Contents Viewer Expansion
- image / video / audio / 3D model / source / final / compare を横断する viewer の拡充
- audio playback と live waveform preview を同一 surface で確認できるようにする
- ✅ テキストレイヤーインライン編集 (実装済み)
- 詳細は `docs/done/MILESTONE_CONTENTS_VIEWER_EXPANSION_2026-03-27.md`
- 追加の review / compare / annotation 方向は `docs/planned/MILESTONE_REVIEW_COMPARE_ANNOTATION_2026-03-28.md`
- **部分完了確認（Update 2026-08-15）**: image／video／audio／3D model、metadata、Source／Final／Compare、A/B wipe／split／difference、fit／zoom／rotation、channel／Parade／Waveform、recent source、viewer assignment、Asset／Project 接続を現行コードで確認。screenshot／export polish と runtime 検証は未完了または未確認。

### M-UI-20 Contents Viewer DCC Surface Layout / A-B / Wipe
- viewer を 4 段構成の DCC surface として整理し、title / viewer badge / transport / channel-meta を統一する
- recent source dropdown / multi-viewer assignment / wipe compare を 1 つの導線として扱う
- 詳細は `docs/planned/MILESTONE_CONTENTS_VIEWER_DCC_SURFACE_LAYOUT_2026-04-03.md`
- **部分完了確認（Update 2026-08-15）**: 現行コードで title／viewer badge／recent source、transport、channel-meta、viewer assignment、A/B source、swap、Wipe／Split／Difference canvas、wipe 状態の保存・復元を確認。GPU／`ImageF32x4_RGBA` 表示経路への移行、surface の責務分割、runtime 視認性・stale frame・異種 media compare の受入れ確認、screenshot／export polish は未完了または未確認。

### M-UI-9 3D Model Review in Contents Viewer
- OBJ / FBX を Contents Viewer で確認し、model inspection の導線を固める
- 詳細は `docs/planned/MILESTONE_3D_MODEL_REVIEW_IN_CONTENTS_VIEWER_2026-03-28.md`
- **部分完了確認（Update 2026-08-15）**: `Artifact3DModelViewer` の Diligent 表示、OBJ／FBX の読み込み、Solid／Wireframe／Solid + Wire 切替、Reset 3D／Ctrl+0、vertex／polygon／bounds／backend／error status、Contents Viewer の3D metadata・mode表示、Asset Browser／Project View からのpreview導線を確認。3Dのsource／final／compareルール、runtime視認性、実機受入は未完了または未確認。

### M-UI-10 3D Model Import and Contents Viewer Integration
- `ufbx` / `tinyobjloader` を使った 3D model 読み込み経路を整え、Contents Viewer へ正式に接続する
- 詳細は `docs/planned/MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md`
- **部分完了確認（Update 2026-08-15）**: `FileTypeDetector` の OBJ／FBX／glTF／GLB 判定、`MeshImporter` の ufbx 本線＋OBJ tinyobjloader fallback、backend／error 状態、`Artifact3DModelViewer` への接続、Contents Viewer／Project View／Asset Browser の preview 導線を確認。対応形式ごとの実ファイル受入、3D compare／annotation、diagnostic 連携は未完了または未確認。

### M-CP-1 Camera Projection Integration ⭐ **新規追加**
- 3D rendering のために camera の projection を適切に扱う
- **機能:** Perspective/Orthographic projection, viewport sync, matrix calculation
- **見積:** 20-30h
- **詳細:** `docs/planned/MILESTONE_CAMERA_PROJECTION_2026-03-31.md`
- **部分完了確認（Update 2026-08-15）**: `ArtifactCameraLayer` の Perspective／Orthographic、FOV／near／far／ortho size、JSON／property、projection matrix、Composition Render Controller の camera projection／fallback、3D gizmo の projection-aware scale／frustum overlay を確認。viewport resize、複数 viewport／active camera、camera transform と3D layerの行列順、保存再読込後のruntime整合は未完了または未検証。

### M-CP-2 3D Viewport Stabilization / Solid / Overlay
- 3D 表示を「読める」状態へ寄せ、solid shading / camera / overlay の責務を分けて安定化する
- gizmo / bounds / HUD の重なり順を固定し、wireframe と solid の両方で破綻しにくくする
- 詳細は `docs/planned/MILESTONE_3D_VIEWPORT_SOLID_CAMERA_OVERLAY_2026-04-10.md`
- **部分完了確認（Update 2026-08-15）**: Solid／Wireframe、material・depth付きmesh描画、PrimitiveRenderer3D のtexture cache、Composition側のcamera frustum／grid／gizmo／selection／HUD overlay、3D gizmo のbounds／projection-aware描画を確認。複雑meshのruntime破綻、viewer／composition editor間のcamera操作契約統一、長時間表示時のcache性能は未完了または未検証。

### M-LL-1 Light Linking System ⭐ **新規追加**
- 3D scene での light の影響を layer ごとに制御する
- **機能:** Light-to-Object linking, include/exclude lists, per-layer light influence
- **見積:** 25-35h
- **詳細:** `docs/planned/MILESTONE_LIGHT_LINKING_2026-03-31.md`
- **部分完了確認（Update 2026-08-15）**: `ArtifactLightLayer` の All／IncludeOnly／ExcludeList、対象 layer ID の保存・復元と Inspector 編集、`Artifact3DModelLayer` の affectedByLights、Composition renderer の light filtering、Directional／Point／Spot／Ambient の接続を確認。Layer Panel に All／Include Selected Layers／Exclude Selected Layers の quick link 操作を追加した。Outliner常時可視化、light group、per-object intensity、shadow linking、実機照明結果は未完了または未検証。

### M-MAT-1 3D Material System ⭐ **新規追加**
- 3D objects の material を定義し、適切な shading を実現する
- **機能:** Basic materials (diffuse/specular), texture mapping, material assignment
- **見積:** 30-40h
- **詳細:** `docs/planned/MILESTONE_3D_MATERIAL_SYSTEM_2026-03-31.md`
- **部分完了確認（Update 2026-08-15）**: `ArtifactCore::Material` の base color／emission／metallic／roughness／alpha、normal／occlusion／opacity、texture path、3D layer の Material Inspector／JSON／import texture、PBR renderer／shader接続を確認。Layer Panel に Matte／Metal／Plastic／Glass の Material preset 適用を追加した。Material Browser／asset管理、高度mapping、実機のshading・透明境界・texture更新受入は未完了または未検証。

### M-MAT-2 MaterialX Document / Exchange Bridge
- MaterialX XML を Material asset / inspector / export の橋渡しにする
- **機能:** document presence, canonical storage, import/export, preview summary
- **見積:** 18-28h
- **詳細:** `docs/planned/MILESTONE_MATERIALX_DOCUMENT_EXCHANGE_2026-04-10.md`
- **部分完了確認（Update 2026-08-15）**: `MaterialType::MaterialX`、XML document の保持／getter／setter／clear、Material JSON／preset経路、3D layer Inspector の MaterialX presence summary を確認。`Material::saveMaterialXDocument()`／`loadMaterialXDocument()` によるファイル保存・読込を追加した。専用編集UI、XML schema validation、PBR／shader renderer bridge、runtime round-trip は未実装または未確認。

### M-TY-1 Advanced Typography Engine ⭐
- **詳細:** `docs/planned/MILESTONE_ADVANCED_TYPOGRAPHY_ENGINE_2026-03-29.md` (Core 実装)
- **部分完了確認（Update 2026-08-15）**: `TextAnimatorEngine` の transform／style override、glyph単位評価、Text.ShapingBackend／Qt shaping、RTL／CJK／emoji基盤、glyph atlas／Diligent glyph描画、Text Layer の glyph evaluation と一部 Animator UI を確認。Layer Panel に Typewriter／Slide Up／Scale In／Rotation In／Tracking Fade／Wiggly Position／Blur Reveal の preset 適用と Clear Animators を追加した。全animator項目の反映、Text3D extrusion、Fluid／Spring physics、OpenType高度機能、runtime受入は未完了または未検証。

### M-TY-2 Typography Preset & Motion Style UI ⭐ **新規提案**
- 高度なタイポグラフィ制御とアニメーションシステムを UI/プリセット化
- **機能:** プリセットライブラリ・文字単位インスペクタ・パス追従 UI
- **見積:** 30-40h
- **詳細:** `docs/planned/MILESTONE_TYPOGRAPHY_PRESET_UI_2026-03-30.md`
- **部分完了確認（Update 2026-08-15）**: `ArtifactTextLayer` に Typewriter／Fade／Slide／Glitch 系の animator preset生成・推定・JSON／Property Editor 適用、Text Animator の Timeline表示、path text の start／end／reverse／align 設定を確認。独立した preset gallery／browser、文字単位専用 inspector、drag&drop適用、motion easing preset連携は未完了または未確認。

### M-CS-1 Advanced Color Science Pipeline
- **詳細:** `docs/done/MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md`
- **部分完了確認（Update 2026-08-15）**: ACES／LUT／ColorSpace／transfer function、Color Science Manager／Panel、composition color settings、HDR monitor（waveform／vectorscope／false color／gamut）、Color Grading Engine の実装を確認。production renderへの全面適用、HDR実機、runtime／color parity検証は未完了または未確認。

### M-SC-3 Color Grading Workspace ⭐ **新規提案**
- プロフェッショナルなグレーディング環境の構築
- **機能:** リアルタイムスコープ (Waveform/RGB Parade/Vectorscope)・比較表示・専用パネル
- **見積:** 32h
- **詳細:** `docs/planned/MILESTONE_COLOR_GRADING_WORKSPACE_2026-03-30.md`
- **部分完了確認（Update 2026-08-15）**: ColorScopeRenderer の Waveform／Vectorscope／Histogram／RGB Parade、HDR解析、ColorWheels／Curves／GradingEngine、LUT管理基盤を確認。専用workspace layout、継続scope更新、Inspector双方向同期、LUT browser preview、Wipe比較、mask partial application UI、runtime受入は未完了または未確認。

## Timeline / Layer

タイムライン系の整理用入口は [MILESTONE_TIMELINE_INDEX_2026-04-22.md](MILESTONE_TIMELINE_INDEX_2026-04-22.md) を先に見る。
古い文書は残しつつ、`Completed / Foundation` と `Active / Current` を分けて読む前提にする。
個別の `M-TL` 番号は legacy と current でぶつかることがあるので、本文のリンク先ファイル名を優先する。

### M-TL-4 Timeline TrackView Owner-Draw Migration
- 右ペインを `QGraphicsView` から owner-draw へ段階移行する
- 詳細は `docs/planned/MILESTONE_TIMELINE_TRACKVIEW_OWNER_DRAW_MIGRATION_2026-03-27.md`
- **部分完了確認（Update 2026-08-15）**: `ArtifactTimelineTrackPainterView` が正規右ペインとして clip／row／playhead／keyframe marker の描画、選択・hover・drag・resize・seek、zoom／horizontal・vertical offset を owner-draw で処理し、現行 `Artifact/src` に旧 `QGraphicsView`／`QGraphicsScene`／`QGraphicsItem` 系を確認できない。大量レイヤーでのscrub／drag／resize性能、shortcut／syncの完全回帰は未検証。

### M-TL-8 Timeline QGraphicsScene Elimination
- 右タイムラインの `QGraphicsScene` 依存を painter 側へ外し切る
- 詳細は `docs/planned/MILESTONE_TIMELINE_QGRAPHICSSCENE_ELIMINATION_2026-03-31.md`
- **完了確認（Update 2026-08-15）**: 現行Timeline実装に `QGraphicsView`／`QGraphicsScene`／`QGraphicsItem`／`TimelineScene`／`ClipItem` の参照はなく、`ArtifactTimelineTrackPainterView` が描画・hit test・入力・編集・playhead・scrollの正規経路。残りは旧資料の棚卸しと実機回帰確認のみ。Composition Graph のQGraphics利用は対象外。

### M-TL-9 Timeline Visual Language
- レイヤーバー、キーフレーム、再生ヘッド、選択ハイライトの色と形を意味ベースで統一する
- 詳細は `docs/planned/MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md`
- **部分完了確認（Update 2026-08-15）**: layer type別色、selection／hover／current状態、keyframe interpolation／easing／color label／lane、共通playhead helper、left panelとのaccent／border同期、Scrubbarのtheme token利用を確認。色定義・marker state helperの共通化、二重描画の実機確認、色覚差／light-dark theme回帰は未完了または未確認。

### M-TL-14 Timeline Layer Specialization Execution
- `Audio / Video / Text / Shape / Image / Particle` の最小専用化を、共通編集を壊さずに段階導入する
- 詳細は `docs/planned/MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23.md`
- timeline index では補助線扱い。view / input / lane の本筋に吸収される。
- **部分完了確認（Update 2026-08-15）**: layer descriptor／badge、Audio／Video／Text／Shape／Image／Particle／3D／Cameraの識別、AudioOnly／VideoOnly／SelectedOnly等の表示モード、Audio waveform preview のキャッシュ／表示、Audio／Video clip kind を確認。Audioの常時波形・fade／automation、Video thumbnail strip／source state、Text preview、Shape path補助、Particle専用stateは未完了または未確認。

### M-TL-15 Timeline Ripple Edit / Downstream Shift ✅ Phase 1 (verified 2026-06-26)
- `RippleTrimOutCommand`, `RippleDeleteCommand`, `SlideClipCommand` + undo/redo 実装済み
- マイルストーン文書: `docs/done/MILESTONE_TIMELINE_RIPPLE_EDIT_PHASE1_EXECUTION_2026-06-04.md`
- Trim / Delete ripple は完了。全レイヤー一括伸縮など Phase 2+ は未着手

### M-TL-5 Timeline Keyframe Editing ✅ (verified 2026-06-26)
- Add/remove/toggle keyframes, drag-move, multi-selection, copy/paste/cut
- Full interpolation control (Linear/Ease/Bezier/Hold) via context menu + Easing Lab
- Key pattern generation (12 presets), reverse, distribute, duplicate, color labels
- Jump navigation (Ctrl+PgUp/PgDn), status summary, undo/redo for all operations
- マイルストーン文書 `docs/done/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md` + `docs/done/MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md`
- **未完了 (stretch goal):** キーフレームの時間軸スケーリング（全レイヤー一括伸縮）

### M-TL-17 Timeline Proportional Keyframe Editing 🚧 Phase 1-2 実装済み（2026-07-30）
- 右ペイン `ArtifactTimelineTrackPainterView` に Blender 風の proportional editing を導入する
- Phase 1 は selected keyframes の time move のみに絞る
- `O` で on/off、`[` `]` で半径変更
- 詳細は `docs/planned/MILESTONE_TIMELINE_PROPORTIONAL_KEYFRAME_EDITING_2026-07-06.md`
- Phase 1-2: marker drag／area body drag／edge resize と preview／Undo の比例編集 ✅
- 残件: value direction、inline F-curve／Graph Editor 共有、advanced falloff／pivot

### M-UI-6b Composition Motion Path Display Improvement
- モーションパス overlay のサンプリングと描画を分離し、適応サンプリング・速度可視化・spatial bezier 表示へ進める
- まずは既存挙動を壊さない Phase 1 から入り、表示基盤を整えてから編集導線を重ねる
- 詳細: `docs/planned/MILESTONE_MOTION_PATH_DISPLAY_IMPROVEMENT_2026-07-10.md`
- **部分完了確認（Update 2026-08-15）**: motion path の sampling／drawing分離、zoom・曲率に応じたadaptive sample step、equal-time velocity dots、実補間位置のcurrent marker、spatial Bezier curve／in-out tangent handleの描画・hit test・drag・Undoを確認。複数キー／lasso選択、roving／constant-speed、modifier、runtime／build検証は未完了または未確認。

### M-TL-6 Timeline Layer Search
- タイムライン上部の検索バーで layer / effect / tag / state をインクリメンタルに絞り込む
- 詳細は `docs/planned/MILESTONE_TIMELINE_LAYER_SEARCH_2026-03-28.md`
- **部分完了確認（Update 2026-08-15）**: 常設検索欄、incremental filter、All Visible／Highlight Only／Filter Only、hit count／Enter・F3 navigation、name／property／type／effect／tag／parent-child／state／note／source queryを確認。検索ヒット移動時の選択行スクロールを実装。runtime回帰は未確認。

### M-TL-7 Timeline Search / Keyframe Integration
- search 結果から keyframe へ素早く飛べるようにし、header / status / highlight を統合する
- 詳細は `docs/done/MILESTONE_TIMELINE_SEARCH_KEYFRAME_INTEGRATION_2026-03-28.md`
- **完了確認（Update 2026-08-15）**: search hit count／current hit、keyframe count／nearest keyframe、filter／highlight status、Enter／Shift+Enter／F3／Shift+F3 の検索・keyframe navigation、header status／marker文脈を現行コードで確認。

### M-LG-1 Layer Group System
- レイヤーグループの保存 / 表示 / 親子 / 可視性 / 操作単位を整理する
- 詳細は `docs/planned/MILESTONE_LAYER_GROUP_SYSTEM_2026-03-27.md`
- Phase 1 実行メモは親文書へ統合済み
- **部分完了確認（Update 2026-08-15）**: `ArtifactGroupLayer` の composition-owned／embedded child、childrenForRender、offscreen composite、opacity／blend／mask、Timelineのgroup row／collapse／rename／selection／parent表示、JSON保存・再読込・循環拒否、state reasonとAutomation作成導線、TimelineからUndo付きのグループ解除を確認。group transform可視化、group単位move／delete／DnD、solo／shy／lockの全組み合わせ、Project View共通表現、再起動後runtime parityは未完了または未検証。

### M-LG-2 Layer Components: Physics / Behavior
- layer 側に軽量 component system を追加し、追従・減衰・トリガーの受け皿を作る
- 詳細は `docs/planned/MILESTONE_LAYER_COMPONENT_SYSTEM_UNITY_LIKE_2026-04-08.md`
- 固定評価順と Cloner / Layout / Crowd / Physics / Fracture / Particle の統合契約:
  `docs/planned/MILESTONE_LAYER_COMPONENT_EVALUATION_PIPELINE_2026-06-28.md`
- **部分完了確認（Update 2026-08-15）**: `LayerComponentHost` のdescriptor／attach・upsert・remove・enable／phase順／scope／validation／JSON、layer側host、compositionのcomponent simulation・runtime snapshot・bake／restore、Inspector validation、Cloner／Physics／Fracture／Particle等の統合経路、Motion Dynamics の追従・減衰・プリセットを確認。Behavior trigger／rule／note の独立共通surface、全componentのlifecycle parity、依存競合・性能・runtime回帰は未完了または未検証。

### M-PH Playhead 整備 ✅ 完了 (2026-07-30)
- ✅ Phase 1: 状態統一 — `currentFrame_` を単一権威、fan-out → `setCurrentFrameForAll()`、9 箇所の手動書替を統合
- ✅ Phase 2: keyframe drag の playhead／work area／layer bounds／keyframe／10-frame snap／プレイヘッド三角ドラッグ／スクロール追従
- 🚧 Phase 3: 表示品質 → ✅ HH:MM:SS:FF (TimeCodeWidget), HiDPI, コンポジションビュー連携、F<n> 入力解釈 済み
- ✅ Phase 4: 操作拡充 → JKL シャトル, ホイールシーク, ドラッグシーク, プレイヘッド三角ドラッグ, タイムコード／フレーム入力
- マイルストーン文書は `docs/done/` へ移動済み

## Render

### M-IR-1 ArtifactIRender API Cleanup
- viewport / canvas / pan / zoom の整理
- primitive API の責務固定
- **部分完了確認（Update 2026-08-15）**: `ArtifactIRenderer` に viewport／canvas／pan／zoom、zoom-around-point、canvas↔viewport変換、view／projection／3D camera行列、2D／3D primitive・glyph・particle・offscreen API が整理され、実装は PrimitiveRenderer へ委譲されている。旧APIとの責務重複、QImage sprite/readback境界、renderer façade のさらなる分離、runtime backend parityは未完了または未検証。

### M-IR-2 ArtifactIRender Software Backend
- Qt painter fallback の強化
- overlay / gizmo 用 2D 描画
- **現行コード確認（Update 2026-08-15）**: `PrimitiveRenderer2D` は `RenderCommandBuffer`／GPU primitive を中心に実装され、QPainter fallback の実体は確認できない。一方、QPainter を使う `OffscreenRenderer2D` と `ArtifactSoftwareImageCompositor` は別の legacy/software 経路として存在する。canvas／viewport変換、rect／line／Bezier／checkerboard／grid／text／sprite は GPU primitive 側で確認できるが、software fallback としての API 統合、primitive parity、QImage境界の縮小、runtime性能・表示差分検証は未完了または未確認。

### M-IR-3 ArtifactIRender Backend Parity
- software と Diligent の primitive 差分を縮める
- **部分完了確認（Update 2026-08-15）**: `PrimitiveRenderer2D`／`ArtifactIRenderer` の共通canvas／viewport変換と、rect／line／Bezier／overlay／sprite等のGPU経路を確認。software fallbackとの全primitive・blend・text・mask parity、pixel比較、backend切替時のruntime受入は未完了または未確認。

### M-RD-1 Software Render Pipeline
- コンポ作成
- Solid 追加
- preview
- effect
- 静止画シーケンス
- **部分完了確認（Update 2026-08-15）**: current composition追従の Software Composition／Layer Test、実データ由来の composition canvas、Solid／Image／Text／Video等の layer描画、transform／blend／bounds反映、Software Image Compositor、Preview／Render Queue の静止画経路を確認。effectの受入 parity、work area連番の実出力、先頭／末尾フレーム確認は未完了または未確認。

### M-RD-9 Render Path Decomposition / Buffer Migration
- `QImage` を render path の内部から段階的に追放し、typed buffer ベースへ寄せる
- `RawImage` は I/O 境界、内部は `ImageF32x4_RGBA` 系に分離する
- 詳細は `docs/planned/MILESTONE_RENDER_PATH_DECOMPOSITION_2026-03-31.md`
- **部分完了確認（Update 2026-08-15）**: `ImageF32x4_RGBA`／cache、明示的なQImage変換、GPU upload／surface cache、frame pass plan・診断summaryを確認。Composition render controllerのsurface／matte／resolved source／preview/readbackにQImage中間値が残り、主要passのtyped buffer化、format／alpha／colorspace契約、GPU／CPU parityは未完了または未検証。

### M-RD-10 Deep Compositing Support
- OpenEXR ベースの deep sample / deep merge / deep read-write の基盤を作る
- flat RGBA compositing と分離し、deep 用 buffer と IO を別系統で持つ
- 詳細は `docs/planned/MILESTONE_DEEP_COMPOSITING_2026-03-31.md`
- Update 2026-08-15: 旧文書は `MILESTONE_DEEP_COMPOSITE_2026-08-01.md` へ統合。DeepImageBuffer、depth sort／over／holdout／DoF、OpenEXR deep read/write、packed GPU契約とshaderを確認。通常Composition／Render QueueへのGPU binding、専用UI、大規模runtime parityは未完了または未検証。
- **部分完了確認（Update 2026-08-15）**: `DeepImageBuffer` の可変長サンプル、depth sort／flatten／Deep over／holdout、flat↔Deep変換、CPU depth DoF、OpenEXR Deep RGBA32F read/write、packed GPU契約／往復変換、DirectCompute front-to-back shaderを確認。通常のComposition Render Controller／Render QueueへのGPU resource binding、Deep専用制作UI、大規模runtimeおよびflat／deep parity検証は未完了または未確認。

### M-RD-11 GPU Mask Cutout Compute Pipeline 🚧 Phase 1-2 ✅, Phase 4 🆕
- Phase 1 (Mask Texture Contract) ✅ — `MaskCutoutPipeline` (+ `MaskCutout.ixx`) 完成済み
- Phase 2 (Compute Mask Apply) ✅ — 既存 `MaskCutoutPipeline::apply()` で compute shader cutout 可能
- Phase 4 (GPU Path Rasterizer) 🆕 — `MaskPathRasterizerPipeline` (+ `MaskPathRasterizer.ixx`) で MaskPath 頂点から直接 GPU マスク生成可能
- **未接続**: Phase 3 (Composition Integration) — `ArtifactCompositionRenderController` への配線は未着手
- CPU fallback (`LayerMask::applyToImage()`) は維持
- **現行確認（Update 2026-08-15）**: `ArtifactCompositionRenderController` は `MaskCutoutPipeline` を遅延初期化し、GPU track matte の source 収集／`LayerBlendPipeline` 適用も行う。一方、通常の `LayerMask::applyToImage()` を GPU cutout に置換する composition 共通契約、preview／playback／export の統一利用、失敗時CPU fallback、CPU／GPU画素 parity は未完了または未検証。
- 主要ファイル:
  - `ArtifactCore/include/Graphics/Shader/Compute/HLSL/MaskPathRasterizer.ixx`
  - `ArtifactCore/include/Graphics/Shader/Compute/MaskPathRasterizerPipeline.ixx`
  - `ArtifactCore/src/Graphics/Shader/Compute/MaskPathRasterizerPipeline.cppm`
  - `ArtifactCore/include/Graphics/Shader/Compute/HLSL/MaskCutout.ixx`
  - `ArtifactCore/include/Graphics/Shader/Compute/MaskCutoutPipeline.ixx`
  - `ArtifactCore/src/Graphics/Shader/Compute/MaskCutoutPipeline.cppm`
- 詳細は `docs/planned/MILESTONE_GPU_MASK_COMPUTE_PIPELINE_2026-04-03.md`

### M-RD-6 FFmpeg GPU Decode Backend
- CPU decode とは別に FFmpeg hwaccel backend を持ち、video layer / playback / preview から選べるようにする
- 詳細は `docs/planned/MILESTONE_FFMPEG_GPU_DECODE_BACKEND_2026-03-28.md`
- 低レベルAI実装メモ: `docs/planned/MILESTONE_FFMPEG_81_PRORES_GPU_DECODE_LOW_LEVEL_AI_2026-05-23.md`
- Update 2026-08-15: FFmpeg Vulkan hardware device／frame検出、GpuVideoFrame化、GPUTextureCache受け渡し、CPU download fallback、FFmpeg／MediaFoundation backend切替を確認。auto／cpu／gpu public policy、Qt preview直接GPU表示、seek／playback parity、driver／codec runtime受入は未完了または未検証。

### M-RD-7 Unified Audio / Video Render Output
- video render の後段で audio を mux し、音声付き出力を render queue から扱えるようにする
- 詳細は `docs/planned/MILESTONE_UNIFIED_AUDIO_VIDEO_RENDER_OUTPUT_2026-03-28.md`
- Update 2026-08-15: Render Queue の integratedRenderEnabled／audio source、Render Output の Include Audio UI、FFmpegAudioEncoder::muxAudioWithVideo、失敗時のvideo-only保持と診断を確認。multi-layer audio mix、codec／container parity、長尺・cancel・runtime受入は未完了または未検証。旧文書は `MILESTONE_AUDIO_PLAYBACK_STABILIZATION_2026-03-28.md` へ統合済み。

### M-RD-12 FFmpeg GPU Encode Backend ⭐ **新規追加**
- FFmpeg の hardware-accelerated encode backend を追加し、Render Queue から backend 選択できるようにする
- **機能:** NVENC/QSV/AMF/VAAPI 対応、自動検出、手動選択、品質/性能プリセット
- **見積:** 30-40h
- **詳細:** `docs/planned/MILESTONE_FFMPEG_GPU_ENCODE_BACKEND_2026-04-03.md`
- Update 2026-08-15: `FFmpegEncoder` の hardware encoder probe／codec候補、Render Queue の GPU backend・preset設定、Render Output の auto／native／pipe-hw (NVENC)／pipe-vulkan／CPU fallback 選択を確認。QSV／AMF／VAAPIの実機経路、renderer textureからのzero-copy、encode失敗時のretry／cleanup、codec・driver別のruntime受入は未完了または未検証。

### M-RD-8 Integrated Rendering Engine
- video / audio を同一 job として扱う render 本体の統合骨格を作る
- 詳細は `docs/planned/MILESTONE_INTEGRATED_RENDERING_ENGINE_2026-03-28.md`
- Update 2026-08-15: 旧文書は `MILESTONE_BATCH_RENDERING_2026-03-28.md` へ統合済み。現行 `RenderQueueService` で integratedRenderEnabled／audio source／codec／bitrate のjob契約、video temp→audio mux→final出力、mux失敗時のvideo-only保持とdiagnostics、JSON永続化を確認。composition音声の一時生成、multi-layer mix、audio range／seek parity、runtime受入は未完了または未検証。

### M-RD-2 Render Queue Hardening
→ 詳細: [MILESTONE_RENDER_QUEUE_2026-03-22.md](MILESTONE_RENDER_QUEUE_2026-03-22.md)
- job 編集
- 範囲指定
- 失敗理由表示
- 履歴と再実行
- **部分完了確認（Update 2026-08-15）**: `ArtifactRenderQueueService` の job 編集／範囲モード（Composition／Work Area／Custom／Selected／Single）／preflight／失敗フレーム再処理／履歴・再実行、永続 queue／completed history の復元、Queue Manager の progress／ETA／failure／history UI、farm checkpoint store と checkpoint-aware resume、worker retry を確認。checkpoint／resume 後の成果物整合、farm worker 障害時の実受入れ、長時間 runtime の queue／renderer 状態一致は未完了または未検証。
- in/out と work area の反映
- バックグラウンドレンダーの安定化
- 分散レンダリングの土台
- レンダー完了後の自動アクション
- checkpoint / resume

### M-RD-3 Dual Backend Parity
- software と Diligent の見た目差分を減らす
- **部分完了確認（Update 2026-08-15）**: Software Render Test に Preview／Render Queue のキャプチャ比較、サイズ不一致検出、pixel差分数／mean RGBA delta／最大deltaの表示を確認。Composition Editor は GPU blend／track matte／frame pass を持ち、software compositor は別経路で動作する。両バックエンドの同一入力・同一フレームでの恒常的な受入基準、mask／effect／text／blend parity、runtime差分記録は未完了または未検証。

### M-RD-5 Animated Image Export
- GIF / APNG / Animated WebP などの web 向け animated image 出力
- 詳細は `docs/planned/MILESTONE_ANIMATED_IMAGE_EXPORT_2026-03-27.md`
- **実装更新（Update 2026-08-15）**: Render Queue の共通FFmpeg sequence encodeで、GIFの全フレームpalettegen／paletteuse＋loop、APNG codec＋loop、Animated WebPの`libwebp_anim`＋alpha pixel format／loopを追加。実ファイル出力、透明背景、palette品質、再生互換性は未検証。

### M-LV-1 Layer Solo View (Diligent)
- 詳細は `docs/planned/MILESTONE_LAYER_SOLO_VIEW_DILIGENT_2026-03-26.md`
- 現行の更新案は `docs/planned/MILESTONE_LAYER_VIEW_ENHANCEMENT_2026-07-08.md`
- 実装順は LW-0 〜 LW-4（LayerPreviewPipeline 整理 → 共有 ViewportState / Overlay / DisplayFilter → レイヤー固有比較 → ドック統合）
- current composition / current layer の追従
- solo 表示の安定化
- mask / roto 入口の整理
- software test widget との見え方差分縮小
- inspect / impact / compare の導線整備
- inspect HUD / compare / effect stack summary の追加
- effect の部分適用 (Rect / Mask) の可視化
- **部分完了確認（Update 2026-08-15）**: `ArtifactLayerEditorWidgetV2` の Final／Alpha／Mask／Wire、View／Transform／Shape／Mask edit、zoom／pan／fit／reset、mask／shape Undo、cache／stage／source／effect／mask HUD、Impact依存表示、`ArtifactRenderLayerEditor` のwrapper接続を確認。共有ViewportState／Overlay／DisplayFilter、完全なBefore／After・channel／ROI／zebra、ドック統合、runtime selection追従とsoftware parityは未完了または未検証。

### M-CE-1 Composition Editor Cache System
- `Composition Viewer` の surface cache / render key / GPU blend fast path を整理
- ✅ ROI (Region of Interest) システム実装済み
- 詳細は `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_CACHE_SYSTEM_2026-03-26.md`
- **部分完了確認（Update 2026-08-15）**: layer単位 processed surface cache、render key による redundant composite suppression、ROI、GPU texture cache、simple scene の GPU blend bypass と preview downsample floor を確認。Composition Result Cache と dirty layer／partial recompose、複雑構成での長時間性能受入は未完了または未検証。

### M-CE-2 Static Layer GPU Cache
- 静止レイヤーの GPU texture を長く使い回す cache 層
- ✅ ギズモ描画最適化 (Phase 2) 完了
- 詳細は `Artifact/docs/MILESTONE_STATIC_LAYER_GPU_CACHE_2026-03-26.md`
- **実装確認（Update 2026-08-15）**: 静止レイヤーの cache signature／source revision、F32・QImageのGPU texture再利用、budget／最大エントリ数／LRU eviction、owner／key／device invalidate、hit／miss／upload／eviction診断、Image Sequence除外を確認。runtime性能とVRAM実測は未検証。

### M-CE-3 Composition Editor Figma-like Overlay / Snap / HUD
- smart guides / selection overlay / useful HUD を足して、Figma っぽい操作補助を入れる
- snap と選択オーバーレイを先に本体描画へ寄せ、その後 context HUD / probe を足す
- 詳細は `docs/planned/MILESTONE_COMPOSITION_EDITOR_FIGMA_LIKE_OVERLAY_2026-04-21.md`
- 実行メモは親文書へ統合済み
- Update 2026-08-15: TransformGizmo の edge／center／spacing／rotation snap、selection bounds／rotation／anchor、shape／mask／roto／3D overlay、operation HUD、Color Sampler の hover RGBA／画像ピクセル／canvas座標／最上位 layer ID 表示を確認。Region単位の mask/matte追跡、decode context、FrameDebug diagnostics統合、runtime視認性は未完了または未検証。
- **部分完了確認（Update 2026-08-15）**: 上記の snap／selection overlay／operation HUD と Color Sampler の hover RGBA／画像ピクセル／canvas座標／最上位 layer ID 表示を確認。Region Probe の mask／matte追跡、decode state context、FrameDebug diagnostics統合、runtime視認性は未完了または未検証。

### M-CE-4 Composition Editor Selection / Comparison Upgrade
- 矩形選択、追加 / 除外選択、ラッソ選択をビューポート側にまとめる
- A/B 切替、参照フレーム固定、差分オーバーレイを同じ比較導線として扱う
- 詳細は `docs/planned/MILESTONE_COMPOSITION_EDITOR_SELECTION_COMPARISON_2026-06-27.md`
- **部分完了確認（Update 2026-08-15）**: 矩形選択、Altラッソ、Shift追加／Ctrl toggle、複数選択HUD、A／B／Diff、reference frame pinning、Contents Viewerのcompare／wipe／swap設定を確認。全ツールモードでの選択契約統一、reference frameのproject保存、比較HUD／context menu統一、複数選択runtime E2Eは未完了または未検証。

## Shared Notes

- `docs/shared/ai-tech-memos/README.md`
- AI 同士で実装メモや調査要点を共有するための軽量な置き場

## Effects

### M-FX-1 Inspector Effect Stack Bridge
- Inspector から effect 追加、削除、順序変更
- **実装更新（Update 2026-08-15）**: `ArtifactInspectorWidget` の composition effect 追加・削除・enable・移動を `ArtifactProjectService` の専用 Undo command 経由へ接続。削除時の元位置、enable状態、stage内の移動元／先を保持して Undo／Redo 対象化した。全effect種別の共通追加UX、effect stack runtime parity、実プロジェクトでの並べ替え受入は未完了または未検証。

### M-FX-2 Solid Color Effects ✅ 完了
- 完了: `docs/done/MILESTONE_SOLID_COLOR_EFFECTS_2026-06-27.md`

### M-FX-3 Creative Effects Bridge
- Halftone
- Posterize
- Pixelate
- Mirror などを接続
- **部分実装更新（Update 2026-08-15）**: `ArtifactEffectService`／Inspector catalog に Halftone、Posterize、Pixelate、Mirror の追加・表示を確認。`builtin.halftone` は `ArtifactHalftoneEffect` の `runCreativeCompute()` 経由で実HLSL dispatchとCPU fallbackを持つ。一方、Core `CreativeEffectFactory` bridge（Mirror等）は現状CPU処理で、creative effect全体の同一ID／parameter schema、GPU executor、CPU／HLSL parity検証は未完了または未検証。

### M-FX-4 Creative Workflow & Inspector Refinement
- Creative Effect Pack (Halftone, etc.) 縺ｮ謗･邯・
- Inspector (Effect Stack) 縺ｨ Property Editor 縺ｮ驕｣蜍輔・蜷梧悄
- 隧ｳ邏ｰ縺ｯ `Artifact/docs/MILESTONE_CREATIVE_WORKFLOW_REFINEMENT_2026-03-13.md`
- **実装更新（Update 2026-08-15）**: Creative effect catalog／EffectService factory、Inspectorの検索・追加・paste・preset、Property Editorのeffect group／enable／remove、Inspector↔Property focus同期、drag reorder、effect preset／mask presetの保存・読込経路を確認。Mirror は Core bridge と Property 表示まで接続済み。composition effect の追加・削除・enable・移動も Undo 対象化した。component面との統合、全catalog項目の runtime parity／受入は未完了または未検証。

### M-FX-5 GPU Effect Parity
- CPU effect は reference として残しつつ、GPU equivalent effect を順に実装する
- `supportsGPU()` の宣言や CPU 呼び出しだけの GPU wrapper は完了扱いにしない。HLSL/compute dispatch を実装する
- CPU reference はテスト／比較／fallback 用として維持し、空間 effect は行・tile 単位で安全に MT 化する
- temporal / history effect は history の read/write 契約を固定してから MT 化する
- GPU 失敗時は結果契約を変えず CPU reference へ fallback する
- 実装・runtime 検証状況は `docs/analysis/EFFECT_MAP_2026-07-16.md` を正本にする
- 詳細は `docs/planned/MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md`
- Update 2026-08-15: `ArtifactAbstractEffect` の CPU／GPU／AUTO選択、GPU実装のHLSL／compute dispatch、各effectのCPU fallbackを確認。多数のColorCorrection／Blur／Glow／Keying等に実GPU経路がある一方、effect map上のCPU-only／declared-only／parity pending、GPUチェーン内readback、PSO／resource再生成、runtime pixel parityは未完了または未検証。詳細は統合先 `MILESTONE_GPU_EFFECT_PERF_FIXES_2026-07-22.md` を参照。
- **部分完了確認（Update 2026-08-15）**: Brightness／Blur／Glow／Color Correction／Mosaic等の一部でGPU context／executor／output resource保持とCPU fallback方向を確認。複数effectに `CopyTexture`→`Flush`→`WaitForIdle`→staging readbackが残り、GPU chain ping-pong、末尾一回readback、PSO共有、pointwise fusion、CPU／GPU parity測定、runtime既定GPU化は未完了または未検証。

### M-FX-6 Color Correction / Grading
- CPU reference を残しつつ、GPU 側の color correction / grading を実装する
- 詳細は `docs/planned/MILESTONE_COLOR_CORRECTION_2026-03-27.md`
- **実装確認（Update 2026-08-15）**: `ColorWheelsEffect`／`CurvesEffect` の CPU／GPU実装、Inspector catalog／factory、`ArtifactColorGradingEngine` の wheels／curves／Hue-Sat-Luma／LUT／preset／sample analysis、`ArtifactColorScienceManager` の LUT load／builtin／intensity／GPU参照を確認。GPU effect chainの恒常利用、grading engineと通常effect stackの状態統合、専用suite UI／scope／LUT browser、CPU／GPU parityのruntime受入れは未完了または未検証。

### M-FX-7 Partial Application
- `Rect` / `Mask` などの部分適用をエフェクトに導入する
- 全体適用と局所適用の両方を同じ stack で扱えるようにする
- mask を切らずに effect 単体で範囲指定できる導線を含める
- 詳細は `docs/planned/MILESTONE_EFFECT_SYSTEM_IMPROVEMENT_2026-03-28.md`
- **実装更新（Update 2026-08-15）**: `ArtifactAbstractEffect` に effect-local `QRectF` region（source surface pixel座標）の設定／解除／取得APIを追加し、`applyConfigured()` の共通blend段階でRect内だけeffect結果を合成するよう実装。Property Editorへ全effect共通の Enabled／X／Y／Width／Height 編集項目を追加し、composition／layer双方から直接編集可能にした。既存primary mask／effect mask imagesとの併用、project serialization、Inspectorのeffect copy/pasteも接続済み。effect preset serialization、tracked／shape／matte拡張、preview／render／solo parityは未完了または未検証。

### M-FX-8 Composition Final Effect
- composition 全体の最後に掛かる final effect / end-stage effect を検討する
- layer / effect stack の後段で、出力直前に 1 回だけ効く処理を想定する
- before / after の比較や render output 調整と合わせて扱う
- **実装確認（Update 2026-08-15）**: composition-owned `ArtifactAbstractEffect` stack と `applyCompositionFinalEffectsToBuffer()` を確認。layer合成後のbufferへ Rasterizer effectを順番どおり一度だけ適用し、Composition Editor／Preview、Render Queue、thumbnailから共通helperを利用している。なおCoreの `CompositionFinalEffectStack`／`ArtifactFinalPostProcess` は別契約として残っており、通常のcomposition effect stackとの統合は未完了。専用Inspector／before-after UI、GPU resource chain常用化、責務表示、runtime parityも未完了または未検証。

### M-FX-10 Visual Effect Bus
- composition final effect を起点に、group/shared render target を使う visual bus を検討する
- send / return を映像向けの中間レンダーターゲット共有として扱う
- 詳細は `docs/planned/MILESTONE_VISUAL_EFFECT_BUS_2026-06-02.md`
- **部分実装確認（Update 2026-08-15）**: composition final effect stackを合成後bufferへ適用する共通経路と、Render Queue／Composition Editor／thumbnailの利用を確認。名前付きsend／return、shared render target所有権、複数consumer、routing loop検出、pre／post契約、bus専用UIは未完了または未検証。

### M-FX-11 Effect UI Standardization
- すべてのエフェクトに共通の `Preview / Preset / Appearance` 契約を持たせる
- 標準エフェクトと OFX サードパーティエフェクトで UI の枠を揃える
- 実装順: `descriptor / section classification -> Inspector bridge -> preset browser / starter flow bridge -> appearance catalog -> OFX fallback -> Property alignment`
- 先行対象: `Gaussian Blur`, `Sharpen`, `Curves`, `Levels`, `Glow`, `OFX Plugin`
- 詳細は `docs/planned/MILESTONE_EFFECT_UI_STANDARDIZATION_2026-06-07.md`
- **実装更新（Update 2026-08-15）**: `EffectUIDescriptor`／`uiDescriptor()` を `ArtifactAbstractEffect` に追加し、Preview／Preset／Appearance／Fallback／Advanced の最小契約を共通化。OFX IDはFallbackへ分類し、Inspector Effect Rack tooltipからdescriptor状態／sectionを確認可能にした。Effect preset保存／読込とProperty Editorのeffect group／focus同期は確認済み。個別override、before／after UI、Appearance widget、preset browser、OFX自動分類の完全適用は未完了または未検証。

### M-FX-9 Face Detection & Auto-Mosaic
- OpenCV による顔認識 → 自動モザイク/ぼかしエフェクト
- Haar Cascade / DNN による検出、追従トラッキング
- 詳細は `docs/planned/MILESTONE_FACE_DETECTION_MOSAIC_2026-04-01.md`
- **実装更新（Update 2026-08-15）**: `AutoMosaicEffect::apply()` を追加し、通常のeffect stackから `ImageF32x4_RGBA` 上で detector 結果／手動領域のpixelate・Gaussian・median・featherを適用可能にした。検出器呼び出しの基盤はあるが、モデル資産、frame cache／FaceTracker接続、除外顔UI、Inspector preview、GPU経路、実フレーム品質・性能は未完了または未検証。

### M-UI-14 Multi-Display Support
- デュアル/マルチディスプレイ環境での制作ワークフロー強化
- セカンドモニタープレビュー、フルスクリーンプレビュー、モニター検出
- 詳細は `docs/planned/MILESTONE_MULTI_DISPLAY_SUPPORT_2026-04-01.md`
- **実装更新（Update 2026-08-15）**: `ArtifactSecondaryPreviewWindow` のscreen list／availableGeometry配置／fullscreen／ESC・F11／OSD／timeline frame更新と、rendererのDPR→physical viewport／render target／入力座標反映を確認。追加で選択screen名、fullscreen、auto update、更新FPS、window geometryを`QSettings`へ保存・復元し、screen名が変わった場合は安全にfallbackする表示プロファイルを接続した。refresh rate／color profile、monitor-aware workspace復元、複数画面・異なるDPIのruntime parityは未完了または未検証。

### M-AS-4 Asset Browser Improvement
- ファイルシステム監視、TBB 並列サムネイル、ブレッドクラム、お気に入り
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md`
- **実装確認（Update 2026-08-15）**: Asset Browserの検索／filter、Favorites／Unused／Missing、sequence detection、非同期thumbnail／waveform、breadcrumb／hover preview、内部D&D、file operation Undo／Redo、QFileSystemWatcher、ディスクthumbnail cacheの容量／期限管理を確認。TBB相当の並列thumbnail scheduling、差分型imported asset cache、watcher世代統合、大量ファイル性能とruntime安定性は未完了または未検証。
- **実装追加（Update 2026-08-15）**: watcher のファイル／ディレクトリ変更時に thumbnail generation を即時無効化し、遅延 refresh 前の stale thumbnail job を破棄する導線を追加。大量更新時の UI refresh coalescing と runtime 安定性は未検証。

### M-AS-4 Asset Browser Improvement
- ファイルシステム監視、TBB 並列サムネイル、ブレッドクラム、お気に入り
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md`

### M-VFX-1 Real-time Particle & Fluid Simulation ⭐ **Surface FXへ統合済み**
- Compute Shader ベースの高性能視覚効果
- **機能:** GPU パーティクル・2D 流体ソルバー (Smoke/Fire)・インタラクティブ・シミュレーション
- **見積:** 40-60h
- **詳細:** `docs/planned/MILESTONE_VFX_PARTICLE_FLUID_2026-03-30.md`
- **補足:** `fluid` は layer component、`pyro` は独立 volume domain として分離する。`docs/planned/MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md`
- **現行確認（Update 2026-08-15）**: `ParticleCompute` のGPU structured buffer／compute dispatch、layer内 `FluidSolver2D` の密度・速度更新、設定保存／復元、preview particle描画を確認。GPU particle emit／draw統合、GPU pressure solve、layer／mouse input、専用VFX pass、depth／mask、bake、runtime parityは未完了または未検証。Surface FX Systemとの責務重複は避ける。
- **方針更新（Update 2026-08-15）**: 独立 Particle／Fluid 経路の追加は行わず、Surface FX または既存 layer component の統合契約を先に更新する。

### M-VFX-2 AE-Style Simple Rain Effect
- 既存 particle / effect 基盤で、AE っぽい簡易雨を最短構成で実現する
- streak / density / direction / splash / depth feel を preset 中心でまとめる
- **見積:** 8-14h
- **詳細:** `docs/planned/MILESTONE_AE_STYLE_SIMPLE_RAIN_EFFECT_2026-05-31.md`
- **実装確認（Update 2026-08-15）**: `SimpleRainEffect` のCPU／ImageF32x4経路、density／streak／speed／wind／opacity／depth／splash／evolution／seed、deterministic hash生成、EffectService factory／catalog登録、`Preset` プロパティによる Light／Heavy 適用を確認。Phase 1〜2と最小プリセット適用は実装済み相当。GPU equivalent、専用preset UI／配布、zoom／bounds、preview／render queue parity、性能受入れは未完了または未検証。

## Audio

### M-AU-1 Composition Audio Mixer
- mute / solo / volume / layer 同期
- **実装更新（Update 2026-08-15）**: `ArtifactCompositionAudioMixerWidget` と `ArtifactAudioMixer` の channel/master、volume／pan／mute／solo、level／peak、routing 表示・同期を確認。`AudioMixerWidget` のAdvanced Routing surfaceでbus rename／remove、output route、sidechain send／clear、cycle拒否まで接続済み。sidechain menuにも既存sendのamountとchecked状態を再表示するよう更新した。owner-draw メーターに 0 dBFS 超過時の赤い clip インジケーターも追加済み。composition再openの完全受入、routing編集のUndo、callback thread安全性、実機runtime parity、複数layerのsolo/mute受入れは未完了または未検証。

### M-AU-2 Playback Sync
- 再生位置と音の同期
- **実装確認（Update 2026-08-15）**: `ArtifactPlaybackEngine` の pre-roll／buffer 供給／`syncWithAudioClock()` と `ArtifactPlaybackService` の audio timer／offset、seek／stop resync 経路を確認。外部 provider 未設定時に engine 側へ `0.0` を返していたため内部 audio clock 同期が無効だった箇所を、controller と同じ timer／offset fallback に修正した。device clock drift、seek後の stale audio、実機 runtime parity は未完了または未検証。

### M-AU-3 Audio Visualization
- waveform / meter
- 詳細は `docs/planned/MILESTONE_AUDIO_WAVEFORM_2026-03-29.md`
- **実装確認（Update 2026-08-15）**: `AudioWaveformGenerator` の peak／RMS 抽出・cache、`AudioLevelMeter` の attack／release／peak hold／clip 検出、Timeline／Composition viewport／Asset Browser／Contents Viewer の waveform、spectrum overlay を確認。静的基盤は実装済み相当だが、長尺・無音・未ロード・source 差し替え、表示遅延、sample-rate 契約、実機 runtime parity は未完了または未検証。
- **実装追加（Update 2026-08-15）**: Contents Viewer の waveform／spectrum 解析で、固定 44.1kHz ではなく audio stream metadata の sample rate を使用するよう補正。metadata 不在時は 44.1kHz fallback。実ファイル runtime parity は未検証。

### M-AU-8 Audio Widget Enhancement / Mixer Surface
- `ArtifactCompositionAudioMixerWidget` を中心に、mute / solo / volume / pan / waveform / meter / state badge をまとめる
- 詳細は `docs/planned/MILESTONE_AUDIO_WIDGET_ENHANCEMENT_2026-04-09.md`
- **実装更新（Update 2026-08-15）**: channel／master strip の volume・pan・mute・solo、FX／routing 導線、level／peak meter、clip indicator、summary/state 表示を確認。Composition Mixer summaryにaudio layerのmissing／unloaded件数を追加表示した。waveform は他の表示面にあるが mixer 内常設、Mixer／Timeline／Inspector のbadge統一、playback head runtime同期、大量素材負荷は未完了または未検証。

### M-AU-7 Audio Waveform Thumbnail Preview
- audio file の thumbnail として waveform を表示する
- Asset Browser / inspector / detail panel で見た目の判別力を上げる
- **AE差別化:** ホバーで波形アニメーション（プロっぽさ向上）
- 詳細は `docs/planned/MILESTONE_AUDIO_WAVEFORM_THUMBNAIL_PREVIEW_2026-03-31.md`
- **実装確認（Update 2026-08-15）**: Asset Browser の非同期 waveform thumbnail、peak／RMS、生成中／失敗 fallback、stale job 破棄、cache、hover preview を確認。長時間音声、cache eviction、source metadata 更新、Inspector／Render Queue 整合、大量アセット負荷は未完了または未検証。

### M-AU-6 Audio Reactor System ⭐ **新規提案**
- オーディオ解析による自動アニメーションシステム
- **機能:** リアルタイム FFT 解析・オーディオ駆動プロパティリンク・スムージング制御
- **見積:** 36h
- **詳細:** `docs/planned/MILESTONE_AUDIO_REACTOR_SYSTEM_2026-03-30.md`
- **実装確認（Update 2026-08-15）**: `AudioAnalyzer` の FFT／RMS／Peak／Low-Mid-High、composition の binding／attack-release／gain／offset／clamp／JSON、Animation menu／Particle／Expression／Viewer 接続を確認。再生ループからの継続解析供給、専用 `AudioFFTService`、main-thread 側の安全な binding 適用、Inspector 常設 Audio Link UI、共通 preset、runtime 負荷・音画同期は未実装または未検証。

### M-AU-4 Audio Layer Integration & UX
- Audio Layer の source / mute / volume / loaded state を inspector と timeline に自然に接続する
- 詳細は `docs/planned/MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md`
- `MILESTONE_AUDIO_ENGINE_2026-03.md` の再生基盤とは分けて、layer 側の見え方と導線を詰める
- **実装確認（Update 2026-08-15）**: `ArtifactAudioLayer` の source／asset identity、volume／pan／mute、PCM／waveform summary、AudioCache、JSON保存復元、Timeline／Inspector／Mixer／Playbackの接続、source置換／relink／Undo経路を確認。missing／decode failure／empty sourceのUI統一、複数layerのsolo／mute実音声挙動、scrub／runtime parityは未完了または未検証。

### M-AU-5 Audio Playback Stabilization
- start-up pre-roll, stop/seek hygiene, buffer diagnostics, format normalization
- 詳細は `docs/planned/MILESTONE_AUDIO_PLAYBACK_STABILIZATION_2026-03-28.md`
- Phase 1〜4 の静的実装済み。スクラブ再入場時の旧バッファ破棄を追加済み。実機の format / drift / underrun 検証は未完了
- **現行確認（Update 2026-08-15）**: pre-roll／buffer threshold／silence補填／resync clear、AudioRendererのbuffer hygiene、underflow／overflow diagnostics、sample-rate resampling、decoder flush／cursor更新、Playback Serviceの内部audio clock fallbackを確認。全channel layoutの正規化、device clock drift、seek／stop／restart後のstale audio、実機のunderrun改善効果は未完了または未検証。

## Project / Asset

### M-PV-1 Project View Basic Operations
- Project View selection と current composition の同期
- rename / delete / double-click
- 基本検索と filter
- footage selection を Asset Browser に返す往復同期を追加し、Project View 起点の探索を短くした
- selection chrome に Asset Browser linked の sync chip を出して、同期状態を読めるようにした
- **AE差別化:** コンポジションとアセットの明確な分離（混在しない構造）、仮想フォルダ vs 実フォルダの分離（実FS同期＋スマートコレクション）
- **実装更新（Update 2026-08-15）**: `ArtifactProjectView` の selection model／current index、composition 選択同期、rename／delete／double-click、incremental search／type・status・tag filter、footage 選択の Asset Browser 返却、selection chrome の linked 表示を確認。再構築時にCompositionId／AssetIdをキーとして既存選択を復元する処理を追加した。複数選択の全操作、外部変更・再読込時の完全な選択維持、仮想フォルダ／実FS／smart collection の完全分離、runtime 回帰は未完了または未検証。

### M-PV-2 Project View Asset Presentation
- thumbnail
- type icon
- size / duration / fps / missing 状態
- selection summary と selection detail を使って、現在選択中 item の path / status を読めるようにしている
- **AE差別化:** ホバープレビュー（サムネイルホバーで動画パラパラ再生、Finder風）、コンポのサムネイルプレビュー、レンダリング状態バッジ（レンダー済み/未レンダー/キャッシュあり）、依存関係の可視化（コンポの依存ツリー表示・逆引き検索）
- **実装確認（Update 2026-08-15）**: Project View の type icon／footage・composition metadata、size／duration／fps、missing／unused 表示、選択 summary／detail、lazy preview cache、hover thumbnail popup を確認。render cache 状態 badge、依存ツリー／逆引き、動画の連続 hover preview、外部変更後の metadata 更新、大量項目時の preview 負荷は未完了または未検証。
- **実装追加（Update 2026-08-15）**: Project View の hover thumbnail popup 遅延を 1100ms から Asset Browser と同じ 300ms に統一し、探索時の preview 応答を改善した。

### M-PV-3 Project View Organization
- folder / bin 整理
- expand / collapse
- unused / tag / virtual view
- **AE差別化:** タグ・カラーラベルでフィルタリング (AEのプロジェクトパネルより使いやすく)、カラムビュー（Finder風ミラー列表示）、ピン留め・スター機能（よく使うコンポ/アセットのクイックアクセス）、未使用アセット・コンポの可視化（グレーアウト/バッジ表示）
- **実装確認（Update 2026-08-15）**: folder/bin の作成・階層表示・expand/collapse・move・cycle 防止・JSON 保存復元、unused／missing の表示・filter、内部 D&D を確認。個別 item の tag／color label／favorite／star／pinned 永続化、Finder 風 column view、smart collection の独立モデル、未使用 composition の常設可視化は未完了または未検証。

### M-PV-4 Project View Interaction Polish
- selection center / quick actions / open-reveal-rename-delete-relink の整理
- **AE差別化:** 最近使ったアセット履歴（プロジェクト跨ぎ）、未使用アセット検出ハイライト、賢いD&D（自動レイヤー生成・複数整列オプション）、構造化クエリ検索（type:comp duration:>30s used:false などのメタデータ検索）
- 詳細は `docs/planned/MILESTONE_PROJECT_VIEW_INTERACTION_POLISH_2026-03-28.md`
- **実装確認（Update 2026-08-15）**: selection center、Reveal／Reveal Proxy／Rename／Delete／Relink の quick actions、unused snapshot／highlight、内部 D&D、`type:`／`tag:`／`regex:`／`unused:true` の構造化検索を確認。仕様例にあった `used:true`／`used:false` と `is:used`／`is:unused` の検索 alias を追加した。最近使用履歴、duration 比較クエリ、複数選択の自動レイヤー生成・整列、プロジェクト跨ぎ履歴は未完了または未検証。

### M-PV-5 Project View Search / Filter / Presentation
- incremental search / multi filter pills / unused emphasis / list-grid presentation / status bar を Project View surface にまとめる
- 詳細は `docs/planned/MILESTONE_PROJECT_VIEW_SEARCH_FILTER_PRESENTATION_2026-04-03.md`
- **実装確認（Update 2026-08-15）**: incremental search、name/path/metadata、`type:`／`tag:`／`regex:`／`unused`／`used`／`missing` filter、Tree／Tile、unused/missing emphasis、件数・filter・選択容量 status、sort／column width／設定保存を確認。独立した multi-select pill、list/grid 名称統一、全 asset type の pill、view transition、複合条件の runtime 検証は未完了または未検証。

### M-PV-6 Project View Scroll Stability ✅ (2026-06-08)
- import で Project View の scroll position を勝手に先頭へ戻さない
- 新規素材追加時も現在の表示位置を維持する
- 詳細は `docs/done/MILESTONE_PROJECT_VIEW_SCROLL_STABILITY_2026-06-07.md`

### M-AS-1 Asset Import Flow
- 読み込み
- 再リンク
- メタ表示
- 未使用管理
- **実装確認（Update 2026-08-15）**: Project View と Asset Browser の file dialog／context import／external drag & drop を既存の `importAssetsFromPathsAsync()` 経路へ統一し、UI イベント中の同期 import を解消。sequence grouping、metadata／missing／unused、bulk relink／search root も確認。import 結果の cross-view 即時反映、relink 通し挙動、save／restore 後の state は未完了または未検証。

### M-AS-2 Composition / Project Organization
- project tree
- 検索
- 並び
- タグ
- **実装確認（Update 2026-08-15）**: Project Model／Project View の tree・folder/bin、検索・type／unused／missing filter、列 sort／column width 保存復元、Asset Browser 連携を確認。ProjectItem にタグを追加し、コンテキストメニュー編集、`tag:` 検索、JSON保存／復元、コピー／貼り付けへ接続。tag と cross-view runtime 受入れは未完了または未検証。

### M-AS-4 Asset System Integration
- `AssetBrowser` と `Project View` の同期
- import / metadata / relink / missing / unused の統合
- Project View の footage selection から Asset Browser への追従もつなぎ、往復同期へ前進
- Asset Browser / Project View の両方に sync chip を置き、連動状態を見える化
- 詳細は `Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md`
- **実装確認（Update 2026-08-15）**: `AppMain` の guard 付き Project View ↔ Asset Browser 双方向 selection、sync chip、imported／missing／unused status、metadata／waveform、selected／bulk relink と Undo、非同期 import 経路を確認。cross-view 即時反映、再読込後の selection／active composition、実データ通し runtime parity は未完了または未検証。

### M-AS-9 Project / Asset Workflow Bridge
- Project View / Asset Browser / Contents Viewer / Render Queue を一続きにする
- import / relink / recent / favorite / missing / dependency の導線整理
- **AE差別化:** ファイルシステムと直結（AEはインポートしないと使えない）・ホットリロード対応（ファイル更新で自動反映）・未使用アセット検出（UI改善）
- 詳細は `docs/planned/MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md`
- search / collections / review の派生詳細は別文書へ分割
- **実装確認（Update 2026-08-15）**: Project View／Asset Browser／Contents Viewer の selection／preview／recent／favorite／missing／unused／dependency 導線を確認。Asset Browser の選択 import、clipboard import、external drop import を既存の `importAssetsFromPathsAsync()` に統一し、drop の完了通知を維持。Project View のCompositionコンテキストメニューからRender Queueへ投入する導線を追加。timelineへのasset bridge、hot reload、cross-view runtime parity、save／restore 後の workflow は未完了または未検証。

### M-AS-4b Vector / SVG Layer Import
- SVG などの vector asset を layer として取り込む
- source 保持 / raster preview / relink / persistence
- 詳細は `docs/planned/MILESTONE_VECTOR_LAYER_IMPORT_2026-03-25.md`
- **実装確認（Update 2026-08-15）**: `ArtifactSvgLayer` の SVG ingest、source path／version、fit／transform property、非同期 raster cache、`ImageF32x4_RGBA` preview、JSON 保存復元、missing／relink 判定を確認。path／text／group 単位の vector 編集、embedded data／複数ページ、SVG stroke／fill 編集、software／Diligent parity、zoom 時再 rasterize、実ファイル runtime 受入れは未完了または未検証。

### M-AS-5 Video Layer Unification
- `Video` 縺ｫ荳譛ｬ蛹・
- layer factory / serialization / property / project presentation 縺ｮ豕ｨ諢丈ｺ・
- 隧ｳ邏ｰ縺ｯ `Artifact/docs/MILESTONE_VIDEO_LAYER_UNIFICATION_2026-03-13.md`
- **実装確認（Update 2026-08-15）**: `LayerType::Media` / `ArtifactMediaLayer` の残存参照なし。factory/import inference、`ArtifactVideoLayer` の `VideoLayer` JSON 復元、`video.*` property、timeline / inspector / Project View の共通経路を確認。動画デコーダー・render backend の受入れは未検証で、静止画・連番・シェイプ優先方針により保留。

### M-AS-6 File Menu Workflow
- project create / open / save / close / restart / quit 縺ｮ謨ｴ逅・
- **実装確認（Update 2026-08-15）**: File Menu の lifecycle、recent project reopen、dirty confirmation、reveal/restart/quit、composition/import bridge を監査。project 未オープン時にも `新規コンポジション` が有効だったため `hasProject` 条件へ修正し、asset filterへSVGを追加してSVG import経路と整合。runtime 受入と save/restart failure の実機フィードバック確認は未完了。
- recent projects / unsaved changes / import / composition create 縺ｮ邨ｱ蜷・
- 隧ｳ邏ｰ縺ｯ `Artifact/docs/MILESTONE_FILE_MENU_2026-03-13.md`

### M-AS-7 Edit Menu Workflow
- undo / redo / copy / cut / paste / delete / duplicate の実コマンド接続
- split / trim / select all / find / preferences の context-aware menu state
- 詳細は `Artifact/docs/MILESTONE_EDIT_MENU_2026-03-13.md`
- **実装確認（Update 2026-08-15）**: Undo/Redo、clipboard、selection、delete/duplicate、split/trim、find/preferences の接続を監査。selection のみで有効だった split/trim 系に playback service 存在条件を追加し、playhead 不在時を無効化。runtime 受入と shortcut 完全一致は未確認。

## Core / Architecture

### M-AR-1 Service Boundary Cleanup
- UI 直参照を減らして service 経由へ統一
- **実装更新（Update 2026-08-15）**: `ArtifactProjectService::ensureProject()` と `currentProjectAssetsPath()` を追加し、MainWindow の新規コンポジション／アセット導線および Asset Browser の assets path helper から `ArtifactProjectManager` 直接参照を除去した。ファイル open、WebUI の composition count、open/save/close facade と current project path API の整理は未完了。
- **実装確認（Update 2026-08-15）**: 通常の編集・asset・composition 操作は `ArtifactProjectService` 経由が中心。一方、`ArtifactMainWindow` の welcome/recent project の open と create/import fallback、`ArtifactProjectManagerWidget` / Asset Browser の path helper、WebUI の composition count には `ArtifactProjectManager` 直接参照が残る。現行 service に async open/save/close の facade がないため、今回の置換は API 増設を伴い、runtime 契約確認なしには安全に進められない。境界整理は未完了。

### M-AR-2 import std Rollout
- 安全な module から順に C++23 / `import std;` 化
- **実装確認（Update 2026-08-15）**: 既存の導入済み範囲を棚卸しし、標準ライブラリ依存が `std::max` / `std::clamp` に閉じた `Artifact/src/Settings/AccessibilitySettings.cppm` を追加移行。global module fragment の `<algorithm>` と未使用 `<cmath>` を除去し、module 宣言後へ `import std;` を配置。CMake／ビルド未実行のため toolchain 受入は未確認。

### M-AR-3 Serialization Cleanup
- layer / composition / effect の JSON 保存整理
- **実装確認（Update 2026-08-15）**: effect の `pipelineStage`・properties・expression/keyframes、composition の effects/transform、layer の canonical numeric `type` と legacy fallback の保存・復元対称性を確認。統一 Serialization Framework の adapter／registry／migration／ProjectSerializer facade は存在するが、全 layer/asset の typed envelope 化と既定 project 形式への全面移行は未完了。

## Test / Validation

### M-QA-1 Software Test Windows
- current composition / current layer 追従の検証窓を強化
- **実装確認（Update 2026-08-15）**: `ArtifactSoftwareRenderTestWidget` は synthetic software render／Preview・Render Queue capture 比較に加え、既存の Project Service と Layer Selection Manager から current composition / current layer を毎フレーム読み取り、context status として表示するよう更新。実際の選択切替・preview output との runtime 受入は未実施。

### M-QA-2 Manual Regression Checklist
- タイムライン、render、audio、dock の確認表
- **実装確認（Update 2026-08-15）**: `Artifact/docs/RC5_EDITING_READY_REGRESSION_CHECKLIST_2026-03-12.md` に timeline / render / audio / dock の補足マトリクスを追加。Pass / Fail(P0/P1/P2) / N/A と観測メモを記録できる。実機でのチェック結果は未入力。

### M-QA-3 Crash / Diagnostics
- recovery
- **実装確認（Update 2026-08-15）**: `CrashHandler` の report 保存、起動時 pending report ingest、Trace/SessionLedger 記録、`--safe-mode` 入口を確認。RC-5 checklist に report ingest、safe mode、recovery point、diagnostic export の手動項目を追加。symbolized stack／minidump、recovery UI、実クラッシュ E2E は未完了・未検証。

## Render / Playback

### M-RP-1 RAM Preview Cache
- frame cache を RAM preview の主経路として扱う
- prewarm / fill / cache range
- playback / scrub / loop との連動
- hit rate / stale cache / dropped frame の可視化
- **実装確認（Update 2026-08-15）**: `ArtifactRamPreviewController` の range／priority／prewarm／cancel、Playback Service の RAM／disk fallback・hit rate・dropped frame、Timeline cache bar、Viewer Footer の cache summary を確認。汎用 `ArtifactFrameCache` の hit 優先再生への完全接続、loop／work-area／scrub の連続再生保証、全 UI の状態統一は未完了・未検証。
- 詳細は `docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md`
- 低レベルAI実装メモ: `docs/planned/MILESTONE_PREVIEW_PLAYBACK_PERFORMANCE_LOW_LEVEL_AI_2026-05-23.md`

### M-RP-2 Disk Cache System
- 永続 preview cache / manifest / eviction / diagnostics
- 詳細は `docs/planned/MILESTONE_DISK_CACHE_SYSTEM_2026-03-26.md`
- **実装確認（Update 2026-08-15）**: 本項目は `MILESTONE_HIERARCHICAL_CACHE_SYSTEM_2026-07-21.md` へ統合済み。現行 Playback Service には composition namespace、manifest v1、state/contract hash、非同期 PNG 書込、RAM hydrate、generation invalidation、namespace/global budget、LRU eviction、orphan cleanup、clear/diagnostic 導線がある。再起動後の実動作、上位 cache facade との横断 key 契約、intermediate promotion、Render Queue 共有は未完了・未検証。

### M-RP-3 GPU-Driven MDI Render
- GPU 側で visibility / compaction / batch formation を行い、MDI submission に繋げる
- 既存の CPU render queue を壊さず、fallback 付きで段階導入する
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_GPU_DRIVEN_MDI_RENDER_2026-04-02.md`
- **実装確認（Update 2026-08-15）**: `ParticleRenderer` / `MeshRenderer` の persistent indirect args、GPU visibility compaction、Diligent 経由の `DrawIndirect` / `DrawIndexedIndirect`、capability 不足・少数 item の direct fallback を確認。Clone の `SolidRectXformPkt` GPU compaction、非同期 counter readback、CPU parity、D3D12/Vulkan backend parity、preview/playback/export 共通化、break-even 測定は未完了・未検証。低レベル backend は変更していない。

## Matte

### M-LYR-1 Matte Stack / Child Matte Nodes
- matte を layer の child / attached node として扱う
- Add / Common / Subtract の複数 matte 合成
- Alpha / Luminance / Inverted の評価
- dependency order / cycle check / diagnostics
- 詳細は `ArtifactCore/docs/MILESTONE_TRACK_MATTE_CORE_2026-03-26.md`
- **実装確認（Update 2026-08-15）**: Core の `MatteStack` / `MatteNode` / `MatteEvaluator` と Artifact の legacy `LayerMatteReference` による複数 matte、cycle 防止、missing diagnostics、render 適用を確認。Core stack を全 render の canonical source へ統合すること、luminance source parity、hidden source policy、実素材受入は未完了・未検証。

---

## App Layer Completeness

### M-RV-1 Reactive Event Editor Window
- 独立ウィンドウで reactive event ルールを編集する
- Target Tree は owner-draw、他は既存 Qt widget を使う
- Target Tree / Event Rules / Inspector / Event Log を 1 画面にまとめる
- ルールはフレーム末キュー前提で、`PropertyOverlay` と `ContactSubscription` を編集対象にする
- `TimelineReaction` / `TriggerReaction` / `PhysicsReaction` の編集導線を整理する
- 詳細は `docs/planned/MILESTONE_REACTIVE_EVENT_EDITOR_WINDOW_2026-03-29.md`

### M-APP ApplicationLayer completeness
- 詳細は `docs/planned/MILESTONE_APP_LAYER_COMPLETENESS.md`
- Update 2026-08-15: サービス／Undo／ツール接続／保存系は現行コードで実装済み。DAG は compile・依存解決・画像伝播・失敗枝遮断まで実装済みだが、Transform／LayerRender の実入力と非画像 backend 評価は未接続。仮の恒等処理は追加せず、次は実データを供給できる画像ステージから接続する。
- Phase 1: サービス層の穴埋め (EffectService, AudioService, TranslationManager)
- Phase 2: Undo Add/RemoveLayerCommand の実装
- Phase 3: EditMode / DisplayMode の UI 接続
- Phase 4: エフェクトパイプライン接続 (Generator::apply, DAG eval, renderFrame)
- Phase 5: データ/永続化 (PreCompose 時間変換, VideoProxy, AspectRatio)
- Phase 6: 拡張 (OFX ホスト, WebBridge) — WebBridge の LayerID／effect lookup／project info／selected-layer JSON は静的実装済み、OFX と runtime 検証は継続
- ログ
- 診断導線

### M-INF-1 Internal Event System Migration
- `ArtifactCore` の `EventBus` を使って、Project / Timeline / Inspector / Render Queue / Asset Browser の広域更新を段階的に置き換える
- Qt signal は高頻度入力と widget 内部状態に限定し、fan-out の大きい状態変化だけ EventBus に寄せる
- 詳細は `docs/planned/MILESTONE_EVENT_SYSTEM_MIGRATION_2026-03-25.md`
- widget 別の切り替え表は `docs/planned/MILESTONE_EVENT_BUS_WIDGET_MIGRATION_2026-04-01.md`

### M-DEV-1 Crash Diagnostics & Recovery
- **実装更新（Update 2026-08-15）**: `ArtifactCore::CrashHandler::captureStackTrace()` のプレースホルダーを Windows `CaptureStackBackTrace` に置換し、クラッシュレポートへ最大128フレームのアドレス列を出力するようにした。symbol 解決、minidump、実クラッシュ E2E は未完了。
- 目的: アプリケーションのクラッシュ原因を迅速に特定し、回復フローと診断情報収集を整備する
- 期待結果: クラッシュ時に一貫した診断データ（スタックトレース、重要オブジェクトスナップショット、環境情報）が収集され、主要クラッシュに対するセーフモード起動や自動復旧案内が提供される
- タスク:
  - (1) 既存のログ＆クラッシュダンプ取得フローを調査してドキュメント化
  - (2) 例外／シグナルハンドラでのスタックトレース収集と簡易ダンプの実装
  - (3) 重要オブジェクト（Project, Composition, Asset コンテナ等）のスナップショット保存ロジック追加
  - (4) ユーザ向け回復案内（セーフモード起動、ログ送信）の実装
  - (5) CI/QA 向け再現手順と小規模回帰テストを用意
- 見積: 4-12時間（段階的実装を想定）

---

## Composition Editor & Layer View

### M-CE-GZ-1 ImGuizmo Direct Code Port
- 詳細は `Artifact/docs/MILESTONE_IMGUIZMO_DIRECT_CODE_2026-04-09.md`
- Update 2026-08-15: `TransformGizmo` の Move／Rotate／Scale、renderer primitive 描画、hover／active hit test、multi-target drag、snap、Undo、Composition／Layer Editor overlay 接続を現行コードで確認。3D 共通化、software／Diligent parity、runtime 診断は未完了・未検証。
- `ImGuizmo` を外部ライブラリとして使うのではなく、描画プリミティブと操作ロジックを Artifact のコードとして移植する
- `TransformGizmo` / `ArtifactIRenderer` / composition overlay へ直接接続する
- translation / rotation / scale を direct code で順に移す
- hit test と draw の座標系を一致させ、backend parity を確認する

### M-CE-TEXT-1 Text Layer Inline Editing
- コンポジットエディタ上で text layer を直接編集する
- caret / selection / IME / commit / cancel を layer モデルに接続する
- 詳細は `docs/planned/MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md`
- Update 2026-08-15: viewport double-click／toolbar／context menu の編集入口、QPlainTextEdit／QTextEdit の IME・selection、commit／Escape cancel、Source Text keyframe、Undo transaction、再描画通知を確認。完全な in-canvas caret／box editing／inspector sync／runtime 受入は未完了・未検証。
- 編集導線の最小入り口は実装済みで、Phase 2 以降の in-canvas input を残す
- `Ctrl+Enter` の commit shortcut を追加し、Phase 1 の確定導線を少し強化した
- 起動時に全文選択するようにして、置き換え入力の初動を軽くした

### M-TXT-1 Text Animator Next Gen
- AE 風 Text Animator の残タスクを UI / selector / preset / timeline まで詰める
- 詳細は `docs/planned/MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md`
- Update 2026-08-15: 個別文書は 2026-08-04 に `MILESTONE_TEXT_ANIMATOR_SYSTEM_2026-03-25.md` へ統合済み。現行コードでは selector／per-glyph evaluation／rendering／JSON／Property Editor／preset の基盤を確認し、専用 UI の操作深度、複雑な shaping 組合せ、大量文字性能、runtime 検証を残課題として扱う。
- 実行メモは親文書へ統合済み

### M-TXT-2 Text Animator Range Color Editing
- テキスト上の範囲選択から直接 color property を割り当てる
- 文字ごとの色変更を、複製 + マスクや expression に逃げずに扱えるようにする
- 詳細は `docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_WORKFLOW_GAPS_2026-06-07.md`
- Update 2026-08-15: `ArtifactTextAnimatorColorEditor` の selection→grapheme range 判定、承認済み `FloatColorPicker`、`applyColorToSelectorRange()`、Animator の `colorEnabled/fillColor`、per-glyph override／JSON 保存を確認。複数範囲の統合、既存色の履歴、timeline 表示、色適用専用 Undo、runtime 受入は未完了・未検証。

### C-TXT-6 GPU Text Rendering / Japanese Shaping
- DX12 / Vulkan backend での日本語 text rendering
- glyph atlas / shaping / backend parity
- 詳細は `ArtifactCore/docs/MILESTONE_GPU_TEXT_RENDERING_JA_2026-04-01.md`
- Update 2026-08-15: Diligent 経由の glyph atlas／GPU quad 配信、`drawGlyphsTransformed()`、CJK fallback、Qt shaping、per-glyph animation の共通経路を確認。`HarfBuzzShapingBackend` は現状 Qt fallback、stroke／AA の backend parity、実機の DX12／Vulkan 受入は未完了・未検証。低レベル backend は変更していない。
- 実行メモは親文書へ統合済み

### Text Workstream Index
- `docs/planned/MILESTONE_TEXT_WORKSTREAM_INDEX_2026-04-30.md`
- Text Animator と GPU Text の入口を 1 枚に束ねる索引

### M-CE-2 Composition Editor Playback Feel Refinement
- playhead / scrub / preview の体感を軽くし、ワープ感や重さを減らす
- 詳細は `docs/planned/MILESTONE_COMPOSITION_EDITOR_PLAYBACK_FEEL_REFINEMENT_2026-04-23.md`
- Update 2026-08-15: 個別文書は 2026-08-04 に `MILESTONE_COMPOSITION_EDITOR_PERFORMANCE_IMPROVEMENT_2026-03-31.md` へ統合済み。現行コードでは preview downsample、render request coalescing、RAM preview state／fallback reason 診断を確認。playhead／scrub の runtime 体感と長時間性能は未検証。

### M-CE-3 Responsive Layout Composition
- `ResponsiveComposition` を別種のコンポとして増やすのではなく、`Composition` に `Responsive Layout` 機能を載せる
- 1つの composition 内に `16:9 / 9:16 / 1:1` の layout variant を持たせ、出力先ごとのレイアウト差分を管理する
- 詳細は `docs/planned/MILESTONE_RESPONSIVE_LAYOUT_COMPOSITION_2026-06-05.md`
- Update 2026-08-15: `ResponsiveLayoutSet`／variant の JSON round-trip、Project View の追加・複製・編集・activate、Composition／Command Palette selector、active summary、Render Queue の output size warning を確認。variant ごとの実レイアウト再配置、同時 Preview Matrix、包括 preflight、safe-area／anchor 適用、runtime parity は未完了・未確認。

### M-CE-CONST-1 Construction Layer
- レンダーされない作業用の設計レイヤーを、composition 内で親子付け・アニメーション可能な形で管理する
- line / circle / grid / annotation / safe area / orbit guide を同じ制作文脈に寄せる
- final render では除外しつつ、editor / timeline では見えるようにする
- Update 2026-08-15: `ArtifactConstructionLayer`、factory／JSON round-trip、Construction property group、GuideSet／SmartGuides、Timeline／Inspector 表示、作成導線を確認。line／circle／annotation／orbit の拡張 item、専用 animation、複数 layer の runtime／再読込 parity は未完了・未確認。
- 詳細は `docs/planned/MILESTONE_CONSTRUCTION_LAYER_2026-06-05.md`

### M-AB Asset Browser Improvement (Unity 風)
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT.md`
- Update 2026-08-15: 個別文書は 2026-08-04 に統合先 `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md` へ supersede。検索／フィルタ、Icon／List、sort、Favorites／Unused／Missing、sequence detection、非同期 thumbnail／waveform、hover preview、breadcrumb、内部D&D、Browser↔Project sync、Undo／Redo は実装済み。TBB並列thumbnail、差分型 imported cache、性能／runtime受入は未確認。
- Phase 0: 左ペイン owner-draw 化の基盤作り
- Phase 1: ビュー切替 & ソート (リストビュー、ソートドロップダウン)
- Phase 2: キーボード操作 & ステータス表示 (矢印/Delete、サムネイルバッジ)
- Phase 3: ナビゲーション & プレビュー (ブレッドクラム、ホバープレビュー、お気に入り)
- Phase 4: 同期 & インスペクタ (Browser↔Project 同期、右パネル)
- Phase 5: 高度な機能 (依存関係追跡、Find References、再リンク)
- Phase 6: 右ペイン owner-draw 拡張を将来検討
- 進捗: 状態バー、Icon/List 切替、Name/Type ソート切替は実装済み、owner-draw へ段階移行中

### M-AB-2 Asset Browser Sequence Grouping
- `image_0001.png` 〜 `image_0100.png` のような連番を 1 アセットとして自動グルーピングする
- 正規表現ベースで basename / frame / padding を検出し、展開可能な論理 item として扱う
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_SEQUENCE_GROUPING_2026-03-31.md`
- Update 2026-08-15: sequence 検出、代表 thumbnail、frame start/count/padding、展開表示、missing／unreadable／size mismatch marker、全 frame path 展開、sequence import／relink／preview 導線を確認。欠落 frame の runtime 一貫性、Project／Timeline identity、Render Queue frame range、実機受入は未検証。

### M-AB-4 Asset Browser Hover Preview
- アセットにホバーすると高品質なプレビューをポップアップ表示
- キャッシュシステム、遅延ローディング、フォールバック機構を実装
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_HOVER_PREVIEW_2026-06-28.md`
- Update 2026-08-15: 300ms遅延、画面端クランプ、画像／動画poster／音声waveform thumbnail、metadata、stale job抑制を確認。専用高解像度cache、popup内の非同期preview、動画再生、sequence専用表示、runtime性能受入は未完了または未検証。

### M-AB-10 Asset Browser Relink Workflow
- 移動/リネームされたアセットファイルの再リンクをサポート
- 依存関係トラッカー、ダイアログUI、アンドゥサポートを実装
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_RELINK_WORKFLOW_2026-06-28.md`
- Update 2026-08-15: 単一／batch relink、候補検索、missing／unused 状態、Undo／Redo、sequence relink、Find References の現行導線を確認。専用参照tracker／references panel、全体参照更新のruntime受入は未完了または未検証。

### M-AB-11 Asset Browser Advanced Sort
- 複数キーによる高度なソート機能を実装
- 自然順序ソート、プリセット保存、設定ダイアログをサポート
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_ADVANCED_SORT_2026-06-28.md`
- Update 2026-08-15: name／date／size／type、natural name、安定tie-break、Type→Name等の固定複合preset、sort key／方向の設定保存を確認。任意multi-keyの個別方向、ユーザー保存preset、custom order／drag sort、大量アセットruntimeは未完了または未検証。

### M-AB-12 Asset Browser Tag System
- アセットにタグ付け機能を追加
- タグデータベース、タグエディタウィジェット、フィルタリング、クラウド表示を実装
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_TAG_SYSTEM_2026-06-28.md`
- Update 2026-08-15: `ArtifactAssetMetaFile`／`MultipleTag` の基盤APIは存在するが、Asset Browser のタグ付与・編集・filter・cloud・project-scoped database への接続は現行コードで未確認。UI実装は metadata 保存形式と Undo 方針を固めてから着手する。

### M-AB-15 Asset Browser AI Support
- AI支援機能をアセットブラウザに統合
- アナライザー、自動タグ付け、類似性検索、レコメンド機能を実装
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_AI_SUPPORT_2026-06-28.md`
- Update 2026-08-15: 共通 AI／MCP 入口と metadata 基盤は確認したが、Asset Browser 専用 analyzer／auto-tag／similarity index／recommendation／自然言語filterは未実装または未接続。状態は Planned のまま。

### M-CP-2 Camera Overlay Experiment ⭐ **新規追加**
- Composition Editor 縺ｧ camera frustum / frame overlay 繧帝∈謚槭〒縺阪ｋ experimental mode
- 2D composition view 縺ｯ縺ｿ縺ｿ螳夂ｾ｡縲・3D editing 縺ｯ縺ゅｊ縺ｪ縺・
- **隧ｳ邏ｰ:** `docs/planned/MILESTONE_CAMERA_OVERLAY_EXPERIMENT_2026-04-02.md`
- Update 2026-08-15: 個別文書は 2026-08-04 に `MILESTONE_CAMERA_PROJECTION_2026-03-31.md` へ supersede。現行 Composition Editor には Camera Frustum toggle、shortcut、設定同期、camera-aware overlay が存在するため、旧項目は独立実装対象ではなく統合先で管理する。

### M-UI-11a UI Theme System Rollout
- `UI Theme System` 繧貞ｫｸ蜿ｳ縺ｮ螳御ｺ・task 繧定ｵｷ繧後ｋ
- `ArtifactInspectorWidget` / `ArtifactPropertyWidget` / `Render Queue` / `Dock` 縺ｮ謨ｴ逅・priorities 繧偵ｃ縺九■繧阪ｋ
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_UI_THEME_SYSTEM_ROLLOUT_2026-04-02.md`
- Update 2026-08-15: 個別文書は 2026-08-04 に `MILESTONE_UI_THEME_MIGRATION.md` へ supersede。現行コードでは DCC theme／QPalette／owner-draw の導入が進んでいるが、全対象 widget の統一と runtime 視認性は統合先で管理する。

### M-PY-2 Script Menu / menu.py Loader
- `Script` menu を固定入口として保ちつつ、`scripts/menu.py` から command を追加できるようにする
- Nuke 風の menu script 拡張を将来の安全な入口として準備する
- 詳細: `docs/planned/MILESTONE_SCRIPT_MENU_PY_LOADER_2026-04-02.md`
- Update 2026-08-15: `ArtifactScriptMenu` の menu.py 雛形生成・編集導線、hooks／macros／CSX 実行は確認したが、PythonEngine と QAction を結ぶ menu registry、addCommand／submenu／separator、reload／失敗診断／設定切替は未実装。状態は部分実装のまま。

### M-PY-3 ExtendScript-Style Script Runtime
- `app / project / selection` を中心にした、アプリ内自動化用の script runtime を作る
- AE ExtendScript 風の操作感で、automation / batch / macro / console 実行を扱えるようにする
- 詳細: `docs/planned/MILESTONE_EXTENDSCRIPT_STYLE_SCRIPT_RUNTIME_2026-04-06.md`
- Phase 1 実行メモは親文書へ統合済み
- Update 2026-08-15: `ScriptRuntime` の parser／VM、JavaScript・ExtendScript style、host snapshot、log／warn／error、file execution、位置付き error は確認。実可変 host API、REPL／console履歴、非同期 cancel／timeout、権限境界、統一 UI 結果表示は未完了または未検証。

### M-TL-13 Timeline Curve Editor Mode
- `ArtifactTimelineWidget` 繧帝ｸ縺､縺ｮ mode 縺ｫ縺吶ｋ縲ゅΝ繝ｼ繝・ヨ timeline / curve editor 繧偵→縺ｪ縺｣縺ｦ縺ｯ縺薙→縺後ｒ謹ｭ縺｣縺励※縺上□縺輔＞
- `U` / `Tab` 繧ｷ繝ｧ繝ｼ繝･縺ｧ playhead / selection / zoom 繧堤舌・縺励※遉ｾ縺ｦ縺薙・繧ｹ繝医Ο繝・ヱ繝ｫ
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md`
- Update 2026-08-15: 個別文書は 2026-08-04 に `MILESTONE_CURVE_EDITOR_DCC_IMPROVEMENTS_2026-07-22.md` へ統合。Tab／graph editor切替、Value／Speed、Bezier、key移動・削除、tangent操作、Undo経路を確認。任意host property／Speed編集／runtime回帰は未完了または未検証。

### M-EAS-1 EasingLab
- Compare easing presets side by side for a selected keyframe segment.
- Keep the first slice read-only, then wire apply through the existing undo path.
- Details: `docs/planned/MILESTONE_EASING_LAB_2026-04-21.md`
- Update 2026-08-15: 個別文書は `MILESTONE_EASING_LAB.md` へ統合済み。6 preset、preview grid、同期scrub、Timeline／Animation menuの起動、選択keyframeへのApply、Undo／Redo経路を確認。数値境界、selection切替、複数composition、未選択時のruntime受入は未検証。
- execution memo は親文書へ統合済み

### M-EXPR-1 Expression System Completeness ⭐ **新規追加**
- エクスプレッションエンジンはパーサー＋評価器が存在するが、AE ライクな表現力に不足がある
- **不足機能:**
  - Pick-whip UI（プロパティ間ドラッグリンク）
  - 組込みプロパティアクセサ（`position`, `opacity`, `rotation` 等）
  - 特殊変数（`thisComp`, `thisLayer`, `thisProperty`, `time`, `value`）
  - ユーティリティ関数（`wiggle()`, `loopIn()/loopOut()`, `pingPong()`, `valueAtTime()`, `speedAtTime()`, `velocityAtTime()`）
  - `effect()` アクセサ
  - エクスプレッションエラー表示（タイムライン上）
  - エクスプレッション→キーフレーム変換
  - 音声リアクティブ変数（`audioLevels`）
- **見積:** 60-80h
- **依存:** `ExpressionParser` ✅, `ExpressionEvaluator` ✅, `ExprIntrinsics` ✅

### M-EXPR-2 Property Reference Linking / Pick Whip
- property 間の参照関係を視覚的に張る
- expression と driven property の target 選択を簡単にする
- 詳細は `docs/planned/MILESTONE_PROPERTY_REFERENCE_LINKING_2026-05-11.md`
- execution memo は親文書へ統合済み
- **現行確認（Update 2026-08-15）**: `PropertyLinkManager` の Direct／Inverse／Scale／Offset／Custom link、source更新時のtarget反映、clear経路を確認。Pick Whip UI、property path／layer identityによる保存復元、expression target選択、循環検出、Inspector／Timeline導線は未実装または未検証。

### M-BLEND-1 Blend Mode Completeness ⭐ **新規追加**
- **監査更新（Update 2026-08-15）**: `ArtifactIRenderer`／`CompositionRenderController` の `LayerBlendPipeline` 通常合成・track matte 接続と、`ArtifactSoftwareImageCompositor` の追加 CPU 分岐を確認。旧記載の GPU／CompositionEditor 未着手は現行コードと不一致。全モード GPU 常設 registry、opacity／alpha semantics、CPU／GPU parity、runtime 受入は未完了。
- 現在 18/38 モード実装。以下のモードを追加する:
  - Dissolve / Dancing Dissolve
  - Linear Burn / Classic Color Burn
  - Linear Dodge / Classic Color Dodge
  - Linear Light / Vivid Light / Pin Light / Hard Mix
  - Classic Difference
  - Divide
  - Stencil Alpha / Stencil Luma / Silhouette Alpha / Silhouette Luma
- CPU (`QPainter::CompositionMode`) と GPU (compute shader) の両方で対応
- **見積:** 20-30h
- **依存:** `ColorBlendMode` ✅, 各 render path
- **現行確認（Update 2026-08-15）**: `BlendMode` enum は追加候補を含む全モードを定義し、`ColorBlendMode` と Software compositor に Linear Burn／Dodge、Vivid／Linear／Pin Light、Hard Mix、Dissolve、Stencil／Silhouette等の分岐を確認。GPU shader／各render pathの恒常接続、Dancing Dissolveの時間挙動、CPU／GPU parity、実機受入れは未完了または未検証。

### M-FX-10 Effects Coverage Expansion ⭐ **新規追加**
- 既存 ~15 エフェクトから AE ライクなエフェクトカタログへ拡充する
- **不足カテゴリ（優先度順）:**
  - **Generate:** Fill, Stroke, Circle, Ellipse, Checkerboard, Gradient Ramp, Grid
  - **Distort:** Displacement Map, Turbulent Displace, Mesh Warp, Liquify, Optics Compensation
  - **Stylize:** Cartoon, Emboss, Find Edges, Mosaic, Brush Strokes, Scatter
  - **Perspective:** Drop Shadow, Radial Shadow, Basic 3D, Bevel Alpha
  - **Transition:** Dissolve, Iris Wipe, Linear Wipe, Card Wipe, Gradient Wipe
  - **Time:** Echo, Time Difference, Posterize Time, CC Force Motion Blur
  - **Utility:** Cineon Converter, Apply Color LUT, Color Profile Converter
- 各エフェクトは CPU reference 実装→GPU compute shader の 2 段階
- **見積:** 120-180h（カテゴリ単位で分割可能）
- **依存:** `EffectStack` ✅, `OFXHost` ✅, `GPUComputeContext` ✅
- **現行確認（Update 2026-08-15）**: 現行 `Artifact/src/Effects` には Fill／GradientRamp／Stroke、DisplacementMap／TurbulentDisplace／Liquify／OpticsCompensation、Mosaic／FindEdges／Emboss、DropShadow／RadialShadow／Bevel、LinearWipe、Echo／PosterizeTime、Color／LUT系など、当初の不足カテゴリに対応するCPU effect実装が多数存在する。全catalog factory／Inspector接続、各effectのGPU compute equivalent、CPU referenceとのparity、runtime性能は未完了または未検証。

## Good Small Tasks

- `M-AR-2 import std Rollout`
- `M-UI-2 Dock / Tab Polish`
- `M-QA-1 Software Test Windows`
- `M-FX-2 Solid Color Effects`
- `M-FX-4 Creative Workflow (Bridge only)`
- `M-PS-1 AE Utility Script Pack`
  - `Quick Rename Layers` / `Clean Layers` / `Trim Comp to Content` をまとめた AE 風の小型自動化群

## Next Recommended

- `M-REACTIVE-1 Reactive Events Engine`
  - `ReactiveEvents` はデータモデルが揃っているので、次は `evaluate()` とリアクション実行の最短経路を閉じる
  - まずは layer property callback と EventBus 送出までに絞り、UI 連携は後段へ分ける
  - 詳細: `docs/analysis/REACTIVE_EVENTS_ENGINE_DESIGN_2026-07-25.md`

### Legacy Note: Timeline Curve Editor Mode
- `ArtifactTimelineWidget` 縺ｧ normal timeline / curve editor 繧偵→縺ｪ縺｣縺ｦ縺ｯ縺薙→縺後ｒ謹ｭ縺｣縺励※縺上□縺輔＞
- `U` 繧ｷ繝ｧ繝ｼ繝･縺ｧ mode toggle, `Tab` / `Shift+Tab` 縺ｧ curve editor 内 focus traversal
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md`

## Timeline / Layer (new)

### M-TM-1 Track Matte Drag-Link UX
- レイヤーパネルから Alt + ドラッグでトラックマット受け側レイヤーを指定する UI
- Inspector の Matte セクション強化（MatteType 即切替 / 参照表示）
- ドラッグ中のハイライトインジケータ、循環参照拒否、Undo/Redo 対応
- 詳細: `docs/planned/MILESTONE_TRACK_MATTE_DRAG_LINK_UX_2026-06-01.md`
- Update 2026-08-15: Alt-drag、matte slot／layer drop、target highlight／tooltip、self／cycle拒否、source置換、Inspector badge／MatteType切替、Undo／Redoの現行導線を確認。削除・移動後のdangling reference掃除とruntime視認性は未完了または未検証。

### M-TA-2 Timeline Audio Waveform Display
- タイムラインの Audio Layer 行で波形(peak min/max)をトラック内に描画
- ズームに応じた粗密制御、フェードハンドル/音量オートメーション keyframe 可視化
- 波形クリックでの seek、trim/gain/fade のドラッグ編集、Undo 対応
- 完了: `docs/done/MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md`

## Multi-Viewport / Preview

### M-VP-1 Multi-Viewport Layout System
- Single / HorizontalSplit / Four-Up / OnePlusThree レイアウト切替 API
- 各ペインに任意の Camera Layer (Perspective / Orthographic: Top/Front/Left) をバインド
- ペインごとの独立 Zoom/Pan 状態保持、playhead の同期更新
- EventBus でのペインイベント multicast、非アクティブペイン低Hz ポーリングによる最適化
- 詳細: `docs/planned/MILESTONE_MULTI_VIEWPORT_LAYOUT_2026-06-01.md`
- Update 2026-08-15: 現行コードで Single／Two-Up／Four-Up、paneごとのcontroller・orientation／zoom／pan基盤、active pane切替を確認。OnePlusThree、Camera Layer assignment／保存、authoritative viewport、EventBus multicast、非アクティブ低Hz最適化、runtime性能は未実装または未検証。

### M-VP-9 Viewport Interaction / Navigation / 3D Cursor
- C4D 的な Point of Interest navigation、Frame Selected、View Undo / Redo、軽量 HUD を共通操作にする
- Blender 的な 3D Cursor / Work Cursor を、pivot、orientation、生成位置、snap の共通基準点にする
- preview-only view と render camera、3D Cursor と object pivot を明確に分離する
- 詳細: `docs/planned/MILESTONE_VIEWPORT_INTERACTION_NAVIGATION_CURSOR_2026-07-04.md`
- Update 2026-08-15: 個別文書は `MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md` へ統合済み。現行コードでは 3D gizmo／snap／projection-aware操作、viewport orientation／active pane、Frame Selected、active pane単位のView Undo／Redoを確認。共通3D Cursor、POI、render cameraとの契約、runtime受入は未完了または未確認。

### M-VP-4 Viewport Canvas Rotation System
- After Effects風のキャンバス回転機能（任意角度 -180°〜+180°）
- マウスジェスチャー（Shift+ドラッグ）で回転、Rキーでリセット
- 回転中心はキャンバス中央、状態はプロジェクトに保存
- `ViewportTransformer` に回転フィールドと座標変換ロジックを追加
- `ViewportCB` に回転情報（ラジアン）を追加
- 詳細: `docs/planned/MILESTONE_VIEWPORT_CANVAS_ROTATION_2026-06-27.md`
- Update 2026-08-15: `ArtifactCompositionRenderWidget` に Shift-drag 回転、中心回りの角度計算、Shift／Alt／Ctrl snap、Rotation HUD、renderer state を確認。project保存、Rリセット、座標変換全体との整合、runtime受入は未完了または未検証。旧文書は `MILESTONE_VIEWPORT_BOOKMARKS_2026-06-27.md` へ統合済み。

### M-VP-5 Viewport Dynamic Resolution Switching
- 表示解像度をリアルタイムで切り替え（25%/50%/75%/100%/150%/200%/カスタム）
- Ctrl+ホイールで解像度変更、メニューからのプリセット選択
- DPR（Device Pixel Ratio）との適切な連動
- `ViewportTransformer` に解像度スケールとDPRフィールドを追加
- レンダーターゲットの再作成に解像度を考慮
- 詳細: `docs/planned/MILESTONE_VIEWPORT_DYNAMIC_RESOLUTION_2026-06-27.md`
- Update 2026-08-15: Full／Half／Quarter preset、操作中downsample、controllerのresolutionScale、resize時DPR更新を確認。25〜200%／custom選択、Ctrl-wheel、DPRとの完全な分離、負荷連動auto scale、runtime受入は未完了または未検証。旧文書は `MILESTONE_VIEWPORT_BOOKMARKS_2026-06-27.md` へ統合済み。

### M-VP-8 Viewport Bookmarks System
- ビューポート状態（ズーム・パン・回転・解像度）をブックマークとして保存/復元
- 1-9キーでブックマーク適用、Ctrl+1-9で現在の状態を保存
- `ViewportBookmarkManager` シングルトンによる集中管理
- プロジェクトごとの保存、名前付けと整理、削除と並べ替え
- 詳細: `docs/planned/MILESTONE_VIEWPORT_BOOKMARKS_2026-06-27.md`
- Update 2026-08-15: `ViewportBookmarkStore` のcomposition単位CBOR保存／読込、名前付きsave／delete、zoom／pan／rotation／orientationの適用、View menu／Editor button導線を確認。shortcut、解像度状態、並べ替え、runtime受入は未完了または未検証。Ctrl+1〜9は他機能と競合するため未接続。

### M-PQ-1 Proxy Quality Toggle in Preview UI
- Playback Control / Viewer フッターから Draft(1/4) / Preview(1/2) / Full を切替
- quality 切替で render cache invalidation と必要に応じて warm-up 再キャッシュ
- Composition 設定として quality preset 保存（新規作成時に復元）
- 詳細: `docs/planned/MILESTONE_PROXY_QUALITY_TOGGLE_UI_2026-06-01.md`
- Update 2026-08-15: View menu に加えて Composition Viewer footer から Draft／Preview／Final を直接切替可能にした。ProjectService→CompositionRenderController の downsample反映、EventBus同期、settingsのquality text保存、render invalidationも確認。composition-resident保存、HUD、shortcut、warm-up runtimeは未完了または未検証。

### M-VP-9 Viewport Interaction / Navigation / 3D Cursor
- C4D 的な Point of Interest navigation、Frame Selected、View Undo / Redo、軽量 HUD を共通操作にする
- Blender 的な 3D Cursor / Work Cursor を、pivot、orientation、生成位置、snap の共通基準点にする
- preview-only view と render camera、3D Cursor と object pivot を明確に分離する
- 詳細: `docs/planned/MILESTONE_VIEWPORT_INTERACTION_NAVIGATION_CURSOR_2026-07-04.md`

### M-PQ-2 Footage Interpret Safety / Proxy Workflow
- footage interpret の frame rate 変更時に keyframe / time remap への影響を明示する
- proxy の生成と切り替えを 1 つの workflow にまとめる
- Update 2026-08-15: Asset Browser の Interpret Footage preflight／frame-rate・color interpretation適用、VideoLayerの proxy quality/path、ProxyManagerの生成・batch・clear・metadata保存を確認。time-remap影響の包括表示、非同期proxy生成、UI workflow統合、Undo／runtime受入は未完了または未検証。
- 詳細は `docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_WORKFLOW_GAPS_2026-06-07.md`

### M-MASK-2 Mask Feather Directional / Render FPS Safety
- mask feather を horizontal / vertical / inner / outer に分ける
- export / render の frame rate 初期値を composition に同期する
- 詳細は `docs/planned/MILESTONE_MASK_FEATHER_DIRECTIONAL_AND_RENDER_FPS_SAFETY_2026-06-07.md`
- Update 2026-08-15: `MaskPath` の horizontal／vertical／inner／outer feather、補間／保存／CPU合成を確認。Render Queue の composition FPS mismatch warning と fallback は確認したが、export初期値の一貫した同期、directional UI、runtime／export受入は未完了または未検証。

### M-3D-2 3D Viewport Orbit / Pan / Preview Mode
- `Alt + Left Drag` orbit / `Middle Drag` pan / wheel zoom を 3D viewport の共通操作にする
- camera を直接動かすモードと preview-only mode を分離する
- 詳細は `docs/planned/MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md`
- Update 2026-08-15: `Artifact3DModelViewer` の Alt+左ドラッグ orbit、中央ドラッグ pan、wheel zoom、Preview Orbit の一時 camera state、Preview HUD／badgeを確認。Composition Editorとの入力契約統一、camera非破壊性、runtime受入は未完了または未検証。旧文書は `MILESTONE_MAYA_VIEWPORT_OPERATIONS_2026-03-25.md` へ統合済み。

### M-TL-2 Scrub Accuracy / Expression Recursion / Cache Reuse
- フレーム単位の scrub で特定 frame を飛ばさない
- expression に安全な再帰と loop を追加する
- render / export で frame cache を共有する
- 詳細は `docs/planned/MILESTONE_SCRUB_EXPRESSION_CACHE_REUSE_2026-06-07.md`
- Update 2026-08-15: ImageSequenceSourceのmutex付きLRU／byte budget／generation invalidation、RAM preview／render queue cache、scrub requested／committed経路とdiagnosticsの一部を確認。scrub truth統一、expression recursion policy、export-wide共通cache、miss理由の一貫表示は未完了または未検証。

### M-EXPR-2 Expression Subframe / Timestep Policy ⚠️ (Phase 1-3 done, 2026-07-30)
- expression の評価を frame locked だけに固定せず、subframe / adaptive step を選べるようにする
- 30fps / 60fps で物理系式の挙動が変わりにくい評価ポリシーを作る
- Phase 1 (Time Evaluation Contract) ✅: EvaluationMode enum, frameRate 変数, evaluateAtTime
- Phase 2 (Subframe Sampling) ✅: 任意時刻評価, evaluateOverRange (4 mode対応)
- Phase 3 (Adaptive Physics Step) ✅: 速度ベースの分割、半ステップ誤差推定、分割回数診断
- Phase 4-5: UI の tradeoff controls、FixedMicrostep の利用導線、profiling／diagnostics UI、30/60fps 実機比較は未完了
- 詳細は `docs/planned/MILESTONE_EXPRESSION_SUBFRAME_TIMESTEP_POLICY_2026-06-07.md`
- Update 2026-08-15: `ExpressionEvaluator` の4評価モード、任意時刻／range評価、adaptive tolerance・min／max step、split count診断を確認。property単位設定、通常property／preview／render queueへのmode伝播、profiling UI、30／60fps runtime parityは未完了または未検証。

### M-UI-24 UI Layout Undo History
- panel close / tab move / split / dock rearrange を undoable にする
- 間違えて閉じた UI を `Ctrl+Z` で戻せるようにする
- 詳細は `docs/planned/MILESTONE_UI_LAYOUT_UNDO_HISTORY_2026-06-07.md`
- Update 2026-08-15: UiLayoutState の JSON／Settings保存・起動復元、dock show/hide／floating／default reset の LayoutSnapshotCommand／Undo経路を確認。全tab reorder／split／detachの粒度統一、broken layout recovery、Ctrl+Z／Redo runtime受入は未完了または未検証。

### M-UI-25 Context Menu Compact Actions
- 右クリックメニューを frequent / all に分けて、初期表示を 10 項目以内に抑える
- よく使う項目のカスタマイズとカテゴリ分けを導入する
- 詳細は `docs/planned/MILESTONE_CONTEXT_MENU_COMPACT_ACTIONS_2026-06-07.md`
- Update 2026-08-15: Layer Panel／Asset Browser の Frequent／All、カテゴリ submenu、主要操作の整理を確認。pin／unpin、recent usage sort、type-ahead検索、折りたたみ保存、全surface共通Menu Registryは未実装または未検証。

### M-UI-26 Numeric Field Quick Calc
- 数値フィールドで `+10` / `-5` / `*2` / `/3` の簡易計算式を受け付ける
- Enter 確定で計算結果を反映し、数値の再入力を減らす
- 詳細は `docs/done/MILESTONE_NUMERIC_FIELD_QUICK_CALC_2026-06-07.md`
- Update 2026-08-15: `ArtifactPropertyEditorNumeric` の float／integer 共通 editor に相対計算 parser を追加し、Enter／editingFinished で既存 commit・preview／Undo 経路へ接続。除算ゼロは無効化し、結果は hard range 内へ clamp する。

### M-TL-3 Keyframe Nudge / Temp Snap Override ✅ 完了
- 完了: `docs/done/MILESTONE_KEYFRAME_NUDGE_AND_TEMP_SNAP_OVERRIDE_2026-06-07.md`

### M-CE-4 Aspect Ratio / Resolution Remap Wizard ⚠️ (Phase 3 done 2026-06-08)
- aspect ratio 変更時に mask / keyframe / anchor を自動再計算する
- `Center Locked` / `Top Left Locked` / `Stretch To Fit` などの保持基準を選べる
- Phase 1 (Preflight) ✅: 差分表示, impact 列挙, 警告表示
- Phase 2 (Policy + Remap) ✅: ResolutionRemap utility, ウィザード, mask 頂点 remap
- Phase 3 (Preview + Apply + Undo) ✅: アスペクト比プレビュー, アンカー検出有効化, 影響表示
  - 残: runtime undo/redo確認、実素材でのプロパティ／アンカー keyframe remap 検証
- 詳細は `docs/planned/MILESTONE_ASPECT_RATIO_RESOLUTION_REMAP_WIZARD_2026-06-07.md`
- Update 2026-08-15: ResolutionRemapDialog、5 policy、impact summary、mask path／transform keyframe／anchor／scale remap、Project／Composition入口、Undo commandを確認。pixel aspect ratio、全データ型の影響一覧、preview／実結果 parity、runtime Undo／再生回帰は未完了または未検証。

## Motion Graphics

### M-MG-1 Motion Graphics Template System (mogrt-like)
- `ArtifactTemplateDocument` — exposedParams 定義 + layer tree + keyframe snapshot
- Export / Import (.artemplate) — 選択レイヤー郡からテンプレート抽出・再配置
- Inspector に Template Parameters セクション（Scalar/Point/Color/Text/Enum）
- Template Library Browser（カテゴリ/タグ/サムネイル / DnD 配置）
- .mogrt 互換読込の option（unzip → JSON ヘッダ + layer tree 抽出）
- 詳細: `docs/planned/MILESTONE_MOTION_GRAPHICS_TEMPLATE_2026-06-01.md`
- Update 2026-08-15: `TemplateSlot`／`TemplateLock`／公開プロパティ接続に加え、`ArtifactTemplateDocument` の JSON 保存・読込契約、layer snapshot／`masterProperties` 抽出、`fromLayers()`、`instantiateLayers()`、`appendToComposition()`、Inspector の Template 専用 tab、`.artemplate` を保存・列挙・読込・削除する `ArtifactTemplateLibrary`、一覧更新と選択読込を行う `ArtifactTemplateLibraryWidget` を実装。Asset Browser の Assets／Templates タブからライブラリへ接続し、テンプレート項目のダブルクリックで現在 Composition へ layer を追加し、`AddLayerCommand` 経由で Undo 履歴へ登録できるようにした。テンプレート一覧から `application/x-artifact-template` を出し、Composition Editor 側で受けて Undo 付き配置する DnD 導線も追加。mogrt互換読込は未実装または未確認。

## Terminal / Shell

### M-UI-24 Terminal Shell / Command Surface
- debug console とは別の、power user 向けの command terminal surface を用意する
- `PowerShellWidget` を使って command / history / working dir / exit code を扱う
- 詳細は `docs/planned/MILESTONE_TERMINAL_SHELL_2026-04-06.md`
- Update 2026-08-15: PowerShell／shの非同期QProcess実行に Stop／Clear、working directory、exit code／終了状態表示、上下キーによるhistory再利用を追加。merged output、二重起動抑止、終了通知も確認。stderr分離、profile／環境変数、history検索、PTY session、dock／menu統合は未実装または未検証。


### M-RD-14 VideoLayer Playback Stability
- `ArtifactVideoLayer` の play → stop が不安定。非同期デコードパイプラインに stop / cancel / reset が存在しない
- `stop()` 新設、デコード世代管理（generation counter）、`seekToFrame()` と `decodeCurrentFrame()` の同期、`currentFrameImageBuffer()` のリファクタ
- 詳細は `docs/done/MILESTONE_VIDEO_LAYER_PLAYBACK_STABILITY_2026-06-25.md`
- Update 2026-08-15: 完了文書と現行コードで stop／cancelPendingDecode、generation invalidation、seek前cancel、stale completion排除を確認。roadmap上は完了、専用unit coverageとruntime受入のみ追跡対象。


















