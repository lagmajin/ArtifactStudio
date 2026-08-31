**最終更新:** 2026-08-31
**ステータス:** In Progress

# DCC操作信頼性マイルストーン (2026-08-30)

## 目的

ArtifactStudioをハイエンドDCCとして成立させるため、機能追加より先に、選択・フォーカス・編集・Undo/Redo・保存／再読込・再生の一連の操作を予測可能で壊れにくい状態へ揃える。

個別機能の追加ではなく、Composition、Timeline、Project、Asset Browser、Inspector、Components／Effects専用面の間で「現在何を編集しているか」が一貫して伝わることを最終目標とする。

## 背景と確認済みの関連項目

- `docs/planned/MILESTONE_COMPOSITION_EDITOR_2026-03-21.md` は、viewport、timeline bridge、selection、preview parityの実ランタイム確認を継続課題としている。
- `docs/planned/MILESTONE_EDIT_MENU_2026-03-13.md` は、command handler、Undo、selection、timeline連携を継続課題としている。
- `docs/planned/MILESTONE_FILE_MENU_2026-03-13.md` は、dirty-state guard、recent project reopen、failure feedbackを継続課題としている。
- `docs/planned/MILESTONE_APP_UX_AND_CORE_REFINEMENT_2026-03-17.md` は、UI rendering stability、UX responsiveness、runtime verificationを継続課題としている。
- `docs/planned/IMPLEMENTATION_PLAN_VIEWPORT_PANE_MANAGER_2026-06-28.md` は、focus任せのactive pane管理、overlay、gizmo、drag責務の分散を課題としている。
- `docs/planned/MILESTONES_BACKLOG.md` には、全actionのcommand owner、selection/current/active stateの正本統一、status／feedback語彙、runtime受入が未完了と記録されている。

これらは別々の機能課題に見えるが、ユーザー視点では「操作が効かない」「別の対象を編集した」「戻せない」「状態が保存されない」という同じ信頼性問題として現れる。

## 原則

1. すべての編集操作は対象、変更前後、Undo単位、失敗結果を明示できる。
2. focusは入力先であり、active object／active paneの正本ではない。編集対象は明示的な状態として管理する。
3. クリック、ドラッグ、数値入力、ショートカット、メニュー、専用エディタは同じ操作結果へ収束させる。
4. キャンセルは変更を残さず、途中状態を次の操作へ漏らさない。
5. 保存／再読込後も選択・current composition・layer order・time range・主要プロパティが破綻しない。
6. 実ランタイム受入を完了条件に含める。ソース上の実装済みだけでは完了としない。

## 実施フェーズ

### OR-1 操作状態の正本と観測

- current composition、selected layers、active layer、active pane、編集モードを一覧化する。
- Viewport、Timeline、Project、Inspector間で状態が変わる入口と反映先を記録する。
- 状態不一致時に、対象・入力コンテキスト・直前操作・Undo stack状態を診断できる最小限のログまたはdebug surfaceを定義する。

### OR-2 編集トランザクションとUndo/Redo

- クリック編集、ドラッグ編集、連続数値入力、複数選択、一括操作のUndo単位を統一する。
- no-op、キャンセル、失敗した操作がUndo履歴へノイズを残さないことを確認する。
- Undo後の選択、current frame、表示範囲、dirty stateを復元する。

### OR-3 フォーカスと入力コンテキスト

- `ArtifactCompositionRenderWidget`、`ArtifactTimelineWidget`、`ArtifactLayerPanelWidget`、`ArtifactPropertyWidget`の入力責務を明確化する。
- `G`、`R`、`S`、軸拘束、`Esc`、`Enter`、右クリックキャンセルなどの基本操作をcontext別に検証する。
- キーボードフォーカスだけでactive paneや対象が暗黙に変わらないことを確認する。

### OR-4 制作シナリオ受入

次のシナリオを、各主要レイヤー種別で反復できる受入表にする。

