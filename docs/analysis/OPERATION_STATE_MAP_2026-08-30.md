# Operation State Map (2026-08-30)

**最終更新:** 2026-08-31

## Scope

`MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md` の OR-1 に対応する静的調査。ビルド、テスト、実機操作は未実施である。

## Current State Owners

| User-facing state | Current owner | Evidence | Synchronization boundary | Risk / follow-up |
|---|---|---|---|---|
| Current composition | `ArtifactProjectService::Impl::currentCompositionId_` | `Artifact/src/Service/ArtifactProjectService.cppm` | `changeCurrentComposition()` resolves the ID and forwards the composition to `ArtifactActiveContextService` | The service falls back to `ArtifactProjectManager::currentComposition()` when its stored ID is unavailable; this recovery is implicit. |
| Active composition | `ArtifactActiveContextService::Impl::activeComp_` | `Artifact/src/Application/ActiveContextService.cppm` | `setActiveComposition()` forwards to playback then announces `activeCompositionChanged()` | A composition change can be initiated through ProjectService or ActiveContext; runtime parity between both owners is not verified. |
| Playback composition | `ArtifactPlaybackService::Impl::currentComposition_` | `Artifact/src/Service/ArtifactPlaybackService.cppm` | `setCurrentComposition()` resets preview state, applies frame range/FPS to engine and controller, then publishes `PlaybackCompositionChangedEvent` | This is intentionally a playback owner, not a replacement for ProjectService's current-composition ID. |
| Selected layers | `ArtifactLayerSelectionManager::Impl::selectedLayers_` and ordered selection | `Artifact/src/Layer/ArtifactLayerSelectionManager.cppm` | `ArtifactProjectService::selectLayer()` validates the current composition before selecting and publishes `LayerSelectionChangedEvent` | Selection must be rejected, not silently retargeted, when the requested layer is absent from the current composition. |
| Current layer | `ArtifactLayerSelectionManager::Impl::currentLayer_` | `Artifact/src/Layer/ArtifactLayerSelectionManager.cppm` | Selection manager updates it together with selected layers; edit/menu operations read it through the manager | Current layer is valid only in the active composition; composition-switch recovery needs runtime verification. |
| Input context / active pane | `ArtifactInputOperator::setActiveContext()` callers | `ArtifactCompositionRenderWidget`, `ArtifactTimelineWidget`, `ArtifactLayerPanelWidget`, `ArtifactInspectorWidget`, `ArtifactProjectManagerWidget`, `ArtifactAssetBrowser` | Focus-in/out handlers set a named context and restore `Global` on focus loss | This is an input-routing context, not the canonical active composition or layer selection. |

`ArtifactPropertyWidget` は OR-3 の対象であるにもかかわらず context owner に含まれていなかったため、2026-08-30 に `Panel.Properties` を追加した。親surfaceの focus-in で設定し、同contextが有効な場合のみ focus-out で `Global` に戻す。これは入力routingだけを補うもので、selection、current composition、property値は変更しない。

## Observed Composition Transition

```text
ProjectService.changeCurrentComposition(id)
  -> ProjectService.currentCompositionId_
  -> ActiveContext.setActiveComposition(composition)
  -> PlaybackService.setCurrentComposition(composition)
  -> PlaybackCompositionChangedEvent
```

This is the intended forward direction observed statically. Any other entry point that changes only one of the first three owners should be treated as a reliability risk until verified.

## Initial Acceptance Matrix

| Scenario | Expected invariant | Static evidence | Runtime evidence needed |
|---|---|---|---|
| Change composition from Project surface | Project, ActiveContext, Playback refer to the same composition ID | ProjectService forwards to ActiveContext; ActiveContext forwards to Playback | Open two compositions, switch repeatedly, then use playback and editing commands. |
| Select a layer from Timeline / Viewport / Project | Selection manager's current layer belongs to ProjectService current composition | `selectLayer()` resolves through current composition before mutation | Cross-surface selection, then change composition and use an edit action. |
| Focus Timeline then Viewport | Input context changes, composition and selection do not change merely from focus | Separate context setters in focus handlers | Verify G/R/S/Esc/Enter act on the intended surface and do not retarget selection. |
| Playback composition change | Preview cache and controller frame range match the new composition | PlaybackService resets cache and applies composition settings | Switch composition during playback; verify frame range, CTI, and cache bars. |

## OR-1 Implementation — 2026-08-30

`Artifact.AI.WorkspaceAutomation::workspaceDiagnostics()` に `operationState` を追加した。既存の read-only diagnostics surface を再利用し、次を一つの snapshot で返す。

- ProjectService / ActiveContext / Playback の composition ID
- current layer ID と selected layer count
- 現在の input context
- 3つの composition owner が利用可能か、ID が一致するか