1. 作成 → 自動選択 → 名前変更 → 保存 → 再読込
2. 複数選択 → 移動／回転／スケール → Undo → Redo
3. Timelineで範囲変更 → keyframe編集 → frame移動 → Undo
4. Project／Asset Browserで選択 → Compositionへ反映 → Inspectorで編集
5. 専用Components／Effects面で変更 → 通常Property面との表示整合を確認
6. 編集中にキャンセル、別対象選択、別pane移動を行い、対象の漏れや残留状態がないことを確認
7. 再生、停止、スクラブ、保存を組み合わせてもUIが無反応または状態不一致にならないことを確認

### OR-5 安定性ゲート

- 重大な操作経路に対して、再現手順、期待状態、復旧操作を記録する。
- crash、無反応、誤対象編集、Undo不能、保存後の状態欠落をリリース阻害バグとして分類する。
- ビルド・テスト・runtime検証を実行できる段階で、静的確認と実行確認を分離して受入記録を残す。

## 完了条件

- 主要surfaceのcurrent／selection／active状態の正本と所有者が文書化されている。
- 主要編集操作が、成功・no-op・キャンセル・失敗の4状態で一貫した結果を返す。
- 主要制作シナリオでUndo/Redo、保存／再読込、focus移動が成立する。
- 操作失敗時に、ユーザーが次に取るべき復旧操作を把握できる。
- 未確認項目を「実装済み」と混同せず、runtime受入結果がマイルストーンへ記録されている。

## 優先順位

1. 選択・フォーカス・active paneの整合性
2. Undo/Redoとキャンセル
3. Viewport／Timeline／Inspectorの同期
4. 保存／再読込とdirty state
5. 数値入力・ドラッグ・ショートカットの一貫性
6. 再生中の応答性とエラーからの復旧

## 影響範囲と注意

このマイルストーンはReactiveEventsの変更を要求しない。既存のcommand、selection manager、workspace／editor controller、診断surfaceを優先的に再利用し、新しいグローバルイベント配線や新規シグナル／スロットを導入しない。

実装着手時は、まずOR-1の状態マップとOR-4の受入表を作成し、再現性の高い2〜3経路から小さく修正する。全UIの一括改修や、未確認の操作を推測で統一することは避ける。

## 未検証事項

- 実ランタイムでの全シナリオの反復結果
- 複数選択、連続ドラッグ、フォーカス移動を含むUndoの完全な一貫性
- 保存／再読込後の全レイヤー種別における選択・時間範囲・専用面状態の復元
- 無反応、クラッシュ、表示崩れの発生頻度と優先度

## 2026-08-30 Update — OR-1 state map started

`docs/analysis/OPERATION_STATE_MAP_2026-08-30.md` に、current composition、active composition、playback composition、selected layers、current layer、input context の現行所有者と同期入口を記録した。`WorkspaceAutomation::workspaceDiagnostics()` の `operationState` に3つのcomposition ID、selection、input context、一致判定を追加し、不一致時は `COMPOSITION_STATE_MISMATCH` を既存 warning surface へ返す。新しいイベント配線や状態変更は行っていない。build / runtime 受入は未実施である。

OR-2 は、Particle drag、layer transform drag、keyframe edit、slide / ripple editの4経路で既存commandのsnapshot・macro・no-op抑止を静的確認した。実機でのselection / current frame / cache / cancelを含む受入は未実施である。

OR-3 は、Viewport、Timeline、Layer Panel、Inspectorに加えて `ArtifactPropertyWidget` が `Panel.Properties` input context を設定・解除するようにした。新規signal/slotは追加せず、focusでproperty値やselectionを変更しない。子editorへfocusが移った場合を含むshortcut優先順位はruntimeで確認する。

Property 面の focus-out は、次のフォーカス先が同じ面の子孫である間は `Panel.Properties` を維持するようにした。これにより、数値欄・検索欄などの子 editor への移動で shortcut context が `Global` に落ちる静的な隙間を塞いだ。別 surface への移動、Escape、編集値、selection の runtime 受入は未実施である。

`ArtifactProjectService::selectLayer()` の無効な layer ID 処理では、selection をクリアした後も要求された無効IDを `LayerSelectionChangedEvent` に載せていたため、受信側が失敗対象を現在レイヤーとして保持し得た。解決済みの current layer がない場合は空の `LayerID` を通知するようにし、失敗状態と通知内容を一致させた。runtimeでの異なるcomposition・Inspector・selection bridge受入は未実施である。

同じ `LayerID` が別 composition に存在する場合の早期再選択も、現在 composition 内の存在確認を通った場合だけ成立するようにした。composition境界を越えた stale な current layer を「再選択済み」と扱わず、通常の解決・失敗経路へ戻す。

保存確認付きの新規作成／プロジェクト切替／終了／再起動では、保存失敗時に操作を中止していたが、失敗理由を表示していなかった。`ArtifactProjectExporterResult::errorMessage` を既存の File Menu 確認経路から表示し、保存先確認や再試行へつながる復旧手がかりを追加した。新規イベント配線は行っていない。

Timeline の `ArtifactTimelineScrubBar` は、既存の WorkArea と異なり、ruler の pixels-per-frame と horizontal offset の公開 setter で負数・非有限値を保持し得た。両値を有限かつ 0 以上へ正規化し、zoom／navigator 同期の入力契約を揃えた。通常の同期経路、frame mapping、cache bitmap の扱いは変更していない。

Timeline の playhead setter 群も、非有限値を `std::clamp` だけでは除外できず、`setCurrentFrameForAll()` の整数変換前に異常値が残り得た。Timeline、ScrubBar、Navigator、WorkArea の各入口で有限値を確認し、frame の共有経路では安全な int 上限、Navigator では `totalFrames - 1` 上限と total-frame 短縮時の再クランプも適用した。WorkArea の total frames／FPS／ruler mapping も同じ有限・非負契約へ揃えた。既存のフレーム範囲クランプと表示責務は維持している。

既存の `LayerSelectionChangedEvent` と `CurrentCompositionChangedEvent` の購読を使い、custom `ArtifactStatusBar` に選択数を常設表示する `Selection` item を追加した。新しい signal／slot は追加せず、既存の selection manager の選択集合を表示するだけに限定している。composition 切替時は空の状態を明示的に 0 へ戻す。

composition 切替時の FPS 反映も、`double` の composition FPS を直接 `int` へキャストせず、有限・正値・上限付きで丸めて ScrubBar／timecode へ渡すようにした。Timeline の keyframe 編集側に散在していた同じ FPS scale 変換も helper へ統一した。既存の整数 FPS 表示契約と、異常値時の 30 FPS フォールバックは維持している。

`ArtifactStatusBar` の既存 `setZoomPercent()` / `setFPS()` も、診断値 API の境界で非有限値を直接表示し得たため、有限値確認と安全な範囲正規化を追加した。通常の AppMain 更新経路の表示形式と status item 配置は維持している。

`ArtifactProjectManager::loadFromFileAsync()` は複数のロード要求を識別していなかったため、後発要求の後に先発の遅い importer が完了すると古いプロジェクトを適用し得た。新規ロード要求、同期ロード、新規作成、クローズで進む operation generation を導入し、最新要求と一致する結果だけを main-thread apply するようにした。古い結果の callback は UI状態を巻き戻さないため通知せず破棄する。

`WorkspaceAutomation::workspaceDiagnostics()` は C++ Automation と AI UI から利用できる一方、既存の `artifact.workspace` Python bridge には登録されていなかった。read-only の同じ結果を返す Python method を登録し、Python hook／外部自動化から composition state mismatch、selection、input context を観測できるようにした。Python engine の実行と実際の不一致検知は未検証である。

Automation の `agentContract()`、`commandVocabulary()`、`selectionSnapshot()`、`renderQueueSnapshot()` と、既存の `get_*` read-only alias も同じ Python bridge へ追加した。Python 側の discovery／post-operation verification を C++／AI UI と同じ観測契約へ揃え、write API の公開範囲は広げていない。