3つの service が利用可能で ID が一致しない場合は、既存の `warnings` / `warningCodes` に `COMPOSITION_STATE_MISMATCH` を追加する。新規の signal、slot、イベント、状態変更はない。実ランタイムでの composition switch と playback 中の確認は未実施である。

## OR-2 Static Undo / Redo Review — 2026-08-30

| Representative operation | Changed state | Undo boundary observed statically | No-op / cancellation behavior | Runtime evidence still needed |
|---|---|---|---|---|
| Particle emitter / effector viewport drag | Particle layer property paths | `SetLayerPropertyValueCommand` を `MacroCommand` へまとめ、drag終了時に1回 push | 開始値との差分が閾値未満なら pushしない | Esc、別layer選択、複数effectorの連続drag後の復元 |
| Layer transform drag | Transform property at current frame | realtime変更を戻してから `MoveLayerCommand` をpushし、redoで再適用 | deltaがなければcommandを生成しない | keyframed transform、複数選択、cache/preview更新 |
| Timeline keyframe add / remove / move / paste | Property keyframe snapshots と選択キー | `TimelineKeyframeSnapshotCommand` がbefore/after snapshot、animatable、Anchor／Color Label、Property dirty、keyframe選択を復元 | 変更されたkeyframeがない場合はpushしない | current frame、scroll範囲、Inspector更新を含むUndo/Redo |
| Timeline slide / ripple edit | Layer timing snapshots | 専用commandまたはmacroで対象layerのbefore snapshotを保持 | 対象／deltaが無効なら実行しない | 複数選択、source境界、保存／再読込 |

この静的確認は、主要経路に既存の Undo infrastructure が存在することを示すだけである。selection、current frame、dirty state、preview cache、およびキャンセルの全組合せは runtime受入で確認するまで未検証とする。

Timeline snapshot の復元は、keyframe 列だけでなく `animatable` と keyframe metadata を再適用し、Property dirty flag と既存変更イベントを通す。これは静的経路の補正であり、runtime の preview cache、current frame、scroll 範囲、Inspector 更新、保存／再読込は未確認である。

AI／automation の代表的な layer／composition setter は、可視・ロック・Solo・Shy・Blend・Opacity、2D Transform、Parent、Layer／Composition Note を既存 Undo command の before／after 境界へ接続した。Template Variation の同一 layer 複数 slot は保留中の Note を追跡して一つの macro にまとめる。Effect scalar parameter と Effect parameter の keyframe／expression も command 化したが、Property Widget の Effect-owned 編集全体の runtime／session history は未検証である。

AI の layer create／split／ripple delete／sequential align も Add／Remove／property commands の macro へ接続した。split は clone を未接続状態で準備し、元 layer の timing変更・追加・index移動を一つの履歴へまとめる。group move／ungroup は複数 layer の hierarchy membership と order を同時に復元する snapshot がまだない。

## 2026-08-30 Hierarchy Undo Correction

上記のgroup move／ungroup記述は、その後のAI automation対応で更新された。group moveは親ID変更を一つのmacroに、ungroupは子の親解除と `RemoveLayerCommand` を一つのmacroにまとめ、既存のcomposition orderを保ったUndo／Redo境界を追加した。選択状態、runtime cache、session reloadは未検証である。

一般レイヤーのmove／remove／duplicateも共通 `ArtifactProjectService` で既存の `MoveLayerIndexCommand`、`RemoveLayerCommand`、`AddLayerCommand`＋index移動macroへ接続した。duplicateは既存サービスが作成・複製した完全なlayerを一度安全にdetachし、commandのredoで再接続する方式で、selectionのUndo復元とruntime／session reloadは未検証である。

AIの一般 keyframe API (`setKeyframe`／`batchSetKeyframes`／`deleteKeyframe`) も propertyごとの before／after keyframe snapshot と animatable flag を `SetLayerPropertyKeyframesCommand` に渡す経路へ統一した。異なるFPSの単発 keyframe は compositionの frame rateを保持し、runtime／session reloadは未検証である。

CommandIR経由の `set_keyframes`／`batch_set_keyframes` も同じ `SetLayerPropertyKeyframesCommand` と共通のkeyframe snapshot変換へ接続した。モーションスケッチ／自動向き生成は有効サンプルを一括投入し、サンプル単位のUndo分裂を避ける。旧payloadのtimeScale省略は30fps fallbackを維持し、runtime／session reloadは未検証である。

AIのeffect複製も、直接追加ではなく既存の `addEffectToLayerWithUndo()` を通すようにした。複製元の全keyframe／expressionをコピーする仕様ではなく、runtime／session reloadは未検証である。

外部 `CommandIRExecutor` の keyframe実行も、各キーを個別に呼ぶ経路から `WorkspaceAutomation::batchSetKeyframes()` 一回へ集約し、mutation前の全件検証と `CommandResult.valid` の補正を追加した。property欠落や不正payloadは先に拒否するが、runtimeでのpartial failure／cache受入は未検証である。