Python bridge と `WorkspaceAutomation::methodDescriptions()` を比較し、未公開だった read-only／validation surface を追加した。viewport 設定、project item／layer／effect／audio 情報、render queue job 照会、export format、effect preset、playback read-only 状態、`validateCommand`／`validateViewportSettings`、各種 remove の dry-run／confirmation message を既存のcompact JSON bridgeへ登録し、実際の書き込み操作の新規公開やイベント配線は行っていない。C++ method description 220件に対しPython登録97件となり、今回選定したread-only／dry-run候補の未登録は0件である。

`saveToFileAsync()` も保存中に project 切替や別保存要求が起きた場合、完了時に現在 project の path／dirty を古い保存結果で更新し得た。保存要求と同期保存でも同じ operation generation を進め、完了時に世代と `ArtifactProjectPtr` の両方を確認してから現在 project を更新するようにした。

ウィンドウ closeEvent は従来、一般的な終了確認だけで未保存変更を確認していなかった。既存 File Menu と同じ save／discard／cancel の保存確認を closeEvent に追加し、File Menu の終了・再起動が先に同じ確認を通った場合は QObject property で二重表示を避けるようにした。

Timeline 内部の keyframe 復元、時間シフト、pattern／preset 生成、curve editor、playback visual、work area 同期にも FPS の有限値境界を適用した。`std::max` だけでは NaN／無限大を除外できないため、1〜10000 FPS の共通 scale と、pattern の BPM 用 60 FPS fallback を使う。既存の編集責務やイベント経路は変更していない。

同じ数値境界を Property Editor の expression bake、共有 playback time、pre-compose の child work area scale にも適用した。いずれも FPS を `RationalTime`／整数 scale に変換する既存経路であり、無効値は 30 FPS（通常 UI）または従来の最小 scale（pre-compose）へ戻す。render path や新規イベント配線は変更していない。

Layer Panel と Playback Control の timecode／current-time 表示でも、再生サービスから受け取った FPS を有限・正値・上限付きにしてから `RationalTime` とフレーム境界へ渡すようにした。通常の 30 FPS fallback と既存の入力・イベント経路は維持している。

Composition Render Widget の preview timer と Composition Editor の text keyframe／SRT・WebVTT timebase でも同じ境界を適用した。NaN／無限大を interval、整数 timebase、`RationalTime` へ渡さず、既存の 16ms／30 FPS fallback を維持している。

Text Gizmo の transform keyframe rate も、正値だけでなく有限値と 10000 FPS 上限を確認するようにした。Diligent の render controller 内部や GPU 経路は今回の数値境界対応では変更していない。

Solid Image／Shape／Audio layer の current-time 評価と Layer Menu の放射状 transform でも、composition FPS を整数 time scale へ変換する前に同じ有限値境界を適用した。ArtifactCore の基盤 `FramePosition`／`FrameRange` 実装と Diligent render controller は変更対象外としている。

`MoveLayerCommand` と Align の Undo/Redo、および Viewport の Center in Comp が `RationalTime(frame, 30000)` を固定使用していた。`AnimatableTransform3D` は時刻を 24fps 基準へ正規化するため、通常の composition frame を 30000 scale で渡すと対象フレームがずれる。

対象レイヤー／composition の実 FPS を有限・正値・1〜10000 の範囲で整数 scale 化し、Move／Align の復元時には既存の Transform dirty flag、dirty reason、LayerChangedEvent、changed 通知も通すようにした。右クリックの Center in Comp も同じ time scale と Transform dirty を使う。Undo/Redo の current frame、複数選択、保存後の再読込、runtime の表示更新は未検証である。

`SetLayerPropertyValueCommand` は従来 `getProperty()->setValue()` だけを呼び、`ArtifactParticleLayer` などの virtual `setLayerPropertyValue()` が持つ実データ更新・cache invalidation・専用通知を迂回していた。Undo／Redoではまず virtual setterを通し、未対応の汎用propertyだけ旧直接更新へフォールバックするようにした。Particle dragと通常Property resetの実機復元は未検証である。

`SetLayerPropertyKeyframesCommand` は永続化可能なcommandとして宣言されていたが、通常コンストラクタで `layerId_` を初期化しておらず、新規作成した keyframe command の `canSerialize()` が常に失敗していた。layer IDを保持し、対象 weak pointer が有効な場合だけserialize可能と判定するようにした。offload／save-reloadでの実機確認は未実施である。

Keyframe snapshotの復元と、virtual setterで扱えない汎用propertyのUndo fallbackも、単なる `changed()` からproperty pathに応じた dirty flag／reasonと既存 `LayerChangedEvent` を伴う通知へ揃えた。Transform／mask／sourceは対応するdirty領域を選び、その他はPropertyとして扱う。cache再生成、runtime表示、保存後の復元は未検証である。

UndoManager の dirty 判定は、従来 push 時の単調増加カウンタだけを比較していたため、保存後の Undo／Redo や Undo 後の分岐編集で履歴位置を正しく識別できなかった。Undo stack と Redo stack に履歴状態 ID を並行保持し、Undo は直前位置、Redo は元の位置、分岐 push は新しい位置へ遷移するようにした。既存の保存履歴 JSON には `currentVersion` を追加し、旧ファイルでは `savedVersion` を fallback として使う。履歴破棄時の実データ復元、履歴保存／再読込、runtime の dirty 表示は未検証である。

`SetLayerPropertyValueCommand` も実際の layer property 編集で使われる一方、command type、layer ID、serialize／deserialize がなく、macro／offload／session history から外れていた。対象 layer の存在と property path を確認し、`QJsonValue::fromVariant()` で表現できる before／after 値だけを永続化する実装と factory 登録を追加した。Particle の専用 setter を通る既存 Undo／Redo動作と、JSON round-trip は未検証である。

serialize可能な command type と `UndoManager` factory の静的照合では、`SetTextAnimatorStackCommand` だけが factory未登録だった。JSONを生成できても load時に commandを再構成できない状態だったため、既存の before／after stack と label を受ける factoryを追加した。全 command type の登録名一致は静的に確認済みで、実際の session reload は未検証である。

通常の `ArtifactPropertyWidget` レイヤープロパティ行と Channel Box は、プレビュー中に選択レイヤーを直接更新した後、確定時も直接 setter を呼ぶだけで、Undo command の before 値を保持していなかった。Property Editor の初回 preview／commit 前に各 target layer の現在値を snapshot し、確定時は既存の `SetLayerPropertyValueCommand` を `MacroUndoCommand` にまとめて1操作として記録する経路を追加した。Opacity の既存 command 経路と effect 面の責務は維持し、通常レイヤーと Channel Box の複数選択にも適用した。

この初回値保持は source localized の commit-only 経路では消費後に破棄し、値編集では command push 後に破棄する。スクラブの Esc／focus-out cancel では既存の復元処理後に snapshot を破棄する callback も通す。Undo／Redo の runtime 表示、auto-key／keyframe mode と通常値 command の組み合わせ、保存／再読込は未検証である。

Property 行のキーフレーム Anchor／Color Label変更、Channel Box の Key All／Key Selected は、変更前後の keyframe sequence を既存 `SetLayerPropertyKeyframesCommand` に渡すようにした。Channel Box の一括キー操作は選択 target ごとの macro として記録する。animatable flag、expression bake、auto-key／keyframe mode と通常値 command の複合境界、runtime 受入は未検証である。

Expression 面の Clear／Convert／Bake も、従来は expression または keyframe を直接書き換えていた。`SetLayerPropertyExpressionCommand` を追加して layer ID と expression before／after を保存可能にし、Clear は単独 command、Convert は keyframe command と expression command の macro、Bake は keyframe command として記録する。sampled keyframe の値型、animatable flag、複数選択、runtime／session reload は未検証である。

Expression Copilot の apply 前に行われていた直接 `setExpression()` を除去し、layer-owned の通常行・参照ドロップ・Property Widget の callback 側で expression command を作るようにした。同値入力は履歴へ積まない。Effect-owned property は既存の直接更新を維持している。Copilot の実操作、式評価エラー表示、session reload は未検証である。