AIの音声編集では、trim in／out、playback rate、de-click設定を既存の layer property command／macroへ接続し、de-click範囲は専用のbefore／after snapshot commandへ接続した。範囲は正規化・マージされ、音声レイヤー側でresampled cacheを無効化する。通常の音声サービス呼出し、Undo／Redo、session reload、cache更新のruntime受入は未検証である。

AI／共通ProjectServiceのlayer renameも、既存の`RenameLayerCommand`へ接続した。同値名はno-opとし、直接改名はUndoManager不在時のfallbackだけに残した。selection、project item表示、session reloadのruntime受入は未検証である。

AIの画像／SVG／音声／Text／Null作成別名は、既存のProjectService作成責務を維持したまま、前後のactive compositionを比較して新規layer IDを結果へ返すようにした。サービス側の作成失敗やactive composition変更を成功扱いしない。作成Undo、selection、runtime／session reloadは未検証である。

composition renameはcomposition本体とProject Viewの`CompositionItem`名を同じ`RenameCompositionCommand`で更新するようにした。コマンドファクトリにも登録し、履歴シリアライズ後にcomposition resolverで再接続できる境界に揃えた。同値名はno-opとし、UndoManager不在時だけ直接適用する。別projectへ切り替えた後の履歴復元、current composition、Project View選択、session reloadは未検証である。

通常のProjectServiceから呼ばれる表示・ロック・Solo・Shy変更も、既存の状態commandへ接続した。Solo Only／Smart Soloは各レイヤーのbefore／afterをmacro childとして保持し、親付け／親解除は`ChangeLayerParentCommand`で親IDを復元する。親レイヤー削除と同時の復元、selection、runtime cache、session reloadは未検証である。

Undo付きsplitは、フレーム範囲・timing lock・project存在・duplicate結果をmutation前後で確認し、複製成功後にだけ元layerを短縮するようにした。失敗時の履歴登録可否、duplicate backendの例外、selection、runtime cache、session reloadは未検証である。

Project Viewの非Composition項目の改名・フォルダ移動も、IDをsnapshotする`RenameProjectItemCommand`／`MoveProjectItemCommand`へ接続した。履歴の再適用時は現在projectのtreeからIDを再解決し、対象がなければ別項目へ誤適用しない。Project item削除とComposition削除のsafe-write表示は、現状Undoがないため`undoAvailable=false`へ補正した。

Project Viewの一括改名・フォルダ移動は、全対象をmutation前に検証し、問題があれば一件も変更せず、成功時は`MacroUndoCommand`一つへまとめるようにした。同値項目はno-opとして件数だけ報告する。selection、別project切替、session reload、runtime受入は未検証である。

Timelineのplayhead分割はAIとActiveContextの双方を`splitLayerWithUndo()`へ接続し、ActiveContextのIn／Out／Trimも既存のtiming property commandへ接続した。playhead境界外とtiming lockは変更せず、UndoManager不在時も分割前後のlayer ID比較で成功判定する。runtimeのselection、current frame、cache、session reloadは未検証である。

AIのIn／Out point、marker、chapter、marker clearはPlaybackServiceのbefore／after JSON snapshotへ委譲し、`InOutPointsSnapshotCommand`の一操作境界へ揃えた。Work Areaの開始・終了・移動も`SetCompositionWorkAreaCommand`でbefore／afterを保持し、Undo／Redo時に既存WorkAreaChangedEventとPlayback Engine範囲同期を通す。composition切替後の履歴、runtime cache、session reloadは未検証である。

AIのProject Viewフォルダ作成は、void APIの呼出しだけで成功を返さず、作成前後の項目ID集合を比較して新規Folderの実体と名前・親を確認するようにした。フォルダ生成後のUndo、selection、session reloadは未検証である。

Project Viewのフォルダ作成は、作成前のID集合から新規Folderを一意に特定し、`CreateProjectFolderCommand`へ接続した。Undoは空フォルダだけを削除し、Redoは保存したID・名前・親・tagを`addProjectItemsFromJson()`へ渡して復元する。同名既存フォルダを誤って履歴対象にしない。子項目を持つ場合のUndo、selection、session reload、runtime受入は未検証である。

Project item削除は、CompositionまたはCompositionを含むFolderを除外し、Folder／Footage／SolidのサブツリーだけをJSON snapshotと親ID・兄弟index付きの`RemoveProjectItemCommand`へ接続した。Undoは同じ親の元位置へサブツリーを復元し、RedoはIDで再解決して削除する。snapshotがUndoの単一エントリ上限を超える場合は削除前に拒否し、safe-writeの`undoAvailable`も対象型とComposition混在を反映する。Composition／render queue復元、selection、session reload、runtime受入は未検証である。