AI／automation の layer・composition setter についても、直接状態を書き換えていた代表経路を既存 Undo command へ接続した。可視・ロック・Solo・Shy・Blend・Opacity、2D Transform、Parent、Layer／Composition Note は変更前後を保持し、位置・スケール・Template Variation の複合操作は macro として一操作にまとめる。同値入力と空の variation は履歴へ積まない。Effect scalar parameter は `SetPropertyCommand`、Effect parameter の keyframe／expression は専用 snapshot command を再利用する。Property Widget の Effect-owned 編集全体と runtime の AI実操作、失敗途中の復旧、保存／再読込は未実施である。

AI の group／solid／noise layer 作成も `AddLayerCommand` の一操作境界へ入り、指定時刻の split、ripple delete、sequential align は timing property command と layer add／remove command を macro 化した。ripple／split の layer membership と timing を同じ履歴へ戻せる。group 階層をまたぐ move／ungroup も親ID・composition orderをmacroで保持するが、selection、runtime cache、session reloadは未検証である。

Property Widget の expression action／Copilot callback では、`propertyName` の表示名一致だけで layer command を生成せず、layer が実際に所有する property pointer と一致する場合だけ `SetLayerPropertyExpressionCommand` を使うようにした。Effect-owned property は専用 effect command が存在しないため、従来の直接更新へ戻し、対象外の no-op layer command が履歴へ残らないようにした。

`SetLayerPropertyKeyframesCommand` の snapshot 復元と JSON codec に Anchor／Color Label を追加し、旧 payload では既定値へフォールバックするようにした。複数選択時の keyframe mirror も target ごとの before／after を macro 化したが、primary layer の toggle command と target mirror の完全な一履歴境界、animatable flag の復元は未検証である。

Project Viewの非Composition項目の改名・フォルダ移動も、IDベースの`RenameProjectItemCommand`／`MoveProjectItemCommand`へ接続した。削除は所有権とComposition／render queue連携を含むため、Undo snapshotを実装するまでsafe-writeの`undoAvailable`をfalseとして扱う。selection、別project切替、session reload、runtime受入は未検証である。

AIのフォルダ作成は、void APIの呼出し後に項目ID集合を比較し、新規Folderの実体・名前・親を確認して成功結果を返すようにした。生成後のUndo、selection、session reload、runtime受入は未検証である。

AIとProject Viewのフォルダ作成は、作成前のID集合から新規Folderを特定し、`CreateProjectFolderCommand`へ接続した。Undoは空フォルダだけを対象にし、Redoは同じID・名前・親・tagを復元する。同名既存フォルダの誤認を避ける。子項目を持つ場合のUndo、selection、session reload、runtime受入は未検証である。

Project Viewの一括改名・フォルダ移動は、対象を先に全件検証し、失敗時はpartial mutationを起こさず、成功時は一つの`MacroUndoCommand`として記録する。同値項目はno-opとして扱う。selection、別project切替、session reload、runtime受入は未検証である。

Project item削除は、CompositionまたはCompositionを含むFolderを除外し、Folder／Footage／Solidのサブツリーのみを`RemoveProjectItemCommand`へ接続した。親ID・兄弟indexとJSON snapshotを保持してUndoで同じ構造と位置を復元し、Undo単一エントリ上限を超える場合は削除前に拒否する。safe-write計画の`undoAvailable`も対象型に合わせる。Composition／render queue復元、selection、session reload、runtime受入は未検証である。

Mask reorderは`MoveMaskCommand`にlayer ID・前後indexのserialization／factoryと、Undo／Redo後のlayer changed通知を追加した。maskの同一性が別操作・session境界で変わる場合、selection、runtime、session reloadは未検証である。

Composition解像度Remapは、既存snapshotを捨てずにcomposition ID・旧／新サイズ・RemapPolicy・mask・transform property・keyframe列をJSON保存し、factory load時にcomposition resolverから再接続するcodecを追加した。許可codec外のkeyframe valueはsession serialization対象から除外する。runtime remap表示、session reload、別composition復元は未検証である。

Timelineのplayhead分割は、AI／ActiveContextとも`splitLayerWithUndo()`へ接続した。ActiveContextのLayer In／Out／Trimも`SetLayerPropertyValueCommand`へ接続し、Trim Inの複合変更はmacro化した。境界外・timing lock・同値入力は変更せず、UndoManager不在時もlayer ID差分で成功判定するが、keyframe retime、selection、current frame、cache、runtime／session reloadは未検証である。

AIのIn／Out point、marker、chapter、marker clearは、PlaybackServiceの共通before／after JSON snapshotへ委譲し、`InOutPointsSnapshotCommand`として記録するようにした。Work Area本体、current frame、cache、runtime／session reloadは未検証である。

共通ProjectServiceのlayer作成は、生成後に適用される初期表示・時刻・selected layer基準の順序・親を含めて、既存のAdd／index／parent commandを`Create Layer` macroへまとめるようにした。UndoManager不在時は従来の直接経路を維持する。作成直後のselection、creation event、runtime cache、session reloadは未検証である。

Work Areaの開始・終了・現在frameへの移動も、`SetCompositionWorkAreaCommand`へ接続し、Undo／Redo時に既存WorkAreaChangedEventとPlayback Engine範囲を同期するようにした。command callbackはsession historyへ保存せず、別composition切替後の履歴、runtime cache、session reloadは未検証である。

通常UI・Timelineのgroup／ungroupは、Undo中にUndo対応済みProjectServiceを再入呼出ししないよう、Add／親変更／Removeを直接構成するmacroへ揃えた。適用後のgroup実体、親ID、group消失、子の親解除を検証し、失敗時は成功を返さない。selection、order、予算超過時の全復元、runtime cache、session reloadは未検証である。

group／ungroupのselectionもbefore／after snapshotへ含め、Undoでは元の複数選択、Redoではgroupまたは解除後の子layerを復元するようにした。別composition切替中のselection bridge、runtime cache、session reloadは未検証である。

Timeline の `KeyframePropertySnapshot` 復元にも、変更前後の `animatable` 状態と Anchor／Color Label を保持・再適用するようにした。復元時は `LayerDirtyFlag::Property` と既存の `LayerChangedEvent` を通し、Timeline の keyframe 操作でも preview cache が古いまま残らない静的経路へ揃えた。runtime の cache／保存／再読込確認は未実施である。

その後、同 command に optional な before／after `animatable` state を追加し、Property 行、Channel Box、Key All／Key Selected、keyframe toggle、複数選択 mirror の新規 command から渡すようにした。通常値 macro の after 値も専用 setter 実行後の実値から採取する。旧 payloadとの互換性を保つため、fieldがない履歴では flagを変更しない。Timeline全経路、Effect-owned property、runtime／session reloadは未検証である。

Effect／layerのモジュレーションsnapshotも、source／assignment／smoothingTimeをsession codecへ接続した。64bit `targetId`のJSON精度落ちを避け、入力検証で不正な参照や非有限値を拒否する。Undo／Redoの音響結果、session reload、runtime cache、別composition復元は未検証である。

Effect modulationの同一snapshot入力はno-opとして扱い、Undo、project dirty、変更通知を発生させないようにした。非有限値の外部入力や音響runtimeは未検証である。

AIのgroup移動／ungroupは、対象ID・自己移動・循環参照・壊れた階層を全件検証してから変更し、移動後の親ID、ungroup後のgroup消失と子の親解除をpostconditionとして確認する。失敗やUndo予算拒否を成功扱いしないが、selection、order、runtime cache、session reloadは未検証である。

AIのscalar transform、note、keyframe、Project Viewの一括rename／moveも、Undo push後の実値・keyframe・parentを確認する。batch macroの部分適用時は履歴Undoまたは旧状態復元へ入り、成功結果と実状態の乖離を減らす。runtime、selection、current frame、session reloadは未検証である。

共通ProjectService／EffectServiceのlayer・effect scalar、状態toggle、parent、audio trim／de-click、effect keyframe／expression、modulationも、Undo予算拒否やcommand適用失敗を成功扱いしない共通postconditionへ揃えた。複製・layer作成・folder作成で実体が先に生成される経路は、command不成立時に元の構造へ戻す。runtime、selection、session reloadは未検証である。