Mask reorderはlayer IDと変更前後indexを保持する`MoveMaskCommand`をserializable commandへ拡張し、履歴load時に既存resolverからlayerを再解決する。Undo／Redo後はlayer changed通知を通す。maskの同一性が別操作・session境界で変わる場合、selection、runtime、session reloadは未検証である。

Composition解像度Remapは、既存のmask・transform property・keyframe snapshotを`ChangeCompositionResolutionCommand`のJSONへ保存し、composition ID・旧／新サイズ・RemapPolicyをfactory load時に復元するようにした。keyframe valueが許可codecで表現できない履歴はserializable対象から除外し、runtimeのremap表示、session reload、別composition復元は未検証である。

## 2026-08-30 Reliability Correction

無効な layer ID の選択では、selection manager が空になることに合わせ、`ArtifactProjectService::selectLayer()` からの `LayerSelectionChangedEvent` も空の `layerId` を通知するようにした。要求された無効IDをイベントへ残さないため、selection実体とInspector側の対象解決が一致する。composition切替中を含むruntime受入は未実施である。

レイヤー作成は、ProjectManagerが生成した実体へProjectServiceが初期配置・時刻・親を適用した後、UndoManagerが利用可能な場合は既存のAdd／index／parent commandを一つのCreate Layer macroとして再接続するようにした。UndoManager不在時の直接経路は維持する。作成直後のselection、creation event、runtime cache、session reloadは未検証である。

通常UI・Timelineのgroup／ungroupは、Undo中にUndo対応済みのProjectServiceを再入呼出しして履歴を入れ子にしないよう、Add／ChangeLayerParent／Removeを直接構成するmacroへ変更した。全対象をactive composition内で確認し、group作成後は親ID、ungroup後はgroup消失と子の親解除を検証する。selection、order、runtime cache、session reloadは未検証である。

同group／ungroup macroには、変更前後のselected layer IDsとcurrent layer IDを保持する`LayerSelectionSnapshotCommand`も追加した。Undo／Redoの構造順序に合わせてselectionを復元するが、別compositionがactiveの場合は誤対象を選択しない。runtimeのselection bridgeとsession reloadは未検証である。

モジュレーションのeffect／layer snapshotも、source定義・assignment・smoothingTimeをJSON化する`EffectModulationSnapshotCommand`／`LayerModulationSnapshotCommand`へ接続した。64bitのassignment `targetId`は文字列で保存し、非有限値・重複source ID・不明source参照・範囲外enumはserializable対象から除外する。runtimeの音響結果、session reload、別composition復元は未検証である。

Effect modulationのsnapshot setterは変更前後を比較し、同一snapshotならUndo、projectChanged、変更通知を発生させないno-op経路にした。

AIのgroup移動／ungroupも、無効ID・自己移動・循環参照・壊れた階層をmutation前に拒否し、Undo push後の親関係／group消失を検証するようにした。予算拒否や適用失敗は成功扱いにせず、可能な場合は直前状態へ復元する。selection、order、runtime cache、session reloadは未検証である。

AIの位置・scale・rotation・opacity、note、keyframe、Project View一括rename／moveも、Undo push後の実値・存在・親関係を検証し、予算拒否を成功扱いしないようにした。batch rename／moveの適用失敗では、記録済みmacroをUndoするか、保持した旧状態から復元する。selection、current frame、runtime、session reloadは未検証である。

共通ProjectServiceとEffectServiceのlayer／effect scalar、visibility・lock・solo・shy・parent、audio trim／de-click、effect keyframe／expression、modulation変更も、既存Undo commandのpush後に実状態を確認する共通経路へ揃えた。失敗時は記録済みcommandをUndoし、成功通知やproject dirtyを先走らせない。runtime、selection、session reloadは未検証である。

session historyでは、serializable commandのtype欠落・空payloadを保存成功としてスキップせず、load時の不正・未知・復元不能entryも部分履歴として受け入れないようにした。disk offloadのenvelopeはwrapperのtype／label／estimatedBytesと照合し、factoryが別typeを返す場合も拒否する。旧形式の`currentVersion`欠落は`savedVersion`へfallbackするが、実際のsave／load、破損ファイル、Undo／Redo復元は未検証である。

退避wrapperが履歴から外れる際は対応する`undo_*.json`をデストラクタで削除し、予算エビクション・session load・manager破棄でもファイルを残しにくくした。権限エラー時のfilesystem cleanupは未検証である。

履歴envelopeのversion／estimatedBytes／state versionは、有限・非負の整数として検証し、小数や文字列の暗黙変換を受け入れないようにした。

keyframeの時刻・補間・制御点・metadata、mask／matte／text animator／effect maskの必須構造もsession codecで検証するようにした。maskのpath数値・enum・Bezier頂点も有限値と要素数を検証する。旧keyframeのframe-only形式は30fps fallbackとして残している。

Align／opacity／composition resolutionも、snapshot配列・レイヤーID・有限値・正のサイズ・RemapPolicyをcodec境界で検証するようにした。旧Alignでscaleが省略された場合だけは1.0へfallbackし、座標やサイズの文字列・小数は受け入れない。

session historyの`estimatedBytes`は、fieldが存在する場合に復元後commandの実測値とも照合する。履歴のサイズmetadataだけを差し替えたentryは、部分復元せず失敗扱いにする。

外部ファイル操作では、`UndoCommand::lastOperationSucceeded()` を通じてrename結果をUndoManagerへ返す。`MoveAssetFileCommand` のpush／undo／redo失敗時はstackを移動せず、Macro／offload wrapperも失敗を上位へ伝播する。

Asset Browserのrelink／delete／import登録もpush結果を確認する。履歴登録できないrelink／importは実データを戻し、deleteはbackup作成・再利用・破棄をcommand単位で管理し、batch relinkはfileとlayerの部分適用をrollbackする。

共通の`pushUndoCommandAndVerify()`とAI automationの直接push経路は、postconditionを評価する前に`UndoManager::push()`の戻り値を確認する。budget拒否・初回redo失敗を、既存状態や前回のoperation outcomeによる誤った成功へ変換しない。folder作成・precompose／ungroup・layer／effect操作も、履歴へ登録できなければ失敗を返す。

Property／Channel Boxのpreview編集とTimelineの先行keyframe編集も、Undo push拒否時のbefore snapshot復元へ接続した。Keyframe列、animatable、Anchor、Color Label、値、Expression、Text Animator、opacityを履歴登録前の状態へ戻し、push成功前にprojectChanged・selection更新・成功メッセージを確定しない。runtime、極小Undo予算、selection／cacheの実動作は未検証である。

TimelineTrackPainterViewの選択keyframe範囲変換、ドラッグ、接線、値、Anchor、Color Label、重複整理、ソーステキスト編集も同じrollback境界へ揃えた。補間・roving・ripple・slideはcommand-onlyとしてpush結果を適用件数と成功表示へ伝播し、先行変更した経路は拒否時にkeyframe snapshotとselectionを戻す。runtime、極小Undo予算、selection／cacheの実動作は未検証である。

Playbackのin/out point・marker、ActiveContextのlayer in/out／trim、Motion Sketchも同じ境界へ追加した。先行変更後のUndo push拒否ではJSONまたは位置keyframeのbefore stateへ戻し、command-only操作は拒否時に成功ログや通知を続けない。runtime、極小Undo予算、playback engine同期は未検証である。

Project Viewのcomposition resolution remapとProjectServiceのsource relink／localize／composition effect追加も、push拒否時に処理成功・dirty通知へ進まないようにした。command-onlyのresolution／source／effect操作は実状態を変更するredoが実行されないため、bool結果をそのまま上位へ伝播する。runtime、極小Undo予算、composition／render cache同期は未検証である。

Render Layer Widget v2のmask／polygon／corner radius／star inner radius drag commitも、先行変更後のUndo push拒否で編集前へ戻す境界を追加した。maskは既存要素数の範囲でbefore maskを戻し、polygon／parametric shapeはbefore geometryを戻す。runtime、複数mask構造変更、render cache同期は未検証である。

Inspectorのmatte変更、mask preset／一括mask操作、effect mask画像、Surface FX element操作も、Undo push拒否を成功扱いしないようにした。mask presetは`MaskEditCommand`で置換／追加を記録し、Surface FXは変更前の選択indexを拒否時に復元する。Timeline左ペインのvariant、matte、visibility／lock／solo／shy、mask削除、layer移動と複数選択macroもpush結果を再描画・選択更新へ伝播し、mask削除はbefore snapshotへ戻す。runtime、極小Undo予算、selection／dirty／cache同期は未検証である。

Layer Menuの整列・分布・spacing・衝突解消は、実値を先に書き換えた後のUndo push拒否で`AlignLayerSnapshot`のbefore位置／scaleへ復元するようにした。ProjectServiceのlayer作成macroもpush結果をpostconditionへ明示的に反映する。runtime、selection、dirty／cache同期は未検証である。

Composition Render Controller／Transform Gizmoの3D・2D transform、anchor、複数選択transform、motion path key／tangent、shape path、live field、camera POI、corner radiusも、既存before snapshotへ戻してから失敗を返すようにした。runtime、極小Undo予算、selection／cache同期は未検証である。
- Rigの骨／制御点／ウェイト／ポーズ、Puppet pin、line endpoint、mask一括操作もpush拒否時の復元へ接続した。複数レイヤーmask macroは各layerのbefore maskを保持し、mask色変更は`MaskEditCommand`へ接続した。runtime、selection／cache、Puppet deform同期は未検証である。

- Composition EditorのPaste Layersは、一時detachした複数レイヤーをpush拒否時に元indexへ戻すようにした。Sequence／Match Durationはtimeline before／after snapshotをUndoへ登録し、Center Layerはcurrent frameの`MoveLayerCommand`へ接続した。マスク単発編集の変更通知もUndo commit成功後へ揃えた。Paste後のselection・親／matte参照、timeline cache、runtime、極小Undo予算は未検証である。

- Pen／Shape Path確定は`ShapePathVertexEditCommand`へ接続し、push拒否時にcustom pathをbeforeへ戻すようにした。Pending Maskはcommit成功後にだけpending stateを消去し、矩形・閉じパス・segment挿入の通知をcommit成功後へ揃えた。path／mask cache、selection、runtime、極小Undo予算は未検証である。
- Layer Menuの複数選択Bring／Send操作は、現在順序をシミュレーションして実行時点の旧indexを求め、相対順序を維持する`MoveLayerIndexCommand`のmacroへまとめた。最上段・最下段の無効移動は除外し、commandも移動後indexを検証してmacroへ失敗を伝播する。runtime、selection、部分移動失敗、保存／再読込後の順序は未検証である。
- `LayoutSnapshotCommand`はrestore callbackの結果を、`AnimationLayerStackSnapshotCommand`は復元後snapshotの一致結果を`lastOperationSucceeded()`へ返し、初回redo／macro内の復元失敗をUndoManagerへ伝播するようにした。復元途中で失敗した場合は逆snapshotを補償適用する。runtime、壊れた対象の復旧、session reloadは未検証である。
- layer／effect propertyのvalue、keyframe、expression commandも、既存setterのbool結果または対象propertyの存在を`lastOperationSucceeded()`へ伝播するようにした。対象消失や不正propertyを履歴登録成功として扱わない。keyframe要素単位の内部失敗、runtime、session reloadは未検証である。
- Text Layerの`text.value`とText Animator stack commandも、既存setterのbool結果または`ArtifactTextLayer`への型判定を`lastOperationSucceeded()`へ伝播するようにした。対象消失・対象型不一致を履歴登録成功として扱わない。Animator内部JSONの妥当性、runtime、session reloadは未検証である。
- mask、matte、de-click、effect mask、source置換、modulation、layer追加／削除、visibility／lock／solo／shy／blend、rename／parentのcommandも、既存のbool戻り値・getter・composition存在確認を`lastOperationSucceeded()`へ伝播するようにした。失敗時の変更通知を抑制し、追加／削除は操作後の存在状態を検証する。要素内容、setter内部の副作用、runtime、session reloadは未検証である。
- 整列は全対象preflightと位置・scaleの適用後検証を行い、途中失敗時に逆snapshotで補償する。opacity／Variant／Project item／in-out／work areaは対象消失、無効index、親・存在状態、JSON復元、正規化後rangeを成功状態へ反映する。opacityの評価値と同期callbackはruntime確認対象である。
- Resolution Remapは全snapshot layerの存在確認後にサイズを適用し、undoのmask数・property／keyframe数、redoのnewSizeを検証する。redo失敗時はoldSizeとbefore snapshotへ補償する。mask要素とkeyframe値の完全一致はruntime確認対象である。
- Source localization／shared relinkは、layer weak pointerとasset APIのbool結果をcallbackから履歴へ伝播し、失敗時に変更通知を進めない。asset managerとfilesystemの拒否はruntime確認対象である。

- Undoのmask／matte、Text Animator、layer／effect property復元は、setter呼出しだけでなく適用後の要素数・keyframe内容・metadata・value・expression・stack snapshotを検証する。不一致時は直前snapshotへ補償し、失敗通知を抑制する。Add／Remove Layerの参照解除・復元、Layout callback、selection復元も同じ成功状態境界へ揃えた。runtime、setter内部副作用、session reloadは未検証である。
- Add／Remove Layerでは本体の追加・削除に加え、dependent layerのmatte／parent関係を検証し、selectionを含む依存状態の失敗を`lastOperationSucceeded()`へ集約する。失敗時は事前関係snapshotから本体・関係状態を補償する。Effect scalarはeditable propertyだけ適用後値を比較し、custom setterは対象存在までを保証範囲とする。
- `CreateVariantCommand`は抽出Variantを保持するstateful commandで、既存JSONだけではRedo後の同一実体を再構成できないため、`canSerialize()==false`としてoffload／session historyから除外した。通常の同一session内操作は継続する。
- session historyの`savedVersion`／`currentVersion`は非負整数に加えて`savedVersion <= currentVersion`を検証し、逆転したdirty／履歴位置をloadしない。
- history save／offload側もtype／label長とestimatedBytes範囲をload側と対称に検証し、永続化前にcodec境界を揃える。
- history loadではtype／labelのJSON型、再構成後label、estimatedBytesのqint64変換範囲も検証し、改変されたenvelopeを取り込まない。
- JSON整数共通decoderでqint64下限・上限を先に検証し、履歴metadataの範囲外doubleを`toInteger()`へ渡さない。
- Text GizmoのAnimator単一値編集もsetter結果と適用後valueを検証し、失敗時は直前valueへ補償してからUndo履歴の成功状態を返す。
- ProjectServiceのprecompose／effect／group／split系ローカルcommandも失敗状態をUndoManagerへ返し、layer effect removeは対象存在と削除後状態を検証する。
- Timelineのripple／slide／interpolation commandもsnapshot restoreと適用関数の失敗を`lastOperationSucceeded()`へ返し、interpolationは対象を先にpreflightする。
- Render Controllerの3D／2D Gizmo、複数選択Gizmo、Rig、Puppet、Anchor、Shape corner radius、Motion Path、Live Field commandも対象消失・型不一致・主要getter不一致を成功状態へ反映し、成功時だけ変更通知を発行する。Gizmo metadata、Puppet deform、複数対象の途中失敗補償は未検証である。
- Command Paletteのマスクsnapshot commandも対象不在・復元後mask数不一致を成功状態へ反映する。mask要素内容と途中追加失敗の全体補償は未検証である。
- `UndoManager::push()`は初回redoの成功状態がfalseの場合に逆操作を一度実行してからfalseを返し、履歴へ登録されない部分適用を共通境界で補償する。逆操作失敗と非対称な外部I/Oは未検証である。
- Macroは失敗子と既適用子を内部で補償し、UndoManagerのpush／undo／redoはMacroを二重補償せず、非Macroの失敗移動だけ逆操作で補償する。逆操作失敗、外部I/O、runtimeは未検証である。
- Asset dropはMoveAssetFileCommandのpush成功後だけ移動済みとしてモデルを更新し、Audio Reactive bindingの追加／削除もpush結果を返す。filesystem raceとruntimeは未検証である。
- Audio Reactive bake／record commitもpush結果を返し、履歴登録失敗時に成功結果へ進まない。recording stateとruntimeは未検証である。
- Audio Mixer advanced routingもsnapshot push結果を確認し、失敗時はgraphを再表示してcomposition changedを確定しない。外部widgetの先行変更補償は未検証である。
- Template importは生成layerを単一MacroUndoCommandへまとめ、複数layerの途中失敗をMacroの補償境界へ集約する。layer順序・selection・参照復元は未検証である。
- Timeline Track Painter側のripple／slide／interpolation／roving commandもsnapshot復元・対象存在・適用件数を成功状態へ反映する。keyframe要素完全比較とcallback型snapshot commandは未検証である。
- Template Library importも生成layerを単一MacroUndoCommandへまとめ、複数layerの途中失敗を部分挿入として残さず、成功時だけ追加件数を返す。layer順序・selection・参照復元は未検証である。
- Nested Macroでは内側の失敗補償を外側が二重実行しないよう、`handlesFailedOperationCompensation()`で補償責務を伝播する。nested構造と逆操作失敗は未検証である。
- Composition EditorのAnimation Layer context menuもsnapshot commit helperへ接続し、manager不在時の操作消失とpush拒否時のbefore不一致を抑える。snapshot完全一致・selection・cacheは未検証である。
- Composition Editorのlayer visibility／lock／solo／shy／centerもUndoManagerをnull-safeに参照し、未初期化時のクラッシュを避ける。manager不在時は安全に中断する。
- Paste／Cleanupのcommand-only経路をnull-safe化し、Render Widgetの先行変更後のドラッグ確定はmanager不在時に直接復元する。push拒否・selection・index・runtimeは未検証である。
- Asset import／relinkとPlayback markerの先行変更経路もmanager不在をrollback扱いにし、履歴なしの実体変更を残さない。filesystem・marker・runtimeは未検証である。
- Layer Menuのtoggleとmask-to-shapeのcommand入口もUndoManager null-safe境界へ揃え、未初期化時のクラッシュを避ける。manager不在時は安全に中断する。
- Particle Render Widgetのdrag rollbackもmanager不在を安全に扱い、Effector位置・半径のbefore値を正しく復元する。particle cache・runtimeは未検証である。
- Composition EditorのText／CenterとSequence／Match Durationもmanager不在を安全に扱い、先行変更されたtimeline stateはpush拒否時にbeforeへ戻す。selection・cache・runtimeは未検証である。
- Safe Delete Layersもmanager不在・macro push拒否時にselectionと成功表示を確定しない。依存関係・dirty・runtimeは未検証である。
- TimelineKeyframeSnapshotCommandはbool callbackとsnapshot target preflightを持ち、Key Pattern／Keyframe Areaで復元失敗を履歴へ伝播する。残存void callback、setter単位の失敗、selection・runtimeは未検証である。
- Curve Editor tangentもbool callbackへ接続し、target preflight失敗をUndo状態へ反映する。setter完全検証・selection・runtimeは未検証である。
- PlayheadのAdd／Remove Keyframeもbool callbackへ接続し、snapshot target preflight失敗をUndo状態へ反映する。setter完全検証・selection・runtimeは未検証である。
- Keyframe Area Valueもafter snapshot適用とredo／undo callbackのbool結果を確認し、target消失を履歴成功へ変換しない。selection・cache・runtimeは未検証である。
- Layer Menuのquick layer／cache policy／proxy qualityもmanager存在確認を行い、quick layerのtransaction拒否時はdetach前へ戻す。dirty・selection・runtimeは未検証である。
- TimelineのMotion Trajectory／Keyframe Fringe／Move／PasteとTrack PainterのReverse／Set Valueもbool snapshot callbackへ接続し、対象消失・preflight失敗を履歴成功にしない。selection・cache・runtimeは未検証である。
- Track PainterのDelete／Reverse／Set Value／Set Anchor／Set Color Labelもbool snapshot callbackへ接続し、keyframe metadataの対象消失・preflight失敗を履歴成功にしない。selection・cache・runtimeは未検証である。
- Track PainterのDuplicate／Distribute／Repeat Selected Keyframesもbool snapshot callbackへ接続し、batch keyframe操作の対象消失・preflight失敗を履歴成功にしない。selection・cache・runtimeは未検証である。
- Track Painter context menuのSet Keyframe Area Valueもbool snapshot callbackへ接続し、area valueの対象消失・preflight失敗を履歴成功にしない。selection・cache・runtimeは未検証である。
- Track Painter context menuのBreak／Unify TangentsとTransform Selected Keyframesもbool snapshot callbackへ接続し、対象消失・preflight失敗を履歴成功にしない。selection・cache・runtimeは未検証である。
- Track PainterのReverse All／Clean Keyframesなど残存snapshot callbackもbool `Operation`へ接続し、主要操作の対象消失・preflight失敗を履歴成功にしない。selection・cache・runtimeは未検証である。
- Timeline Widget／Track Painterのsnapshot command利用箇所は全てbool `Operation`へ統一し、未使用のvoid互換コンストラクタを削除した。selection・cache・runtimeは未検証である。
- Layer Panelのinline renameもcommand所有権を`unique_ptr`へ統一し、UndoManager不在時のraw pointerリークを防ぐ。selection・runtimeは未検証である。
- Inspector／Layer Panelのmatte／variant／layer move commandも所有権を`unique_ptr`へ統一し、manager不在時のリークを防ぐ。selection・runtimeは未検証である。
- Inspector／Layer Panelの対象Undo commandはraw `new`を残さず、`make_unique`／`move`へ統一した。別形式のallocationとruntimeは未検証である。
- Layer PanelのVariant切替／作成とinline renameは、UndoManager不在時にcommandの`redo()`を直接実行するfallbackを追加し、manager初期化前の無操作を避けた。dirty・selection・runtimeは未検証である。
- Layer Panelのblend mode、visibility／lock／solo／shyの単一・複数選択経路も、UndoManager不在時にcommand／macroの`redo()`を直接実行するfallbackへ揃え、無操作とUI取り残しを抑えた。dirty・selection・runtimeは未検証である。
- Layer Panelのmatte type／enabled／opacity／blend／fit helperも、UndoManager不在時にcommandの`redo()`を直接実行して成功状態を返すfallbackへ揃え、matte編集の無操作を避けた。dirty・selection・runtimeは未検証である。
- `MacroUndoCommand`は空の子command集合を失敗扱いとし、状態変更のない空履歴と空serialize payloadの生成を抑えた。session復旧・runtimeは未検証である。
- `UndoManager::push()`／`createCommand()`は、単一commandまたは全体memory budgetで保持不能なcommandを初回redo／session復元前に拒否し、実体だけ適用される履歴外編集を抑えた。budget縮小・offload・runtimeは未検証である。
- `saveSessionHistory()`は非シリアライズ commandを省略せず、履歴全体を保存できない場合に失敗するようにして、保存成功後の履歴欠落とversion不一致を抑えた。保存UX・runtimeは未検証である。
- `UndoManager::Impl::stackBytes()`は推定サイズを飽和加算し、異常な合計値でもmemory budget enforcementがwrapで無効化されないようにした。runtimeは未検証である。
- offload file名とcleanup globにmanager単位のUUIDを付け、共有directoryで別managerの履歴ファイルをcleanupしないようにした。旧形式孤児ファイル・runtimeは未検証である。
- state ID割り当ては既存undo／redo IDとsaved/current versionを避けるようにし、wrap後の履歴version衝突を抑えた。ID空間枯渇・runtimeは未検証である。
- Undo／Redo両stackをentry／memory budgetの対象に統合し、全履歴が設定上限を超えないよう淘汰する。`currentMemoryBytes()`とmemory pressureも全履歴を数えるが、budget変更・runtimeは未検証である。
