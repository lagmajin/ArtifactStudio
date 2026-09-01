**最終更新:** 2026-09-01

## 2026-08-31 — ArtifactPr の GPU Preview は ArtifactRenderer を再利用できない

- **関連:** `ArtifactPr/CMakeLists.txt`、`ArtifactRenderer/CMakeLists.txt`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- **確認できた事実:** `ArtifactRenderer` は Qt Core/Gui/Svg だけに依存する外部レンダリング用 executable で、Diligent のリンク可能ライブラリではない。`ArtifactCore` は D3D12 の Diligent 基礎ライブラリを公開しており、`ImageF32x4RGBAWithCache` は GPU texture upload を提供するが、swap chain と共有 device を安全に所有する `DiligentDeviceManager` は `ArtifactRender` 側にある。一方、Artifact の GPU viewport は巨大な `CompositionRenderController` と AE 系 Composition/Layer モデルに結合している。
- **気づき（未実装）:** ArtifactPr の `RenderPlan` と `ImageF32x4_RGBA` を入力にする専用 GPU compositor を新設し、CPU compositor を明示フォールバックとして残すのが適切。既存 Artifact viewport の直接 import や `ArtifactRenderer` への依存追加は、モデル・実行形態の不一致を解決しない。実装前に `DiligentDeviceManager` を共有レンダリング基盤へ移すか、ArtifactPr 専用の小さな GPU support target を新設するかを決める必要がある。
- **対応 (2026-08-31):** `ArtifactGpuFoundation` static target を追加し、Config と DiligentDeviceManager を ArtifactRender から分離した。ArtifactRender はこの target を public link し、ArtifactPr は renderer 本体へ依存せず foundation だけを link する。CMake configure と runtime の device ownership は未検証。
- **価値または懸念:** NLE の decode/RenderPlan/FFmpeg export 契約を維持しながら preview を GPU 化できる。Diligent device 所有、RGBA float upload、PSO、swap chain、GPU/CPU output parity と障害時の fallback を別途設計・実機検証する必要がある。
- **次に確認すべきこと:** 共通 Diligent host/texture-upload API を ArtifactCore または ArtifactPr に最小依存で置けるか、GPU compositor の preview-only 導入後に RenderPlan の CPU export と同一フレームを比較できるかを確認する。

## 2026-08-31 — Layer選択Automationの結果を実状態で検証する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` の `selectLayer`
- **確認できた事実:** SelectionManagerはlocked／selection-locked layerに対して選択処理を無視するが、Automation APIは選択後の状態を確認せず常に成功を返していた。
- **対応:** `isSelected` と `currentLayer` の両方を確認し、選択が成立しない場合は失敗を返すようにした。
- **価値 / 懸念:** AIがロックされた対象を選択できたと誤認しない。選択はプロジェクトUndo対象ではなく、既存の選択イベント経路を維持している。
- **次に確認:** 通常／locked／selection-locked layerと、既存選択がある状態でのruntime結果を確認する。

## 2026-08-31 — Asset importの作成失敗と部分結果を明示する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` の `importAssetsFromPaths`
- **確認できた事実:** Automation経路は、暗黙のproject作成失敗を確認せず、Service不在や一部pathの失敗でも `success`／`failedCount`／`partial` を返していなかった。
- **対応:** project作成結果を検証し、import結果に成功件数・失敗件数・部分成功・暗黙作成の有無を追加した。
- **価値 / 懸念:** AIが空のimport結果を成功と誤認せず、project作成を含む失敗段階を判別できる。Service内部のcopy後にprojectへ追加できない場合のrollbackは別課題。
- **次に確認:** 有効／無効path混在、空入力、project作成失敗、sequence importキャンセル時の結果契約をruntime確認する。

## 2026-08-31 — UndoManager不在時の一括キーフレーム失敗を復元する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` の `batchSetKeyframes` / generic keyframe wrapper
- **確認できた事実:** UndoManagerが利用できないfallbackでは、複数プロパティを順に直接適用し、後続プロパティの検証失敗時に先行プロパティの変更が残る可能性があった。またgeneric wrapperが失敗時も `executed: true` を返していた。
- **対応:** 一括適用失敗時に変更済みプロパティを適用前のkeyframe／animatable状態へ戻し、wrapperの `executed` を実際の成功値へ合わせた。
- **価値 / 懸念:** UndoManagerの有無で一括操作の原子性と結果報告が変わる問題を縮小した。keyframe metadataの完全な復元は既存の直接fallback仕様に依存するためruntime確認が必要。
- **次に確認:** UndoManager有／無の両経路で複数propertyの途中失敗、Undo/Redo、animatable状態をruntime確認する。

## 2026-08-31 — 全資産削除の空状態を失敗として扱う

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` の `removeAllAssets` / `removeAllAssetsConfirmed`
- **確認できた事実:** 資産が0件のとき、削除対象がないにもかかわらず通常APIが成功扱いになり、確認付きAPIのdry-runも失敗予定を示していなかった。
- **対応:** 対象0件を変更失敗として返し、通常／確認付きdry-runの `wouldFail` を同じ条件へ揃えた。
- **価値 / 懸念:** 自動化が「削除できた」と誤認して後続処理を進めることを防ぐ。利用側が空状態を成功相当のno-opとして期待していないかはruntime確認が必要。
- **次に確認:** 空プロジェクト、資産あり、削除後の再実行で結果とSafeWrite監査をruntime確認する。

## 2026-08-31 — AIのEffect preset適用もUndo境界へ統合する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` の `loadLayerEffectPreset`
- **確認できた事実:** InspectorのEffect preset読込はUndo化した一方、AI／Automationの同名操作はServiceの直接適用だけで、履歴に入っていなかった。
- **対応:** AI経路でも適用前後のJSONを `EffectPresetSnapshotCommand` に渡し、履歴登録失敗時は適用前へ復元する。最近使用プリセットの記録も成功後だけにした。
- **価値 / 懸念:** UIとAIの同じEffect編集操作が同じUndo契約へ収束した。Effect固有setterとruntime/session復元は未検証。
- **次に確認:** UI／AIを交互に使ったEffect preset適用、Undo/Redo、履歴オフロード復元をruntimeで確認する。

## 2026-08-31 — Render Queue 音声設定のno-op通知を抑止する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` のQueue音声設定API
- **確認できた事実:** 音声ソース、codec、bitrateは正規化後の値が既存値と同じでもService setterを呼び、不要なjob更新を発生させていた。
- **対応:** 正規化後の値を先に比較し、同値なら成功扱いのno-opとして即時返却するようにした。
- **価値 / 懸念:** 自動化の同値入力でQueue UI更新やdirty相当の通知を増やさない。Service側の通知粒度はruntime確認が必要。
- **次に確認:** 空文字codec、長いパス、bitrate境界を含むno-op／変更通知をruntimeで確認する。

## 2026-08-31 — Render Queue 全件停止の結果検証とモデル同期

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`
- **確認できた事実:** Queue全件pause/cancelのAutomation APIはService呼出し後に常に成功を返し、Serviceの全件操作もcore queue modelを同期していなかった。
- **対応:** pause後にRenderingが残っていないこと、cancel後にPending/Renderingが残っていないことを確認するようにし、Serviceの全件操作後に既存の `syncCoreQueueModel()` を呼ぶようにした。
- **価値 / 懸念:** AI結果と実Queue状態、Queue UI表示の乖離を抑えた。worker停止と外部rendererのruntimeタイミングは未検証。
- **次に確認:** 複数Queueのpause/cancel、再開、UIモデル反映をruntimeで確認する。

## 2026-08-31 — Batch relink の途中成功を逆順rollbackする

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` の `batchRelinkFootageByPath`
- **確認できた事実:** 複数relinkの途中で失敗すると、先行成功分が残ったまま失敗結果を返していた。
- **対応:** 成功した旧パス／新パスを保持し、失敗時に逆順でrelinkを戻す。`rolledBackCount` と `rollbackSucceeded` を結果へ追加し、rollback自体の失敗も隠さない。
- **価値 / 懸念:** 自動化のbatch操作が部分状態を残しにくくなった。パス交換や外部ファイル状態が競合するケースはruntime確認が必要。
- **次に確認:** 複数 footage の全成功、途中失敗、逆順復元失敗、同一パス競合をruntimeで確認する。

## 2026-08-31 — Effect preset 適用をJSONスナップショットUndo化する

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- **確認できた事実:** Effect preset読込はEffectを直接変更した後にUndoコマンドを作成しておらず、適用操作をUndo/Redoできなかった。
- **対応:** 適用前後の正規化済みpreset JSONを `EffectPresetSnapshotCommand` に保存し、Undo/Redo時は既存のPresetManager適用APIと適用後のJSON一致検証を使うようにした。適用後の履歴登録に失敗した場合は適用前へ戻す。
- **価値 / 懸念:** Effect preset適用を1操作として復元可能にした。Effect固有setterや画像データの完全一致はruntime確認が必要。
- **次に確認:** Effect presetのマスク画像・複数プロパティを含むUndo/Redoと履歴オフロード復元をruntimeで確認する。

## 2026-08-31 — Mask preset 適用を全UI導線でUndo化する

- **関連:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`、`Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- **確認できた事実:** Inspector以外のMask preset適用導線が `clearMasks()` / `addMask()` を直接呼び、Undo履歴を迂回していた。
- **対応:** レイヤーメニューとAsset Browserも既存の `MaskEditCommand` で置換・追加を記録し、manager不在時はcommandの結果を確認してから変更通知するようにした。
- **価値 / 懸念:** UI導線によるマスク適用のUndo整合性と失敗時の表示整合性を揃えた。プリセット全体のファイル操作Undoは対象外。
- **次に確認:** runtimeで置換・追加、Undo/Redo、UndoManager不在時の適用を確認する。

## 2026-08-31 — Mask preset を適用前検証する

- **関連:** `Artifact/src/Project/ArtifactPresetManager.cppm` の `applyPresetJsonToMask`
- **確認できた事実:** paths 配列や頂点を構造検証せず、先に既存マスクを消去してから読み込んでいたため、不正エントリで既存状態を失う可能性があった。
- **対応:** paths / vertices / animationKeyframes の配列・オブジェクト構造と頂点座標の有限値を適用前に検証するようにした。
- **価値 / 懸念:** malformed なMask presetで既存マスクが部分的に消えるリスクを抑える。Mask preset全体のUndoは別途未実装。
- **次に確認:** 実プリセットのアニメーション頂点・feather値を含む読み込みをruntimeで確認する。

## 2026-08-31 — Effect enabled 切替の検証失敗時に復元する

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm` の layer / composition effect enabled setter とUndo command
- **確認できた事実:** setter後の値検証に失敗した場合、layer・compositionの通常フォールバックと composition用Undo command が `false` を返すだけで、部分変更を残す可能性があった。
- **対応:** 各経路で変更前のenabled状態を保持し、検証失敗時に旧状態を再適用するようにした。
- **価値 / 懸念:** UIやAIが失敗と判断した操作がEffect状態だけ残すことを防ぐ。復元setter自体の失敗はruntime確認が必要。
- **次に確認:** Effect実装ごとの enabled setter が失敗し得る条件をruntimeで確認する。

## 2026-08-31 — Effect preset の既知パラメータを適用前検証する

- **関連:** `Artifact/src/Project/ArtifactPresetManager.cppm` の `applyPresetJsonToEffect`
- **確認できた事実:** 既知パラメータでも JSON の型を検査せず `toInt` / `toDouble` / `toBool` 等へ変換していたため、不正値を別の値として適用し、プリセット読み込みを成功扱いにする可能性があった。
- **対応:** properties 配列、各エントリ、既知パラメータの型、有限な数値、色文字列を適用前に検証するようにした。未知パラメータは将来互換性のため従来どおり無視する。
- **価値 / 懸念:** 不正プリセットによる部分変更や、型変換による意図しないEffect状態を抑える。setter後の完全な値一致やプリセット全体のUndoは別途runtime確認が必要。
- **次に確認:** 各Effectの整数・色・Point2Dパラメータを含む実プリセットで読み込み結果を確認する。

## 2026-08-31 — Batch relink の部分成功を結果に明示する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` の `batchRelinkFootageByPath`
- **確認できた事実:** batch relink は `requested` / `succeeded` / `failed` を返していたが、他の batch API と違い、部分成功を直接判定する `partial` と件数名の統一 alias がなかった。
- **対応:** 既存キーを維持したまま `requestedCount` と `partial` を追加し、空入力では `partial: false` を返すようにした。
- **価値 / 懸念:** 自動化側が全成功・全失敗・部分成功を同じ件数計算なしで判別できる。複数件途中失敗時に先行成功を巻き戻す処理は未実装。
- **次に確認:** 呼び出し側が `partial` を使って再試行や警告表示を分けるか runtime で確認する。

## 2026-08-31 — Effect 複製で存在しない対象を受け付けない

- **関連:** `Artifact/src/Service/ArtifactEffectService.cppm` の `duplicateEffect`
- **確認できた事実:** 指定された `effectId` がレイヤー内に存在しなくても、入力IDを型名候補として新規Effectを作り、複製成功に進む可能性があった。
- **対応:** source effect の探索に失敗した場合は、Effect生成・Undo登録を行わず `Effect not found` を返すようにした。
- **価値 / 懸念:** AIや外部自動化からの誤IDで、意図しないEffectが追加されることを防ぐ。
- **次に確認:** composition effect の同等操作にも、対象IDの存在検証が揃っているか確認する。

## 2026-08-31 — Effect property の検証失敗時に変更前値へ戻す

- **関連:** `Artifact/src/Service/ArtifactEffectService.cppm` の layer / composition effect property setter
- **確認できた事実:** UndoManager が利用できないフォールバック経路で、setter 後の値が期待値と異なる場合に `false` を返すだけで、setter が部分的に変更した値を復元していなかった。modulation snapshot、effect expression、effect keyframe にも同じ問題があった。
- **対応:** layer と composition の effect property は `oldValue`、modulation snapshot は `before`、effect expression は `oldExpression`、effect keyframe は変更前の全キー集合を再適用してから失敗を返すようにした。
- **価値 / 懸念:** 自動化呼び出し側の失敗表示とプロジェクト実状態の乖離を抑える。復元 setter 自体の失敗は既存APIでは検出できないため、runtime確認が必要。
- **次に確認:** 各 effect property 型で setter の部分適用が起きるケースを runtime で確認する。

## 2026-08-31 — Export Matrix の部分投入を完全成功と報告しない

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` の `queueExportMatrixForCurrentComposition`
- **確認できた事実:** 複数の export job のうち一部しか render queue に追加できなくても、1件以上追加されれば `success: true` になっていた。
- **対応:** `requestedCount` / `addedCount` / `failedCount` を返し、全 job が追加された場合だけ成功と判定するようにした。
- **価値 / 懸念:** 自動化側が部分投入を完全成功と誤認しない。既に追加された job の削除や再試行を自動で行う共通 rollback は未実装。
- **次に確認:** render queue API が追加失敗時に job を残すかどうかを runtime で確認し、必要なら batch rollback を設計する。

## 2026-08-31 — Export Matrix の空キューを成功扱いにしない

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` の `queueExportMatrixForCurrentComposition`
- **確認できた事実:** 有効な export matrix job が 0 件でも、また queue 追加件数が 0 件でも `success: true` を返していた。
- **対応:** 有効 job がない場合は `NO_ENABLED_EXPORTS`、追加結果が 0 件の場合は `success: false` を返すようにした。
- **価値 / 懸念:** 自動化側が「キュー投入できた」と誤認しない。個々の render queue setter は既存 API が戻り値を提供しないため、設定適用までの完全な検証は runtime 確認に残る。
- **次に確認:** export matrix の不正な format / codec を含む場合に、追加後の queue job 設定を実値で検証する。

## 2026-08-31 — WorkspaceAutomation の空バッチを成功扱いにしない

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` の project item rename / move、footage relink バッチ
- **確認できた事実:** 入力配列が空の場合、rename と project item move は実変更がないまま `success: true` を返し、relink も `failed == 0` 判定だけで成功になっていた。
- **対応:** 3つの空入力を `success: false` とし、既存の件数・`details` / `failures` 形式を維持した。relink には `errorCode: NO_ITEMS` を追加した。
- **価値 / 懸念:** AIやUI側が「成功した変更」と「何も依頼されなかった状態」を区別できる。既存の batch relink は複数件の途中失敗時に先行成功を巻き戻さないため、共通 rollback 化は別作業として残る。
- **次に確認:** バッチ操作の呼び出し側が `success` と成功件数を併用して表示・再試行を判断しているか、runtime で確認する。

### Audio Automation の複合設定フォールバックを原子的にする

- **日付:** 2026-08-31
- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` の De-click 設定／Audio Trim
- **事実:** UndoManager がないフォールバックでは、2項目のsetterを独立に呼んでいたため、後段の失敗時に前段だけ変更が残る可能性があった。
- **修正:** 前段の適用失敗時は即時終了し、後段の適用失敗時は前段を保存済みの旧値へ戻す順序付き処理へ変更した。UndoManager利用時の既存macro境界は維持する。
- **価値/懸念:** manager初期化前や履歴利用不能時にも複合音声設定の部分適用を抑えられる。復元setter自体の失敗はruntime確認が必要。
- **次に確認:** De-click threshold／width、Trim in／outの各setter拒否と、片方だけ変更可能なケースで最終値がbeforeへ戻ることを確認する。

### WorkspaceAutomation の実行executorにも明示的な effect target を通す

- **日付:** 2026-08-31
- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` の内部 CommandIR executor
- **事実:** 公開されている `executeCommand()` の実executorは `set_property` を常に汎用レイヤープロパティへ渡しており、effect編集の正しい引数経路へ到達しなかった。
- **修正:** `target.effectId` と `effect.` 接頭辞が揃った場合だけ、effect enabled または接頭辞を除いた parameter 名で既存のUndo対応WorkspaceAutomation setterを呼ぶようにした。条件が揃わない場合は従来の汎用経路を維持する。
- **価値/懸念:** AI／Python／CommandIRのeffect property編集で、property pathをeffect IDとして誤解釈しない。effect targetを必須化するvalidationとruntime確認は未実施。
- **次に確認:** effect instance ID付きのenabled／parameter変更、ID欠落、存在しないID、Undo拒否時の結果を確認する。

### CommandIR executor の返却型と effect target を一致させる

- **日付:** 2026-08-31
- **関連:** `Artifact/src/AI/CommandIRExecutor.cppm`
- **事実:** WorkspaceAutomation の layer creation は `QVariantMap(success, layerId)`、effect creation は effect ID文字列を返すが、executor が両方を `toBool()` していた。また `set_property` の effect 経路は property path を effect IDとして渡していた。
- **修正:** creation map の `success`、effect IDの非空値で結果を判定し、成功時の対象IDを `details` に返すようにした。effect property は明示的な `target.effectId` がある場合だけ正しい引数列で呼び出す。
- **価値/懸念:** 成功したAI操作を失敗と誤認せず、曖昧な識別子による誤適用を防ぐ。実際のCommandIR payloadとruntime実行は未検証。
- **次に確認:** layer作成、effect追加、effect.enabled、effect parameterの各CommandIRで結果とUndo件数を確認する。

### Automation の batch property 結果は全件スキップを成功にしない

- **日付:** 2026-08-31
- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx` の `batchSetLayerProperties()`
- **事実:** ロックされた対象は `skippedCount` だけを増やし `failureCount` を増やさないため、全件スキップでも従来の `failureCount == 0 && operations 非空` 判定が `success: true` を返していた。
- **修正:** 成功には少なくとも1件の実適用と、failure／skip がないことを要求し、成功とスキップが混在する場合だけ `partial: true` とする。
- **価値/懸念:** AI／Python automation が未適用の操作を成功済みと誤認しにくくなる。バッチ全体の単一Undo macro化は別途 runtime／設計確認が必要。
- **次に確認:** ロック対象のみ、成功＋ロック混在、全件成功の3ケースで返却マップとUndo件数を確認する。

### Audio Mixer の Undo 登録失敗時は変更前へ復元する

- **日付:** 2026-08-31
- **関連:** `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm` のミキサー・ルーティング変更
- **事実:** レイヤー音量等のプロパティ変更と AudioMixer JSON スナップショット変更は、ライブ変更を変更前へ戻してから Undo コマンドの `redo()` で記録する構造になっている。
- **修正:** `UndoManager::push()` が失敗した場合の復元先を `after` から `before` に修正した。
- **価値/懸念:** 履歴容量超過やコマンド失敗時に、履歴へ登録されていない変更だけが残る不整合を防げる。実行時の容量超過経路は未検証。
- **次に確認:** ミキサーの各編集で、Undo 登録失敗後に表示値・Mixer JSON・再描画が変更前で一致することを確認する。

## 2026-08-31: 残存 custom Qt 接続の境界判定

- 関連: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`, `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cppm`, `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm`
- 事実: 横断抽出で残った custom Qt 接続の多くは、複合 widget 内の child→owner 更新、channel strip の操作、または小ボタン／入力であり、独立した widget 間の共有状態通知ではなかった。
- 判断: `QModelIndex` のような model 所有権に依存する値や高頻度 audio level を、識別子・頻度制御なしに global EventBus へ載せない。現状の局所 Qt 境界を維持し、安定した ID／command payload と複数購読先が揃った時点で移行候補にする。
- 価値/懸念: EventBus を単なる Qt signal の置換先にせず、責務境界と payload の寿命を確認してから段階移行できる。残存 signal 数だけで未移行と判断すると、逆に global routing と再描画を増やすリスクがある。
- 次に確認: 複合 widget が独立 surface として再利用される時点で、child→owner 通知を stable ID／command event に分離できるか再監査する。ビルド・テストは未実施。

## 2026-08-31: 3D Model Viewer の表示モード通知を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Render/Artifact3DModelViewer.ixx`, `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`, `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cppm`
- 事実: Model Viewer の `displayModeChanged` は Contents Viewer の親側でヘッダ／surface metadata を更新するためだけに接続されていた。`EventBus::publish()` は同期 dispatch である。
- 判断: Qt signal を削除し、発信元ポインタと mode を持つ `ModelViewerDisplayModeChangedEvent` を publish した。Contents Viewer は自分の `modelViewer` と一致する event だけを購読し、他の viewer の表示を更新しない。
- 価値/懸念: 子 viewer から親の表示責務へ渡る状態通知を内部 EventBus に揃えられる。payload が widget pointer を持つため、非同期 queue へ送る用途には使わず、現在の同期 publish 前提を維持する。
- 次に確認: Model Viewer の combo／programmatic mode change、複数 Contents Viewer、破棄時の subscription disconnect を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Asset Browser の EventBus 移行済み選択経路を通常呼び出しへ統一

- 関連: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- 事実: Asset Browser の `selectionChanged()` は既に `AssetBrowserSelectionChangedEvent` を publish する通常メソッドだったが、ファイル選択の一経路だけ古い `emit selectionChanged(...)` 表記が残っていた。
- 判断: payload、publish 処理、Breadcrumb の小さな `pathClicked` Qt 境界は変更せず、残存箇所を `selectionChanged(...)` の直接呼び出しへ置換した。
- 価値/懸念: EventBus を正規経路とする Asset Browser の実装表記が揃う。新しい global event や接続は追加していない。
- 次に確認: Asset Browser の選択／ダブルクリック経路と EventBus 購読先を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: ToolBar の孤立 display mode signal を撤去

- 関連: `Artifact/include/Widgets/ArtifactToolBar.ixx`, `Artifact/src/Widgets/ArtifactToolBar.cppm`
- 事実: `displayModeChanged` は ToolBar 内で発火されていたが、リポジトリ内に購読先・接続先がなく、状態更新のための通信ではなかった。
- 判断: EventBus event を新設せず、未使用の Qt signal 宣言と発火だけを撤去した。`cameraToolRequested` など実際の操作入口と display mode の状態更新は維持した。
- 価値/懸念: 孤立した Qt 境界と不要な `emit` を減らせる。外部バイナリがこの内部 widget signal に依存する場合は別途 API 影響確認が必要だが、同一リポジトリ内の参照はない。
- 次に確認: ToolBar の表示モード変更が設定／呼び出し側で必要になった時点で、利用側の責務に応じて EventBus または通常 API を設計する。ビルド・テストは未実施。

## 2026-08-31: Timeline Navigator の viewport range 通知を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Timeline/ArtifactTimelineNavigatorWidget.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineNavigatorWidget.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: Navigator の start／end signal は Timeline 親の `syncTimelineViewportFromNavigator()` を呼び、viewport zoom と水平 offset を更新していた。親からの viewport 同期では `setStart`／`setEnd` が programmatic に呼ばれる。
- 判断: start／end を個別通知せず、mouse drag 中の実変更を `TimelineNavigatorRangeChangedEvent` として publish する形へ変更した。setter は無通知の状態反映に保ち、親の2本の Qt 接続を1本の EventBus 購読へ集約した。
- 価値/懸念: Timeline の viewport 操作面から親への表示状態通知を typed internal event に揃えられる。global event のため複数 Timeline が同居する場合は instance routing が必要だが、既存の single Timeline 前提は維持している。
- 次に確認: Navigator の handle trim／range move、viewport zoom、programmatic sync、Work Area／ScrubBar との表示同期を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Work Area Control の range command を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Timeline/ArtifactWorkAreaControlWidget.ixx`, `Artifact/src/Widgets/Timeline/ArtifactWorkAreaControlWidget.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: Work Area Control の start／end signal は Timeline 親で composition の work area range に変換されていた。composition からの programmatic 同期では `QSignalBlocker` と `setStart`／`setEnd` が使われている。
- 判断: start／end を個別に通知せず、ユーザーの mouse drag 中に現在の normalized range を一つの `TimelineWorkAreaChangeRequestedEvent` として publish する形へ変更した。setter は状態反映専用に保ち、親の重複 Qt 接続を一つの EventBus 購読へ集約した。frame 変換、clamp、最小幅、composition 更新は維持した。
- 価値/懸念: Work Area という独立した Timeline 操作面から composition への編集 command を typed event に揃えられる。global event のため複数 Timeline／composition が同時に存在する場合は composition／instance routing が必要になる。
- 次に確認: handle trim、body move／scale、programmatic composition sync、work area event の再入と undo／保存同期を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline ScrubBar の seek／drag lifecycle を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Timeline/ArtifactTimelineScrubBar.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: ScrubBar は Timeline 親へ `frameChanged`、`frameDragStarted`、`frameDragFinished` を渡し、親が seek、audio scrub 開始／位置更新／終了、停止後 preview を処理していた。既存の `TimelineSeekRequestedEvent` は他の Timeline seek 操作でも使われている。
- 判断: `TimelineSeekRequestedEvent` を ScrubBar のユーザー操作 frame 通知に再利用し、drag lifecycle 用に `TimelineScrubStartedEvent`／`TimelineScrubFinishedEvent` を追加した。親の3本の Qt 接続を EventBus 購読へ置き換え、programmatic `setCurrentFrame()` では event を publish しないことで再配送ループを避けた。seek の適用、audio scrub 更新、停止後 preview の順序は維持した。
- 価値/懸念: Timeline の独立した操作面から親への seek／lifecycle 通知を typed internal event に揃えられる。global bus のため複数 Timeline が同時に存在する場合は instance routing が必要で、今回も既存の single Timeline 前提を維持している。
- 次に確認: mouse seek、handle drag、keyboard seek、audio scrub、停止後 preview、外部 seek からの `setCurrentFrame()` が再帰せず同期することを runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline 親の EventBus 通知呼び出しを直接呼び出しへ統一

- 関連: `Artifact/include/Widgets/ArtifactTimelineWidget.ixx`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: Timeline 親の `zoomLevelChanged` と `timelineDebugMessage` は通常メソッドとして実装され、各々 `TimelineZoomLevelChangedEvent`／`TimelineDebugMessageEvent` を publish していたが、編集操作・検索・waveform・keyframe 操作の内部呼び出しに `Q_EMIT` 表記が残っていた。
- 判断: EventBus publish の実装、購読処理、payload、呼び出し順を変更せず、Timeline 実装内の `Q_EMIT` を通常のメソッド呼び出しへ置換した。Qt signal 宣言や標準 widget signal には触れていない。
- 価値/懸念: Timeline の widget 間通知が Qt signal に見える残骸をさらに減らし、EventBus を正規経路として明確化できる。大きな実装ファイルのため、差分は表記置換だけであることを静的に確認する必要がある。
- 次に確認: Timeline／TrackPainter／CurveEditor の `Q_EMIT` 残存、module hygiene、EventBus helper の整合をまとめて静的確認する。ビルド・テストは未実施。

## 2026-08-31: EventBus 化済み widget API の残存 Q_EMIT 表記を除去

- 関連: `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm`, `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`
- 事実: CurveEditor と Timeline TrackPainter の外向き API はすでに通常メソッドとして EventBus event を publish していたが、内部の呼び出し側に `Q_EMIT` 表記が残っていた。両ヘッダに対象メソッドの `W_SIGNAL` 宣言はない。
- 判断: 対象2ファイルだけ `Q_EMIT method(...)` を直接の `method(...)` 呼び出しへ機械的に置換した。既存の EventBus payload、呼び出し順、Qt signal の `shyToggled` など未移行経路は変更していない。
- 価値/懸念: EventBus 移行済み処理が Qt signal に見える残骸を減らし、将来の signal 接続追加を防ぎやすくする。TrackPainter は大きな実装ファイルのため、差分で置換範囲を限定して確認する必要がある。
- 次に確認: 対象ファイルの `Q_EMIT` 残存数、module self-import／post-module include、EventBus helper の実装整合を静的確認する。ビルド・テストは未実施。

## 2026-08-31: Playback Service の engine signal は内部処理、外部通知は EventBus 済み

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`, `Artifact/include/Event/ArtifactEventTypes.ixx`
- 事実: engine から Service への Qt 接続は、playback session／audio clock、composition frame 同期、RAM／disk cache、drop 集計など Service 内部の処理に使われている。外部 consumer 向けの state、frame、audio level 通知はそれぞれ `PlaybackStateChangedEvent`、`FrameChangedEvent`、`AudioLevelChangedEvent` として publish されている。
- 判断: engine→Service の内部 Qt 接続は widget 間境界ではないため保持し、同じ情報を二重に EventBus 化しない。既存の外部 EventBus 経路を正規の通知経路とする。
- 価値/懸念: worker thread からの frame backpressure、composition／cache の整合性、高頻度 audio level 配送を維持できる。内部 connection を外部 event と混同すると、thread affinity と処理順序を壊すリスクがある。
- 次に確認: 主要な custom signal 接続について、今回の分類で残存 Qt を「局所 UI」「内部 service」「高頻度通知」「未接続 API」に整理し、今後の移行対象を独立面間の実動作 command に限定する。ビルド・テストは未実施。

## 2026-08-31: Audio Mixer の strip／meter 通知は内部 Qt 境界として保持

- 関連: `Artifact/include/Audio/ArtifactAudioMixer.ixx`, `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm`, `Artifact/src/Audio/ArtifactAudioMixer.cppm`
- 事実: `AudioMixerChannelStrip`／`AudioMixerMasterBus` の volume、pan、mute、solo、level signal は、Composition Audio Mixer 内の strip row／master row が表示値・meter を同期するために購読している。`AudioMixer` 自身も strip／master の内部同期に同 signal を使っている。
- 判断: これらは独立 widget 間の command ではなく、Audio Mixer 内部の子部品同期と高頻度 meter 通知であるため、今回 global EventBus へ移行しない。編集確定や composition 状態の外部通知が必要になった場合だけ、集約済みの typed event を別単位で設計する。
- 価値/懸念: level の global 配送と instance routing を避け、既存の音量編集・undo・meter 更新の局所性を保てる。将来 Audio Mixer を複数 host で共有する場合は、UI 内 signal と外部状態 event を分離して再評価する。
- 次に確認: Playback の service→widget 通知は高頻度経路と状態更新経路を分けて監査し、既存 EventBus で代替できる低頻度通知だけを候補化する。ビルド・テストは未実施。

## 2026-08-31: 残存する小部品 signal は局所 Qt 境界として保持

- 関連: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`, `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`, `Artifact/src/Widgets/Render/ArtifactLayerEditorPanel.cppm`
- 事実: Project Manager の toolbox signal は直下のボタンから同じ親 widget の create／delete／proxy 処理へ、Asset Browser の breadcrumb signal は同じ Browser の folder navigation へ、Timeline の search／scrub signal は同じ Timeline の検索・playhead処理へ、Viewer Footer の snapshot signal は同じ editor panel の撮影処理へ接続されている。
- 判断: いずれもボタン／入力部品と所有親の局所 UI 境界であり、独立した widget 間 command やアプリ状態通知ではないため、Qt 接続を保持する。大きな境界へ広げるためだけに EventBus event を追加しない。
- 価値/懸念: EventBus の global routing と instance 識別を増やさず、検索・スクラブの高頻度処理や親子 UI の同期を局所に閉じ込められる。Project View の選択・double click など外部へ出る通知は既存の EventBus 経路を維持する。
- 次に確認: 残る独立面間の custom signal 接続を再検索し、サービス／独立 editor／複数 host にまたがるものだけを次の移行候補にする。ビルド・テストは未実施。

## 2026-08-31: ArtifactToolBar の未接続 signal は移行せず死蔵 API として監査

- 関連: `Artifact/include/Widgets/ArtifactToolBar.ixx`, `Artifact/src/Widgets/ArtifactToolBar.cppm`, `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- 事実: `cameraToolRequested` はカメラ action から発火するがリポジトリ内の購読先がなく、`viewModeChanged` は宣言のみ、`displayModeChanged` は setter 内で発火するだけだった。一方、workspace mode は `WorkspaceModeChangedEvent`、tool 選択は `ToolChangedEvent` に既に接続されている。
- 判断: 実動作する widget 間境界が存在しないため、3 signal を EventBus に置き換える作業は行わない。外部利用者や未実装のカメラ導線を壊さないよう、今回の段階では API を保持し、死蔵 API 整理候補として記録する。
- 価値/懸念: EventBus に購読者のない global event を増やさず、既存の workspace／tool 状態通知との重複も避けられる。将来カメラ操作や表示モードを複数 widget で実装する場合は、用途を確定してから typed command／state event を追加する必要がある。
- 次に確認: `ArtifactProjectManagerToolBox`、`ArtifactCompositionViewerFooter`、Timeline の検索・スクラブ部品に残る接続を、ボタン／子部品の局所境界として再確認する。ビルド・テストは未実施。

## 2026-08-31: 3D Viewer の display mode signal は局所 Qt 境界として保持

- 関連: `Artifact/include/Widgets/Render/Artifact3DModelViewer.ixx`, `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`, `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cppm`
- 事実: `Artifact3DModelViewer::displayModeChanged` の購読先は、所有元である `ArtifactContentsViewer` の `updateHeader()`／`updateSurfaceMeta()` を呼ぶ1本だけだった。アプリ全体の状態通知や別の独立ウィジェットへの command 伝播ではない。
- 判断: この signal は小さな子ビューと親コンテナの局所境界に該当するため、今回の EventBus 移行対象から外し、Qt 接続を保持する。widget を越えるように見えるだけで、内部 EventBus を増やさない。
- 価値/懸念: EventBus の global routing と instance 識別を増やさず、表示メタ情報の同期責務を所有元に閉じ込められる。将来、表示モードを Toolbar／Inspector／複数 Viewer が共有する場合は別の状態イベントとして再評価する。
- 次に確認: Toolbar の未購読 signal（camera tool／view mode／display mode）の意図と利用箇所を監査し、実動作のある大きな境界だけを次の移行単位にする。ビルド・テストは未実施。

## 2026-08-31: Layer Panel の重複 visible rows／vertical offset signal を撤去

- 関連: `Artifact/include/Widgets/Timeline/ArtifactLayerPanelWidget.ixx`, `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: Layer Panel は visible rows 変更時に既存の `TimelineVisibleRowsChangedEvent`、縦スクロール変更時に既存の `TimelineVerticalScrollEvent` を publish していた一方、panel から wrapper へ Qt signal を転送していた。Timeline 親はすでに EventBus を購読している。
- 判断: panel／wrapper の重複 signal と転送接続を削除した。LayerChanged の interactive 更新箇所は EventBus publish に置き換え、structure 更新の publish、縦スクロールの source／clamp／更新処理は維持した。
- 価値/懸念: Layer Panel → Timeline の状態通知を EventBus 一本にできる。global event のため複数 Timeline では composition／timeline instance routing が必要になる。
- 次に確認: layer create／remove、filter／search、縦スクロール、Timeline refresh、composition 切替時の同期を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Layer Panel の property focus 通知を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Timeline/ArtifactLayerPanelWidget.ixx`, `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: Layer Panel の `propertyFocusChanged` は wrapper が panel から受けて転送し、Timeline 親が TrackPainter の keyframe context と selection state を更新していた。wrapper の転送以外に同 signal の購読先はなかった。
- 判断: `TimelinePropertyFocusChangedEvent` に compositionId／layerId／propertyPath を持たせ、panel から直接 publish、wrapper の転送 signal と Timeline 親の2本の Qt 接続を削除した。親では compositionId を確認してから従来の2処理を同じ順序で実行する。
- 価値/懸念: Layer Panel → Timeline の focus 通知を internal event に揃え、global bus の誤配送リスクを composition 単位で抑えられる。将来同一 composition に複数 Timeline が存在する場合は timeline instance routing が追加で必要になる。
- 次に確認: property focus の変更、keyframe context の切替、selection state、composition 切替時のイベントフィルタを runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: CurveEditor の key delete command を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/ArtifactCurveEditorWidget.ixx`, `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: CurveEditor の `keyDeleted` は track／key index を親 Timeline に渡し、親が property lookup、frame-rate 変換、削除前後 snapshot、undo、失敗時復元、curve refresh を処理していた。
- 判断: `CurveEditorKeyDeletedEvent` を追加し、CurveEditor の signal を EventBus publish helper、親の Qt 接続を EventBus 購読へ変更した。削除条件、snapshot、undo、復元、refresh は維持した。
- 価値/懸念: CurveEditor → Timeline の編集 payload 通知を EventBus に揃えられる。global event のため複数 CurveEditor／Timeline では instance routing が必要になる。
- 次に確認: key delete、最後の key、undo／redo、無効 index、composition 切替時の refresh を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: CurveEditor の key move command を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/ArtifactCurveEditorWidget.ixx`, `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: CurveEditor の `keyMoved` は track／key index、新 frame、新 value を親 Timeline に渡し、親が validation、`applyCurveEditorMove()`、cached track 更新、refresh timer を実行していた。
- 判断: `CurveEditorKeyMovedEvent` を追加し、CurveEditor の signal を EventBus publish helper、親の Qt 接続を EventBus 購読へ変更した。validation、property 更新、cache 更新、timer interval は維持した。keyDeleted は別単位として残した。
- 価値/懸念: curve key 編集の widget 間 command を typed internal event に移せる。global event のため複数 CurveEditor／Timeline では instance routing が必要になる。
- 次に確認: drag／tangent 操作による key move、frame／value 更新、refresh timer、undo session との連携を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: CurveEditor の key selection 通知を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/ArtifactCurveEditorWidget.ixx`, `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: CurveEditor の `keySelected` は親 Timeline の curve property list focus／scroll にだけ使われ、keyIndex 自体は親処理で参照されていなかった。
- 判断: `CurveEditorKeySelectedEvent` を追加し、CurveEditor の signal を EventBus publish helper、親の Qt 接続を EventBus 購読へ変更した。track focus、list 更新、scroll 条件は維持した。
- 価値/懸念: curve focus の widget 間通知を typed internal event に移せる。global event のため複数 CurveEditor／Timeline では instance routing が必要になる。
- 次に確認: key click、shift selection、property list の focus／scroll、curve context 切替を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: CurveEditor の current frame 通知を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/ArtifactCurveEditorWidget.ixx`, `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: CurveEditor の `currentFrameChanged` は Timeline 親で frame 全体の表示更新、playhead 同期、Active Context の seek に使われていた。既存の `TimelineSeekRequestedEvent` は audio preview と interactive seek dedup を含むため処理は同一ではない。
- 判断: `CurveEditorCurrentFrameChangedEvent` を追加し、CurveEditor の signal を EventBus publish helper、親の Qt 接続を EventBus 購読へ変更した。frame 更新と context seek の順序・内容は維持した。
- 価値/懸念: CurveEditor の playhead 通知を widget 間 Qt 接続なしで共有できる。global event のため複数 CurveEditor／Timeline では instance routing が必要になる。
- 次に確認: CurveEditor の playhead scrub、frame 表示、context seek、Timeline scrub／playback との同期を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: CurveEditor の interaction boundary を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/ArtifactCurveEditorWidget.ixx`, `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: CurveEditor の `interactionStarted/Finished` は Timeline 親が curve 編集の undo snapshot、drag state、refresh timer を管理するために購読していた。key 移動／削除／focus 通知は別の payload と UI 責務を持つ。
- 判断: 開始／終了の2 event を追加し、CurveEditor の Qt signal を EventBus publish helper、Timeline 親の2接続を EventBus 購読へ変更した。undo snapshot、selection restore、refresh、既存の key payload signal は維持した。
- 価値/懸念: curve 編集セッションの widget 間境界を Qt signal なしで共有できる。global event のため複数 CurveEditor／Timeline が同居する場合は instance routing が必要になる。
- 次に確認: curve drag、tangent／handle 編集、undo snapshot の開始・確定、途中キャンセル、refresh timer の挙動を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Composition Render Controller の debug 通知を EventBus 化

- 関連: `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `Artifact/include/Widgets/Render/ArtifactCompositionEditor.ixx`, `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 事実: Render Controller は `videoDebugMessage` Qt signal を発火し、CompositionEditor が受け取って同じ `TimelineDebugMessageEvent` を publish、StatusBar が EventBus を購読していた。
- 判断: Controller の signal を EventBus publish を行う通常メソッドへ変更し、Editor の中継接続・中継メソッドを削除した。render loop の debug 判定、重複抑制、ログ出力は維持した。
- 価値/懸念: controller → editor → status bar の Qt 中継をなくし、既存 internal event に一本化できる。event 名は既存互換のため維持しており、将来 video／timeline debug の分類が必要なら別 event を検討する。
- 次に確認: GPU／fallback render の debug message 到達、StatusBar 表示、CompositionEditor 破棄時の安全性を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline Track Painter の keyframe move command を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: `keyframeMoveRequested` は TrackPainter が移動結果ごとに layerId／propertyPath／fromFrame／toFrame を渡し、Timeline 親が validation、snapshot undo、selection restore、refresh、debug 通知を処理していた。
- 判断: `TimelineKeyframeMoveRequestedEvent` を追加し、TrackPainter の signal を EventBus publish helper、親の Qt 接続を EventBus 購読へ変更した。親の validation、undo／redo、失敗時 refresh、メッセージ内容は維持した。
- 価値/懸念: keyframe 編集 command の widget 間境界を typed internal event に移せる。global event のため複数 Timeline では composition／timeline instance の routing が必要になる。
- 次に確認: 単一／複数 keyframe の移動、既存 keyframe への merge、undo／redo、選択復元、無効 target の扱いを runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline Track Painter の clip slide command を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: `clipSlid` は TrackPainter の slide 完了時に clipId／startFrame を渡し、Timeline 親は composition lookup、timing lock 判定、snapshot 取得、`SlideClipCommand` の undo 分岐を実行していた。
- 判断: `TimelineClipSlideRequestedEvent` を追加し、TrackPainter の signal を EventBus publish helper、親の Qt 接続を EventBus 購読へ変更した。snapshot、lock 判定、undo／redo、失敗時の早期 return は維持した。
- 価値/懸念: slide 編集の widget 間 command を typed internal event に移せる。global event のため複数 Timeline では composition／timeline instance の routing が必要になる。
- 次に確認: clip slide の境界条件、timing lock、undo／redo、composition 切替中の誤適用がないことを runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline Track Painter の clip resize command を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: `clipResized` は TrackPainter の resize 完了時に発火し、Timeline 親は `applyTimelineLayerTrim()` を呼ぶだけだった。
- 判断: `TimelineClipResizeRequestedEvent` を追加し、TrackPainter の signal を EventBus publish helper、親の Qt 接続を EventBus 購読へ変更した。clip slide の undo snapshot 処理は別単位として残した。
- 価値/懸念: clip resize の widget 間 command を typed internal event に移せる。global event のため複数 Timeline では composition／timeline instance の routing が必要になる。
- 次に確認: clip resize の trim clamp、undo／redo、選択状態、他 Timeline UI の更新を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline Track Painter の clip move command を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: `clipMoved` は TrackPainter の move 操作完了時に発火し、Timeline 親は `applyTimelineLayerMove(compositionId, clipId, startFrame, 0.0)` を呼ぶだけだった。
- 判断: `TimelineClipMoveRequestedEvent` を追加し、TrackPainter の signal を EventBus publish helper、親の Qt 接続を EventBus 購読へ置き換えた。slide／resize は異なる編集ロジックを持つため今回分離した。
- 価値/懸念: レイヤー移動という widget 間 command を typed internal event に移せる。global event のため複数 Timeline では composition／timeline instance の routing が必要になる。
- 次に確認: clip move の undo、開始フレーム、選択状態、他 Timeline UI の更新を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline Track Painter の layer selection request を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: TrackPainter の `clipSelected` は clip／keyframe marker のクリックから発火し、Timeline 親は clipId を使わず layerId だけで `ArtifactProjectService::selectLayer()` を呼んでいた。親の `syncingLayerSelection_` ガードもこの接続に含まれていた。
- 判断: 結果通知の `LayerSelectionChangedEvent` とは分けて `TimelineLayerSelectionRequestedEvent` を追加し、TrackPainter の signal を publish helper、親の Qt 接続を EventBus 購読へ変更した。clipId も将来の routing 用に event に保持した。
- 価値/懸念: Timeline surface から親への選択依頼を Qt signal なしで表現できる。global event のため複数 Timeline では layerId だけでなく composition／timeline instance の routing が必要になる。
- 次に確認: clip／marker／keyframe area のクリック、modifier 選択、layer tree との相互同期、再入防止を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline Track Painter の keyframe selection 通知を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: `keyframeSelectionChanged` は TrackPainter の複数の選択変更箇所から発火し、Timeline 親は選択数を使わず `updateKeyframeState()` と `updateSelectionState()` を実行していた。
- 判断: `TimelineKeyframeSelectionChangedEvent` を追加し、TrackPainter の同名 Qt signal を EventBus publish helper に変更、親の Qt 接続を EventBus 購読へ置き換えた。各選択操作と選択状態更新の順序は維持した。
- 価値/懸念: keyframe selection の widget 間通知を typed internal event に移せる。global event のため複数 Timeline では instance routing が必要になり、イベントの selectedCount は現状将来の購読用に保持している。
- 次に確認: marquee／marker／keyframe 移動後の選択表示、Inspector 同期、選択解除時の更新を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline Track Painter の row height 通知を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: TrackPainter の `trackRowHeightChanged` は行高変更時に発火し、Timeline 親は Layer Panel の row height を設定するだけだった。既存の親子 Qt 接続以外の購読先は確認できなかった。
- 判断: `TimelineTrackRowHeightChangedEvent` を追加して TrackPainter が publish、Timeline 親が購読する経路へ変更した。最小行高 16 の既存 clamp と行高計算は維持した。
- 価値/懸念: 行高変更の親子 widget 通知を typed internal event に移せる。global event のため複数 Timeline が同居する場合は instance routing が必要になる。
- 次に確認: 行高ドラッグ、Layer Panel の追従、再描画とスクロール位置を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline Track Painter の重複 vertical offset signal を撤去

- 関連: `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: TrackPainter の `verticalOffsetChanged` は発火と同じ箇所で既存の `TimelineVerticalScrollEvent` を publish しており、Timeline 親は Qt 接続ではなく EventBus 購読で両パネルを同期している。
- 判断: 重複する W_SIGNAL 宣言と発火だけを削除し、`TimelineVerticalScrollEvent`、source 識別、再入防止付き同期処理は維持した。
- 価値/懸念: 縦スクロールの widget 間経路を EventBus 一本にできる。global event のため複数 Timeline が同居する場合は instance routing が必要になる。
- 次に確認: TrackPainter／LayerPanel の縦スクロール相互同期と再入防止を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline Track Painter の debug 通知を EventBus 化

- 関連: `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: TrackPainter の `timelineDebugMessage` は Qt signal で Timeline 親へ渡され、親の同名メソッドが既存の `TimelineDebugMessageEvent` を publish していた。StatusBar と CompositionEditor は既存 EventBus を購読している。
- 判断: TrackPainter の Qt signal 宣言を通常の内部 publish helper に変更し、同メソッドから EventBus へ直接 publish、Timeline 親の Qt 接続を削除した。既存のメッセージ生成箇所と親自身の debug publish は維持した。
- 価値/懸念: Timeline surface から StatusBar 等へ伝播する debug 通知を widget 間 Qt 接続なしで共有できる。global event のため複数 Timeline のログ識別が必要になった場合は composition／timeline instance の routing を追加検討する。
- 次に確認: keyframe／clip 編集時の debug 表示、StatusBar への到達、親 widget の破棄時に購読が残らないことを runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline Track Painter の zoom 通知を EventBus 化

- 関連: `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: TrackPainter の zoom 操作は `zoomLevelChanged` Qt signal で Timeline 親へ渡され、親が ruler／navigator／zoom summary／playhead を同期してから既存の `TimelineZoomLevelChangedEvent` を投稿していた。StatusBar も同 EventBus を購読している。
- 判断: TrackPainter の zoom signal と親の Qt 接続を削除し、TrackPainter が既存 EventBus event を publish、Timeline 親が同 event を購読して従来の UI 同期処理を行う経路へ変更した。親自身の zoom 操作が既存の publish helper を通る処理は維持した。
- 価値/懸念: 大きな Timeline surface から親 widget への zoom 通知を Qt 接続なしで共有できる。global event のため将来複数 Timeline が存在する場合は composition／timeline instance の routing が必要になる。
- 次に確認: wheel／shortcut／navigator 操作時の ruler、navigator、zoom summary、playhead、StatusBar の同期を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline Track Painter の未接続 deselect signal を撤去

- 関連: `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: `clipDeselected` は TrackPainter の背景クリック時に発火していたが、Timeline 親の接続処理は `Q_UNUSED(this)` の no-op で、他の購読先も確認できなかった。選択状態の解除自体は TrackPainter 内部で別途処理されている。
- 判断: `clipDeselected` の W_SIGNAL 宣言、発火、no-op 接続だけを削除した。実際に親へ選択を伝える `clipSelected` と、編集結果を伝える clip／keyframe signal は今回の単位に含めず残した。
- 価値/懸念: widget 外へ意味のある情報を伝えていない Qt signal を減らせる。リポジトリ外 plugin が signal を参照していた可能性は静的検索では保証できない。
- 次に確認: Timeline の背景クリックによる選択解除、marquee 選択、既存の layer selection 同期を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Toolbar／View Menu の Grid／Guide command を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/src/Widgets/ArtifactToolBar.cppm`, `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 事実: Toolbar と View Menu の Grid／Guide QAction は、CompositionEditor の `CompositionRenderController` と同じ表示状態を変更していた。Toolbar の旧 signal には購読先がなく、View Menu は active editor を直接参照していた。
- 判断: 既存の `CompositionViewCommandRequestedEvent` に表示状態 command と `visible` 値を追加し、両 UI から EventBus へ publish、CompositionEditor で主／副 render controller と AppSettings に適用する経路へ揃えた。対応済みの Toolbar Grid／Guide signal は削除し、Camera と未発火の View mode signal は対象を確定できないため保留した。
- 価値/懸念: widget 間の表示変更を Qt signal／直接 widget 参照から内部 command に寄せられる。CompositionEditor が複数になる場合は、現行の global subscription と active-view routing を見直す必要がある。Toolbar 固有設定と viewport 設定の既存キー差は今回維持した。
- 次に確認: Toolbar／View Menu の Grid／Guide 操作、複数 preview の同期、再起動後の設定復元を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Secondary Preview の未接続 Qt 通知を撤去

- 関連: `Artifact/include/Widgets/ArtifactSecondaryPreviewWindow.ixx`, `Artifact/src/Widgets/ArtifactSecondaryPreviewWindow.cppm`
- 事実: `closed` と `fullscreenToggled` は Secondary Preview Window 内で発火するだけで、リポジトリ内の購読先がなかった。View Menu はウィンドウを保持・表示するが、これらの通知には接続していない。
- 判断: 2本の W_SIGNAL 宣言と発火を削除し、フルスクリーン切替、OSD 表示、closeEvent、preview 更新の処理は維持した。購読先のない EventBus 通知も追加していない。
- 価値/懸念: 画面外へ伝播しない Qt 通知を公開面から減らせる。リポジトリ外 plugin が signal を参照していた可能性は静的検索では保証できない。
- 次に確認: Secondary Preview の表示／終了／フルスクリーン操作が既存の View Menu 管理で継続することを runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Looks Preset Browser の未接続 Qt 通知を撤去

- 関連: `Artifact/include/Widgets/ArtifactLooksPresetBrowser.ixx`, `Artifact/src/Widgets/ArtifactLooksPresetBrowser.cppm`
- 事実: `presetApplied` と `presetFavorited` はダイアログ内で発火するだけで、リポジトリ内に `LooksPresetBrowserDialog` の生成箇所や購読先がなかった。
- 判断: 2本の W_SIGNAL 宣言と発火を削除し、preset の選択、適用表示、favorite 切替、thumbnail 更新は変更していない。未使用の EventBus 通知は追加していない。
- 価値/懸念: 実際に接続されていない Qt API を公開面から減らせる。リポジトリ外からこのダイアログを生成・購読していた可能性は静的検索では保証できない。
- 次に確認: 将来この dialog を実配置する際は、適用結果を返す dialog API または内部 command の責務を先に定義する。ビルド・テストは未実施。

## 2026-08-31: Color Swatch の重複 Qt 通知を撤去

- 関連: `Artifact/include/Widgets/Color/ArtifactColorSwatchWidget.ixx`, `Artifact/src/Widgets/Color/ArtifactColorSwatchWidget.cppm`
- 事実: 色選択と swatch 変更は既存の `ColorSwatchSelectedEvent`／`ColorSwatchChangedEvent` を EventBus へ投稿しており、Qt の `colorSelected`／`swatchChanged` は同じ処理で重複発火していた。ウィジェットの生成・購読箇所もリポジトリ内に確認できなかった。
- 判断: 2本の W_SIGNAL 宣言と発火を削除し、既存 EventBus 投稿、GPL の load/save、clear、一覧更新は維持した。
- 価値/懸念: widget 間の色変更通知を Qt と EventBus の二重経路にせず、既存の内部イベントへ揃えられる。リポジトリ外 plugin が Qt signal を参照していた可能性は静的検索では保証できない。
- 次に確認: 色選択・palette load・clear の購読側が既存 EventBus だけで必要な更新を行うことを runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Font Picker の重複 Qt 通知を撤去

- 関連: `Artifact/include/Widgets/ArtifactFontPickerWidget.ixx`, `Artifact/src/Widgets/ArtifactFontPickerWidget.cppm`, `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorString.cppm`
- 事実: Font Picker の選択処理は既存の `FontChangedEvent` を EventBus へ投稿しており、Qt の `fontChanged` は発火するだけだった。唯一の参照は `if (false)` 内の旧 Property Editor 接続だった。
- 判断: `fontChanged` の W_SIGNAL 宣言と発火、およびそれを参照していた `if (false)` の旧接続を削除し、既存 EventBus 投稿と Property Editor の購読、picker の選択 UI は維持した。新しい signal／slot 接続や代替イベントは追加していない。
- 価値/懸念: font 選択の widget 外通知を Qt と EventBus の二重経路から内部 EventBus に揃えられる。リポジトリ外 plugin が signal を参照していた可能性は静的検索では保証できない。
- 次に確認: Property Editor の font 選択が現行 EventBus 購読設計で必要な変更を反映することを runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Color Palette の未接続 signal を撤去

- 関連: `Artifact/include/Widgets/ArtifactColorPaletteWidget.ixx`, `Artifact/src/Widgets/ArtifactColorPaletteWidget.cppm`
- 事実: `paletteSelected` は宣言のみで、`ArtifactColorPaletteWidget` の実装内に発火箇所もリポジトリ内の購読先もなかった。Palette Widget 自体は View Menu から生成される。
- 判断: 未接続の W_SIGNAL 宣言だけを削除し、palette manager の設定、harmonic／smart extract、load／save、一覧更新は変更していない。購読先のない EventBus 通知は追加していない。
- 価値/懸念: 実装されていない Qt 通知を公開面から除き、実際の palette 操作責務に合わせられる。リポジトリ外 plugin が signal を参照していた可能性は静的検索では保証できない。
- 次に確認: Palette Widget の操作と View Menu の dock 表示を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline Global Switches の Qt 境界を EventBus 化

- 関連: `Artifact/include/Widgets/ArtifactTimelineGlobalSwitches.ixx`, `Artifact/src/Widgets/ArtifactTimelineGlobalSwitches.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: Global Switches の shy／motion blur／frame blending／graph editor は既存の4種類の EventBus state event を投稿していた。Timeline は shy について EventBus 購読済み、graph editor については親 widget への Qt 直接接続が残っていた。motion blur／frame blending の widget 外 Qt 購読は確認できなかった。
- 判断: 4本の Qt signal と発火を削除し、Timeline の無効化済み shy 接続を撤去、graph editor の親処理を `TimelineGraphEditorToggledEvent` 購読へ移した。既存の settings 更新と EventBus 投稿、Timeline の graph 表示切替処理は維持した。
- 価値/懸念: Global Switches から親 Timeline への状態通知を内部 EventBus に一本化できる。EventBus の queued dispatch に合わせたため、graph 表示切替のタイミングは旧 direct connection と異なり得る。ビルド未確認のため module／wobject 整合を静的確認する。
- 次に確認: shy／graph の表示切替、shortcut、curve editor の focus／fit、設定復元を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Timeline Track Painter の seek command を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`, `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- 事実: `seekRequested` は TrackPainter のクリック／scrub 処理から発火し、Timeline 親が購読して `applyTimelineSeek` を呼んでいた。また親自身の shortcut／wheel 処理も同じ signal を直接呼び出していた。
- 判断: `TimelineSeekRequestedEvent` を追加し、TrackPainter と親の手動発火を EventBus publish に変更、Timeline 親は同 event を購読するようにした。seek の clamp、audio preview、playback 呼び出し、playhead 更新処理は維持し、他の clip／keyframe signal は今回触っていない。
- 価値/懸念: 大きな Timeline surface から親 widget への seek 通知を Qt signal から内部 command に移せる。global event のため将来複数 Timeline が存在する場合は composition／timeline instance の routing が必要になる。
- 次に確認: クリック、scrub-preview、wheel／shortcut seek の反応順、audio preview、frame clamp を runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: Toolbar の重複 current-tool signal を撤去

- 関連: `Artifact/include/Widgets/ArtifactToolBar.ixx`, `Artifact/src/Widgets/ArtifactToolBar.cppm`
- 事実: `currentToolChanged` は `setCurrentTool()` から発火するだけで購読先がなく、ツール選択の状態通知は既存の `ToolChangedEvent` が発行・購読されている。
- 判断: 重複する W_SIGNAL 宣言と発火を削除した。Toolbar の action 更新、ToolService の選択処理、ToolChangedEvent の経路は維持した。
- 価値/懸念: 同じツール状態を Qt signal と EventBus の二重 API で持たず、widget 間の正規経路を一本化できる。外部 plugin が signal を参照していた可能性は静的検索では保証できない。
- 次に確認: Toolbar 選択、CompositionEditor の tool label／edit mode 同期が ToolChangedEvent だけで継続することを runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: 未接続 Layer Menu の nullLayerCreated signal を撤去

- 関連: `Artifact/include/Widgets/Menu/ArtifactLayerMenu.ixx`, `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`
- 事実: `nullLayerCreated` は Null layer 作成後に発火するだけで、リポジトリ内の購読先がなかった。作成処理は既存の Project Service 呼び出しで完了している。
- 判断: W_SIGNAL 宣言と発火を削除し、Menu の QAction 接続と Null layer 作成処理は維持した。購読先のない EventBus 通知も追加していない。
- 価値/懸念: Layer Menu の未使用 Qt 通知を減らし、実際の責務に合わせられる。外部 plugin がこの signal を参照していた可能性は静的検索では保証できない。
- 次に確認: Null layer 作成後の selection／timeline／inspector 更新が既存の service／state event 経路で継続することを runtime で確認する。ビルド・テストは未実施。

## 2026-08-31: 未接続 Project Memo jump signal を撤去

- 関連: `Artifact/include/Widgets/ArtifactProjectMemoWidget.ixx`, `Artifact/src/Widgets/ArtifactProjectMemoWidget.cppm`
- 事実: `memoJumpRequested(qint64)` はダブルクリック時に発火するだけで購読先がなく、`ArtifactProjectMemoWidget` のアプリ内生成箇所も確認できなかった。Timeline 側にはフレーム移動 API があるが、この Memo UI との接続は存在しない。
- 判断: 未接続の W_SIGNAL 宣言と発火を削除した。購読先のない新しい EventBus command は追加せず、Memo の表示・追加・編集・削除処理は変更していない。
- 価値/懸念: 未接続の Qt widget API を増やさず、実装実態に合わせられる。将来 Memo を実際に配置する場合は、composition context を含む Timeline jump command を設計してから再導入する必要がある。
- 次に確認: Memo surface を再接続する段階で、current composition と frame seek の対象境界を定義する。ビルド・テストは未実施。

## 2026-08-31: Asset Browser の未使用 folder/drop signal を撤去

- 関連: `Artifact/include/Widgets/ArtifactAssetBrowser.ixx`, `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- 事実: `folderChanged` と `filesDropped` は実装内で発火するだけで、リポジトリ内に購読先がなかった。Breadcrumb の `pathClicked` は Asset Browser 親子内の接続であり、今回の対象外だった。
- 判断: 2本の W_SIGNAL 宣言と発火を削除し、非同期 import の callback は既存の空 callback 形式へ揃えた。navigate／import／selection／double-click の既存 EventBus 経路は変更していない。
- 価値/懸念: Asset Browser の公開面から実動作のない Qt signal を減らせる。リポジトリ外 plugin がこれらを購読していた可能性は静的検索では保証できない。
- 次に確認: Asset Browser の import 完了後の表示更新、folder navigation、外部 API として signal を残す必要性を runtime／API 方針で確認する。ビルド・テストは未実施。

## 2026-08-31: Toolbar の View command を EventBus 化

- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/src/Widgets/ArtifactToolBar.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 事実: Toolbar の Home／Zoom 5 QAction は `ArtifactCompositionEditor` の reset／zoom 操作に対応する一方、旧 `homeRequested`／`zoom*Requested` signal の購読先は存在しなかった。CompositionEditor は実アプリ内で生成箇所が1つで、同じ操作メソッドを既に持つ。
- 判断: `CompositionViewCommandKind` と `CompositionViewCommandRequestedEvent` を追加し、Toolbar から EventBus へ publish、CompositionEditor で購読して既存メソッドへ委譲した。Camera は `ToolType` に対応する値も実処理もないため、今回の移行対象から外した。
- 価値/懸念: Menu／Toolbar などの widget 間 command 配線を Qt signal に依存せず統一できる。将来 CompositionEditor が複数になる場合は、イベントの対象識別子または active-view routing が必要になる。
- 次に確認: 静的に旧5 signal の宣言・発火・購読が残っていないこと、EventBus subscription の module／wobject 整合を確認する。実行確認はビルド許可後に行う。

## ProjectManager の未使用 file-drop signal を撤去

- 日付: 2026-08-31
- 関連: `Artifact/include/Widgets/ArtifactProjectManagerWidget.ixx`, `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- 確認できた事実: `ArtifactProjectManagerWidget::onFileDropped(const QStringList&)` は宣言以外に発火・購読・実装参照がなく、ドロップ処理は子の ProjectView 側で処理されていた。
- 判断・仮説: EventBus 化する実動作がないため、死蔵していた W_SIGNAL 宣言だけを削除した。ProjectView→ProjectManager の親子接続と既存の ProjectItemActivatedEvent 経路は変更していない。
- 価値・懸念: 未使用の Qt signal API を減らし、ProjectManager の公開面を実装実態へ合わせられる。外部 plugin がこの宣言を使っていた可能性はリポジトリ内検索では保証できない。
- 次に確認すべきこと: Project View の file drop から import／asset 更新が従来どおり行われ、外部 ABI として file-drop signal を提供する要件がないことを runtime／API 方針で確認する。ビルド・テストは未実施。

## AnimationMenu の未使用 expression/preset signal 6本を撤去

- 日付: 2026-08-31
- 関連: `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx`, `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`
- 確認できた事実: `addExpressionRequested`／`editExpressionRequested`／`removeExpressionRequested`／`convertToKeyframesRequested`／`saveAnimationPresetRequested`／`loadAnimationPresetRequested` は宣言以外にリポジトリ内の発火・購読参照がなく、該当 QAction は AnimationMenu 内の直接処理へ分岐していた。
- 判断・仮説: EventBus へ置き換える実動作が存在しないため、6本の W_SIGNAL 宣言だけを撤去した。expression 操作、keyframe 変換、preset 保存／適用の既存処理は変更していない。
- 価値・懸念: 死蔵 Qt API を減らし、AnimationMenu の公開面を実際の責務に近づけられる。リポジトリ外の plugin が宣言を参照する可能性は検索では保証できないため、外部 ABI を提供する場合は別途互換方針が必要になる。
- 次に確認すべきこと: 各 expression/preset QAction が従来どおり直接処理され、外部連携を正式に提供する必要がないことを runtime／API 方針で確認する。ビルド・テストは未実施。

## 補間適用要求を型付き EventBus に移行

- 日付: 2026-08-31
- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx`, `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`, `Artifact/src/Widgets/ArtifactMenuBar.cppm`, `ArtifactCore/include/Geometry/Interpolate.ixx`
- 確認できた事実: Linear／EaseIn／EaseOut／EaseInOut／Constant／Bezier の6 QAction は1本の `applyInterpolationRequested(InterpolationType)` signal に集約され、MenuBar の1箇所だけが購読して Timeline の同じメソッドを呼んでいた。`InterpolationType` は依存のない Core の `Math.Interpolate` module で定義されている。
- 判断・仮説: `TimelineInterpolationCommandRequestedEvent` の payload に `ArtifactCore::InterpolationType` を直接保持し、整数化せずに EventBus へ移行した。不要になった `W_REGISTER_ARGTYPE` も削除し、補間適用処理と QAction 内部の `QMenu::triggered` 接続は維持した。
- 価値・懸念: 補間種別の型安全性を維持したまま Menu と Timeline 間の Qt signal を除去できる。Event Types module が `Math.Interpolate` を import するため依存は増えるが、同 module に Event／UI 依存はなく循環リスクは低いと判断した。
- 次に確認すべきこと: 6つの補間項目が選択キーフレームへ従来と同じ補間を適用し、Bezier を含む既存の値と Undo 通知が変わらないことを runtime で確認する。ビルド・テストは未実施。

## 時間リマップ要求3本を EventBus command 化

- 日付: 2026-08-31
- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx`, `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`, `Artifact/src/Widgets/ArtifactMenuBar.cppm`
- 確認できた事実: 時間リマップ有効化、フリーズフレーム、逆再生の要求は MenuBar の3本の Qt signal 接続だけが購読し、選択レイヤーと現在コンポジションを使う既存処理へ転送していた。
- 判断・仮説: `TimelineTimeRemapCommandRequestedEvent` を追加し、3操作を `Enable`／`Freeze`／`Reverse` に分けて AnimationMenu から発行し、MenuBar の購読で従来の処理へ振り分けた。レイヤーのキー生成・クリア・有効化の順序は維持した。
- 価値・懸念: Menu とレイヤー／Timeline のウィジェット間 Qt signal を減らし、時間リマップ要求の責務を型付き command に移せる。要求は現在の選択レイヤーとコンポジションを購読時点で解決するため、別 surface から発行する場合も同じ selection context を意図しているか確認が必要になる。
- 次に確認すべきこと: 3つのメニュー項目で時間リマップのキー結果、単一フレーム時の逆再生、Undo／dirty 通知が従来どおりになることを runtime で確認する。ビルド・テストは未実施。

## AnimationMenu のグラフ操作3本を command EventBus 化

- 日付: 2026-08-31
- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx`, `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`, `Artifact/src/Widgets/ArtifactMenuBar.cppm`, `Artifact/src/Widgets/ArtifactTimelineGlobalSwitches.cppm`
- 確認できた事実: グラフエディタ表示、値グラフ表示、速度グラフ表示の3要求は MenuBar の Qt signal 接続だけが購読していた。既存の `TimelineGraphEditorToggledEvent` は GlobalSwitches の状態変更通知として発行されていた。
- 判断・仮説: 状態通知と要求を混同しないため、`TimelineGraphCommandRequestedEvent` を新設し、3操作を `ShowEditor`／`ShowValue`／`ShowSpeed` に分けて発行・購読した。GlobalSwitches の既存 state event と Timeline の既存表示処理は変更していない。
- 価値・懸念: Menu と Timeline／GlobalSwitches のウィジェット間 Qt signal を減らしつつ、状態通知と command intent の境界を保てる。現在は UI thread の同期 publish を前提にしており、別 thread から同じ要求を発行する場合は UI marshal が必要になる。
- 次に確認すべきこと: 3つの Animation メニュー項目が従来どおりグラフモードと表示状態を切り替え、GlobalSwitches の state 通知と二重実行しないことを runtime で確認する。ビルド・テストは未実施。

## キーフレーム反転要求4本を共通 EventBus command に統合

- 日付: 2026-08-31
- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx`, `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`, `Artifact/src/Widgets/ArtifactMenuBar.cppm`
- 確認できた事実: 選択キーフレーム、現在レイヤー、選択レイヤー、コンポジション全体の反転要求は、MenuBar の4本の Qt signal 接続だけが購読し、それぞれ既存の Timeline 反転メソッドへ転送していた。
- 判断・仮説: 既存の `TimelineKeyframeEditCommandRequestedEvent` に4種類の反転 command を加え、AnimationMenu から発行し、MenuBar の同じ購読で Timeline 呼び出しへ振り分けた。先行して移行した基本編集5本と同じ command event にまとめ、反転処理自体は変更していない。
- 価値・懸念: AnimationMenu と Timeline 間の Qt signal をさらに4本減らし、キーフレーム編集要求の語彙を一箇所へ集約できる。event enum を拡張するたびに購読側の switch を更新する必要があるため、将来 command 数が増えすぎる場合は別の command routing 分割を検討する。
- 次に確認すべきこと: 4つの反転メニュー項目が対象範囲だけを従来どおり反転し、Undo／選択状態／空選択時の挙動が変わらないことを runtime で確認する。ビルド・テストは未実施。

## 基本キーフレーム編集要求5本を EventBus 化

- 日付: 2026-08-31
- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx`, `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`, `Artifact/src/Widgets/ArtifactMenuBar.cppm`
- 確認できた事実: AnimationMenu の追加／削除／全選択／コピー／貼り付け要求は、MenuBar の5本の Qt signal 接続だけが購読先で、それぞれ既存の Timeline 操作へ1対1で転送されていた。
- 判断・仮説: `TimelineKeyframeEditCommandRequestedEvent` と種別 enum を追加し、AnimationMenu は QAction 操作時に発行、MenuBar は既存の Timeline メソッドへ振り分ける形にした。局所的な QAction→QMenu の接続、実際の編集処理、ナビゲーション4本の EventBus 化は維持した。
- 価値・懸念: 基本キーフレーム操作のウィジェット間 Qt signal を型付き要求へまとめられる。現在は UI thread の同期 publish と MenuBar の購読寿命を前提にしており、外部 thread 発行や別の実行コンテキストを追加する場合は UI marshal と重複購読の確認が必要になる。
- 次に確認すべきこと: 5つのメニュー項目が従来どおり現在の Timeline に対して一度だけ作用し、選択・クリップボード・Undo 状態が壊れないことを runtime で確認する。ビルド・テストは未実施。

## Timeline キーフレーム移動要求4本を EventBus 化

- 日付: 2026-08-31
- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx`, `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`, `Artifact/src/Widgets/ArtifactMenuBar.cppm`
- 確認できた事実: `ArtifactAnimationMenu` の次／前／先頭／末尾キーフレーム要求は、`ArtifactMenuBar` の4本の Qt signal 接続だけが購読先で、MenuBar は対応する既存 Timeline 操作を呼び出していた。
- 判断・仮説: `TimelineKeyframeNavigationRequestedEvent` と種別 enum を追加し、AnimationMenu は QAction 操作時に EventBus へ発行、MenuBar は Impl の購読寿命内で同じ Timeline 呼び出しへ振り分ける形にした。QAction→AnimationMenu の局所接続と既存操作は維持した。
- 価値・懸念: メニューと Timeline のウィジェット間 Qt signal を1つの型付き要求へ置き換え、将来別の command surface からも同じ要求へ合流できる。EventBus は UI thread の同期 publish を前提にしており、外部 thread から発行する経路を追加する場合は UI marshal が必要になる。
- 次に確認すべきこと: 4つのメニュー項目から Timeline が従来と同じ方向・位置へ移動し、購読の再生成や MenuBar 破棄後に不正 callback が起きないことを runtime で確認する。ビルド・テストは未実施。

## Toolbar の未使用 tool request signal 10 本を撤去

- 日付: 2026-08-31
- 関連: `Artifact/include/Widgets/ArtifactToolBar.ixx`, `Artifact/src/Widgets/ArtifactToolBar.cppm`, `Artifact/include/Tool/ArtifactToolManager.ixx`, `Artifact/src/Tool/ArtifactToolManager.cppm`
- 確認できた事実: `panBehindToolRequested`／`shapeToolRequested`／`penToolRequested`／`textToolRequested`／`brushToolRequested`／`cloneStampToolRequested`／`eraserToolRequested`／`puppetToolRequested`／`motionSketchToolRequested`／`scrubPreviewToolRequested` は Toolbar 内の発火以外にリポジトリ内の購読先がなかった。各 QAction は対応する `setTool()` を実行し、ToolService／ToolManager が `ToolChangedEvent` を発行していた。
- 判断・仮説: 未使用 signal の宣言と発火だけを撤去し、各 tool の `setTool()` と既存 EventBus 通知を維持した。`homeRequested`／`cameraToolRequested` は代替実動作が未確認のため保留した。
- 価値・懸念: Toolbar の死蔵 Qt 通知と重複 API をさらに減らせる。外部 plugin の ABI 互換性はリポジトリ内検索では保証できないため、公開 ABI を維持する場合は別途互換層が必要。
- 次に確認すべきこと: 各 tool QAction、ショートカット、ToolChangedEvent 購読先でツール切替が従来どおり動くことを runtime で確認する。ビルド・テストは未実施。

## Toolbar の未使用 selection/view request signal 3 本を撤去

- 日付: 2026-08-31
- 関連: `Artifact/include/Widgets/ArtifactToolBar.ixx`, `Artifact/src/Widgets/ArtifactToolBar.cppm`, `Artifact/include/Tool/ArtifactToolManager.ixx`, `Artifact/src/Tool/ArtifactToolManager.cppm`
- 確認できた事実: `selectToolRequested`／`handToolRequested`／`zoomToolRequested` は Toolbar 内の QAction 分岐以外にリポジトリ内の参照がなく、各分岐は `setTool()` と既存 `ToolChangedEvent` で実動作を完結していた。
- 判断・仮説: 未使用 signal の宣言・発火だけを撤去し、Selection／Hand／Zoom の `setTool()` と EventBus 通知は変更しなかった。`cameraToolRequested` など、実動作が未確定な残りの signal は保留した。
- 価値・懸念: Qt signal の死蔵 API と重複通知をさらに小さく減らせる。外部 plugin が参照する公開 ABI まではリポジトリ内検索で保証できないため、ABI 互換が必要な配布形態では別判断が必要。
- 次に確認すべきこと: Selection／Hand／Zoom の QAction とショートカットから ToolChangedEvent が一度だけ発行され、各 surface の状態が更新されることを runtime で確認する。ビルド・テストは未実施。

## Toolbar の未使用 transform request signal 3 本を撤去

- 日付: 2026-08-31
- 関連: `Artifact/include/Widgets/ArtifactToolBar.ixx`, `Artifact/src/Widgets/ArtifactToolBar.cppm`, `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- 確認できた事実: `moveToolRequested`／`rotationToolRequested`／`scaleToolRequested` は MainWindow の qDebug 接続以外にリポジトリ内の購読先がなく、MainWindow の接続削除後は未使用だった。実際のツール変更は既存 `ToolChangedEvent` で通知されている。
- 判断・仮説: 未使用の 3 signal の宣言と発火だけを撤去し、各 QAction から `setTool()` へ進む実動作と `ToolChangedEvent` の発行は維持した。Toolbar の他の request signal は利用先確認なしに一括削除していない。
- 価値・懸念: Qt signal の死蔵 API と重複通知を小さく減らせる。外部 plugin がこの未使用 signal を参照する可能性はリポジトリ外からは確認できないため、公開 ABI を保証する場合は別途互換方針が必要になる。
- 次に確認すべきこと: Move／Rotation／Scale の QAction、ショートカット、`ToolChangedEvent` 購読先が従来どおり動き、重複発火がないことを runtime で確認する。ビルド・テストは未実施。

## Toolbar のツール選択ログを既存 ToolChangedEvent へ統一

- 日付: 2026-08-31
- 関連: `Artifact/src/Widgets/ArtifactMainWindow.cppm`, `Artifact/src/Widgets/ArtifactToolBar.cppm`, `Artifact/src/Service/ArtifactToolService.cppm`, `Artifact/src/Tool/ArtifactToolManager.cppm`
- 確認できた事実: MainWindow の `moveToolRequested`／`rotationToolRequested`／`scaleToolRequested` 接続は qDebug ログだけを実行していた。ツールの実状態は ToolService／ToolManager／Toolbar が既存の `ToolChangedEvent` を発行し、複数の surface が購読していた。
- 判断・仮説: MainWindow のログを `ToolChangedEvent` の購読へ移し、Toolbar→MainWindow の Qt 接続を 3 本削除した。新しいイベントや Qt signal は追加せず、Toolbar の QAction→Toolbar 内部処理は局所境界として維持した。
- 価値・懸念: ツール状態の横断経路を既存 EventBus に統一できる。ログは全ツール変更のうち Move／Rotation／Scale に限定し、ToolManager の重複抑制に従って状態変更時だけ出力される。将来ログ目的が変わる場合は購読側の責務を再評価する。
- 次に確認すべきこと: Toolbar、ショートカット、他の tool service 経路から Move／Rotation／Scale を選択したとき、ツール状態更新とログが一度だけ発生することを runtime で確認する。ビルド・テストは未実施。

## Welcome の asset import 要求を EventBus 化

- 日付: 2026-08-31
- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/ArtifactWelcomeWidget.ixx`, `Artifact/src/Widgets/ArtifactWelcomeWidget.cppm`, `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- 確認できた事実: Welcome の `importAsset` は MainWindow にだけ接続され、MainWindow が project の確保、複数ファイル選択、import 確認ダイアログ、非同期 import、失敗通知を担当していた。
- 判断・仮説: `ImportAssetsRequestedEvent` は引数なしの要求として発行し、ファイル選択以降の UI／サービス orchestration は MainWindow に維持した。Welcome 内のボタン→Welcome 接続は局所 Qt 境界として残した。
- 価値・懸念: Welcome と MainWindow の最後の command signal を EventBus に統一できる。EventBus 購読は UI thread でダイアログを開く前提なので、別 thread 発行を追加する場合は MainWindow 側の marshal が必要になる。
- 次に確認すべきこと: Welcome の Import Asset ボタンでキャンセル、確認ダイアログ、空選択、非同期失敗通知が従来どおり一度だけ動くことを runtime で確認する。ビルド・テストは未実施。

## Welcome の project open 要求を EventBus 化

- 日付: 2026-08-31
- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/ArtifactWelcomeWidget.ixx`, `Artifact/src/Widgets/ArtifactWelcomeWidget.cppm`, `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- 確認できた事実: Welcome の `openProject` は引数なしで MainWindow に接続され、MainWindow 内で `QFileDialog` と `loadFromFileAsync()` を実行していた。ダイアログとロード処理そのものは MainWindow の UI／orchestration 責務だった。
- 判断・仮説: `OpenProjectRequestedEvent` で要求だけを EventBus に渡し、ファイル選択・失敗ダイアログ・非同期ロードは既存の MainWindow 処理を維持した。ボタン→Welcome の Qt 接続は局所境界として残した。
- 価値・懸念: Welcome と MainWindow の widget signal 依存を減らし、将来別 surface から同じ open 要求へ合流しやすくなる。要求の発行元が UI thread であることを前提にしているため、別 thread 発行を追加する場合は MainWindow 側の marshal を明示する必要がある。
- 次に確認すべきこと: Welcome の Open Project ボタンでダイアログ、キャンセル、ロード失敗通知が従来どおり一度だけ動くことを runtime で確認する。ビルド・テストは未実施。

## Welcome の recent project 通知を EventBus 化

- 日付: 2026-08-31
- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/ArtifactWelcomeWidget.ixx`, `Artifact/src/Widgets/ArtifactWelcomeWidget.cppm`, `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- 確認できた事実: recent project の選択は Welcome のリストから MainWindow の非同期 project loader へ path を渡す cross-widget 通知で、他の購読先や中間状態はなかった。
- 判断・仮説: path を `OpenRecentProjectRequestedEvent` に値コピーして EventBus で渡し、Welcome 内の QListWidget→Welcome 接続だけを Qt の局所境界として残した。空 path の拒否も発行側で維持した。
- 価値・懸念: QModelIndex／widget signal を MainWindow に露出せず、非同期 callback が使う path の寿命もイベント payload で確定できる。イベント購読は MainWindow の QObject thread で実行される前提のため、別 thread 発行を追加する場合は marshal 規約を明示する必要がある。
- 次に確認すべきこと: recent project の選択で非同期ロード、失敗ダイアログ、Welcome の表示更新が従来どおり一度だけ動くことを runtime で確認する。ビルド・テストは未実施。

## Welcome のコンポジション作成通知を EventBus 化

- 日付: 2026-08-31
- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/include/Widgets/ArtifactWelcomeWidget.ixx`, `Artifact/src/Widgets/ArtifactWelcomeWidget.cppm`, `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- 確認できた事実: Welcome の `createNewComposition` は MainWindow にだけ接続され、受信側では `ArtifactProjectService::ensureProject()` と HD preset の作成を行っていた。ボタンから Welcome への接続は同一 widget 内の局所境界だった。
- 判断・仮説: コンポジション作成要求を payload のない `CreateCompositionRequestedEvent` として発行し、MainWindow が `const Event&` callback で購読することで、Welcome と MainWindow の widget 間 Qt signal を 1 本減らせる。ボタン→Welcome の局所 Qt 接続は残した。
- 価値・懸念: 将来 File Menu など別 surface から同じ作成要求を扱う場合にも、同じ内部イベントへ合流できる。イベント名が UI 起点を抽象化しているため、作成 preset を要求に含める設計へ拡張する際は契約を見直す必要がある。
- 次に確認すべきこと: Welcome の新規作成ボタンで ensureProject と composition 作成が一度だけ実行され、既存の project 状態・表示更新が維持されることを runtime で確認する。ビルド・テストは未実施。

## PlaybackEngine のフレーム範囲通知を EventBus 化

- 日付: 2026-08-31
- 関連: `Artifact/include/Playback/ArtifactPlaybackEngine.ixx`, `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`, `Artifact/src/Service/ArtifactPlaybackService.cppm`
- 確認できた事実: `frameRangeChanged` の Engine signal 利用先は PlaybackService の中継だけで、UI は既存の `PlaybackFrameRangeChangedEvent` を購読していた。発火は `setFrameRange()` と `setFrameRate()` の低頻度設定変更時に限られる。
- 判断・仮説: `FrameRange` を開始・終了 frame の値へ変換して Engine の QObject スレッドから既存 EventBus event を発行すれば、範囲通知も widget 横断の Qt signal から分離できる。`setFrameRate()` が現在範囲を再通知する既存挙動は維持した。
- 価値・懸念: PlaybackService の再生設定中継をさらに 1 本減らし、UI 更新経路を EventBus に統一できる。範囲値を scalar 化しているため、将来 `FrameRange` に開始・終了以外の意味を追加する場合は event 契約を見直す必要がある。
- 次に確認すべきこと: composition 切替、work area、frame rate 変更時に timeline/control の範囲表示が一度ずつ更新されることを runtime で確認する。ビルド・テストは未実施。

## PlaybackEngine のループ通知を EventBus 化

- 日付: 2026-08-31
- 関連: `Artifact/include/Playback/ArtifactPlaybackEngine.ixx`, `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`, `Artifact/src/Service/ArtifactPlaybackService.cppm`
- 確認できた事実: `loopingChanged` の利用先は PlaybackService の EventBus 中継で、Engine では `setLooping()` の低頻度な設定変更時にだけ発火していた。フレーム画像や再生ワーカーの高頻度経路は関与しない。
- 判断・仮説: 速度通知と同じく、Engine の QObject スレッドへ marshal して既存の `PlaybackLoopingChangedEvent` を直接発行することで、widget 横断通知の Qt signal を 1 本減らせる。
- 価値・懸念: ループ状態の購読先を EventBus に統一できる。設定変更が Engine の所有スレッド以外から呼ばれた場合は従来の同期的な signal 発火ではなく queued 発行になるため、呼び出し側が即時通知を前提にしていないことを runtime で確認する必要がある。
- 次に確認すべきこと: ループボタン、Time メニュー、再生設定からの切替で一度だけ状態が反映されることを確認する。ビルド・テストは未実施。

## PlaybackEngine の速度通知を EventBus 化

- 日付: 2026-08-31
- 関連: `Artifact/include/Playback/ArtifactPlaybackEngine.ixx`, `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`, `Artifact/src/Service/ArtifactPlaybackService.cppm`
- 確認できた事実: `ArtifactPlaybackService` は `playbackSpeedChanged` を受けて `PlaybackSpeedChangedEvent` を発行するだけで、速度通知自体のサービス副作用は持っていなかった。速度変更は UI 操作と再生ワーカーの方向反転から発生し得る。
- 判断・仮説: この低頻度の scalar 通知は Engine が既存の `PlaybackSpeedChangedEvent` を直接発行する境界へ移しても、widget 横断の Qt signal 接続を減らせる。Engine の QObject スレッドへ marshal してから発行することで、ワーカーからの発火も同じ実行規約に揃えた。
- 価値・懸念: PlaybackService の Qt signal 中継を 1 本減らし、速度変更を EventBus の横断イベントとして一貫させられる。一方、再生状態には音声クロック・セッション処理、フレーム通知には `QImage` 変換があるため、それらを同時に移行すると責務と高頻度経路が混ざる。
- 次に確認すべきこと: ビルド環境で Engine の C++20 module 依存と実行時の速度プリセット更新を確認する。今回の作業ではビルド・テストは未実施。

## 2026-08-31 — Qt イベント境界の残存接続を責務分類

- **関連:** `Artifact/src/Widgets/ArtifactMainWindow.cppm`、`Artifact/src/Widgets/ArtifactWelcomeWidget.cppm`、`Artifact/src/Widgets/ArtifactToolBar.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- **確認できた事実:** 横断状態・編集値に使われていた主要な widget 間通知は EventBus 化済み。残存する direct Qt 接続は、Welcome の open/import などの起動コマンド、ToolBar の単発ツール選択、Timeline／Composition 内の子 widget から親 orchestration への配線が中心である。
- **判断:** 起動・ツール選択はユーザー操作コマンド、Timeline／Composition の子→親は同一機能内のレベル境界として、現段階では Qt のまま維持する。未使用 signal の一括撤去や、QImage を含む再生フレーム経路の置換は別設計・runtime 確認なしに進めない。
- **価値または懸念:** EventBus 化の対象を「複数 surface に波及する状態／編集結果」に絞ることで、Qt を完全排除するための過剰な配線変更と module 依存の拡大を避けられる。残存接続が将来別 widget やサービスへ広がった場合は、今回の分類を再評価する必要がある。
- **次に確認すべきこと:** runtime で各横断イベントの購読先と残存 Qt 配線を操作経路ごとに確認し、新たな cross-surface consumer が現れた通知から順に EventBus 化する。

## 2026-08-31 — WorkspaceMode の状態通知を EventBus 化

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/include/Widgets/ArtifactToolBar.ixx`、`Artifact/src/Widgets/ArtifactToolBar.cppm`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`
- **確認できた事実:** ToolBar の `workspaceModeChanged` は MainWindow の表示構成と workspace ボタン表示を更新する状態通知で、ToolBar→MainWindow の direct Qt 接続が残っていた。WorkspaceMode enum は `Widgets.ToolBar` 側に所有され、汎用 EventTypes から直接 import すると循環しやすい。
- **変更:** `WorkspaceModeChangedEvent` を追加し、境界 payload は enum の基底整数値に限定した。MainWindow は許容範囲を検証してから enum に戻し、既存の表示更新を行う。ToolBar 発行側と MainWindow 購読側の両方で QObject thread への marshal を維持した。
- **価値または懸念:** workspace 状態を widget signal から内部イベントへ移し、enum module の責務を動かさずに循環を避けられた。整数値は enum 順序に依存するため、WorkspaceMode の追加・並べ替え時にはイベント契約と検証範囲を同時に更新する必要がある。
- **次に確認すべきこと:** ToolBar、View menu、ショートカット、起動時復元からの workspace 切替で、MainWindow の表示構成・ボタン表示・設定保存が従来どおり一度ずつ更新され、無効な mode 値が UI に反映されないことを runtime で確認する。

## 2026-08-31 — ToolOptionsBar の編集値通知を EventBus 化

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/include/Widgets/ArtifactToolOptionsBar.ixx`、`Artifact/src/Widgets/ArtifactToolOptionsBar.cppm`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`
- **確認できた事実:** ToolOptionsBar はブラシ、モーションスケッチ、テキスト、シェイプ等の編集値を `optionChanged(toolName, optionName, QVariant)` で MainWindow に渡し、MainWindow の受信側がツール／選択レイヤーへ適用していた。単一ボタン通知ではなく、複数の編集責務に波及する widget 境界だった。
- **変更:** `ToolOptionChangedEvent` を追加し、ToolOptionsBar の発行メソッドから EventBus へ渡すようにした。MainWindow は EventBus を購読し、既存の値変換、設定保存、レイヤー dirty 通知、LayerChanged 発行ロジックを維持した。発行メソッドは別スレッドから呼ばれた場合に ToolOptionsBar の QObject スレッドへ marshal する。
- **価値または懸念:** ToolOptionsBar と MainWindow の QObject signal 接続を除去し、編集値の安定 payload を内部境界に置けた。`QVariant` のキー名と値の単位は既存契約を引き継いでいるため、将来オプションを整理するときはイベント契約と適用ロジックを同時に見直す必要がある。
- **次に確認すべきこと:** 各ツールの option UI 変更が MainWindow の適用結果、設定保存、選択レイヤーの dirty／表示更新に一度ずつ反映され、別スレッド呼び出し時も UI 操作が ToolOptionsBar／MainWindow のスレッド外で実行されないことを runtime で確認する。

## 2026-08-31 — レイヤーノート通知を EventBus 化

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/include/Layer/ArtifactAbstractLayer.ixx`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`Artifact/src/Widgets/ArtifactMarkdownNoteEditorWidget.cppm`
- **確認できた事実:** レイヤーノート変更は `ArtifactAbstractLayer` の Qt 通知を経由し、Inspector と Markdown Note Editor の2ウィジェットが購読していた。両者はノート本文だけを必要とし、レイヤーの QObject ポインタを境界 payload に渡す必要はない。
- **変更:** `LayerNoteChangedEvent`（composition ID、layer ID、note）を追加し、レイヤー側から EventBus 発行へ変更した。2ウィジェットは安定 ID で対象を照合し、別スレッド発行時は各ウィジェットの Qt スレッドへ marshal する。既存のウィジェット内 textChanged 接続と初期値読み込みは維持した。
- **価値または懸念:** 複数ウィジェットへ広がるモデル通知を Qt signal から内部イベント境界へ移し、UI model／QObject 参照の漏出を避けられた。イベントは同期発行であり、破棄順序、選択変更と遅延イベントの前後関係、同一ノートを編集する際の再入を runtime で確認する必要がある。
- **次に確認すべきこと:** Inspector と Markdown Note Editor のどちらから編集しても相互表示が更新され、composition／layer 切替後に古い遅延イベントが新しい対象へ反映されず、プロジェクト終了時に購読 callback が残らないことを runtime で確認する。

## 2026-08-31 — Clip Buffer の貼り付け要求を EventBus 化

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/include/Widgets/ArtifactClipBufferWidget.ixx`、`Artifact/src/Widgets/ArtifactClipBufferWidget.cppm`、`Artifact/src/AppMain.cppm`
- **確認できた事実:** Clip Buffer の double-click／Paste ボタンは、保存済みの JSON レイヤー配列を `QVariant` に保持し、AppMain へ Qt シグナルで渡していた。データは raw widget/model 参照ではなく、既存の `ClipCopiedEvent` と同じシリアライズ済み payload だった。
- **変更:** `ClipPasteRequestedEvent` を追加し、Clip Buffer は貼り付け要求を EventBus 発行へ変更した。AppMain のレイヤー生成、親子／clone／matte 参照の再構築、選択処理は維持した。
- **価値または懸念:** Clip Buffer と Main の Qt 直結を外し、モデルの `QModelIndex` を境界に出さずに済んだ。`QVariant` 内の payload 形式は JSON 配列である前提が残るため、将来の形式変更時はイベント契約を同時に更新する必要がある。
- **次に確認すべきこと:** Clip Buffer の Paste／double-click で現在 composition に正しく追加され、親子・clone・matte参照と選択状態が従来どおり復元されることを runtime で確認する。

## 2026-08-31 — Project View の選択通知を安定 payload 化

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`、`Artifact/src/AppMain.cppm`
- **確認できた事実:** Project Manager は内部で `SelectionChangedEvent` を既に EventBus 発行していた一方、AppMain は Project View の `QModelIndex` 通知を2本直接購読し、proxy model と raw `ProjectItem*` を再解決していた。
- **変更:** `SelectionChangedEvent` に現在の composition ID、現在の footage path、選択中 footage のパス一覧を追加し、Project Manager 発行側で解決した。AppMain はこの stable payload を購読し、旧 Project View→AppMain の2本の Qt 接続を削除した。
- **価値または懸念:** 選択同期の境界から `QModelIndex` と model 構造を外へ漏らさず、既存イベントを再利用できた。選択変更と Asset Browser 逆同期は相互発火し得るため、既存 guard が再入を抑止することを runtime で確認する必要がある。
- **次に確認すべきこと:** composition／footage／複数 footage／folder の選択、検索・proxy model 更新後の選択で、composition 切替、Contents Viewer、Asset Browser の各更新が従来どおり成立することを runtime で確認する。

## 2026-08-31 — Project Manager のアイテム起動通知を EventBus 化

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/include/Widgets/ArtifactProjectManagerWidget.ixx`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`、`Artifact/src/AppMain.cppm`
- **確認できた事実:** Project View の double-click は Project Manager 内部で処理され、Project Manager から MainWindow へは `QModelIndex` を含む Qt 通知が直接接続されていた。発行時点ではモデルの役割から `ProjectItem::id`、composition ID、footage path を取得できる。
- **変更:** `ProjectItemActivatedEvent` を追加し、Project Manager の公開 double-click 通知を安定 payload の EventBus 発行へ変更した。`QModelIndex` を跨 widget payload にせず、Project View→Project Manager の内部接続と composition／footage の既存起動処理は維持した。
- **価値または懸念:** MainWindow が Project Manager の QObject シグナルと proxy model の構造に依存しなくなった。EventBus は同期発行のため、project item の再構築やアプリ終了時の購読寿命は runtime で確認が必要である。
- **次に確認すべきこと:** composition／footage の double-click と context menu の Preview が、dock 起動・現在 composition・Asset Browser 同期・Contents Viewer 更新を一度ずつ行うことを runtime で確認する。

## 2026-08-31 — Asset Browser から Main／Contents View への通知を EventBus 化

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/include/Widgets/ArtifactAssetBrowser.ixx`、`Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`、`Artifact/src/AppMain.cppm`
- **確認できた事実:** Asset Browser の `selectionChanged` と `itemDoubleClicked` は AppMain が直接受け、Project Manager の選択同期と Contents Viewer の表示を行っていた。payload はファイルパス／パス一覧で、`QModelIndex` のような短命な UI model 参照ではなかった。
- **変更:** `AssetBrowserSelectionChangedEvent`／`AssetBrowserItemDoubleClickedEvent` を追加し、2通知を EventBus 発行へ変更した。AppMain は QPointer で保持した対象へ購読側から処理し、既存の selection guard、Viewer の単一ファイル更新、double-click の dock 起動を維持した。folder／drop など未接続または別責務の通知は変更していない。
- **価値または懸念:** Asset Browser から複数の UI へ伸びる Qt 直結を除去し、安定したファイルパスpayloadで内部境界を作れた。AppMain の長寿命購読はアプリ終了時の破棄順序と、選択同期による再入を runtime で確認する必要がある。
- **次に確認すべきこと:** Asset Browser の単一／複数選択、double-click、Project View からの逆同期で、Contents Viewer と Project Manager の表示・選択が従来どおり一度だけ更新されることを runtime で確認する。

## 2026-08-31 — Timeline／Composition Editor から StatusBar への通知を EventBus 化

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/include/Widgets/ArtifactTimelineWidget.ixx`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/include/Widgets/Render/ArtifactCompositionEditor.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/include/Widgets/ArtifactStatusBar.ixx`、`Artifact/src/Widgets/ArtifactStatusBar.cppm`、`Artifact/src/AppMain.cppm`
- **確認できた事実:** StatusBar は Timeline の zoom／debug と Composition Editor の video debug を AppMain で直接購読していた。Timeline 側の通知は内部の TrackPainterView から親 widget へ伝える必要があるが、StatusBar は横断的な表示先だった。
- **変更:** `TimelineZoomLevelChangedEvent`／`TimelineDebugMessageEvent` を追加し、StatusBar が EventBus を購読する形へ変更した。Timeline／Composition Editor の公開通知メソッドはイベント発行メソッドにし、TrackPainterView→Timeline の内部 Qt 接続は維持した。AppMain の widget→StatusBar 直結は削除した。
- **価値または懸念:** StatusBar が特定 widget の QObject シグナル配線に依存しなくなり、複数 Timeline の通知も同じ内部経路で扱える。イベント購読は receiver thread へ marshal するため、StatusBar 破棄順序と複数 Timeline の表示優先順は runtime で確認が必要である。
- **次に確認すべきこと:** Timeline の zoom／debug、Composition Editor の video debug が StatusBar に従来どおり反映され、複数 Timeline を開閉した際に遅延イベントや表示の取りこぼしがないことを runtime で確認する。

## 2026-08-31 — AIClient のサービス通知を EventBus 化

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/include/AI/AIClient.ixx`、`Artifact/src/AI/AIClient.cppm`、`Artifact/src/Widgets/AIChatWidget.cppm`
- **確認できた事実:** `AIClient` の5本の Qt 通知は、`AIChatWidget` がサービス境界を跨いで直接 `Qt::QueuedConnection` 購読していた。AIClient は初期化・クラウド処理・ローカルストリーミングを detached worker と queued callback の両方から通知している。
- **変更:** 5種類を `AIClientChangedEvent` と変更種別へ統合し、AIClient の発行を EventBus 化した。AIChatWidget は1本の typed EventBus 購読にまとめ、受信側 QObject の thread へ明示的に marshal する。ウィジェット内のボタン等の Qt 接続とストリーミングの16ms更新制御は維持した。
- **価値または懸念:** サービスからウィジェットへの Qt 直結を除去し、メッセージ本文・初期化結果を型付き内部境界で扱える。EventBus は同期発行なので、将来別の UI 購読者を追加する場合も receiver-side marshal と AIClient singleton の寿命を確認する必要がある。
- **次に確認すべきこと:** ローカル／クラウド応答、ストリーミング、キャンセル、初期化成功・失敗、ウィジェット破棄中の遅延イベントで、表示更新と購読解除が従来どおり安全に完了することを runtime で確認する。

## 2026-08-31 — Active composition の死んだ Qt 通知を整理

- **関連:** `Artifact/include/Application/ActiveContextService.ixx`、`Artifact/src/Application/ActiveContextService.cppm`、`Artifact/include/Layer/ArtifactLayerSelectionManager.ixx`、`Artifact/src/Layer/ArtifactLayerSelectionManager.cppm`、`Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm`
- **確認できた事実:** `ActiveContextService::activeCompositionChanged()` と `ArtifactLayerSelectionManager::activeCompositionChanged()` は宣言と発火箇所だけが残り、購読・QObject 接続は存在しなかった。アクティブ composition の正規の横断通知は `ArtifactProjectService` が発行する `CurrentCompositionChangedEvent` である。
- **変更:** 2つの未購読 Qt シグナルと発火処理を削除し、Edit Menu の Undo/Redo 同期に残っていた手動発火も削除した。既存の selection 内部イベント発行と `CurrentCompositionChangedEvent` は維持し、二重発行は追加していない。
- **価値または懸念:** 状態保持と横断通知の責務が分離され、死んだ Qt 通知を将来の依存先と誤認しにくくなる。アクティブ composition の UI 更新経路は ProjectService 起点に揃っている前提のため、runtime で切替・Undo/Redo 後の更新を確認する必要がある。
- **次に確認すべきこと:** composition 切替と Undo/Redo で Timeline、Menu、Inspector の表示が欠落せず、`CurrentCompositionChangedEvent` が一度だけ届くことを runtime で確認する。

## 2026-08-31 — ApplicationService の未使用ライフサイクル通知を整理

- **関連:** `Artifact/include/Service/ApplicationService.ixx`、`Artifact/src/Service/ApplicationService.cppm`
- **確認できた事実:** `initialized`／`shutdownRequested` は自身の `initialize()`／`shutdown()` から発火するだけで購読者がなく、`projectOpened`／`projectClosed` はリポジトリ内で発火も購読もなかった。
- **変更:** 4本の Qt シグナル宣言と、実際に呼ばれていた2本の発火処理を削除した。現在の正規プロジェクト通知を置き換える EventBus は追加していない。
- **価値または懸念:** ApplicationService の公開面から、実動配線のないライフサイクル通知を除去できる。外部プラグインがこの未使用 API に依存していないことは、runtime／利用者側コードの確認が必要である。
- **次に確認すべきこと:** アプリ初期化・終了時にサービス所有オブジェクトの生成／破棄が従来どおり完了することを runtime で確認する。

## 2026-08-31 — CompositionPlaybackController の未購読通知を整理

- **関連:** `Artifact/include/Composition/ArtifactCompositionPlaybackController.ixx`、`Artifact/src/Composition/ArtifactCompositionPlaybackController.cppm`
- **確認できた事実:** コントローラの playback state／frame／speed／loop／range 通知は、コントローラ自身から発火するだけで、リポジトリ内に購読・QObject 接続がなかった。横断的な再生状態・フレーム通知は `ArtifactPlaybackService` の EventBus 経路が正規である。
- **変更:** 5本の未使用 Qt シグナル宣言と、コントローラ内の発火処理を削除した。PlaybackService／PlaybackEngine の実動配線や既存の EventBus 通知は変更していない。
- **価値または懸念:** 旧コントローラからウィジェットやサービスへ広がる未接続の Qt 通知を除去できる。外部利用者がコントローラ通知を参照していないことは runtime／利用者側コードで確認が必要である。
- **次に確認すべきこと:** controller 経由の再生・移動操作が PlaybackService の状態・フレーム更新と従来どおり同期することを runtime で確認する。

## 2026-08-31 — UndoManager の横断履歴通知を EventBus 化

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Widgets/ArtifactUndoHistoryWidget.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`
- **確認できた事実:** `historyChanged` は Undo History、Property、Timeline Layer Panel の3 UIが購読していた。`propertyChanged`／`anythingChanged` は多数の編集経路から通知されるが、QObject購読は存在しなかった。
- **変更:** `UndoManagerChangedEvent` と変更種別を追加し、3種の通知を EventBus へ同期発行する。3 UIは履歴種別だけを購読し、発火元がUIスレッドでない場合は各自の QObject スレッドへ queued dispatch する。旧 Qt シグナル接続は削除した。
- **価値または懸念:** Undo 境界から複数ウィジェットへ伸びる通知を型付き内部イベントへ揃えられる。`anythingChanged` は高頻度になり得るため、将来購読者を追加する際もイベント種別の絞り込みと UI marshal を維持する必要がある。
- **次に確認すべきこと:** push／undo／redo／clear／session復元で履歴表示、Property 値、Timeline の undo flash が従来どおり更新され、非UIスレッド発火でも安全であることを runtime で確認する。

## 2026-08-31 — ColorScienceManager の通知を typed EventBus 化

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/include/Color/ArtifactColorScienceManager.ixx`、`Artifact/src/Color/ArtifactColorScienceManager.cppm`
- **確認できた事実:** color science manager の `settingsChanged`／`lutChanged`／`compositionSettingsChanged` は Qt シグナルとして宣言・発火されていたが、リポジトリ内の購読・QObject 接続は存在しなかった。色管理は設定、LUT、composition単位の3種類に意味が分かれる。
- **変更:** 3種類を `ColorScienceManagerChangedEvent` の変更種別へ移し、既存の発火回数・composition ID を維持した。Qt シグナル宣言と発火を削除し、既存の色変換・LUT処理は変更していない。
- **価値または懸念:** 色管理の状態変化をウィジェット依存ではない内部イベント境界に置ける。現時点で購読者がないため、renderer がこのイベントを必要とする場合の接続は別途設計・runtime確認が必要である。
- **次に確認すべきこと:** 設定変更、LUT読込／解除、composition固有設定変更で、必要な将来購読者が変更種別とIDを正しく受け取れることを runtime で確認する。

## 2026-08-31 — Core ActionManager の横断通知を EventBus 化

- **関連:** `ArtifactCore/include/UI/InputOperator.ixx`、`ArtifactCore/src/UI/InputOperator.cppm`
- **確認できた事実:** `ActionManager` の action登録／解除／実行通知はアプリ全体のサービス境界にあったが、リポジトリ内に Qt購読・QObject接続はなかった。`InputOperator` には別途低レベルの key/action 通知があり、今回の対象から分離した。
- **変更:** actionポインタを跨がせず、action ID・params・変更種別を持つ `ActionManagerChangedEvent` を追加し、3本の Qt通知を EventBus 発行へ変更した。Action／KeyMap／InputOperator の低レベル通知は未変更。
- **価値または懸念:** command登録と実行の横断観測点を Core内の型付きイベントへ揃えられる。現時点で購読者はないため、将来の Command Palette／diagnostics 購読者は必要に応じて receiver thread を扱う必要がある。
- **次に確認すべきこと:** action登録・解除・実行で event の種別、ID、params が一致し、Actionポインタの寿命に依存しないことを runtime で確認する。

## 2026-08-31 — Core SelectionManager の選択通知を EventBus 化

- **関連:** `ArtifactCore/include/UI/SelectionManager.ixx`
- **確認できた事実:** Coreの `SelectionManager` は Artifact側で利用されておらず、3本の Qt シグナルに対する購読・QObject接続もなかった。一方、クラス自体はグローバルな composition／layer／asset 選択状態を所有している。
- **変更:** 同じ `UI.SelectionManager` モジュール内に3つの typed event を定義し、inline setter の通知を EventBus 発行へ変更した。Qtシグナル宣言は削除し、選択状態の保持・query APIは維持した。Artifact.Event.Typesへの逆依存は追加していない。
- **価値または懸念:** Coreのグローバル選択通知も widget 非依存の内部境界へ揃えられる。現時点で購読者がないため、Artifact側の正規 `ArtifactLayerSelectionManager` と統合するかは別設計として残る。
- **次に確認すべきこと:** Core SelectionManager を利用する最小経路で composition／layer／asset 選択イベントが正しい payload で一度ずつ発行されることを runtime で確認する。

## 2026-08-31 — AbstractAssetFile の未購読 status 通知を整理

- **関連:** `ArtifactCore/include/Asset/AbstractAssetFile.ixx`、`ArtifactCore/src/Asset/AbstractAssetFile.cppm`
- **確認できた事実:** `statusChanged` は `setStatus()` から発火するだけで、ArtifactCore／Artifact内に購読・QObject接続がなかった。status値は `status()` で同期取得できる。
- **変更:** CoreからArtifactのEvent Typesへ逆依存を作らず、未購読の Qt シグナル宣言と発火処理を削除した。Asset status の保持・取得・load/unload 処理は変更していない。
- **価値または懸念:** Asset file の責務から実動配線のない通知を除去できる。将来 Asset Browser が非同期 status 更新を必要とする場合は、Core側に依存方向を守った typed event／observer境界を別途設計する必要がある。
- **次に確認すべきこと:** missing／modified status 更新と Asset Browser の表示判定が、同期 query と既存モデル更新経路で従来どおり成立することを runtime で確認する。

## 2026-08-31 — HDRMonitor の通知を typed EventBus 化

- **関連:** `Artifact/include/Render/ArtifactHDRMonitor.ixx`、`Artifact/src/Render/ArtifactHDRMonitor.cppm`
- **確認できた事実:** `settingsChanged` は `setSettings()` から発火するだけで購読者がなく、`analysisComplete` は `analyzeFrame()` から発火していたが、リポジトリ内の購読・QObject接続はなかった。
- **変更:** 2種類を `HDRMonitorSettingsChangedEvent`／`HDRAnalysisCompletedEvent` として EventBus へ移した。解析結果の内容と、設定変更・解析完了の発火タイミングは維持し、同期APIの戻り値も変更していない。
- **価値または懸念:** HDR解析クラスから未接続のQt配線を除去し、将来のscope／diagnostics UIが内部イベントを購読できる。解析結果を含むイベントはデータ量が大きくなり得るため、高頻度購読者を追加する際はコピー負荷を確認する必要がある。
- **次に確認すべきこと:** HDR設定変更と解析結果生成でイベントが一度ずつ届き、解析結果と同期戻り値が一致することを runtime で確認する。

## 2026-08-31 — OCIOManager の色管理通知を EventBus 化

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/include/Color/ArtifactOCIOManager.ixx`、`Artifact/src/Color/ArtifactOCIOManager.cppm`、`Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm`
- **確認できた事実:** `configChanged` は Color Science Panel が直接購読しており、working-space／display-view 通知は宣言・発火だけで他の購読者がなかった。OCIO manager は singleton で、Panel の更新は UI QObject 上で行う必要がある。
- **変更:** 3種類を `OCIOManagerChangedEvent` として EventBus へ移し、Panel は ConfigChanged を購読して所有パネルの thread へ marshal するようにした。既存の設定変更順序（詳細通知後に config 通知）は維持した。
- **価値または懸念:** OCIO の service→widget 直結を除去し、将来 renderer 等が詳細な色管理変更を購読できる。イベントは同期発行なので、新規UI購読者も receiver-side marshal を必須とする。
- **次に確認すべきこと:** preset／config／working space／display／view の変更で Panel の表示と ColorScience 同期が一度ずつ更新され、Panel破棄後に遅延通知が残らないことを runtime で確認する。

## 2026-08-31 — PlaybackShortcuts の実動通知を EventBus 化

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/include/Service/ArtifactPlaybackShortcuts.ixx`、`Artifact/src/Service/ArtifactPlaybackShortcuts.cppm`
- **確認できた事実:** `shortcutExecuted` は各ショートカット処理から実際に発火していたが、リポジトリ内に購読・QObject接続はなかった。`inPointSet`／`outPointSet`／`markerAdded` は発火処理がコメント化されていた。
- **変更:** 実動していた通知を `PlaybackShortcutExecutedEvent` として EventBus へ移し、action ID を維持した。コメント化された3本の Qt シグナル宣言も削除し、InOutPoints／command の既存正規経路を残した。
- **価値または懸念:** ショートカット実行の観測点をサービスから内部イベントへ移せる。現時点の購読者はないため、将来の監視・ヘルプUI等で購読する場合はUIスレッドへのmarshalを購読側で行う必要がある。
- **次に確認すべきこと:** 各 playback shortcut の action ID が一度だけ発行され、in/out point・marker操作の状態変更が従来の command／InOutPoints 経路で行われることを runtime で確認する。

## 2026-08-31 — ToolService の未購読モード通知を整理

- **関連:** `Artifact/include/Service/ArtifactToolService.ixx`、`Artifact/src/Service/ArtifactToolService.cppm`
- **確認できた事実:** `editModeChanged`／`displayModeChanged` は ToolService 内で発火するだけで、リポジトリ内に購読・QObject 接続がなかった。active tool の横断通知には既存の `ToolChangedEvent` がある。
- **変更:** 2本の未使用 Qt シグナル宣言と発火処理を削除した。モード状態の保持、active tool の更新、既存の `ToolChangedEvent` は変更していない。
- **価値または懸念:** ToolService の公開面から未接続の Qt 通知を除去し、tool変更とmode状態の責務を分けられる。外部利用者がこの未使用 API に依存していないことは runtime／利用者側コードで確認が必要である。
- **次に確認すべきこと:** View／Transform／Mask 等の mode切替で active tool と各UIの表示が従来どおり同期することを runtime で確認する。

## 2026-08-31 — Logger 横断通知の EventBus 化と receiver-side marshal

- **関連:** `ArtifactCore/include/Diagnostics/Logger.ixx`、`ArtifactCore/src/Diagnostics/Logger.cppm`、`Artifact/src/AppMain.cppm`、`Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- **確認できた事実:** Logger の `logAdded`／`logsCleared` は AppMain と Debug Console の複数 UI が購読し、Qt メッセージハンドラ経由で任意スレッドから発火し得た。旧 QObject 接続は receiver が UI だったため、Qt の AutoConnection が UI 側への queued dispatch を担っていた。
- **変更:** Core の `LogAddedEvent`／`LogsClearedEvent` を追加し、Logger はスレッド固定を仮定せず EventBus へ同期発行する。各 UI 購読者は自分の QObject スレッドを確認し、必要な場合だけ `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` で UI 操作を戻す。AppMain の長寿命購読には `QPointer` を使う。
- **価値または懸念:** 複数 UI にまたがる通知が Qt シグナル配線から型付き内部イベントへ揃い、Logger を特定スレッドのイベントループへ依存させない。新しい非 UI 購読者を追加する場合も、UI API を直接触らずスレッド境界を個別に扱う必要がある。
- **次に確認すべきこと:** 任意スレッドからのログ追加、クリア、UI 終了順序でイベント欠落・use-after-free・表示遅延がないことを runtime で確認する。

## 2026-08-31 — Layer変更通知の移行前に通知網羅性を揃える

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`
- **確認できた事実:** 旧 `changed()` は物理・レイアウト・親子設定などで `Q_EMIT` される一方、`notifyLayerMutation()` には既に `LayerChangedEvent` 経路があった。Property Editor／MainWindow の直結はこの変更通知を購読していた。
- **変更:** `ArtifactAbstractLayer::changed()` を Qt シグナルではない内部通知メソッドへ変更し、全ての既存呼び出しから `LayerChangedEvent` を発行するようにした。ワーカースレッドからの通知は Layer の QObject スレッドへ marshal し、Property Editor／MainWindow は layer ID で絞った EventBus 購読へ移行した。
- **価値または懸念:** レイヤー変更がウィジェット間の Qt 接続を経由せず、共通イベント境界へ揃った。イベントは同期発行を基本とし、UI操作は QObject スレッドへ戻すため、他の EventBus 購読者のスレッド安全性と高頻度更新の負荷は runtime で確認が必要である。
- **次に確認すべきこと:** 旧 `changed()` の全派生レイヤー経路で `LayerChangedEvent` が一度だけ発行されること、Property Editor／MainWindow の更新、ワーカースレッド由来通知の UI 安全性を runtime で検証する。

## 2026-08-25 — component.joint（レイヤー間ジョイントcomponent）を実装（ビルド未検証）

- **ビルド検証メモ (2026-08-25):** ビルド試行はユーザー指示で中断。座標検証は静的解析のみ実施済み（`Physics2D` 重力を `{0,-9.8}`→`{0,9.8}` に修正済み。MPM `+980` / SoftBody `+9.8` との整合）。環境側の未解決事項: J:コピーのビルドツリー `out/build/x64-Debug`（Ninja、classic vcpkg=C:\vcpkg）では新規 port の `find_package` が `<installed>/share/<pkg>` を探索できず（box2d/OpenCV は過去キャッシュの `_DIR` で通っているだけ）、`meshoptimizer:x64-windows` を C:\vcpkg へ入れても数分後に消失する現象が発生（別プロセス/manifest クリーンアップの疑い、要切り分け）。X:\Dev\ArtifactStudio 側は manifest mode (`out/build_ninja/vcpkg_installed`) で box2d 3.1.1 を確認済み。

- **関連:** `ArtifactCore/include/Physics/2D/Physics2D.ixx` / `src/Physics/Physics2D.cppm`（joint管理API拡張）、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`（Joint property group・setter・JSON・descriptor）、`Artifact/include/Layer/ArtifactLayerComponentSystem.ixx`（`makeJointComponentDescriptor`）、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`（`evaluateJointConstraints()`、setFramePosition/goToFrame に配線）、`Artifact/src/Widgets/ArtifactInspectorWidget.cppm` / `ArtifactPropertyWidgetShared.cppm`
- **ドナー記録:** Domain=layer-to-layer 2D joints。Primary donor=vendored Box2D v3（MIT、`b2CreateDistanceJoint` hertz/dampingRatio 契約をそのまま使用）。新規ベンダーコードなし。Ten-Minute-Physics(MIT) は将来の anchor 編集・ロープ展開時の参照。
- **事実:** `Physics2D::addDistanceJoint/addRevoluteJoint` は存在したが呼び出しゼロ・削除APIなし。`enableRigidBodyPhysics()` も呼び出しゼロでリジッド経路全体が到達不能だった。jointは同一world内の2体が必要なため、現行のレイヤー単位world構造では「相手レイヤー」を所有world内の静的プロキシとして表現する設計にした。
- **変更:** (1) Core: `addStaticAnchor()` 追加、joint id ベクトル管理＋`removeJoint/clearJoints/getJoints`、`clear()` でjoint先破棄。(2) Layer: `component.joint.enabled/type(0=Distance,1=Pin)/targetLayer/length/stiffness(hertz)/damping(ratio)` をCollisionと同型のproperty group・JSON・descriptorで追加。**joint有効化が `enableRigidBodyPhysics()` の初の到達可能な呼び出し口になり、監査項目(4)を実質解消**。(3) Composition: `evaluateJointConstraints()` がフレーム毎にtarget layerを名前解決→targetのcomp空間中心をowner局所空間へマップ→静的アンカー(cloneIndex==-2)生成/追従→signature比較でjoint再生成。Distanceの length=0 は作成時の実距離を採用。(4) リジッド読み取り/body選択は cloneIndex!=-2 の自レイヤーbodyを選ぶよう修正（proxy混在でもfront()前提が壊れない）。shape/restitution変化でのbody再構築時は `clearJoints()` して再接続。
- **価値:** 振り子・距離維持・ピン留めなど層間の物理的拘束がcomposition再生(fixed-step)に乗る。Box2D資産の初めての実利用経路。
- **未検証・懸念:** ビルド・ランタイム未実施。**`Physics2D` world重力が {0,-9.8}（Y上向き）で、Y下向き画面座標と逆の疑い**——到達不能だったため露見していなかった。ランタイム検証時に最初に確認。rigid worldはsnapshot対象外のためスクラブ復元は非対応（softBody/materialのみ）。target名の重複時は先勝ち。アンカー編集UI・revolute角制限は未実装。
- **次の確認:** ビルド後、Shape/ImageレイヤーでJoint有効+target指定→再生で振り子/距離維持が効くこと、重力符号、無効化時にproxy+jointが消えること。

## 2026-08-25 — Polygon collider をレイヤー導線へ露出（Core〜UI、ビルド未検証）

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`（shape property/clamp/JSON/sync 経路）、`Artifact/include/Layer/ArtifactAbstractLayer.ixx`（仮想 `collisionOutlineLocalPoints()`）、`Artifact/src/Layer/ArtifactShapeLayer.cppm`（outline override）、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`（MPM collider polygon 分岐）、`Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`ArtifactCore/src/Physics/Physics2D.cppm`（`addPolygonBody` 拡張）、`ArtifactCore/src/Physics/PhysicsSystem.cppm`（グローバル流体死に経路削除）
- **事実:** Core の `SoftBodyCollider` / `MpmCollider2D` には Polygon 型と resolver が 2026-08-23 追加済みだったが、`Artifact/` 側から `polygonPoints` を埋める経路はゼロで、layer collision shape は 0..2（Auto/Box/Circle）に固定されていた。ShapeLayer には `customPolygonPoints()` と通常形状の輪郭生成（`buildRenderablePoints`）が既存。Box2D v3 の `b2ComputeHull` は最大 `b2_maxPolygonVertices`(8) 頂点までしか受け付けず、超過入力は空 hull を返す。
- **変更:** (1) `component.collision.shape` に `3=Polygon` を追加し tooltip enum・range・setter・JSON 復元 clamp を 0..3 へ拡張（tooltip 末尾 `.` が enum ラベルに混入する既存問題も解消）。(2) `ArtifactAbstractLayer::collisionOutlineLocalPoints()` 仮想メソッドを新設、ShapeLayer が custom path/custom polygon/通常形状輪郭を返す。operator stack 適用時と Line は空→auto-bounds フォールバック。(3) SoftBody sync は outline ≥3 で Polygon collider 登録、Rigid sync は `addPolygonBody`（8 点ダウンサンプル＋失敗時 Box フォールバック）を使用、Composition MPM 経路は source outline を target 局所空間へ vertex-exact マップ、Clone 経路は outline bbox の conservative proxy。(4) `addPolygonBody` に friction/restitution 引数を追加（既定値付き後方互換）し hull 失敗時に body を破棄。(5) 呼び出しゼロだった deprecated グローバル流体 API（`initFluid`/`getFluidSolver()`/`fluidSolver_` メンバー群）を PhysicsSystem から削除、per-layer `createFluidSolver(layerId)` に一本化。
- **価値:** ShapeLayer の実際の輪郭で衝突判定ができるようになり、多角形レイヤー同士や MPM 材料との接触が頂点精度で解決される。死んだグローバル流体経路の削除で Fluid の正規経路が per-layer のみと明確化。
- **未検証・懸念:** ビルド・ランタイム未実施（ユーザー指示待ち）。SoftBody 経路の shape==2 は従来どおり Box 扱い（挙動維持、Circle 化は別判断）。Rigid polygon は凸包になるため凹形輪郭は凸近似。clone instance の polygon は bbox proxy のまま。
- **次の確認:** ビルド後、ShapeLayer(Polygon) で Collision 有効＋SoftBody/MPM を付けて落下接触が輪郭通りになること、shape 0..2 の既存プロジェクト JSON が無影響であること。

## 2026-08-25 — モーションブラーが velocity ターゲット未生成のため通常プレビューで沈黙していた(修正)

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`(renderOneFrameImpl の pipeline initialize ~32401 行、Resolve pass の MotionBlurPass::apply ~33990 行、useLayerMsaa 条件)
- **事実:** `RenderPipeline::hasVelocityTarget()` は `emissionEnabled_` を要求し、emission 系ターゲットは `auxiliary3DChannelRequested`(AOV デバッグチャネル / SSGI 表示)時のみ生成されていた。そのためタイムラインのモーションブラー設定が有効でも、デバッグチャネルを開いていない限り `MotionBlurPass::apply` は常にスキップされていた。
- **対応:** `motionBlurVelocityRequested`(= timelineMotionBlurActive)を emission ターゲット生成条件に追加。併せて velocity パスが単一サンプル depth を前提とするため、モーションブラー有効時は per-layer MSAA を除外(AOV キャプチャと同じ扱い)。さらに AA モード設定(`compositionAntiAliasingMode`: 0=Off / 1=FXAA / 2=MSAA4x、既定 FXAA)と shutter phase 設定(`timelineMotionBlurShutterPhase`)を AppSettings に追加し、ApplicationSettingDialog の Preview Quality に UI を露出(AA コンボ + shutter angle/phase スピン)。FXAA は mode==1、per-layer MSAA は mode==2 のときのみ適用。
- **次の確認:** ビルド後、モーションブラー ON + 3D レイヤー移動でブレが乗ること、OFF 時に velocity ターゲットが作られずメモリ増加がないこと。FXAA は 3D 表示時のみ適用される現行挙動を維持。

## 2026-08-25 — World Position AOV のリソースは既存 offscreen API で用意可能、書き込みパスが本体

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`PrecompGpuOutputEntry`、`createOffscreenComputeTexture` 利用箇所）、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`（mesh draw PSO）
- **事実:** `ArtifactIRenderer` には float 系 offscreen texture（`createOffscreenComputeTexture`）と SRV 取得 API が既に存在するため、world position / depth AOV 用ターゲットの生成はバックエンド無変更で可能。一方、position 書き込みは mesh draw パスへの専用 PSO（または MRT）追加が必要で、これは Diligent シビアコードに触る。
- **次の確認:** 専用 PSO スライスとして、(1) 既存 mesh VS を流用した position 出力 VS/PS の追加、(2) PrecompGpuOutputEntry と同型の target/SRV 保持、(3) DOF/fog 消費側の depth 契約（Phase 3 計画と合流）を順に設計レビューする。

## 2026-08-24 — 製品rendererのglyph PSO取得を専用provider境界へ経由させた（G2移行の中間ステップ）

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm`、`Artifact/include/Render/ArtifactTextGlyphSubmitter.ixx`、`Artifact/src/Render/ArtifactTextGlyphPipelineAdapter.cppm`、`Artifact/cmake/ArtifactSources.cmake`
- **事実:** `ArtifactTextGlyphPipelineProvider` 契約と `makeArtifactTextGlyphPipelineProvider(ShaderManager&)` アダプタは TextRuntime/smoke ターゲット専用で存在し、製品ビルドの module グラフには入っていなかった。製品 `DiligentImmediateSubmitter::setPSOs()` は ShaderManager の glyph getter を直接呼んでいた。PSOAndSRB は refcount 手動管理（AddRef/Release）なので生ポインタからの再所有に AddRef が必要。
- **対応状況 (2026-08-24):** setPSOs() の glyph PSO/SRB/sampler 取得を provider アダプタ経由に変更（同一オブジェクトのため描画挙動は不変）。Contract / Adapter の2モジュールを ArtifactSources.cmake へ明示登録。ビルド・実行検証はユーザー指示待ちで未実施。
- **次の確認:** ビルド後に (1) テキストレイヤーの viewport 描画が退行しないこと、(2) `ArtifactTextGlyphSmoke` が引き続き通ること。その後の本命は ShaderManager 内の Glyph PSO 生成本体（シェーダソース＋PSO作成）を専用providerへ移動させ、DIS の glyph draw を `ArtifactTextGlyphSubmitter` 本体へ委譲するか判断すること。ただし製品 glyph 経路は outline/shadow-blur/stroke を持ち、検証用 submitter は未対応のため全面置換は機能退行リスクあり。


# Insight Log

未解決の設計判断・runtime 検証待ちだけを記録する。実装済みの局所修正と履歴は `docs/analysis/INSIGHT_ARCHIVE_2026-08-11.md` を参照する。

## 2026-08-29 — Layer modulation は opacity を最初の正規対象に限定

- **関連:** `Artifact/include/Layer/ArtifactAbstractLayer.ixx`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`docs/planned/MILESTONE_PROPERTY_MODULATION_2026-08-29.md`
- **事実:** `ArtifactAbstractLayer` は共通の永続プロパティキャッシュを持つが、Transform は keyframe、variant、animation layer、物理・レイアウト評価へそれぞれ異なる形で分岐している。`opacity()` は共通の単一評価点で、既存の順序が base → keyframe → animation layer → effect envelope と明確だった。
- **変更:** layer ごとの `ModulationRouter` を追加し、stable path `layer.<layer-id>.layer.opacity` を base/keyframe/animation layer の後、effect envelope の前へ適用した。`goToFrame()` と `opacity()` の双方から同一フレーム idempotent な control clock を呼ぶため、通常の描画経路と直接の opacity 参照のどちらでも source を二重に進めない。
- **価値または懸念:** 全 layer type で使える最小の runtime 接続になる。一方 Transform を同じパターンで一括適用すると、現在の variant/physics/layout と値の書き戻しを混同するリスクがあるため未実装。
- **次に確認すべきこと:** Null/Image Layer へ constant/LFO mapping を設定し、preview/export・正逆シークで opacity が一致すること。次の対象は transform ではなく、保存可能な source/assignment state と Undo 境界の設計。

## 2026-08-29 — Modulation source の保存は設定のみを値オブジェクト化

- **関連:** `ArtifactCore/include/Audio/Modulation/{Modulator,Router}.ixx`、`tests/ArtifactCore/AudioModulationRouterTest.cpp`
- **事実:** LFO は waveform/frequency/phase offset/pulse width/unipolar、ADSR は ADSR 値、Random は rate/smoothing/seed/unipolar を公開している。いずれも再生中の phase、sample-and-hold、gate/level をそのまま project state として保存する既存契約はない。
- **変更:** `ModulationSourceDefinition` と `sourceDefinitions()` / `restoreSources()` を追加。source ID を維持し、assignment は既存の source ID 参照をそのまま再接続する。未知の実装型・ID 0・重複 ID は復元しない。
- **価値または懸念:** JSON / Undo は C++ polymorphic source を直接扱わず値オブジェクトだけを扱える。runtime state を保存しないため project reopen は決定的に frame zero から開始するが、途中再生状態の復元は行わない。
- **次に確認すべきこと:** effect / layer 所有側で source definitions と assignments を JSON 化し、deserialize 後に `restoreSources()` → assignment restore の順で呼ぶ。Undo は router snapshot を同じ値オブジェクトで保持する。

## 2026-08-29 — Composition effect の modulation JSON 復元順を固定

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` の `serializeEffect()` / `deserializeEffect()`
- **事実:** Composition effect は既に ID、一般プロパティ、keyframe/expression/envelope をこの局所 helper で JSON 化している。effect の instance ID は composition への追加時に一意化され、modulation target path に含まれる。
- **変更:** `modulation.sources` と `modulation.assignments` を effect JSON に追加した。target ID は JSON の数値として保存せず、stable `targetPath` から再計算するため 64-bit hash を JSON double に落とさない。復元は source definitions を復元後に assignment を追加する。
- **価値または懸念:** Composition effect は project reopen 後も LFO/ADSR/Random 設定と mapping を失わない構造になった。layer が所有する router は別シリアライザであり、まだ同じ JSON helper を利用していない。実ファイル round-trip と runtime は未検証。
- **次に確認すべきこと:** Composition serialization のテスト対象を確立して modulation JSON の round-trip を追加し、layer serialization でも同一 schema を使うよう局所 helper を共有または移動する。

## 2026-08-29 — Layer Router も effect と同一の modulation JSON schema を使用

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `toJson()` / `fromJsonProperties()`
- **事実:** 全 concrete layer の読み込みは `ArtifactAbstractLayer::fromJsonProperties()` または 2D 派生経由へ到達する。共通 layer JSON には animation/property layer など共通状態を保存する正規の位置がある。
- **変更:** layer 所有 router の `modulation.sources` / `modulation.assignments` を共通 layer JSON へ追加。source definitions を復元後に stable target path から assignment を復元する。`layer.opacity` の target path は layer ID を含むが、hash を保存しないため JSON 整数精度の影響を受けない。
- **価値または懸念:** layer type ごとの個別 serializer を増やさずに opacity mapping を再読込できる。effect / layer の JSON field schema は同一だが、現在は各既存 serializer に局所 helper があり重複している。schema を Core の dedicated serializer へ寄せるのは将来の整理候補。
- **次に確認すべきこと:** project save/open の round-trip test または runtime 確認を追加。Undo は property変更とは別に router snapshot を扱う command が必要。

## 2026-08-29 — Router snapshot は保存・Undo とも完全置換にする

- **関連:** `ArtifactCore/include/Audio/Modulation/Router.ixx`、`tests/ArtifactCore/AudioModulationRouterTest.cpp`、effect/layer JSON restore helper
- **事実:** source definitions の復元だけでは、同じ source ID を参照する古い assignment が Router に残る可能性がある。JSON の load は通常 fresh instance だが、Undo/Redo と編集済み instance では完全置換が必要。
- **変更:** `ModulationRouterSnapshot`（sources、assignments、smoothingTime）と `restoreSnapshot()` を追加。restore 前に assignment を clear し、source を復元後に assignment を追加する。JSON helpers も source restore 前に assignment を clear するよう統一した。
- **価値または懸念:** save/load と Undo command が同じ state shape を共有でき、stale mapping を残さない。snapshot は runtime phase/gate を含まないため、Undo 後の source 再生位置は frame clock により再評価される。
- **次に確認すべきこと:** existing UndoManager の command パターンに snapshot を接続し、assignment追加・削除・depth変更を同じ command で扱う。UI を作る前に effect と layer の所有者解決を含む command API が必要。

## 2026-08-29 — Modulation Undo command は state のみを復元する

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`
- **事実:** UndoManager は immediate redo を実行して history に積み、effect/layer を weak reference で保持する command と `notifyAnythingChanged()` の既存パターンを持つ。Animation Layer snapshot も選択・再生時刻を復元しない。
- **変更:** `EffectModulationSnapshotCommand` / `LayerModulationSnapshotCommand` を追加。前後の `ModulationRouterSnapshot` を完全置換し、memory estimate は source definition / assignment path / label を含める。session history への command serialization は未実装で、通常の in-memory undo として扱う。
- **価値または懸念:** assignment の add/remove/depth/source configuration を単一 command にまとめられる。target がすでに破棄済みでも安全に no-op となるが、全体更新通知は送る。選択・frame・dirty/cache の実ランタイム挙動は未検証。
- **次に確認すべきこと:** Inspector 導線は effect/layer の owner を保持した上で before snapshot → mutate copy → `UndoManager::push` を使う。runtime で undo/redo、project dirty、preview 再描画、save/reopen を確認する。

## 2026-08-29 — mapping は存在する source だけを参照できる

- **関連:** `ArtifactCore/include/Audio/Modulation/Router.ixx`、`tests/ArtifactCore/AudioModulationRouterTest.cpp`
- **事実:** assignment は source ID を保持するが、従来 `addAssignment()` は source の存在を確認していなかった。source を欠く assignment は評価時に無効扱いになるだけで、保存・編集状態に残り得た。
- **変更:** `addAssignment()` が source ID 0、target ID 0、または未登録 source を拒否するように変更。source definition restore の後に assignment を追加する JSON / snapshot の順序と整合する。
- **価値または懸念:** 削除済み source の mapping を UI や復元が生成しない。未対応の source type を含む将来ファイルでは、対応 source を復元できない限り mapping も drop されるため、migration 時には明示的な warning/diagnostic が必要。
- **次に確認すべきこと:** Inspector 導線は assignment 作成失敗を表示可能な validation として扱う。UI 実装前に、既存 Property Editor の event/command 経路を確認する。

## 2026-08-29 — Macro は最初の時間非依存 modulation source

- **関連:** `ArtifactCore/include/Audio/Modulation/{Modulator,Router}.ixx`、effect/layer modulation JSON helper、`tests/ArtifactCore/AudioModulationRouterTest.cpp`
- **事実:** LFO / ADSR / Random は時間または gate で変化する。一方、複数 mapping を同じ手動値で駆動する Macro は Bitwig の unified modulation workflow における基本 source だが、現行 Core にはなかった。
- **変更:** `MacroSource` を追加。値は非有限値を 0、有限値を [0,1] へ clamp して process ごとに同一値を返す。`ModulationSourceDefinition` の `Macro` / `macroValue` と effect/layer JSON schema に追加し、snapshot も自動的に保持する。
- **価値または懸念:** 音声入力や transport に依存せず、複数の property mapping を安全にまとめて制御できる。UI 未実装のため値は public source API 経由でのみ設定でき、automation/keyframe 化は未着手。
- **次に確認すべきこと:** Property Editor の既存変更 command 経路から Macro value と mapping を編集する小さな専用 section を実装し、Undo / project dirty / save-open を runtime で確認する。

## 2026-08-29 — Modulation JSON の source ID は符号付き int に狭めない

- **関連:** `Artifact/src/{Composition/ArtifactAbstractComposition,Layer/ArtifactAbstractLayer}.cppm`
- **事実:** Router の source ID は `uint32_t`。effect/layer JSON helper は以前 `int` へ cast して保存しており、ID が `INT_MAX` を越えると符号変換に依存した。
- **変更:** source definition の `id` と assignment の `sourceId` を JSON double として保存するよう統一。uint32 の全整数値は JSON double の正確な整数範囲内であり、既存の `QVariant::toUInt()` 復元と互換。
- **価値または懸念:** 長期編集・大量追加後にも source / assignment の参照一致を保てる。JSON helper は effect/layer に重複しているため、将来の schema version 追加時には Core serializer への統合が望ましい。
- **次に確認すべきこと:** max-range source ID を含む serialization 回帰テスト（Artifact target または project round-trip harness）を追加し、実ファイル保存で確認する。

## 2026-08-29 — source ID の wrap-around でも 0 を発行しない

- **関連:** `ArtifactCore/include/Audio/Modulation/Router.ixx`、`tests/ArtifactCore/AudioModulationRouterTest.cpp`
- **事実:** Router は source ID 0 を invalid として扱う。一方、単純な `nextSourceId_++` は `UINT32_MAX` の直後に 0 を発行し、assignment 作成が常に拒否される source を生み得た。
- **変更:** `addSource()` は next candidate が 0 の場合 1 に戻し、既存 source ID をスキップして未使用 ID を選ぶ。全 ID が埋まる理論上のケースは 0 を返して失敗する。最大 ID 復元後に次の source が 1 になる回帰ケースを追加した。
- **価値または懸念:** 長寿命 session や restore 後でも 0 が有効 source として混入しない。ID 枯渇時の UI feedback は未実装で、現状の public API は 0 を失敗値として返す。
- **次に確認すべきこと:** UI 導線では `addSource()` の 0 を validation error として扱う。実用上の ID 枯渇は到達不能に近いが、diagnostic を追加するなら Router owner 側で行う。

## 2026-08-29 — Effect service を modulation UI の正規 mutation 境界にする

- **関連:** `Artifact/include/Service/ArtifactEffectService.ixx`、`Artifact/src/Service/ArtifactEffectService.cppm`、`Artifact/include/Undo/UndoManager.ixx`
- **事実:** Effect service は layer/composition effect の解決、project dirty、LayerChangedEvent / ProjectChangedEvent、既存 `effectChanged` signal を既に担う。Router を UI が直接書き換えるとこれらを迂回する。
- **変更:** `setEffectModulationSnapshot()` と `setCompositionEffectModulationSnapshot()` を追加。現在の Router snapshot を before、受け取った snapshot を after として既存 UndoManager command に積み、service の既存 mutation 通知を実行する。新規 signal/slot は追加しない。
- **価値または懸念:** Inspector / Effect surface は owner 解決と Undo の詳細を持たずに編集できる。layer 自身の Router（opacity）は別の layer service mutation API が必要で、今回の effect service 対象外。
- **次に確認すべきこと:** effect property surface から Macro source の値・assignment を編集する UI を追加し、service API 経由で project dirty と undo/redo を runtime 確認する。layer Router は Property Editor の既存 layer mutation service を調査して別途接続する。

## 2026-08-28 — 画像エフェクト歪み系の再活性化（CC プリセット系は別タスク）

- **関連:** [`docs/planned/MILESTONE_DISTORT_EFFECTS_COMPLETION.md`](docs/planned/MILESTONE_DISTORT_EFFECTS_COMPLETION.md)（ハブ、2026-08-28 再活性化）、[`docs/planned/MILESTONE_MESH_WARP_LIQUIFY_2026-06-02.md`](docs/planned/MILESTONE_MESH_WARP_LIQUIFY_2026-06-02.md)（集約先参照のみ追記）
- **事実:** 2026-08-15 監査で歪み系は Phase 1〜3 とも未着手だった。`MILESTONE_DISTORT_EFFECTS_COMPLETION` をハブとして `**最終更新: 2026-08-28**` / `**ステータス: In Progress**` に再活性化。着手 Phase は **Phase 2 (TwistTransform / BendTransform)** から。`Artifact/include/Effects/Transform/{TwistTransform,BendTransform}.ixx` は header-only stub 82 行で `applyCPU()` 未実装。
- **CC プリセット系判断:** CC Glass / CC Twirl / CC Ball Action / CC Power Pin / CC Grid Wipe / CC Kaleida 等は「汎用性が低いので単純導入はやめたほうがよい」（2026-08-28 ユーザー判断）。本書スコープ外、必要時に別マイルストーン化。
- **価値:** Phase 2 は P0・工数最小。`applyCPU()` 追記 + キーフレーム `AnimatableFloat` 評価の薄い実装で成立。GPU パスは Phase 1 の `runDistortCompute()` 共通ヘルパー作成後に着手。
- **未検証:** 既存歪み効果の GPU/CPU parity（LiquifyEffect 85%、Spherize 75%、Wave 65%、LensDistortion 75% — 2026-08-15 監査）。
- **次の確認:** Phase 2 着手可否（ビルド・テスト実行はユーザー指示待ち）。Phase 2 完了後、Phase 1 → Phase 3 → Phase 4（CC 除く）の順。Liquify 部分は既存 `LiquifyEffect` 85% 完成の基盤を Phase 2 完了後に統合。

## 2026-08-28 — Phase 2 (TwistTransform / BendTransform) CPU 実装（ビルド未検証）

- **関連:** `Artifact/include/Effects/Transform/{TwistTransform,BendTransform}.ixx`（書き換え）、`Artifact/src/Effects/Transform/{TwistTransform,BendTransform}.cppm`（新規）、`Artifact/cmake/ArtifactSources.cmake:752-753`（明示リスト追加）、`Artifact/src/Service/ArtifactEffectService.cppm:90-91,613-621,1166-1167`（既存 import / ファクトリ / EffectID 登録）
- **事実:** 既存 `.ixx` は `ArtifactAbstractFieldPtr field_` を保持する header-only stub で apply メソッドなし。新規 `.cppm` で **PIMPL + DualImpl パターン**（`SpherizeEffect` と同じ）に書き直し、`applyCPU` を OpenCV + `Core::Parallel::For` で実装。`applyGPU` は基底 `ArtifactEffectImplBase` デフォルトで CPU フォールバック。`ArtifactAbstractEffect::apply` 内で `cpuImpl_->applyCPU` / `gpuImpl_->applyGPU` が自動呼出。
- **Twist 数式:** 中心 (cx, cy) からの距離 r に対する線形減衰 `factor = 1 - r/r_max`、各ピクセル (x, y) を `theta = angle · factor` で回転。`angle` は degree。bilinear サンプル。
- **Bend 数式:** `nx = sin(2π·y/size)·amount` / `ny = sin(2π·x/size)·amount` を `direction` 角で H/V 合成。`amount` は pixel 振幅、`size` は波の周期。`direction = 0°` で水平波、`90°` で垂直波。
- **価値:** 既存 `ArtifactAbstractFieldPtr` 経路を撤去し、既存歪み効果（Spherize / LensDistortion / Wave）と同じパターンに揃えることで、`ArtifactEffectService` の既存 catalog 登録（`"twist"` / `"bend"`）が機能する状態に。
- **未検証・懸念:** ビルド・runtime 未実施。**`Bend` の `amount` を pixel 単位振幅で扱ってる**（AE 互換の degree 換算ではない）。AE 互換の degree 換算が必要なら `setAngle` 側で `amount_rad = angle · π/180` を施すラッパを後付け。Bilinear サンプル時の `OpenCV` `cv::Vec4f` 演算は `operator*`/`operator+` のスカラー対応に依存（OpenCV 4.x で提供）。`factor` のクランプ（中心 r=0 で factor=1、r=r_max で factor=0。`r > r_max` のピクセルは `factor < 0` で逆回転 — 仕様上問題なければ OK）。
- **次の確認:** ビルド成功 → EffectService カタログ確認 → テスト画像（チェッカーボード/同心円）で Twist 45°/90°/180°、Bend direction 0°/90°/45°、amount 0/10/50 で期待通りの挙動。GPU パス化は Phase 1 `runDistortCompute()` ヘルパー作成後。

## 2026-08-24 — Inspector を Composition Viewer ツールバー風に整理したいという要望の保留メモ

- **関連:** `docs/WIDGET_MAP.md:21-26,33,82`（Inspector vs Composition Editor vs PropertyEditor 責務）、`Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`Artifact/docs/PROPERTY_EDITOR_AUDIT_2026-03-11.md`、`references/artifactstudio-property-editor-review.md`、ユーザー提示画像（AEの `Composition`ビューポート + 下部 `100% / 0:00:01:00 / フル画質 / アクティブカメラ / 1画面` ツールバー + `エフェクトコントロール: adjustment / CC Power Pin`）
- **事実:** ユーザーが「下部のメニューみたいにインスペクタのメニューも整理したい」と要望。画像下部は `ArtifactCompositionEditor`相当のビューポート状態バー（ズーム・時間・画質・カメラ・レイアウト）で、インスペクタ（`ArtifactInspectorWidget` `Panel.Inspector`）とは責務が異なる。レビューはソースのみでランタイム未検証。
- **判定（未実装・メモ止まり）:** インスペクタを下部ツールバー風の水平密詰めアイコン帯にすると (1) Inspectorがviewport責務を吸収する境界違反（AGENTS.mdの新規signal/global配線禁止に抵触）、(2) 常時表示クロームで編集領域がfold以下に押される密度問題、(3) ラベル-値スキャンとグループ階層の喪失、が起きる。AEも `Composition`ツールバーと `エフェクトコントロール`を別ドックに分離しており、混ぜないのが正規。
- **次の確認（要望再開時）:** 下部バーはComposition Editorに残し、Inspector側は `PROPERTY_EDITOR_AUDIT` 方針通り `ArtifactPropertyWidget`/`ArtifactPropertyEditorRowWidget`内で行クローム（リセット/keyframe affordance）とグループ順（Transform優先、Components由来は露出しない）を整える最小修正で対応するか、ユーザーと再合意する。ユーザーは現時点では「よくわからない」とのことで一旦メモのみ。

## 2026-08-24 — オーディオミックス契約: layer volume/pan の二重適用（静的確認済み、未修正）

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm:3163-3170`（mixer 経路がレイヤー volume をバスフェーダー dB へ設定）、`ArtifactCore/src/Audio/AudioBus.cppm:306-364`（process が post-fader で volumeDb を線形ゲイン適用）、`Artifact/src/Layer/ArtifactAudioLayer.cppm`（getAudio がサンプルへ volume を乗算、pan もレイヤー内で定電力適用）、`Artifact/src/Layer/ArtifactVideoLayer.cppm:2791-2792`（video getAudio も audioVolume を内部適用）、`Artifact/src/Service/ArtifactAudioService.cppm:227-264`（setLayerBusVolume/setLayerBusPan がレイヤー property とバス両方に同値を書き込み）
- **事実:** mixer 有効時、レイヤー volume は (1) getAudio 内のサンプル乗算と (2) バスフェーダー gain の二重適用になる（vol=0.5 で -6dB ではなく約 -12dB）。pan もレイヤー内定電力とバス post-fader の二重適用で center が落ちる。legacy 直加算経路（mixer 無効）はレイヤー内適用のみで単回。つまり mixer の有無で同じ project の音量・定位が変わる。Service/UI はレイヤー property とバス dB の両方を永続化しており、project 再読込後も二重適用が再現する。
- **価値・懸念:** DAW 的にはゲインステージングの所有者を一本化する必要がある（案A: バスフェーダー唯一、レイヤー getAudio は raw 出力＋legacy 経路で明示適用／案B: レイヤー内唯一、layer bus のフェーダー適用をスキップ）。いずれも全 getAudio 呼び出し元（PlaybackEngine / Scrub / RenderQueue / RenderController）とエクスポート parity、meter 表示基準に波及するため、単独スライス＋実機検証が必要。
- **次の確認:** 方向決定（設計レビュー）後、ゲインステージング単一化スライスとして実装し、mixer on/off での同一 project 音量一致を実機確認。

## 2026-08-23 — Physics 全体監査: SoftBody LOD 反復上書きバグ、決定性リスク、死に経路（コード確認済み）

- **関連:** `ArtifactCore/src/Physics/{SoftBodySolver,PhysicsSystem,FluidSolver2D,SandSim2D}.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`docs/planned/MILESTONE_PHYSICS_PRODUCTION_HARDENING.md`、`MILESTONE_LAYER_PHYSICS_COMPONENT_2026-06-13.md`
- **事実:** (1) `SoftBodySolver::update()` の既定引数 `iterations=5`（L407）が `simulateStep` 内で `constraintIterations_` を毎ステップ無条件上書きし（L432）、`PhysicsSystem::update` の LOD 設定 `setConstraintIterations`（PhysicsSystem.cppm:424-426）が実質無効。`collisionIterations_` は影響なし。(2) `SandSim2D` は既定シード `std::random_device{}`（L16）で再生再現が非決定、`setRandomSeed` 明示時のみ決定的。(3) `FluidSolver2D::update(dt)` は dt 無 clamp・固定ステップなし、並列パス（`parallelEnabled_`/`advectRows`）は一度も使われない死蔵コード。(4) RigidBody(Box2D) 実行経路は入口 `enableRigidBodyPhysics` の呼び出しゼロで到達不能＋Box2D v3 ラップの friction/restitution 未適用（Physics2D.cppm:121-122,:144-145）。(5) 死に経路群: `PhysicsSystem::initFluid/getFluidSolver`（グローバル流体）、`setPhysicsLODSettings` UI、`softBodyDeformationMesh()`、`enabledLayerComponents(phase)`（phase 順は検証のみで実行順未使用）、`Graphics.RenderIndex`、`Graphics.BoidsCompute`、`FluidVisualizer`。(6) 物理ユニットテストはゼロ。(7) 共有 PhysicsWorld・Polygon collider は未実装のまま。**【2026-08-23 訂正】** 初回記載の「clone physics の `initialVelocityY/maxBounces` が solver 未消費」は誤り — `applyClonePhysicsTiming`（ArtifactCloneEffectSupport.ixx:450-453）がプロパティキャッシュ経由で消費している。真の問題は impl_ メンバーとプロパティキャッシュの二重管理で、プログラム経路（JSON 復元・undo・reaction・composition override）がメンバーのみ更新し、キャッシュ未同期またはミス時に physics timing が既定値 (0/4) にフォールバックする鮮度バグだった。
- **価値・懸念:** (1) は LOD 機能の品質とパフォーマンスに直接響く確定バグ。(2) はベイク/再生一致を壊す要因。(3) は巨大 dt で速度場発散の可能性。
- **対応状況 (2026-08-23):** (1) `SoftBodySolver::update/simulateStep` の既定引数を `-1`（維持）へ変更し `iterations > 0` 時のみ上書き。(2) 既定シードを固定値 `0x9E3779B9` へ変更。(3) `FluidSolver2D::update` に dt<=0 早期リターン＋0.05s clamp を追加し、死蔵並列パスを削除。決定性回帰テスト `tests/ArtifactCore/PhysicsDeterminismTest.cpp` を追加。(8) SoftBody 自己衝突を空間ハッシュ broadphase 化（候補順を edge→昇順 point に正規化し総当たりと同じ訪問順を維持）。(9) `SoftBodyCollider`/`MpmCollider2D` に `Polygon` 型追加（even-odd 内外判定＋最近傍辺へ射影、Core レベルのみで UI shape 0..2 は未拡張）。(10) clone physics の `initialVelocityY/maxBounces` を `persistentLayerProperty` 経由でキャッシュ同期（setter と JSON 復元の両経路）。未着手: (4) RigidBody 入口復活 or 削除判断、(5)(7) の残り死に経路整理、Polygon collider の UI/JSON 露出。
- **次の確認:** ビルドと `ArtifactCorePhysicsDeterminismTest` 実行（許可待ち）。自己衝突ハッシュ化後の挙動比較（broadphase はステップ開始時位置基準のため、極端な押し出しが同一ステップ内で半径超過になる稀なケースで brute-force と差が出る可能性・未検証）。RigidBody/Box2D 経路の扱い判断。

## 2026-08-24 — MPM GPU compute backend 実装: CPU fallback 付き設計、parity 未検証

- **関連:** `ArtifactCore/src/Graphics/MpmCompute.cppm`（NEW）、`ArtifactCore/include/Graphics/MpmCompute.ixx`（NEW）、`ArtifactCore/src/Physics/MpmSolver2D.cppm`、`ArtifactCore/include/Physics/MpmSolver2D.ixx`
- **事実:** (1) MPM GPU compute backend を `SandGPUCompute` パターンに倣い実装。5 つの HLSL ナレッジ（Clear/P2G/Forces/GridUpdate/G2P+F）を `groupshared` grid で ordered atomic 累積。P2G で `float32_t` 原子加算のみ使用（`uint32` reinterpret は SandGPUCompute からの直写に留める）。(2) `MpmSolver2D` は `setBackend(MpmBackend::GPU)` + `attachGPUSimulation(dev, ctx)` で GPU レーンに切替可能。CPU fallback は compute dispatch 失敗時に自動的。`(3)` GPU lane は elastic のみ parallelize し、plasticity/fracture/colliders は CPU tail で 1 回実行。parity は未検証（MPM の ordering dependency を nonatomic 化しているため last-bit nondeterminism が発生する可能性あり）。(4) `addImpulse` を正規化方向 + `isfinite` guard + zero-distance skip で改善。NaN guard を SoftBody/Fluid の simulation loop 末尾に追加。
- **価値・懸念:** GPU パスは elastic のみが GPU kernel 内で処理され、plasticity/fracture は CPU で後処理されるため、CPU パスとの物理 parity は保証されない（MPM の ordering dependency を nonatomic 化しているため last-bit nondeterminism が発生する可能性あり）。反復回数/物質パラメータの parity は維持可能だが、exact replay は CPU のみで保証。
- **未検証:** MpmCompute の HLSL パスが Diligent の DX12/Vulkan compute で正しく dispatch されるか、粒子数 0/1 のエッジケース、readback の遅延 sync 問題。
- **次の確認:** ビルド許可後に MpmCompute ターゲットのコンパイル確認、`MpmSolver2D::setBackend(GPU)` + 10K particles の動作確認、CPU と GPU の結果 diff 比較（parity が許容範囲内か判断）。

## 2026-08-24 — Physics バグ修正バッチ: LOD/Fluid/Mpm/Box2D/決定性 (コード修正済み、ビルド未検証)

- **関連:** `SoftBodySolver.cppm:131` `reduceGridResolution`, `FluidSolver2D.cppm:114` `computeSolverIterations`, `MpmSolver2D.cppm:1104` NaN guard, `PhysicsSystem.cppm:132` per-layer Fluid, `Physics2D.cppm:93` Box2D材質, `SandSim2D` seed
- **事実:** (1) SoftBody LODが 0.75→0.5 と二段階で縮小する際に既縮小グリッドから再サンプルして劣化していたのをバックアップ元から再サンプルするように修正。(2) Fluid adaptive反復が閾値ちょうどで+4跳ねていたのを `(blocks-1)` にして閾値ではbaseline維持に平坦化。(3) Mpm `stepOnce`/GPU tail 末尾に `isfinite` ガード追加で非有限pos/velをゼロ化、Fをidentityリセット。(4) Box2D v3で `friction/restitution` がコメントアウトで無効だったのを `shapeDef.material` へ修正、static bodyも `bodies` に追跡。(5) SandSim 固定seed `0x9E3779B9` で決定性確保、Fluid/Sandの read-only getter、Mpm `particles()` view と `isGPUReady` 等を追加。per-layer Fluid と Pyro/Boids の PhysicsSystem 配線、`buildMpmParticleRenderData` ブリッジも追加。
- **価値・懸念:** いずれも既存挙動の局所修正/追加で副作用は小。Box2D材質とFluid閾値は挙動が変わるため既存プロジェクトの再現に影響するが意図した修正。
- **次の確認:** `ArtifactCorePhysicsDeterminismTest` と手動の Fluid/SoftBody LOD 変化の目視確認 (ビルド許可待ち)。

## 2026-08-23 — オーディオレイヤー精査: getAudio のマルチスレッド呼び出しと単一スロットキャッシュ、死にコード（未検証）

- **関連:** `Artifact/src/Layer/ArtifactAudioLayer.cppm`（getAudio :873 / resampledCache_ / decodeFrameToCache :826）、呼び出し元: PlaybackEngine(:756,:847)、AudioScrubController(:153)、TimelineWidget(:549)、RenderQueueService(:365,:407)、RenderController(:37174)
- **事実:** (1) PCM は AssetManager の共有ペイロード（audio.pcm.f32）としてレイヤー間共有され、getAudio は線形補間リサンプル＋volume/clipGain/fade/pan/deClick を都度適用する設計。(2) getAudio は**再生エンジン・スクラブ・タイムライン UI・レンダーキュー worker の複数スレッドから呼ばれる**が、`impl_->resampledCache_` への読み書きは無ロック。（3）resampledCache_ は startSample 単一スロットで、連続再生ではほぼ毎フレーム miss（全リミックス再計算）。計算量は軽いので実害は限定的と推定（未検証）。(4) `decodeFrameToCache()` と `cache_` は**呼び出しゼロの死にコード**（.ixx private 宣言のみ）。(5) タイムリマップは非対応（画像シーケンスは対応済みで機能差）。
- **価値・懸念:** (2) は画像レイヤーで指摘したものと同型のデータ競合で、audio スレッドと render worker が同時呼び出しする現実的な経路がある。修正案: mutex 保護 or 呼び出しスレッド別キャッシュ、(4) は削除で可。
- **対応状況 (2026-08-23):** (5) タイムリマップ対応を実装。getAudio が `isTimeRemapEnabled()` 時はビデオレイヤーと同じ規約（comp フレーム → `getSourceFrameAtCompFrame`）で各出力フレームをマッピングし、remap 中は resampledCache_ をバイパス。(2) は `resampledCacheMutex_` 導入済み（commit 60b68eb0）。同日の機能拡充スライスで追加修正: 作業ツリーに残っていたキャッシュ照会部の宣言前使用（`volume`/`effPan`/`effClipGainDb`、コンパイル不通）を解消。タイムリマップ経路が `producedFrames > 0` でも `false` を返し合成側が音声を破棄するバグを修正（remap 音声が無音になる確定バグ）。アニメート済み volume/pan/clipGain の評価をプレイヘッドではなく要求ブロックの comp フレーム基準に変更し preview/export の一致を改善。非破壊 trim in/out・playback rate API を追加。残る (4) 死にコード削除は未着手。
- **次の確認:** (4) の削除。リマップ時の音ズレを実機確認（remap キーはコンポフレーム基準）。trim/rate の保存再読込とエクスポート parity の実機確認。

## 2026-08-23 — 画像レイヤー精査: 連番は堅牢だがシーケンス GPU アップロードと const 経路の可變状態が懸念（未検証）

- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cppm`（refreshSequenceFrame :1091 / draw :2134 / toQImage :2264 / canShareSourceGpuTexture :1517）、`ArtifactCore/src/Media/ImageSequenceSource.cppm`（frameIndexAtTime :482）
- **事実:** (1) 連番再生は `ImageSequenceSource` 経由でフレーム追従し、タイムリマップ対応・in/out 境界のホールド契約・欠番時に stale pixel を返さない契約が明示実装されている。fps 不一致は `frameIndexAtTime` で時間ベース変換され二重適用はない。(2) `refreshSequenceFrame` は const メソッド（toQImage/draw）から mutable メンバ cache_/cacheBuffer_/sequenceSource_ を**ロック無しで書き換える**。ファイル内に mutex は存在しない。(3) `canShareSourceGpuTexture()` は連番を除外しており、シーケンス描画はフレーム毎の GPU 再アップロードになる構造。(4) $F テンプレートはロード時 frame=0 固定（既知・記録済み）。
- **価値・懸念:** (2) はメインスレッドのサムネイル/プロパティ UI とレンダーワーカの toQImage 同時呼び出しでデータ競合になり得る（呼び出し直列化の保証を実装で確認していない）。(3) は長尺連番・4K で顕在化する性能懸念。改善候補: 軽量 mutex or ダブルバッファ化（B案）、`GPUTextureCacheManager` を使うシーケンスフレーム LRU（A案）、draw 時 $F 展開による連番テンプレート追従（C案、変数展開 milestone の次段と接続）。
- **対応状況 (2026-08-23):** (1) `refreshSequenceFrame` を `sequenceStateMutex_` で直列化し、入れ替え前のフレームバッファを retired リスト（最大4世代）に退避させてリーダの使用中解放を回避。currentFrameBuffer 側の lazy 生成も二重検査ロック化。(2) ビューポート描画（CompositionViewDrawing）でシーケンスを `seq-f32:f<frame>|cs|tf` キーで GPUTextureCacheManager に乗せ、フレーム毎の再アップロードを解消（既存 budget/LRU で追い出し）。新公開 API `ArtifactImageLayer::sequenceCachedFrameIndex()`。
- **次の確認:** マルチスレッド再生での競合消失確認（TSAN 相当は無いため実機観察）。シーケンス再生時の GPU メモリ使用が LRU 予算内に収まること。

## 2026-08-22 — ArtifactPr コアの Core 共有モジュール統合（ビルド検証待ち）

- **関連:** `ArtifactCore/include/Video/FFMpegVideoDecoder.ixx`(seekToFrame 公開化)、`ArtifactPr/include/ArtifactPrEditorEngine.ixx`、`ArtifactPr/include/EditCommand.ixx`、`ArtifactPr/src/MediaFrameDecoder.cppm`、`ArtifactPr/src/SequenceExporter.cppm`、`ArtifactPr/src/AudioPreviewMixer.cppm`、`ArtifactPr/CMakeLists.txt`
- **事実:** ArtifactPr は手書き Undo スタック(QVector<UndoCommand*> + 独自基底)、cv::VideoCapture 直叩きデコード、float 手書き加算ミックスと、Core 既存機構(Command.Session / Codec.FFmpegVideoDecoder / Audio.Mixer)との三重実装だった。Core の Command/Audio/Video/NLE は集約ターゲットから分離した STATIC lib(ArtifactCoreCommand/Audio/Video/NLE)。FFmpegVideoDecoder の seek は Impl private で公開 API になかった。
- **対応状況 (2026-08-22):** (1) Core FFMpegVideoDecoder に `seekToFrame(int64_t)`(既存 Impl::seekByFrameNumber を bool 返しで公開)+width/height/fps getter を追加。(2) Pr 全コマンド16種を Core SerializableCommand 派生に(commandType/serialize/deserialize 実装、description()→setText())、Engine のスタックを EditSession(QUndoStack)へ置換。QUndoStack::push が redo 即時実行するため「cmd->redo(); pushUndo()」パターンを廃止し push 一本化、未使用だった AddMarker/DeleteMarker コマンドは削除。(3) MediaFrameDecoder/SequenceExporter の同期デコードを FFmpegVideoDecoder へ置換(RGB24→RGBA 詰め替え、静止画は ImageF32x4_RGBA::load)。(4) AudioPreviewMixer を AudioMixer バスグラフ経由に(クリップ毎 createBus+master connect、addInput→process)。CMake は domain ターゲット4種を明示リンク。
- **次の確認:** ビルド後、(1) undo/redo 一巡(move/trim/slip/slide/ripple-delete/insert/overwrite/lift/marker/transition)で状態完全復元、プロジェクト新規/読込でスタッククリア。(2) プレビューの動画シーク＋順次再生・静止画表示・エクスポート差分。(3) 2トラック音声のバランス・メーター・seek 整合。特に QUndoStack::push の redo 即時実行による二重適用が無いことを DeleteClip(旧コードは push 後手動 removeAt で二重になるパターンがあった)周辺で確認。
- **価値・懸念:** serialize 実装により将来のコラボ同期(CommandFactory 経由の op 再現)の土台ができた。懸念は (a) EditSession.pushCommand が全コマンドを historyLog_ に蓄積し続ける(長時間セッションでメモリ増)、(b) ClipPropertyCommand の serialize が QVariant を toString で持つため数値型情報が落ちる(deserialize 後は文字列として再適用される — doApply は toDouble/toBool 変換するので実害は限定的だが厳密には非対称)、(c) rebuildBuses が mixer を丸ごと作り直すので setClips 頻発時にバスオブジェクトが再生成される。

## 2026-08-22 — シェイプ領域: コアShapeLayerの孤立とSVG出力の未接続（→ 2026-08-23 接続実装済み、コアSVGは単色のみ）

- **関連:** `ArtifactCore/src/IO/VectorExport.cppm`、`ArtifactCore/include/Shape/ShapeLayer.ixx`、`Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`
- **事実:** (1) `SvgFrameExporter`（コア、完全実装）の呼び出しは Artifact 配下にゼロで、RenderQueueService の SVG 出力分岐は黒矩形スタブだった。(2) アプリ→コア ShapeLayer の変換層は存在しない。(3) コア `svgStyleString()`（ShapeLayer.cppm:98）は fill/stroke を単色 HexArgb のみ出力し、`FillSettings::FillType` のグラデーション種別は無視される。
- **対応状況 (2026-08-23):** 案Aで接続済み。`ArtifactShapeLayer::toCoreShapeLayer()` 新設＋RenderQueueService SVG 分岐を実パス出力に置換。P2 の shape.* キーフレームも `resolveShapeGeomDims()` 経由で全描画経路（GPU/ソフト/bounds/card points/SVG）一元評価し、キーフレーム有り時のみキャッシュをフレーム再構築。詳細は `docs/planned/MILESTONE_SHAPE_SVG_EXPORT_AND_KEYFRAME_VERIFY_2026-08-22.md`。
- **次の確認:** (1) コア `svgStyleString` / `elementToSvg` を拡張し `<linearGradient>`/`<radialGradient>` defs 出力に対応すると、現在の中間色縮退（fill/stroke の start/end 中間色）と stroke taper / stroke align 未反映を解消できる。(2) ビルド後、SVG 出力・width キーフレーム再生の実機確認。

## 2026-08-22 — 連番画像の欠番契約: MissingFramePolicy が実装に存在しない（archive記載と不一致）

- **関連:** `ArtifactCore/include/Asset/AssetSequence.ixx`、`ArtifactCore/src/Media/ImageSequenceSource.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`（detectSequenceImportGroups）、`Artifact/src/Layer/ArtifactImageLayer.cppm`（refreshSequenceFrame）
- **事実:** `docs/analysis/INSIGHT_ARCHIVE_2026-08-11.md` の2026-07-27項は「`MissingFramePolicy` を追加し、既定 `Split` / 明示 `Preserve` + `missingFrames` 返却」と記録しているが、現行ツリーにも `ArtifactStudio_merge_20260717_check` スナップショットにも `MissingFramePolicy` は存在しない（git log でも該当commit無し）。現状は gap での無条件 Split のみ。一方 `ImageSequenceSource::open()`（単一ファイルからのディレクトリ再スキャン経路）は元フレーム番号を保持して欠番を1本のシーケンスに残すため、**同一フォルダが「複数の連続シーケンス」（複数ファイルインポート経由）と「欠番を含む1本」（単一ファイルインポート経由）の2通りに解釈されうる**。Preserve/Split の選択導線は実装前のまま。
- **価値・懸念:** 検出ロジックが std::regex（AssetSequence）と QRegularExpression（ImageSequenceSource）で二重化しており、suffix 文字クラスも `\.[a-zA-Z0-9]+` と `\.[^.]+` で微妙に異なる。契約が1か所に集約されていないため将来の UI 追加時に乖離が再発しやすい。また `tests/` 配下に `Asset.Sequence` / `ImageSequenceSource` の直接テストが皆無。
- **対応状況 (2026-08-22):** (1) `Asset.Sequence` に `MissingFramePolicy`（既定 `Split`=現行挙動不変 / 明示 `Preserve`=1グループ保持+`SequenceGroup::missingFrames` 返却）を追加。既存呼び出し（ProjectService）はデフォルト引数のため無影響。(2) `ImageSequenceSource::parseSequencePattern` の suffix を `\.[a-zA-Z0-9]+` に整合。(3) `metadata()` の全フレーム stat ループを廃止し、open 時シード+フレームアクセス時遷移更新の `Impl::missingFrameCount` 追跡方式へ置換（数値欠番カウントとの累積は維持、欠番の検出が open/decode 時点ベースに変化）。ビルド未検証。
- **次の確認:** (1) `J:\dev\ArtifactStudio_TestSequences\png_missing_frame`（manifest.json付きの準備済みfixture）を使った GTest で、detectSequences の Split/Preserve 境界・openFramePaths の欠番保持・metadata().missingFrameCount・tryFrameAt の負の結果キャッシュを固定化する。(2) 単一ファイルインポート経路と複数ファイルインポート経路で同一フォルダの解釈が分岐する件を、Project View 上での見え方込みで実機確認する。
- **残タスク (2026-08-22 調査時に未修正):** `Artifact/src/Layer/` 直下に誤生成と思われるジャンクファイル `currentQImage_`・`drawSprite(0.0f` と放置の `ArtifactVideoLayer_draw.patch`（内容確認後に削除要）。`detectSequences` の GroupKey は padding を含むため `img_0001.png` と `img_000001.png` が別シーケンスに分離される（仕様として妥当かは要判断）。

## 2026-08-22 — CPU合成パスのブレンドモード潰れ修正と残課題（ビルド検証待ち）

- **関連:** `Artifact/src/Render/Software/ArtifactSoftwareImageCompositor.cppm`、`Artifact/include/Render/Software/ArtifactSoftwareImageCompositor.ixx`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`
- **事実:** オフライン監査(2026-08-22)で、CPUバックエンドの `renderSingleFrameComposition()` が全レイヤーを QPainter `setCompositionMode()` で合成しており、QPainter非対応の Subtract/Hue/Saturation/Color/Luminosity/Dissolve/DancingDissolve/LinearDodge/LinearBurn/Divide/PinLight/VividLight/LinearLight/HardMix/Classic系3種/Stencil/Silhouette が全て SourceOver に潰れ GPU と出力が不一致（パリティ破綻）。一方で同モジュール内に正確な float ブレンドエンジン(`blendColor()`)が匿名namespaceの死蔵コードとして存在していた。
- **対応状況 (2026-08-22):** compositor に `qPainterSupportsBlendMode()`(QPainterネイティブ再現可否判定)と `blendSurface()`(RGBA8888直書きのfloatブレンド、既存blendColorエンジンを活用、DissolveはdeterministicNoiseで決定論的分岐)を追加し、render queue のソフトパスで非対応モードだけ「透明一時キャンバスにtransform描画→painter.end()→blendSurface→painter.begin()再開」へ迂回。QPainter対応13モード(Normal/Add/Multiply/Screen/Overlay/Darken/Lighten/Dodge/Burn/HardLight/SoftLight/Difference/Exclusion)は現行経路のまま挙動不変。
- **次の確認:** ビルド・実行後、(1) Subtract/Hue/Luminosity レイヤーを含むコンポの CPU/GPU 出力差分、(2) transform+opacity付きレイヤーの位置ずれ、(3) Stencil/Silhouette のマット結果。GPU経路の `readbackToImageF32()` と同じ HSL 計算(ColorConversion)を使うため数学的一致は期待できるが8bit量子化差は残る。
- **価値・懸念:** `composeToBuffer()`(ImageF32x4出力、呼び出しゼロ)と float エンジンを接続すれば QImage ホットパス違反の抜本解消になるが、レイヤーサーフェス生成自体が `toQImage()` 前提のため今回は見送り(最小差分優先)。MFR有効化(useMfr=false @ ArtifactRenderQueueService.cppm:6660)には S1–S15 の snapshot 分離が前提で別案件。SVG エクスポートは黒矩形スタブのまま。

## 2026-08-22 — 乱数生成の統合（RandomStream統一）

- **関連:** `ArtifactCore/include/Math/Random.ixx`、`ArtifactCore/src/ImageProcessing/OpenCV/*.cppm`、`ArtifactCore/src/Geometry/Fracture.cppm`、`ArtifactCore/src/Generate/StarfieldGenerator.cppm`、`ArtifactCore/src/Animation/KeyframeEditingTools.cppm`
- **事実:** 乱数生成が4系統混在(RandomStream/mt19937+random_device/QRandomGenerator/std::rand)。OpenCVエフェクト群(Noise/Glitch/VHS/Glow)とStarfield/Fracture/Scatterはrandom_deviceまたは固定seed mt19937で決定論性が分断。MpmSolver2Dの「rand()使用」は誤検出(関数名のみ)。
- **対応状況 (2026-08-22):** (1) `Random` singletonにmutex追加でthread safety確保。(2) 8ファイルのmt19937+distributionをRandomStreamへ置換: Noise.cppm(gaussian+range)、GlitchCV.cppm(rangeInclusive+unitFloat)、VHS_CV.cppm(unitFloat)、Glow.cppm(unitFloat)、Scatter.cppm(nextU32)、Fracture.cppm(range、全関数signature変更)、StarfieldGenerator.cppm(regex一括置換で全distribution解消)、KeyframeEditingTools.cppm(gaussian+range、chrono entropy seed)。残るmt19937: TextAnimator(wiggly、既存seed決定論のため互換性維持)、ExpressionEvaluator(randomSeeded、同様)。残るrandom_device: ParticleSystem(TurbulenceForce)、SandSim2D、SoftwareRayTracer(thread_local、適切)、AudioTone。
- **次の確認:** ビルド後、各エフェクトのseed再現性を同一seed+同一入力で確認。TextAnimator/ExpressionEvaluatorのRandomStream移行は互換性ブレーキがあるため別判断。

## 2026-08-21 — ArtifactPr 二重モデル(NLEストア/legacy Demo*)の乖離とメタデータ欠落（未検証）

- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`ArtifactPr/include/ArtifactPrEditorEngine.ixx`
- **事実:** ArtifactPr は NLEProjectStore(真のモデル)と legacy Demo*(UI 読み取りビュー)の二重管理で、rebuildLegacySnapshotFromNLE() が一方向同期するが、splitClipAtPlayhead/duplicateSelectedClip/pasteClip/marker系/transition系は legacy 側のみ編集し nleStore_ を更新しない(乖離が蓄積)。通常インポート経路(addMediaToPool)は SourceRef を NLE ストアに登録せず、loadDemoProject 経由のみ固定ダミー(timeBase 30fps 固定・availableRange 0-100000f)で登録。saveProject/loadProject は legacy JSON のみで NLE ストアは非永続化。autoSaveIntervalSec_(エンジン)と MainWindow 固定60秒タイマーが二重管理。DemoClip には enabled フィールドがなく(NLE Clip.enabled は転記されず捨てられる)、トラック solo も合成側で未反映。
- **対応状況 (2026-08-21):** 大部分を解消 — split/duplicate/paste/lift/rippleDelete/insert/overwrite/marker系/transition系を NLE ストア経由に統一、Core へ `NLEProjectStore::removeMarker` 追加、rebuildLegacySnapshotFromNLE が markers/transitions 復元、save/load の NLE スナップショット永続化(+旧形式からの legacy→NLE 再構築 importLegacyProjectToNLE)、addMediaToPool プローブ登録、DemoClip.enabled 追加、solo/mute/enabled 合成反映。残課題: autoSave 二重管理のみ未着手。
- **次の確認:** ビルド検証後、split/paste 操作直後のプレビュー表示ずれが解消しているか再現確認する。

## 2026-08-22 — リポジトリに X:\Dev\ArtifactStudio の別コピーが存在し build/ がそちらに紐づく

- **関連:** `J:\dev\ArtifactStudio` (作業ツリー)、`X:\Dev\ArtifactStudio` (別コピー)、ルート `build/CMakeCache.txt`
- **事実:** ルート build/ の CMakeCache は source を `X:/Dev/ArtifactStudio` として生成されており(VS2022 Debug)、X 側 CMakeLists は ArtifactPr 無効の旧状態。J と X は同一ディレクトリではない(CMakeLists 内容不一致をハッシュで確認)。J 側で `cmake -S . -B build` すると "does not match the source used to generate cache" エラー。quick_build.bat / build_artifact.bat も `X:\dev\artifactstudio` 決め打ち。
- **仮説（未検証）:** X: 側が古い実験コピーか、逆に X: が正規で J がサンドボックスの可能性。ユーザー確認まで J 側でのビルド構成新設(X 側 build の削除・上書き)は行わない。
- **次の確認:** どちらが正規か、J 側へ新規 build dir (例: CMakePresets の out/build/x64-Debug-Agent)を作ってよいかユーザーに確認する。


## 2026-08-21 — コラボレーション機能の現状とセッションモデル新設（進行中）

- **関連:** `ArtifactCore/src/Collaborate/CollaborationSession.cppm`(新設)、`ArtifactCore/src/Collaborate/CollaborationProtocol.cppm`、`ArtifactCore/include/Network/CollaborationWebSocket.ixx/.cppm`、`tools/collaboration-server/server.js`
- **事実:** WebSocketクライアント(再接続/heartbeat/5メッセージ+rule sync)は完備だがアプリから未接続。サーバーはprojectId毎セッション+操作履歴中継+ロック権限+プレゼンス中継のプロトタイプ。セッションモデル(参加者名簿・ロック台帳・バージョン管理・エコー重複排除)が存在しなかった。operationペイロードはopaque JSONでスキーマ未定義。
- **対応状況 (2026-08-21):** `CollaborationSession`(module `Collaborate.Session`)を新設。トランスポート非依存の純粋状態モデルとして、(1)参加者名簿(join/left/presence、離脱時にpeerロック自動解放)、(2)操作ログ(server版号採番・エコー重複排除・pending local op への版号確定)、(3)ロック台帳(grant/deny/release+local要求pending追跡+isLayerLockedByOther)、を提供。QObject/signal非依存でAGENTS.mdのsignal接続制約を回避、全ロジックunit test可能(`CollaborationSessionTest.cpp` 4ケース+CMake登録)。
- **仮説（未検証）:** 次段階は (1) WebSocket→Session のアダプタ(signal接続のためAGENTS.md設計レビュー要)、(2) operation スキーマ定義(transform/property/layer追加削除のJSON契約)、(3) プレゼンスpayload標準化(カーソル・選択レイヤー・ビューポート)、(4) リモート操作とUndoの統合。
- **対応状況 (2026-08-22):** `CollabPresenceState`(型付きpresence、未知キーraw保持)をSessionに追加。`CollabReview.cppm` 新設(コメント: composition/layer/frameアンカー+スレッド1階層+解決/所有者限定編集削除、提案: Pending→Accepted/Rejected/Withdrawn遷移強制・acceptは操作配列返しのみで状態非変更)。監査でエコー照合キー欠陥を発見→`opSeq`フィールド追加で修正(同一ミリ秒衝突とサーバーtimestamp上書きの双方に耐性、回帰テスト追加)。監査全文は `docs/analysis/COLLABORATION_IMPLEMENTATION_AUDIT_2026-08-22.md`。テスト合計35ケース。
- **対応状況 (2026-08-22 追加):** `CollabOperations.cppm` 新設(operationスキーマ: property.set/layer.transform/layer.add/layer.remove のビルダー+バリデータ、未知type前方互換)。Sessionに `createLocalOperation(CollabOperationData)` オーバーロードと `staleParticipantClientIds(nowMs, timeoutMs)`(heartbeatタイムアウト検出)を追加。テスト合計38ケース。
- **対応状況 (2026-08-22 追加2):** `CollabOperations.cppm` に提案検証パイプラインを追加(`validateCollabProposal`/`acceptValidatedProposal` — 全操作検証通過までacceptを遮断)。サーバー(server.js)の `clientTimestamp` 保持修正(監査#6)。Sessionに `createLocalOperation(CollabOperationData)` オーバーロードと staleParticipantClientIds を追加。テスト合計41ケース。
- **対応状況 (2026-08-22 追加3):** `CollaborationSessionAdapter`(module `Collaborate.SessionAdapter`)新設 — WebSocket信号↔Sessionモデルのpoint-to-point接着(グローバル配線なし、接続はアダプタdtorで全解除)。signal 3種(userJoined/userLeft/remoteLockGranted)にclientIdパラメータを追加(セッションモデルに必須、既存使用箇所ゼロのため破壊的影響なし)。CollaborateターゲットがArtifactCoreNetworkへ依存。テスト合計42ケース(アダプタ経由のinboundルーティング: opSeq往復・roster・ロック・presence・離脱解放)。アダプタのoutbound(sendLocalOperation等)のruntime検証は実サーバー結合時に行う。
- **次の確認:** ビルド・テスト実行後、実サーバー(tools/collaboration-server)に対する結合確認。UI層(presence描画・コメントパネル)は別マイルストーン。

## 2026-08-21 — std代替レイヤー(Core.Artifact*)の充実方針と現状（進行中）

- **関連:** `ArtifactCore/include/Core/Artifact*.ixx`、`ArtifactCore/src/Core/*.cppm`、親 `CMakeLists.txt`(import std gate)
- **事実:** プロジェクトは実験的な `import std;`(C++23 std modules)を149ファイルで使用しており、これがコンパイルエラーの主要源。ユーザーの方針は std 依存の削減で、std再発明と思われた `Core.Artifact*` ファミリーは削除対象ではなく置換レイヤーとして完成させる方向。ハウススタイルは `artifact*` 接頭辞付き関数(artifactExchange/artifactBitCast/artifactCmp*)。import stdファイル内の実際の使用トップは max/clamp(算法~1000)、vector(331)、move/make_unique(~290)、string(117)、function(61)。既存: Array(自己完結vector)/String(SSO)/Span/Variant/Function/Dict(QHashラッパー)/Ptr+Ref/Mutex。
- **対応状況 (2026-08-21):** `Core.ArtifactMath.ixx` を新設(artifactMax/Min/Clamp/Abs/IsFinite/IsNaN/Sqrt/Pow/Sin/Cos/Tan/Atan2/Floor/Ceil/Lround/Llround/Lerp)。ArtifactUtilityに artifactMove/artifactForward を追加。ArtifactPtrに UniquePtr/makeUnique を追加。Arrayに operator[]/data() を補完(at()はOptional返しのまま)。Foundationが全モジュールをexport import。テスト `tests/ArtifactCore/ArtifactFoundationTest.cpp` 新設(Math/Utility/UniquePtr/Array/String/Dict)。未着手: import std 149ファイルの段階移行、unordered_mapの自己完結版検討、chrono/mutex系の整理。
- **対応状況 (2026-08-21 第2回):** `Core.ArtifactAlgorithms.ixx` を新設(artifactSort=heapsort/Find/FindIf/Contains/AllOf/AnyOf/NoneOf/MinElement/MaxElement/Fill/Reverse/RemoveIf/LowerBound/BinarySearch/IsSorted+コンテナオーバーロード)。ArtifactMathに `NumericTraits<T>` を追加。ArtifactUtilityに `Pair`/`artifactMakePair` を追加。Arrayに `StaticArray<T,N>`(std::array代替)を追加。既存の ArtifactHashMap が自己完結チェインバケット実装として完備済みであることを確認(Dict のQHash依存は別経路)。
- **対応状況 (2026-08-21 第3回):** `Core.ArtifactTuple.ixx` を新設(再帰Tuple/artifactGet<I>/artifactMakeTuple/tupleSizeV/等値比較)。StringにASCIIユーティリティを追加(asciiLower/Upper/Trimmed/StartsWith/EndsWith/Split/Join — ロケール非依存)。ArtifactAlgorithmsに Accumulate/Iota/Count/CountIf/MinMaxElement を追加。ArtifactUtilityに `artifactHashCombine` を追加。
- **対応状況 (2026-08-21 第4回):** `Core.ArtifactChrono.ixx` を新設(Duration(ns分解能)+SteadyClock(QueryPerformanceCounter/clock_gettimeのプラットフォーム分岐、秒毎tickキャッシュ)+Stopwatch)。`Core.ArtifactRegex.ixx` を新設(パーサ→AST→バイトコード→明示スタックbacktracking VM。文字クラス/量詞lazy含む/選択/グループ/アンカー/エスケープ対応。ステップ上限400万・プログラム8192命令・グループ9個上限)。replaceAllは $0-$9/$$ 置換対応。
- **対応状況 (2026-08-21 第5回):** `ArtifactSet.ixx` を自己完結HashSetへ全面書き換え(チェインバケット+挿入順イテレーション、load factor 0.75で自動rehash、`HashSet<T>` + `ArtifactSet` エイリアス。旧raw()は削除—使用箇所ゼロ確認済み)。Regexに **lookahead `(?=...)` `(?!...)`** を追加(AST→子ノードをサブプログラムとして別コンパイル、VMはLookahead命令でネスト実行。positive成功時はキャプチャ保持・negativeは常にslot復元)と**後方参照 `\1`-`\9`**(パース時に既出グループ番号検証、VMはバイト一致比較、未設定グループはfail)。非対応: lookbehind・条件分岐・再帰をヘッダ明記。テスト合計31ケース(Set操作/lookahead消費なし検証/negative位置/後方参照繰り返し語/未開放グループエラー)。

## 2026-08-21 — Frame/Timeクラス全面PImplのホットパスヒープ確保（未検証）

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`ArtifactCore/src/Frame/FramePosition.cppm`、`Artifact/include/Layer/ArtifactAbstractLayer.ixx`(L433 currentFrame/inPoint)、`ArtifactCore/include/Property/AbstractProperty.ixx`(L296 evaluateValue)
- **事実:** FramePosition/FrameRange/FrameRate/RationalTime/TimeCode/FrameOffset/FrameTime/Durationが全てインスタンスごとに`new Impl`。プロパティ評価`interpolateValue(RationalTime)`はレイヤー×プロパティ×フレームで呼ばれ毎回ヒープ確保。レイヤーAPIは`currentFrame()=int64_t`と`inPoint()/outPoint()=FramePosition`で混在。対照的にFloatColorは直接メンバ16バイト。RationalTimeの比較は連分数で正確だが、`operator+/−`は`value*(lcm/scale)`展開でfromSeconds(scale=1e7)長尺時にint64オーバーフローリスク、`rescaledTo`は切り捨て(llround非整合)。Durationはほぼコメントアウトの死にクラス、TimeCodeRangeは未接続。
- **仮説（未検証）:** value型化(PImpl廃止)が性能の根本解だがABI影響大。まず実測(profiler)でRationalTime生成がフレーム時間に占める割合を確認し、ホットパス限定でint64オーバーロードやキャッシュを導入する段階的移行が安全。
- **対応状況 (2026-08-21):** `rescaledTo` を半分離れ丸め(round-half-away-from-zero)に変更し、極端な大きさではdouble丸めへフォールバック。`operator+/−` は約分→checkedMul/checkedAdd/checkedNegate(移植可能なオーバーフロー検出)→失敗時 `fromSeconds` 経由のdouble合算、の順に修正。テストは `FrameTimeTest.cpp`(丸め・クロススケール・巨大値安全性)に追加。既存約90呼び出しのうち同一スケール変換は挙動不変、クロススケールは境界±1フレームがより正確な方向へ変化。

## 2026-08-21 — 色域語彙の三重化(Gamut/SurfaceColorPrimaries/ColorSpace)とFloatColor::toLinearのsRGB固定（未検証）

- **関連:** `ArtifactCore/include/Color/ColorGamutConversion.ixx`(Gamut enum+行列)、`ArtifactCore/include/Graphics/SurfaceColorContract.ixx`(SurfaceColorPrimaries)、`ArtifactCore/include/Color/ColorSpace.ixx`(ColorSpace enum)、`ArtifactCore/src/Color/FloatColor.cppm`(toLinear/fromLinear)
- **事実:** 同じ「色域」概念に3つのenum(Gamut/SurfaceColorPrimaries/ColorSpace)が存在。ガムット変換行列はGamut側にのみ実装。FloatColor::toLinear/fromLinearはsRGBハードコードで、17種TransferFunctionを持つColorTransferFunction::encode/decodeとは接続されていない(TaggedColor::toTransferが正規経路として昨日追加)。LabColor/XYZColorはPImplヒープでCore内部のみ使用。FloatColorは直接メンバ16バイトで軽量(FloatRGBA統合の障害は低い)。
- **仮説（未検証）:** Gamut↔SurfaceColorPrimariesのマップ関数を追加しTaggedColorにgamut変換を提供するのが正道。FloatColor::toLinearは[[deprecated]]化してTaggedColorへ誘導。ColorSpace enumはColor.ColorSpace利用者との互換確認後にGamutへ統合。
- **対応状況 (2026-08-21):** `gamutForPrimaries()` / `primariesForGamut()` をTaggedColor.ixxに追加(SRGB_Rec709/DisplayP3/Rec2020/ACES_AP0-1のみ対応。DCI-P3/AdobeRGB/DWG/XYZは対応なしを明示)。`TaggedColor::toPrimaries()` が線形化→Bradford白色点適応済みの `ColorGamutConversion::convert`→再エンコードでgamut変換を提供(transfer維持、unknown transferは無変換通過)。`FloatColor::toLinear/fromLinear` は呼び出しゼロを確認の上 `[[deprecated]]` 化し、実装を `ColorTransferFunction` に委譲して数学を単一源へ統一。テストは `ColorBridgeTest.cpp`(語彙マップ・Rec709↔Rec2020往復1e-4・no-op/通過ケース)に追加。

## 2026-08-21 — Color系のFloatColor/FloatRGBA重複とQColor境界変換の散在（未検証）

- **関連:** `ArtifactCore/include/Color/FloatColor.ixx`、`ArtifactCore/include/Color/FloatRGBA.ixx`、`ArtifactCore/include/Color/ColorConversion.ixx`(HSVColor/HSLColor)、`Artifact/src/Layer/*`(toQColor/toFloatRGBA/colorFromJsonValue等86箇所)
- **事実:** FloatColor(PImpl・Artifact側1425箇所)とFloatRGBA(constexpr・75/112箇所)がほぼ同一のfloat RGBA型として重複。QColor↔float色変換とJSON直列化(colorToJson/colorFromJson系)がレイヤー・エフェクト各ファイルで局所再実装されており、NLE marker色対応でも同様のローカル実装を追加した。NamedColor enum(FloatColor.ixx)は使用箇所ゼロ、FloatColor.ixxの前方宣言 `class HSV;` は未定義でデッド。LabColor/XYZColorはCore内部のみ。色値型に色空間タグはなく、sRGB↔linearのtoLinear/fromLinearのみ。画像側は ImageF32x4_RGBA::colorDescriptor が primaries/transfer/alphaMode/range を保持し値側と分離。
- **対応状況 (2026-08-21):** `ArtifactCore/include/Color/ColorBridge.ixx`(module `Color.Bridge`)にQColor/JSON/hex境界を一元化(toQColor/toFloatColor/toFloatRGBA/colorToJson/floatColorFromJson/floatRgbaFromJson/colorToHexArgb)。JSONは `{"r","g","b","a"}` とhex文字列を受け付け、不正入力はfallback返し。`include/Color/TaggedColor.ixx`(module `Color.Tagged`)に色空間タグ付き値型を追加(SurfaceColorPrimaries/TransferFunction/SurfaceAlphaModeをSurfaceColorDescriptorと同一語彙で保持、toTransfer/premultiplied/straight/surfaceDescriptor)。gamut変換は未実装(次段階)。既存86箇所のローカル変換の一括置換は未実施(ビルド検証後に段階移行)。unit test `tests/ArtifactCore/ColorBridgeTest.cpp` 新設。
- **価値・懸念:** 境界変換の一元化で直列化の微妙な不一致(丸め・alpha既定・16進形式)が解消される一方、FloatRGBA統合はテンプレート/constexpr利用箇所の書き換えが必要で影響大。

## 2026-08-21 — Frame/Time系クラスの断片化とtimecode三重実装（未検証）

- **関連:** `ArtifactCore/include/Frame/*`（FramePosition/FrameRange/FrameRate/FrameOffset/FrameTime）、`ArtifactCore/include/Time/*`（RationalTime/TimeCode/TimeRemap）、`ArtifactCore/include/NLE/Core.ixx`（TimeBase）
- **事実:** 時間表現が6系統存在し相互変換APIが非対称(FramePosition→RationalTimeなし、FrameOffset→はあり)。FrameRateはfloat保持のみで30000/1001を表現できず、`hasDropframe()` は23.976を検出しない。timecode生成/解析が `FrameRange::toTimecode`(ノンドロップのみ)/`TimeCode`(drop対応だがtoStringが';'を出力せずsetFromQStringが';'をパースできない)/`NLE::TimeBase`(round-trip可能)の3重実装で挙動不一致。FrameTimeとFramePositionはほぼ重複しFrameTimeはArtifact側で62箇所現役使用。Frame/Time数学のunit testは存在しない。
- **対応状況 (2026-08-21):** FrameRateに有理数保持(`fromRational/setRationalRate/numerator/denominator/hasExactRational/exactFps`)を追加し、分数文字列・JSONでexact維持。`hasDropframe()` は23.976/47.952も検出。TimeCodeのtoString/toStdStringがdrop時に';'を出力し、setFromQStringが';'をパースするよう修正。`FrameRange::toTimecode` はTimeCodeに委譲しdrop対応。`FramePosition↔RationalTime` 変換と `qHash(FramePosition)` を追加。unit test `tests/ArtifactCore/FrameTimeTest.cpp` を新設。未着手: TimeBase timecodeのTimeCode集約(strict validation維持のため現状分離)、FrameTime統合。
- **仮説（未検証）:** FrameTime統合は使用箇所が多く別段階で機械的に行う必要がある。
- **価値・懸念:** 放置するとUI表示のタイムコードが経路ごとに食い違い、NLE統合時にレート変換誤差がクリップ位置ずれとして表面化する。一方FrameRateの内部変更は全レート比較コードに波及するため段階的導入が必要。
- **次の確認:** FrameRate有理化→timecode集約→FramePosition/RationalTime変換→unit test新設の順で段階実装するかを決める。

## 2026-08-21 — 2Dリグ利用導線の3箇所の断絶（skinMesh生成・ボーン追加・キーフレーム）（未検証の改善案）

- **関連:** `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`（handleCreateRig L2414）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（RigSelect/RigWeight 入力 L22579 / ウェイト操作 L17620 / オーバーレイ L38485）、`Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`（skinMesh JSON L320）、`ArtifactCore/include/Rig/Rig2D.ixx`（createSkinMesh L803）
- **事実:** リグ導線は実装済み（Layer→リグレイヤー作成→RigSelect自動切替、Ctrl+Tabでモード切替、ボーン回転ドラッグ＋Undo、Bでウェイト、N/S/M、ポーズクリップボード/スロット）。一方コード検索で (1) `Rig2D::createSkinMesh()` / `setSkinMesh()` の呼び出し元がアプリ側にゼロ（skinMesh は JSON 読込経路でしか存在しない）ため RigWeight・正規化/スムーズ/ミラー・スキン変形描画はユーザーが到達不能なデッド導線、(2) `addRigBone` / `addRigPoint` の呼び出し元は handleCreateRig のみで、VP・右クリック・パネルのいずれにもボーン追加/削除/リネーム導線が無い（RigHierarchyPanel は未実装、VP の HIERARCHY は読み取り専用 HUD）、(3) `Bone2D::evaluate(time)` とキーフレーム API は Core 実装済みだが、ボーンドラッグは静的 localTransform 編集のみでキー追加導線が無く、タイムラインにボーントラック非表示。
- **仮説（未検証）:** P0 は「画像レイヤー→SkinMesh 生成＋autoBind」の導線追加（spec SPEC_RIG_SYSTEM_UI_TASKS §8 相当）。これがないとリグ機能はユーザーから見て何も変形しない。次点で VP ダブルクリック/右クリックでの子ボーン追加、auto-key または手動キー追加＋タイムライン統合。Ctrl+Tab は ShortcutBindings 経由でないハードコードであり、プロジェクトのショートカット整合ルール（AGENTS.md）からも登録先の明示が望ましい。
- **価値・懸念:** Core の評価・Undo・オーバーレイは完成度が高い半分、UI 導線の断絶により機能価値がユーザーに伝わらない。リグレイヤーは solid ベース（opacity 0.18 の平面が合成に残る）で、spec が意図した画像バインド型と食い違う。
- **次の確認:** スキンメッシュ生成導線の設計（対象レイヤー選択 UI、メッシュ解像度、autoBind のボーン距離閾値）、リグレイヤーとソース画像レイヤーの関係（同一レイヤーか参照か）、ボーンキーの auto-key 有無の設計レビュー。

## 2026-08-21 — Text Animator のキーフレーム可視化と Undo 漏れ（未検証）

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm`（`addAnimator` L2212 / `removeAnimator` L2220 / `updateGlyphEvaluation` L4577）、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`（`isTimelineHiddenLayerPropertyGroup` L166）、`Artifact/src/Widgets/Timeline/*`
- **事実:** Text Animator の全プロパティ（Range start/end/offset、position/scale/opacity/tracking/fillColor等21項目）と Source Text キーフレームはキーフレーム化・時間評価・JSON round-trip まで実装済み。一方、(1) `isTimelineHiddenLayerPropertyGroup` が Transform 以外のグループ行を全て隠すため animator のキーがタイムラインに表示されず、用意済みの `displayLabelForPropertyPath` ラベルが標準経路で到達不能、(2) `addAnimator` / `removeAnimator` / Inspector の animator 値編集 / ◆トグル / タイムライン「Add/Remove Keyframe」に Undo push がなく、削除時はキーフレームごと失われる、(3) gpu-glyph パスの早期リターンは `rasterize==true` 条件のため、GPU描画では毎フレーム全再シェーピング＋animator評価＋不要な `animatedGlyphBounds`（per-glyph QPainterPath）が走る。surface cache キーは `animatorCount()>0` で `|frame=N` が付き enabled 依存なし。
- **対応状況 (2026-08-21):** (2) は `SetTextAnimatorStackCommand`（animatorスタック全体のJSON snapshot/restore）と ◆トグル・タイムラインキー操作の `SetLayerPropertyKeyframesCommand` push で解消済み。auto-key連打のUndo化は履歴氾濫のため未実施。(3) はアニメーター無し・Source Text静的な場合にGPUパスでもglyph評価をキャッシュする保守版を適用済み（`applyAnimatorStack` は空スタックで即returnするためフィールド影響なしを確認）。有効animator＋静的値スタックのキャッシュ（Fix B）は、field×transformの時間依存とenvelope検出コストの分析が必要なため未実施。
- **仮説（未検証）:** (2) のUndo対応がデータロス防止として最優先。(1) はAGENTS.mdの「左ペイン標準グループはTransformのみ」ルールと衝突するため、mask/matte型の専用行としての露出を設計レビューで決める必要がある。(3) はGPU glyph評価のキャッシュ（フレームキー＋source/styleキー）で圧縮可能。
- **価値・懸念:** AEユーザーの基本ワークフロー（テキスト→animator→キー打ち→タイムライン調整）のうち、タイムライン調整の導線が事実上欠落している。削除のUndo不可は事故につながる。
- **次の確認:** animator操作のUndo command化、タイムライン専用行の設計判断、glyph評価キャッシュの効果測定の順に扱う。

## 2026-08-21 — GPU blend pipeline のレイヤー毎全画面パスは実測後に融合を判断する（未検証）

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`prepareGpuLayerForBlend` L11572、`blendGpuLayerIntoAccum` L11889、`drawGpuLayerToIntermediate` 内 `renderer_->flush()` L11394、`finalizeGpuRenderToViewport` L12002）、`Artifact/src/Render/GPUTextureCacheManager.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`
- **事実:** 非Normalブレンド・3D・SSGI・マルチチャネル構成のGPUパイプラインは、レイヤー毎に全画面 `convertLayerToFloat`（compute）＋ `blendLayers`（compute、ping-pong）＋ `ctx->Flush()` を発行し、Nレイヤーで 2N+1 回の全画面パスになる。3DレイヤーはAOV分で最大6回 `layer->draw()`。ゲート条件は L32150-32215。Normalブレンドのみの2D構成はフォールバック直描画で対象外。`beginFrameGpuProfiling` / `ProfileScope` / `RenderPerformanceMonitor` が実装済みで計測可能。
- **事実（2026-08-21修正済み）:** `GPUTextureCacheManager::acquireOrCreate` はキャッシュ照会前にフルイメージ変換（QImage→RGBA8888、F32→Rgba32LinearStraight）しており、3Dカード・深度パスの毎フレーム無条件呼び出し（コントローラ L8622, L8699）と合わさってヒット時も全画像変換が発生していた。cache-first peek（`tryAcquireExistingLocked`）とpending早期リターンで解消。`PrimitiveRenderer2D` のスプライトキャッシュpruneもドロー数基準（60ドロー）だったため、高ドロー構成で恒久再アップロードの恐れがあり、pruneサイクル基準へ変更済み。両修正のruntime効果は未計測。
- **仮説（未検証）:** convert+blendのcompute融合、flushのフレーム末尾集約（UAV barrier置換）でレイヤー数に比例するGPUパス時間を圧縮できる。ただし依存関係（レイヤー間のaccum直列化）とDiligentのバリア意味論を要確認で、AGENTS.mdのシビアなコード扱い。
- **価値・懸念:** レイヤー数の多い3D/エフェクト構成でのプレビューfpsとRender Queue時間に直結する。一方、計測前に触ると表示品質リスクが高く、既存profilerでの実測が先。
- **次の確認:** (1) 修正済みcache-first経路の効果を、3Dカードを含む構成でGPUタイム・フレーム時間を実測比較。(2) レイヤー数10/30/60の非Normal構成で `layerToFloatConvertCount` / `blendDispatchCount` とフレーム時間の相関を取得。(3) 融合の要否と安全な導入順（flush集約から先行）を判断する。

## 2026-08-20 — 自動トランジション挿入は再生ヘッド操作の反復ではなく、候補計画の一括適用にする（未検証）

- **関連:** `ArtifactPr/include/ArtifactPrEditorEngine.ixx`、`ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`ArtifactPr/src/ArtifactPrMainWindow.cppm`、`plans/transition-effects-expansion-2026-07-09.md`
- **事実:** `EditorEngine::addTransitionAtPlayhead()` は選択クリップと現在の再生ヘッド位置から隣接クリップを解決して 1 件を追加する。映像トランジションの追加・削除・長さ変更は `TransitionStateCommand` により Undo/Redo の対象となる。既存の `addTransition()` は重複、隣接性、ハンドル長を検証しない。
- **仮説（未検証）:** 複数の編集点に対して同 API を繰り返すと、再生ヘッドおよび選択状態に依存して対象がずれ、部分失敗時に一括 Undo できない。候補を先に固定してから、重複・ロック・隣接性・最小ハンドルを検証し、1 個の state command で適用する bulk operation が安全である。
- **価値・懸念:** 制作時の「選択範囲に既定クロスフェード」を高速化しつつ、既存トランジションの二重配置と意図しない編集を防げる。一方、映像の実際の source handle と render 経路の接続は別課題であり、動画解析や AI 選択を初期範囲に含めるべきではない。
- **次の確認:** `Auto Transition Plan`（候補、採用、スキップ理由、既定長）を純粋な計算として定義し、選択トラック／選択範囲／マーカー範囲の3入力で候補が安定すること、bulk apply の Undo/Redo・保存／再読込・重複回避を確認する。

## 2026-08-15 — Two-panel native dock MVP の境界（未検証）

- **関連:** `Artifact/include/Widgets/ArtifactNativeDockSurface.ixx`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`、`docs/planned/MILESTONE_INDEPENDENT_DOCK_MANAGER_2026-08-13.md`
- **事実:** `ARTIFACT_NATIVE_DOCK_MVP=1` の opt-in 経路では、Composition Viewer と Inspector を `NativeDockSurface` に登録し、表示、activate、pinned、tab、portable layout の経路を native surface 側へ渡している。通常起動時の QADS 経路は維持している。
- **仮説（未検証）:** 現在の native surface は QADS の central dock 内にホストされるため、MVP は native panel の内部挙動と保存契約を検証できるが、top-level の splitter、floating、drag/drop を含む QADS 置換性までは証明しない。
- **価値・懸念:** 実運用リスクを限定したまま backend-neutral API を検証できる一方、native backend を QADS の代替と表現しすぎると検証範囲を誤認する。MVP の runtime 確認では「QADS 内に埋め込まれた native surface」と「独立 workspace backend」を分けて評価する必要がある。
- **次の確認:** runtime で2面の resize、tab、visibility、portable save/restore を確認した後、native surface を QADS manager の外側へ昇格できる root layout seam を設計する。

## 2026-08-15 — ドック追加メニューは registry facade の UX 層に限定する（未検証）

- **関連:** `docs/planned/MILESTONE_DOCK_PANEL_ADD_MENU_2026-08-15.md`、`ArtifactWorkspaceWidget`、DockManager / dock registry
- **事実:** トップレベル widget architecture は workspace / DockManager をレイアウト所有者として整理中で、各パネルは個別の責務を持つ。
- **仮説（未検証）:** 追加メニューを個別 widget の一覧管理にせず、安定した panel ID を持つ registry facade の薄い UX 層として実装すると、重複 dock、表示名依存、パネル責務の混線を避けやすい。
- **価値・懸念:** 最近使用・お気に入り・再表示を追加しても、Components / Effects / Properties などの専用面を汎用 inspector に戻さずに済む。現行 registry API と保存境界が十分かは未検証。
- **次の確認:** 現行の dock 登録・生成・activate・save/restore 経路を一覧化し、既存 API で Phase 1 の契約を満たせるか確認する。

## 2026-08-14 — 静止画・連番画像の受入ギャップ棚卸し（未検証）

- **関連:** `docs/analysis/STILL_IMAGE_LAYER_ACCEPTANCE_MATRIX_2026-08-08.md`、`docs/planned/MILESTONE_STILL_IMAGE_LAYER_PRODUCTION_READINESS_2026-08-08.md`、`docs/planned/MILESTONE_IMAGE_SEQUENCE_WORKFLOW_COMPLETION_2026-07-27.md`、`ArtifactImageLayer`、`ImageSequenceSource`
- **事実:** 静止画は OIIO header preflight、非同期 float decode、入力色解釈、GPU cache、JSON 保存／復元、crop を含む `toQImage()` 境界まで静的実装済みと整理されている。一方、受入マトリクスの IMG-01〜14 と OP-01〜10 は、ほぼすべて実素材・runtime 未確認である。
- **事実:** 連番は Asset Browser の単一素材表示、展開、欠番／読込失敗／relink 診断、Composition 投入時の関係保存、bounded cache、時刻依存の frame switching が実装済みと整理されている。残りは保存／再読込、欠番、範囲外、cache hit/miss、実機性能の検証である。
- **仮説（未検証）:** 次の価値が最も高い作業は新規機能追加ではなく、同一の最小受入素材セットを使って静止画と連番の Preview／Software Preview／Render Queue を比較し、失敗段階を受入表へ反映すること。ここで差異が出れば、source／color／cache／composite のどの境界を直すべきかを限定できる。
- **価値・懸念:** 静的実装済みと制作利用可能を混同せず、動画対応や低レベル backend へ広げる前に、現在の優先対象である静止画・連番画像の品質を測定できる。ビルド・テスト・runtime 検証はユーザー許可が必要なため未実施。
- **次の確認:** 8-bit sRGB、alpha付き、16-bit／float、grayscale、missing／corrupt の静止画素材に加え、正常連番、欠番、範囲外、異解像度、差し替え連番を用意し、(1) frame advance、(2) stale frame 非表示、(3) bounded cache、(4) 保存／再読込、(5) Preview／Render Queue の一致を順に確認する。

## 2026-08-13 — Point2D キーフレームの JSON 復元型（未検証）

- **関連:** `ArtifactCore/include/Property/PropertySerializationBridge.ixx`、`PropertyType::Point2D`
- **事実:** `Point2D` の通常値は JSON object から `QPointF` へ明示復元される一方、キーフレーム値は汎用の `QJsonValue::toVariant()` を通り、object の場合は map 系の QVariant になる。今回確認した Color にはキーフレーム専用の型復元を追加したが、Point2D は依頼範囲外のため変更していない。
- **仮説（未検証）:** Point2D キーフレームを保存・再読込すると、補間側が期待する `QPointF` へ変換できず、値が欠落または既定値化する可能性がある。
- **価値・懸念:** 汎用 Property の位置系アニメーションの保存互換性に影響し得る。既存ファイル形式との互換性を保った局所復元が必要。
- **次の確認:** Point2D のキーフレームを含む最小 round-trip を許可されたテストで確認し、再現時は Color と同様に型別復元を追加する。

## 2026-08-13 — focused pack の module 名検査

- **関連:** `scripts/check_source_manifests.py`、`Artifact/CMakeLists.txt`
- **事実:** 既存 checker は focused pack の path 重複、件数一致、source の存在だけを検証していた。
- **対応:** focused pack ごとに interface の `export module` と implementation の `module` 名を読み取り、pack 間の module 名衝突と interface/implementation の不一致を報告する検査を追加した。
- **価値・懸念:** 異なるファイルに同じ module 名を割り当てる事故を、CMake configure 前の静的検査で検出できる。既存の全モジュールを対象にせず、複数 implementation unit が正当な既存モジュールへ過剰適用しない。
- **次の確認:** 新しい focused effect pack を追加する際に checker を実行し、module 名と source ownership を同時に確認する。

## 2026-08-13 — focused pack target wiring の検査

- **関連:** `scripts/check_source_manifests.py`、`Artifact/CMakeLists.txt`
- **事実:** source set の存在だけでは、対応する CMake target が作られ、両 source list が `target_sources` に登録されていることまでは保証できない。
- **対応:** `ArtifactEffectsColor` を含む全 focused pack を検査対象に戻し、target 名（`SurfaceFX` の大文字略称を含む）、`add_library(... STATIC)`、`target_sources` と module/implementation set の参照を検証するようにした。
- **価値・懸念:** source ownership と target wiring の片側だけが更新される分割漏れを configure 前に検出できる。互換 umbrella（Spatial/Rasterizer/Residual）は focused pack の target wiring 検査から除外している。
- **次の確認:** CMake configure 時に target graph と module BMI 参照が静的 checker の想定どおり解決することを確認する。

## 2026-08-13 — legacy RadialBlur の residual 漏れ

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactEffectsRasterizer`、`ArtifactEffectsResidual`
- **事実:** legacy Rasterizer-path の `RadialBlur` は Rasterizer umbrella の source list から除外されていたが、residual source list の明示除去には含まれていなかった。
- **対応:** legacy `RadialBlurEffect.ixx/.cppm` を residual の module / implementation 除去リストにも追加した。
- **価値・懸念:** 旧 RadialBlur が canonical な `ArtifactEffectsFinishing` と residual で二重コンパイルされる経路を閉じた。CMake configure / build は未実施。
- **次の確認:** residual の静的評価で module / implementation が空になり、focused pack と重複しないことを確認する。

## 2026-08-13 — focused pack の link 到達性検査

- **関連:** `scripts/check_source_manifests.py`、`Artifact/CMakeLists.txt`
- **事実:** 各 focused pack target が定義されていても、`Artifact` または互換 umbrella から link graph 上で到達できることは別の条件である。
- **対応:** `target_link_libraries` を静的に収集し、`Artifact` を起点に全 focused pack target を辿れるか checker で検証するようにした。
- **価値・懸念:** source が target に登録されているだけで実行ファイルへ伝播しない wiring 漏れを検出できる。実際の CMake target 解決・link order は configure / build 未検証。
- **次の確認:** ビルド許可後に CMake configure と link で、静的 graph と実 target graph の一致を確認する。

## 2026-08-13 — compatibility umbrella の集約検査

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactEffectsSpatial`、`ArtifactEffectsRasterizer`、`ArtifactEffectsResidual`
- **事実:** focused pack target の到達性だけでは、旧 umbrella 名が意図した pack 群をすべて伝播させることまでは保証できない。
- **対応:** checker に Spatial 11 pack、Rasterizer 8 pack、Residual 全 22 packの期待リンク集合を追加し、umbrella からの欠落を報告するようにした。
- **価値・懸念:** 互換 target の更新漏れによる機能欠落を静的に検出できる。期待集合は現行の責務分割を固定するため、将来の再分類時は同時更新が必要。
- **次の確認:** CMake configure 後に実際の target link interface と静的期待集合を照合する。

## 2026-08-13 — base effect source の ownership 検査

- **関連:** `Artifact/CMakeLists.txt`、`scripts/check_source_manifests.py`
- **事実:** focused pack と umbrella の検査だけでは、元の `ARTIFACT_EFFECTS_MODULES/IMPL` に残った source がどこかの target から除去・移管されたことまでは保証できない。
- **対応:** base effect source と `list(REMOVE_ITEM ...)` の変数展開を静的に追跡し、focused/residual ownership に入らない source を報告する検査を追加した。1 行形式と複数行形式の CMake list の両方に対応した。
- **価値・懸念:** source が無所属になって静かにビルド対象から消える回帰を検出できる。CMake の完全な評価器ではないため、configure / build による最終確認は必要。
- **次の確認:** 新規 effect source 追加時に checker が未移管 source を報告することを確認する。

## 2026-08-13 — app-side effect source の二重所有検査

- **関連:** `Artifact/CMakeLists.txt`、`APP_MODULES` / `APP_IMPL`、`ARTIFACT_EFFECTS_MODULES` / `ARTIFACT_EFFECTS_IMPL`
- **事実:** focused pack source が base effect list に存在しない場合、explicit app manifest 側に残って app target と focused target の二重所有になる可能性がある。
- **対応:** focused pack の全 source が base effect list に所属することと、base list が `APP_MODULES/APP_IMPL` から除去されることを checker で検証するようにした。
- **価値・懸念:** pack 分割後の二重コンパイル・BMI重複の回帰を早期検出できる。CMake の実評価や MSVC module scan は未検証。
- **次の確認:** source 追加・pack移動時に checker が app-side 除去漏れを検出することを確認する。

## 2026-08-13 — focused pack の共通直接依存検査

- **関連:** `Artifact/CMakeLists.txt`、22 focused effect target
- **事実:** source と umbrella の wiring が正しくても、pack target 自身の共通 link dependency が欠けると依存が別 target 経由の偶然に委ねられる。
- **対応:** 全 focused pack target が `ArtifactCore`、`ArtifactRender`、`ArtifactEffectContract` を直接 link しているか checker で検証するようにした。
- **価値・懸念:** pack 単体の再利用性と依存宣言の明示性を保ち、umbrella 経由だけで成立する不安定な link graph を検出できる。個別 effect の追加依存までは自動推論していない。
- **次の確認:** CMake configure / link 後に各 pack の実際の usage requirement と static checker の共通依存が一致することを確認する。

## 2026-08-13 — focused target の C++ module file set 検査

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactEffects*` focused targets
- **事実:** `target_sources` が source 変数を参照していても、module interface を `PUBLIC FILE_SET CXX_MODULES` として登録していなければ BMI / module dependency graph に入らない。
- **対応:** 全 focused target に private implementation section と public C++ module file set が存在することを checker で検証するようにした。
- **価値・懸念:** source list の参照だけでは不十分な CMake target wiring を検出できる。実際の CMake file-set 解決は configure / build 未検証。
- **次の確認:** CMake configure 後に生成された module dependency graph と各 target の file set を確認する。

## 2026-08-13 — ArtifactCore pack wiring の親側検査

- **関連:** `ArtifactCore/CMakeLists.txt`、`scripts/check_source_manifests.py`
- **事実:** ArtifactCore には 52 個の `ARTIFACTCORE_*_MODULES/IMPL` pack variable と対応する static target がある。子リポジトリの source は今回変更していない。
- **対応:** 親リポジトリの checker から ArtifactCore の pack variable を読み取り、対応 target、`target_sources` の variable 参照、source path の存在を検証するようにした。`AI` / `IPC` / `NLE` / `VST` / `VST3` / `ColorCollection` / `FileSystem` の target 名例外も扱う。
- **価値・懸念:** Artifact 側だけが正しく分割されても Core pack の target wiring が崩れると全体が壊れるため、親の source check で早期検出できる。CMake configure / build は未実施。
- **次の確認:** configure 後に ArtifactCore の実 target graph、module BMI、親 Artifact の transitive link を確認する。

## 2026-08-13 — ArtifactCore Acoustic / Platform の親 link 漏れ

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactCore/CMakeLists.txt`、`ArtifactCoreAcoustic`、`ArtifactCorePlatform`
- **事実:** Core pack target と source set は存在していたが、親 `Artifact` の link graph を静的に辿ると `ArtifactCoreAcoustic` と `ArtifactCorePlatform` だけが未到達だった。
- **対応:** 親 `Artifact` の内部ライブラリ link に両 target を追加し、親・子 CMake を合成した checker で全 52 Core pack target の到達性を検証するようにした。
- **価値・懸念:** Acoustic / Platform module が target 定義だけ存在して実行ファイルへ伝播しない状態を解消した。CMake configure / build による実際の transitive link と module BMI 解決は未検証。
- **次の確認:** configure 後に両 target の link interface と Artifact の最終 link line を確認する。

## 2026-08-13 — ArtifactCore pack の基盤依存検査

- **関連:** `ArtifactCore/CMakeLists.txt`、52 Core pack targets
- **事実:** Core pack が親から到達できても、pack 自身が `ArtifactCore` を直接 link していない場合は、依存が別 target の transitive link に依存する。
- **対応:** 全 Core pack target の `target_sources` に public C++ module file set があり、`ArtifactCore` を直接 link していることを checker で検証するようにした。
- **価値・懸念:** Core pack を単独で再利用できる依存契約を保ち、link 順依存の回帰を検出できる。個別外部ライブラリ依存の完全な推論は行っていない。
- **次の確認:** configure / link 後に各 Core pack の実 usage requirements と static checker の依存契約を確認する。

## 2026-08-13 — 合成 target link graph の循環検査

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactCore/CMakeLists.txt`、`scripts/check_source_manifests.py`
- **事実:** 親 Artifact と ArtifactCore の target link graph は静的評価で 92 nodes / 323 edges、循環 0 件だった。
- **対応:** 親・子 CMake の link edge を合成し、checker に循環検出を追加した。
- **価値・懸念:** pack 分割後に target 相互依存が発生し、link order や module dependency 解決を不安定にする回帰を検出できる。CMake の実 target graph は未生成。
- **次の確認:** configure 後の実 target graph と static graph の循環判定が一致することを確認する。

## 2026-08-13 — ArtifactCore module 重複検査の分類

- **関連:** `ArtifactCore/CMakeLists.txt`、`scripts/check_source_manifests.py`
- **事実:** ArtifactCore の `.ixx` と `.cppm` は同じ named module を正当に共有するため、interface と implementation を一つの重複集合として扱えない。
- **対応:** checker の module 名検査を interface / implementation 別に分離した。実測では両分類とも pack 間の重複は 0 件。
- **価値・懸念:** 正常な interface / implementation 対を誤検出せず、同じ分類内の二重定義だけを検出できる。
- **次の確認:** 新しい Core pack 追加時に同一分類の module 名衝突が checker で検出されることを確認する。

## 2026-08-13 — ArtifactCoreLocalization の cross-pack module reference

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Localization/LocaleFormatting.ixx`、`ArtifactCore/include/Utils/Localization.ixx`、`ArtifactCore/src/Localization/Localization.cppm`
- **事実:** `ArtifactCoreLocalization` は `LocaleFormatting.ixx`（`Localization.LocaleFormatting`）を module interface として登録する一方、implementation set は `Localization.cppm`（`Core.Localization`）を登録している。`Core.Localization` の interface `Utils/Localization.ixx` は `ARTIFACTCORE_MODULES` 側に残っている。
- **事実の補強:** `ArtifactCoreModuleReferences.cmake` に `Localization.cppm|Core.Localization|include/Utils/Localization.ixx` が明示登録され、CMake は implementation に `/reference` と interface object dependency を付与する設計になっている。`TranslationManager.cppm` と `AppMain.cppm` は `Core.Localization` を import する。
- **価値・懸念:** これは通常の同一 pack interface/implementation 対ではなく、base target の interface BMIを分割 implementation targetから参照する特殊経路である。明示 reference が実 configure / MSVC module generation で正しく解決するかは未検証で、現時点で確定バグとは断定しない。
- **次に必要:** CMake configure / build 許可後に `ArtifactCoreLocalization` の `/reference`、interface object dependency、親 Artifact の module BMI 解決を確認する。失敗時のみ pack 境界の再整理を検討する。

## 2026-08-13 — configure-time source scan の残存確認

- **関連:** root `CMakeLists.txt`、`Artifact/CMakeLists.txt`、`ArtifactCore/CMakeLists.txt`
- **事実:** source tree の `.ixx/.cppm/.cpp` を列挙する `GLOB_RECURSE` は残っていない。残存する GLOB は MSVC `modules.json` / Windows SDK の toolchain discovery と Artifact icon resource discovery に限定されている。
- **価値・懸念:** explicit source manifest 化による configure-time source scan 削減の方針は維持されている。SDK/toolchain discovery は環境依存のため別途 configure 検証が必要。
- **次の確認:** configure 後に source manifest が実際の target source と一致し、resource/toolchain discoveryだけが動作することを確認する。

## 2026-08-13 — ArtifactCore explicit module reference の stale entry

- **関連:** `ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`、`ArtifactCore/src/AI/CloudAgent.cppm`、`ArtifactCore/src/ImageProcessing/NoiseImageGenerator.cppm`
- **事実:** reference table の `CloudAgent` entry は interface path が `ICloudAIAgent.ixx` で `include/` を欠き、`NoiseImageGenerator` entry は存在しない `Generator.ixx` を指定している。実際の primary interface は `include/AI/ICloudAIAgent.ixx` と `include/Channel/Generator.ixx` で、後者の implementation は `module Generator;` と宣言されている。
- **価値・懸念:** explicit `/reference` の path stale により、configure 後の interface object dependency が誤る可能性がある。reference table は子リポジトリ内のため、編集は明示承認待ち。
- **次に必要:** 承認後に2 entryを実在する interface pathへ修正し、reference table の全 entryで path / module declaration 検査を追加する。
- **切り分け:** Artifact 側には同形式の explicit module reference table は存在せず、この stale path 問題は現在 ArtifactCore 側に限定される。

## 2026-08-11 — Shared render device lease の段階移行

- **関連:** `Artifact/include/Render/DiligentDeviceManager.ixx`、`Artifact/src/Effects/`
- **事実:** shared render device は Diligent smart pointer と独立した手動 refCount を持つ。effect群には acquire/release の非対称な経路が残る。`SharedRenderDeviceLease` を導入し、`InvertEffect` の一時利用を移行した。
- **価値・懸念:** device loss と backend 切替時に共有deviceが解放されないリスクを減らす。永続resourceを持つ effect と一時利用を混ぜて機械移行してはならない。
- **次の確認:** effectごとの所有期間を分類し、leaseへ段階移行した後、shared refCountが0へ戻るruntimeケースを確認する。

## 2026-08-11 — ImageF32 GPU dirty 通知契約

- **関連:** `ArtifactCore/src/Image/ImageF32x4_With_Cache.cppm`
- **事実:** CPU/GPU同期は実装済み。外部GPU passがUAVへ書いた後の `MarkGpuDataDirty()` 呼び出し元は静的検索で0件だった。
- **価値・懸念:** 将来のGPU直書きでCPU readbackを省略すると、古いCPU画像をGPUへ再uploadする可能性がある。readbackは同期的なためhot pathへ増やさない。
- **次の確認:** UAV直書き導入時は同一スコープでdirty通知を必須にし、CPU読取りの頻度をruntime計測する。

## 2026-08-11 — 3D 描画の行列スコープとflush契約

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`
- **事実:** `PrimitiveRenderer3D` はflush時のカメラ行列でキューを送信する。Controller内の3Dレイヤー／Cardは局所RAIIスコープへ移行済みで、Overlay側は個別flushで保護している。
- **価値・懸念:** set/resetとflushの順序依存を減らす。RT/DSV・viewport復元は別責務であり、同一スコープへ安易に統合しない。
- **次の確認:** すべての3D matrix設定経路を静的監査し、複数カメラ・ライト・選択overlayの実機ケースを確認する。

## 2026-08-11 — Viewport shortcut context

- **関連:** `Artifact/src/Widgets/Render/`、`ArtifactCore/include/UI/ShortcutBindings.ixx`
- **事実:** AE式ツール切替とBlender式 `G/R/S` モーダル操作は競合する。
- **価値・懸念:** 単一キーの場当たり的追加を避け、Viewport focus・テキスト入力・専用ツールを区別する入力コンテキストが必要。
- **次の確認:** 変換セッションの状態機械と、`G/R/S`、`X/Y/Z`、確定／キャンセル操作の優先順位を設計する。

## 2026-08-11 — Layer-type property presentation migration

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、`Artifact/src/Layer/ArtifactImageLayer.cppm`
- **事実:** 標準profileは `Initial` / `Transform` のみを表示し、Imageレイヤー固有の `Image` / `Source Reframe` グループを除外していた。Imageと固定Planeを初回対象として明示profileへ追加し、表示順を `Transform` 優先にした。ImageのCrop / Panは未有効時にTransformの追加ボタンだけを表示し、有効化後に専用グループを挿入する。
- **価値・懸念:** 既存のComponents専用面を露出させず、型固有の主要項目を段階的に表示できる。profileはまだWidget側の暫定定義で、モデル側の契約へは未移行。
- **次の確認:** ImageとPlaneの編集・保存再読込を確認後、Text、Shape、Solidの順で同じ最小変更を行う。

## 2026-08-11 — Dock focus outline and current-tab indicator

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Dock/DockGlowStyle.cppm`
- **事実:** PropertiesとTimelineの白い外周はQADSではなく、各widgetに追加された `QFocusFrame` だったため撤去した。QADSの外周Glowを無効にすると、同じフラグで保護されていたcurrent-tab下線も描画されなかった。
- **価値・懸念:** Dock外周Glowなしでもcurrent-tab下線を残せる。QADSのstyle dispatchが `PE_Widget` 以外を通る環境での描画はruntime未確認。
- **次の確認:** Dock領域ごとのタブ切替、非フォーカスDock、floating/re-dock、およびDPI変更後に下線と枠が残らないことを確認する。

## 2026-08-11 — Numeric property focus selection

- **関連:** `Artifact/include/Widgets/ArtifactRelativeSpinBox.ixx`
- **事実:** 数値editorはQt標準SpinBoxの内部LineEditを使用し、focus/通常クリック時にsuffixを除く数値部分だけが自動選択される。共通relative spinboxで自動選択を解除した。
- **価値・懸念:** 値欄はcaret状態で開き、明示ドラッグ等の選択は維持する意図。Tabフォーカスから即時入力する既存操作のruntime挙動は未確認。
- **次の確認:** float/int/rotationのクリック、Tab移動、ドラッグ選択、suffix付き値、相対入力（`+` / `-`）を確認する。

## 2026-08-11 — Runtime verification backlog

- **関連:** GPU effect、Diligent binding、audio/FFmpeg、render job
- **事実:** 多数の防御修正はビルド・実機未確認で、履歴はアーカイブへ移設した。
- **価値・懸念:** 個別の全消化ではなく、device lifecycle、GPU effect、seek/EOS、render jobをまとめた代表回帰ケースが必要。
- **次の確認:** ビルド許可後に代表ケースを定義し、診断ログとともに実行する。

## 2026-08-12 — MultiChannelImage copyFrom後のチャンネル参照

- **関連:** `Artifact/src/Effect/ArtifactCreativeEffects.cppm`、`ArtifactCore/include/Image/MultiChannelImage.ixx`
- **事実:** `MultiChannelImage::copyFrom()` は内部channel mapをclearして再構築する。copyFrom前に取得した `SharedPtr<VideoChannel>` は旧チャンネルを保持し続けるため、処理結果を読む前に `getChannel()` で再取得する必要がある。Creative共通アダプタは再取得するよう修正した。
- **価値・懸念:** Core effectが正しく処理してもArtifact出力が元画像のままになる静かなバイパスを防ぐ。同じcopyFromパターンが別経路にある可能性は未検証。
- **次の確認:** `MultiChannelImage::copyFrom()` の全呼び出し元を監査し、copyFrom前のチャンネル参照を処理後に再利用していないことを確認する。

## 2026-08-12 — Residual Rasterizer effect pack boundary

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/src/Effects/Rasterizer/`
- **事実:** P1〜P3の分割後に残った35組のうち、Rasterizer配下の30組は、インターフェース上で共通契約以外の個別effect moduleを直接importしていなかった。Temporal pack対象は除外し、30組を`ArtifactEffectsRasterizer`へ分離した。
- **閃き・仮説:** ディレクトリ名だけでなく、履歴状態を持つTemporal群とstatelessなRasterizer operator群を別targetにすると、通常のラスター処理の変更が履歴系・色補正系のBMI再構築へ波及しにくくなる。
- **価値・懸念:** 最大の残存`ArtifactEffects` targetを35組から5組へ縮小できる。一方、静的ライブラリのobject pull-in、factoryのlink order、各effectの実装側依存はビルド未検証である。
- **次の確認:** ビルド許可後にCMake configureと代表的effect factoryを含むリンクを検証し、P4 packのBMI境界を確認する。

## 2026-08-12 — ArtifactCore Audio domain boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/cmake/ArtifactCoreSources.cmake`
- **事実:** Core source manifestにはAudio 43 module / 30 implementationがあり、Audio moduleを直接importする非Audio moduleは`Media.Encoder.FFmpegAudioDecoder`と`Particle.System`の2組だった。これらを含めて`ArtifactCoreAudio`へ移し、分割後のCore本体からAudio moduleへのimport edgeが0件になることを静的確認した。
- **閃き・仮説:** domainディレクトリ単位の移動だけでなく、直接importする少数のconsumerを同じpackへ閉じ込めると、base targetが抽出targetへ逆依存する循環を避けやすい。
- **価値・懸念:** Core本体の再コンパイル範囲をAudio変更から切り離せる可能性がある。一方、Qt Multimedia / FFmpeg、MSVC module reference、静的ライブラリのlink順は未検証である。
- **次の確認:** ビルド許可後にconfigure、Audio moduleのBMI生成、FFmpeg decoderとParticle systemを含むリンクを確認する。

## 2026-08-12 — ArtifactCore AI leaf boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`Artifact/src/AppMain.cppm`、`Artifact/src/Widgets/AIChatWidget.cppm`
- **事実:** AI系33 module / 6 implementationは、Core内の非AI moduleからの直接importが0件だった。`ArtifactCoreAI`を追加し、AI moduleを利用するArtifact本体へリンクした。
- **閃き・仮説:** optional backend（ONNX、llama、Python）を含むleaf domainを分離すると、AIコード変更やその依存探索を通常のCore targetのBMI再構築から切り離しやすい。
- **価値・懸念:** AI domainの変更範囲とoptional link依存を局所化できる。一方、実際のoptional backend構成、静的archiveのobject pull-in、MSVC module referenceは未検証である。
- **次の確認:** ビルド許可後にAI packのconfigure、optional backend有無ごとのリンク、AppMain / AIChatWidgetのmodule解決を確認する。

## 2026-08-12 — ArtifactCore Video boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`Artifact/src/Render/`、`Artifact/src/Layer/ArtifactNLETransitionBridge.cppm`
- **事実:** Video系31 module / 20 implementationとFFmpeg video decoderを`ArtifactCoreVideo`へ移した。Video.VideoFrameを直接利用するMedia 3組は、P8のMedia packへ整理した。分割後のCore本体からVideo packへのmodule import edgeは0件だった。
- **閃き・仮説:** frame型を利用するMedia decoder/controllerをVideo pack側へ閉じ込めることで、Video domainを単独targetとして成立させられる。
- **価値・懸念:** Video transition / decoder変更のBMI再構築範囲をCore本体から切り離せる可能性がある。FFmpeg link、Render targetのmodule reference、static archive解決は未検証。
- **次の確認:** ビルド許可後にVideo packのconfigure、FFmpeg video decoder、Render GPUTextureCacheManager、NLE transition bridgeのmodule/link解決を確認する。

## 2026-08-12 — ArtifactCore Media boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`
- **事実:** Media系16 module / 12 implementationを`ArtifactCoreMedia`へ移し、Media.Infoを直接利用する`Codec.FFmpegThumbnailExtractor`も同じpackへ含めた。MediaはVideo frameを利用するため、Media targetはVideo targetへ依存させた。
- **閃き・仮説:** Video frameを利用するMedia controller/decoderをVideo packへ混在させずMedia packへ戻すことで、Video（codec/transition）とMedia（source/playback）の責務境界を明確にできる。
- **価値・懸念:** source/asset/render側のMedia変更をCore本体から分離できる可能性がある。Media→Videoのmodule reference、FFmpeg thumbnail link、static archive順序は未検証。
- **次の確認:** ビルド許可後にMedia / VideoのBMI生成、thumbnail extractor、AssetBrowser、RenderQueueのlink解決を確認する。

## 2026-08-12 — ArtifactCore Composition leaf boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`Artifact/src/Composition/`、`Artifact/src/Layer/ArtifactCompositionLayer.cppm`
- **事実:** Composition系9 module / 8 implementationを`ArtifactCoreComposition`へ移した。Core内の非Composition moduleからComposition moduleへの逆importは0件で、Composition targetはCoreとMediaへ依存させた。
- **閃き・仮説:** Composition buffer / pre-compose / template契約は、基盤Coreから分離しても利用側へ一方向に提供できるleaf domainである。
- **価値・懸念:** project/composition機能の変更時にCore本体のBMI再構築を抑えられる可能性がある。保存形式、Artifact側の複数利用者、static archive link順は未検証。
- **次の確認:** ビルド許可後にComposition packのBMI生成、Project/Layer/RenderQueue利用者のmodule解決とlinkを確認する。

## 2026-08-12 — ArtifactCore Analyze boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Analyze/`、`ArtifactCore/include/Time/TimeRemap.ixx`
- **事実:** Analyze系5 module / 4 implementationに`Time.TimeRemap`を加え、`ArtifactCoreAnalyze`へ6 module / 5 implementationを移した。TimeRemapはAnalyze.OpticalFlowを直接利用するため同じpackへ閉じ込めた。
- **閃き・仮説:** optical-flowや画像解析と時間再マップは、再生・解釈側へ一方向に提供する分析packとして分離できる。
- **価値・懸念:** Analyze/TimeRemap変更時のCore本体BMI再構築を抑えられる可能性がある。FootageInterpretService、CurveEditor、SmartPalette利用側のlinkは未検証。
- **次の確認:** ビルド許可後にAnalyze packのmodule生成、OpticalFlow、TimeRemap、Artifact利用側のlink解決を確認する。

## 2026-08-13 — ArtifactCore Tracking boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Tracking/`、`ArtifactCore/src/Tracking/`
- **事実:** Tracking系3 module / 2 implementationを`ArtifactCoreTracking`へ移した。Core内の逆向き参照はなく、Transformへの一方向依存だけを持つ。
- **閃き・仮説:** motion / planar / camera trackingは、画像・レイヤー処理から独立した解析サービス境界としてCore本体から切り離せる。
- **価値・懸念:** Tracking変更時のCore本体BMI再構築を抑えられる可能性がある。現時点のArtifact側直接利用とstatic link順は未検証。
- **次の確認:** ビルド許可後にTracking packのmodule生成、OpenCV依存、Transform参照、Artifact利用側のlink解決を確認する。

## 2026-08-13 — ArtifactCore IPC boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/IPC/`、`ArtifactCore/src/IPC/`
- **事実:** IPC系3 module / 3 implementationを`ArtifactCoreIPC`へ移した。Core内の逆向き参照はなく、Image型を利用する共有メモリ・render-farm transportのpackとして閉じ込めた。
- **閃き・仮説:** IPC transportは画像処理・レンダリングの実装本体から分離し、必要な利用側だけが明示的にリンクする境界にできる。
- **価値・懸念:** IPC変更時のCore本体BMI再構築を抑えられる可能性がある。隠れたrender-farm利用者とstatic link順は未検証。
- **次の確認:** ビルド許可後にIPC packのmodule生成、Image参照、render-farm利用側のlink解決を確認する。

## 2026-08-13 — ArtifactCore NLE / Playback / Preview boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/NLE/`、`ArtifactCore/include/Playback/`、`ArtifactCore/include/Preview/`
- **事実:** NLE 2 module / 2 implementation、Playback 2 module / 1 implementation、Preview 2 module / 2 implementationを個別packへ移した。Video→NLE、Media→Playbackの一方向参照をtarget linkへ反映し、Previewは逆向き参照なしでArtifact本体へ明示リンクした。
- **閃き・仮説:** 編集形式、再生状態、プレビュー設定は、それぞれ利用側へ契約を提供するleaf domainとしてCore本体から切り離せる。
- **価値・懸念:** NLE / playback / preview変更時のCore本体BMI再構築を抑えられる可能性がある。OTIO、Media再生、Preview設定利用側のmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後に3 packのmodule生成、Video/Mediaの依存解決、Artifact本体のlink解決を確認する。

## 2026-08-13 — ArtifactCore Export / VST3 / Localization / Coordinate boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Export/`、`ArtifactCore/include/VST3/`、`ArtifactCore/include/Localization/`、`ArtifactCore/include/Coordinate/`
- **事実:** Export 3 module / 2 implementation、VST3 1/1、Localization 1/1、Coordinate 1/1を個別packへ移した。ExportはRig、CoordinateはSerializationへ依存し、Artifact側のLottie、VST host、Project Memo利用者には明示リンクを追加した。
- **閃き・仮説:** 形式出力、外部plugin ABI、表示ローカライズ、座標プロファイルは、Core本体へ常時伝播させず用途別packとして保持できる。
- **価値・懸念:** これらの変更時にCore本体のBMI再構築を抑えられる可能性がある。Lottie/VST3/Project Memo/coordinate利用側のmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後に4 packのmodule生成、外部依存、Artifact本体のlink解決を確認する。

## 2026-08-13 — ArtifactCore Event / File / Plugin / Control / Database boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Event/`、`ArtifactCore/include/File/`、`ArtifactCore/include/Plugin/`、`ArtifactCore/include/Control/`、`ArtifactCore/include/Database/`
- **事実:** Event 3/2、File 3/2、Plugin 3/2、Control 3/2、Database 2/1を個別packへ移した。UI/Playback→Event、Asset→Fileの依存をtarget linkへ反映し、Plugin/Control/DatabaseはCore内の逆向き参照なしでArtifact本体へ明示リンクした。
- **閃き・仮説:** event transport、file detection、plugin registry、external control、database storageは、Core本体の共通基盤から分離して必要な利用側だけへ公開できる。
- **価値・懸念:** 各境界の変更時にCore本体BMI再構築を抑えられる可能性がある。UI/Playback/Assetのlink順、外部control/backendの実装条件は未検証。
- **次の確認:** ビルド許可後に5 packのmodule生成、依存解決、Artifact本体のstatic linkを確認する。

## 2026-08-13 — ArtifactCore Mask / Configuration boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Mask/`、`ArtifactCore/include/Configuration/`、`ArtifactCore/include/Application/ArtifactAppSettings.ixx`
- **事実:** Mask 4/4を`ArtifactCoreMask`へ、Configuration 3/2とApplication.AppSettingsを`ArtifactCoreConfiguration`へ移した。UI→Mask、AI/Asset→Configurationの依存をtarget linkへ反映した。
- **閃き・仮説:** mask計算と設定／AppSettingsはCore本体へ混在させず、UI・AI・Assetなどの利用側へ一方向に提供できる。
- **価値・懸念:** mask・設定変更時のCore本体BMI再構築を抑えられる可能性がある。RotoMaskEditor、AI API key、Asset importer、Artifact側設定利用者のlink順は未検証。
- **次の確認:** ビルド許可後に2 packのmodule生成、Color/Grid依存、UI/AI/Asset/Artifactのlink解決を確認する。

## 2026-08-13 — ArtifactCore Text boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Font/`、`ArtifactCore/include/Text/`、`ArtifactCore/include/Shape/`
- **事実:** Font 3 moduleとText 6 module / 5 implementationを`ArtifactCoreText`へ統合し、Shapeから暫定配置のGlyphLayout / TextAnimatorを除去した。Shape→Textの依存をtarget linkへ反映した。
- **閃き・仮説:** font descriptor、shaping、glyph atlas、layout、animatorは単一のText ABI境界として扱う方が、ShapeやRenderの実装へ漏れにくい。
- **価値・懸念:** Text変更時のCore本体BMI再構築を抑えられる可能性がある。FreeType/Qt font、Shape、Render、Artifact text layerのmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後にText packのBMI生成、Shape依存、PrimitiveRenderer/ArtifactTextLayerのlink解決を確認する。

## 2026-08-13 — ArtifactCore Generate / Simulation / Track / Source / Project boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Generate/`、`ArtifactCore/include/Simulation/`、`ArtifactCore/include/Track/`、`ArtifactCore/include/Source/`、`ArtifactCore/include/Project/`
- **事実:** Generate 2/2、Simulation 2/2、Track 2/1、Source 1/1、Project 2/0を個別packへ移した。TrackとProjectはArtifact側の利用者があるため、Artifact本体へ明示リンクした。
- **閃き・仮説:** 生成・シミュレーション・トラック・source abstraction・project metadataは、共通Coreの一部として常時ビルドせずleaf packへ切り出せる。
- **価値・懸念:** 各機能変更時のCore本体BMI再構築を抑えられる可能性がある。OpenVDB、NCC tracker、Project statistics利用側のstatic link順は未検証。
- **次の確認:** ビルド許可後に5 packのmodule生成、Geometry/Image/Memory/Utils参照、Artifact利用側のlink解決を確認する。

## 2026-08-13 — ArtifactCore Scene / Rig / Grid / ColorCollection / Sound / Sequence boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Scene/`、`ArtifactCore/include/Rig/`、`ArtifactCore/include/Grid/`、`ArtifactCore/include/ColorCollection/`、`ArtifactCore/include/Sound/`、`ArtifactCore/include/Sequence/`
- **事実:** Scene 2/1、Rig 1/1、Grid 1/0、ColorCollection 1/1、Sound 2/0、Sequence 2/0を個別packへ移した。Composition→Scene、Export→Rig、Configuration→Gridの依存をtarget linkへ反映した。
- **閃き・仮説:** scene graph、rig、grid、color grading collection、sound/sequence contractsを必要な利用側だけへ提供するleaf境界として扱える。
- **価値・懸念:** これらの変更時にCore本体BMI再構築を抑えられる可能性がある。Composition/Export/Configurationのlink順とColorCollection利用側のstatic linkは未検証。
- **次の確認:** ビルド許可後に6 packのmodule生成、依存解決、Artifact本体のlink解決を確認する。

## 2026-08-13 — ArtifactCore Material / Environment / Light / Crowd / Domain / FileSystem / Icon / VST boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Material/`、`ArtifactCore/include/EnvironmentVariable/`、`ArtifactCore/include/Light/`、`ArtifactCore/include/Crowd/`、`ArtifactCore/include/Domain/`、`ArtifactCore/include/FileSystem/`、`ArtifactCore/include/Icon/`、`ArtifactCore/include/VST/`
- **事実:** Material 1/1、Environment 1/1、Light 1/1、Crowd 1/0、Domain 1/0、FileSystem 1/0、Icon 1/0、VST 2/0を個別packへ移した。Scene→Materialの依存をtarget linkへ反映し、OpenXRはoptional条件を維持するため分割対象から除外した。
- **閃き・仮説:** material、environment、IES、crowd、domain、filesystem、icon、VST契約はそれぞれ小さなleaf packとしてCore本体から隔離できる。
- **価値・懸念:** optional backendや各契約の変更時にCore本体BMI再構築を抑えられる可能性がある。Qt/OS/VST利用側のstatic link順は未検証。
- **次の確認:** ビルド許可後に8 packのmodule生成、Scene/Artifact利用側、optional backend条件のlink解決を確認する。

## 2026-08-13 — ArtifactCore Network boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/NetworkRPCClient.ixx`、`ArtifactCore/NetworkRPCServer.ixx`、`ArtifactCore/include/Network/`
- **事実:** Network 3 module / 3 implementationを`ArtifactCoreNetwork`へ移し、ArtifactRender、ArtifactWorker、Artifact本体からNetwork targetへのリンクを追加した。
- **閃き・仮説:** RPC/WebSocket transportはRenderやworkerの実装本体から切り離し、必要な実行経路だけへ提供できる。
- **価値・懸念:** network transport変更時のCore本体BMI再構築を抑えられる可能性がある。Qt Network、RPC ABI、Render/Workerのstatic link順は未検証。
- **次の確認:** ビルド許可後にNetwork packのmodule生成、RenderFarmMaster、FarmWorkerMain、WebSocket利用側のlink解決を確認する。

## 2026-08-13 — ArtifactCore Collaborate boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/src/Collaborate/CollaborationProtocol.cppm`
- **事実:** CollaborationProtocol 1 moduleを`ArtifactCoreCollaborate`へ移した。既存のReactive.Events moduleを利用するだけで、ReactiveEvents本体は変更していない。
- **閃き・仮説:** collaboration protocolのserialization契約は、凍結中のReactiveEvents実装を動かさず、独立した上位packとして切り離せる。
- **価値・懸念:** collaboration protocol変更時のCore本体BMI再構築を抑えられる可能性がある。Reactive.Eventsのmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後にCollaborate packのmodule生成とReactive.Events参照解決を確認する。

## 2026-08-13 — ArtifactEffectsResidual boundary

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/src/Effects/`
- **事実:** 分割後に残った5 effect module / 5 implementationを`ArtifactEffectsResidual`として明示化し、既存の`ArtifactEffects`名はaliasにした。
- **閃き・仮説:** TimeDisplacement、Noise、OpticsCompensation、RadialShadow、SurfaceFXは共通effect契約へ収束し、既存の大きなEffects target名から切り離せる。
- **価値・懸念:** 残存effectの変更時にtarget責務を明確化できる。Diligent、Image、Property依存とstatic link順は未検証。
- **次の確認:** ビルド許可後にResidual packのmodule生成、既存alias利用、Artifact本体のlink解決を確認する。

## 2026-08-13 — ArtifactRenderSupportContracts boundary

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Render/ArtifactRenderContext.ixx`、`Artifact/include/Render/ArtifactRenderROI.ixx`
- **事実:** RenderSupportのContext、ROI、Foundation、PerformanceMonitorの4 moduleを`ArtifactRenderSupportContracts`へ移し、Scheduler/Controller等の実装はSupport本体に残した。
- **閃き・仮説:** RenderSupportの契約層を実装層から分離すると、EffectContractやRenderがscheduler実装へ依存せずに共有契約だけを利用できる。
- **価値・懸念:** render context変更時のSupport実装全体のBMI再構築を抑えられる可能性がある。Render/EffectContractとのstatic link順は未検証。
- **次の確認:** ビルド許可後にContracts packのBMI生成、Support本体、Render、EffectContractのmodule/link解決を確認する。

## 2026-08-13 — ArtifactColor Palette / Node boundaries

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Color/ColorPaletteManager.ixx`、`Artifact/include/Color/ArtifactColorNode.ixx`、`Artifact/include/Color/ArtifactColorNodeGraph.ixx`
- **事実:** ColorPaletteManagerを`ArtifactColorPalette`へ、ColorNode/NodeGraphを`ArtifactColorNode`へ移した。既存ArtifactColorにはOCIO、Science、Settings、Management、Gradingを残した。
- **閃き・仮説:** palette persistenceとnode graphはOCIOの重い管理層から独立した変更単位として分離できる。
- **価値・懸念:** palette/node変更時のArtifactColor全体のBMI再構築を抑えられる可能性がある。Serialization、Core Color、NodeGraphのstatic link順は未検証。
- **次の確認:** ビルド許可後に2 packのmodule生成、Palette/NodeGraph利用側、Artifact本体のlink解決を確認する。

## 2026-08-13 — ArtifactColor Settings / Science boundaries

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Color/ArtifactColorSettings.ixx`、`Artifact/include/Color/ArtifactColorScienceManager.ixx`
- **事実:** ColorSettings 1/1を`ArtifactColorSettings`へ、ColorScience 1/1を`ArtifactColorScience`へ移した。ArtifactColor本体はScienceを利用するため明示依存を追加した。
- **閃き・仮説:** 設定契約とLUT/ACES科学計算をOCIO・Management・Grading実装から分けることで、Color変更の再構築範囲をさらに縮小できる。
- **価値・懸念:** ColorSettings/Scienceのmodule referenceとArtifactColorのstatic link順は未検証。
- **次の確認:** ビルド許可後に2 packのBMI生成、OCIO managerのScience参照、Artifact本体のlink解決を確認する。

## 2026-08-13 — ArtifactColor Management / Grading boundaries

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Color/ArtifactColorManagement.ixx`、`Artifact/include/Color/ArtifactColorGradingEngine.ixx`
- **事実:** ColorManagement 1/1を`ArtifactColorManagement`へ、ColorGradingEngine 1/1を`ArtifactColorGrading`へ移した。既存ArtifactColorから両packへの依存を追加した。
- **閃き・仮説:** management helperとgrading engineをOCIO manager・science・node層から独立した変更単位として扱える。
- **価値・懸念:** Color管理・grading変更時のArtifactColor全体のBMI再構築を抑えられる可能性がある。Core Color/Parallelとstatic link順は未検証。
- **次の確認:** ビルド許可後に2 packのBMI生成、ArtifactColorの依存解決、Artifact本体のlinkを確認する。

## 2026-08-13 — ArtifactCore Material / Environment / Light / Crowd / Domain / FileSystem / Icon / VST boundaries

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Material/`、`ArtifactCore/include/EnvironmentVariable/`、`ArtifactCore/include/Light/`、`ArtifactCore/include/Crowd/`、`ArtifactCore/include/Domain/`、`ArtifactCore/include/FileSystem/`、`ArtifactCore/include/Icon/`、`ArtifactCore/include/VST/`
- **事実:** Material 1/1、Environment 1/1、Light 1/1、Crowd 1/0、Domain 1/0、FileSystem 1/0、Icon 1/0、VST 2/0を個別packへ移した。Scene→Materialの依存をtarget linkへ反映し、OpenXRはoptional条件を維持するため分割対象から除外した。
- **閃き・仮説:** material、environment、IES、crowd、domain、filesystem、icon、VST契約はそれぞれ小さなleaf packとしてCore本体から隔離できる。
- **価値・懸念:** optional backendや各契約の変更時にCore本体BMI再構築を抑えられる可能性がある。Qt/OS/VST利用側のstatic link順は未検証。
- **次の確認:** ビルド許可後に8 packのmodule生成、Scene/Artifact利用側、optional backend条件のlink解決を確認する。

## 2026-08-12 — ArtifactCore Thread boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Thread/`、`ArtifactCore/include/Media/ImageSequenceSource.ixx`
- **事実:** Thread系5 module / 2 implementationを`ArtifactCoreThread`へ移した。Core内の唯一の利用者はMedia.ImageSequenceSourceで、Media targetからThread targetへの依存を追加した。
- **閃き・仮説:** background task / ticker / thread helperはMedia source cacheのような上位domainへ一方向に提供するleaf utilityとして分離できる。
- **価値・懸念:** thread utility変更時のCore本体BMI再構築を抑えられる可能性がある。Media側のmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後にThread packのmodule生成、ImageSequenceSourceの参照、Media link解決を確認する。

## 2026-08-12 — ArtifactCore Platform boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Platform/`、`ArtifactCore/src/Platform/`
- **事実:** Platform系6 module / 4 implementationを`ArtifactCorePlatform`へ移した。Core内の逆importは0件で、Artifact側の有効なPlatform module利用者も静的検索で確認されなかった。
- **閃き・仮説:** OS/process/shell utilityはdomain依存が薄いleaf packにしてもAPI境界を保ちやすく、将来のplatform条件分岐をCore本体から隔離できる。
- **価値・懸念:** platform-specific変更のBMI再構築を局所化できる可能性がある。現時点でlink伝播を追加していないため、隠れたmodule利用者はビルド時に確認が必要。
- **次の確認:** ビルド許可後にPlatform packのWindows条件分岐、module生成、実利用者の有無を確認する。

## 2026-08-12 — ArtifactCore Shape boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Shape/`、`ArtifactCore/include/IO/VectorExport.ixx`、`ArtifactCore/include/Text/GlyphLayout.ixx`
- **事実:** Shapeのprimary module 12組とShape利用側のIO/Text facade 4組を`ArtifactCoreShape`へ移した。Shape利用者だった`IO.ixx`と`Text.TextAnimator`も同じpackへ含め、Core本体からShape packへの逆import closureを閉じた。
- **閃き・仮説:** Shapeを単独で切り出すのではなく、直接のfacade consumerまで同梱することで、geometry / vector export / text layoutのtarget境界を保てる。
- **価値・懸念:** Shape変更時のCore本体BMI再構築を抑えられる可能性がある。IO facadeの再exportとTextAnimator利用側、static link順は未検証。
- **次の確認:** ビルド許可後にShape packのmodule生成、VectorExport、TextAnimator、ArtifactのShape利用者を確認する。

## 2026-08-12 — ArtifactCore Acoustic boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/src/Acoustic/`、`ArtifactCore/src/Diagnostic/DiagnosticRegistry.cppm`
- **事実:** Acoustic系7 moduleとAcoustic snapshotを保持する`Artifact.Diagnostic.Registry`を`ArtifactCoreAcoustic`へ移した。Acousticの唯一のCore内consumerだったregistryを同梱し、逆importを解消した。
- **閃き・仮説:** telemetry registryが特定domainの型を直接保持する場合、そのregistryをdomain packへ置く方がbase targetへの逆依存を避けられる。
- **価値・懸念:** Acoustic変更をCore本体から分離できる可能性がある。registryの他利用者が将来追加される場合はtarget依存を再評価する必要がある。ビルド・linkは未検証。
- **次の確認:** ビルド許可後にAcoustic packのmodule生成とDiagnosticRegistry利用を確認する。

## 2026-08-12 — ArtifactCore Command boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Command/`、`ArtifactCore/include/UI/InteractiveActions.ixx`
- **事実:** Command系7 moduleとUIの`InteractiveActions`を`ArtifactCoreCommand`へ移した。Command targetは`ArtifactCore`へ依存し、`ArtifactCoreUI`はCommand targetへ依存する一方向構成にした。
- **閃き・仮説:** UI facadeが利用するcommand session/action契約をcommand pack側へ置くと、UI input層と編集履歴層の責務境界をtargetでも表現できる。
- **価値・懸念:** command実装変更のBMI再構築をUI以外のCore domainから切り離せる可能性がある。UI/Commandのmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後にCommand packのmodule生成、UI.InteractiveActions、shortcut/action利用側のlink解決を確認する。

## 2026-08-12 — ArtifactCore Data leaf boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Data/`、`ArtifactCore/include/Asset/DataAssetFile.ixx`
- **事実:** Data系12 moduleを`ArtifactCoreData`へ移した。`Asset.DataAssetFile`はP13のAsset packへ戻し、Data targetは`ArtifactCore`へ一方向に依存する。
- **閃き・仮説:** implementationを持たないdata contract群は、consumerを同梱すれば独立module packとして切り出しやすい。
- **価値・懸念:** CSV/table/type inference変更によるCore本体のBMI再構築を抑えられる可能性がある。Asset targetのmodule referenceとstatic link順は未検証。
- **次の確認:** ビルド許可後にData packのinterface生成、Asset.DataAssetFile、ArtifactAssetのlinkを確認する。

## 2026-08-12 — ArtifactCore Asset boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`ArtifactCore/include/Asset/`、`ArtifactCore/include/Utils/AssetManager.ixx`
- **事実:** Asset系11 moduleに`Utils.AssetManager`とimplementationを加え、`ArtifactCoreAsset`へ12 module / 7 implementationを移した。`Asset.DataAssetFile`はData packからAsset packへ戻し、Asset targetはData targetへ依存する。
- **閃き・仮説:** Asset managerがAsset domainの唯一のCore consumerであるため、同じpackに閉じ込めるとAsset database/source lifecycleの境界をtargetで表現できる。
- **価値・懸念:** Asset変更時のCore本体BMI再構築を抑えられる可能性がある。ArtifactAssetのmodule reference、Data/Assetのstatic link順は未検証。
- **次の確認:** ビルド許可後にAsset packのdatabase、DataAssetFile、ArtifactAssetのlink解決を確認する。

## 2026-08-12 — ArtifactCore UI leaf boundary

- **関連:** `ArtifactCore/CMakeLists.txt`、`Artifact/include/Widgets/`、`Artifact/src/Widgets/`
- **事実:** UI系19 module / 8 implementationを`ArtifactCoreUI`へ移した。Core内の非UI moduleからUI moduleへの逆importは0件で、input operator、shortcut、selection、layout契約を分離した。
- **閃き・仮説:** UI state / input contractをCore本体から分けると、shortcutやviewport操作の変更を他domainのBMI再構築から切り離しやすい。
- **価値・懸念:** Artifactのアプリ本体は`ArtifactCoreUI`をリンクして既存APIを維持できる。一方、ShortcutBindingsの実際の利用target、MSVC module reference、static archive link順は未検証。
- **次の確認:** ビルド許可後にUI packのBMI生成、AppMain / timeline / composition editorのmodule解決とlinkを確認する。

## 2026-08-12 — VolumetricShineの入力は事前抽出済みバッファを要求する

- **関連:** `ArtifactCore/include/ImageProcessing/VolumetricShine.ixx`、`Artifact/src/Effects/Glow/GlowEffect.cppm`
- **事実:** `VolumetricShine::process()` はサンプル輝度を計算するが選別には使わず、渡されたRGB全体を放射状に蓄積し、さらに入力バッファ自身へ加算する。Artifact側のVolumetric Shineは、しきい値で明部を事前抽出し、処理後から抽出元を差し引いて元画像へ合成している。
- **価値・懸念:** 未抽出の通常画像を直接渡すと画面全体が光線化し、処理済みバッファをそのまま加算すると明部が二重加算される。API名だけではこの前提が読み取りにくい。
- **次の確認:** Core側Settingsへ明示的なthresholdを追加するか、入力契約を型またはコメントで明示し、既存呼び出し元との互換性を確認する。

## 2026-08-12 — 合成補助エフェクトは既存の名前付き入力基盤を再利用できる

- **関連:** `Artifact/include/Effects/ArtifactEffectFrameSampler.ixx`、`Artifact/src/Effects/ArtifactEffectFrameSampler.cppm`、`Artifact/include/Effects/EffectHostContract.ixx`
- **事実:** `IEffectFrameSampler::sampleNamedInput()` と `EffectInputBundle` は、レイヤーIDで保持した同一フレーム画像を補助入力として取得できる。Depth Bokehに加え、Light Wrap Pro、Match Grain、Wire / Object Remover、Depth Relight、Matte Refine、Pixel / Dust Fixer、Atmospheric Depth、Edge Color Compositeがこの経路を利用する実装になった。
- **価値・懸念:** 背景、参照素材、クリーンプレート、除去・修復マスク、depth/normal入力を、新しいイベント配線なしで共有できる。現状の入力指定は文字列ID中心で、既存の `ObjectReference` editorは値を`qint64`へ変換するため、任意文字列のLayerIDをそのまま安全に往復できない。UIでのレイヤー選択・欠損時表示・保存再読込・キャッシュ依存の明示は未検証。
- **次の確認:** 補助入力レイヤーを変更した際のキャッシュ無効化を確認し、文字列LayerIDを失わない既存Property Editor上の選択UIへ安全に写像できるか設計する。

## 2026-08-13 — Keying effects は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Keying/`、`Artifact/src/Effects/Keying/`
- **事実:** LumaKey / ChromaKey / DifferenceKey / IBKKeyer の4 module / 4 implementationを`ArtifactEffectsKeying`へ移し、Spatial packから除去した。4実装のimportは共通Effect contract、Image、Property、Core Parallel、IBKKeyerのみRender/Diligentへ収束している。
- **閃き・仮説:** matte生成をSpatial画像処理から分離すると、keyerの変更によるSpatial packのBMI再構築を抑えつつ、GPU keyerだけを独立して検証できる可能性がある。
- **価値・懸念:** Keyingという責務がtarget構成にも現れ、今後のmatte/refinement拡張の依存方向を明確にできる。実際のmodule BMI生成とstatic link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsKeying`のmodule生成、IBKKeyerのDiligent依存、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Blur effects は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Blur/`、`Artifact/src/Effects/Blur/`
- **事実:** AnisotropicFlowBlur / ApertureShapeBlur / ReactionDiffusionBlur の3 module / 3 implementationを`ArtifactEffectsBlur`へ移し、Spatial packから除去した。共通のEffect contract、Image / Property / Core Parallelを中心とする依存で、Blur packのtarget linkをArtifact本体へ追加した。
- **閃き・仮説:** Blur系をSpatialの汎用残余から分離すると、ぼかしアルゴリズムの変更を他の空間効果のBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Blur責務をtarget構成にも表現できる。一方、Blur実装の未登録補助moduleやstatic archiveの実際のpull-inは未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsBlur`のmodule生成、ImageProcessing / Core Parallelの参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Procedural generators は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Generate/`、`Artifact/src/Effects/Generate/`
- **事実:** SimpleRain / RadioWaves の2 module / 2 implementationを`ArtifactEffectsGenerate`へ移し、Spatial packから除去した。両実装は共通のEffect contract、Image、Property、Core Parallelを中心に依存する。
- **閃き・仮説:** procedural generatorを小さなpackに閉じ込めると、生成系エフェクトの変更をSpatialの他のオペレータから切り離せる可能性がある。
- **価値・懸念:** Generate責務をtarget構成にも表現できる。実際のmodule BMI生成とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsGenerate`のmodule生成、Image/Parallel参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Distort effects は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Distort/`、`Artifact/include/Effects/TurbulentDisplace/`、`Artifact/src/Effects/`
- **事実:** DisplacementMap / TurbulentDisplace の2 module / 2 implementationを`ArtifactEffectsDistort`へ移し、Spatial packから除去した。両実装は共通のEffect contract、Image、Property、Core Parallelを中心に依存する。
- **閃き・仮説:** distortion operatorを独立packにすると、画像変位系の変更を他のSpatial operatorのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Distort責務をtarget構成にも表現できる。実際のmodule BMI生成とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsDistort`のmodule生成、Image/Parallel参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Stylize effects は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/{Kaleidoscope,Dithering,Kuwahara,Bevel}/`、`Artifact/src/Effects/`
- **事実:** Kaleidoscope / Dithering / Kuwahara / Bevel の4 module / 4 implementationを`ArtifactEffectsStylize`へ移し、Spatial packから除去した。4実装は共通のEffect contract、Image、Property、GPU compute、Render境界に収まる。
- **閃き・仮説:** stylize operatorを独立packにすると、GPUベースの画調・質感効果の変更をSpatialの残余operatorから切り離せる可能性がある。
- **価値・懸念:** Stylize責務をtarget構成にも表現できる。実際のmodule BMI生成とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsStylize`のmodule生成、GPU compute/Render参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Glow effects は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Glow/`、`Artifact/src/Effects/Glow/`、`Artifact/include/Effects/DirectionalGlowEffect.ixx`
- **事実:** DirectionalGlow、Glow、EdgeBloom、ChromaticGlow、ReactiveGlow、LiquidGlow、ResidualGlow、PhysicalHalation、LuminescenceCausticsの9 module / 9 implementationを`ArtifactEffectsGlow`へ移し、Spatial packから除去した。Glow packはImage/Property、GPU compute、Renderを主な依存境界とする。
- **閃き・仮説:** Glow系を一つのpackに閉じると、光学・発光アルゴリズムの変更をSpatial残余operatorのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Glow責務をtarget構成にも表現できる。PhysicalHalationのParticle依存と、既存Glow variantの自動登録との重複は静的確認済みだが、実際のmodule/link解決は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsGlow`のmodule生成、Particle/GPU compute/Render参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Optical distortion effects は複数の旧packをまたぐ境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/LensDistortion/`、`Artifact/include/Effects/OpticsCompensation/`、`Artifact/src/Effects/`
- **事実:** LensDistortion / OpticsCompensation の2 module / 2 implementationを`ArtifactEffectsOptics`へ集約した。LensDistortionはSpatial側、OpticsCompensationはresidual側にあったため、両方の元source listから除去し、ImageProcessing.Distortionを共通依存とした。
- **閃き・仮説:** sourceの物理配置や旧分類ではなく、共有する画像変形契約でpackを切ると、光学補正の変更範囲を一つのtargetに閉じ込められる可能性がある。
- **価値・懸念:** Spatial/residual間の責務重複を解消できる。OpticsCompensation.cppmは通常のmodule implementation形式のため、CXX_MODULES登録とMSVCのmodule参照は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsOptics`のmodule生成、ImageProcessing.Distortion参照、両旧packからの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Wave effect は独立した GPU operator pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Wave/WaveEffect.ixx`、`Artifact/src/Effects/Wave/WaveEffect.cppm`
- **事実:** Wave の1 module / 1 implementationを`ArtifactEffectsWave`へ移し、Spatial packから除去した。Wave実装はImage、Property、GPU compute、Render、Core Parallelを主な依存とする。
- **閃き・仮説:** 単一でも責務と変更頻度が独立したGPU operatorは専用packにすると、Spatial残余の再構築範囲を明確に抑えられる可能性がある。
- **価値・懸念:** Wave責務をtarget構成にも表現できる。実際のmodule BMI生成とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsWave`のmodule生成、GPU compute/Render参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Image filters は Spatial から独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/{LinearWipe,Liquify,Mosaic,Spherize,Sharpen,FindEdges}/`、`Artifact/src/Effects/`
- **事実:** LinearWipe / Liquify / Mosaic / Spherize / Sharpen / FindEdges の6 module / 6 implementationを`ArtifactEffectsFilters`へ移し、Spatial packから除去した。共通のImage、Property、GPU compute、Render、Core依存をtargetで表現した。
- **閃き・仮説:** 画像フィルタ群を残余Spatialから分離すると、フィルタアルゴリズムの変更を他のSpatial operatorのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Filters責務を明示できる。実際のmodule BMI生成とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsFilters`のmodule生成、GPU compute/Render参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — AddNoise は独立した GPU/image operator pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/AddNoise/AddNoiseEffect.ixx`、`Artifact/src/Effects/AddNoise/AddNoiseEffect.cppm`
- **事実:** AddNoise の1 module / 1 implementationを`ArtifactEffectsNoise`へ移し、Spatial packから除去した。Image upload、GPU compute、Render、Core Parallelを主な依存とする。
- **閃き・仮説:** 単一moduleでもGPU uploadを伴う独立operatorは専用packに分けることで、ノイズ実装変更の再構築範囲を明確化できる可能性がある。
- **価値・懸念:** Noise責務をtarget構成にも表現できる。実際のmodule BMI生成とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsNoise`のmodule生成、Image upload/GPU compute参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — AutoMosaic は独立した CV operator pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/AutoMosaicEffect.ixx`、`Artifact/src/Effects/AutoMosaicEffect.cppm`
- **事実:** AutoMosaic の1 module / 1 implementationを`ArtifactEffectsAutoMosaic`へ移し、Spatial packから除去した。FaceDetection、CvUtils、Property、Core Parallelを主な依存とする。
- **閃き・仮説:** 顔検出を伴うCV operatorを一般的なSpatial残余から分離すると、検出依存の変更を画像効果群から切り離せる可能性がある。
- **価値・懸念:** AutoMosaicのCV責務をtarget構成にも表現できる。実際のFaceDetection/CvUtils link解決は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsAutoMosaic`のmodule生成、FaceDetection/CvUtils参照、Spatialとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Motion/flow rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{OpticalFlowBlur,VectorBlur,VectorFlowGlitch,LightTrails,MotionTrail}Effect.ixx`、対応する`Artifact/src/Effects/Rasterizer/`実装
- **事実:** OpticalFlowBlur / VectorBlur / VectorFlowGlitch / LightTrails / MotionTrail の5 module / 5 implementationを`ArtifactEffectsMotion`へ移し、Rasterizer packから除去した。Effect.Context、Image、Property、GPU compute、Renderを共通依存とする。
- **閃き・仮説:** motion/flow operatorをRasterizer残余から分離すると、履歴・ベクトル系の変更を他のstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Motion責務をtarget構成にも表現できる。実際のEffect.Context module参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsMotion`のmodule生成、Effect.Context/GPU compute/Render参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — プロ向けエフェクトUIのボトルネックはパラメータ記述契約にある

- **関連:** `ArtifactCore/include/Property/AbstractProperty.ixx`、`Artifact/src/Effects/ArtifactAbstractEffect.cppm`、`Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorShared.cppm`、`Artifact/src/Effect/ArtifactEffectPreset.cppm`、各 `Artifact/src/Effects/**/getProperties()`
- **事実:** 共通 `PropertyMetadata` は表示名、単位、tooltip、hard/soft range、step を保持できるが、静的検索では `getProperties()` を持つ76ファイルに対し、unit / tooltip / step / soft range のいずれかを設定するファイルは12件だった。Gaussian Blurはrange/stepを定義する一方、Glow、Levels、Curvesなどには値だけの項目が多い。列挙候補は専用metadataではなくtooltip文字列またはproperty名のハードコードで推定される。effect presetの値型はFloat / Color / Stringのみで、Boolean / Integerを型付きで保持しない。
- **閃き・仮説:** エフェクト数や個別UIを増やす前に、stable parameter ID、表示label、型、単位、hard/soft range、step/precision、enum choices、section、visibility dependency、animatable、quality cost、preset inclusionを一つのdescriptor契約へ集約すると、Inspector、Property Editor、preset、OFX bridge、automationが同じ意味を共有できる。
- **価値・懸念:** 代表的な5エフェクトから段階導入すれば、数値操作の精度、意味の理解、プリセット再現性、将来の互換性を小さい変更範囲で改善できる。表示名を識別子としている既存effectがあるため、一括renameや全effect移行は保存互換性を壊す懸念がある。
- **実装状況:** Curvesの制御点editor、Levelsのmaster range editor、GlowのContribution Onlyとrange/quality metadata、effect preset schema 2のInteger / Boolean / Double型保持、および旧schema読込を追加した。既存property名は互換aliasとして維持し、新しい複合controlだけstable IDを採用した。ビルド・runtime確認は未実施。
- **次の確認:** Curves / Levelsのdrag previewと保存再読込、GlowのCPU/GPU Contribution表示一致、schema 1/2 presetの往復を確認する。その後Gaussian Blur / White Balanceへ同じmetadata契約を展開する。

## 2026-08-13 — Digital artifact rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{DataMosh,Glitch,FilmDamage,Deflicker}Effect.ixx`、対応する`Artifact/src/Effects/Rasterizer/`実装
- **事実:** DataMosh / Glitch / FilmDamage / Deflicker の4 module / 4 implementationを`ArtifactEffectsDigital`へ移し、Rasterizer packから除去した。Effect.Context、Image、Property、Core Parallelを共通依存とする。
- **閃き・仮説:** digital artifact系を独立packにすると、glitch/film damage/deflicker変更を他のstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Digital責務をtarget構成にも表現できる。実際のEffect.Context参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsDigital`のmodule生成、Effect.Context/Image/Parallel参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Pattern rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{Bricks,HexGrid,Halftone,Stripes,Voronoi}Effect.ixx`、対応する`Artifact/src/Effects/Rasterizer/`実装
- **事実:** Bricks / HexGrid / Halftone / Stripes / Voronoi の5 module / 5 implementationを`ArtifactEffectsPatterns`へ移し、Rasterizer packから除去した。Image、Property、Core Parallel、GPU compute、Renderを共通依存とする。
- **閃き・仮説:** pattern generatorを独立packにすると、テクスチャ生成系の変更を他のstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Patterns責務をtarget構成にも表現できる。実際のGPU compute/Render参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsPatterns`のmodule生成、Image/Property/Parallel/GPU compute参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Chromatic rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{ChromaticAberration,ChromaticRelief}Effect.ixx`、対応する`Artifact/src/Effects/Rasterizer/`実装
- **事実:** ChromaticAberration / ChromaticRelief の2 module / 2 implementationを`ArtifactEffectsChromatic`へ移し、Rasterizer packから除去した。Image、Property、Core Parallel、GPU compute、Renderを共通依存とする。
- **閃き・仮説:** chromatic operatorを独立packにすると、色収差・色レリーフの変更を他のstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Chromatic責務をtarget構成にも表現できる。実際のGPU compute/Render参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsChromatic`のmodule生成、Image/Property/Parallel/GPU compute参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Shadow rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/DropShadowEffect.ixx`、`Artifact/include/Effects/Rasterizer/InnerShadowEffect.ixx`、対応する実装
- **事実:** DropShadow / InnerShadow の2 module / 2 implementationを`ArtifactEffectsShadows`へ移し、Rasterizer packから除去した。Image、Property、Core Parallel、GPU compute、Renderを共通依存とする。
- **閃き・仮説:** shadow operatorを独立packにすると、影生成の変更を他のstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Shadows責務をtarget構成にも表現できる。実際のGPU compute/Render参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsShadows`のmodule生成、Image/Property/Parallel/GPU compute参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Context-aware rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{DifferenceMatte,Edge,Ghost,PixelSort}Effect.ixx`、対応する実装
- **事実:** DifferenceMatte / Edge / Ghost / PixelSort の4 module / 4 implementationを`ArtifactEffectsContextual`へ移し、Rasterizer packから除去した。Effect.Context、Image、Property、Core Parallelを共通依存とする。
- **閃き・仮説:** frame-contextを参照するraster operatorを独立packにすると、入力コンテキスト連携の変更をstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Contextual責務をtarget構成にも表現できる。実際のEffect.Context参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsContextual`のmodule生成、Effect.Context/Image/Property/Parallel参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Temporal context rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{PosterizeTime,ScreenShake}Effect.ixx`、対応する実装
- **事実:** PosterizeTime / ScreenShake の2 module / 2 implementationを`ArtifactEffectsTemporalContext`へ移し、Rasterizer packから除去した。Effect.Context、Image、Property、Core Parallelを共通依存とする。
- **閃き・仮説:** 時間・フレームコンテキストを持つraster operatorを独立packにすると、時間サンプリング変更をstateless raster effectのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** TemporalContext責務をtarget構成にも表現できる。実際のEffect.Context参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsTemporalContext`のmodule生成、Effect.Context/Image/Property/Parallel参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Finishing rasterizers は独立した pack 境界を持つ

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/{RadialBlur,Stroke,Vignette}Effect.ixx`、`Artifact/include/Effects/Satin/SatinEffect.ixx`、対応する実装
- **事実:** RadialBlur / Satin / Stroke / Vignette の4 module / 4 implementationを`ArtifactEffectsFinishing`へ移し、Rasterizer packから除去した。Image、Property、Core Parallel、GPU compute、Renderを共通依存とする。
- **閃き・仮説:** 仕上げ処理を独立packにすると、最終画調・輪郭処理の変更を他のraster operatorのBMI再構築から切り離せる可能性がある。
- **価値・懸念:** Finishing責務をtarget構成にも表現できる。実際のGPU compute/Render参照とstatic archive link順は未検証。
- **次の確認:** ビルド許可後に`ArtifactEffectsFinishing`のmodule生成、Image/Property/Parallel/GPU compute参照、Rasterizerとの重複除去、アプリのlink解決を確認する。

## 2026-08-13 — Legacy Rasterizer path の同名 module 二重定義を target から除外した

- **関連:** `Artifact/CMakeLists.txt`、`Artifact/include/Effects/Rasterizer/GlowEffect.ixx`、`Artifact/include/Effects/Rasterizer/KaleidoscopeEffect.ixx`、canonicalな`ArtifactEffectsGlow` / `ArtifactEffectsStylize`
- **事実:** Rasterizer pathのGlow/Kaleidoscopeはcanonical pack側と同じmodule名を持つ別sourceだった。ファイルは削除せず、Rasterizer targetのsource listから除外し、canonical packだけがmoduleを提供するようにした。FinishingのSatin interface pathも実在ファイルへ修正した。
- **閃き・仮説:** 分割ではtarget追加だけでなく、同一module名の旧経路を明示的に閉じないと、BMI/リンクのownershipが不定になる可能性がある。
- **価値・懸念:** moduleの二重提供を静的に避けられる。canonical sourceとlegacy sourceの内容差分を保持したままなので、legacy側を完全廃止できるかは未検証。
- **次の確認:** ビルド許可後にGlow/Kaleidoscopeのmodule定義が一つずつ生成されること、Satin interfaceと実装の対応、Rasterizer residualの実source ownershipを確認する。

## 2026-08-13 — Residual effect source ownership を閉じた

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactEffectsDistort`、`ArtifactEffectsShadows`、`ArtifactEffectsNoise`、`ArtifactEffectsSurfaceFX`
- **事実:** residualに残っていたTimeDisplacementをDistort、RadialShadowをShadows、NoiseEffectをNoiseへ統合し、SurfaceFXを`ArtifactEffectsSurfaceFX`へ分離した。さらに全 focused packを`ARTIFACT_EFFECTS_MODULES/IMPL`から明示的に除去した。
- **閃き・仮説:** pack用変数を狭めた後に汎用残余リストを除去すると、CMake変数の評価順によってsplit sourceがresidualへ戻るため、ownership除去はfocused pack単位で明示する必要がある。
- **価値・懸念:** residual targetのsource ownershipを4 module / 4 implementationまで縮退させ、二重コンパイルを静的に防げる可能性がある。CMake configure / module scanは未検証。
- **次の確認:** ビルド許可後にresidualがTimeDisplacement/RadialShadow/Noise/SurfaceFXを含まないこと、4 residual sourceの実際のlink解決、focused packとの重複がないことを確認する。

## 2026-08-13 — Empty residual target を compatibility umbrella に変更した

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactEffectsResidual`、`ArtifactEffects` alias、focused ArtifactEffects packs
- **事実:** focused packへのsource移管後にresidual sourceが0/0になったため、`ArtifactEffectsResidual`をSTATICからINTERFACEへ変更し、全focused packへのlink委譲だけを持たせた。既存の`ArtifactEffects` alias名は維持した。
- **閃き・仮説:** 空archiveを互換入口として残すより、INTERFACE umbrellaにすると既存link名を保ちながら不要なbinary targetを生成せずに済む。
- **価値・懸念:** source ownershipをfocused packへ一意化できる。umbrella経由のtransitive link順と既存consumerのarchive pull-inは未検証。
- **次の確認:** ビルド許可後にresidual archiveが生成されないこと、`ArtifactEffects` aliasからfocused packが伝播すること、既存のeffect consumerが解決することを確認する。

## 2026-08-13 — Empty Spatial/Rasterizer targets を compatibility umbrella に変更した

- **関連:** `Artifact/CMakeLists.txt`、`ArtifactEffectsSpatial`、`ArtifactEffectsRasterizer`、focused effect packs
- **事実:** source 0/0になった`ArtifactEffectsSpatial`と`ArtifactEffectsRasterizer`をSTATICからINTERFACEへ変更し、それぞれfocused pack群へのtransitive link入口として維持した。
- **閃き・仮説:** source ownershipを全てfocused packへ移した後も旧target名をumbrellaとして残すと、既存consumerのtarget参照を保ちながら空archive生成を避けられる。
- **価値・懸念:** Spatial/Rasterizerの互換入口を維持しつつ、実体targetを増やさずに済む。umbrella経由のlink順と既存consumerの解決は未検証。
- **次の確認:** ビルド許可後に両targetがarchiveを生成しないこと、旧target名から各focused packが伝播すること、全effect consumerが解決することを確認する。

## 2026-08-13 — Focused pack ownership を manifest checker に追加した

- **関連:** `scripts/check_source_manifests.py`、`Artifact/CMakeLists.txt`、Artifact focused effect packs
- **事実:** checkerにfocused `ARTIFACT_EFFECTS_*_MODULES/IMPL`の件数一致、source path存在、pack間重複検査を追加した。実行結果は全pack pass、重複0、missing path 0だった。
- **閃き・仮説:** explicit source manifestだけではCMake target間の二重ownershipを検出できないため、pack変数の静的検査を同じcheckerに置くと分割変更の回帰を早期検出できる。
- **価値・懸念:** 今回のresidual再登録問題のようなCMake評価順の回帰を、configure前に検出できる可能性がある。CMakeの実configure解釈そのものは未検証。
- **次の確認:** CIまたはsource追加後にcheckerを実行し、focused packの件数・重複・path検査が継続してgreenであることを確認する。

## 2026-08-13 — Focused pack の link reachability を静的確認した

- **関連:** `Artifact/CMakeLists.txt`、22 focused `ArtifactEffects*` STATIC pack、Spatial/Rasterizer/Residual INTERFACE umbrella
- **事実:** 22 focused packすべてがArtifact本体または互換umbrellaの`target_link_libraries`から到達可能で、未リンクpackは0件だった。
- **価値・懸念:** source ownershipを分離してもArtifact executableから孤立するpackがないことを静的に確認できた。実際のstatic archive pull-in、module BMI、link orderは未検証。
- **次の確認:** ビルド許可後に各packのmodule生成と、umbrella経由を含む実リンク解決を確認する。

## 2026-08-13 — Ownership checker を root custom target の経路へ統合した

- **関連:** `CMakeLists.txt`、`scripts/check_source_manifests.py`
- **事実:** rootの`check_source_manifests` custom targetは既存のPython checkerを実行しており、checker拡張後はexplicit manifestに加えてfocused packの件数・重複・path ownershipも同じ経路で検査する。targetのコメントを実際の責務に合わせて更新した。
- **価値・懸念:** CIや開発者が既存の検査targetを呼ぶだけで、CMake source ownershipの回帰も検出できる。CMake configureそのものは未実行。
- **次の確認:** ビルド許可後にroot custom target経由でcheckerが起動することを確認する。
### 2026-08-13: RadialBlur の旧 Rasterizer 重複も所有リストから除外
- **関連:** `Artifact/CMakeLists.txt`、RadialBlur の canonical / legacy source paths
- **事実:** `Artifact.Effect.Rasterizer.RadialBlur` は `Effects/RadialBlur` と `Effects/Rasterizer` の両方にインターフェース・実装が存在し、モジュール名が重複していた。
- **対応:** `ArtifactEffectsFinishing` が現在所有する `Effects/Rasterizer/RadialBlurEffect.ixx/.cppm` を Rasterizer umbrella と residual の source list から除外し、未使用の `Effects/RadialBlur` 側は manifest exclusion のまま保持した。ファイル自体は削除していない。
- **価値/懸念:** 二重定義を避けつつ、履歴上の旧ファイルを保全できる。CMake configure / build による実際の target 解決は未検証。
- **次に確認:** 他の module-name 重複は既存の分割実装かを確認し、同様に明確な二重定義だけを所有リストから除外する。
## 2026-08-13 — QADS adapter と native dock surface の段階移行境界

- **関連:** `Artifact/include/Widgets/ArtifactDockManager.ixx`、`Artifact/include/Widgets/ArtifactNativeDockSurface.ixx`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`
- **事実:** 公開widget moduleからQADS型を除去し、QADS adapterとbackend-neutralな `DockLayoutRegistry` を分離した。native surfaceは5領域、tab化、portable layout、visible／pinned／activate／area移動を持つが、floatingとdrag/dropは未対応としてcapabilityで明示している。
- **仮説:** QADS state blobを既定のモデルにし続けると、native backendへの切替時にfloatingやtab groupの差異が暗黙に失われるため、portable modelを先に正規化し、未対応機能は復元時に診断ログへ出す方が安全。
- **価値・懸念:** adapter交換の境界と部分復元の失敗条件を明示できる。一方、native surfaceは現在ArtifactMainWindowの既定backendへ接続しておらず、実機表示・module hygiene・QADS完全撤去は未検証。
- **次に確認:** ビルド許可後に新規moduleのコンパイル、native surfaceの実機表示、portable復元、既存QADS layoutとの比較を検証する。

## 2026-08-13 — AI write 結果は既存 CommandResult を再利用する

- **関連:** `ArtifactCore/include/AI/CommandIR.ixx`、`Artifact/include/AI/WorkspaceAutomation.ixx`
- **事実:** `ArtifactCore::CommandResult` は `success`、`valid`、`executed`、`type`、`error`、`undoLabel`、`diagnostics`、`details` を持ち、`toVariantMap()` と `commandResultFromVariantMap()` を備えている。`WorkspaceAutomation` の `validateCommand` / `executeCommand` はこの型を経由している。
- **対応:** AI 側の共通判定に合わせ、既存フィールドを保持したまま `validateCommand` は `ok = valid`、`executeCommand` は `ok = success` を追加した。
- **価値/懸念:** 新しい結果型を増やさず、既存の write 実行経路を AI から一貫して判定できる。`errorCode` の体系はまだ存在せず、自由文 `error` を機械分類する設計は未着手。
- **次に確認:** command type ごとの error taxonomy を設計し、`error` の自由文と互換な `errorCode` を段階的に追加する。ビルド・runtime確認は未実施。

## 2026-08-13 — CommandResult の error taxonomy は段階導入する

- **関連:** `ArtifactCore/include/AI/CommandIR.ixx`、`Artifact/src/AI/CommandIRExecutor.cppm`
- **事実:** 現在の `CommandResult` は `error` を自由文で保持し、validation failure、unsupported command、target/property failure、render failure が同じ文字列フィールドに入る。既存の command 実装には安定した `errorCode` フィールドはない。
- **提案:** 既存の `error` を保持したまま、まず `COMMAND_INVALID`、`UNSUPPORTED_COMMAND`、`TARGET_NOT_FOUND`、`PROPERTY_INVALID`、`EXECUTION_FAILED`、`RENDER_FAILED` の粗い分類を追加する。詳細な command-specific code は後段にする。
- **価値/懸念:** AI が再試行・ユーザー確認・入力修正を選べるようになる。一方、自由文からの自動分類は誤判定し得るため、各 executor の失敗分岐で明示設定する必要がある。
- **実装状況:** `WorkspaceAutomation` の `validateCommand` / `executeCommand` で、既存 `error` を保持したまま `COMMAND_INVALID`、`UNSUPPORTED_COMMAND`、`PROPERTY_INVALID`、`TARGET_NOT_FOUND`、`RENDER_FAILED`、`EXECUTION_FAILED` の粗い分類を段階導入した。`CommandResult` に `errorCode` と `retryable` を追加し、validation と `CommandIRExecutor` の明示的な property / effect-index failure は executor 側で直接設定する。facade は既存呼び出しとの互換 fallback として残している。
- **次に確認:** 残りの executor failure branch を、意味が確定するものだけ段階移行する。ビルド・runtime確認は未実施。

## 2026-08-14 — GPU文字 atlas はカラー絵文字を単色QRawFont経路で表現できない

- **関連:** `ArtifactCore/src/Text/GlyphAtlas.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`、`experiments/TextAnimatorLab/artifact_gpu_text_smoke.cpp`
- **事実:** DX12のGPUスモークで日本語と通常ラテン文字は描画できるが、`U+1F9EA`（🧪）はQRawFontのalpha atlas経路では□になる。UTF-8ファイル入力で引数変換を排除しても再現したため、PowerShellのUnicode transportだけが原因ではない。
- **仮説:** Windowsのカラー絵文字フォントをalpha-only atlasへ落とす現在の設計では、カラーレイヤー情報を失うか、代替グリフの輪郭を取得している。絵文字は単色フォールバック、カラーbitmap atlas、または別の絵文字描画契約を選択できる必要がある。
- **価値・懸念:** 「文字が存在する」ことと「GPU atlasで正しく描画できる」ことを分離して監査できる。絵文字を通常文字と同じGlyphKeyだけで扱うと、カラー情報とgrapheme/ZWJ単位を失う。
- **次に確認:** QRawFontのglyph index・alphaMapサイズ・font familyを絵文字ケースごとに記録し、単色記号（★）とカラー絵文字（🧪、😀、ZWJ）を比較する。

## 2026-08-14 — Segoe UI Emoji はalpha取得可能、欠落点はカラー転送

- **関連:** `experiments/TextAnimatorLab/artifactcore_text_smoke.cpp`、`ArtifactCore/src/Text/GlyphAtlas.cppm`
- **事実:** `🧪` は `Segoe UI Emoji` のglyph index 3620として解決され、`QRawFont::alphaMapForGlyph` は86x88のbitmapを返し、`pathForGlyph`も空ではなかった。alpha画像は `artifactcore_emoji_alpha.png` として保存できた。
- **結論:** 「絵文字glyphを取得できない」は誤り。現在の単色coverage atlasは輪郭を取得できるが、カラーbitmapの色レイヤーを保持しない。GPU側の未完了範囲はカラーatlas形式、転送、shader分岐である。
- **次に確認:** alpha-only絵文字をGPUで描画する経路を最新ArtifactCore/ArtifactRenderビルドで再検証し、その後カラーbitmap取得方式を選定する。

## 2026-08-14 — Windowsカラーglyphの実装候補はDirectWrite 3

- **関連:** `ArtifactCore/src/Text/GlyphAtlas.cppm`、Windows SDK `um/dwrite_3.h`
- **事実:** 現行Windows SDKには `IDWriteFontFace5`、`DWRITE_COLOR_GLYPH_RUN1`、カラーglyph列挙APIが存在する。Qtの`QRawFont::alphaMapForGlyph`だけでは色レイヤーを取得できない。
- **提案:** Windows実装ではDirectWriteのカラーglyph runをRGBA bitmapへラスタライズする専用providerを設け、`GlyphRenderMode::ColorBitmap`だけをそのproviderへ分岐する。通常glyphは既存QRawFont coverage経路を維持する。
- **価値・懸念:** モノクロ経路を壊さず、カラー／COLR／SVG系をOSのフォント実装に合わせられる。一方、DirectWriteのfont faceとQtのfont family・glyph indexの対応、およびGPU atlas更新のスレッド境界は未検証。
- **次に確認:** DirectWrite font face生成と`DWRITE_COLOR_GLYPH_RUN1`のbitmap化を小さなWindows専用Coreスモークで検証し、QImageは入力境界に限定してRGBAバッファへ明示変換する。

## 2026-08-14 — DirectWriteカラーglyph run列挙は実機で成立

- **関連:** `experiments/TextAnimatorLab/directwrite_color_glyph_smoke.cpp`
- **事実:** Windows SDKの`IDWriteFactory2::TranslateColorGlyphRun`で`🧪`（glyph 3620）を実行時に列挙し、5つのカラーglyph runとパレットインデックスを取得できた。
- **結論:** カラー情報の取得不能ではなく、残る実装範囲はrunのRGBAラスタライズ、GlyphAtlasへの明示コピー、GPU shaderでのカラーサンプル分岐である。
- **次に確認:** DirectWriteカラーrunを一時RGBAターゲットへ描画する方法を、既存のQt合成禁止・GPU本流優先ルールに沿って選定する。まずCPU診断用の最小RGBAバッファで座標・透明度・パレット合成を検証する。

## 2026-08-14 — DirectWriteカラーrunはalpha textureへラスタライズ可能

- **関連:** `experiments/TextAnimatorLab/directwrite_color_glyph_smoke.cpp`
- **事実:** `IDWriteFactory3::CreateGlyphRunAnalysis` と `IDWriteGlyphRunAnalysis::CreateAlphaTexture` を使い、`🧪` の5カラーrunから合計3841個の非透明alpha pixelを取得できた。
- **結論:** DirectWriteカラーrunは、runごとのパレット色とalpha textureを明示合成してRGBAアトラスへ変換できる。QtのQPainterをGPU本流へ追加する必要はない。
- **次に確認:** palette entry取得、runごとのtexture boundsの共通キャンバス合成、GlyphAtlasのカラー専用入力APIを実装する。

## 2026-08-14 — Segoe UI EmojiのカラーrunはrunColorを直接提供する

- **関連:** `experiments/TextAnimatorLab/directwrite_color_glyph_smoke.cpp`
- **事実:** `🧪` の5runは大きな`paletteIndex`値を返すが、各runの`runColor`には有効なRGBA色が入っている（例: `(0.765, 0.937, 0.235, 1.0)`）。`imageFormats=0x5`、alpha textureも生成済み。
- **結論:** カラー合成ではpalette indexを通常CPAL indexとして解釈せず、DirectWriteが返す`runColor`を優先する。特殊palette indexはそのままGPU契約へ持ち込まない。
- **次に確認:** runColor×alpha textureのCPU合成を診断バッファで検証し、GlyphAtlasのカラー矩形へ保存するデータ形式を固定する。

## 2026-08-14 — DirectWriteカラーrunのRGBA合成スモークが成立

- **関連:** `experiments/TextAnimatorLab/directwrite_color_glyph_smoke.cpp`、`directwrite_color_glyph.ppm`
- **事実:** 5つのカラーrunを各texture boundsの共通キャンバス（93x92）へ配置し、`runColor`とalphaをpremultiplied相当のsource-overで合成できた。PPM出力は25,681 bytes、合成入力はalphaPixels=3841。
- **結論:** GlyphAtlas側で必要な最小データは、カラー矩形、RGBA8画素、bearing/advance、render modeで固定できる。Qt合成やQImageのホットパス追加は不要。
- **次に確認:** この合成処理をCoreのWindows専用providerへ移し、DirectWrite非対応環境では既存coverageまたは明示的unsupportedへフォールバックする。
## 2026-08-14: ArtifactCore の分割ターゲット重複が全体GPUビルドを阻害

- 関連: `ArtifactCore/CMakeLists.txt`, `src/AI/OnnxDmlLocalAgent.cppm`
- 事実: `OnnxDmlLocalAgent.cppm` が統合 `ArtifactCore` と分割 `ArtifactCoreAI` の双方のコンパイル対象になり、モジュール実装の宣言解決エラーが発生している。
- 影響: テキスト/GPU実装とは独立した既存ビルド構成の問題だが、アプリ全体の `ArtifactRender` ビルドを止める。
- 次に確認: 分割ターゲット移行時の重複ソース除去方針を設計し、全体ビルドの別マイルストーンとして扱う。

## 2026-08-14: 旧ArtifactRenderと新Diligent/Coreの混在はGPUスモークを起動直後に壊す

- 関連: `experiments/TextAnimatorLab/gpu_smoke_standalone/CMakeLists.txt`, `Artifact/ArtifactRender.lib`
- 事実: 既存のArtifactRender静的ライブラリ（2026-08-11）を複数世代のDiligent/Coreライブラリと組み合わせると、APIバージョン不一致または起動直後のアクセス違反になり、実画像が生成されない。
- 影響: GPU合否はソース修正だけでは判定できず、ArtifactRender・ArtifactCore・Diligentを同一ビルド世代で再生成する必要がある。
- 次に確認: 全体ビルドが完了した世代のライブラリだけで専用スモークを再リンクし、`image=幅x高さ saved=1`を監査の必須条件にする。

## 2026-08-14: ArtifactRenderTextSmokeは現状でもArtifactRender/全Core依存を引き込む

- 関連: `Artifact/CMakeLists.txt`、`ArtifactCore/CMakeLists.txt`
- 事実: `ArtifactRenderTextSmoke`は`ArtifactRender`にリンクし、`ArtifactRender`は全体のCore依存を通るため、コメントにある「UIなしの軽量GPUスモーク」でもArtifactCore全体のモジュール生成をスケジュールする。
- 影響: テキスト専用GPU検証のビルド時間と失敗範囲が、Particle/Audio等の無関係なCore境界に広がる。
- 次に確認: 本番Rendererからテキスト描画に必要なGPU契約・atlas upload・readbackを独立したRendererTextRuntimeへ分離できるか設計し、既存ArtifactRenderとのABI混在を避ける。
## 2026-08-14: ArtifactIRendererはテキストGPU実験の最小依存ではない
- related: Artifact/src/Render/ArtifactIRenderer.cppm, Artifact/src/Render/DiligentImmediateSubmitter.cppm, Artifact/CMakeLists.txt
- fact: ArtifactRenderTextRuntimeからPostProcess/MotionBlur/GPUTextureCacheを除外しても、ArtifactIRendererがMesh/Material/LayerBlend/RayTracing/Particle/LOD等を直接importするため、ArtifactCore全体のモジュールグラフを再び広げる
- impact: 既存IRendererをそのまま再利用する分離では、TextSmokeの高速・安定した検証目標を満たせない
- hypothesis: テキストGPU経路には、Device/Shader/CommandBuffer/Primitive2D/ glyph submit/readbackだけの専用Facadeが必要
- next: ArtifactIRendererのAPIをTextRenderContext等へ分解し、既存Renderer本体とスモーク依存を切り離す
## 2026-08-14: GPUテキスト経路の次の依存ボトルネックはImmediateSubmitter
- related: Artifact/src/Render/PrimitiveRenderer2D.cppm, Artifact/src/Render/DiligentImmediateSubmitter.cppm, Artifact/include/Render/DiligentImmediateSubmitter.ixx
- fact: PrimitiveRenderer2D::drawGlyphs は GlyphAtlasSprite packet を RenderCommandBuffer に積むだけで、GPU実行は DiligentImmediateSubmitter::submitAtlasSprite に委譲される。
- fact: DiligentImmediateSubmitter は glyph path 以外にも PrimitiveRenderer3D、ParticleRenderer、全Sprite/Rect/Line PSO群を公開・実装依存として import している。
- impact: ArtifactIRenderer を外しても、現状の Submitter をそのまま使う限り最小GPUテキストターゲットは全描画依存を再び取り込む。
- hypothesis: GlyphText/AtlasSprite のsubmit処理、必要なShaderManagerのglyph PSO、RenderCommandBufferの該当packetだけを専用Submitterへ分離すれば、Core/Renderer全体を避けた実GPU smokeを構築できる。
- next: glyph-only submitterの依存グラフと、ShaderManagerからglyph PSO生成に必要な最小シェーダー群を抽出する。
## 2026-08-14: 独立Glyph GPU経路でatlas upload後のalpha監査が必要
- related: experiments/TextAnimatorLab/artifact_text_glyph_smoke.cpp, ArtifactCore/src/Text/GlyphAtlas.cppm, Artifact/include/Render/ArtifactTextGlyphShaderSources.ixx
- fact: D3D12 device、Glyph PSO/SRB、Core GlyphAtlasの `T` rect (63x90)、quad draw、640x180 readbackまでは同一Debug出力で成功した。
- observation: readback画像は非透明の矩形として見え、文字形状のalphaマスクとして期待する結果ではない。CPU atlasのrect取得自体は `atlasRect=0,0 63x90` で成立している。
- hypothesis: QImage RGBA upload、Alpha8からRGBA8へのcoverage展開、またはshaderのalpha/blend/resource-state境界のいずれかで透明度が失われている。未検証。
- fact: 原因はスモーク側のGPU texture descriptorが1x1のまま2048x2048 atlasを渡していたことだった。descriptorをatlas実寸へ修正後、GPU alphaは `min=0 max=255` となり、readback画像でT形状を確認できた。
- next: スモークを単一Tから実際のTextLayout glyph列とカラーemojiへ拡張し、複数rect・カラー保持・アニメータ変形を同じGPU経路で検証する。
- fact: `Text Sample1 🧪` をCore GlyphAtlasから12 glyphとして生成し、D3D12 readbackで白文字とカラー試験管emojiを確認できた。カラーglyphは1件、GPU alphaは0..255。
- fact: 正式Submitter APIで `offsetRotation` / `offsetScale` / `offsetOpacity` を各Glyphへ設定し、回転した文字列とカラーemojiのreadback画像を確認した。これはタイムライン依存なしのGlyph単位GPU変形の実証になる。

## 2026-08-14: FloatColorへの汎用Variant埋め込みはカラー統合の初手にしない
- 関連: `ArtifactCore/src/Color/FloatColor.cppm`、`Artifact/src/Color/ArtifactColorScienceManager.cppm`、`Artifact/src/Effects/Rasterizer/VectorBlurEffect.cppm`
- 事実: `FloatColor` は加減乗除、補間、色変換、UIパレット、合成処理で広く使われている。一方、`SurfaceColorDescriptor` は少なくともエフェクト側で既に色の格納形式・原色・伝達関数・参照方式を表現している。
- 結論: `ColorAny` / 無制限 `std::variant` を `FloatColor` の代替として導入すると、描画内部へ型判定と変換責務が拡散する。まず `SurfaceColorDescriptor` を入力・画像バッファ境界の正規メタデータとして採用し、演算内部の `FloatColor` は当面維持する方が変更範囲と循環依存を抑えられる。
- 次に確認: ピッカー、LUT、コンポジットの各入口で、色値とdescriptorを別々に受け渡せる既存APIを棚卸しし、変換が暗黙に起きている境界から段階的に整理する。

## 2026-08-14: FloatColorPickerはHDR編集不能をUI仕様として固定している
- 関連: `ArtifactWidgets/src/Dialog/FloatColorPicker.cppm`
- 事実: RGB/HSB/HSL/明度/アルファのスライダーは全て0〜1000の範囲で、値を0〜1へ変換する。HEX表示・入力も8bit（0〜255）で、`FloatColor` に1.0超の値を保持していてもUIから編集・往復できない。
- 結論: HDR対応は `FloatColor` の型変更だけでは解決せず、ピッカーにシーン参照モード、露出表示、1.0超の数値入力、表示用HEXとの分離が必要。既存のArtifactWidgetsを変更する作業として独立して扱うべき。
- 次に確認: HDR用UIを既存ピッカーへ追加するか、通常ピッカーとシーン参照ピッカーを分離するかを設計レビューで決める。Qt QColorへの変換は表示専用境界に限定する。

## 2026-08-14: OCIOは現行本線、旧ColorManagerは未接続候補
- 関連: `Artifact/src/Color/ArtifactOCIOManager.cppm`、`Artifact/src/Color/ArtifactColorManagement.cppm`、`Artifact/src/Widgets/Render/ViewportColorPipeline.cppm`、`Artifact/src/Layer/ArtifactImageLayer.cppm`
- 事実: `ArtifactOCIOManager` は画像入力変換、ビューポート表示変換、プロジェクト保存/読込から参照されている。Artifact側の `ColorManager` は定義と自身の実装以外の呼び出し箇所が検索上確認できず、`ArtifactColorScienceManager` はカラーサイエンスパネルと旧LUT管理を保持している。
- 追加事実: `ArtifactCore/include/Color/ColorSpace.ixx` には公開 `ColorManager` API が存在するため、Artifact側実装の未使用だけを根拠にColorManager全体を削除してはならない。
- 追加事実: `ColorManager::instance()` の呼び出しは `ArtifactCore` / `Artifact` / `ArtifactPr` のソース検索で見つからず、Core側の公開宣言とArtifact側の実装が残っている。一方、レンダリング契約など複数のインターフェースが `Color.ColorSpace` をimportしているため、モジュール境界そのものは依存されている。
- 結論: 3系統を同列に統合するのではなく、まずOCIOを現行の正規経路として明文化する。Artifact側の旧実装を整理する場合も、ArtifactCoreの公開ColorManagerとの互換境界を先に定義する。
- 次に確認: 削除ではなく、`ColorManager` のAPIを互換層として残し、実装をOCIO設定・変換サービスへ委譲できるかを設計する。`ArtifactColorScienceManager` のLUT責務はOCIO設定・ビュー変換責務と分離して整理する。

## 2026-08-14: ColorLUTの既存CPU経路はHDRを明示的に失う
- 関連: `ArtifactCore/src/Color/ColorLUT.cppm`、`ArtifactCore/include/Color/ColorLUT.ixx`
- 事実: `ColorLUT::apply(float&, float&, float&)` は入力と補間結果を0〜1へクランプする。`applyToImage()` は入力を `QImage::Format_ARGB32` に変換し、8bit RGBAへ書き戻す。
- 結論: HDR対応はピッカーだけでなくLUT適用経路にも必要。既存のQImage APIの意味を変えず、F32画像／バッファ向けにHDR値を保持する別APIを追加し、表示用変換とシーン値のLUT適用を分離するのが安全。
- 次に確認: `ImageF32x4_RGBA` または既存のF32バッファ型へLUTを適用する境界を確認し、クランプが必要なのはLUTサンプル座標だけか、出力値もクランプする仕様かを決める。

## 2026-08-14: 3D回転のX/Y/Z項目は現状モデルへ保存されていない
- 関連: `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`、`ArtifactCore/src/Animation/AnimatableTransform3D.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- 事実: `ArtifactAbstractLayer::setRotation3D(QVector3D)` は `rot.x()` だけを `AnimatableTransform3D::setRotation()` に渡す。`AnimatableTransform3D` の公開setterも単一の `float degrees` で、JSON/UIには `rotationX/Y/Z` が現れる箇所があるが、内部のアニメーション値と評価経路は単一角度。
- 結論: 3D回転対応はUI項目の追加ではなく、X/Y/Z各軸のアニメーション値、シリアライズ、補間、描画行列を一貫して拡張するモデル変更。既存の`rotation`をZ軸互換として扱う移行仕様が必要。
- 次に確認: 3Dレイヤーの描画行列生成箇所と、既存JSONの`rotation`/`rotationX/Y/Z`読込優先順位を棚卸しし、互換変換を先に定義する。
- 追加事実: `Artifact3DModelLayer.cppm` と `ArtifactProcedural3DLayer.cppm` はいずれも `QMatrix4x4::rotate(angle, 0, 0, 1)` を使う。共通の `ArtifactAbstractLayer::getLocalTransform4x4()` も現状は単一回転前提で、個別3Dレイヤーだけを修正しても2D/3D共通変換や親子変換との整合を失う。
- 次に確認: まず共通のローカル行列生成をEuler順序またはQuaternionに置き換える設計を決め、その後に3Dモデル・Procedural3D・Gizmo・Undoの各経路を同じ回転値へ接続する。

## 2026-08-14: Transform3Dの通常行列はZ位置を落としている
- 関連: `ArtifactCore/src/Animation/AnimatableTransform3D.cppm`
- 事実: `getMatrix()` のtranslationは `(currentX_, currentY_, 0.0f)` を使う一方、`getAllMatrix()` は `(currentX_, currentY_, currentZ_)` を使う。`getMatrixAt()` もZ位置を0固定で生成する。
- 懸念: 3Dレイヤーの評価経路によってZ位置が反映されたり失われたりする可能性がある。3軸回転の実装前に、`getMatrix` / `getAllMatrix` / `getMatrixAt` / `getAllMatrixAt` の責務とZ位置の扱いを統一する必要がある。
- 追加事実: `getMatrixAt()` はアニメーションのoffset値（`x_`/`y_`/`z_`、`scaleX_`等）を直接行列へ入れる一方、`getAllMatrixAt()` はinitial値との合成値を使う。`getMatrix()` と `getMatrixAt()` の責務差はコードコメントだけでは明確でなく、3D化時に初期値・offset値の合成規約を確定する必要がある。

## 2026-08-14: バッチ再リンクは既存relink APIの単純拡張では足りない
- 関連: `Artifact/include/Service/ArtifactProjectService.ixx`、`Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- 事実: 現在は `relinkFootageByPath(old,new)` と `relinkFootageItems(items,new)` があり、Asset Browserには単一アセットのUndo付き再リンク導線がある。旧パスから候補ファイルを探索するbasename、相対パス、サイズ、mtime、ハッシュ等の解決器は見当たらない。
- 結論: バッチ再リンクは既存APIにループを足すだけでは不十分。候補探索結果、曖昧候補、連番グループ、全参照更新、Undo単位をまとめる専用サービス境界が必要。
- 次に確認: まず候補探索を副作用なしの `RelinkCandidateResolver` として定義し、確定後に既存の `relinkFootageByPath` を呼ぶ二段階構成にする。自動確定ではなく候補提示を初期仕様とする。
- 追加事実: `relinkFootage()` の現行実装は `FootageItem::filePath` と連番の `sequencePaths` を更新するが、`image.sourcePath` / `video.sourcePath` / `audio.sourcePath` 等のレイヤー側パスを同じ操作内で更新していない。
- 懸念: レイヤーがFootageItemを参照して再解決する経路が別に存在する可能性はあるが、コード上は再リンク直後の全参照伝播が保証されていない。なお、再確認により `ArtifactAbstractComposition::allLayer()` / `allLayerRef()` は既に公開されていることが判明したため、先の「全レイヤー列挙APIがない」という見立ては誤り。候補探索より先に、AssetDatabase・FootageItem・レイヤーsourcePathの正規参照関係を確認する必要がある。
- 追加事実: Asset Browserの `RelinkAssetCommand` は `relinkFootageByPath()` だけをredo/undoしている。
- 懸念: relinkFootage内でレイヤーsourcePathまで直接変更すると、既存UndoがFootageItemのパスしか戻さず、レイヤー参照だけが取り残される。伝播を実装する場合はFootageItem変更と全レイヤー変更を同一Undoコマンドにまとめる必要がある。

## 2026-08-14: QPA障害ではなくZWJ描画単位の未接続が残る
- 関連: `experiments/TextAnimatorLab/run_gpu_smoke.ps1`、`experiments/TextAnimatorLab/artifact_text_glyph_smoke.cpp`、`ArtifactCore/src/Text/TextShapingBackend.cppm`
- 事実: Debug QPAを明示してRTX 4070 Ti / D3D12上で `Text1`、CJK、`👩‍💻` を同一GPUスモークへ通せる。通常文字とCJKは描画できるが、ZWJは現行のQt glyph列生成で3 glyph（カラー2件）として出力される。
- 結論: QPA探索とGPU起動は解決済み。ZWJ・variation selector・modifierを単一の描画／アニメーション単位として扱うには、Unicode grapheme契約とglyph atlasのsequence rasterization境界を一致させる必要がある。部分表示を成功扱いにせず、sequence対応を独立した完了条件にする。
- 次に確認: `GlyphKey` / `GlyphAtlas::acquire()` が単一code point前提のため、sequence keyとDirectWrite color glyph runの合成結果をキャッシュできる最小APIを設計する。
- 追加事実: DirectWriteへsequence全体のglyph配列を試験的に渡すと、現状のQt由来glyph列とは位置・合字結果が一致せず、同じsequence画像を複数回描画する危険がある。Submitterは現在、ZWJ/variation selectorをスキップし、scalar color glyphを明示的な暫定フォールバックとして使う。
- 結論: sequence rasterizerを有効化するには、DirectWriteのshape結果（glyph index、原点、advance、run bounds）をCore layoutへ戻し、Submitterが1 cluster 1 quadを生成する契約まで一体で検証する必要がある。単に`sequenceUtf8`をキーへ渡すだけでは製品品質にならない。
- 追加事実: Qt `QGlyphRun` のstring indexを使ってshaped glyph indexをCore `GlyphItem`へ保持し、DirectWriteへglyph index 1623を渡すと、`👩‍💻`の合成済みcolor glyphを実GPUで1描画単位としてreadbackできた。Qtが返さない継続codepointはSubmitterでスキップする。
- 更新結論: sequence対応の最小実装は「Coreのshape結果を捨てず、Atlas keyにshaped glyph indexを含める」ことで成立する。複数run、異なるfont fallback、modifier sequenceは引き続き追加ケースとして検証が必要。
- 追加事実: `GlyphItem.shapedGlyphIndices` を追加し、同一cluster内のshaped glyph indexをCoreで集約した。家族絵文字は実行時に1 cluster / 4 shaped glyphとして取得できる。
- 残課題: Submitter/Atlasはまだscalar indexを描画単位にしているため、配列契約は接続済みだが家族clusterの合成画像化は未完了。配列全体のDirectWrite run rasterizationと、cluster bounds/advanceの伝搬が次の実装境界。
- 追加事実: `GlyphKey.shapedGlyphIndices`へcluster配列を伝搬し、Submitterがcluster先頭だけをAtlasへ渡す経路を実装した。家族絵文字の4 glyphはDirectWriteの1 runとして処理されるが、run boundsの左端／レイヤー境界の扱いによりreadback画像にclipが残る。
- 次に確認: DirectWrite color runの各layer boundsをglyph runの原点へ戻す座標変換を検証し、union boundsのminX/minYをbearingとして保持する。単純にscalar QRawFont boundingRectへ置換するだけでは不十分。
- 追加検証: 家族clusterをx=120へ移動して実GPU描画したところ、合成run画像は欠けずに表示できた。先の左端clipはAtlas union boundsではなく、Smokeのx=0付近で回転したquadが画面端で切れた結果だった。実アプリではcluster boundsを考慮した安全な画面配置／自動フレーム内判定が別途必要。
- 追加事実: 前後文字を含む複合Smokeでは、通常glyphのAtlas rectはvalidでもGPU readbackから消える。`A B`だけでも再現するため、家族cluster固有ではない。単独`Text1`との差分は、現行Submitterの複数glyph／変形描画状態にある可能性が高い。
- 次に確認: rotation/scale/opacityを無効にした同一Submitter試験と、1 draw callに全quadをまとめる方式を比較し、DrawAttribsのvertex offsetまたは変形後座標の問題を分離する。
- 原因確定: 複合ケースで通常文字が消えた原因はGPU draw状態ではなく、`QImage::Format_Alpha8`をGrayscale8へ変換してcoverageを読んでいたことだった。Alpha8はalpha channelを直接読む必要があり、元の分岐へ戻すと無変形`A B`および`A 👨‍👩‍👧‍👦 B`が実GPUで復旧した。
- 追加原因確定: 前後Latin文脈で家族emojiが一部になったのは、`QGlyphRun`の重複したcluster先頭string indexをfallbackが行頭0から割り当てていたため。run内の有効なstring indexをfallback開始位置に使うと、`A 👨‍👩‍👧‍👦 B`でA・家族emoji・Bの全てを実GPU表示できた。

## 2026-08-14: 3D回転モデルと連番再リンクの実装反映
- 関連: `ArtifactCore/src/Animation/AnimatableTransform3D.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- 事実: `AnimatableTransform3D` にX/Yの独立値・キーフレーム評価を追加し、既存 `rotation` をZ軸互換として共通行列、スナップショット、保存／再読込、3Dモデル、Procedural3D、ギズモ、Undoへ接続した。Euler適用順序はZ→Y→X。
- 事実: Asset Browserの複数選択再リンクは候補を素材ごとに確認してから一括適用し、途中失敗時にロールバックする複合Undoを持つ。同一連番の複数フレーム選択はFootageItem単位へ正規化した。
- 懸念: いずれもビルド・実行検証は未実施。旧 `rotation` と新X/Y/Zの初期値・offset合成、およびレイヤー固有プロパティUIの3軸編集契約は引き続き確認が必要。

## 2026-08-14: 再リンク参照一致は正規化絶対パスで行う
- 関連: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- 事実: FootageItemの絶対パスとレイヤーJSONのsourcePathは、相対表記や区切り文字の違いを含み得る。
- 結論: バッチ再リンクのsourcePath伝播では生文字列比較を避け、`QFileInfo(...).absoluteFilePath()` と `QDir::cleanPath()` を通した比較を使う。
- 次に確認: 大文字小文字の扱いはOS依存のため、Windows上のケース差を含む実行検証が必要。

## 2026-08-14: バッチ再リンク候補には参照数を併記する
- 関連: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- 事実: 候補選択前に全コンポジションのレイヤーJSONを走査し、旧パスを参照するレイヤー数を集計できる。
- 結論: 候補のスコア・理由だけでなく参照数も表示し、影響範囲を確認してから確定できるようにした。候補探索と参照集計は適用前に実行されるため、副作用はない。

## 2026-08-14: AssetDatabaseにも再リンクのID維持移行が必要
- 関連: `ArtifactCore/include/Asset/AssetDatabase.ixx`、`ArtifactCore/src/Asset/AssetDatabase.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`
- 事実: `AssetManager::acquireSource(newPath, ...)` は新しいAssetDatabase登録を作れるが、旧パスのAssetInfoを自動移行・削除するAPIは存在しなかった。
- 対応: Asset IDを維持したままpathToIdとAssetInfoのパスを移す `relinkAssetPath()` を追加し、連番は全フレームの移行に失敗した場合に逆順ロールバックする。
- 未検証: 実プロジェクトでのAssetDatabase永続化、既存newPath衝突、ビルド・実行挙動。

## 2026-08-14: RAMプレビューは二経路が存在し、PlaybackService側は既に先読み接続済み
- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`、`Artifact/src/Render/ArtifactRamPreviewController.cppm`、`docs/analysis/AE_PAIN_POINT_IMPROVEMENT_MAP_2026-08-13.md`
- 事実: `ArtifactPlaybackService` はRAMキャッシュ、周辺フレーム先読み、世代番号による要求キャンセル、進捗／ヒット率、再生開始をキャッシュ準備で待たせない経路を持つ。複数のWidgetもPlaybackServiceの状態を参照している。
- 事実: `ArtifactProjectService::setPreviewQualityPreset()` 内には旧 `progressiveRenderer_` 呼び出しのコメントが残るが、実際の `CompositionRenderController::setPreviewQualityPreset()` は Draft/Preview/Final を 4/2/1 倍の downsample に変換し、品質変更時にRAM preview cacheをinvalidateして再描画を要求する。
- 事実: 別の `ArtifactRamPreviewController::startBuild()` はレンダーコールバックを同一スレッドのwhileループで処理する。CMakeには登録されているが、現状のアプリ実行コードからの利用箇所は確認できず、既存の階層キャッシュ計画でもlegacy initial controller扱いになっている。
- 結論: 改善の主眼は新しいRAMプレビュー機構を追加することではなく、PlaybackServiceを正規経路として二経路を整理し、旧Controllerには新機能を追加せず、PlaybackService側の実際の非同期性と品質プリセットを検証すること。
- 未検証: ビルド・実行時に旧Controllerがリンク対象／外部利用されていないこと、およびPlaybackServiceのフレーム生成がUIスレッドを長時間ブロックしないこと。

## 2026-08-14: 連番再リンクでは同一フレームのAssetDatabase移行を無操作成功にする
- 関連: `Artifact/src/Service/ArtifactProjectService.cppm`、`ArtifactCore/src/Asset/AssetDatabase.cppm`
- 事実: 連番再リンクでは、移行先の一部フレームが既存パスと同一になることがある。`AssetDatabase::relinkAssetPath()` は同一パスを拒否するため、移行不要なフレームまで失敗扱いにすると全体ロールバックへ入る。
- 対応: `ArtifactProjectService::relinkFootage()` の移行ヘルパーで正規化絶対パスが同一の場合は成功扱いにし、AssetDatabase APIを呼ばずに続行する。
- 未検証: 混在した連番の実プロジェクトでのAsset ID維持、衝突時ロールバック、ビルド・実行挙動。

## 2026-08-14: 再リンク同一判定はAssetDatabaseと同じWindows大小文字規則が必要
- 関連: `Artifact/src/Service/ArtifactProjectService.cppm`、`ArtifactCore/src/Asset/AssetDatabase.cppm`
- 事実: `AssetDatabase::normalizedAssetPath()` はWindowsでcase foldingを行うが、再リンクサービス側の同一パス判定は当初 `cleanPath` のみだった。
- 対応: 再リンク移行ヘルパーでもWindowsではcase foldingしてから同一パスを無操作成功と判定するようにした。
- 未検証: Windows上で大文字小文字だけ異なる既存連番のAsset ID維持とロールバック。

## 2026-08-14: 再リンク移行の同一判定はcanonical pathを優先する
- 関連: `Artifact/src/Service/ArtifactProjectService.cppm`、`ArtifactCore/src/Asset/AssetDatabase.cppm`
- 事実: `AssetDatabase` は実在ファイルのcanonical pathをAsset identityに使うが、サービス側の移行前比較はabsolute pathだけだった。
- 対応: サービス側の移行ヘルパーもcanonical path、空の場合はabsolute path、clean path、Windows case foldingの順に正規化するようにした。
- 未検証: シンボリックリンクを含む連番のAsset ID維持と、移行失敗時の逆順ロールバック。

## 2026-08-14: 再リンク検索入口も同一のcanonical path正規化へ統一
- 関連: `Artifact/src/Service/ArtifactProjectService.cppm`
- 事実: `findFootageItemByPath()` と `relinkFootageByPath()` は移行ヘルパーとは別にabsolute path比較を持っていたため、symlink・Windows大小文字差で対象FootageItemを見失う余地があった。
- 対応: 匿名名前空間の `normalizeRelinkPath()` を追加し、検索・同一判定・AssetDatabase移行前判定で共有するようにした。
- 未検証: 実ファイルのsymlink、Windowsケース差、連番の混在パスを含む検索から移行までの実行確認。

## 2026-08-14: AI操作の初期ハンドシェイクを契約化
- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx`、`design/user-personas/api-agent.md`
- 事実: WorkspaceAutomationにはスナップショット、コマンド検証、dry-run、監査ログ、診断が既に存在するが、AIが起動直後に安全な利用順序と必須レスポンス項目を一括取得する入口はなかった。
- 対応: `agentContract()` を追加し、発見・安全実行順序・観測・高リスク操作・失敗レスポンス項目・運用原則を機械可読な `QVariantMap` で返すようにした。
- 未検証: 実行時の登録経路から `agentContract` を呼び出せること、外部AIクライアントが契約情報を利用すること、ビルド・実行挙動。

## 2026-08-14: AI操作契約を共通システムプロンプトにも反映
- 関連: `ArtifactCore/include/AI/AIPromptGenerator.ixx`
- 事実: `AIPromptGenerator` はCore層にあり、Artifact層のWorkspaceAutomationを直接importできない。一方、全AIバックエンドが共通の操作方針を受け取る入口になっている。
- 対応: 状態観測、安定ID解決、validateCommand、preview/dry-run、明示確認、実行後再観測、失敗情報保持の順序を日本語・英語のシステムプロンプトへ追加した。
- 未検証: 各バックエンドが生成済みシステムプロンプトを実際に使用すること、ビルド・実行挙動。

## 2026-08-14: クラウドAIへ実行時のエージェント契約を注入
- 関連: `Artifact/src/AI/AIClient.cppm`、`Artifact/include/AI/WorkspaceAutomation.ixx`
- 事実: クラウドチャットは共通システムプロンプトとツールスキーマを使用するが、契約の具体的なバージョン・観測メソッド・安全メソッドはプロンプトに含まれていなかった。
- 対応: `agentContract()` の現在値をCompact JSON化し、クラウドAIのシステムプロンプトへ追加した。
- 未検証: QVariantからJSONへの変換結果、各クラウドプロバイダのプロンプト受け渡し、ビルド・実行挙動。

## 2026-08-14: AI起動時の読み取りをagentPreflightへ集約
- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx`、`design/user-personas/api-agent.md`
- 事実: AIは契約、現在状態、診断を別々に取得すると、呼び出し順や一部取得漏れを起こしやすい。
- 対応: 読み取り専用の `agentPreflight()` を追加し、契約・workspace snapshot・diagnosticsを一括返却するようにした。
- 未検証: 実行時のJSONシリアライズ、AIクライアント側での自動利用、ビルド・実行挙動。

## 2026-08-14: AI契約とpreflightをツールブリッジ検査へ固定
- 関連: `Artifact/src/Test/ArtifactTestAIToolBridge.cppm`
- 事実: AI向けメソッドはツールスキーマへ登録されるため、登録漏れや返却形状の退行は起動後まで見つからない可能性がある。
- 対応: `agentContract` / `agentPreflight` のスキーマ登録、契約バージョン、読み取り専用フラグ、主要返却項目を既存のAIツールブリッジテストで検査するようにした。
- 未検証: テストの実行結果、ビルド・実行挙動。

## 2026-08-14: AI APIリファレンスにpreflight契約を公開
- 関連: `Artifact/docs/AI_API_EXTENDED_REFERENCE.md`、`Artifact/docs/AI_API_CLOUD_WIDGET_NOTES.md`
- 事実: 実装とテストに追加したagentContract / agentPreflightが、既存のAI APIリファレンスには記載されていなかった。
- 対応: 起動時の推奨呼び出し順、読み取り専用preflight、検証・確認・実行後観測の契約を公開ドキュメントへ追記した。
- 未検証: ドキュメントからのサンプルJSONが各外部クライアントでそのまま解釈されること、ビルド・実行挙動。

## 2026-08-14: クラウドツール実行後にpreflightを再観測
- 関連: `Artifact/src/AI/AIClient.cppm`
- 事実: クラウドのツールループは実行結果のtraceを次の応答へ渡していたが、変更後のworkspace状態を同じ応答に含めていなかった。
- 対応: ツール呼び出し成功直後に`agentPreflight()`を読み取り、`post_tool_preflight`として次のAI応答へ渡すようにした。
- 未検証: ツール実行後のsnapshot内容、長いpreflight JSONによるコンテキスト増加、ビルド・実行挙動。

## 2026-08-14: 共通プロンプトでagentPreflightの発見性を明示
- 関連: `ArtifactCore/include/AI/AIPromptGenerator.ixx`
- 事実: 共通プロンプトは安全な観測順序を説明していたが、ローカルAIが具体的な一括入口を選ぶにはメソッド名の手掛かりが不足していた。
- 対応: WorkspaceAutomation利用時は`agentPreflight()`を最初の読み取りハンドシェイクとして優先する指示を日本語・英語へ追加した。
- 未検証: 各ローカルモデルがこの優先順位を守ること、ビルド・実行挙動。

## 2026-08-14: AI Cloud Widgetの実行経路にもpost-tool観測を追加
- 関連: `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`
- 事実: AIClientのクラウドループとは別に、Cloud Widgetが承認付きでツールを直接実行する経路を持っていた。
- 対応: 承認済みツール実行の結果へ`post_tool_preflight`を付加し、UI経由でも次のAI応答が変更後状態を観測できるようにした。
- 未検証: MCP外部ツールを含む場合のpreflight適用範囲、ビルド・実行挙動。

## 2026-08-14: Python Workspace APIへagentPreflightを公開
- 関連: `Artifact/src/Script/ArtifactPythonHookManager.cppm`、`Artifact/docs/AI_API_EXTENDED_REFERENCE.md`
- 事実: Python bridgeにはworkspaceSnapshotや各種編集操作が登録されていたが、AIエージェント向けの契約・状態・診断の一括取得入口がなかった。
- 対応: `artifact.workspace.agentPreflight()` を追加し、C++側と同じcompact JSONを返すようにした。
- 未検証: PythonEngine初期化後の関数登録、JSON受け渡し、ビルド・実行挙動。

## 2026-08-14: WorkspaceAutomationの説明文にAI安全入口を明示
- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx`
- 事実: `agentPreflight` はスキーマ登録されていても、コンポーネント詳細説明だけを読むクライアントには優先入口として伝わらなかった。
- 対応: 詳細説明に、読み取り専用preflight、書き込み検証、完了前の再観測を明記した。
- 未検証: 各クライアントが詳細説明を表示・利用すること、ビルド・実行挙動。

## 2026-08-14: agentContractにPython代替入口を記載
- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx`、`design/user-personas/api-agent.md`
- 事実: C++のWorkspaceAutomationとPythonの`artifact.workspace`は同じpreflightを提供するが、契約情報にはPython名がなかった。
- 対応: `alternateEntryPoints.python` に `artifact.workspace.agentPreflight` を追加し、ペルソナ文書にも併記した。
- 未検証: PythonEngine未初期化時の利用可否、ビルド・実行挙動。

## 2026-08-14: agentPreflightに観測時刻を付加
- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/src/Test/ArtifactTestAIToolBridge.cppm`
- 事実: preflightはworkspace・診断・契約をまとめて返していたが、AIが結果の新しさを判定する時刻情報がなかった。
- 対応: `observedAtUtc` をISO 8601 millisecond形式で追加し、ブリッジテストでも空でないことを検査するようにした。
- 未検証: 長時間処理中のsnapshotと実際の編集時刻の差、ビルド・実行挙動。

## 2026-08-15: 直近レンダリング調査レポートの妥当性確認
- 関連: `docs/analysis/BATCH_RENDER_FAILURE_2026-08-13.md`、`docs/analysis/IMAGE_BUFFER_PRECISION_AUDIT_2026-08-13.md`、`docs/analysis/OCCLUSION_CULLING_IMPLEMENTATION_MEMO_2026-08-13.md`、`docs/analysis/ADVANCED_RENDERING_GAP_2026-08-13.md`
- 事実: `useMfr = false`、フレーム全体を覆う `compositionFrameStateMutex_`、`ArtifactBatchRenderer` の未初期化設定、RT の BLAS no-op、RenderGraph の診断専用経路など、主要な指摘は一次ソース上で確認できた。
- 判断: レポートは概ね妥当。ただし、並列レンダー・float/HDR 化・Hi-Z・RenderGraph実行化はいずれも子リポジトリの広範な変更を伴い、現時点で一括実装すべき単一修正ではない。
- 次に確認すべきこと: ユーザーが対象サブモジュールと優先順位を明示した後、最小の縦切り（まずバッチ設定バグ修正、または性能基盤の設計分離）を選定する。ビルド・実行検証は別途許可が必要。

## 2026-08-15: 既存フレームパス実装と共有RenderGraphの接続点
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`ArtifactCore/include/Graphics/RenderGraph.ixx`
- 事実: Composition 側には `FunctionalRenderPass` / `RenderPassExecutor` による既存の段階的パス実行があり、RenderGraph は診断グラフだけでなく、フレームパス順序の検証・共有スケジューラとして段階導入できる。
- 対応: `renderOneFrameImpl` のフレームパス計画から共有 RenderGraph を構築し、依存チェーンの compile 検証を追加した。既存 executor の資源所有・実行は維持している。
- 未検証: RenderGraph executor から実 GPU パスを直接駆動した場合のリソース状態遷移、実行時間、runtime 表示。

## 2026-08-15: レイヤー縦切りをRenderGraph executorへ移行
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 対応: Layer Raster → Mask / Track Matte → Blend の3パスについて、既存 `FunctionalRenderPass` を共有 `RenderGraph::execute()` の executor から実行する `runAllWithRenderGraph()` を追加した。
- 未検証: 1レイヤーごとの graph compile コスト、GPU resource barrier の実装、複数レイヤー間での transient resource aliasing。

## 2026-08-15: RTウォームアップをTLAS参照経路へ拡張
- 関連: `ArtifactCore/src/Graphics/RayTracingManager.cppm`
- 対応: 固定色だけを書いていた ray-generation shader に TLAS、TraceRay、miss、triangle closest-hit を追加し、PSO/SBT に hit group と TLAS binding を登録した。
- 未検証: 実メッシュ登録後の DXR/Vulkan runtime shader compilation、空 TLAS での TraceRay、GPU 出力のヒット色。

## 2026-08-15: RT登録対象を不透明メッシュに限定
- 関連: `Artifact/src/Render/ArtifactIRenderer.cppm`
- 対応: BLAS登録条件に実効 opacity、base color alpha、opacity texture の判定を追加し、透明メッシュを RT の不透明ジオメトリ経路へ登録しないようにした。
- 未検証: 同一 geometry の複数 instance 管理、透明化／不透明化がフレーム中に切り替わる場合の BLAS/TLAS 更新。

## 2026-08-15: RenderGraph transient allocation slot 計画
- 関連: `ArtifactCore/include/Graphics/RenderGraph.ixx`
- 対応: compile 結果に各 resource の生存区間と allocation slot を付加し、区間が重ならない transient resource を同一 slot に割り当てる greedy aliasing 計画を追加した。External/Persistent resource は再利用対象外とした。
- 未検証: Diligent texture/buffer 実体への slot 適用、フォーマット・サイズ互換性を考慮した aliasing、backend barrier との連携。

## 2026-08-15: RenderGraph aliasing 予算を診断へ公開
- 関連: `ArtifactCore/include/Graphics/RenderGraph.ixx`
- 対応: compiled graph に allocation slot 数を追加し、diagnostic snapshot に論理 resource 総量とは別の alias 後推定 byte 数を追加した。
- 未検証: 実 backend allocation との差、アライメント・メモリ heap 制約、FrameDebug JSON への表示統合。

## 2026-08-15: aliasing メモリ見積もりを FrameDebug に公開
- 関連: `ArtifactCore/include/Frame/FrameDebug.ixx`、`Artifact/src/Widgets/Diagnostics/FramePipelineViewWidget.cppm`
- 対応: `estimatedAliasedResourceBytes` と resource の `allocationSlot` を JSON 往復・診断表示へ追加した。
- 未検証: 実 GPU allocation との差、古い capture JSON との表示互換性、UI上の長文レイアウト。

## 2026-08-15: RenderGraph executorへcompiled allocation計画を伝播
- 関連: `ArtifactCore/include/Graphics/RenderGraph.ixx`
- 対応: `RenderGraphExecutionContext` に `CompiledRenderGraph` を追加し、executor が resource lifetime と allocation slot を参照できるようにした。handle から lifetime を引く accessor も追加した。
- 未検証: backend allocator が実際に slot を使う実装、pass間の resource state transition、executor callback の runtime 性能。

## 2026-08-15: compiled graph に allocation slot descriptor を追加
- 関連: `ArtifactCore/include/Graphics/RenderGraph.ixx`
- 対応: slot ごとに resource 種別、寸法、format、最大 byteSize を保持する `RenderAllocationSlotDescriptor` と accessor を追加した。executor は slot descriptor を参照して backend resource を確保できる。
- 未検証: Diligent の実 texture/buffer pool 実装、heap alignment、alias slot の state transition。

## 2026-08-15: allocation slot descriptor を FrameDebug 往復化
- 関連: `ArtifactCore/include/Graphics/RenderGraph.ixx`、`ArtifactCore/include/Frame/FrameDebug.ixx`、`Artifact/src/Widgets/Diagnostics/FramePipelineViewWidget.cppm`
- 対応: compiled graph の slot descriptor 一覧を diagnostic snapshot、JSON 往復、Frame Pipeline の slot 数表示へ追加した。
- 未検証: slot 個別の UI 詳細表示、実 backend pool と descriptor の一致、旧 capture の migration 表示。
- 対応: Frame Pipeline に各 allocation slot の種別・寸法・format・byteSize の詳細行を追加した。

## 2026-08-15: MeshRenderer の BLAS buffer bind 不整合を修正
- 関連: `ArtifactCore/src/Graphics/MeshRenderer.cppm`、`ArtifactCore/src/Graphics/RayTracingManager.cppm`
- 事実: MeshRenderer の position/index buffer は vertex/index bind のみで作成されていたが、Diligent の BLAS build source buffer には `BIND_RAY_TRACING` が必要だった。
- 対応: position/index buffer に `BIND_RAY_TRACING` を追加し、buffer pointer または geometry 数が変わった場合は既存 BLAS を再生成するようにした。
- 未検証: 実デバイスの BLAS build 成功、同一 geometry の複数 instance、buffer rebuild 中の GPU lifetime。
- 追記: `BIND_RAY_TRACING` は Ray Tracing 対応デバイスでのみ付与し、非対応デバイスの通常メッシュ作成を維持する。
- 対応: MeshRenderer の RT 対応判定を RayTracingManager と同じ feature state + `STANDALONE_SHADERS` capability 判定へ統一した。
- 対応: TLAS に `ALLOW_UPDATE` を付け、同一 instance 数のフレーム更新では update scratch size を使った TLAS update を選択する。instance 数が変わる場合は full build に戻す。
- 対応: BLAS/TLAS scratch buffer・instance buffer の生成失敗と TLAS 最大 instance 数超過を build 前に拒否する。
- 対応: BLAS 登録時に vertex/index buffer の `BIND_RAY_TRACING` を検査し、診断カウンタの BLAS build 数を実 build 数単位に修正した。
- 対応: BLAS ごとの dirty 状態を追加し、geometry layout が変わった BLAS だけを再構築するようにした。transform 更新時は BLAS build を省略し TLAS update へ進める。
- 対応: `updateInstanceTransform()` / `hasBLAS()` を追加し、geometry が同じでも transform 変更時だけ TLAS update を発行するようにした。透明状態から不透明状態へ戻るメッシュも再登録できる。
- 対応: 不透明でない mesh instance は TLAS mask=0 で無効化し、再び不透明になった際は transform update 経由で再有効化する。
- 対応: `RayTracingCapabilities` に登録 BLAS 数、有効 instance 数、直近 build 成否を追加し、初期化ログへ出力した。
- 対応: TLAS が未構築の初期化段階では `traceUnitQuad()` が TraceRays を発行しないようにした。

## 2026-08-15: BLAS/TLAS 静的整合性監査
- 関連: `ArtifactCore/include/Graphics/RayTracingManager.ixx`、`ArtifactCore/src/Graphics/RayTracingManager.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`
- 事実: 新規 pure virtual API の実装は `RayTracingManager` に集約され、呼び出し側も ArtifactIRenderer のみだった。
- 対応: BLAS 登録数を有効な BLAS 実体数として数えるよう修正し、TLAS build 失敗時の `lastBuildSucceeded` を必ず false に戻すようにした。TLAS scratch の build/update 最大サイズ判定も統一した。
- 未検証: コンパイラによる C++20 module 整合性、Diligent 実デバイス上の BLAS/TLAS build、複数 instance の表現。

## 2026-08-15: 現行 mesh 呼び出しの RT 識別子確認
- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`、`Artifact/src/Layer/ArtifactProcedural3DLayer.cppm`、`Artifact/include/Render/ArtifactIRenderer.ixx`
- 事実: `drawMesh()` の `cacheKey` は通常の 3D モデルでは source path と layer ID、procedural mesh では layer ID から生成されるため、現行のレイヤー描画単位では TLAS instance 識別子として機能する。
- 判断: 直ちに別の instance map を導入する必要はない。将来、同一 layer が 1 frame 内で複数回描画される機能を追加する場合は、`drawMesh()` API に明示的な instance ID を導入する。
- 未検証: 実行時に同一 layer が複数回 submit される特殊経路の有無。

## 2026-08-15: Diligent RT API 参照照合
- 関連: `ArtifactCore/src/Graphics/RayTracingManager.cppm`、`libs/DiligentEngine/DiligentSamples/Tutorials/Tutorial22_HybridRendering/src/Tutorial22_HybridRendering.cpp`
- 事実: BLAS/TLAS の source buffer に `BIND_RAY_TRACING` を付与すること、scratch / instance buffer の用途、`BuildBLASAttribs`・`BuildTLASAttribs` の主要フィールド、transform の設定方法は Diligent の公式サンプルと一致している。
- 未検証: Artifact の C++20 module コンパイル、使用 GPU backend 固有の RT shader / SBT 制約、実フレームでの API 呼び出し順。

## 2026-08-15: RAM preview も RenderGraph executor 経由へ移行
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 対応: RAM preview の Base / Composite 2-pass 実行を、既存の `RenderPassExecutor::runAllWithRenderGraph()` に統一した。GPU pipeline の主要 layer 3-pass に加え、fallback branch でも compiled pass order と executor failure propagation を通す。
- 未検証: 実フレームの pass resource state transition、GPU pipeline 全体の各 pass を RenderGraph へ置き換える作業。

## 2026-08-15: Composition の単一 pass 実行も RenderGraph 経由へ統一
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 対応: Setup、GPU Base、Resolve、RAM/direct fallback、Overlay、Present の単一 pass 実行に `runWithRenderGraph()` を導入した。複数 pass の layer 実行と合わせ、旧 `RenderPassExecutor::run()` の直接呼び出しを除去した。
- 未検証: RenderGraph が実 GPU resource allocation や state barrier を所有する段階への移行、実フレームの描画結果。

## 2026-08-15: フレーム診断グラフの resource 見積りを実寸化
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 対応: フレーム pass plan の診断用 resource をゼロ寸法の Buffer から viewport 寸法の Texture へ変更し、RGBA8 相当の byteSize を設定した。allocation slot の alias 見積りが実際の画面サイズを反映する。
- 未検証: 実 backend の format mapping、MSAA / HDR / AOV ごとの実際の resource 分割、GPU allocation との一致。

## 2026-08-15: RenderGraph executor に graph 本体を公開
- 関連: `ArtifactCore/include/Graphics/RenderGraph.ixx`
- 対応: `RenderGraphExecutionContext` に `const RenderGraph& graph` を追加した。pass executor は compiled graph の allocation slot だけでなく、resource descriptor を handle から解決できるため、将来の backend allocator / barrier adapter を context から接続できる。
- 未検証: 実 backend 側の allocator 実装、resource state transition、context ABI 変更の module build。

## 2026-08-15: RT pipeline resource variable 数の不整合修正
- 関連: `ArtifactCore/src/Graphics/RayTracingManager.cppm`
- 事実: RT warmup PSO の `Variables` 配列には `g_OutputTex` と `g_TLAS` の2項目があったが、`NumVariables` が1だった。
- 対応: `NumVariables = 2` に修正し、TLAS static resource variable が resource layout に含まれるようにした。
- 未検証: Diligent PSO 作成、static binding、SBT / TraceRays の実 backend 動作。

## 2026-08-15: RenderGraph executor 移行時の null pass 防御を維持
- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 対応: `runAllWithRenderGraph()` の callback に null pass 検査を追加した。旧 `runAll()` と同様、無効な pass pointer を dereference せず失敗伝播する。
- 未検証: 実フレームでの executor failure propagation。

## 2026-08-15: drawMesh の RT 分岐を再整形・再確認
- 関連: `Artifact/src/Render/ArtifactIRenderer.cppm`
- 対応: BLAS/TLAS 分岐のインデントとブロック構造を明確化した。透明 instance の無効化、不透明化時の再登録、transform 更新、geometry 更新時の BLAS 再構築の範囲を読み違えにくくした。
- 未検証: C++20 module compile、GPU 実行時の TLAS 更新結果。

## 2026-08-15: RT warmup shader の payload / hit group 整合性確認
- 関連: `ArtifactCore/src/Graphics/RayTracingManager.cppm`
- 事実: RayGen / Miss / ClosestHit が同一 `Payload { float4 color; }` を使用し、SBT 登録名は PSO の shader 名と一致している。Miss と ClosestHit の双方が payload を初期化し、RayGen が UAV へ書き込む。
- 未検証: DXC コンパイル、各 backend の shader model / SBT 制約、実際の TraceRays 出力。

## 2026-08-15: VP監査で確認したcache・同期境界の分離
- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/src/Widgets/Render/ArtifactSoftwareRenderInspectors.cppm`
- **事実:** Composition のcamera-only GPU cacheは実装済みだが、静止画／solid・Normal blend・effect/maskなし等に限定され、設定opt-inかつruntime検証待ちである。通常2D direct pathではレイヤー後に`ArtifactIRenderer::flush()`が呼ばれ、surface cacheがhitしてもeffect/mask付きimage・SVG・text・videoでは入力の`QImage`化やmatte解決が先に残る。Software Composition TestはQPainter系の別実装で、3Dは実描画せずfallback card、videoは情報カードになり得る。
- **仮説（未検証）:** VP改善を一つの「高速化」変更として扱うと、camera cache、layer surface cache、RTV/UAV flush境界、software parityの問題を混同する。まず2D direct pathのflush削減可能条件、次にcache hit前のsource変換、最後に3D/software parityを個別に受入する必要がある。
- **価値・懸念:** 表示品質と性能の証拠を同じ指標に混ぜず、DiligentのD3D12/Vulkan共通境界を壊さずに、最小の改善単位を選べる。`QImage`／QPainterの新規ホットパス拡大や、子リポジトリ変更を誘発しない。
- **次に確認すべきこと:** ビルド・runtime許可後、(1) 2D direct pathでflush回数とGPU frame time、(2) effect/mask付き静止画でsource変換回数、(3) 3D／video／software previewのfresh captureと画素差、(4) focus移動・overlay外クリック・selection同期のUI sessionを分けて計測する。

## 2026-08-15: VPのflush診断値が常時ゼロになる経路
- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- **事実:** `renderOneFrameImpl()` のoverlay pass後に `flushMs = 0` が無条件代入され、その後 `lastFlushMs_` とFrameDebugのflush passへ渡される。`renderer_->flush()` の呼び出しは複数あるが、現行の`flushMs`単独では総flush時間を表さない。
- **仮説（未検証）:** flush削減の性能判断を現行ログだけで行うと、実際のsubmissionコストを見落とす。frame全体の累積flush時間、またはflush回数と最終flushの区別が必要。
- **価値・懸念:** 先に診断の意味を修正しないと、direct pathのflush集約前後を比較できない。計測追加はDiligentの`submitQueuedDraws()`と`IImmediateContext::Flush()`の境界を壊さず、待機を導入しない形に限定する。
- **次に確認すべきこと:** `flush()` wrapper入口で累積時間／回数を記録し、frame endでリセットする案と、既存の`Submit2D` profiler計測との重複を比較する。ビルド・runtime検証は許可後に行う。
## 2026-08-15: ShapePath の fill rule はレイヤー境界で明示保存が必要
- 関連: `Artifact/include/Layer/ArtifactShapeLayer.ixx`、`Artifact/src/Layer/ArtifactShapeLayer.cppm`
- 事実: Core の `ShapePath` は既に Winding／EvenOdd と triangulation を持つが、`ArtifactShapeLayer` の custom Bézier 設定には fill rule がなく、JSON・Property Editor・native geometry 間で選択値を保持できなかった。
- 対応: custom path fill rule をレイヤー設定、native geometry／operator経路、JSON保存／復元へ接続し、既定値はWindingに維持した。
- 未検証: C++20 module compile、穴を含むEvenOdd描画のpixel parity、Preview／Render Queueのruntime結果。
## 2026-08-15: Final Post Process の未適用成功扱い
- 関連: `Artifact/src/Render/ArtifactFinalPostProcess.cppm`
- 事実: view transform が有効でもLUTが未設定の場合、GPU出力を書かずに `apply()` が `true` を返していた。
- 対応: 実際にpost-processを適用できない場合は `false` を返し、呼び出し側がstale destinationを採用しないようにした。
- 未検証: GPU runtime、LUT適用、OCIO/ACES display transform の実出力。
## 2026-08-15: 3D layer の source-less JSON stale restore
- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: `fromJsonProperties()` は `sourcePath` が空で `fixedGeometry=Auto` の場合、既存レイヤーに読み込まれていたmeshを置き換えなかった。
- 対応: sourceのない復元ではCubeへ戻し、旧モデルが表示に残らないようにした。
- 未検証: C++20 module compile、モデル欠落／再読込のruntime、3D遮蔽parity。
## 2026-08-15: 3D missing source 復元時の旧mesh残留
- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: `fromJsonProperties()` のmissing model pathで `loadFromFile()` が早期returnし、既存レイヤーのmeshが表示に残る可能性があった。
- 対応: source pathを保持したまま `meshLoaded_ = false` とし、missing状態を描画へ持ち越さないようにした。
- 未検証: missing／relink runtime、UIのmissing表示、3D render queue parity。
## 2026-08-15: 3D transform snapshot の固定30fps
- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: 現在／前フレームの `RationalTime` が固定30fpsで作られ、非30fps compositionで3Dアニメーションの時刻がずれる可能性があった。
- 対応: `compositionFrameRate()` を安全なフォールバック付きで使うようにした。
- 未検証: 24／25／29.97／60fpsのruntime、モーションブラー／velocity連携。
## 2026-08-15: Camera／Light のfps整数丸め
- 関連: `Artifact/src/Layer/ArtifactCameraLayer.cppm`、`Artifact/src/Layer/ArtifactLightLayer.cppm`
- 事実: composition fpsを `int64_t` へ丸めており、29.97fpsなどの時刻基準が30fpsへ変わっていた。
- 対応: 実数fpsを `RationalTime` へ渡すよう変更し、Model3D／Camera／Lightの時間基準を揃えた。
- 未検証: 29.97fpsのruntime、カメラシェイク／ライトアニメーションの実機結果。
## 2026-08-15: 3D編集補助経路の固定30fps
- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 事実: selection outline、固定平面の投影／ray hit、Model3D pickingのtransform snapshotに固定30fpsが残っていた。
- 対応: 各レイヤーの`compositionFrameRate()`を使い、描画本体と編集補助の時刻基準を統一した。
- 未検証: 非30fpsのruntime選択・picking・投影、3D gizmo parity。
## 2026-08-15: Layer component JSON の stale state
- 関連: `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- 事実: components／componentGraph を持たないJSONを既存レイヤーへ復元すると、以前のcomponent activation・追加modifier・descriptor graphが残る可能性があった。
- 対応: 欠落ブロック時にlegacy activation、追加modifier、script binding、component host graphを明示クリアしてからbuiltin descriptorを再同期するようにした。
- 未検証: C++20 module compile、component runtime phase parity、旧JSON互換。
## 2026-08-15: Precomp source composition の stale restore
- 関連: `Artifact/src/Layer/ArtifactCompositionLayer.cppm`
- 事実: `composition.sourceId` がないJSONを既存precomp layerへ復元すると、以前のsource composition IDが残る可能性があった。
- 対応: source IDを常に復元し、欠落時は空IDへ明示的に戻すようにした。
- 未検証: precompose／unprecompose runtime、nested compositionの描画・undo parity。
## 2026-08-15: Clone Layer source／effector stale restore
- 関連: `Artifact/src/Layer/ArtifactCloneLayer.cppm`
- 事実: JSONに`sourceLayerId`または`useEffector`がない場合、既存Clone Layerの以前の設定が残る可能性があった。
- 対応: 欠落時はsource layer IDを空、effector使用をfalseへ明示的に戻すようにした。
- 未検証: Clone Layerのpartial JSON互換、runtime generator／effector parity。
## 2026-08-15: Render Preflight の出力安全チェック不足
- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 事実: preflightは出力ディレクトリの存在までで、書込み不可と既存出力ファイルを診断していなかった。
- 対応: 書込み不可をError、既存ファイルを上書きWarningとして追加した。
- 未検証: Windows／ネットワークドライブの権限判定、sequence／video出力の実書込み。
## 2026-08-15: Timeline playhead の非有限値伝播
- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`
- 事実: `setCurrentFrame()` がNaN／Infを直接`std::clamp`へ渡し、current frameとdirty rectangle計算へ不正値が伝播する余地があった。
- 対応: 有限値でない入力は現在フレームへ戻してから範囲clampするようにした。
- 未検証: UI scrub／外部transportからのNaN入力、長時間再生のruntime。
## 2026-08-15: Timeline viewport値の非有限値伝播
- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`
- 事実: duration、pixels-per-frame、scroll offsetも非有限値を直接clamp／座標計算へ渡す余地があった。
- 対応: 各入力を有限値へ正規化してからclampし、Timelineの描画・スクロール状態を安定化した。
- 未検証: レイアウト復元、外部transport、長時間scrubのruntime。
## 2026-08-15: Timeline duration短縮時のplayhead残留
- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`
- 事実: durationを短縮してもcurrent frameが旧終端のまま残り、playheadが表示範囲外になる可能性があった。
- 対応: duration更新時にcurrent frameを新終端へclampした。
- 未検証: duration変更中の外部transport同期、runtime再生／scrub。
## 2026-08-15: InputSurface capture後のtarget／context残留
- 関連: `ArtifactCore/src/UI/InputOperatorManager.cppm`
- 事実: commit／cancel後にmodeはOffへ戻るが、前回のtargetIdとcontextがstateに残っていた。
- 対応: Off正規化時にtarget／contextをクリアし、次回captureへのstale対象混入を防いだ。
- 未検証: Timeline／Inspector UIの状態表示、property書込み、runtime capture連続操作。
## 2026-08-15: InputSurface の負フレーム入力
- 関連: `ArtifactCore/src/UI/InputOperatorManager.cppm`
- 事実: transport／step frameとcapture開始引数を負値のまま状態へ保存できた。
- 対応: setterおよびcapture開始時に0未満を0へ正規化した。
- 未検証: 外部transport、step keyframe書込み、runtime scrub境界。
# 2026-08-15 — InputSurface の確定・取消でコンテキストを残さない

- 関連: `ArtifactCore/src/UI/InputOperatorManager.cppm` の `commitCapture()` / `cancelCapture()`。
- 事実: capture 終了時に mode と armed 等は Off 相当に戻していたが、`targetId` と `context` は明示的に消去されていなかった。
- 対応: Off の共通正規化を確定・取消経路にも通し、次の入力セッションへ対象・文脈が残留しないようにした。
- 価値/懸念: DAW-style 入力の再利用時に、前回の編集対象へ誤って書き込むリスクを下げる。ビルド未実施のため、呼び出し側の期待値は未検証。
- 次に確認: 実装をビルド／実行できる段階で、commit/cancel 後の stateChanged payload と再開始時の target/context を確認する。
# 2026-08-15 — TransformGizmo の対象差し替え境界

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`。
- 事実: マルチターゲット変換はドラッグ開始時に全対象の Undo スナップショットを保持するため、ドラッグ中の `setLayer()` / `setTargetLayers()` は旧スナップショットと新対象を混在させ得た。
- 対応: 対象差し替え前に進行中の操作を `cancelInteraction()` で復元・終了する。
- 価値/懸念: 選択変更時に誤ったレイヤーへ変換や Undo を適用するリスクを下げる。ビルド未実施のため、選択変更イベントとの実行順序は未検証。
- 次に確認: 実行時にドラッグ中の選択変更、取消後の dirty/event 通知、Undo 履歴の増加がないことを確認する。
# 2026-08-15 — TransformGizmo のターゲット配列正規化

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm`。
- 事実: `setTargetLayers()` は null や同一 ID の重複を受け入れられ、マルチドラッグ時の変換・Undo対象が重複し得た。
- 対応: 対象差し替え時に null と重複 ID を除去し、入力順は維持する。
- 価値/懸念: 同一レイヤーへの二重適用を防ぐ。ビルド未実施のため、呼び出し側が null を件数として扱う前提は未検証。
- 次に確認: 複数選択の順序、同一 ID の重複入力、全件無効入力時の Gizmo 非表示を実行時に確認する。
# 2026-08-15 — マスクスタックの並べ替え API

- 関連: `Artifact/include/Layer/ArtifactAbstractLayer.ixx`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- 事実: マスクの追加・削除・置換は存在したが、Phase 1 のスタック順変更を表すモデル API がなかった。
- 対応: `moveMask(fromIndex, toIndex)` を追加し、無効 index／同一 index は no-op、成功時は順序と `maskRevision` を更新する。
- 価値/懸念: UI の Drag&Drop 並べ替えを既存レイヤー責務内で実装できる。ビルド未実施のため、公開モジュール宣言との整合は未検証。
- 次に確認: パネル側からの Undo 接続と、マスク合成順が UI 順序と一致するかを確認する。
# 2026-08-15 — マスク順変更の Undo 境界

- 関連: `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`。
- 事実: マスク順変更 API は追加できたが、Undo 層に対応コマンドがなかった。
- 対応: `MoveMaskCommand` を追加し、弱参照レイヤーに対して old/new index を反転適用する。
- 価値/懸念: マスクスタック UI は順変更を履歴化できる。現時点では Drag&Drop UI からの push 接続は未実装。
- 次に確認: マスクスタック UI の並べ替えイベントから、変更成功時だけ `UndoManager::push()` する。
# 2026-08-15 — Inspector からマスク順変更を履歴化

- 関連: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`。
- 事実: `MoveMaskCommand` は存在したが、ユーザーが実行できる導線がなかった。
- 対応: 既存 Inspector コンテキストメニューに、複数マスクの各項目の Up/Down 操作を追加し、`UndoManager::push()` 経由で順変更する。
- 価値/懸念: 新規シグナルなしでマスク順変更と Undo を接続できる。専用 Drag&Drop パネルは未実装。
- 次に確認: マスク順の表示名、Undo/Redo 後の合成順、選択レイヤー更新を実行時に確認する。
# 2026-08-15 — マスク一括状態操作の Undo

- 関連: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`。
- 事実: マスク順変更の導線は追加済みだったが、Phase 1 の一括 Enable/Disable/Invert 操作はなかった。
- 対応: 既存 `MaskEditCommand` に before/after のマスク配列を渡し、変更がある場合だけ履歴化する。
- 価値/懸念: 複数マスクの状態変更を一回の Undo で戻せる。専用 Drag&Drop パネルと個別選択 UI は未実装。
- 次に確認: 一括操作後のマスク合成結果、Undo/Redo、0件／全同値状態で不要な履歴が積まれないことを実行時に確認する。
# 2026-08-15 — マスクパス合成モードの一括変更

- 関連: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`MaskMode`。
- 事実: `LayerMask` は複数 `MaskPath` の合成モードを保持するが、Inspector から全パスを一括変更する導線がなかった。
- 対応: Add/Subtract/Intersect/Difference の一括操作を追加し、変更がある場合だけマスク配列の before/after を `MaskEditCommand` に渡す。
- 価値/懸念: マスクスタックの合成ルールをまとめて調整できる。個別パス選択・専用パネルは未実装。
- 次に確認: 複数パスの合成結果と Undo/Redo が一致するかを実行時に確認する。
# 2026-08-15 — CompositionCompareMode の責務境界

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 事実: `CompositionCompareMode` と A/B state variant の切替、`Diff` 値の保持は存在するが、レンダー本体で compare mode に応じた2出力・差分合成を行う分岐は確認できない。
- 仮説: 現状の compare mode は state variant 選択の準備段階で、Phase 3 の DiffComposite／SplitView を直接提供するものではない。
- 価値/懸念: UI に差分モードを露出する前に、フル合成と選択対象の2つのレンダー結果を保持する境界を追加する必要がある。推測を実装に広げず、今回のターンではコード変更を見送った。
- 次に確認: `RenderPassResources` または既存 offscreen render target を比較用に再利用できるか、GPU readback を増やさずに2パスを合成できるかを調査する。
# 2026-08-15 — 比較レンダー用レイヤーフィルター

- 関連: `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 事実: 既存のレイヤー合成ループは全レイヤーを処理しており、選択レイヤーのみを再描画する指定がなかった。
- 対応: `CompositionLayerRenderFilter` と `setLayerRenderFilter()` を追加し、`SelectedOnly` 時は選択集合外をスキップする。既定値は `All`。
- 価値/懸念: DiffComposite／SplitView の2パス目へ進むための最小境界を追加した。ただし比較用の別レンダーターゲットと差分合成は未実装。
- 次に確認: フィルター切替時の base composite 無効化、選択なし時の空出力、既存 solo／visibility 判定との順序を確認する。
# 2026-08-15 — 比較フィルターのコンポジション境界

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の composition reset。
- 事実: 比較用 `SelectedOnly` は controller の状態として保持されるため、コンポジション切替時に明示リセットしないと次のコンポジションにも残り得た。
- 対応: 既存 compare state の reset と同じ境界で `CompositionLayerRenderFilter::All` に戻す。
- 価値/懸念: コンポジション切替後の表示欠落を防ぐ。2パス差分合成自体は未実装。
- 次に確認: controller destroy／再initialize と composition 差し替えの両方で filter getter が All を返すことを実行時に確認する。
# 2026-08-15 — CompositionRenderController destroy 時の比較状態初期化

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 事実: composition 差し替え時の filter reset は追加済みだったが、controller の `destroy()` では compare mode／filter の明示リセットがなかった。
- 対応: destroy 境界でも `CompositionCompareMode::Off` と `CompositionLayerRenderFilter::All` に戻す。
- 価値/懸念: renderer 再初期化後に古い比較表示状態が復活しない。2パス合成は未実装。
- 次に確認: destroy→initialize の後に通常全レイヤー描画へ戻ることを実行時に確認する。
# 2026-08-15 — SelectedOnly の単一選択フォールバック

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 事実: 選択集合 API が空でも単一の `selectedLayerId_` は設定される経路があり、集合だけを見ると SelectedOnly が全レイヤーを除外していた。
- 対応: 複数選択集合が空の場合は selectedLayerId と比較し、単一選択を描画対象にする。
- 価値/懸念: 単一選択と複数選択で比較用フィルターの意味が一致する。ビルド未実施のため、selection manager の更新順序は未検証。
- 次に確認: 単一選択、複数選択、選択解除の3状態で SelectedOnly の描画対象を確認する。
# 2026-08-15 — 比較2パス向けレイヤー判定の共通化

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- 事実: SelectedOnly の複数／単一選択フォールバック判定がレイヤー描画ループ内に埋め込まれていた。
- 対応: `passesLayerRenderFilter()` に切り出し、filter・selectedIds・selectedLayerId・layer を同じ契約で評価する。
- 価値/懸念: フル／選択のみの2パス化で対象判定が分岐しない。今回の動作は従来と同じで、別ターゲット描画は未接続。
- 次に確認: 2つのレンダーパスが同じ選択集合と単一選択フォールバックを共有することを確認する。
# 2026-08-15 — Timeline キーフレームスニペット基盤

- 関連: `Artifact/include/Widgets/ArtifactTimelineWidget.ixx`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: 選択キーフレームの JSON Copy/Paste と Undo は既存だったが、名前付きの一時保存はなかった。
- 対応: Timeline 内に `QHash<QString, QJsonArray>` を追加し、保存／適用／削除 API を実装。適用は既存 Clipboard／Paste 経路を通す。
- 価値/懸念: スニペット適用時も既存の複数レイヤー適用と Undo を再利用できる。現時点では名前入力・一覧 UI と永続化は未実装。
- 次に確認: UI からの名前入力、同名上書き確認、Timeline 再生成時の保持、プロジェクト保存との境界を設計する。
# 2026-08-15 — キーフレームスニペット UI 接続

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: スニペット API は追加済みだったが、名前入力と一覧操作の UI がなかった。
- 対応: Curve Editor ヘッダーに Snippet ボタンを追加し、保存・適用・削除を既存 API へ接続した。
- 価値/懸念: 既存 Paste 経路で Undo を維持できる。スニペットは現在 Timeline widget の寿命内だけ保持し、プロジェクト永続化は未実装。
- 次に確認: 同名保存の上書き確認、widget 再生成、プロジェクト保存／再読込への統合を確認する。
# 2026-08-15 — キーフレームスニペットの設定保存

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: スニペットは widget のメモリ内だけに保持され、再生成・再起動で失われていた。
- 対応: `QSettings` の `Timeline/KeyframeSnippets` グループに各 JSON 配列を保存し、Impl コンストラクタで復元する。
- 価値/懸念: プロジェクト形式を変更せずユーザー設定として再利用できる。プロジェクト単位の共有・移行は未実装。
- 次に確認: 壊れた JSON、空名、同名上書き、設定削除後の復元を実行時に確認する。
# 2026-08-15 — Alt ドラッグの自動スムージング

- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`。
- 事実: Shift／Ctrl の軸拘束と複数キー移動は実装済みだったが、Alt ドラッグ確定時に補間を自動調整していなかった。
- 対応: Alt（Ctrl なし）でドラッグしたキーについて、前後キーの速度から `tryComputeEasyEaseHandles()` を使い、Bezier 補間とハンドルをスナップショットへ反映する。
- 価値/懸念: 既存 Easy Ease と同じ計算・Undo 経路を再利用できる。隣接キーがない／非スカラー値では従来補間を維持する。ビルド未実施。
- 次に確認: Alt 単独、Alt+Ctrl、隣接キーなし、複数選択の各ケースを実行時に確認する。

# 2026-08-15 — Timeline チャンネルフィルターの最小導入

- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: 既存検索はレイヤー行を絞り込むが、キーフレームマーカーのプロパティチャンネル絞り込みはなかった。
- 対応: `transform:` / `audio:` / `effect:` を既存検索欄で解釈し、マーカー収集時にチャンネル選別する API を追加。
- 価値/懸念: 新規シグナルを増やさず既存更新経路を使える。Property 行とマーカーの両方を同じ分類で更新する。
- 次に確認: 実 UI で接頭辞入力時の行表示、空グループの非表示、既存検索語との併用を確認する。

# 2026-08-15 — チャンネルフィルターとカーブエディタの同期

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: Property 行とマーカーを絞り込んでも、Curve Editor は選択レイヤーの全プロパティを収集していた。
- 対応: Curve／Speed Graph の両方へ同じ `PropertyChannelFilter` を渡し、対象トラックを同期した。
- 価値/懸念: フィルター変更後にカーブだけ別チャンネルが残る不整合を防げる。プロパティ分類は既存パス命名に基づく簡易判定である。
- 次に確認: Transform／Audio／Effect 各モードで選択・カーブ編集・Undo の対象が一致することを実行時に確認する。

# 2026-08-15 — フィルター変更時のカーブ更新保証

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: 検索欄からチャンネルを変更した際、Curve Editor の再構築は EventBus の遅延更新に依存していた。
- 対応: 検索変更処理で Timeline と Curve Editor を明示的に再同期し、非表示チャンネルの選択・フォーカスが残らない経路を確保した。
- 価値/懸念: UI 操作直後の表示遅延を減らせる。既存の更新処理を直接呼ぶため、頻繁な検索入力時の負荷は実行時に確認が必要。
- 次に確認: 連続入力、空検索への復帰、フィルター中のカーブ編集後 Undo を確認する。

# 2026-08-15 — チャンネル接頭辞とプロパティ検索の併用

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: 接頭辞を単独トークンとして解釈していたため、`transform:position` のような検索はチャンネル指定として認識されなかった。
- 対応: `transform:`／`audio:`／`effect:` の後続文字列を通常のプロパティ検索語として左ペインへ渡すようにした。
- 価値/懸念: チャンネル指定とプロパティ名検索を一つの検索欄で併用できる。分類は引き続きプロパティパス命名に依存する。
- 次に確認: 大文字小文字、空白付き接頭辞、未知の接頭辞を含む検索を確認する。

# 2026-08-15 — Timeline からのアニメーションレイヤーベイク

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`, `Artifact/include/Undo/UndoManager.ixx`。
- 事実: レイヤー側には Work Area 範囲のアニメーションレイヤーベイクとスナップショット Undo が既にあったが、Timeline の選択レイヤーから直接呼ぶ導線がなかった。
- 対応: 既存 Pattern ボタンのメニューに範囲ベイクを追加し、選択レイヤーごとに Work Area をベイクして既存 Undo コマンドへ登録するようにした。
- 価値/懸念: 複数レイヤーを同じ範囲で一括ベイクできる。Undo が利用可能な場合はレイヤーごとに履歴へ積み、利用できない場合もベイク結果を保持する。
- 次に確認: 空 Work Area、非選択状態、複数レイヤーの Undo／Redo、ベイク後の Curve 更新を確認する。

# 2026-08-15 — 選択キーフレームのフリンジ生成

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: Phase 7 の範囲ベイク導線はあったが、選択範囲の端で補間を安定させる近接キーフレーム生成はなかった。
- 対応: Pattern メニューから、選択プロパティごとの最初／最後の選択値を範囲端の隣接フレームへ複製する機能を追加した。既存のキーフレームスナップショット Undo を利用する。
- 価値/懸念: 範囲端の補間値を固定しやすくなる。既存キーがある場合、またはコンポジション範囲外では追加しない。
- 次に確認: 単一／複数プロパティ、範囲端、既存キー、Undo／Redo を確認する。

# 2026-08-15 — Phase 8 ブロック移動の既存実装監査

- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`, `ArtifactCore/src/UI/ShortcutBindings.cppm`。
- 事実: Keyframe Area の Body／Edge ドラッグは選択キー群をまとめて移動・伸縮し、複数トラックを扱える。`Ctrl+G` は Curve Editor 切替に既に予約されている。
- 対応: 既存のブロック操作を再利用対象として確認し、ショートカット競合を避けるため `Ctrl+G` の上書きは行わなかった。
- 価値/懸念: 既存 Undo・スナップ経路を維持できる。永続的な名前付きグループはまだなく、Phase 8 の「グループ化」は Area 操作ベースである。
- 次に確認: Phase 9 のプロパティブロックコピー／ペーストへ進む。

# 2026-08-15 — Property Block Copy/Paste の既存経路監査

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`, `ArtifactCore/include/Clipboard/ClipboardManager.ixx`。
- 事実: 選択キーフレームの Clipboard レコードは各要素に `propertyPath` を持ち、Paste 時に対象レイヤーごとに同じプロパティパスを解決する。
- 対応: 複数プロパティを含む既存 Copy／Paste を Property Block の実装として確認し、別形式や重複 UI は追加しなかった。
- 価値/懸念: 既存の JSON／システムクリップボード／Undo 経路を維持できる。プロパティ値全体（非キーフレーム）のブロックコピーは別機能として未実装。
- 次に確認: Phase 10 の数値入力スピニングを監査する。

# 2026-08-15 — 修飾ホイールによる数値スピニング

- 関連: `Artifact/include/Widgets/ArtifactRelativeSpinBox.ixx`。
- 事実: 相対 SpinBox は誤操作防止のためホイールを無条件に無視していた。
- 対応: 通常ホイールは従来どおり無効のまま、Shift=0.1x、Ctrl=10x、Alt=0.01x の修飾時だけ Double／Integer SpinBox を更新するようにした。
- 価値/懸念: 意図しないスクロール変更を避けつつ、Inspector の微調整・粗調整を共通化できる。Integer SpinBox は整数丸めのため極小倍率でも最小 1 step となる。
- 次に確認: 各修飾キー、上下方向、範囲端、通常ホイール無効の挙動を確認する。

# 2026-08-15 — Timeline マルチプロパティ検索

- 関連: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`。
- 事実: 検索対象はレイヤー名とプロパティグループ名が中心で、個別プロパティ名や正規表現による行絞り込みはなかった。
- 対応: プロパティ名／表示ラベルを検索キャッシュへ追加し、通常文字列と `/正規表現/` の両方で Property 行を絞り込むようにした。
- 価値/懸念: `transform:position` など上位の Timeline フィルターと組み合わせて、実際に編集対象となる行だけを表示できる。正規表現が不正な場合は一致なしとして扱う。
- 次に確認: 正規表現、表示ラベル、空検索復帰、保存済み検索フィルターの導線を確認する。

# 2026-08-15 — Timeline 検索フィルターの保存

- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm`。
- 事実: Phase 11 の検索は実装済みだったが、検索語を名前付きで保存・再適用する導線がなかった。
- 対応: 検索欄のコンテキストメニューに保存／適用を追加し、`QSettings` の `Timeline/SavedSearchFilters` に名前付きフィルターを保存するようにした。
- 価値/懸念: 正規表現やチャンネル接頭辞を含む検索条件を再利用できる。削除 UI はまだなく、同名保存は上書きする。
- 次に確認: 保存・再起動後の復元、同名上書き、空検索の保存拒否を確認する。

# 2026-08-15 — Dock Add Menu の registry 境界監査

- 関連: `Artifact/src/Widgets/ArtifactMainWindow.cppm`, `Artifact/include/Widgets/ArtifactDockManager.ixx`。
- 事実: Dock manager は dock ID の登録・重複拒否・一覧取得を既に持ち、MainWindow 側には既存 dock の再表示／activate 経路がある。
- 対応: Add Menu の Phase 1 として、表示名ではなく objectName／dock ID を永続キーにする責務境界を確認した。
- 価値/懸念: 新規 dock registry を重複作成せず既存管理を再利用できる。カテゴリ／表示名 descriptor はまだない。
- 次に確認: 現行 dock 登録箇所を一覧化し、Phase 2 の descriptor と追加メニューを最小範囲で実装する。

# 2026-08-15 — Dock パネル再表示メニューの最小導線

- 関連: `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`。
- 事実: 既存の Window Panels メニューは表示切替を持っていたが、追加／再表示の意図が独立していなかった。
- 対応: 登録済み dock だけを列挙する「パネルを追加／再表示」サブメニューを追加し、既存 dock の表示・activate API を再利用した。
- 価値/懸念: 未登録 panel の見せかけや重複生成を避けられる。タイトルバーの専用 `+` 導線と ID ベースの履歴は未実装。
- 次に確認: MainWindow title bar の適切なホスト位置を特定し、同じ submenu を `+` 入口へ移す。

# 2026-08-15 — Dock 最近使用／お気に入りの ID 保存

- 関連: `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`。
- 事実: Dock の再表示メニューはあったが、頻繁に使う面を ID ベースで再利用する保存層がなかった。
- 対応: 最近使用（最大8件）とお気に入りを `QSettings` に Dock ID で保存し、View メニューから activate／切替できるようにした。存在しない Dock ID は表示時に除外する。
- 価値/懸念: 表示名変更や未登録面の混入に強い。専用 title-bar `+` とカテゴリ descriptor はまだ未実装。
- 次に確認: Dock title bar の公開拡張 API を依存ヘッダで確認し、可能なら同じメニューを `+` に接続する。

# 2026-08-15 — Dock 追加メニューのカテゴリ整理

- 関連: `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`。
- 事実: 登録済み Dock の再表示項目は単一のフラット一覧だった。
- 対応: Project / Assets、Editing、Animation、Render / Diagnostics、Other のカテゴリ submenu に分け、各項目の activate 経路は既存 API を維持した。
- 価値/懸念: パネル数が増えても探索しやすい。分類は現行表示名のキーワードに基づくため、将来は Dock descriptor の明示カテゴリへ移行する。

# 2026-08-15 — Dock メニュー設定の再読込修正

- 関連: `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`。
- 事実: Dock 一覧が変わらない場合にメニュー再構築を早期終了していたため、最近使用順の更新が次回表示へ反映されなかった。
- 対応: Window Panels メニューを表示時に設定から再構築し、最近使用順・お気に入りの変更を即時反映するようにした。
- 価値/懸念: 設定と UI の stale 表示を防げる。Dock 数が非常に多い場合の再構築コストは runtime で確認する。

# 2026-08-15 — Dock メニューのアクセシビリティ metadata

- 関連: `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`。
- 事実: 新しい Dock サブメニューは表示名だけで、スクリーンリーダー向けの役割説明がなかった。
- 対応: 最近使用／お気に入り／追加・再表示メニューに accessible name／description、個別 action に tooltip を追加した。
- 価値/懸念: メニューの目的と操作結果を識別しやすくなる。狭幅レイアウトと実際のキーボード受入は未確認。

# 2026-08-15 — MainWindow 上部 chrome への Dock `+` 入口

- 関連: `Artifact/src/Widgets/ArtifactMenuBar.cppm`, `Artifact/include/Widgets/ArtifactMainWindow.ixx`。
- 事実: ADS 内部 title bar の公開拡張 API は workspace から確認できなかったが、MainWindow の QMenuBar には右上 corner widget の拡張点がある。
- 対応: 右上に `+` QToolButton を追加し、登録済み Dock を表示時に列挙して既存 `setDockVisible()`／`activateDock()` へ接続した。
- 価値/懸念: 新規 Dock 生成や ADS 本体変更なしで追加導線を提供できる。狭幅メニューバーでの表示密度は runtime 未確認。

# 2026-08-15 — Dock `+` 入口の最近使用／お気に入り同期

- 関連: `Artifact/src/Widgets/ArtifactMenuBar.cppm`, `docs/planned/MILESTONE_DOCK_PANEL_ADD_MENU_2026-08-15.md`。
- 事実: 上部 chrome の `+` 入口は登録済み Dock のフラット一覧だけを持っていた。
- 対応: View メニューと同じ `QSettings` の最近使用／お気に入り ID を表示時に読み込み、既存の Dock activate 経路と最近使用更新を共有した。
- 価値/懸念: 入口が違っても利用頻度の高い Dock に同じ手順で到達できる。カテゴリ分類の完全な parity は未実装で、狭幅表示は runtime 未確認。

# 2026-08-15 — Render Queue の単一フレーム表記

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`。
- 事実: Render Queue の frame range mode `4` は単一フレーム出力だが、UI 表記が `Single Frame` で現在の playhead との関係が曖昧だった。
- 対応: 内部 mode／保存形式を変更せず、一覧 summary と combo の表示を `Current Frame` に統一し、選択肢の accessible description を追加した。
- 価値/懸念: Current Frame が Composition／Work Area／Selected Frames と並ぶ出力範囲の意味を読み取りやすくなる。実際の queue 実行時 frame 解決は runtime 未確認。

# 2026-08-15 — Composition Settings の共通 finalize 経路

- 関連: `Artifact/include/Service/ArtifactProjectService.ixx`, `Artifact/src/Service/ArtifactProjectService.cppm`, `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm`, `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`。
- 事実: Composition Menu と Project View は設定フォームと解像度 remap 判定を個別に持つ一方、確定後の project dirty 通知・playback range／FPS 同期もそれぞれ実装していた。
- 対応: `finalizeCompositionSettingsChange()` を Project Service に追加し、両 UI から共通利用するようにした。解像度変更の Undo／remap、フォーム責務、新規 signal 配線は変更していない。
- 価値/懸念: 片方だけ同期処理が抜ける divergence を減らせる。設定フォームと remap 判定そのものの共通化、および runtime 受入は未完了。

# 2026-08-15 — Composition Menu の Render Queue 追加導線整理

- 関連: `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm`。
- 事実: 全範囲／Current Frame／Work Area／選択レイヤー系の6 action が Composition Menu の同じ階層に並び、範囲とレイヤー対象の違いが一覧で追いにくかった。
- 対応: 6 action を「レンダーキューに追加」submenu にまとめ、既存 QAction、shortcut、handler、enable 判定は変更せず、submenu の accessible metadata を追加した。
- 価値/懸念: 追加操作の探索性を上げつつ command 互換性を維持できる。submenu の狭幅表示と runtime 操作確認は未実施。

# 2026-08-15 — Timeline audio waveform の layout 同期ブロック遅延化

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: waveform cache が未構築または signature 不一致の場合、`refreshTracks()`／`updateLayout()` の同期中に `buildAudioWaveformForLayer()` が実行されていた。
- 対応: cache miss を per-layer の pending set で重複抑制し、次の UI event loop tick に生成を遅延。完了後に同じ composition の track を再構築する。composition 切替時は pending を破棄する。
- 価値/懸念: レイアウト更新入口の同期ブロックと重複生成を減らせる。layer snapshot の安全な worker 契約が未定義のため、decode／生成自体の別スレッド化と runtime 負荷検証は残る。

# 2026-08-15 — Property Reset の値／キーフレーム Undo 単位統一

- 関連: `Artifact/include/Undo/UndoManager.ixx`, `Artifact/src/Undo/UndoManager.cppm`, `Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`。
- 事実: Property Editor の Reset は keyframe を削除する Undo だけを作り、default value の変更自体は同じ Undo 単位に含めていなかった。
- 対応: layer property value 用の `SetLayerPropertyValueCommand` を追加し、keyframe command と `MacroUndoCommand` にまとめた。keyframe がない Reset も値変更を Undo 対象にした。
- 価値/懸念: Reset 前の値とアニメーション状態を1回の Undo で復元できる。通常の複数選択編集と runtime 受入は未完了。

# 2026-08-15 — Render Queue 履歴 metadata と行アクション

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`。
- 事実: 履歴行は時刻付きテキストだけで、service event の job ID／frame range／failure stage が履歴から読み取りにくく、行から Retry／Reveal を直接実行できなかった。
- 対応: service event の履歴表示に job metadata を付加し、source index を `QListWidgetItem::UserRole` に保持。履歴行の context menu から Retry Job／Reveal Output を既存 service API へ接続した。
- 価値/懸念: 失敗履歴から次の操作へ直接進める。既存保存履歴の metadata 復元と service が公開する永続 stable job ID は未完了。

# 2026-08-15 — Screenshot async readback の失敗段階表示

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。
- 事実: async readback の完了後に null image／保存失敗を通知していたが、readback と encode/write のどちらで失敗したかが表示されなかった。
- 対応: null image に `Stage: readback`、保存失敗に `Stage: encode/write` を付加し、readback 完了後の進捗表示を `Saving ...` に更新した。
- 価値/懸念: UI 操作だけで失敗段階を切り分けやすくなる。Whole Window／multi-channel の同期経路と runtime 受入は未確認。

# 2026-08-15 — Four-Up deferred start の世代管理

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。
- 事実: Four-Up の controller start は event loop に段階分散されていたが、切替直後の古い `QTimer::singleShot` callback が新しい layout に対して残る可能性があった。
- 対応: viewport layout generation を追加し、世代が変わった deferred callback を無効化。各 pane start の遅延時間を debug log に記録する。
- 価値/懸念: レイアウト切替時の stale renderer 起動と不要な初期化を減らせる。view／controller の完全な lazy materialization と runtime 計測は未完了。

# 2026-08-15 — Viewport display-transform clear state

- 関連: `Artifact/src/Widgets/Render/ViewportColorPipeline.cppm`。
- 事実: `clear()` は baked LUT を破棄していたが、post-process の view-transform enabled flag は明示的に戻していなかった。
- 対応: LUT と flag を同時に clear し、OCIO config／display transform 無効化後の状態を一致させた。
- 価値/懸念: stale display-transform state の残留を防げる。実素材での HDR／log round-trip と preview／export parity は未検証。

# 2026-08-15 — Layer Component dependency graph validation

- 関連: `Artifact/include/Layer/ArtifactLayerComponentSystem.ixx`。
- 事実: `LayerComponentHost::validate()` は missing／disabled／late dependency を検出していたが、空の required type と循環依存は検出していなかった。
- 対応: 空 dependency type をエラー化し、descriptor type graph を DFS して循環依存を validation issue として返すようにした。
- 価値/懸念: phase evaluator に曖昧な依存グラフが入る前に診断できる。実 component graph と runtime phase parity は未検証。

# 2026-08-15 — Generator／Field／Modifier stack descriptor validation

- 関連: `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `Artifact/docs/MILESTONE_GENERATOR_MODIFIER_FIELD_STACK_2026-07-01.md`。
- 事実: 追加 descriptor stack は保存・再読込・UI 表示へ接続されていたが、stack ごとの空 id／type と重複 id の validation は builtin component host と分離されていた。
- 対応: `validateLayerComponents()` に generator／field／modifier 共通の id／type validation を追加し、既存 diagnostics surface へ統合した。
- 価値/懸念: descriptor merge／評価へ進む前に不正な stack identity を検出できる。field binding・merge／weight 契約と runtime parity は未検証。

# 2026-08-15 — Live field noise／solid shape parity

- 関連: `Artifact/src/Composition/ArtifactAbstractComposition.cppm`, `docs/planned/MILESTONE_LIVE_FIELD_AUTHORING_UX_2026-07-04.md`。
- 事実: composition field の保存形式と共通評価器は radial／box／linear のみを shape として扱っていた。
- 対応: `noise` と `solid` を JSON round-trip と共通 scalar evaluator に追加し、既存の target／coordinate parent／blend／invert 経路へ接続した。
- 価値/懸念: field descriptor の shape 拡張を renderer 側の大改修なしで先行できる。noise は決定的 CPU 評価のみで、時間変化・GPU parity・viewport handle は未検証。

# 2026-08-15 — App Debugger goal-first capture summary

- 関連: `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`, `docs/planned/MILESTONE_HARNESS_ENGINEERING_2026-05-12.md`。
- 事実: Debug Render Harness は goal-first の report summary を持つ一方、App Debugger の Capture Details は capture／baseline の比較情報中心だった。
- 対応: App Debugger 側にも `goal / expected / actual / nextAction` を追加し、既存の capture／failure／compare 情報を再利用した。
- 価値/懸念: 診断 surface 間で次の行動を読み取りやすくなる。status taxonomy の完全統合と runtime smoke は未検証。

# 2026-08-15 — Command IR keyframe preflight validation

- 関連: `Artifact/src/AI/CommandIRExecutor.cppm`, `docs/planned/MILESTONE_COMMAND_IR_AUTOMATION_FOUNDATION_2026-06-28.md`。
- 事実: keyframe command は各 setter を順番に呼び出すため、入力 payload の不備を mutation 前に一括確認していなかった。
- 対応: 単一／batch keyframe command に property path、batch、frame、value の preflight validation を追加した。
- 価値/懸念: malformed request による partial mutation を防げる。setter の runtime failure を跨ぐ rollback は別契約として未実装。

# 2026-08-15 — Command Palette MRU restore normalization

- 関連: `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`。
- 事実: JSON から MRU を復元する経路は文字列をそのまま追加し、空 ID／重複 ID を許容していた。
- 対応: trim、空 ID 除外、重複除外を復元時に追加した。
- 価値/懸念: 再起動後の palette ranking が安定する。Recipe 全体の再起動後復元と runtime 受入れは未検証。

# 2026-08-15 — Workspace layout structural fallback

- 関連: `Artifact/src/Core/ArtifactWorkspaceManager.cppm`。
- 事実: session／preset JSON が空でない場合、`layout` オブジェクト欠落でも復元成功扱いになり得た。
- 対応: `applyWindowState()` で layout object の存在を必須化し、不完全な状態は default-layout recovery に委譲するようにした。
- 価値/懸念: 壊れた session が部分復元状態を成功として固定するのを防ぐ。破損 session の UI 通知と runtime 受入れは未検証。

# 2026-08-15 — Interactive Shell source recursion guard

- 関連: `Artifact/src/Application/ArtifactInteractiveShell.cppm`。
- 事実: nested `source` は再帰を検出していたが、top-level script が active set に登録されず、自己 source と symlink 経由の再帰を防げなかった。
- 対応: top-level／nested source で共有する active script set と canonical path を導入した。
- 価値/懸念: script include の無限再帰を抑止できる。外部 script sandbox／権限と runtime 受入れは未検証。

# 2026-08-15 — Asset Browser search history completer

- 関連: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`。
- 事実: 検索は incremental filter を持っていたが、過去の検索語を再利用する候補／永続化経路がなかった。
- 対応: `QCompleter` と bounded `QSettings` history を既存 search field に接続し、2文字以上の検索語を重複排除して保存するようにした。
- 価値/懸念: 大量素材の再検索を短縮できる。runtime UX と検索履歴切替の受入れは未検証。

# 2026-08-15 — Timeline playhead hit radius ownership

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`。
- 事実: playhead overlay の hit radius が event filter と通常 mouse press に重複定義されていた。
- 対応: 共通定数へ集約し、入力経路間の調整値のずれを防いだ。
- 価値/懸念: 今後の不感帯調整を一箇所で行える。実機入力とテーマ別の視認性は未検証。

# 2026-08-15 — Property row label width alignment

- 関連: `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`、`Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorShared.cppm`。
- 事実: shared row layout は label 幅 132px だが、concrete editor row は 124px だった。
- 対応: concrete row の標準 label 幅を 132px に統一した。
- 価値/懸念: Property Editor と section／channel／transform／effect row の値列開始位置を揃えられる。実機での長いラベルと狭幅レイアウトは未検証。

# 2026-08-16 — Audio monitor/export responsibility boundary

- 関連: `ArtifactCore/src/Audio/AudioMixer.cppm`、`Artifact/src/Service/ArtifactPlaybackService.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- 事実: ExportはAudioMixerの最終segmentを直接取得し、PlaybackだけがAudioRendererのmaster volumeを通るため、現状のmonitor音量とexport音量は別経路になっている。
- 仮説: Cue／Control Room出力を追加する場合は、既存Masterを再利用せず、明示的なmonitor／cue出力役割をAudioMixerまたはPlayback境界に追加する必要がある。
- 価値/懸念: exportへmonitor補正が混入する事故を避けられる。Cue出力のルーティング、複数デバイス、UI責務は未設計・未検証。

# 2026-08-17 — 単一画像レイヤー化は「解析」より生成契約が重要（未検証）

- 関連: `docs/planned/MILESTONE_SINGLE_IMAGE_LAYERIZATION_2026-08-17.md`、`ArtifactImageLayer`、`LayerMask`、`OpenCVRotoBrushEngine`、`MaskCutoutPipeline`
- 事実: 画像バッファ、マスク、マスクからパスへの変換、RotoBrush補助、GPU cutoutの部品は存在する。一方、それらを複数の通常画像レイヤー、背景補完、保存／再読込へまとめる生成契約は確認できない。
- 仮説（未検証）: 最初にモデル精度を追うより、候補のmask／bounds／confidence／provenanceと、一括Undo・source identity・推定画素の保存契約を固定した方が、モデル交換やCPU／GPU fallbackに耐える制作機能になる。
- 価値・懸念: 一枚の画像から復元できない隠し画素を確定情報として扱う事故を避け、AI結果を通常のマスク編集へ安全に引き渡せる。候補の前後順と背景補完品質は素材依存で、runtime評価が必要。
- 次の確認: Phase 0の代表素材で、単一候補のmask生成から画像レイヤー作成、保存／再読込、Preview／Render Queue一致までの最小往復を確認する。モデル選定と新規モジュール追加は、その接続点を確認してから決める。

# 2026-08-18 — Solid2Dグラデーション平面の直接GPU描画

- 関連: `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`、`ArtifactIRenderer::drawGradientRectTransformed`
- 事実: Solid2Dの非単色塗りは、レイヤー自身の描画経路では一度`QImage`のグラデーションを生成してからスプライトとしてGPUへ送っていた。既存のGPUグラデーション矩形APIは同じパラメータを受け取れる。
- 対応: グラデーション画像生成を廃止し、既存のGPU矩形シェーダー経路へ直接送るようにした。単色平面と同じく、変換・クローン重み・不透明度をGPU描画コマンドへ渡す。
- 価値/懸念: 通常の平面描画でCPU画像生成とテクスチャ作成を避けられる。マスク／ラスタライズエフェクト付きの合成経路は別途CPUフォールバックが残り、runtime画質・色補間・fillTypeの一致確認は未実施。
- 次の確認: GPU／CPU出力のグラデーション境界、透明度、反転、中心・スケール・オフセット、クローン表示を実機で比較する。

# 2026-08-18 — 画像デプス機能のCore分離

- 関連: `ArtifactCore/include/Image/DepthMap.ixx`、`ArtifactCore/include/AI/ImageDepthEstimator.ixx`
- 対応: 画像由来の深度値を保持する`DepthMap`と、AIモデルを差し替え可能にする`IImageDepthEstimator`／オプション契約をCoreへ追加した。深度の正規化・反転・範囲制限はCoreで扱い、ONNX／DirectMLの具体的なモデル接続とGPU描画はArtifact側へ分離する。
- 価値・懸念: 手動深度マップ、AI推定深度、将来のクラウド推定を同じデータ契約で扱える。現時点では推定モデル実装とGPU視差描画への接続は未完了で、`DepthMask`は既知のワールド深度用であり画像推定とは別責務。
- 次の確認: Artifact側で深度テクスチャを3Dカード／細分化メッシュへ接続し、最初のローカル推定プロバイダを選定する。
- 追記: `DepthMap::fromQImage()`／`toQImage()`を追加し、グレースケール画像を明示的な入力・表示境界として扱えるようにした。深度メッシュの法線も隣接頂点から近似計算する。
- 追記: `ArtifactImageLayer`に深度マップとメッシュ設定のAPIを追加し、ファイル参照画像では既存3Dカメラ＋`drawMesh()`経路へ切り替えられるようにした。現在はメモリ上の深度設定で、深度マップの保存復元とインメモリGPUテクスチャ材質は未対応。
- 追記: `DepthMap`の画像ファイル入出力と、`ArtifactImageLayer`の深度マップパス・メッシュ設定のJSON保存／復元を追加した。復元時に深度ファイルが欠落している場合は通常画像へ安全にフォールバックする。
- 追記: `MeshRenderer::setBaseColorTextureView()`と`ArtifactIRenderer::drawMesh()`のテクスチャビュー受け渡しを追加した。GPUキャッシュ済みのインメモリ画像をファイル再読込なしで深度メッシュへ接続するための境界ができたが、現在の`ArtifactImageLayer`は引き続きファイルパスを優先する。
- 追記: CompositionRenderControllerで深度有効な`ArtifactImageLayer`の`ImageF32x4_RGBA`をGPUテクスチャキャッシュから取得し、深度メッシュ＋直接SRVの`drawMesh()`へ渡す経路を追加した。これにより、フレームバッファ画像もファイル再読込なしで深度メッシュへ合流する。
- 追記: `OnnxImageDepthEstimator`を追加し、ONNX Runtime + DirectMLで一般的なRGB NCHW入力／1ch深度出力モデルを実行できるCoreプロバイダを用意した。バックエンド未導入、モデル不在、推論失敗時はエラーを保持して呼び出し側がCPUフォールバックへ切り替えられる。
- 2026-08-18 — ObjectDetectorのONNX接続
- 関連: `ArtifactCore/include/AI/ObjectDetector.ixx`、`ArtifactCore/src/AI/ObjectDetector.cppm`
- 対応: 既存の仮検出を維持しつつ、ONNX／DirectMLモデルを初期化して、一般的なN行6列（`x1,y1,x2,y2,score,class`）またはYOLO系の検出テンソルを読み取る経路を追加した。推論失敗時は既存フォールバックへ戻る。
- 懸念: YOLOv8の転置出力、ラベルファイル、NMS、モデル固有の前処理はまだモデルアダプター側で吸収していない。モデルごとの入出力契約を固定してから本番モデルを選ぶ必要がある。
- 追記: YOLOv8系の`[1,channels,boxes]`転置出力とYOLOv5系の`[boxes,85]`形式を判別し、クラス別NMSを追加した。ラベルファイルとレターボックス補正は引き続き未対応。
- 追記: `ObjectDetector`へクラスラベルファイル読み込みとレターボックス前処理／座標逆変換を追加した。これにより、アスペクト比を保持した推論結果を元画像へ戻し、`class_7`ではなくモデルラベル名を返せる。
- 2026-08-18 — ONNX画像セグメンテーション契約
- 関連: `ArtifactCore/include/AI/ImageSegmenter.ixx`、`ArtifactCore/include/AI/OnnxImageSegmenter.ixx`
- 対応: 1chマスク出力を`DepthMap`形式で受ける`IImageSegmenter`と、ONNX／DirectMLのRGB入力・マスク出力プロバイダを追加した。正規化、反転、しきい値処理をCore側で行い、既存のマスク／切り抜き処理へ渡せる。
- 懸念: U2Net等のモデル固有の出力チャネル、アルファ合成、人物ラベル、複数クラスマスクの扱いはモデルアダプターで追加する必要がある。
- 追記: `applySegmentationMask()`を追加し、推定マスクを双線形サンプリングして`ImageF32x4_RGBA`のアルファへ適用できるようにした。切り抜き用途へ渡す前のCore合成段階を固定した。
- 追記: `ArtifactImageLayer::applySegmentationMask()`を追加し、画像レイヤーの現在バッファへマスク切り抜きを明示的に適用できるようにした。元ソースパスは変更せず、レイヤーの編集バッファとキャッシュを更新する。
- 追記: `LuminanceImageSegmenter`を追加し、ONNXモデルがない場合も同じ`IImageSegmenter`契約で簡易マスクを生成できるようにした。これは人物認識ではなく、明度しきい値による明示的な低品質フォールバックである。
- 追記: `ImageSegmentationOptions::applySigmoid`を追加し、確率出力とlogit出力の両方をモデル設定で扱えるようにした。
- 追記: `OnnxImageSegmenter`がモデルの入力テンソル形状を読み取り、固定512ではなくモデル指定の幅・高さで前処理するようにした。動的形状や不正値は安全な既定範囲へフォールバックする。
- 追記: `OnnxImageDepthEstimator`も入力テンソル形状を取得する方式へ統一し、固定384の前処理をモデル指定サイズへ変更した。

# 2026-08-18 — コンポジションのソースエリアと独立ノイズレイヤー（アイデア）

- 関連: コンポジション構成、参照素材、画像処理・レイヤー合成
- 事実: 制作中に参照画像、カラーチャート、HDRI、マスク作成元などを本番レイヤーと同じ場所へ置くと、編集対象と参照対象の区別が曖昧になりやすい。現在、専用のソースエリアは未確定。
- アイデア: コンポジション内に「ソースエリア」を設け、参照専用レイヤーを本番レンダーから除外する。さらに、粒状感・フィルムグレイン・ディザ・TVノイズなどを担う「ノイズレイヤー」を通常の画像／エフェクトとは独立したレイヤー種別として検討する。
- 仮説（未検証）: ノイズを独立レイヤーにすると、適用範囲、合成順、強度、シード、アニメーション、プレビュー／書き出し差分を管理しやすくなる。一方、単なる画像レイヤーとして実装すると、時間変化するシードや色空間、合成モードの責務が不明確になりやすい。
- 価値・懸念: 参照素材と本番素材の誤編集を防ぎ、ノイズを再利用可能な制作要素として扱える。専用レイヤー化は保存形式、GPU／CPU実装、レンダー順、キャッシュ無効化条件を増やすため、最初から広いノイズ種類を抱えない方が安全。
- 次の確認: ソースエリアはグループ＋非レンダー属性で足りるか、ノイズはまずGPU生成の単一方式（例: film grain）から始めるか、またノイズをレイヤー単位・コンポジション単位のどちらで適用するかを決める。
# 2026-08-20 — PBR環境光の色空間と事前フィルタリング

- 関連: `ArtifactCore/src/Graphics/MeshRenderer.cppm`、PBR環境キューブ、Material IBL
- 事実: 既存の環境キューブは equirectangular 画像から生成され、専用 irradiance cube と BRDF LUT を持っていた。specular mip は単純な面内平均だったため、GGX prefilter としては近似に留まっていた。
- 対応: 8bit LDR環境 RGB の sRGB→線形変換、mipごとの GGX importance sampling、環境単独時の IBL 経路、Clearcoat／Transmission／AO／HDR出力の整合を追加した。
- 価値/懸念: HDRI の diffuse、specular、clearcoat、transmission が同じ線形環境値を基準に評価できる。GGX生成はCPU起動時処理であり、サンプル数・環境解像度によって初期化コストが増える。Transmission は厚み・内部散乱を持たない薄い近似である。
- 次の確認: build/runtime を省略しているため、Diligent のキューブサブリソース順序、HLSL `refract` の backend 差、HDR／LDR環境の見た目、起動時コストを実機で確認する。

- 追記: `MaterialAlphaMode` を MeshRenderer の alpha-test／blend 選択へ接続し、`alphaCutoff`（既定値 0.5）を Material、GPU 定数、JSON、3Dレイヤーの数値プロパティまで通した。現時点では enum 専用 UI と glTF インポータの alphaCutoff 読み込みは未対応。
- 追記: 同一 `ArtifactIRenderer` 内の MeshRenderer 間で cubemap／irradiance／BRDF LUT を参照共有する経路を追加した。共有元と共有先は同じ `GpuContext`／device 所有範囲で使う前提で、異なる device 間の共有、LRU解放、プロセス全体キャッシュは未対応。
- 追記: equirectangular 環境画像の cubemap 初期化、GGX prefilter、irradiance convolution のサンプルを bilinear＋水平ラップへ統一した。極方向は clamp するため、極付近の立体角補正と実機画質は未検証。
- 追記: Skybox は IBL と別の描画経路だったため、環境 intensity と Y 回転を同じ定数契約で渡すようにした。背景の露出・方向は揃うが、skybox 専用の露出／トーンマップ設定はまだ分離していない。
- 追記: `ArtifactEnvironmentMapLayer::setHdriPath()` でパスを trim し、レイヤー保存値と `ArtifactIRenderer` の環境ロードキーを一致させた。空白のみの入力は空環境として扱われる。
- 追記: 共通PBR経路でも `refract` の無効方向を検出し、全反射時の transmission Fresnel を 1.0 に固定した。環境反射のフォールバックは維持し、透過寄与だけを抑制する。

# 2026-08-20 — 環境マップ共有キャッシュの次段階

- 関連: `ArtifactCore/src/Graphics/MeshRenderer.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、PBR IBL リソース管理
- 事実: 現在は同一 `ArtifactIRenderer`／同一 device 内でのみ、生成済み cubemap・irradiance・prefilter・BRDF LUT を MeshRenderer 間で参照共有している。同じ環境を別 renderer が読み込む場合は、renderer ごとに生成が発生し得る。
- 仮説（未検証）: device をキーにした共有環境リソース表と、正規化パス＋画像更新世代をキーにした参照カウント付きキャッシュを導入すれば、複数コンポジション／プレビュー間の重複生成を抑えられる。ただし Diligent の device 寿命、スレッド境界、画像変更時の世代破棄、LRU上限を明示しないまま static 所有へ移行するとリークや stale SRV の原因になる。
- 価値・懸念: HDRI の高価なCPU prefilterを再利用できる一方、プロセス全体キャッシュは renderer の単純な所有モデルを変えるため、先に cache key・device lifetime・eviction 契約を固定する必要がある。
- 次の確認: `GpuContext`／device の安定した識別子、環境画像の更新世代、既存 renderer 破棄時の参照解放を確認し、まず同一 device 限定の小さな共有レジストリから設計する。

# 2026-08-20 — PBRプレビューの光学係数整合

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`、`Artifact3DModelViewer` の PBR プレビュー
- 事実: プレビューは誘電体 F0 を固定値 0.04 としており、本レンダラーが Material の specular／IOR から F0 を計算する経路と一致していなかった。
- 対応: プレビューの ColorCB に optical factors を追加し、specular と IOR を既定値付き API から渡して、本レンダラーと同じ F0 の組み立てへ寄せた。
- 価値/懸念: Material Inspector とプレビューの反射量・ガラス感の差を縮められる。プレビューは依然として簡略化した環境光であり、完全な IBL／透過の見た目一致ではない。
- 次の確認: build/runtime を省略しているため、ColorCB の Diligent constant-buffer layout と specular／IOR の実機表示を確認する。
- 追記: プレビューの transmission fallback を固定色から IOR による `refract` 方向ベースの簡略環境サンプルへ変更した。厚み・吸収・内部反射は持たないため、ガラスの物理的な透過表現ではなく、preview readability を目的とした近似である。

# 2026-08-20 — ufbxスキニング属性の共通化

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`、`ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: ufbx は `skin_deformers` の頂点ウェイトとクラスタを提供しているが、既存 importer はそれを Mesh 属性へ変換していなかった。既存 `applySkinning()` は内部 `BoneWeight` 型を探す一方、PMD importer は `QVector4D` 属性を書いていた。
- 対応: ufbx の最大4影響を `boneIndices` / `boneWeights` の `QVector4D` 属性へ正規化し、`applySkinning()` がこの共通形式を処理できるようにした。
- 価値/懸念: FBX/glTF のウェイトデータを後段のCPU LBSへ渡せる。ただしクラスタと骨ノードの名前・階層・アニメーションを Artifact のリグ契約へ保持する処理は未実装で、GPU skinning接続も未検証。
- 次の確認: Mesh の骨格メタデータ契約を定め、clusterのbind matrix／bone node階層／animation stackを保持して、glTF/FBXの実ファイルで変形結果を確認する。

# 2026-08-20 — 非PMDモデルの初期ポーズ表示接続

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`、`Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 対応: ufbx cluster の `geometry_to_world` を初期ポーズ行列として `Mesh::SkinBone` に保持し、モデル設定時に既存CPU LBSを適用してからDiligentの静的頂点バッファへアップロードする経路を追加した。
- 価値/懸念: FBX/glTFの現在ポーズを既存viewerへ渡せる。現時点は読み込み時の一回適用で、アニメーション時間変更ごとの再評価・GPU skinning・bind/rest頂点の非破壊保持は未実装。
- 次の確認: 実ファイルで座標系と `geometry_to_world` の二重変換がないことを確認し、時間サンプラーを導入する場合は元頂点から再計算するキャッシュ契約を追加する。

# 2026-08-21 — 非PMDアニメーションクリップ契約

- 関連: `ArtifactCore/include/Mesh/Mesh.ixx`、`ArtifactCore/src/Geometry/MeshImporter.cppm`
- 対応: ufbx `anim_stacks` の名前・開始時刻・終了時刻を `Mesh::SkinAnimationClip` として保持するAPIを追加した。
- 価値/懸念: FBX/glTFアセットのアニメーション候補を importer から後段へ渡せる。まだ `ufbx_evaluate_scene()` による時刻サンプリングとボーン行列更新は接続していない。
- 次の確認: クリップ選択・時刻評価の所有者をviewerか再生サービスか決め、評価済みsceneの寿命とbind頂点キャッシュを同じ契約で管理する。

# 2026-08-21 — RenderWindowのスキン姿勢更新API

- 関連: `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`、同 cppm
- 対応: 任意のボーン行列列を受け取り、元頂点からCPU LBSを再適用してGPU頂点バッファをdirty化する `setSkinPoseMatrices()` を追加した。初期ポーズも同じAPIを通す。
- 価値/懸念: タイムラインやアニメーション評価側が毎フレーム同じ描画入口を使える。行列生成側はまだ未接続で、GPU skinningではなくCPU頂点再アップロードのため大規模メッシュではコストがある。

# 2026-08-21 — Viewer経由のスキン姿勢更新

- 関連: `Artifact/include/Widgets/Render/Artifact3DModelViewer.ixx`、同 cppm
- 対応: Viewerから `setSkinPoseMatrices()` を呼べる薄い委譲APIを追加した。Viewerは現在のMeshとRenderWindowの存在を確認し、更新後にステータスと再描画を要求する。
- 価値/懸念: 上位のアニメーション評価側がRenderWindow実装へ直接依存せずに姿勢を反映できる。時間サンプラー自体と、CPU再アップロードを間引く更新ポリシーは未実装。

# 2026-08-21 — ufbx時刻評価付きMeshImporter入口

- 関連: `ArtifactCore/include/Geometry/MeshImporter.ixx`、`ArtifactCore/src/Geometry/MeshImporter.cppm`
- 対応: `importMeshFromFileAtTime(path, time, clipIndex)` を追加し、FBX/glTF/GLBでは対象anim stackを選択して `ufbx_evaluate_scene()` を通した評価済みシーンからメッシュ・ウェイト・ポーズ行列を抽出できるようにした。通常のimportは従来どおり未評価シーンを使う。
- 価値/懸念: 時刻指定の非PMDスキニング評価をImporter単体で再現できる入口になった。評価済みsceneは元sceneを参照するため、解放順を維持する必要があり、毎フレーム再読込は高コスト。
- 次の確認: viewer／再生サービスからこのAPIを呼ぶ更新方式か、sceneを長寿命保持するruntime evaluator方式かを選ぶ。

# 2026-08-21 — Viewerの非PMDアニメーション再生入口

- 関連: `Artifact/include/Widgets/Render/Artifact3DModelViewer.ixx`、同 cppm
- 対応: `setAnimationPlaybackEnabled()`、`setAnimationClipIndex()` を追加し、既存の表示タイマーから `loadModelAtTime()` を呼ぶ再生経路を接続した。再生は既定OFF。
- 価値/懸念: 評価済みsceneの現在姿勢をViewerへ反映できるため、非PMDのクリップ再生の最低限の動作経路ができた。現実装はフレームごとにファイルを再評価するため高コストで、長寿命ufbx scene保持またはGPU skinningへの置換が必要。
# 2026-08-20 — hostfxr反復実行セッションの境界

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`、`Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`
- 事実: 既存の CSX 実行は毎回 `CSharpScript.EvaluateAsync` を呼び、Roslyn の `ScriptState` を保持していなかった。そのため Unity の Play Mode のような変数・実行状態の継続ができなかった。
- 対応: C++ 側に `beginScriptSession` / `stepScriptSession` / `endScriptSession` を追加し、C# 側に `ScriptState` を保持する `EvaluateSession` / `ResetSession` を追加した。既存の単発実行 API は維持している。
- 価値/懸念: 同一セッション内の反復評価が可能になる。一方、ソース変更検知、コンパイル失敗時の旧状態維持、AssemblyLoadContext の完全なアンロードは未実装であり、次段階でセッション管理と再コンパイルポリシーを追加する必要がある。
- 次の確認: hostfxr 有効環境で `begin → step → step → end` の状態継続、失敗後の状態、再初期化を実行確認する。
# 2026-08-20 — hostfxrセッション再ロードのトランザクション境界

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`、`Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`
- 事実: Roslyn `ScriptState` を継続するだけでは、ソース変更時に新しいスクリプトを検証してから旧状態を交換する契約がなかった。
- 対応: `reloadScriptSession()` と `ReloadSession` を追加し、新しい `ScriptState` の評価成功後にだけ静的セッションを交換するようにした。失敗時は旧セッションを保持する。
- 価値/懸念: Unity の再コンパイル失敗時に直前の実行状態を維持する挙動へ近づく。AssemblyLoadContext の分離・アンロードとファイル監視はまだ別段階。
- 次の確認: 成功 reload、コンパイル失敗 reload 後の旧変数参照、次ステップ継続を実行確認する。
# 2026-08-20 — hostfxrセッションのソース更新検知

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `reloadScriptSessionFile()` と `isScriptSessionSourceChanged()` を追加し、ファイルの更新時刻を成功した再ロードと一緒に記録するようにした。
- 価値/懸念: UI／既存の更新ループから変更検知とトランザクション再ロードを呼べる。監視スレッドや新規シグナルは導入していないため、呼び出し側がポーリング周期を決める必要がある。ファイルシステムのタイムスタンプ精度に依存する。
- 次の確認: 更新時刻変更、再ロード失敗時の旧セッション維持、ファイル削除時の扱いを hostfxr 有効環境で確認する。
# 2026-08-20 — hostfxr delegate解決エラーの分離

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: hostfxr の export 解決失敗時にロード済みライブラリを後始末し、`get_function_pointer` の戻り値を保存して型名・メソッド名とともに返すようにした。
- 価値/懸念: セッション再ロード失敗と CLR delegate 解決失敗を診断しやすくする。実際の hostfxr バージョン別エラーコードと C# 例外の対応表は未整備。
- 次の確認: hostfxr 有効環境で不在メソッド、未ロードアセンブリ、delegate 解決失敗のエラー文を確認する。
# 2026-08-20 — hostfxrファイルセッションの初回ロード

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `beginScriptSessionFile()` を追加し、初回の `.csx` 読み込み時からソースパスと更新時刻をセッションへ結び付けた。
- 価値/懸念: ファイル開始 → 反復 step → 更新検知 → トランザクション再ロードの一貫した導線ができる。初期コードも `EvaluateSession` で評価するため、最初の宣言・変数を次の step から利用できる。
- 次の確認: `.csx` ファイルを使った初回ロードと再ロードの runtime 検証。
# 2026-08-20 — Roslynブートストラップの明示ビルドターゲット

- 関連: `Artifact/CMakeLists.txt`、`Artifact/scripts/dotnet/Artifact.Scripting/Artifact.Scripting.csproj`
- 事実: CMake は `dotnet` の存在を検出していたが、Roslyn ブートストラップ DLL を生成する custom target は存在せず、C++ 側の探索候補が手動ビルド前提になっていた。
- 対応: NuGet restore をネイティブビルドの副作用にしないため、明示実行用の `ArtifactScriptingDotnet` target を追加した。
- 価値/懸念: `ArtifactScriptingDotnet` を個別に実行してから hostfxr セッションを利用できる。アプリ本体への自動依存は付けていないため、配布パッケージへの DLL コピーは次段階の課題。
- 次の確認: CMake 再生成後に target が見え、Debug／Release の DLL 出力先が C++ の探索候補と一致することを確認する。
# 2026-08-20 — RoslynホストDLLの明示パス指定

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `setScriptHostAssemblyPath()` を追加し、設定された `Artifact.Scripting.dll` を最優先でロードするようにした。未設定時は既存の相対候補探索へフォールバックする。
- 価値/懸念: 起動ディレクトリやインストール配置が異なる環境でもブートストラップ DLL を解決できる。パス設定の保存・UI 統合と、配布時の既定パス決定は別途必要。
- 次の確認: 明示パスでの hostfxr ロードと、存在しない明示パスから既定候補へ戻る挙動を runtime 確認する。
# 2026-08-21 — hostfxrセッションのフレーム時間コンテキスト

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`、`Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`
- 対応: `updateScriptSession(code, timeSeconds, deltaSeconds, frame)` を追加し、Roslyn globals の `Time`、`DeltaTime`、`Frame` を更新してから同一セッションを評価するようにした。
- 価値/懸念: アプリの更新ループから Unity の `Update` 相当の時間情報をスクリプトへ渡せる。スクリプトの実行スレッド、停止処理、例外後のフレームポリシーはまだ呼び出し側の責務である。
- 次の確認: 時間値の継続、フレーム番号、`updateScriptSession` 失敗時の状態保持を runtime 確認する。
# 2026-08-21 — hostfxrセッションの明示ライフサイクルコールバック

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `invokeScriptSessionCallback(functionName)` を追加し、識別子として検証した関数名を同一 Roslyn セッション内で呼び出せるようにした。
- 価値/懸念: `OnEnable`／`Update`／`OnDisable` 相当の規約を呼び出し側で選択できる。関数が存在しない場合の CLR 例外は返すが、コールバックの自動発火順序や停止時の保証はまだ定義していない。
- 次の確認: セッション内で定義した callback の呼び出し、無効な識別子の拒否、例外後の状態継続を runtime 確認する。
# 2026-08-21 — hostfxrセッションの状態スナップショット

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `ScriptSessionSnapshot` と `scriptSessionSnapshot()` を追加し、active 状態、ソースパス、時間、delta、フレーム番号、直近エラーを取得できるようにした。
- 価値/懸念: UI／診断面が内部 `Impl` を直接参照せずにセッション状態を表示・記録できる。スナップショットは同期なしの値コピーであり、現時点ではメインスレッドからの利用を前提とする。
- 次の確認: セッション開始・更新・再ロード・終了でスナップショットが期待どおり遷移することを runtime 確認する。
# 2026-08-21 — hostfxrセッションの直列化

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: セッション操作へ `std::recursive_mutex` を追加し、開始、step、フレーム更新、callback、reload、終了、更新検知、スナップショット取得を直列化した。ファイル開始／ファイル再ロードが内部 API を呼ぶため recursive mutex を選択した。
- 価値/懸念: UI 更新と別の監視処理が同時にセッションへ入っても、hostfxr コンテキストと Roslyn `ScriptState` の同時利用を防げる。スクリプト実行そのものはロック中に同期実行されるため、長時間処理では UI 停滞が起こり得る。
- 次の確認: 実行スレッドと状態取得スレッドを分けた場合の待ち時間、例外後のロック解放を runtime 確認する。
# 2026-08-21 — hostfxrセッションの協調停止

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`、`ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `requestScriptSessionStop()` と `isScriptSessionStopRequested()` を追加し、停止要求後の step／update／callback を拒否するようにした。`endScriptSession()` と新しい session 開始で停止フラグをクリアする。
- 価値/懸念: Play／Stop の外側の状態機械から安全に次回実行を止められる。現在実行中の同期 C# 呼び出しを強制中断する機能ではなく、長時間処理の中断は別途協調キャンセル API が必要。
- 次の確認: 停止要求後の状態スナップショットと再開始時のフラグ初期化を runtime 確認する。
# 2026-08-21 — runtimeconfig.jsonの一時フォールバック

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: DLL 隣接 config、`Artifact.runtimeconfig.json` の順に探索し、両方がない場合は検出した `shared/Microsoft.NETCore.App` の最新 runtime version から一時 `runtimeconfig.json` を生成するようにした。hostfxr shutdown 時に生成ファイルを削除する。
- 価値/懸念: runtimeconfig を出力しない単純な .NET DLL でも hostfxr 初期化を試行できる。ターゲット DLL の TFM や依存 framework が runtime と一致する保証はなく、互換性エラーは hostfxr の診断へ委ねる。
- 次の確認: config なし DLL、既存 `Artifact.runtimeconfig.json`、無効な runtime version の各ケースを runtime 確認する。
# 2026-08-21 — ソース削除を変更として扱う監視契約

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `isScriptSessionSourceChanged()` は `last_write_time` の取得エラーも変更ありとして返すようにした。
- 価値/懸念: ソース削除・一時的なアクセス失敗を監視側が検出できる。再ロード自体は失敗するため、呼び出し側はエラー表示や復旧待ちを行う必要がある。旧 `ScriptState` は破棄しない。
- 次の確認: ファイル削除、再作成、短時間の置換保存を runtime 確認する。
# 2026-08-21 — ソース内容ハッシュによる再ロード検知

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: セッション開始／ファイル再ロード時にソース内容のハッシュを保存し、`isScriptSessionSourceChanged()` で内容差分も確認するようにした。ファイルが開けない場合は変更ありとして扱う。
- 価値/懸念: 同一タイムスタンプや同一サイズで置換された保存も検出しやすくなる。ポーリングごとにファイル内容を読むため、大きなスクリプトや高頻度監視では呼び出し周期を抑える必要がある。
- 次の確認: 同一サイズ・短時間保存、改行だけの変更、削除後の再作成を runtime 確認する。
# 2026-08-21 — CSharpScriptEngine診断状態の直列化

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: `isInitialized()`、出力 callback 設定、`getLastError()`、`hasError()`、`clearError()` をセッション mutex で保護した。
- 価値/懸念: 更新処理と診断 UI／ログ取得が同時に走っても、エラー文字列や callback の参照競合を避けられる。単発 `executeScript()` 自体の完全な非同期実行モデルはまだ導入していない。
- 次の確認: 実行失敗と診断取得を別スレッドから行った場合の runtime 挙動を確認する。
# 2026-08-21 — hostfxr context再利用

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: 既存の host context と `load_assembly`／`get_function_pointer` delegate が有効な場合は、runtime を再初期化せず Assembly の追加ロードへ進むようにした。
- 価値/懸念: セッション開始後の追加 DLL ロードで host context を上書きしてリークするリスクを抑えられる。異なる runtimeconfig／TFM の Assembly を同一 context にロードできるかは CLR の解決結果に依存する。
- 次の確認: 同一 runtime 上での複数 Assembly ロードと、異なる TFM の拒否時エラーを runtime 確認する。
# 2026-08-21 — hostfxr load_assembly ABIの是正

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: 自己定義していた `load_assembly_fn` は coreclr delegate の呼び出し ABI と一致していなかった。
- 対応: ローカルの .NET 8／9／10 SDK 付属 `coreclr_delegates.h` と照合し、assembly path・load context・reserved の 3 引数契約へ修正した。
- 価値/懸念: hostfxr 有効環境での Assembly ロード時のスタック／レジスタ不整合リスクを下げる。実 SDK ヘッダを使った ABI 照合と runtime 検証は未実施。
- 次の確認: `coreclr_delegates.h` の対象 SDK 版と宣言を照合し、実 DLL のロードを確認する。
# 2026-08-21 — UnmanagedCallersOnly評価ABIの統一

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `get_function_pointer` で取得した `UnmanagedCallersOnly` メソッドを、hostfxr の 6 引数 component entry point として直接呼ぶ旧 `evaluate()` 経路が実メソッド ABI と一致していなかった。
- 対応: `evaluate()` を `DotnetRuntimeHost::invokeUtf8()` へ統一し、C# 側の `IntPtr argument`／`IntPtr result`／`int capacity` 契約で呼び出すようにした。
- 価値/懸念: Windows／Linux の文字幅分岐と誤った 6 引数呼び出しを除去できる。任意の C# メソッドシグネチャを呼ぶ汎用 API ではなく、UnmanagedCallersOnly の UTF-8 bridge 専用である。
- 次の確認: `EvaluateCode`／`EvaluateSession`／任意の UnmanagedCallersOnly メソッドを runtime 確認する。
# 2026-08-21 — hostfxr delegate typeとUnmanagedCallersOnly指定の是正

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: 自己定義 enum は `hdt_load_assembly=1`／`hdt_get_function_pointer=3` としていたが、SDK の enum 連番では `hdt_get_function_pointer=6`／`hdt_load_assembly=7`。また `get_function_pointer` の delegate type に `nullptr` を渡すと既定 component delegate になり、`UnmanagedCallersOnly` 指定にならない。
- 対応: .NET SDK の `hostfxr.h`／`coreclr_delegates.h` と照合し、enum 値を修正。`(const char_t*)-1` 相当の `UNMANAGEDCALLERSONLY_METHOD` を渡すようにした。
- 価値/懸念: 正しい delegate 取得と C# bridge 呼び出しに必要な ABI 契約へ近づけた。SDK 版ごとの ABI 変更がないことは runtime で確認する必要がある。
- 次の確認: `EvaluateCode` と `EvaluateSession` の function pointer 解決を runtime 確認する。
# 2026-08-21 — hostfxr／runtime version選択の数値比較

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: hostfxr directory と `shared/Microsoft.NETCore.App` の候補選択が文字列比較だったため、`10.0.x` と `9.0.x` のようなメジャーバージョン順を誤る可能性があった。
- 対応: ドット区切りの数値部分を比較する `runtimeVersionGreater()` を追加し、hostfxr と runtime config fallback の両方で使用するようにした。
- 価値/懸念: 複数メジャー／パッチ版が共存する環境でも数値上の最新を選択できる。preview suffix の厳密な SemVer precedence は未定義で、数値部分を優先する。
- 次の確認: 8／9／10 共存環境で選択された DLL と runtime version を runtime 確認する。
# 2026-08-21 — hostfxr失敗時のcontext後始末

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: delegate 取得または初回 `load_assembly` 失敗時に `hostfxr_close` を呼び、`hostContext_` と delegate pointer をクリアする `resetRuntimeContext()` を追加した。ライブラリ自体の解放は従来どおり shutdown 時に行う。
- 価値/懸念: 失敗した CLR context を次回ロードへ持ち越さず、再初期化可能な状態へ戻せる。hostfxr が部分初期化状態でも close が安全であることは SDK runtime の確認が必要。
- 次の確認: delegate 解決失敗、Assembly ロード失敗後の再初期化を runtime 確認する。
# 2026-08-21 — runtimeconfig生成先の依存解決整合

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 対応: 自動生成する runtimeconfig をまず DLL 隣接の標準名 `<assembly>.runtimeconfig.json` とし、書き込み不可の場合だけ hash 付き temp パスへフォールバックするようにした。
- 価値/懸念: Assembly と依存ファイルの基準ディレクトリを揃えやすくなる。DLL 配置先が読み取り専用の場合は temp config となり、依存 Assembly の解決は runtime／deps 契約に依存する。
- 次の確認: 書き込み可能・読み取り専用の DLL 配置で Roslyn 依存 Assembly が解決されることを runtime 確認する。
# 2026-08-21 — セッション終了時の時刻状態リセット

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `finalize()` は `Time`／`DeltaTime`／`Frame` を初期化していたが、通常の `endScriptSession()` はソース監視だけを破棄し、時刻状態を保持していた。
- 対応: `endScriptSession()` でも時刻・デルタ・フレームをゼロへ戻し、終了後の snapshot が次回セッションの状態を誤って示さないようにした。
- 価値/懸念: セッション境界の状態が明確になる。実行中の C# 側 globals は `ResetSession` 後の次回評価で再利用されるため、runtime で再開始時の観測値を確認する必要がある。
- 次の確認: セッション終了→再開始直後の `Time`／`DeltaTime`／`Frame` の runtime 確認。
# 2026-08-21 — CSharpScriptEngine公開操作のhost状態保護

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: セッション API と診断 API は mutex を取得していたが、`initialize()`、`execute()`、`executeScript()`、`executeScriptFile()`、`evaluate()` は hostfxr state と `lastError_` を無保護で操作していた。
- 対応: 既存の再帰 mutex をこれらの公開操作にも適用した。`loadAssembly()` や file wrapper の再入は `std::recursive_mutex` で許容する。
- 価値/懸念: UI スレッドと preview／script 更新経路が同時に host state を触る場合の競合を抑えられる。C# callback が同一 engine を再入するケースは runtime で確認が必要。
- 次の確認: 並行ロード・評価と callback 再入時の deadlock／再入挙動を runtime 確認する。
# 2026-08-21 — ファイル駆動tick APIの集約

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`, `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: Unity 風のファイル反復には、呼び出し側が毎フレーム source 読込、変更検知、reload、globals 更新、評価を個別に組み立てる必要があった。
- 対応: `updateScriptSessionFile()` を追加し、active session の source path が変わった場合または内容が変わった場合に reload してから、同じ tick の code を `Time`／`DeltaTime`／`Frame` と共に評価するようにした。
- 価値/懸念: preview／editor update loop からの導入面を一つにできる。reload 成功後の tick 評価が失敗した場合は新 source metadata が先に更新されるため、旧状態へ戻すトランザクション性は今後の検討対象。
- 次の確認: 同一 path の変更、別 path への切替、reload 成功後の tick 失敗を runtime 確認する。
# 2026-08-21 — C#例外本文のC++側伝播

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: C# の各 bridge は例外文字列を result buffer に書いていたが、C++ の `invokeUtf8()` は non-zero return 時に buffer を `result` へコピーせず、数値コードだけを保存していた。
- 対応: return code 判定前に buffer を result へコピーし、例外本文がある場合は `lastError_` にも含めるようにした。
- 価値/懸念: CSX の compile／runtime error を editor 側で表示しやすくなる。固定 64 KiB buffer を超える例外本文は従来どおり切り詰められる。
- 次の確認: compile error、callback error、reload error で例外本文が UI まで届くことを runtime 確認する。
# 2026-08-21 — Frame型の符号なし契約整合

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`, `Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`
- 事実: C++ の session frame は `std::uint64_t` だが、C# `SessionGlobals.Frame` と `SetSessionTime()` の parser は `long` だった。
- 対応: C# 側を `ulong`／`ulong.Parse` へ変更し、非負のフレーム番号契約を揃えた。
- 価値/懸念: 大きなフレーム番号を符号反転させず保持できる。C# script 側で `Frame` を `long` 引数へ渡す場合は明示 cast が必要になる。
- 次の確認: 通常範囲、`long.MaxValue` 近傍、符号付き変換エラーの runtime 確認。
# 2026-08-21 — File session読み込みエラーの遮断

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: file open 成功後の read error を確認していなかったため、部分的な source を session bootstrap／reload／tick に渡す可能性があった。
- 対応: `beginScriptSessionFile()`、`reloadScriptSessionFile()`、`updateScriptSessionFile()` で `file.bad()` を確認し、I/O error 時は評価せずエラーを返すようにした。
- 価値/懸念: 部分 source による不可逆な状態更新を避けられる。replace-save 中の一時的な空／不完全ファイルは read 成功扱いになり得るため、変更検知と reload の runtime policy は残る。
- 次の確認: read error、replace-save、file lock 中の reload で旧状態が維持されることを runtime 確認する。
# 2026-08-21 — 単発CSX読み込みエラーの整合

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: session file API では read error を遮断したが、単発の `executeScriptFile()` は部分読み込み後にそのまま評価する可能性が残っていた。
- 対応: `executeScriptFile()` にも `file.bad()` 検査を追加し、session／単発のファイル入力契約を揃えた。
- 価値/懸念: 壊れた CSX source を Roslyn へ渡す経路を減らせる。replace-save の一時的な完全ファイルは引き続き通常の compile error として扱われる。
- 次の確認: 単発 CSX の read error と replace-save 中の評価結果を runtime 確認する。
# 2026-08-21 — Source変更検知のread error扱い

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `isScriptSessionSourceChanged()` は source hash 用の iterator read 後に stream error を確認していなかった。
- 対応: binary read 後に `file.bad()` を確認し、I/O error は変更ありとして reload／旧状態保持の判断へ渡すようにした。
- 価値/懸念: 読み込み失敗を「内容が変わっていない」と誤認しない。呼び出し側が reload を試みるため、アクセス不能状態では毎 tick 変更ありになる可能性がある。
- 次の確認: source lock／一時アクセス不能時に旧 ScriptState が保持され、復旧後に reload できることを runtime 確認する。
# 2026-08-21 — Session time payloadのlocale固定

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`, `Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`
- 事実: C++ の `ostringstream` は process locale の影響を受け得る一方、C# `SetSessionTime()` は `InvariantCulture` で time／delta を解析していた。
- 対応: `updateScriptSession()`、変更 source reload、`tickScriptSession()` の payload stream を `std::locale::classic()` に固定した。
- 価値/懸念: 小数点がカンマになる環境でも C# parser と契約が一致する。NaN／Infinity の扱いは C# の `double.Parse` と runtime 契約に依存する。
- 次の確認: comma-decimal locale 下で time／delta が正しく渡ること、特殊浮動小数値のエラー方針を runtime 確認する。
# 2026-08-21 — Session timeの有限値検証

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: C# `double.Parse` は `NaN`／`Infinity` も入力として扱えるため、非有限の time／delta が globals に入る可能性があった。
- 対応: `updateScriptSession()`、`updateScriptSessionFile()`、`tickScriptSession()` の入口で `std::isfinite()` を検証し、非有限値を host に渡さずエラーにした。
- 価値/懸念: session の時間状態を有限値に限定できる。負の time／delta は逆再生や seek の用途があるため、現時点では許可している。
- 次の確認: NaN／Infinity の拒否と、負の time／delta の意図した利用を runtime 確認する。
# 2026-08-21 — Session timeの往復精度固定

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: session payload の `ostringstream` は既定 precision 6 で、time／delta の小数桁を丸める可能性があった。
- 対応: 全 `SetSessionTime` payload に `std::numeric_limits<double>::max_digits10` の precision を設定し、locale と合わせて double の往復可能精度を確保した。
- 価値/懸念: 高精度 delta と長時間 time の tick でも C# 側の値を安定させられる。C# 側で計算した値の再量子化は別契約である。
- 次の確認: 小さい delta、長時間 time、倍精度 round-trip の runtime 確認。
# 2026-08-21 — Session time payload生成の共通化

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: update、file reload、standard tick が locale／precision 設定を個別に持っていた。
- 対応: `makeSessionTimePayload()` に classic locale、`max_digits10`、`time;delta;frame` 形式を集約し、3 経路から共通利用するようにした。
- 価値/懸念: 将来の session tick API 追加時に数値直列化契約がずれにくい。payload の delimiter escape は不要な数値専用形式として維持する。
- 次の確認: 共通 helper を通る全経路の C# round-trip を runtime 確認する。
# 2026-08-21 — C#セッションglobalsの境界リセット

- 関連: `Artifact/scripts/dotnet/Artifact.Scripting/ArtifactScriptHost.cs`
- 事実: C++ の `endScriptSession()` は時刻状態をリセットしていたが、C# 側の static `SessionGlobals` は `ResetSession()` 後も前セッションの値を保持していた。
- 対応: `ResetSession()` で `Time`／`DeltaTime`／`Frame` もゼロ化した。
- 価値/懸念: 次の session bootstrap が前セッションの時間情報を誤って観測しない。初回 tick まで globals はゼロ値になる。
- 次の確認: 終了→再開始→bootstrap 評価時の globals がゼロであることを runtime 確認する。
# 2026-08-21 — 標準Update tickの追加

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`, `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: 既存の `updateScriptSession()` は呼び出し側から毎フレーム評価コードを渡す汎用経路で、Unity 風の標準 `Update()` 呼び出しは別途組み立てる必要があった。
- 対応: `tickScriptSession()` を追加し、`Time`／`DeltaTime`／`Frame` を更新してからセッション内の `Update()` を評価するようにした。
- 価値/懸念: preview loop が固定ライフサイクルへ接続しやすくなる。`Update()` を定義しないスクリプトはエラーになるため、標準スクリプト契約または optional callback 方針を後続で決める必要がある。
- 次の確認: `Update()` 定義あり／なし、停止要求中、callback 例外時の runtime 挙動を確認する。
# 2026-08-21 — 変更フレームの二重評価回避

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `updateScriptSessionFile()` は source 変更時に `reloadScriptSessionFile()` を実行した後、同じ source code を通常 update として再評価していた。
- 対応: source 変更時は `SetSessionTime` を先に行い、reload 自体をその tick の評価として扱う。同一 source が継続している場合だけ `updateScriptSession()` を実行する。
- 価値/懸念: 変更フレームの副作用・ログ・状態更新の二重実行を避けられる。reload code 内で `Update()` を明示的に呼ぶ設計では、その呼び出しが一度だけ行われる。
- 次の確認: source 変更フレームの副作用が一回だけで、reload 失敗時に旧状態が維持されることを runtime 確認する。
# 2026-08-21 — セッション停止要求の再開API

- 関連: `ArtifactCore/include/Script/CSharpScriptEngine.ixx`, `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `requestScriptSessionStop()` は stop flag を立てるだけで、session を破棄せずに再開する公開経路がなかった。
- 対応: `clearScriptSessionStopRequest()` を追加し、active session の stop flag を明示的に解除できるようにした。
- 価値/懸念: editor の pause／resume 操作を session lifetime と分離できる。停止要求は実行中コードを中断せず、次の host 呼び出しを拒否する協調停止である。
- 次の確認: stop→clear→tick の再開、stop 中の reload／callback 拒否を runtime 確認する。
# 2026-08-21 — PlaybackServiceとのscript tick境界

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`, `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `ArtifactPlaybackService` の frame tick は playback engine、composition 同期、RAM／disk preview、音声状態を横断しており、C# session lifecycle を直接受け取る専用境界は現状存在しない。
- 判断: playback service に C# session を直接埋め込まず、`CSharpScriptEngine::tickScriptSession()`／`updateScriptSessionFile()` を独立した editor／preview tick API として維持する。
- 価値/懸念: 既存再生・キャッシュ経路の責務拡大を避けられる。将来統合する場合は、再生フレーム通知・script 実行順序・停止時の session policy を先に定義する必要がある。
- 次の確認: script tick を受け渡す専用 service または明示的な playback observer の設計が必要になった時点で再評価する。
# 2026-08-21 — File session sourceの単一読込

- 関連: `ArtifactCore/src/Script/CSharpScriptEngine.cppm`
- 事実: `beginScriptSessionFile()`／`reloadScriptSessionFile()` は `ostringstream::str()` を評価、hash、metadata 設定で複数回呼び出していた。
- 対応: 1 回生成した `code` を session 評価と source hash の両方へ渡すようにした。
- 価値/懸念: 同一 file read の内容を評価と変更検知の基準に揃え、不要な一時 string 生成を減らす。ファイルが評価中に更新される race 自体は別途残る。
- 次の確認: replace-save／同時書き換え時の source metadata と reload 結果を runtime 確認する。

# 2026-08-21 — ufbxスキニング行列の契約（訂正）

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`, `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: ufbxのヘッダ実装では `geometry_to_world` がすでに `bone_node_to_world * geometry_to_bone` として更新される。別途 `pose * inverseBind` を合成すると逆バインドを二重適用する。
- 対応: `Mesh::skinPoseMatrices()` は `geometry_to_world` に相当する `poseMatrix` をそのまま返す契約へ戻した。
- 価値/懸念: FBX/glTFの初期姿勢とアニメーション時刻評価でufbxの行列契約を保てる。実ファイルごとの座標系・評価結果はruntime確認が必要。
- 次の確認: 非TポーズのFBX/glTF/GLBでbind姿勢、clip先頭、clip中間時刻の変形を確認する。

# 2026-08-21 — ufbx論理頂点と展開頂点の対応

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`
- 事実: ufbxのskin weightsは論理頂点単位だが、MeshImporterは面コーナー単位へ展開している。`vertex_indices` を介さずにウェイトを書き込むと、共有頂点・UV分割時に別頂点のウェイトが割り当たる。
- 対応: いったん論理頂点用のウェイト配列へ保持し、面コーナー展開時に `vertex_indices[idx]` で出力頂点へコピーするようにした。
- 価値/懸念: FBX/glTFのスキニング対象で頂点ウェイトと変形対象の対応が安定する。実ファイルの複数メッシュ・UV seamを含む受入れはruntime確認が必要。
- 次の確認: 複数メッシュ、共有頂点、UV seamを含むモデルでウェイト対応を確認する。

# 2026-08-21 — CPU LBSのゼロウェイト復元

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: 複数フレーム評価で有効なbone influenceがない頂点を何もしないままにすると、前フレームの変形位置が残る。
- 対応: 各評価で元位置・元法線を基準にし、総ウェイトがゼロなら元データへ復元、1未満/超過なら位置を総ウェイトで正規化するようにした。
- 価値/懸念: 部分的なskin deformerや壊れた入力でも、変形がフレーム間で蓄積しない。壊れた入力の診断表示は別途必要。
- 次の確認: 無ウェイト頂点と非正規化ウェイトを含むモデルでフレーム往復を確認する。

# 2026-08-21 — 3D Layer JSON復元コードの配置

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: アニメーション設定のJSON復元コードが時刻指定ロード関数内に混入し、同関数のスコープに存在しない `obj` を参照していた。
- 対応: 時刻指定ロードから除去し、`fromJsonProperties()` のモデルロード後にclip数で範囲制限して復元するようにした。
- 価値/懸念: 非PMDアニメーションのロード経路とプロジェクト復元経路の責務を分離できる。JSON復元の実動作はruntime確認が必要。
- 次の確認: 保存→再読込で animation.enabled と animation.clipIndex が維持されることを確認する。

# 2026-08-21 — ufbxウェイトの有限値検証

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`
- 事実: skin weight の単純な `<= 0` 判定はNaNを除外できず、合計ウェイトの正規化結果を非有限値にする可能性がある。
- 対応: influence と合計ウェイトを `std::isfinite` で検証し、有限かつ正の値だけを最大4 influenceへ採用するようにした。
- 価値/懸念: 壊れたFBX/glTF入力で頂点位置へNaNが伝播する可能性を下げる。入力ファイル単位の警告・診断表示は未実装。
- 次の確認: 不正ウェイトを含むファイルで、読み込み後のboundsと描画データが有限であることを確認する。

# 2026-08-21 — ufbxアニメーション時刻の有限値フォールバック

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`
- 事実: 公開時刻評価APIへNaNが渡ると、`std::clamp` 後も非有限時刻のまま評価へ伝播する可能性がある。
- 対応: 非有限時刻は選択clipの開始時刻へフォールバックしてから範囲clampするようにした。
- 価値/懸念: 外部制御や壊れたキー値から評価結果が不定になるリスクを下げる。時刻入力元の診断表示は未実装。
- 次の確認: 不正時刻入力時にclip開始姿勢が安定して返ることを確認する。

# 2026-08-21 — Viewerの非PMDアニメーション既定再生

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: Viewerにはclip再生APIがあったが、既定の再生フラグがfalseで、clipを持つFBX/glTF/GLBも静止表示になっていた。
- 対応: モデル読み込み結果にanimation clipがある場合だけ、自動再生を有効化するようにした。clipのないモデルや読み込み失敗時は無効のままにする。
- 価値/懸念: 非PMDスキニング対応がViewerの既定導線で視認できる。現状は時刻ごとに再importするため、複雑なモデルではCPU/I/O負荷のruntime確認が必要。
- 次の確認: clipありモデルの初回表示、再生停止、clip切替、読み込み失敗後の再読み込みを確認する。

# 2026-08-21 — Viewerアニメーション状態の可視化

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: Viewer statusにはbone数とclip数は表示されていたが、再生フラグの状態は表示されていなかった。
- 対応: `Animation: Playing/Stopped` をstatusへ追加した。
- 価値/懸念: 非PMDモデルが「clipを持つが停止中」なのか「再生中」なのかを、追加のイベント配線なしで確認できる。
- 次の確認: clipなしモデル、clipあり自動再生、明示停止の各status表示を確認する。

# 2026-08-21 — Packed bone indexの有限値ガード

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: packed `QVector4D` のbone indexはfloatで保持されるため、壊れた入力のNaNを整数へ変換すると未定義な変換になり得る。
- 対応: packed indexを整数化する前に有限値を確認し、非有限slotをスキップするようにした。
- 価値/懸念: 不正な非PMDウェイト属性がskin評価を壊す可能性を下げる。小数indexの診断・拒否は未実装。
- 次の確認: NaN/小数/範囲外indexを含む属性で、評価がクラッシュせず元頂点へ復元されることを確認する。

# 2026-08-21 — LBSウェイトの有限値ガード

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: `+∞` のweightは `w > 0` を通過し、総ウェイトと変形位置の正規化を壊す可能性がある。
- 対応: LBSへ加算するweightを有限かつ正の値に限定した。
- 価値/懸念: 不正なpacked／歴史的weight属性からNaN位置が生成される経路をさらに狭める。
- 次の確認: NaN/∞ weightを含む属性でboundsが有限に保たれることを確認する。

# 2026-08-21 — LBS総ウェイトのオーバーフローガード

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: 個別weightが有限でも、異常に大きい値の累積で総ウェイトが∞になる可能性がある。
- 対応: 総ウェイトが有限かつ正の場合だけ変形結果を採用し、それ以外は元頂点復元へ落とすようにした。
- 価値/懸念: 異常な属性から無限大除算やNaN位置が生成される経路を閉じる。
- 次の確認: 極端なweight合計でboundsと頂点配列が有限に保たれることを確認する。

# 2026-08-21 — Packed bone indexの整数値検証

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: packed bone indexはfloat属性で運ばれるが、1.5のような小数を整数へ暗黙変換すると別boneを誤参照する。
- 対応: 有限値に加えて `std::trunc(index) == index` を満たすslotだけをLBSへ採用するようにした。
- 価値/懸念: 壊れたpacked属性によるbone誤参照を防ぐ。入力診断をUIへ出す経路は未実装。
- 次の確認: 小数indexを含む属性で該当slotだけが無効化されることを確認する。

# 2026-08-21 — Viewerのアクティブclip名表示

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: Viewer statusはclip数を表示していたが、複数clipを持つモデルで現在の選択対象を識別できなかった。
- 対応: 選択中clipの名前を `Clips: count (name)` として表示し、名前が空の場合は `-` を表示するようにした。
- 価値/懸念: FBX/glTFのclip選択状態を追加操作なしで確認できる。clip選択UI自体は未追加。
- 次の確認: 名前付きclip、空名clip、clip index変更後のstatus表示を確認する。

# 2026-08-21 — Preview GPU skinningの大規模rig fallback

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`, `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`
- 事実: Solid viewportの `SkinningCB` は128行列固定で、129本以上のboneをGPUへ渡すと高いindexのinfluenceが無視される。
- 対応: 128本以下だけGPUスキニングを使い、超過rigは既存CPU LBSへフォールバックするようにした。
- 価値/懸念: リグサイズによる部分変形を避け、DiligentのGPU経路とCPU互換経路の境界を明示できる。大規模rigのGPU palette拡張は未実装。
- 次の確認: 128本・129本のrigで変形結果が連続し、後者が欠損しないことを確認する。

# 2026-08-21 — Diligent PreviewのGPU skinning接続

- 関連: `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: ViewerのDiligent Solid viewportは従来、MeshをCPU変形してから通常頂点だけを描画していた。既存のMesh importerが持つbone属性とpose行列はGPUへ渡されていなかった。
- 対応: 頂点にbone index/weight入力を追加し、128本のSkinningCBをDiligent dynamic uniform bufferへ更新するGPU LBS VSを接続した。128本超はCPU fallbackとした。
- 価値/懸念: FBX/glTF/GLBのViewer再生で、通常フレームはCPU頂点再書き込みを避けてGPUでスキニングできる。行列レイアウト、D3D12/Vulkan shader portability、実機表示は未検証。
- 次の確認: D3D12/Vulkan双方でbind姿勢・clip中間姿勢・wireframeが一致し、GPU/CPU fallbackの境界が正しいことを確認する。

# 2026-08-21 — GPU pose配列の完全性fallback

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: GPU経路へrigより少ない行列配列を渡すと、未提供boneがSkinningCBのidentity値を使い、部分的に誤変形する。
- 対応: pose行列数がbone数以上の場合だけGPU LBSを使い、不足時はCPU LBSへfallbackするようにした。
- 価値/懸念: 不完全な外部pose入力で静かにidentity変形される挙動を避ける。呼び出し側のpose不足を診断するUIは未実装。
- 次の確認: 完全pose、不足pose、128本超rigの3ケースで変形結果を確認する。

# 2026-08-21 — GPU pose更新時の頂点再upload抑制

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: GPU LBSへ切り替えた後もpose更新で `meshDirty_` を立てており、毎フレームbase頂点バッファを再uploadしていた。
- 対応: 完全poseかつ128本以下のGPU経路ではSkinningCBだけを更新し、`meshDirty_` はCPU fallback時だけ立てるようにした。
- 価値/懸念: GPUスキニングのCPU upload削減効果を保てる。GPU定数更新・描画同期の実機確認は未実施。
- 次の確認: 連続pose更新中に頂点VB uploadが初回以外発生せず、GPU結果だけが更新されることを確認する。

# 2026-08-21 — CPU fallback時のGPU bone属性無効化

- 関連: `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: 128本超rigをCPU LBSへfallbackしてもbone属性をGPUへ残すと、GPUが128本未満のinfluenceだけ部分再評価し、混合ウェイトを壊す可能性がある。
- 対応: `gpuSkinningActive_` を導入し、CPU fallback時はVBへゼロweightを出してGPU skinningを無効化するようにした。
- 価値/懸念: CPUで完成したposeをGPUが再変形しない。GPU/CPU切替時のVB再uploadと実機表示は未確認。
- 次の確認: 128本以下、129本以上、混合高indexウェイトの3ケースで結果が一致することを確認する。

# 2026-08-21 — GPU LBS weight overflow guard

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: CPU LBSでは有限値を検証していたが、埋め込みGPU VSは正の無限大weightを受け入れる条件だった。
- 対応: GPU VSでも極端なweightと総ウェイトを上限比較で除外し、CPU経路と同じく異常値を変形計算へ入れないようにした。
- 価値/懸念: D3D12/Vulkanで共通のHLSL比較だけを使い、NaN/∞由来の頂点破壊を抑える。実機shader compilerの受入れは未確認。
- 次の確認: GPU pathで異常weightを含む属性を描画し、頂点がNaN化しないことを確認する。

# 2026-08-21 — Viewer skinning経路の可視化

- 関連: `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`, `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: 128本以下のrigはGPU、超過rigや不完全poseはCPUへfallbackするが、Viewer statusから経路を識別できなかった。
- 対応: `gpuSkinningActive()` を追加し、statusへ `Skinning: GPU/CPU/-` を表示するようにした。
- 価値/懸念: 実機での経路確認と性能比較が容易になる。表示は経路選択を示すだけで、shader実行成功までは証明しない。
- 次の確認: GPU対応rig、CPU fallback rig、非スキンmeshのstatus表示を確認する。

# 2026-08-21 — SkinningCB dirty更新

- 関連: `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: GPU LBS導入後も、poseが変化しない描画フレームで128行列のSkinningCBを毎回mapしていた。
- 対応: mesh/pose変更時だけ `skinPoseDirty_` を立て、CB upload成功後にクリアするようにした。
- 価値/懸念: 静止フレームのCPU submissionとbuffer mapを削減する。device lossやMap失敗時の再試行はdirtyを維持する。
- 次の確認: 静止描画、pose更新、Map失敗後の再uploadを実機で確認する。

# 2026-08-21 — 停止中のアニメーションクリップ切替

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`, `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: クリップ番号だけを変更すると、再生中は次フレームで評価されるが、停止中は表示中のポーズが更新されなかった。
- 対応: クリップ変更時に選択クリップの `timeBegin` を即時評価し、Viewerでは再生停止状態を保持するようにした。
- 価値/懸念: InspectorやViewerのクリップ選択が停止中でも視覚的に反映される。再インポートによるコストは既存の時刻評価経路と同じ。
- 次の確認: 再生停止中のクリップ切替、再生中の切替、クリップなしmeshの3ケースを実機で確認する。

# 2026-08-21 — Viewerアニメーション状態表示の同期

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: クリップ切替時の再インポート処理が一時的に再生状態を有効化するため、停止中に切り替えるとstatusが一瞬または継続してPlaying表示になる可能性があった。
- 対応: 切替後に元の再生状態を復元し、statusと再描画要求を明示的に更新する。再生／停止操作自体もstatusへ反映する。
- 価値/懸念: UI表示と実際の再生状態のずれを抑える。実機でのタイマー・再インポート競合は未確認。
- 次の確認: 停止中のクリップ切替後にStopped表示と先頭ポーズが一致することを確認する。

# 2026-08-21 — Viewerアニメーション時刻の異常delta保護

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: タイマー停止復帰や時計異常で大きな負値・非有限のdeltaが入ると、クリップ時刻のfmodへ不正値が伝播し得た。
- 対応: deltaを有限値かつ0〜0.25秒へ制限し、animationTimeが非有限の場合はclip先頭へ戻す。
- 価値/懸念: 一時停止復帰時の大きなジャンプとNaN伝播を抑える。実際のタイマー再接続挙動は未確認。
- 次の確認: 長時間停止後の再開、時計差分異常、通常再生の3ケースを実機で確認する。

# 2026-08-21 — 3Dレイヤーのclipループ評価

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: Viewerはclip範囲をループしていたが、3Dレイヤーのdraw経路はコンポジション時刻をそのまま渡し、ufbx側のclamp後に終端ポーズで停止していた。
- 対応: 有効なclip範囲を使って相対時刻を `fmod` し、clip先頭から継続評価するようにした。不正な範囲や非有限時刻は評価をスキップする。
- 価値/懸念: Viewerと3Dレイヤーでアニメーション再生の継続挙動を揃えられる。実ファイルのclip境界を跨ぐ実機確認は未実施。
- 次の確認: clip開始・終端直前・終端超過の3時点でポーズが連続することを確認する。

# 2026-08-21 — ufbx clip範囲の正規化

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`
- 事実: 外部ファイルのanim stackが終端より前の開始時刻を持つ保証は、インポート側では明示されていなかった。
- 対応: 評価時刻のclampと公開clip metadataの双方で `min(begin,end)` / `max(begin,end)` を使い、逆順範囲を正規化した。
- 価値/懸念: 逆順metadataがViewer・3Dレイヤーの時刻評価へ伝播しない。実ファイルでの異常metadataは未確認。
- 次の確認: 正常範囲、同一時刻、逆順範囲のanim stackを読み込んで評価結果を確認する。

# 2026-08-21 — ufbx null anim stackのclip番号整合

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`
- 事実: metadata公開側はnull stackを除外していた一方、時刻評価側は元の配列番号を直接参照しており、null stackが混在すると選択clipがずれる可能性があった。
- 対応: 有効stackだけを数える選択処理に変更し、範囲外は最後の有効stackへ丸めた。
- 価値/懸念: Viewer/3Dレイヤーが表示するclip番号と評価対象のstackを一致させる。null stack混在ファイルは未確認。
- 次の確認: null stackを含むanim stack配列でclip選択と時刻評価を確認する。

# 2026-08-21 — 3DレイヤーのSkin Animation無効化pose

- 関連: `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: 再生中にSkin Animationを無効化すると、従来は最後に評価したアニメーションposeが残っていた。
- 対応: 無効化時に選択clipの `timeBegin` を再評価し、初期poseへ戻すようにした。
- 価値/懸念: 設定を無効化した状態の表示が静止した初期poseになり、Viewerの再生停止操作とは役割を分けられる。実機UI操作は未確認。
- 次の確認: 再生中の無効化、再有効化、clipなしmeshの3ケースを確認する。

# 2026-08-21 — Viewer clip選択番号の可視化

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: statusはclip名だけを表示しており、無名clipや同名clipでは選択対象を区別しにくかった。
- 対応: clip総数に加えて現在のclip index（0-based）を表示するようにした。
- 価値/懸念: UI操作なしでも選択clipの評価対象を確認できる。表示変更のみで、clip選択UI自体は追加していない。
- 次の確認: 無名・同名clipを含むモデルでstatusの番号が評価対象と一致することを確認する。

# 2026-08-21 — Mesh rig変更時のskin base cache無効化

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: `setSkinBones()` はrig paletteだけを更新し、既存のLBS基準頂点cacheを保持していた。
- 対応: ボーンpalette変更時に基準position/normal cacheを消去し、次回LBSで現在の頂点を再取得するようにした。
- 価値/懸念: Mesh再利用時に旧rig由来の基準データが混ざる可能性を抑える。setter経由の複雑なrig差し替えは未確認。
- 次の確認: 同一Meshへ異なるbone paletteを設定して再評価するケースを確認する。

# 2026-08-21 — Mesh clip setterの時刻範囲正規化

- 関連: `ArtifactCore/src/Mesh/Mesh.cppm`
- 事実: importer経由ではclip範囲を正規化しているが、公開setterへ直接渡されるclipには同じ保証がなかった。
- 対応: `setSkinAnimationClips()` でも有限値の逆順範囲を入れ替えるようにした。
- 価値/懸念: importer外で生成されたMeshもViewer／レイヤーの時刻評価契約を満たす。非有限値はゼロ長clipへ正規化し、再生を停止状態にする。
- 次の確認: setterへ正常・逆順・非有限clipを渡した場合の評価挙動を確認する。

# 2026-08-21 — timed importerの非有限時刻入口

- 関連: `ArtifactCore/src/Geometry/MeshImporter.cppm`
- 事実: 非有限時刻をclip先頭へfallbackする処理は存在したが、評価条件が `evaluationTime >= 0` のみでNaN/Inf時には到達しなかった。
- 対応: `-1` の予約された非評価値を除き、非有限時刻も評価経路へ通し、clip先頭へ正規化するようにした。
- 価値/懸念: 公開timed APIへ異常時刻が入ってもbind poseへ不意に戻らず、選択clipの初期poseを返す。実ファイルruntimeは未確認。
- 次の確認: 正常時刻、NaN/Inf、`-1` の3入力で結果が意図どおり分かれることを確認する。

# 2026-08-21 — Viewer clip metadata API

- 関連: `Artifact/include/Widgets/Render/Artifact3DModelViewer.ixx`, `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: Viewerはclip選択setterとstatus表示を持つが、外部UIがclip数や名前を取得するAPIはなかった。
- 対応: `animationClipCount()` と `animationClipName()` を追加し、既存のclip index setterと組み合わせて利用できるようにした。
- 価値/懸念: 新規signal/slotなしで、将来の既存UIやホスト側UIからclip選択を構成できる。UI自体は追加していない。
- 次の確認: モデル未読込、範囲内、範囲外indexの戻り値を確認する。

# 2026-08-21 — 3Dレイヤー clip metadata API

- 関連: `Artifact/include/Layer/Artifact3DModelLayer.ixx`, `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- 事実: 3Dレイヤーはclip indexを保持・編集できたが、外部のInspector／ホストがclip数と名前を取得するAPIはなかった。
- 対応: `skinAnimationClipCount()` と `skinAnimationClipName()` を追加した。
- 価値/懸念: 既存のinteger propertyを置き換えずに、将来のclip selector実装へ接続できる。UIとruntime検証は未実施。
- 次の確認: 未読込、範囲内、範囲外indexの戻り値を確認する。

# 2026-08-21 — GPU/CPU skinning経路切替時の頂点属性再upload

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: 不完全poseのCPU fallback後に完全poseへ戻ると、GPU経路は有効化されても、直前のCPU uploadでゼロ化されたbone属性VBが再uploadされない可能性があった。
- 対応: GPU/CPU経路の状態が変わった場合に `meshDirty_` を立て、bone属性を含む頂点VBを再uploadするようにした。
- 価値/懸念: pose更新ごとの不要なVB uploadは増やさず、経路切替時だけ属性を同期する。実機での切替描画は未確認。
- 次の確認: 完全pose→不完全pose→完全poseの順でGPU/CPU表示と変形結果を確認する。

# 2026-08-21 — GPU skinning boundsの保守的フレーミング

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: GPU LBSではMeshのCPU boundsがbind poseのままなので、アニメーションposeがbind boundsを超えるとpreviewのworld framingで切れる可能性があった。
- 対応: GPU pose行列でbind boundsの8隅を変換した保守的boundsをworld framingに使うようにした。CPU fallbackは既存の変形済みMesh boundsを使用する。
- 価値/懸念: shader変形中の大きなposeでもpreview内に収まりやすい。実機でのbounds精度と過剰拡大は未確認。
- 次の確認: bind pose、四肢が広がるpose、非uniform scale poseで表示範囲を確認する。

# 2026-08-21 — GPU boundsの非有限pose保護

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: 保守的bounds計算へ異常なpose行列が入ると、NaN/Infがworld transformへ伝播する可能性があった。
- 対応: 変換cornerが有限値のときだけboundsへ取り込み、全て不正なら元のbind boundsを維持する。
- 価値/懸念: 異常poseでpreviewのカメラ行列が壊れる可能性を抑える。shader側の異常値挙動は別途runtime確認が必要。
- 次の確認: 正常poseと異常poseを混在させたbounds計算を確認する。

# 2026-08-21 — GPU boundsへのbind範囲union

- 関連: `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- 事実: bone変換cornerだけをbounds化すると、無ウェイト／不正ウェイトでbind位置に残る頂点を範囲から落とす可能性があった。
- 対応: GPU skinned boundsの初期値にbind boundsを含め、変換後範囲とunionするようにした。
- 価値/懸念: 部分的なスキン属性や異常ウェイトを含むmeshでも、静止頂点がpreview外へ出ない。保守的な範囲拡大は残る。
- 次の確認: 無ウェイト頂点と大きく移動するweighted頂点が混在するmeshでフレーミングを確認する。

# 2026-08-21 — Viewer再生再開時の先頭pose同期

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: 再生有効化時はanimationTimeだけをclip先頭へ戻しており、表示中の旧poseは次のtimer tickまで残っていた。
- 対応: 再生を有効化した時点でclip先頭を即時再importし、表示poseと時刻を同期するようにした。
- 価値/懸念: 再生ボタン相当のAPI操作に対する表示遅延をなくす。再importコストは既存の時刻評価経路と同じ。
- 次の確認: 停止中の途中poseから再生を有効化した直後に先頭poseが表示されることを確認する。

# 2026-08-21 — Viewer clip切替時のclock再基準化

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: 再生中のclip切替で再importに要した時間がanimation deltaへ混ざり、切替直後に不要な時間ジャンプが発生し得た。
- 対応: clip切替と先頭pose再評価の完了後にanimation clockを現在時刻へ再基準化した。
- 価値/懸念: clip切替直後の再生開始が安定する。実時間の再import負荷自体は変わらない。
- 次の確認: 再生中にclipを切り替えた直後の時刻連続性を確認する。

# 2026-08-21 — 通常モデル読み込み時のanimation clock同期

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: 時刻付き読み込みやclip切替ではclockを再基準化していたが、通常の`loadModel()`では読み込み時間が初回deltaへ混ざる可能性があった。
- 対応: 通常読み込みで初期animation stateを設定した直後にclockを現在時刻へ同期した。
- 価値/懸念: モデル読込直後の再生開始が読み込み時間に依存しない。実タイマー接続は未確認。
- 次の確認: アニメーション付きモデルの通常読み込み直後に初期poseから再生が始まることを確認する。

# 2026-08-21 — timed load完了時のanimation clock同期

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: 外部callerから`loadModelAtTime()`を直接呼ぶ場合、再import時間が次回の再生deltaへ混ざる余地が残っていた。
- 対応: timed loadの成功・失敗を問わず、状態更新前にanimation clockを現在時刻へ再基準化した。
- 価値/懸念: timer経路と外部時刻設定経路でclock初期化を統一する。再importコストは変わらない。
- 次の確認: 外部時刻設定直後の再生再開とtimer再生のdelta連続性を確認する。

# 2026-08-21 — Viewer内部animation timeのclip範囲正規化

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: timed Importerは時刻をclip範囲へclampしていたが、Viewerの`animationTime_`には呼び出し元の範囲外／非有限値が残る可能性があった。
- 対応: 読み込み後のactive clip metadataを使い、内部animation timeも有限値かつclip範囲内へ正規化した。
- 価値/懸念: 表示poseとViewer内部時刻の不一致を抑える。clip metadataが無効な場合は0へ戻す。
- 次の確認: 範囲内、範囲外、NaN/Infのtimed loadで内部時刻と表示poseが一致することを確認する。

# 2026-08-21 — Viewer clear時のanimation clock初期化

- 関連: `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- 事実: `clearModel()` は再生状態と時刻を初期化していたが、clock自体は古い時刻を保持していた。
- 対応: モデル削除時にもclockを現在時刻へ再基準化した。
- 価値/懸念: clear→loadの境界で古いdeltaが再利用されない。実タイマー接続は未確認。
- 次の確認: 再生中のclear→新規モデルloadで初回poseが安定することを確認する。
# 2026-08-21 — packed bone indexの整数範囲検証

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm` の `Mesh::applySkinning`
- **確認できた事実:** packed `QVector4D` のbone indexはfloatで保持され、finiteかつ整数であることだけを確認してから `int` に変換していた。
- **対応:** `int` の表現範囲外の値を変換前に除外し、破損したスキニング属性による範囲外変換を防いだ。
- **価値または懸念:** 不正なインデックスは従来どおりその頂点の有効影響から除外され、通常のFBX/glTF/PMD経路には影響しない。
- **次に確認すべきこと:** 実データでのGPU/CPU経路の表示確認は、ビルド・実行許可後に行う。
# 2026-08-21 — skin cluster単位のbone palette

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm` のufbx skin import
- **確認できた事実:** 以前はbone node単位でskin clusterを重複排除していたため、複数メッシュで同一nodeを使いながらgeometry行列が異なる場合、最初のclusterの行列を共有してしまう可能性があった。
- **対応:** paletteとweight lookupを`ufbx_skin_cluster*`単位に変更し、node単位の対応は親階層の補助情報だけに限定した。
- **価値または懸念:** 複数メッシュのgeometry-to-bone行列を保持でき、GPU上限を超えた場合も既存のCPUフォールバックへ自然に移行する。重複clusterによりbone数が増える可能性がある。
- **次に確認すべきこと:** 複数メッシュ・共有boneを含むFBX/glTFで、実行時の姿勢とboundsを確認する。
# 2026-08-21 — skin cluster lookupのelement ID fallback

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm` のufbx weight lookup
- **確認できた事実:** weightが参照するclusterはdeformerのリスト要素であり、sceneのclusterリストと同一ポインタであることを前提にしていた。
- **対応:** ポインタlookupに加えて`element_id` lookupを用意し、参照が別ポインタになっても同じclusterへ解決できるようにした。
- **価値または懸念:** 既存のcluster単位paletteの精度を維持しつつ、ufbxリスト間の参照差によるウェイト消失を防ぐ。
- **次に確認すべきこと:** 実FBX/glTFでウェイト数と表示姿勢を確認する。
# 2026-08-21 — CPU LBSの非有限変換結果保護

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm` のCPU skinning
- **確認できた事実:** bone indexとweightの検証はあったが、外部から渡された行列の変換結果がNaN/Infになる場合は頂点へ加算され得た。
- **対応:** 各influenceの変換後position/normalを検証し、非有限な影響だけをスキップするようにした。
- **価値または懸念:** 不正なpose行列によるbounds汚染と表示破綻を局所化する。GPU経路の行列検証は別途runtime確認が必要。
- **次に確認すべきこと:** 異常poseを含むCPU fallbackで、bind頂点復元とboundsが維持されることを確認する。
# 2026-08-21 — GPU pose行列の有限値検証

- **関連ファイル・機能:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **確認できた事実:** CPU LBSでは変換結果を検証したが、GPU経路はpose行列をそのままuniform bufferへ送っていた。
- **対応:** 初期poseと更新poseの16要素を検証し、非有限値を含む場合はGPU skinningを無効化してCPU fallbackへ切り替える。
- **価値または懸念:** shader内でNaNが頂点・boundsへ伝播するのを防ぐ。CPU fallback側でも不正influenceは既存の有限値検証で除外される。
- **次に確認すべきこと:** 異常pose入力時のGPU/CPU切り替えとmesh属性再uploadをruntimeで確認する。
# 2026-08-21 — Mesh boundsの非有限頂点スキップ

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm` の`Mesh::updateBounds`
- **確認できた事実:** スキニング入力や外部mesh属性に非有限positionがあると、先頭頂点を初期値に使うbounds計算全体がNaN/Infへ汚染され得た。
- **対応:** 有限なpositionだけでmin/maxを計算し、有限頂点が一つもない場合は既存boundsを保持する。
- **価値または懸念:** CPU/GPU previewのframingへ不正頂点が伝播する範囲を抑える。入力データ自体の修復は行わない。
- **次に確認すべきこと:** 異常poseと部分的に壊れたmesh属性で、既存bounds保持と再評価を確認する。
# 2026-08-21 — CPU fallbackからGPU skinningへ戻す際のbind復元

- **関連ファイル・機能:** `ArtifactCore/include/Mesh/Mesh.ixx`, `ArtifactCore/src/Mesh/Mesh.cppm`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **確認できた事実:** 129骨以上・不正poseなどでCPU fallbackを通った後、128骨以下の有効poseへ戻ると、mesh属性がCPU変形済みのままGPU shaderへ渡る可能性があった。
- **対応:** `Mesh::restoreSkinningBase()`を追加し、CPU→GPU切替時にbind-space position/normalを復元してからGPU属性を再uploadする。
- **価値または懸念:** GPU/CPU切替やpose異常からの復帰で二重変形を防ぐ。runtimeでの切替確認は未実施。
- **次に確認すべきこと:** 128骨境界と不正poseからの復帰で、GPU表示が一度だけposeを適用することを確認する。
# 2026-08-21 — CPU LBSの加算結果finite検証

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm` の`Mesh::applySkinning`
- **確認できた事実:** 各matrix変換結果がfiniteでも、極端なweightや複数influenceの加算でposition/normalの累積値がoverflowする可能性があった。
- **対応:** 累積position/normalを頂点属性へ代入する前にfinite検証し、異常時は既存のbind-space復元分岐へ送る。
- **価値または懸念:** CPU fallbackからNaN/Infがboundsやrendererへ伝播する経路をさらに抑える。
- **次に確認すべきこと:** 極端なweightを含む破損meshでbind復元が機能することをruntime確認する。
# 2026-08-21 — ufbx blend shape offsetのMesh取り込み基盤

- **関連ファイル・機能:** `ArtifactCore/include/Mesh/Mesh.ixx`, `ArtifactCore/src/Mesh/Mesh.cppm`, `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **確認できた事実:** ufbxはblend deformer/channel/shapeとsource vertex単位のposition/normal offsetを公開しているが、Meshには保持契約がなかった。
- **対応:** `Mesh::BlendShape`を追加し、ufbxのshape offsetをflatten後のface-corner vertexへ展開して保持する。target shapeとchannel keyframe shapeを重複統合する。
- **価値または懸念:** 既存bone skinningとは独立した入力データとして、将来のmorph適用・アニメーション評価へ接続できる。現段階ではoffsetを頂点へ適用していない。
- **次に確認すべきこと:** blend offsetをskin前に適用する順序、channel weightとkeyframe補間、複数meshの同名shape統合を実装・確認する。
# 2026-08-21 — evaluated blend shapeのImporter適用

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **確認できた事実:** ufbxのchannel weightは評価済みsceneに含まれるため、時間指定ロードごとに現在のblend状態を取得できる。
- **対応:** channel weightを`Mesh::BlendShape::weight`へ保持し、flatten済みposition/normalへoffsetをImporter段階で適用してから既存bone skinningへ渡す。
- **価値または懸念:** FBX/glTFの単純なblend shapeは、時間指定ロードとbone skinningの順序を維持したまま表示できる基盤になった。複数keyframeの厳密な補間とruntime UI制御は未実装。
- **次に確認すべきこと:** keyframe effective weightの扱い、shape weight範囲、skin→morph順序が必要なデータでの実機確認。
# 2026-08-21 — blend keyframeごとのeffective weight

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **確認できた事実:** channelのtarget shapeとkeyframe shapeは同一weightではなく、ufbxがkeyframeごとに`effective_weight`を保持している。
- **対応:** targetにはchannel weight、keyframe shapeには`effective_weight`を使用してMeshのblend shape weightへ反映する。
- **価値または懸念:** 時間指定で評価された中間morphが、全keyframeへ一律channel weightを掛けるより正確になる。同名shapeの統合は最大weight方式のまま。
- **次に確認すべきこと:** 複数keyframeの同時出現と同名shapeの合成規則をruntimeで確認する。
# 2026-08-21 — Viewer statusへのmorph数表示

- **関連ファイル・機能:** `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- **確認できた事実:** blend shapeをMeshへ取り込んでもViewer statusではbone数とclip数しか確認できなかった。
- **対応:** 既存statusへ`Morphs`件数を追加し、読み込まれたdeformerデータの存在をinspection時に確認できるようにした。
- **価値または懸念:** 新しいsignal/slotやQt CSSを追加せず、既存statusだけで診断情報を増やした。
- **次に確認すべきこと:** 実ファイルでmorph数表示と実際の形状変化が一致することをruntime確認する。
# 2026-08-21 — blend shapeの同一pointer二重加算防止

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **確認できた事実:** ufbx channelはtarget shapeとkeyframe listの両方から同じshape pointerを返す場合があり、単純収集ではoffsetを二重加算する可能性があった。
- **対応:** channel内のshape referenceをpointer単位で重複排除してからoffsetを収集する。
- **価値または懸念:** 単純morphの形状量がtarget/keyframeの列挙形式に依存しなくなる。同名だが別pointerのshape合成規則は未確定。
- **次に確認すべきこと:** 複数channelで同一shapeを共有するデータのweight合成をruntime確認する。
# 2026-08-21 — mesh内共有blend shapeのoffset重複防止

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **確認できた事実:** 複数channelが同じufbx blend shape pointerを参照する場合、channel単位の重複排除だけではoffsetがmesh内で複数回加算され得た。
- **対応:** source mesh単位で収集済みshape pointerを記録し、offsetは一度だけ加算する。weight更新は既存の統合処理へ委ねる。
- **価値または懸念:** shared corrective shapeの位置offsetがchannel数に比例して膨らむ問題を防ぐ。複数channelのweight合成は引き続き最大値規則。
- **次に確認すべきこと:** 同一shapeを共有する複数channelの意図的な加算が必要なデータをruntime確認する。
# 2026-08-21 — morph再適用順序の公開境界

- **関連ファイル・機能:** `ArtifactCore/include/Mesh/Mesh.ixx`, `ArtifactCore/src/Mesh/Mesh.cppm`
- **確認できた事実:** blend shapeのbase cacheはImporterでmorphをskin前に適用するため成立するが、skin後にweightだけを変更するとbone poseとの再評価順序を別途管理する必要がある。
- **対応:** `applyBlendShapes()`は追加したが、順序管理なしの公開weight setterは追加しない。runtime編集はmorph→skinを一体で再評価できるAPI設計後に接続する。
- **価値または懸念:** morph weight変更による二重変形を未完成APIから発生させない。
- **次に確認すべきこと:** morph weight、skin pose、base cacheを一つのdeformer評価スナップショットへまとめる。
# 2026-08-21 — deformer評価順序の統一

- **関連ファイル・機能:** `ArtifactCore/include/Mesh/Mesh.ixx`, `ArtifactCore/src/Mesh/Mesh.cppm`, `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** 3D layerの初期pose、時間pose、手動poseが個別に`applySkinning()`を呼び、morphを再評価する順序が共通化されていなかった。
- **対応:** `Mesh::applyDeformers()`を追加し、skin base復元→blend shape適用→bone skinningの順序へ統一した。
- **価値または懸念:** 将来のruntime morph weight編集でも、morphとskinの二重変形を避ける評価入口になる。Viewer専用GPU経路は既存のpose uploadを維持する。
- **次に確認すべきこと:** morph weight変更とclip再生を同一meshで行った場合のbase cache更新をruntime確認する。
# 2026-08-21 — runtime morph weightのskin pose再適用

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm`, `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** Meshは最後に適用したskin poseを保持しておらず、weight変更後に同じposeを安全に再適用する経路が不足していた。
- **対応:** `activeSkinMatrices`を保持し、`setBlendShapeWeight()`から`applyDeformers()`を呼ぶことで、base復元→morph→skinを一体で再評価する。
- **価値または懸念:** runtime weight変更時の二重変形を避けられる。GPU preview側のmorph編集UIと実ファイルでのruntime確認は未実施。
- **次に確認すべきこと:** clip再生中のweight変更、GPU/CPU切替後の再評価、morph名・weight編集UIの接続を確認する。
# 2026-08-21 — 非スキンmeshのblend shape頂点対応

- **関連ファイル・機能:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **確認できた事実:** face-corner展開時のblend shape offset転送がskin属性配列の存在条件に内包され、skinを持たないmorph meshではoffsetが全て失われていた。
- **対応:** source vertex indexの取得をskin転送から分離し、skinなしでもblend shapeのposition/normal offsetをflattened vertexへ転送する。
- **価値または懸念:** FBX/glTF/GLBの非スキンmorphとスキンmorphで同じoffset経路を使える。実ファイルでのruntime形状確認は未実施。
- **次に確認すべきこと:** 複数source mesh、UV seamによるface-corner複製、skin有無混在モデルでoffset対応を確認する。
# 2026-08-21 — blend shape weightの読取契約

- **関連ファイル・機能:** `ArtifactCore/include/Mesh/Mesh.ixx`, `ArtifactCore/src/Mesh/Mesh.cppm`, `Artifact/include/Layer/Artifact3DModelLayer.ixx`, `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** runtime weight setterは存在したが、現在値のgetterがなく、Inspectorや保存処理が編集状態を読み取れなかった。
- **対応:** Meshと3D layerにindex検証付き`blendShapeWeight()`を追加した。
- **価値または懸念:** 非PMD deformerをUI・JSONへ接続するための最小の読書き契約が揃う。実際のUI接続と永続化は未実装。
- **次に確認すべきこと:** blend shape weightをProperty Editorの責務へ接続するか、専用Morph UIとして設計する。
# 2026-08-21 — 3D deformer weightのJSON復元

- **関連ファイル・機能:** `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** blend shapeのruntime setter/getterは揃ったが、レイヤーJSONにはweightが保存されず、プロジェクト再読込でdeformer編集状態が失われていた。
- **対応:** `deformers.blendShapes`配列へ名前とweightを保存し、モデル読込後に名前一致でweightを復元する。復元後はboundsも更新する。
- **価値または懸念:** FBX/glTF/GLBのmorph編集状態をモデル再読込後も維持できる。未知のshape名は安全に無視し、複数同名shapeの編集規則は未定義。
- **次に確認すべきこと:** JSON schemaの命名統一、同名shapeの扱い、skin animation再生と保存weightの組み合わせをruntime確認する。
# 2026-08-21 — Morph weightのProperty Editor接続

- **関連ファイル・機能:** `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** 3D layerには既存の`setLayerPropertyValue()` overrideがあり、Morph専用の動的PropertyGroupを追加できる。
- **対応:** `deformers.blendShapes.<index>.weight`を動的生成し、shape名を表示ラベルに設定。編集値は既存のdeformer再評価APIへ接続した。
- **価値または懸念:** FBX/glTF/GLBのMorph weightを既存Property Editorから編集・保存できる導線が成立する。shape名変更や同名shapeの編集規則は未定義。
- **次に確認すべきこと:** property cacheの再構築タイミング、アニメーション中のweight編集、0〜1以外のDCC weight表現をruntime確認する。
# 2026-08-21 — timed skin reload時のMorph weight保持

- **関連ファイル・機能:** `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** `setAnimationTime()`は毎回importerでmeshを再生成するため、runtimeまたはProperty Editorで設定したMorph weightが新しい評価meshの初期値に戻っていた。
- **対応:** timed load前に既存shapeの名前とweightを保存し、skin pose初期化後に同名shapeへ再適用する。
- **価値または懸念:** skin animation再生中もMorph編集状態を維持できる。名前変更・同名shape・shape追加削除時の対応は名前一致の範囲に限定される。
- **次に確認すべきこと:** Property Editorのweight animationとclip切替を組み合わせたruntime確認。
# 2026-08-21 — Contents ViewerのMorph操作API

- **関連ファイル・機能:** `Artifact/include/Widgets/Render/Artifact3DModelViewer.ixx`, `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`, `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **確認できた事実:** Model ViewerはMorph数をstatus表示するだけで、shape名・weightの読書きAPIがなかった。Mesh更新後にGPU preview geometryを再uploadする明示入口も不足していた。
- **対応:** ViewerへMorph count/name/weight getter/setterを追加し、RenderWindowに`refreshMeshGeometry()`を追加してrevision更新後のgeometry再uploadを要求する。
- **価値または懸念:** Contents Viewerや将来の専用Morph UIから非PMD deformerを操作できる。既存animation poseを保持したままmesh geometryだけを更新するruntime確認は未実施。
- **次に確認すべきこと:** GPU skin pose中のMorph変更、CPU fallbackからの復帰、Viewer UIからの操作導線。
# 2026-08-21 — Morph後のLBS入力cache更新

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **確認できた事実:** `applyDeformers()`はMorph適用後に`applySkinning()`を呼ぶが、LBS側の`skinBasePositions`が初回skin入力のままだと、後から変更したMorph offsetがskin計算へ入らない。
- **対応:** Morph適用直後のposition/normalを当該skin評価の入力cacheへ更新する。次回評価では`applyBlendShapes()`がblend baseから再構成するため累積変形は起きない。
- **価値または懸念:** MorphとLBSの順序が実際の評価データにも反映され、runtime weight変更・timed reload・CPU fallbackで同じ挙動になる。dual-quaternion等の別deformer順序は未対応。
- **次に確認すべきこと:** Morph weight変更後の骨pose、weightを0へ戻す操作、GPU/CPU切替のruntime確認。
# 2026-08-21 — Viewer CPU fallbackのdeformer入口統一

- **関連ファイル・機能:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **確認できた事実:** Diligent Viewerの129本以上または不正pose時のCPU fallbackだけが`applySkinning()`を直接呼び、Morph→LBSの共通順序を迂回していた。
- **対応:** 初期CPU fallbackとpose更新時のCPU fallbackを`applyDeformers()`へ変更した。
- **価値または懸念:** GPU ViewerのCPU fallbackでもMorph付き非PMDモデルのdeformer順序が3D layerと一致する。GPU shader側のblend shape直接評価は行わず、Morphはgeometry upload済みsourceへ適用する設計。
- **次に確認すべきこと:** 128本境界でのMorph保持、CPU→GPU再入場、pose不正時の復帰をruntime確認する。
# 2026-08-21 — Contents Viewer timed reload時のMorph保持

- **関連ファイル・機能:** `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- **確認できた事実:** Viewerのanimation playbackも各時刻でmeshを再importするため、Viewer APIで設定したMorph weightだけが再評価時に初期化されていた。
- **対応:** `loadModelAtTime()`で旧meshの名前付きweightを保存し、新meshの同名shapeへ再適用してからRenderWindowへ渡す。
- **価値または懸念:** 3D layerとContents ViewerでMorph保持の挙動を統一できる。再import失敗時やshape名変更時のruntime挙動は未確認。
- **次に確認すべきこと:** Viewer再生中のweight変更とGPU geometry refreshの組み合わせをruntime確認する。
# 2026-08-21 — Morph animationと手動overrideの分離

- **関連ファイル・機能:** `Artifact/src/Layer/Artifact3DModelLayer.cppm`, `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- **確認できた事実:** timed reload時に全Morph weightを保持すると、DCC側でアニメーションしているblend channelの評価結果まで固定してしまう。
- **対応:** layerとViewerに名前ベースの手動weight override mapを持たせ、timed reloadではoverrideだけを再適用する。新規load/clearではViewer overrideを破棄する。
- **価値または懸念:** DCCのMorph animationを維持しつつ、Property Editor／Viewerから明示的に編集したshapeだけを固定できる。override解除APIと同名shapeの規則は未実装。
- **次に確認すべきこと:** animated Morphと手動overrideの混在、JSON復元後のclip再生、override解除UXをruntime確認する。
# 2026-08-21 — Morph手動overrideの解除経路

- **関連ファイル・機能:** `Artifact/include/Layer/Artifact3DModelLayer.ixx`, `Artifact/src/Layer/Artifact3DModelLayer.cppm`, `Artifact/include/Widgets/Render/Artifact3DModelViewer.ixx`, `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- **確認できた事実:** 手動Morph overrideを設定できても解除APIがなく、DCC側のMorph animationへ戻すにはモデル再読込が必要だった。
- **対応:** Layer／Viewerに`clearBlendShapeWeightOverride()`を追加。Layerは現在フレームで再評価し、Viewerは現在時刻・clipで再importしてDCC評価へ戻す。
- **価値または懸念:** 手動編集とDCCアニメーションの切替が明示的に可能になった。再評価時のファイルI/Oコストと同名shapeの扱いはruntime確認が必要。
- **次に確認すべきこと:** 複数overrideの一部解除、animation disabled時の解除、Property Editorからの解除導線。
# 2026-08-21 — JSONに評価済みMorph値を固定しない

- **関連ファイル・機能:** `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- **確認できた事実:** JSONがmeshの全Morph weightを保存すると、DCC animationの現在フレーム評価値まで手動overrideとして復元され、保存後にMorph animationが停止する。
- **対応:** JSONへは`blendShapeWeightOverrides_`の手動編集値だけを`override: true`付きで保存する。既存形式のname/weight entriesは後方互換として読み込む。
- **価値または懸念:** animated Morphは保存・再読込後も評価継続し、手動編集だけを固定できる。旧JSONに含まれる評価値は手動値として解釈されるため、旧形式の完全な判別はできない。
- **次に確認すべきこと:** animated Morphを含むJSONの保存・再読込、旧JSON互換、override解除後の保存状態をruntime確認する。
# 2026-08-21 — LBS法線のnormal matrix適用

- **関連ファイル・機能:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **確認できた事実:** skinning法線がbone matrixの`mapVector()`を直接使っており、非均一scaleを含むFBX/glTF/GLBリグでは法線が正しく直交化されない可能性があった。
- **対応:** 各bone influenceの法線変換を`QMatrix4x4::normalMatrix()`（逆転置3x3）で評価し、既存のweight合成・正規化へ渡す。
- **価値または懸念:** 非PMDモデルのスケール付きskin poseでライティング法線の破綻を抑えられる。GPU Viewerのshader側法線変換は別経路のため、GPU/CPUの非均一scale parityはruntime確認が必要。
- **次に確認すべきこと:** 非均一scale骨、zero-scaleに近い行列、不正行列の有限値ガードをruntime確認する。
# 2026-08-21 — Diligent GPU法線のnormal matrix parity

- **関連ファイル・機能:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **確認できた事実:** CPU LBSはnormal matrixへ改善した一方、Viewer HLSLはbone／world matrixの3x3を直接法線へ適用していた。
- **対応:** HLSLでboneごと、およびworld transformに逆転置3x3を適用し、CPU経路と非均一scale時の法線変換前提を揃える。
- **価値または懸念:** GPU previewとCPU fallbackのライティング法線差を縮小できる。singular matrix時のGPU inverse挙動はruntime未確認。
- **次に確認すべきこと:** zero-scale近傍のpose、D3D12/Vulkan shader compiler parity、CPU fallbackとの画像比較。
# 2026-08-21 — GPU normal matrixのsingular guard

- **関連ファイル・機能:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **確認できた事実:** HLSLの`inverse()`はzero-scaleまたは特異なbone/world matrixで未定義値を生成する可能性があり、CPU側の有限値ガードと一致していなかった。
- **対応:** bone/worldの3x3行列式を検査し、閾値以下では入力法線をfallbackとして使う。通常行列では逆転置normal matrixを維持する。
- **価値または懸念:** 不正poseでGPU previewのNaN伝播を抑制できる。GPU shader compilerごとの`determinant/inverse`挙動は未検証。
- **次に確認すべきこと:** D3D12/Vulkan shader compile、zero-scale pose、CPU fallbackとの法線一致をruntime確認する。
## 2026-08-21: JSON復元Morphはoverride mapにも登録する

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm` / 3DモデルMorphのJSON復元
- **気づき:** 保存対象を手動Morph overrideだけに分離しても、JSON復元時にMeshのweightだけを書き換えると、後続の時刻再評価でoverride mapにない値が失われる。復元時は名前解決後に同じoverride mapへ登録し、Meshの評価値も更新する必要がある。
- **価値・懸念:** Morphアニメーションを保持したまま、保存した手動編集値を再評価後も維持できる。重複名がある場合は既存の名前ベース仕様に従うため、名前一意性は別途確認が必要（未検証）。
- **次に確認:** 実モデルのJSON再読込後に時刻を進めても手動Morph値が維持されることを実行確認する。
## 2026-08-21: 初回GPUスキニング選択時もCPU変形を戻す

- **関連:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm` / `setMesh()`
- **気づき:** モデル読込側はソフトウェア経路でも利用できるよう初期ポーズをCPU評価している。その直後に初回GPUスキニングへ切り替えると、CPU変形済み頂点へGPUが同じポーズを再適用して二重変形になる可能性があった。
- **対応:** 有限な128本以下の初期ポーズをGPU経路へ採用する直前に`restoreSkinningBase()`を呼び、Morph適用後・スキニング前のソース形状をGPUへ渡す。
- **次に確認:** GPU初回表示とCPUフォールバック復帰で、同一ポーズが一度だけ適用されることをruntime確認する。
## 2026-08-21: GPU表示中のMorph編集でも二重スキニングを防ぐ

- **関連:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm` / `refreshMeshGeometry()`
- **気づき:** `Mesh::setBlendShapeWeight()`はCPU経路を維持するためMorph適用後に現在のskin poseも評価する。GPUスキニング中にその結果をそのまま再アップロードすると、GPUが同じskin poseを二度適用する。
- **対応:** GPUが有効なジオメトリ更新時は`restoreSkinningBase()`でMorph後・スキニング前へ戻してから頂点バッファを再構築する。
- **次に確認:** GPU表示でMorph weightを変更したとき、CPU表示と同じ形状になることをruntime確認する。
## 2026-08-21: PMD/PMXローダーとAsset Browserの形式一覧を一致させる

- **関連:** `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`, `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`
- **確認できた事実:** `MeshImporter`にはPMDローダーがある一方、Asset Browserと共通ファイルフィルタの3D形式一覧にはPMDが含まれていなかった。旧来のPMX分岐はPMDバイナリ判定を共有しており、PMX対応を保証していなかった。
- **対応:** 3Dフィルタとモデルアイコン判定へ実装済みの`pmd`を追加し、ファイルメニューの対応アセット／3Dフィルタにも追加した。PMXは実装済みと誤認しないよう入口へ追加していない。
- **価値または懸念:** 既存のPMD系スキニングをAsset Browser経由でも選択できる。PMXは別形式のため、専用パーサーなしでは対応済みと扱えない。
- **次に確認:** PMDファイルをAsset Browserから選択し、実際の読込結果と表示アイコンをruntime確認する。
- **補足:** PMXは未対応としてProperty Editorのファイル選択候補からも除外した。
- **追加確認:** `AssetDirectoryModel`の3D判定にも`pmd`を追加し、ファイルツリー段階でPMDが除外されないようにした。
- **追加対応:** Viewerのbackend表示にも`PMD`を追加し、読込成功時に`none`と誤表示しないようにした。
## 2026-08-21: GPUスキニング法線のゼロ長フォールバック

- **関連:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm` / solid vertex shader
- **気づき:** normal matrixや入力法線が退化している場合、GPU shaderの`normalize(0)`は不定値になり、NaN法線として表示へ伝播する可能性がある。
- **対応:** bone合成後とworld変換後の法線長を検査し、閾値未満なら入力法線または固定Z法線へフォールバックする。
- **次に確認:** 退化法線・特異変換を含むモデルで、GPU表示がNaN化せずCPU経路と同様に安定することをruntime確認する。
## 2026-08-21: CPU LBS入力頂点・法線の有限値ガード

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm` / `Mesh::applySkinning()`
- **気づき:** GPU shaderには退化法線のフォールバックがあっても、CPU fallbackで入力position/normalがNaN・Infまたはゼロ長なら、無効値をそのまま復元してしまう可能性がある。
- **対応:** LBS評価前にpositionを原点、normalを+Yへフォールバックし、CPU経路でも有限値と非ゼロ法線を保証する。
- **次に確認:** 壊れた入力属性を含むFBX/glTFで、CPU fallbackがNaNを出力しないことをruntime確認する。
## 2026-08-21: Blend Shape offsetの有限値ガード

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm` / `Mesh::applyBlendShapes()`
- **気づき:** shape weightだけを検証しても、position/normal offset自体がNaN・InfならMorph後の頂点・法線が壊れ、その後のLBSへ伝播する。
- **対応:** base属性と各offsetを有限値検証し、退化法線は+Yへ戻す。normal offset適用後もゼロ長なら既存法線を保持する。
- **次に確認:** 不正なMorph属性を含むモデルでCPU/GPU経路が無効値を出力しないことをruntime確認する。
## 2026-08-21: Position offsetなしのMorphでもnormal offsetを適用

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm` / `Mesh::applyBlendShapes()`
- **気づき:** position offset配列のサイズをshape適用の入口条件にしていると、normal offsetだけを持つBlend Shapeが無視される。
- **対応:** position offsetとnormal offsetを独立して範囲検証し、どちらか一方だけ存在するshapeも適用可能にした。
- **次に確認:** normal-only shapeを含むFBX/glTFで法線変化が反映されることをruntime確認する。
## 2026-08-21: JSON復元失敗時に旧Morph overrideを破棄

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm` / `fromJsonProperties()`
- **気づき:** 新しいsourcePathが存在しない場合、表示は停止しても旧モデルのMorph override mapが残り、後続保存へ誤ったdeformer状態が混入する可能性があった。
- **対応:** 欠落sourceの復元分岐でMorph override mapを明示的にクリアする。
- **次に確認:** 既存モデル表示中に存在しないsourceをJSON復元した場合、旧Morph値が再保存されないことをruntime確認する。
## 2026-08-21: 欠落source復元時に内部Meshも空にする

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm` / `fromJsonProperties()`
- **気づき:** `meshLoaded_`だけをfalseにして旧Meshを保持すると、非表示状態でもboundsや動的Morph Propertyが旧モデル由来になる可能性がある。
- **対応:** sourceが存在しない復元分岐でMeshを初期化し、source sizeも空Meshから再計算する。
- **次に確認:** 欠落sourceを復元後にProperty Editorや保存処理が旧Morph情報を参照しないことをruntime確認する。
## 2026-08-21: Morph JSONのoverrideフラグを復元時に尊重

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm` / Morph JSON restore
- **気づき:** 保存形式に`override`フラグがあるのに、復元側が常にweightを手動overrideとして適用していた。
- **対応:** `override:false`を明示したエントリは復元対象から除外し、未指定は従来互換でtrueとして扱う。
- **次に確認:** DCC評価値を保存した旧JSONと手動override JSONの双方で、再評価後のMorph値が意図どおりになることをruntime確認する。
## 2026-08-21: 同名MorphのJSON復元規則をtimed reloadと一致

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm` / Morph JSON restore
- **気づき:** timed reloadは名前一致の全shapeへoverrideを適用する一方、JSON復元は最初の1件でループを終了していた。
- **対応:** JSON復元の`break`を除去し、同名Morph全件へ同じweightを適用する挙動に統一した。
- **次に確認:** 同名shapeを含むモデルで、初回JSON復元と時刻再評価後のMorph値が一致することをruntime確認する。
## 2026-08-21: CPU/GPU LBS法線の退化フォールバックを一致

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm` / `Mesh::applySkinning()`
- **気づき:** GPU側は合成法線のゼロ長を入力法線へ戻す一方、CPU側は無条件に`normalized()`してゼロ法線を出す可能性があった。
- **対応:** CPU側も合成法線の有限値・長さを検証し、退化時は元法線へフォールバックする。
- **次に確認:** 同一モデル・同一poseでCPU fallbackとGPU skinningの法線が一致することをruntime確認する。
## 2026-08-21: ufbxのnull mesh要素を安全にスキップ

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm` / FBX・glTF mesh抽出
- **気づき:** ufbxのmesh配列にnullまたは空meshが含まれる場合、頂点数集計・skin cluster抽出前の参照でクラッシュする可能性があった。
- **対応:** 頂点数集計と実体抽出の双方でnull／空meshをスキップする。
- **次に確認:** 部分的または破損したFBX/glTFを読み込んでも、残りの有効meshだけで安全に表示できることをruntime確認する。
## 2026-08-21: ufbx抽出直後のposition/normalを有限化

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm` / FBX・glTF vertex extraction
- **気づき:** ViewerのGPU経路ではMeshがCPU LBSを通らず直接頂点バッファへ入る場合があり、インポート属性のNaN・Inf・ゼロ法線がCPU側ガードを経由しない可能性がある。
- **対応:** ufbxからposition/normalを取り出した直後に有限値と法線長を検証し、positionは原点、normalは+Yへフォールバックする。
- **次に確認:** 不正頂点属性を含むFBX/glTFをGPU経路へ直接渡しても表示が破綻しないことをruntime確認する。
## 2026-08-21: 非LBS skinning方式をインポート時に明示

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm` / ufbx skin deformer
- **気づき:** ufbxはDual Quaternion／Blended DQを表現できるが、現在のMesh評価はLBSのみで、非LBSモデルを無言でLBSとして扱うと品質差を見落としやすい。
- **対応:** 非LBS方式を検出したときに警告を出す。既存のLBS抽出・CPU/GPU経路は変更しない。
- **次に確認:** 非LBSモデルのログで警告が出ること、LBSモデルでは警告が出ないことをruntime確認する。
## 2026-08-21: Meshへskinning method metadataを保持

- **関連:** `ArtifactCore/include/Mesh/Mesh.ixx`, `ArtifactCore/src/Mesh/Mesh.cppm`, `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **気づき:** 非LBS方式を警告するだけでは、後続のDual Quaternion実装やViewer診断が入力方式を参照できない。
- **対応:** MeshにLinearBlend/Rigid/DualQuaternion/BlendedDualQuaternionの方式metadataを追加し、ufbx deformer方式をimport時に記録する。現行の評価自体は引き続きLBS。
- **次に確認:** ViewerやCPU fallbackがmetadataを利用して方式表示・DQ経路選択へ発展できることを確認する。
## 2026-08-21: skinning method metadataをViewer診断へ表示

- **関連:** `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- **気づき:** Meshへ入力方式を記録しても、ViewerではGPU/CPUの実行経路しか表示されず、LBSとして評価されたDQ入力を見分けられなかった。
- **対応:** statusへ実行経路（GPU/CPU）と入力方式（LBS/Rigid/DualQuaternion/BlendedDQ）を併記する。
- **次に確認:** 各方式のモデルでstatus表示がImporter metadataと一致することをruntime確認する。
## 2026-08-21: 非LBS metadataでGPUスキニングを無効化

- **関連:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **気づき:** GPU shaderはLBS専用なのに、DQ/Blended DQ入力でも128本以下ならGPU経路を選択し、入力方式と実装方式を混同する可能性があった。
- **対応:** GPU選択条件をLinearBlend/Rigidに限定し、DQ系metadataはCPU fallbackへ送る。CPU側は現状LBS評価のため、DQ本体対応は未完了のまま明示される。
- **次に確認:** DQ入力がGPUではなくCPU経路として表示され、LBS入力だけがGPUへ入ることをruntime確認する。
## 2026-08-21: 混在deformerのskinning method優先順位を固定

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **気づき:** 1 meshに複数skin deformerがある場合、検出順によってDual QuaternionとBlended DQのmetadataが後勝ちし、方式表示・GPU fallback判定が不安定になる可能性があった。
- **対応:** Blended DQを最優先、次にDQ、Rigid、LBSの順でmetadataを保持する。
- **次に確認:** 複数deformerを含むFBX/glTFで、入力方式表示とGPU/CPU選択が順序に依存しないことをruntime確認する。
## 2026-08-21: 非LBS警告をimport単位で抑制

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **気づき:** 複数meshを持つモデルでは、同じ非LBS方式の警告がmeshごとに繰り返され、診断ログが埋もれる。
- **対応:** 1回のimport処理につき非LBS警告を1回だけ出すようにした。方式metadataの記録とLBS fallbackは維持する。
- **次に確認:** 複数meshのDQ入力で警告が過剰出力されず、方式表示は維持されることをruntime確認する。
## 2026-08-21: CPU Dual Quaternion skinningを追加

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm` / `Mesh::applySkinning()`
- **対応:** `SkinningMethod::DualQuaternion`では、bone行列をreal/dual quaternionへ変換し、符号整合した4 influenceを正規化ブレンドしてpositionとrotation-only normalを評価する。GPUは引き続きCPU fallback。
- **制限:** Blended DQは頂点ごとの`dq_weight`をMeshへ保持していないため、現状LBS fallback。scale/shearを含む行列のDQ品質と実ファイルruntimeは未検証。
- **次に確認:** DQモデルのCPU表示がLBS警告だけの状態から改善し、GPU選択されないことをruntime確認する。
## 2026-08-21: Blended Dual Quaternionの頂点ブレンド率を保持

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm` / `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** ufbxの`ufbx_skin_vertex::dq_weight`を`skinDQWeight`属性へコピーし、CPU側でLBS結果とDQ結果を頂点単位で補間する。DQ=0はLBS、DQ=1はDual Quaternionとなる。
- **制限:** 複数deformerの値は最大値を採用。scale/shearを含む行列と実ファイルruntimeは未検証。
- **次に確認:** runtimeで混合率が期待どおり変化することを確認する。
## 2026-08-21: 実装対象外PMXのファイルフィルタ残骸を除去

- **関連:** `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`
- **気づき:** PMX importerを撤去した後もレイヤー追加ダイアログの3DフィルタだけがPMXを提示していた。
- **対応:** 実際に対応しているPMD、FBX、glTF等と一致するようPMXをフィルタから除去した。
- **次に確認:** 他の3Dファイル選択導線にPMX表記が残っていないことを確認する。
## 2026-08-21: flatten後のメッシュ方式を頂点属性で保持

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm` / `ArtifactCore/src/Mesh/Mesh.cppm`
- **気づき:** ufbxの複数メッシュを一つのMeshへflattenする構造では、ファイル全体のskinning methodだけで評価すると、DQメッシュの方式が別メッシュへ波及し得る。
- **対応:** `skinMethod`属性へ各ソースメッシュの方式を保存し、CPU deformerは頂点属性を優先してLBS/Rigid/DQ/Blended DQを選択する。
- **制限:** 一つのソースメッシュに複数skin deformerがある場合は最も非線形な方式を採用。runtime未検証。
## 2026-08-21: Blended DQの変換失敗時にLBSへ復帰

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **気づき:** DQ行列を生成できない頂点でBlended DQが元頂点へ戻ると、LBS成分まで失われる。
- **対応:** Blended DQでは有効なLBS積分があればそれをフォールバック出力にする。純粋なDQ方式は従来どおり元頂点へ戻す。
## 2026-08-21: Dual Quaternion入力行列の有限値検証

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** DQ変換前にボーン行列16要素を検査し、NaN/Infを含む行列をQuaternion化しない。
- **次に確認:** runtimeで破損行列が出ても変形結果が有限値を維持することを確認する。
## 2026-08-21: SkinningMethod APIの未知値をLBSへ正規化

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** `setSkinningMethod()` の未知enum値をLinearBlendへ戻し、未定義の評価経路を防止する。
## 2026-08-21: DQ実装状況のInsight記述を現行状態へ補正

- **関連:** 上記のskinning metadata / CPU DQ / Blended DQ記録
- **補正:** 過去の中間記録にある「DQはLBS評価」「Blended DQは未対応」は実装途中時点の記述。現在はCPU DQ、頂点`dq_weight`によるBlended DQ、メッシュ方式属性、LBSフォールバックまで実装済み。
- **未検証:** 実ファイルruntimeとビルド確認は、ユーザー指定どおり未実施。
## 2026-08-21: SkinningMethod変更をMesh revisionへ反映

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** `setSkinningMethod()` が実際に方式を変更したときだけMesh revisionを進め、描画側の更新監視へ変更を伝播する。
## 2026-08-21: 8 influence超過の切り捨てをimport警告

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **対応:** CPU用の追加4 influence属性を追加し、最大8本まで保持する。8本を超える頂点がある場合はimportごとに一度だけ警告する。
- **制限:** GPU shaderは4本入力のため、追加influenceがあるMeshはCPU経路へ送る。超過分は最弱影響から切り捨てる。
## 2026-08-21: ufbxの追加skin influenceをCPUで最大8本保持

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm`, `ArtifactCore/src/Mesh/Mesh.cppm`, `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **対応:** `boneIndicesExtra` / `boneWeightsExtra` を追加し、LBS・DQ・Blended DQのCPU評価を最大8 influenceへ拡張。追加属性を持つMeshは4本入力のGPU shaderを使わずCPUへ送る。
- **制限:** 8本を超える影響は最弱から切り捨てる。runtime未検証。
## 2026-08-21: DCCギャップ分析を8 influence/DQ実装へ同期

- **関連:** `docs/analysis/REPORT_DCC_GAP_3D_TEXT_2026-08-18.md`
- **対応:** 旧来の「最大4 influence・CPU LBSのみ」という記述を、最大8 influence、CPU LBS/Rigid/DQ系、GPU4本＋CPU fallbackの現行実装へ更新した。
- **未検証:** 実ファイルruntime受入れとビルド確認は未実施。
## 2026-08-21: 3D Model milestoneを現行skinning実装へ同期

- **関連:** `docs/planned/MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md`
- **対応:** 旧来の最大4 influence / CPU LBS記述を、最大8 influence、方式別CPU評価、LinearBlend限定GPU、その他CPU fallbackの現行状態へ更新した。
## 2026-08-21: Viewer statusへskin influence幅を表示

- **関連:** `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- **対応:** 既存statusに4本/8本以上のinfluence幅を追加し、追加influence時のCPU fallbackを画面上で判別できるようにした。
- **補正:** ボーンを持たない静的Meshでは influence幅を `-` と表示し、未スキニング状態を明示する。
## 2026-08-21: skinMethod属性の非整数値を拒否

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** 頂点方式属性は有限・整数・0〜3の値だけをenumへ変換し、それ以外はMesh既定方式へフォールバックする。
## 2026-08-21: Rigid skinningを最大weightの単一bone評価へ分離

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** `SkinningMethod::Rigid`では複数influenceが入力されても最大weightのboneだけを選び、通常LBSと異なる方式の意味をCPU評価へ反映する。
## 2026-08-21: Rigid方式のGPU LBS誤差を回避

- **関連:** `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`
- **対応:** RigidはCPUで単一最大weight評価を行うため、GPUのLBS shaderとは意味が異なる。GPU互換方式をLinearBlendだけに限定し、RigidはCPU fallbackへ送る。
## 2026-08-21: scale/shear行列をDQ変換から除外

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** DQ化前に行列の有限性、行列式≈1、各回転軸の単位長・相互直交性を確認する。scale/shear/反転を含む行列は失敗扱いとし、Blended DQではLBSへ戻す。
- **未検証:** 実モデルのアニメーション行列が常にこの許容範囲に収まるかはruntime未確認。
## 2026-08-21: Blended DQで無効DQ影響のLBS寄与を維持

- **関連:** `ArtifactCore/src/Mesh/Mesh.cppm`
- **対応:** 各影響のLBS変換をDQ変換より先に評価し、scale/shear等でDQだけが失敗してもBlended DQのLBS側合成から影響を落とさないようにした。
## 2026-08-21: ufbx疎形式のBlended DQ重みを取り込み

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm`
- **対応:** 頂点ごとの`dq_weight`に加えて、ufbxの`dq_vertices/dq_weights`疎配列表現も`skinDQWeight`へ統合する。

## 2026-08-21: Preview Stopの非同期APIは既存だったが呼び出し側が同期経路を使用

- **関連:** `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`, `ArtifactCore/src/Audio/AudioRenderer.cppm`, `ArtifactCore/src/Audio/WASAPIBackend.cppm`
- **事実:** `AudioRenderer::requestStop()` と `WASAPIBackend::requestStop()` は既に存在し、backend threadをjoinせず停止要求だけを出す契約になっていた。一方、再生エンジンの通常Stopは同期`audioRenderer_->stop()`を呼んでいた。
- **対応:** 再生エンジンの通常Stopを`requestStop()`へ切り替え、joinは次回startまたはclose側に残した。
- **未検証:** WASAPI実機でStop応答時間、Stop→Play競合、close時のjoin完了はruntime未確認。

## 2026-08-21: File Menu recent project pathの重複を正規化

- **関連:** `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`
- **事実:** recent projectは保存時と表示時で絶対化・clean化されておらず、相対パスや表記差が重複項目になる可能性があった。
- **対応:** 追加・pruneの両方でabsolute + clean pathへ正規化し、存在確認・重複排除・保存値を同じ契約へ揃えた。
- **未検証:** 設定に残る旧相対パスを実環境で再構築した場合の表示と再オープンはruntime未確認。

## 2026-08-21: Async project load/saveのパス契約を同期経路と統一

- **関連:** `Artifact/src/Project/ArtifactProjectManager.cppm`
- **事実:** `loadFromFileAsync` と `saveToFileAsync` は入力をtrimするだけで、完了後のcurrent pathやexporterへ相対パスが渡り得た。
- **対応:** 両経路の入口でabsolute + clean pathへ正規化し、存在確認、保存先、project root、hook通知の基準を統一した。
- **未検証:** 相対パスを指定した async save/load の実動作と、project移動後の全source relinkはruntime未確認。

## 2026-08-21: Async save完了通知をcurrent path更新後へ移動

- **関連:** `Artifact/src/Project/ArtifactProjectManager.cppm`
- **事実:** 保存成功時の`onFinished`が、queuedな`currentProjectPath_`更新より先に呼ばれていた。
- **対応:** 成功コールバックを状態更新・dirty解除・after-save hookの後へ移動し、保存直後のFile Menuが新しいproject pathを観測できる順序にした。
- **未検証:** 保存直後の連続Save/Save As操作、UI thread上のcallback順序はruntime未確認。

## 2026-08-24: text-animator-2026-08-14 ブランチの codex/2026-08-24-dev へのマージ

- 関連: `ArtifactCore` / `Artifact` / `docs/planned/MILESTONE_TEXT_GLYPH_SUBMITTER_2026-08-14.md`
- 事実: 2026-08-14〜15 の Text Glyph Submitter / shapedGlyphIndices / stringMappingValid 実装は `origin/codex/text-animator-2026-08-14` にのみ存在し、現行 dev ブランチには未取り込みだった。両子リポジトリへ merge し、conflict 17 ファイル（Core 8 + Artifact 9）は HEAD 側（NamedVector 移植・新 Parallel::For 構造・GlyphKey 拡張済み呼び出し側）を優先して解消した。
- 未検証: feature 側が追加した `ImageMorphEffect`（text 無関係）は `cmake/ArtifactRenderModuleReferences.cake` マニフェストに登録されていない。GLOB 対象か split target 対象かでビルド影響が変わるため、ビルド時に要確認。
- 次に確認: ビルド＋GPU smoke（Regional Indicator 🇯🇵、家族 emoji run bounds、複数 ZWJ）をこのブランチで再実行し、マイルストーン残ケースの実装を再開する。
## 2026-08-24: ImageMorphEffect のマニフェスト未登録を静的確認

- 関連: `Artifact/include/Effects/Distort/ImageMorphEffect.ixx`, `Artifact/src/Effects/ImageMorph/ImageMorphEffect.cppm`, `Artifact/cmake/ArtifactSources.cmake`, `cmake/ArtifactRenderModuleReferences.cake`（実体は `Artifact/cmake/ArtifactRenderModuleReferences.cmake`）
- 確認事実（未検証からの更新）: マージで取り込まれた `ImageMorphEffect` の .ixx / .cppm は `ArtifactSources.cmake` の明示リストにも、Distort pack の pin（`TimeDisplacement` のみ）にも登録されていない。`ArtifactRenderModuleReferences.cmake` は Render 実装ユニット専用の BMI 参照リストであり、そもそも effects の登録先ではない。ソース収集は GLOB ではなく明示リストのため、未登録でもビルドへの悪影響はない（＝現状デッドファイル）。
## 2026-08-24: Text Animator 構造の設計評価（AE 比較）

- 関連: `ArtifactCore/include/Text/TextAnimator.ixx`, `ArtifactCore/src/Text/TextAnimator.cppm`, `Artifact/src/Layer/ArtifactTextLayer.cppm`
- **対応状況 (2026-08-24):** (1) のタプル問題は第一段階として解消。`SelectorCombineMode` + `AnimatorSelectorSet` を追加し、純粋評価 `evaluateAnimatorWeights()` / `applyAnimatorSets()` を新設、legacy tuple 版は adapter として委譲、`ArtifactTextLayer` 側も名前付き構造へ移行済み。(2) の in-place 適用は現状維持（weight 合成のみ pure 化）。(3) の index property path は未対応。(4) の時間二重構造は未対応。

- 事実: セレクター（Range/Wiggly/Expression）× AnimatorProperties の分離、per-glyph weight 評価、units/shape/order/grouping の網羅は AE モデルと整合しており自然。一方 (1) animator=固定タプル（Range+Wiggly+Expression+Props 各1個）で AE の「1アニメーターにセレクターを複数スタック（add/intersect/subtract/min/max）」が表現できない、(2) 複数 applyAnimatorStack オーバーロードの累積（「後方互換100%」コメント）と glyphs_ への in-place 変形適用で per-animator 寄与の分離が難しい、(3) ランタイム評価が `text.animators.N.*` の index ベース property path 参照で、animator の並べ替え・削除時に path identity がずれる、(4) 時間表現が RationalTime（プロパティ評価）と float seconds（applyAnimatorStack）の二重構造。
- 未検証: (3) の並べ替え UI が存在するかは未確認（現状追加・削除のみなら実害は限定的）。
- 価値: v1 としては自然だが、`MILESTONE_TEXT_ANIMATOR_SEMANTIC_PIPELINE_2026-07-04.md` の pure evaluator 移行でタプル→セレクターリスト化するのが次の自然な構造改善。

- 次に確認: ImageMorphEffect を有効化するか、feature 側から意図的に除外したまま残すかを判断する。有効化する場合は `Artifact/CMakeLists.txt` の Distort pack foreach と `ArtifactSources.cmake` への追加が必要。text 無関係のため本マイルストーンの範囲外。


## 2026-08-24: Layer Source コンポーネント化 Phase A/B の構造決定

- 関連: `Artifact/include/Layer/ArtifactAbstractLayer.ixx`, `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `Artifact/src/Layer/ArtifactSolidImageLayer.cppm`
- 事実: `LayerComponentHost` を保持する `ArtifactAbstractLayer::Impl` は outer クラスへの back-pointer を持たないため、`Impl::syncBuiltinComponentDescriptors()` 内から outer の仮想関数（レイヤー種別の取得など）は呼べない。このため Source 相 descriptor の種別登録は「仮想関数」ではなく「protected setter (`setBuiltinLayerSourceComponentType`) + Impl メンバー (`builtinSourceComponentType_`)」方式とした。派生クラスの ctor 本体で setter を呼ぶと、base ctor で走る初回 sync の後に遅延 resync 経路で即時反映される。
- 事実: `resolveLayerSourceOverride()`（protected virtual, 既定 nullptr）を draw()/toQImage() の冒頭シームとして配線済み。現状オーバーライドは無いので挙動変化ゼロ。Solid 平面レイヤーのみ `builtin.source-solid-fill` / `artifact.component.source-solid-fill` descriptor が登録され、Source 相が初めて非ゼロになった。
- 価値: `MILESTONE_SOLID_LAYER_NOISE_FILL_2026-08-18.md`（M-SOLID-NOISE）は Fill 拡張ではなく `resolveLayerSourceOverride()` の実装として載せると、将来の Image ソース component 化（Phase D）や sequence-player との責務整理と矛盾しない。
- 未検証: ビルド・ランタイム未確認（指示時までビルド禁止ルール）。Components 専用面への source descriptor 表示は汎用列挙なら自動で出るはずだが、UI 側のフィルタ有無は未確認。
- 次に確認すべきこと: Phase C で SolidImageLayer::Impl の塗り生成を buffer 化し override を返す実装。その際 GPU sprite path は QImage 境界での明示変換になるためキャッシュ設計が必要。

## 2026-08-24: Solid Source Override 実装（Phase C）の設計判断

- 関連: `Artifact/src/Layer/ArtifactSolidImageLayer.cppm`, `Artifact/include/Layer/ArtifactSolidImageLayer.ixx`
- 事実: `toQImage()` を override 経由にすると `resolveLayerSourceOverride()` → `currentFillImage()` の呼び出しで循環・float→QImage 往復変換が発生するため、`currentFillImage()`（QImage キャッシュの正規生成元、generation カウンタ付き）を新設し、`toQImage()` はその委譲のみにした。override は「テクスチャ系フィル（現状グラデ）のとき `ImageF32x4_RGBA` バッファを返す」契約。単色は従来どおり `drawSolidRectTransformed` 直描の fast path を維持。
- 事実: draw() の override 分岐ではバッファからではなく `currentFillImage()` を直接使うことで往復変換を回避。副作用としてグラデのクローン描画が「インスタンス毎の QGradient 再生成」から「キャッシュ済み QImage スプライト + opacity 引数」に変わる。
- 変換は `ArtifactImageLayer.cppm` の `toFrameBuffer()` と同一経路（qImageToCvMat → CV_32FC4 → legacyOpenCvBgra32Float/Premultiplied）をローカル関数 `solidFillToFrameBuffer()` として複製。
- 懸念: グラデ平面レイヤーは 1080p で約33MBの float バッファを追加保持する。ノイズフィル（M-SOLID-NOISE）がこのバッファを消費する前提のため許容したが、8bit backing（setFromRGBA8 への寄せ）で削減可能か要検討。
- 未検証: スプライト経路での opacity 適用点の変化（旧: QColor アルファ焼き込み / 新: renderer の alpha 引数）による見た目差、ビルド・ランタイム全体が未確認。

## 2026-08-24: Image Source Override 実装（Phase D）で draw 再配線が不要だった件

- 関連: `Artifact/src/Layer/ArtifactImageLayer.cppm`, `Artifact/include/Layer/ArtifactImageLayer.ixx`
- 事実: `ArtifactImageLayer::draw()` は既に `hasCurrentFrameBuffer()` → `currentFrameBuffer()`（`SharedPtr<ImageF32x4_RGBA>` の `cacheBuffer_`）→ `drawSpriteTransformed` バッファオーバーロードの順で、crop は UV リフレーム、シーケンスは refresh 後にバッファ描画と、内部構造が既に「ソースコンポーネント形状」だった。Phase B/C のようなシーム再配線は不要と判断し、descriptor 登録（`builtin.source-image`）と `resolveLayerSourceOverride()` の公開のみ実装。
- 事実: override は `&currentFrameBuffer()` のアドレスを返す。`cacheBuffer_` は AssetManager 公開時に差し替わり得るが、既存 draw() の `const auto& buffer = currentFrameBuffer()` と同一の生存期間パターン（同スタック内で即時使用）なので等価。シーケンス時は currentFrameBuffer→toQImage 経由でフレーム refresh が走るため時間依存コンテンツになる。
- 次に確認すべきこと: (1) descriptor settings のプロパティミラー（solid/image とも現状 kind 情報なしの空 settings）、(2) Components 専用面での source descriptor 表示・toggle セマンティクス、(3) sequence-player（Drive 相）との責務整理、(4) ビルド＋平面グラデ・画像・連番の目視回帰。

## 2026-08-24: [Superseded] M-SOLID-NOISE Phase 1（CPU 経由のノイズフィル）を Source Override 上に実装

- 関連: `Artifact/src/Layer/ArtifactSolidImageLayer.cppm`, `Artifact/include/Layer/ArtifactSolidImageLayer.ixx`, `Artifact/include/Layer/ArtifactLayerInitParams.ixx`
- 実装: `ArtifactSolidFillType::Noise = 6` 追加。`ProceduralTextureSettings`（Core 流用、新規構造体なし）を Impl に保持し、`resolveLayerSourceOverride()` の Noise 分岐で `ProceduralTextureGenerator::generate(settings, buffer)` を直接 `ImageF32x4_RGBA` へ生成（QImage 経由ゼロ）。キャッシュ無効化は全パラメータ+カラーマップの署名文字列比較。カラーマッピングはグレースケール→colorA/B lerp の CPU ループ。draw() はバッファオーバーロードの `drawSpriteTransformed` で描画。`currentFillImage()` は Noise 時バッファ→QImage の明示変換のみ（thumbnail/export 境界）。JSON は `solidNoise` オブジェクト（kind は安定文字列、未知 kind は Perlin フォールバック）。プロパティは既存 solidGroup 内に `solid.noise.*` として露出。
- 設計メモ: フィル種別切替時のバッファ混汚染対策として、Noise 経路は `sourceBufferGeneration_ = -1` でグラデ側世代同期を無効化し、グラデ経路は `noiseBufferSignature_.clear()` で逆方向も強制再生成する双方向ガードを入れた。
- 未検証: ビルド・描画未確認。GPU compute パス（正規経路）、Create/Edit ダイアログ UI、プリセット、キーフレーム対応（offset/rotation 駆動アニメーション）は本マイルストーンの残フェーズ。

## 2026-08-24: ノイズ実装を平面 Fill から独立レイヤーへ方針変更＋ファイル破損インシデント

- 関連: `Artifact/src/Layer/ArtifactSolidImageLayer.cppm`, `docs/planned/MILESTONE_SOLID_LAYER_NOISE_FILL_2026-08-18.md`
- 方針変更: M-SOLID-NOISE（平面 Fill 拡張）は Superseded。ユーザー決定により独立 `ArtifactNoiseLayer`（新規 LayerType::Noise）として実装する。平面側の solid.noise.* 実装一式（enum 値・アクセサ・シリアライズ・プロパティ・生成分岐）は revert 済み。Phase B/C のソース override アーキテクチャ（descriptor 登録、currentFillImage 分離、resolveLayerSourceOverride のグラデ経路）は維持。
- 技術資産: `ProceduralTextureGenerator::generate(settings, ImageF32x4_RGBA&)` 直生成 + パラメータ署名キャッシュ + カラーマップ lerp の実装パターンは検証済み。新レイヤーで再利用する。kind 文字列契約（perlin/simplex/fbm/voronoiDistance/voronoiCell/voronoiEdge/white/value/gradientLinear/gradientRadial、未知は Perlin フォールバック）も流用。
- インシデント: bash での部分文字列削除操作が ArtifactSolidImageLayer.cppm を二重化破損（関数ブロック全体の重複）。`git show HEAD:` からの復元は PowerShell の `>` リダイレクトが UTF-16LE を書くため二重に失敗し、`cmd /c` 経由の生バイト redirect で復旧。
- 教訓: (1) 大きなブロック削除は edit ツールか git ベースで行い、bash の IndexOf/Remove 文字列手術を使わない。(2) PowerShell 5.1 の `>` リダイレクトは UTF-16LE。git オブジェクトをファイルへ落とすときは cmd /c 経由か [IO.File] API。(3) 親リポジトリの git diff は submodule 内ファイルの変更を表示しない。子リポジトリ側で status/diff を見る。

## 2026-08-24: ArtifactNoiseLayer 独立レイヤー実装

- 関連: `Artifact/include/Layer/ArtifactNoiseLayer.ixx`, `Artifact/src/Layer/ArtifactNoiseLayer.cppm`, `Artifact/src/Layer/ArtifactLayerFactory.cppm`, `docs/planned/MILESTONE_NOISE_LAYER_2026-08-24.md`
- 実装: `LayerType::Noise = 30`。生成は `resolveLayerSourceOverride()` 内で `ProceduralTextureGenerator::generate(settings, buffer)` 直生成（QImage ゼロ、パラメータ署名キャッシュ、colorA/B カラーマッピング）。draw() は常にテクスチャ経路（単色 fast path 不要）。descriptor は `builtin.source-noise` として Source 相に登録。シリアライズは `type=30` + `noise` オブジェクト（kind 安定文字列、未知は Perlin フォールバック）。プロパティはレイヤー固有グループ `Noise`。
- 事実: ファクトリの fromJson は数値 type を正規 discriminator とするため新 LayerType の roundtrip は switch case 追加のみで動く。legacy 文字列分岐は新タイプには不要だった。Artifact 側 CMake は「.ixx 対ありの純実装 .cppm」はそのまま APP_IMPL 残り、force list 不要だった。
- 当時の未検証: この記録時点ではビルド・描画・保存/再読込・タイムライン表示を確認していなかった。作成 UI・automation 露出・プリセット・キーフレーム駆動は M-NOISE-LAYER の残フェーズで、GPU compute は後続記録で初期接続済みだが runtime parity 等は未検証。

## 2026-08-24: Noise レイヤー GPU 接続点の確認（GPU 初期接続前）

- 関連: `Artifact/src/Layer/ArtifactNoiseLayer.cppm`, `ArtifactCore/include/ImageProcessing/ProceduralTexture.ixx`, `ArtifactCore/src/ImageProcessing/ProceduralTexture.cppm`
- 事実（GPU 初期接続前）: `ProceduralTextureComputePipeline` は ArtifactCore に公開され、`initialize()` / `generate(IDeviceContext*, ITextureView*, settings)` / `createOutputTexture()` を持っていた。一方、この時点の Noise レイヤーの `resolveLayerSourceOverride()` は `ProceduralTextureGenerator::generate(settings, ImageF32x4_RGBA&)` の CPU 経路のみを呼び、GPU context・UAV・テクスチャ寿命を保持していなかった。後続の「Noise draw の GPU compute 初期接続」で、color mapping 無効時の GPU 経路と CPU fallback が追加された。
- 当時の判断: GPU 接続は単純な関数置換ではなく、renderer の GPU context 所有境界、出力テクスチャのキャッシュキー、UAV→sprite resource の同期、CPU fallback をまとめて設計する必要がある。Diligent の低レベル実装を推測で変更せず、独立した設計・runtime確認フェーズに分離した。後続実装では既存 renderer 境界を使う初期接続まで進めた。
- 未検証: 実 GPU backend での compute pipeline 初期化、Noise レイヤーとの texture cache 共有可否、color mapping の GPU 実装コスト。
- 当時の次に確認: 既存 renderer の compute effect / texture cache が持つ context と同期契約を調査し、Noise 専用 pipeline を増やす前に再利用可能なサービス境界を特定する。後続実装では Noise レイヤーの draw 内で既存 `ProceduralTextureComputePipeline` を直接利用する初期接続を採用したため、GPUTextureCacheManager 統合と runtime 検証が現在の残課題になっている。

## 2026-08-24: 既存 compute effect との比較

- 関連: `Artifact/src/Effects/WhiteBalanceEffect.cppm`, `Artifact/src/Effects/Wave/WaveEffect.cppm`, `Artifact/src/Effects/Rasterizer/HexGridEffect.cppm`, `Artifact/include/Render/GPUTextureCacheManager.ixx`
- 事実: 既存の GPU compute effect は effect 実行時に `GpuContext` / `ComputeExecutor` を構築または保持し、出力 texture を effect 内で所有して SRV/UAV を直接 bind している。Noise レイヤーの source override は renderer の GPU texture view を返す契約ではなく、CPU float buffer を返す契約である。
- 事実: `GPUTextureCacheManager` は owner ID と cache handle を持つ renderer 側の別責務で、Noise レイヤーから直接利用できる接続は現状確認できない。
- 当時の判断: 最初の GPU 化候補は `resolveLayerSourceOverride()` の置換ではなく、renderer が Noise source の compute output view を取得・保持し、CPU buffer を fallback として残す source texture provider 境界の追加である。既存 effect の局所 executor パターンをそのまま Layer に複製するのは texture cache／device reset 管理と衝突しやすい。実装ではまず Noise の draw 内に限定した初期接続を採用し、source provider／texture cache 統合は未実施のまま残している。
- 未検証: `GPUTextureCacheManager` を Noise source provider の backing に使えるか、renderer の device reset 時に owner invalidation を正しく通知できるか。

## 2026-08-24: Renderer 側に既存の GPU 出力資産を確認

- 関連: `Artifact/include/Render/ArtifactIRenderer.ixx`, `Artifact/src/Render/ArtifactIRenderer.cppm`, `Artifact/include/Render/GPUTextureCacheManager.ixx`
- 事実: `ArtifactIRenderer` は `device()`、`immediateContext()`、`createOffscreenComputeTexture(width, height)` を既に公開しているため、GPU 出力 texture の生成と compute dispatch に必要な renderer 所有資産は既にある。
- 訂正: 直前の記録で `IDeviceContext*` の公開 accessor が未確認としたのは誤り。`immediateContext()` が存在するため、Noise GPU provider は新しい accessor を増やさず既存 renderer 境界を利用できる。
- 判断: 残る設計課題は context の取得ではなく、Noise レイヤーの render-frame 内での compute 実行タイミング、出力 texture のキャッシュキー／寿命、GPUTextureCacheManager への登録、device reset 時の無効化である。

## 2026-08-24: Noise draw の GPU compute 初期接続

- 関連: `Artifact/src/Layer/ArtifactNoiseLayer.cppm`, `ArtifactCore/include/ImageProcessing/ProceduralTexture.ixx`
- 実装: Noise の `draw()` で color mapping 無効時に既存 `ArtifactIRenderer::device()` / `immediateContext()` と `ProceduralTextureComputePipeline` を使い、RGBA16F UAV を生成して compute dispatch 後に texture-view sprite として描画する経路を追加。compute 失敗時、device/context 不在時、color mapping 有効時は既存 CPU float-buffer 経路へ fallback する。
- キャッシュ: Noise signature と device identity を基準に GPU texture を再生成し、device identity 変更時は pipeline/context/texture を破棄する。GPUTextureCacheManager への登録はまだ行っていない。
- 未検証: ビルド、D3D12/Vulkan runtime parity、UAV→SRV transition の実機確認、cloner/fracture overlay との組み合わせ、GPU texture memory budget。


## 2026-08-25: 3D シャドウマップ経路は単一ライト実装済み、softness 未接続

- 関連: `Artifact/src/Render/ArtifactIRenderer.cppm`（beginShadowMapFrame / renderShadowMapFrame）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（L31629 付近のライト選択）、`ArtifactCore/src/Graphics/MeshRenderer.cppm`（prepareShadow / drawShadow / setShadowMap）
- 事実: シャドウマッピングは既にエンドツーエンドで接続済み。Controller が最初の castsShadows 有効な Directional／Spot を選び、D32 シャドウマップ深度プレパス → MeshRenderer の PCF 比較まで動く。
- 事実: `ArtifactIRenderer.cppm:1066` の `setShadowMap()` 呼び出しは depthBias 既定値のみで、`shadowRadius` → softness 引数を渡していない。ライトレイヤーの Shadow Radius は UI 上存在するが影の柔らかさに反映されない可能性が高い（未検証: 実機描画）。
- 未検証・残課題: Point/Area ライトは影なし（コードコメントで意図的に除外）、シャドウライトは 1 灯のみ、Directional の ortho extent は原点中心固定 2048（シーン境界未適合）、実機受入れ未確認。

## 2026-08-25 — カメラレイヤー整備（POI / DoF / カメラMB）実装時の気づき

**関連**: `Artifact/src/Layer/ArtifactCameraLayer.cppm`, `Artifact/include/Render/ArtifactDepthOfFieldPass.ixx`（新規）, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

1. **previewRenderSlot.depthTargetView は SSGI / MotionBlurPass と同じ「非線形 [0,1] window depth」契約**
   - 事実: `offscreenTextureShaderResourceView(previewRenderSlot.depthTargetView)` の深度は `zNear*zFar/(zFar - d*(zFar-zNear))` で視線深度へ逆算するのが既存パス（SSGI）の規約。新規 DOF パスもこれに合わせた。
   - 価値: 今後ポストエフェクトを追加する場合はこの変換を共通ヘルパー化すると二重実装を防げる（未検証の改善案）。

2. **`ArtifactCameraLayer::draw()` のギズモは POI 有効時に `effectiveGlobalTransform()` を見るようにしたが、VP 上の POI ギズモ（ドラッグ移動）は未実装**
   - AE 相当にするには POI の VP ハンドルが必要。次の候補作業。

3. **カメラ MB はタイムライン MB 設定と独立に発火する設計にした**
   - `cameraMotionBlurRequested` が velocity ターゲット確保条件に入ったため、カメラ MB 単独で ON のときもパイプライン初期化コストが増える。プレビュー性能への影響は要計測（未検証）。

4. **DOF パスの CoC モデルは簡易リニア ramp（±focusDistance で最大半径）**
   - 実レンズの f-stop ベースではない。`aperture` を物理単位で使う本格モデルへ拡張する余地あり。既存 `depthOfFieldParameters().maxCoc = aperture × blurScale` の意味論とも要整合確認。

5. **`viewMatrix()` が POI モードで authored rotation を完全無視する**
   - AE では 2ノードカメラでも rotation を微調整できる（POI + rotation オフセット）。現在実装は「POI 優先・rotation 無視」。ユーザー要件次第でオフセット合成へ拡張可能。

## 2026-08-25 — Lens Surface は既存 SurfaceFX の用途別編集面として成立する

- 関連: `docs/planned/MILESTONE_SURFACE_FX_SYSTEM_2026-07-22.md`、`ArtifactCore/include/Graphics/Effect/SurfaceFX.ixx`、`Artifact/include/Effects/SurfaceFX/SurfaceFXEffect.ixx`、`Artifact/src/Effects/SurfaceFX/SurfaceFXEffect.cppm`
- 事実: ScreenSpace anchor、Scratch / Droplet / Streak / Condensation / Dirt、effect stack、Composition JSON、決定的な時間評価、CPU 局所屈折が既に存在する。
- 事実: 現行データ契約には texture asset identity、blend mode、tint、pivot、fade / growth がなく、generic property は先頭 element しか編集できない。
- 判断: Camera Layer 内に別の Virtual Lens システムを作らず、ユーザー向け `Lens Surface` を既存 `SurfaceFXEffect` の専用編集面として実装する。血・泥は enum を用途ごとに増やさず `TextureDecal` と素材プリセットで表現する。
- 価値・懸念: レンダーと保存基盤を再利用しながら傷・血・雨を一つの制作導線へ統合できる。GPU pass、asset relink、専用 element stack、viewport 操作、preview / render queue parity は未実装または未検証。
- 次に確認: `ArtifactEffectTabSurface` の専用 editor 差し込み口、既存 asset identity / relink 契約、effect stack undo command、composition viewport overlay の最小接続点。

### Update 2026-08-26

- `TextureDecal` のJSON v3契約、AssetDatabase asset identity、複数element選択、血／泥プリセット、OpenCV texture decode/cache、CPU blend、復元時のimpl同期を追加した。
- Effects detail panel に `Lens Surface Elements` リストを追加し、既存の `Surface Element Index` property を介して汎用編集行を選択要素へ切り替える導線を作った。
- 同リストに Duplicate / Delete / Up / Down を追加し、`SurfaceFXElementSnapshotCommand` で複数デカールの順序・複製・削除をUndo/Redo可能にした。viewport直接操作は未実装。
- SurfaceFX element は `AssetDatabase::getAssetInfo(assetId)` から描画パスを解決する。既存 `relinkAssetPath()` がIDを維持するため、再配置後も同じデカールを参照できる契約になった。実プロジェクトでのrelink受入れは未検証。
### 2026-08-26 — Lens Surface 専用UIからのデカール追加

- **関連:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`, SurfaceFX/Lens Surface
- **事実:** `Lens Surface Elements` パネルに `Add Decal` を追加し、既存の要素スナップショット Undo コマンドで追加・取り消しを扱うようにした。
- **価値:** 専用UIから要素数プロパティを探さずにデカールを作成でき、血・傷・雨の各要素を同じ編集導線に載せられる。
- **未検証:** 実ビルド・実行時のウィジェット表示と Undo/Redo のランタイム挙動。

## 2026-08-25 — POI VP ハンドル実装時の気づき

**関連**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `ArtifactCompositionRenderOverlay.cppm`, `ArtifactCompositionRenderWidget.cppm`

1. **POI ドラッグ平面は「POI を通り view カメラに向く平面」で実装**
   - 事実: `createPickingRay` の ray と、view 行列の逆変換で取った forward ベクトルを法線とする平面交差（`intersectPickingRayFixedPlaneAt` 同様の既存パターン）で実装。
   - 価値: この平面では Z 軸方向の POI 移動はできない。AE の 2-node camera のような Z 変更は Inspector の POI X/Y/Z 数値入力で行う設計。

2. **ビルド未検証**: POI overlay/drag/cancel 系はすべて新規追加コードのため diff 上の既存破壊なし。実機描画確認（特に `drawGizmoRing` の normal (0,0,1) が world 基準か view 基準か）が次の確認点。
## 2026-08-25 — FOV ↔ focal length 整合

**関連**: `Artifact/src/Layer/ArtifactCameraLayer.cppm`, `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`, `ArtifactCompositionRenderOverlay.cppm`

- **事実**: レイヤーに `focalLength()` / `setFocalLength()` を追加（35mm equivalent、水平 FOV、36mm センサ: fov = 2*atan(18/fl)）。読み出しは実効 FOV（manual/auto zoom 由来）から導出し、書き込みは manual FOV へ切替。`CreateCameraLayerDialog` の既存変換式と同一。
- **価値**: ダイアログのプリセット単位系（15〜200mm）がそのままレイヤーへ保持され、AE 相当のレンズ指定が通る。カメラ選択パネルも「xx.xmm | FOV yy」表示に統一。
- **未検証・懸念**: 水平 FOV ベースだが `projectionMatrix()` は垂直 FOV を使うため、アスペクト比によっては AE との厳密一致からずれる可能性（AE も同様の水平基準のため実害は小さい見込み、要視覚確認）。

## 2026-08-25 — DOF f-stop 物理モデル拡張と配線再適用

**関連**: `ArtifactDepthOfFieldPass.ixx/.cppm`, `ArtifactCompositionRenderController.cppm`

- **事实**: `DepthOfFieldSettings` に thin-lens パラメータ（`focalLength` mm / `fStop`）を追加。fStop > 0 で物理モデル（CoC = |A*f*(d_f - d)| / |d_f*(d - f)|，aperture を f-stop スケールとして解釈）、<= 0 で従来の線形 ramp にフォールバック。
- **事实(重要)**: 前回セッションで `git checkout --` により resolve パスへの DOF/カメラMB 配線が working tree から失われていた。今回再適用し、パイプライン初期化条件に `cameraMotionBlurRequested` も含めた。この種の再適用前は `git diff --stat` で存在確認が必要。
- **未検証**: HLSL コンパイル、thin-lens CoC の視観確認（aperture 値のスケーリングは要調整の可能性）。

## 2026-08-26 — シェイプレイヤー整備 4 項目（Pen パス作成 / VP 頂点編集 / 演算子アニメーション / パスキーフレーム）

**関連**: `Artifact/src/Layer/ArtifactShapeLayer.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `ArtifactCompositionEditor.cppm`

- **実装**: (1) Pen ツールが Shape レイヤー選択時はマスクではなくカスタムパスを作成（開始点クリック or Enter で確定、Backspace で頂点取消、Escape キャンセル）。(2) メイン VP でカスタムパス頂点＋タンジェントハンドルのヒットテスト・ドラッグ・smooth 反射・Undo（`ShapePathVertexEditCommand`）。(3) 演算子パラメータ（TrimPaths start/end/offset、Repeater copies/offset/rotation/opacity ほか）のキーフレーム評価 — 描画時に clone へ適用し静的値は Inspector 保持。`hasAnimatedShapeOperators()` でキャッシュ回避。(4) パス頂点自体のキーフレーム — `shape.path.keyframes` プロパティに JSON で頂点配列を格納し、`evaluatePathAt(frame)` が線形補間（トポロジ不一致時は snap）。
- **事実:** `ArtifactRenderLayerWidgetv2`（LayerEditorPanel 内）に既存の頂点編集実装があり、それをメイン VP 契約へ移植した。既存 Pen 経路は完全に mask 専用のまま（Shape 分岐は前段で return）。
- **未検証・懸念:** ビルド未実施。タンジェント smooth 反射の長さ保存比、パスキーフレームの UI（キー追加導線は API のみ）、`shape.path.keyframes` の timeline 表示統合は次段階。

## 2026-08-26 — ラインレイヤー初期導線（作成メニュー / 既定値）

**関連**: `Artifact/src/Layer/ArtifactShapeLayer.cppm`, `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`

- **事実:** Line は独立レイヤーではなく `ArtifactShapeLayer::ShapeType::Line` として実装済みだが、Shape 作成サイクルは6種類で Line を含まず、`setShapeType(Line)` も共有Shape既定値（fill on / stroke off / width 0）のままだった。
- **対応:** 作成サイクルを7種類へ拡張して `Line 1` を追加。`setShapeType(Line)` 時に Fill OFF、Stroke ON、Stroke Width 1.0（既存幅が0以下の場合のみ）を設定し、新規Lineが不可視になる初期状態を防止。
- **価値:** 既存のShape/JSON/描画モデルを変えず、Lineを作成直後から可視・利用可能にする最小導線になった。
- **未検証・懸念:** ビルド・ランタイム未実施。Tool Options / Inspector ではLine専用表示になっておらず、Shape編集モードではLineの直接端点編集を意図的に除外している。GPU `PolylineStyle` は StrokeAlign を持たないため、Inside/Outside の表示差は別途設計・検証が必要。

## 2026-08-27 — Construction Item の描画境界

**関連**: `Artifact/include/Layer/ArtifactConstructionLayer.ixx`, `Artifact/src/Layer/ArtifactConstructionLayer.cppm`, `Artifact/include/Render/ArtifactIRenderer.ixx`

- **事実:** Construction Layer は現在グリッド等を `drawSolidRectTransformed` で描画する一方、line/circle/text の transformed 共通 API は Construction Layer から直接利用できる形では揃っていない。
- **判断:** まず item の型・JSON 往復・Layer 所有 API を追加し、描画 API を既存の矩形近似で代用しない。次段で `ArtifactIRenderer` の既存 primitive 契約を確認し、selection/editing と同時に接続する。
- **未検証:** module ビルド、item の runtime 描画、既存プロジェクトとの round-trip 実行確認。

## 2026-08-27 — GroupContainer の移行開始点

**関連**: `Artifact/include/Composition/ArtifactCompositionNodes.ixx`, `Artifact/src/Composition/ArtifactCompositionNodes.cppm`, `Artifact/cmake/ArtifactSources.cmake`

- **事実:** 既存 `ArtifactGroupLayer` は UI、AI、Undo、Composition View、描画経路に広く参照されているため、直ちに独立 Container へ置換するのは高リスク。
- **対応:** Layer 非継承の `CompositionNode`／`ContainerNode`／`GroupContainerNode` を追加し、ID・parentId・kind・子 ID の重複拒否・JSON 往復を先行実装した。現在は`ArtifactAbstractComposition`のNodeStore同期と`ArtifactGroupLayer`の互換アダプタまで接続済みである。
- **未検証:** module ビルド、循環親子関係を含むComposition全体のruntime検証、独立Containerとしてのrender boundary／Preview／Export parity。

## 2026-08-27 — GPU track matte の4枚以上は既存逐次GPU経路で処理可能

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`ArtifactCore::LayerBlendPipeline`
- **事実:** 3枚以下は1回の `applyTrackMatte()` にまとめる一方、既存コードには任意枚数を1枚ずつ同じDiligent GPU APIへ適用するフォールバックがある。3枚超だけ早期returnしていたため、その経路へ到達せず無加工レイヤーが返っていた。
- **対応:** 早期returnを除去し、3枚超は既存の逐次GPU適用経路で処理するようにした。D3D12/Vulkan固有コードやDiligentEngineには変更しない。
- **価値・懸念:** CPU側と同じ複数source matte契約へ近づける。各referenceの順序・blend・opacityは逐次適用されるため、GPU/CPUの実画素受入と高枚数時のフレーム時間測定が必要。
- **次に確認:** 4枚以上かつ Add / Intersect / Subtract / Difference を混在させた静止画で、Preview・Software・Render Queue の同一フレーム比較を実行する。

## 2026-08-29 — ArtifactScript Composition API の最小境界

**関連:** `ArtifactCore/include/Script/ArtifactScript/ArtifactScript.ixx`, `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm`

- **事実:** 既存の `ArtifactScriptHost::registerFunction` は評価器から名前解決される汎用 callback registry であり、Composition API 専用の型境界は存在しなかった。
- **対応:** `ArtifactScriptCompositionApi` の5 callback（layer取得・件数・時刻・property取得・設定）を追加し、`installCompositionApi()` から標準関数として登録した。Composition／UI の型には依存しない。
- **価値:** 実際の Composition 実装を直接 ArtifactCore へ持ち込まず、アプリ側が callback を注入できる。`setProperty` の拒否理由は既存の host error 経路へ流せる。
- **未検証:** module ビルド、テスト実行、Artifact 側 Composition API adapter の接続。
## 2026-08-29 — Bitwig 着想の Property Modulation は既存 Router を正規化する

**関連:** `ArtifactCore/include/Audio/Modulation/Router.ixx`, `docs/planned/MILESTONE_PROPERTY_MODULATION_2026-08-29.md`

- **事実:** `Audio.Modulation.Router` はすでに `IModulatorSource`、source ID、target ID、depth、block 単位処理を持っていた。LFO/ADSR/Random と source→target 加算は新規設計を必要としない。
- **対応:** property path を保持する assignment、Add/Multiply 合成、同一 mapping の更新、読み取り用 mapping 一覧を既存 Router に追加した。
- **価値:** property の基準値を直接書き換えず、keyframe/envelope/expression の後に非破壊で合成できる。将来の UI・JSON・Undo は同じ mapping 契約を所有すればよい。
- **追加対応:** `AbstractProperty::evaluateValue()` は明示的な router と stable target path を受け、keyframe/envelope/expression の評価後に Float/Integer へ modulation を適用する。既存 min/max 実値範囲は適用後も守る。
- **未検証:** layer/effect が router 所有者と target path を渡す接続、mapping の JSON/Undo、Audio Follower の block-to-frame 時間変換、runtime の preview/export parity。

### 2026-08-29 — Timeline modulation は stateful source の再現可能な clock が前提

- **関連:** `ArtifactCore/include/Audio/Modulation/Router.ixx`, `ArtifactCore/include/Audio/Modulation/Modulator.ixx`
- **事実:** audio block 用の `process(numFrames)` を effect の `setContext()` ごとに呼ぶと、同一 frame の再描画で LFO/Random が進み、preview と render の値が一致しない。
- **対応:** `processAtFrame(frame, frameRate)` を追加し、同一 frame は idempotent、巻き戻しは reset/replay とした。`RandomSource` は seed を保持して reset 後の系列も再現する。
- **未検証:** 長尺の大きな逆 seek のコスト、ADSR gate event の再生履歴、Audio Follower の音声時刻から control frame への変換。

### 2026-08-29 — Effect modulation target は composition 内の instance ID を使う

- **関連:** `Artifact/src/Effects/ArtifactAbstractEffect.cppm`, `Artifact/src/Composition/ArtifactAbstractComposition.cppm`
- **事実:** effect factory ID は同種 effect 間で重複し得るが、composition へ追加される時点で `uniqueEffectIdForComposition()` が suffix 付きの一意 ID を割り当て、JSONにも保存する。
- **対応:** effect target path を `effect.<instance-id>.<property-name>` とし、`ArtifactAbstractEffect` が per-effect router を所有して `EffectContext::compositionFrame` で進める。
- **未検証:** layer effect 追加経路すべてで instance ID が重複しないこと、effect router/mapping の JSON 保存、preview・offline render の同一 frame parity。
## 2026-08-29 — Property Modulation は row context menu を入口にできる

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` / Property Editor
- **事実:** animatable effect property の既存 row は context menu の auxiliary action を持てる。EffectService には modulation snapshot の Undo 適用 API がある。
- **気づき:** 常時表示の modulation toolbar を増やさず、row の既存補助メニューから source / depth / mix mode を追加することで、Property Editor の責務と縦方向の密度を維持できる。
- **価値・懸念:** 最小導線としては有効。ただし現実装は source の詳細パラメータ編集・既存 assignment の一覧／削除をまだ提供しない。
- **次に確認:** UI runtime で context menu、Undo/Redo、JSON round-trip、複数 effect 選択時の対象表示を確認する。


### 2026-08-29 - Timeline planned マイルストーンの「現状ステータス」棚卸し

- 関連: docs/planned/MILESTONE_TIMELINE_STATUS_INDEX_2026-08-29.md (新規), docs/planned/MILESTONE_TIMELINE_INDEX_2026-04-22.md (既存), docs/DOC_LIFECYCLE.md
- 事実: タイムライン系 planned マイルストーンは 26 件を超え、うち 11 件は冒頭 SUPERSEDED 注記で他文書に吸収済み。残り 15 件で Update 2026-08-15 の判定文を見たところ、4 件が「実装済み相当」、9 件が「部分実装」、2 件が「未着手」だった。SUPERSEDED 文書の吸収先は MILESTONE_KEYFRAME_STATE_SPEC_2026-06-17.md と MILESTONE_CURVE_EDITOR_DCC_IMPROVEMENTS_2026-07-22.md に集約されつつある。
- 気づき: 既存の役割別 index (MILESTONE_TIMELINE_INDEX_2026-04-22.md) は「本筋/補助線」の語彙で整理しているが、最終更新日欄がなく、DOC_LIFECYCLE.md のルールからは外れている。ステータス棚卸し index は「判定基準 → 区分別表 → SUPERSEDED マップ → 着手目安」の順で組むと、未着手テーマを選ぶときの思考順序と一致しやすい。
- 価値・懸念: 「着手テーマを選ぶ前に全体を見渡す」ための俯瞰図として機能する。ただし判定根拠は Update 節の語彙に依存するため、新しい milestone を作る人が Update 節を必ず書くようでないと、判定がすぐに陳腐化する。
- 次に確認: python tools/generate_doc_inventory.py 実行で docs/INDEX_GENERATED.md の再生成と planned/ 残存 Complete 文書の警告を確認し、必要なら planned/ → done/ 移動や SUPERSEDED 文書のアーカイブ可否を別途検討する。


## 2026-08-29 - Timeline が「ハイエンド DCC 感」を欠く理由

- 関連: Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm (10,545 行), ArtifactLayerPanelWidget.cppm (7,827 行), ArtifactTimelineWidget.cppm, TimelineScaleWidget.cppm (161 行), TimelinePlayheadDraw.hpp, MILESTONE_TIMELINE_STATUS_INDEX_2026-08-29.md
- 事実: TrackPainterView 本体で QPainter::Antialiasing 検索結果 0 件、QPainter::TextAntialiasing 0 件。Antialiasing を立てているのは TimelinePlayheadDraw.hpp と LayerPanel の 1 箇所のみ。TimelineScaleWidget は逆に setRenderHint(QPainter::Antialiasing, false) を明示。setMouseTracking は LayerPanel 1 箇所のみ。setStatusBar/zoom level 検索結果 0 件 (status bar に zoom level 表示なし)。frame 単位の ruler は「0f/10f/20f」の整数+「f」サフィックス固定、timecode・秒換算・sub-frame 切替なし。QFont::setFamily("Consolas") がハードコード。
- 気づき: ハイエンド DCC 感は「機能の多寡」ではなく「触っている感を演出する視覚誘導の密度」で決まる。今回観察した現状は、機能の大半は実装されているが、表面仕上げが不足している。特に目立つのは (1) 描画の AA/TextAA が playhead 以外ほぼ無効、(2) status bar に zoom level/frame rate/selection 数が常に表示されない、(3) ruler の単位が frame 整数のみで timecode と秒の切替がない、(4) hover tracking がタイムライン右ペインまで降りていない、(5) 単一 widget に 1 万行近い責務集中 (TrackPainterView) で「機能の総体」は見えても「1 機能 1 widget」の分離感が薄い。AE/Blender/Houdini のルーラーは単位切替、minor/major 動的密度、playhead 同期、KB-driven increment (1/5/10/60 frame) を持ち、AA と TextAA が既定で立った ruler を 1 widget で描いている。Artifact の TimelineScaleWidget は 161 行で「最小機能」は持つが、timecode 表記、sub-frame tick、frame rate 反映、zoom level の常時表示が抜けている。
- 価値・懸念: 「機能はあるが完成していない」を「未着手 planned」と誤認すると、棚卸し (status index) の方が機能過剰に見える。実作業は「機能の有無」ではなく「DCC 感の演出層の不足」を埋める方向に振った方が、体感品質と工数のバランスがよい。巨大単一 widget (10,545 行) の分割は AGENTS.md に従い別判断だが、もし着手するなら「Surface と Interaction を分離」してからの方が進めやすい。
- 次に確認: AE/Blender/Nuke のタイムライン UI スクリーンショットで「ルーラー表記」「status bar 必須項目」「hover tracking 範囲」「AA/TextAA の境界」を実機サンプルとして並べ、Artifact の TimelineScaleWidget と TrackPainterView のギャップをピンポイントで書き出す。可能なら MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-29.md を planned/ に起こし、status index の着手目安に「DCC 感ギャップ解消」を 1 軸として加える。
## 2026-08-29 — Timeline GPU化の前に track-indexed visual cache を共通基盤にできる

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`
- **事実:** 右ペインはtrackの可視範囲を計算している一方、clipとkeyframe markerはpaintごとに全件走査してから可視判定していた。既存のmarker cache再構築点はselection/search用indexをまとめて更新している。
- **気づき:** clip／markerをtrack indexごとに束ねれば、現行QPainter経路と将来のGPU instance packet生成で同じ可視visual抽出を再利用できる。
- **価値・懸念:** 多数layer・keyframe時のpaint CPU負荷を減らし、GPU移行時のsnapshot境界になる。現段階ではUI thread上のcacheであり、worker thread化やGPU resource lifetimeは未実装。
- **次に確認:** `TimelineTrackPaint`で大量layer／keyframe、縦scroll、marker drag、clip trim時の時間と表示回帰を測定する。

## 2026-08-29 — Timeline GPU移行は immutable visual snapshot で旧UIと分離できる

- **関連:** `Artifact/include/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.ixx`, `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm`, `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- **事実:** 既存のDiligent経路には共有device、backend選択、`PrimitiveRenderer2D`、`RenderCommandBuffer`、`DiligentImmediateSubmitter`が揃っている。既存タイムラインは選択・編集・Undo・スクロール同期を集約している。
- **気づき:** モデルや入力をGPU windowへ移さず、完成済みの表示プリミティブだけを世代番号付きsnapshotとして渡せば、現行タイムラインを正規経路として保持したままDX12/Vulkan表示を並行開発できる。
- **価値・懸念:** backend固有コードと編集回帰を抑えられる。一方、現段階のsnapshot生成はUI thread上であり、全marker/clipの取得コピーとGPU submitの計測は未検証。
- **次に確認:** DX12/Vulkanでpreview切替、device初期化失敗時の復帰、長尺compositionのsnapshot構築時間、既存表示との画像parityを確認する。

## 2026-08-29 — 3D Primitive mesh 描画の Provider 境界 (Phase 1 / L1) を導入
- **関連:** Artifact/include/Render/Artifact3DPrimitiveSubmitter.ixx / Artifact3DPrimitivePipelineAdapter.ixx、Artifact/src/Render/Artifact3DPrimitiveSubmitter.cppm / Artifact3DPrimitivePipelineAdapter.cppm、Artifact/cmake/ArtifactSources.cmake、Artifact/cmake/ArtifactRenderModuleReferences.cmake、Artifact/docs/MILESTONE_PRIMITIVE3D_RENDER_PATH_2026-03-21.md (Phase 1 節追記)
- **事実:** Artifact3DLayer::draw() は draw3DLine 経由の line 描画のみで mesh 実体の GPU PSO 経路に載っていない。ShaderManager は mesh PSO を集中保持しており、3D Primitive 用の variant (Unlit / FlatLit / Wire) は未分離。Text Glyph G2 移行で provider 境界 (Contract/Adapter) 整備は終わっており、3D 側にも同形の境界を切る素地がある。ArtifactIRenderer::drawMesh は ShaderManager 直結で shadingMode int を取る。ArtifactIRenderer / ShaderManager / DiligentImmediateSubmitter はシビアコード領域のため、AGENTS.md の方針に沿って境界導入のみで止めて実体化は次フェーズ。
- **変更:** (1) Artifact3DPrimitivePipelineProvider 構造体に unlitPipeline/unlitBinding/flatLitPipeline/flatLitBinding/wirePipeline/wireBinding と hasUnlit/hasFlatLit/hasWire/isValid を追加。Phase 1 では全メンバーが nullptr。
(2) Artifact3DPrimitiveSubmitter は Stage enum (Unlit/FlatLit/Wire) と SubmitPacket (positions/normals/indices/model/view/projection + BaseColor/Emission/Opacity) を export し、uploadMesh のみ動作 (CPU 側に staging copy)、submit は Phase 1 では常に false。
(3) makeArtifact3DPrimitivePipelineProvider(ShaderManager&) は空 provider を返す stub。
(4) Artifact3DLayer::draw() には未接続、既存 draw3DLine 経由の挙動は完全に維持。
(5) CMake に 4 ファイル + module reference 2 エントリを追加。
- **価値または懸念:** Text Glyph G2 移行と同形の provider 境界が 3D 側にも用意され、Phase 2 (ShaderManager に PSO getter 追加 + Adapter 実体化) と Phase 3 (Material 全部 PBR + Custom Shader 設計レビュー) の前提が揃う。Artifact3DLayer にはまだ繋いでいないため描画の回帰リスクはゼロ。AGENTS.md の PImpl / shared_ptr / unique_ptr 禁止ルールと DiligentEngine シビアコード慎重な扱いに沿った。
- **未検証・懸念:** ビルド・ランタイム未実施 (ユーザー指示待ち)。Phase 2 で ShaderManager に PSO 取得関数を増やす際、PSO ライフタイム (ShaderManager 破棄時に Artifact3DPrimitiveSubmitter が AddRef した参照をどう扱うか) を Text Glyph と同じく検討必要。Material 全部 PBR 接続 (Phase 3) は設計レビュー前提。
- **次の確認:** ビルド成功、既存 3D Primitive (Plane/Box/Sphere/Cylinder/Cone + Torus/Capsule/Pyramid) の line 表示が変わらないこと。


## 2026-08-29 — 3D Particle 完成度マイルストーンを新設 (P4-1〜P4-6)
- **関連:** Artifact/docs/MILESTONE_3D_PARTICLE_2026-08-29.md (新規)、Artifact/src/Layer/ArtifactParticleLayer.cppm:437 (Impl() 構築)、Artifact/src/Layer/ArtifactFormParticleLayer.cppm、Artifact/src/Render/ArtifactIRenderer.cppm:1483 (drawParticles)、ArtifactCore/include/Graphics/ParticleData.ixx (Core API)。
- **事実:** ArtifactParticleLayer と ArtifactFormParticleLayer の Impl 構築で setIs3D(true) が未呼出 (他 3D 系 layer は呼出済み)。ArtifactCompositionRenderController.cppm:8913 の if (layer->is3D()) 3D bundle gate を通らず、drawParticles は常に 2D fallback (identity view + NDC proj) で billboard 描画。ArtifactCore::ParticleRenderer は setViewMatrix/setProjectionMatrix/GPU cull/indirect draw を実装済みだが、layer からの経路で view/proj が届く機会がない。ArtifactParticleLayer::draw() の 	ransformParticleRenderData は QTransform で (px, py) のみ写像し、pz/vx/vy/vz は source のまま GPU へ。emission/normal AOV gate (drawGpuLayerEmissionToTarget:11613 / drawGpuLayerNormalToTarget:11642) も Particle をスキップ。
- **変更:** MILESTONE_3D_PARTICLE_2026-08-29.md を新設し、P4-1 (3D フラグ付け + default true for new / false for legacy JSON) → P4-2 (transformParticleRenderData の 2D/3D 分岐 + ParticleRenderData.modelMatrix + ParticleRenderer::setModelMatrix 新設) → P4-3 (FormParticleLayer 3D 経路) → P4-5 (AOV 経路) → P4-4 (VelocityAligned runtime 検証) → P4-6 (Diagnostics) の段階を定義。各段階で Functional / Visual / Performance の Quality Gate と回帰なし invariant を明文化。
- **価値または懸念:** Phase 1 で Provider 境界導入のみで止めたのと同じ方針で、P4-1 は「setIs3D(true) 1 行 + JSON default 戦略」で最大効果・最小リスクを狙える。既存 2D particle プロジェクトは romJsonProperties 経由で is3D=false を保存しているため、default 戦略の判断 (新規 true / 既存 false) を P4-1 で明示する必要あり。
- **未検証・懸念:** コード作成はまだ未着手 (マイルストーン文書のみ)。P4-2 で ParticleRenderData.modelMatrix を追加すると Core ABI に影響するため、関連テストの回帰確認が必要。
- **次の確認:** P4-1 から着手。ArtifactParticleLayer::Impl 構築で setIs3D(true) を呼出し、ArtifactAbstractLayer::fromJsonProperties の復元戦略を P4-1 着手時に確定、ArtifactCompositionRenderController 11581 周辺の has3DCamera 判定に Particle Layer の is3D() を含める。

### 2026-08-29 P4-1 着手 — ArtifactParticleLayer::Impl 構築に setIs3D(true) 追加

- **追加実装:** `Artifact/src/Layer/ArtifactParticleLayer.cppm:441` の `ArtifactParticleLayer()` コンストラクタで `setIs3D(true)` を `createParticleSystem()` 直前に呼出 (1 行)。`ArtifactFormParticleLayer` は既に `Grid3D` preset 選択時に `syncSourceSize` 経由で `setIs3D(true)` 呼出済 (line 514)、`applyPropertiesFromJson` 復元経路も `syncSourceSize` を呼ぶ (line 1173) ため FormParticleLayer 側は完了済み。
- **JSON default 戦略:** 新規作成 Particle は `is3D=true` (コンストラクタの `setIs3D(true)` が生きる)、既存 particle JSON は `ArtifactAbstractLayer::toJson` (line 4533) で `obj["is3D"]` が必ず書き込まれ、`fromJsonProperties` (line 5194-5195) で `obj.contains("is3D")` のときのみ復元。**既存 particle プロジェクト (1 度保存 = `is3D=false` 永続化) は復元時に `is3D=false` のまま → 表示は回帰しない**。
- **Composition 側非改変:** `has3DCamera` 判定 (line 32874) は `activeCamera` (CameraLayer) 存在時のみ true になる設計で触らず。3D camera 行列配信の `if (layer->is3D())` bundle gate (line 8913) は `is3D()` ベースなので、Particle の `is3D()=true` で自動的に gate 通過。
- **未検証 (AGENTS.md によりユーザー指示待ち):** (1) 新規 Particle Layer を 3D composition に追加して camera orbit で billboard が立体的に動くか、(2) 既存 particle JSON を開いて 2D 表示のまま無回帰か、(3) AOV (emission/normal) gate (line 11613/11642) 通過するか。P4-1 単独では Z が `transformParticleRenderData` (QTransform 経路) で潰れたままなので、P4-2 (`modelMatrix` + `setModelMatrix`) を伴って初めて立体的に動く。

## 2026-08-29 — Liquid Container の spill ownership と長尺seek（2026-08-30更新）

- **関連:** `ArtifactCore/include/Physics/FluidSolver2D.ixx`、`ArtifactCore/src/Physics/FluidSolver2D.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- **事実:** `LiquidSolver2D` はレイヤー局所座標で状態を所有し、上辺を越えた粒子も現在のレイヤーtransformで描画する。2026-08-30に30フレーム間隔・最大256個のcheckpoint、snapshot restore、composition revision／fps／設定変更時のinvalidationを追加し、600フレーム上限は撤去した。
- **気づき:** 制作品質では「容器内はlayer-local、流出後はcomposition/world-space」への所有権移行と、revision付きcheckpointが必要。2026-08-30に上辺通過粒子の抽出、流出フレームtransformによるposition／velocity移行、world-down更新を実装した。初期充填だけでは「容器へ注ぐ」制作ができないため、開口辺からのrate／width／speed／position付き決定的inflowも追加し、小数粒子carryをcheckpoint対象にした。inflowは開口幅に収まる1列、開口端1 spacing余白、既存粒子のuniform-grid占有判定で制限し、壁外や詰まった入口へ同座標粒子を重ねない。PolygonのOpening Edgeは-1自動／0以上手動指定とし、横口でもCore境界、流入、流出、壁previewが同じedgeを参照する。
- **価値または懸念:** 容器を移動してもこぼれた水が追従せず、checkpointにはcontainerとspillを一組で保存するためseekでも所有権移行を再現できる。2026-08-30に既存CollisionのBox／Circle／Polygonへ低反発point-contactと前位置からのsweep判定を接続し、同一レイヤーのCollision Polygonは最上辺を自動開口したcontainerとしてCoreへ渡した。さらにcontainer／spill双方のCore stateへ法線衝突速度を指数減衰させるimpact履歴を持たせ、流出時にworld単位へ変換して引き継ぎ、surface foam抽出へ渡した。spillの弱い凝集・分離・近傍粘性に加え、container内の距離制約・粘性も決定的uniform gridへ移し、局所近傍外の総当たりを避けた。container Surface Tensionは反発stiffnessだけでなく1.55 spacing内の弱い凝集にも接続した。simulationと同じ矩形／Polygon辺を既存Diligent line commandで可視化し、開口辺は描かない。Polygon sweepは完全なswept-circleではなく、spillも非圧縮性液体solveではない。
- **次に確認:** 容器を回転／移動しながら注ぐruntime確認、checkpointのメモリ・同一frame seek一致、凹Polygon／薄い斜辺、矩形／Polygon内部壁と外部床のimpact foam連続性、10万粒子時のgrid負荷、凝集係数の見た目、Spill Cull Margin境界を跨ぐframeの再現性を検証する。長尺粒子数対策は一律寿命ではなくcomposition外余白cullとし、床に溜まった水は保持する。Coreの同一入力、snapshot replay、不正snapshot、inflow occupancy／Polygon方向、spill相互作用、surface全laneの決定性は既存PhysicsDeterminismTestへ回帰ケースを追加済みだが未実行。surface sample全fieldの有限値／範囲検証とdensity radius 8cell上限も追加した。

## 2026-08-30 — 2D Particleは既存Generatorを正規基盤にできる

- **関連:** `Artifact/include/Generator/ArtifactParticleGenerator.ixx`、`Artifact/src/Generator/ArtifactParticleGenerator.cppm`、`Artifact/src/Layer/ArtifactParticleLayer.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- **事実:** `Artifact.Generator.Particle` は既に fixedTimeStep、maxSubSteps、randomSeed、deterministic replay、self-collision broadphase、`ParticleRenderData` captureを持つ。一方、これらのsimulation設定はParticle LayerのPropertyとJSONに未接続だった。
- **気づき:** Liquid内部粒子と通常Emitterを同じsolverへ統合する必要はなく、authoritative solverは分離したまま immutable snapshotを共通 `ParticleRenderData` に変換すれば、2D描画・LOD・Diligent backendだけを安全に共有できる。
- **価値または懸念:** 通常Particleの既存制作機能を維持し、Liquid固有の密度制約をParticle emitterへ漏らさずに描画基盤を共通化できる。2026-08-30にLiquidだけはCoreで疎なdensity nodeからthickness付き三角形surface、同thresholdのcontour、高速・低density／impact sampleのfoam pointを抽出し、既存Diligent PrimitiveRenderer2DとParticleRendererへ渡した。Surface／Edge／Foam量とLiquid／Foam Colorはcomponent PropertyとJSONで調整でき、Colorは既存QColor UI境界・内部FloatColorに統一した。通常Liquid detail 95,000＋foam 4,096で100,000 vertex上限内に収め、authoritative simulationやGPU resourceをsurface cacheへ移していない。新規ParticleLayerは2D既定へ変更し、旧JSONの明示is3Dは維持する。通常Particle GPU drawの欠落captureと毎frame debug logも整理した。world-space変換ではpositionに加えてvelocityとparticle rotationも2D layer transformへ追従させ、VelocityAligned方向の不一致を解消した。
- **次に確認:** 同一seed／frameの保存再読込一致、appearance／Color Propertyのpicker操作とJSON round-trip、Liquid thickness／contour／foamのD3D12／Vulkan表示、遠距離spillを含むsurface LOD、最大20,000 contour packetとfoam抽出のframe-timeをruntime確認する。

## 2026-08-30 — 作成プリセットは生成定義と実体生成を分離する

- **関連:** `Artifact/include/Project/ArtifactPresetManager.ixx`、`Artifact/src/Project/ArtifactPresetManager.cppm`
- **事実:** エフェクト／マスクの既存プリセット管理へ、UIやComposition／Layer APIに未接続の作成プリセット定義を追加した。
- **気づき:** 平面＋円形マスクのような時短構成は、まず型付きの生成定義として保存し、後から既存生成APIへ接続すると責務の混線を避けられる。
- **価値または懸念:** 標準プリセット一覧とJSON交換の基盤を先に用意できる。一方、実際のマスク頂点生成とUI導線は未接続で、後続作業が必要。
- **次に確認:** 既存Layer／Composition生成APIへ接続する際のUndo・選択・親子関係の契約を確認する。

## 2026-08-30 — コンポジション作成プリセットをレイヤー構成まで拡張

- **関連:** `Artifact/include/Project/ArtifactPresetManager.ixx`、`Artifact/src/Project/ArtifactPresetManager.cppm`
- **事実:** 作成プリセットに背景色、フレームレート、尺、子レイヤー定義を追加し、背景＋円形画像の標準コンポジションプリセットを追加した。
- **価値または懸念:** コンポジションを単一設定ではなく、順序付きレイヤー構成として交換できる。実体生成・画像パス・親子関係・Undo接続はまだ未実装。
- **次に確認:** 既存のComposition／Layer生成APIへ、JSONの順序とマスク指定を安全に適用する変換境界を設計する。

## 2026-08-30 — Bindlessはlegacy併存のopt-in経路として育てる

- **関連:** `Artifact/include/Render/DiligentBindlessSubmitter.ixx`、`Artifact/src/Render/DiligentBindlessSubmitter.cppm`
- **事実:** Bindless submitterは既存legacy submitterをfallbackとして内部保持しているが、常時有効／無効の明示的な運用ポリシーと送信結果カウンタは未整備だった。
- **変更:** Bindlessをデフォルト無効にし、明示的な `setEnabled(true)` でのみ試せるようにした。未対応 packet、初期化失敗、upload失敗は既定でlegacyへ戻り、attempted／accepted／fallback／rejectedを取得できるようにした。
- **価値または懸念:** 全面置換せず、限定 workload の canary として実機比較へ進められる。現時点ではArtifactIRendererの標準submitter自体をBindlessへ交換していない。
- **次に確認:** 明示的な接続作業で、設定の保存場所、frame単位の統計リセット、legacy／bindless画像差分とD3D12／Vulkan parityを確認する。

## 2026-08-30 — バッチテンプレート適用の安全性を先に固める

- **関連:** `Artifact/src/Render/ArtifactBatchRenderer.cppm`
- **事実:** テンプレート適用時の出力設定変数が、Render Queueから取得される前に条件式のfallback値として参照されていた。またテンプレート保存は直接ファイルへ書き込んでいた。
- **変更:** 出力設定を取得してからoverride値を解決するよう修正し、テンプレート名・必須項目を検証、保存を `QSaveFile` のcommit方式へ変更した。
- **価値または懸念:** バッチ追加時の未定義値利用と、保存途中の破損リスクを減らせる。実際の複数ジョブ並列化やruntimeレンダー性能は未検証。
- **次に確認:** queue側のジョブ追加失敗を戻り値で追跡できる契約、並列化時のGPU context／readback所有権、frame順序保証を確認する。

## 2026-08-30 — バッチテンプレートに共有検証入口を追加

- **関連:** `Artifact/include/Render/ArtifactBatchRenderer.ixx`、`Artifact/src/Render/ArtifactBatchRenderer.cppm`
- **事実:** テンプレート保存時だけ一部の入力検証を行い、テンプレート適用時には同じ制約が共有されていなかった。
- **変更:** `BatchTemplate::isValid()` と `ArtifactBatchRenderer::validateTemplate()` を追加し、保存と一括ジョブ追加の両方で名前、出力先、ファイル名、解像度、FPS、フレーム範囲、bitrate、paddingを検証するようにした。
- **価値または懸念:** UI未接続でも、バッチ入口ごとの入力制約を揃えられる。個別ジョブ追加の戻り値がvoidであるため、追加後の失敗理由まではまだ収集できない。
- **次に確認:** Render Queue APIにジョブ追加結果を返す境界を設計し、並列レンダー導入前に失敗ジョブを明示的に分類する。

## 2026-08-30 — バッチ診断の保持入口を先行追加

- **関連:** `Artifact/include/Render/ArtifactBatchRenderer.ixx`、`Artifact/src/Render/ArtifactBatchRenderer.cppm`
- **変更:** 直近バッチの診断を保持・消去する `lastBatchErrors()` / `clearBatchErrors()` を追加し、全コンポジション一括入口と通常のコンポジション一括入口で処理開始時に診断をリセットする契約を整えた。
- **価値または懸念:** 後続でRender Queueの失敗結果を収集してもUIシグナルを増やさず公開できる。現時点では全ての失敗分岐への詳細記録と、実レンダー失敗の収集は未接続。
- **次に確認:** job追加APIの戻り値化またはqueue snapshotから、コンポジションID単位の失敗理由をこの診断へ集約する。

## 2026-08-30 — Timeline painter品質は既存surfaceへの局所適用から始める

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimelineNavigatorWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`、`Artifact/src/Widgets/Timeline/ArtifactWorkAreaControlWidget.cppm`
- **事実:** 正規右ペインの `ArtifactTimelineTrackPainterView` はAAを明示無効化していた一方、周辺surfaceはAAのみ部分的に有効化していた。`TimelineScaleWidget` はpixel-perfect tickのためAA無効化を意図的に維持している。
- **変更:** 上記5 surfaceでアンチエイリアス、テキストアンチエイリアス、平滑pixmap変換を明示的に有効化した。
- **価値または懸念:** 既存のowner-draw責務や入力経路を変えず、タイムラインの線・文字・アイコンの表示品質を揃えられる。実機でのrulerとの見え方、細線の鮮明さ、描画負荷は未検証。
- **次に確認:** `Consolas` のtheme経由化、hover tracking、実機でのlight/dark表示と性能を確認する。


## 2026-08-29 - VP とタイムラインの「操作感の悪さ」「安定性のなさ」の正体

- 関連: docs/bugs/COMPOSITION_EDITOR_PERFORMANCE_2026-03-26.md, COMPOSITION_EDITOR_PERF_ANALYSIS_2026-04-11.md, BUG_TIMELINE_4ISSUES_2026-04-19.md, BUG_RENDER_SCHEDULER_THREAD_FLOOD_2026-04-18.md, EVENTBUS_CASCADE_REDRAW_PERF_2026-04-05.md, EVENTBUS_FLOW_ANALYSIS_2026-04-12.md, BUG_FIX_COMPOSITION_VIEWPORT_INTERACTION_PERF_2026-03-25.md, BUG_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md, INSPECTOR_LAYER_SELECTION_NOT_UPDATING_2026-04-07.md, ISSUE_REPORT_LAYER_SELECTION.md, LAYER_OPACITY_UI_UPDATE_ISSUE_2026-04-05.md, PROPERTY_WIDGET_PERFORMANCE_2026-03-27.md, docs/analysis/SHARED_DEVICE_AND_IMAGE_CACHE_AUDIT_2026-08-11.md, docs/analysis/REPORT_CE_RENDER_ROI_2026-06-16.md
- 事実: VP 側の重さ (60-70% が readbackToImage の GPU 同期ストール 10-30ms/frame, 20-25% が QImage::cacheKey ベース texture cache ミスで毎フレーム GPU テクスチャ再作成 5-15ms, 10-15% が renderOneFrame() の 31 箇所からの多重実行で 1 イベント最大 4 回の再描画) と、タイムライン 4 issues (composition 作成時 worker thread 一斉起動フリーズ, playhead ghost pixel, playhead と ruler で異なる orange hardcode, 左/右ペインのツリー展開後非同期) と、EventBus 二重発火 (Qt signal + EventBus publish 同居、ブレンドモード変更が ProjectChangedEvent を全体 broadcast、レイヤー追加で 5 種イベント連続発火) と、選択系バグ (LayerSelectionChangedEvent の compositionId が weak_ptr 期限切れで空になり Inspector が NoLayer 化、プロパティ編集時の selection リセット、Property Widget 1 選択変更で約 400 widget 破棄/再作成 + 230 回 icon ファイル I/O) が、2026-03 から 2026-04 にかけて大量に文書化されている。さらに docs/analysis/SHARED_DEVICE_AND_IMAGE_CACHE_AUDIT_2026-08-11.md は 2026-08-11 時点でも「GPU→CPU readback は同期的に Flush()/WaitForIdle() するため、hot path に追加しないこと」と注意喚起しており、構造的解決には至っていない。
- 気づき: ユーザーが「操作感の悪さと安定性のなさが弱点」と感じるのは、planned milestone の「機能追加」とは別系統の「ホットパスの本質的な重さ」が解消されていないから。タイムラインは owner-draw 化など表示系の基盤が育った一方、VP 側は「CPU で QImage 化 → GPU 転送 → 同期 readback」の 3 段を経由する設計が hot path に残っており、1 操作あたりのフレーム予算を食い潰している。タイムラインと VP の相互作用 (タイムラインを触ると VP がフリーズ) は EventBus の「同じ操作で複数イベントが多重発火」+「購読者ごとに重い処理が再走する」構造に由来する。BUG_TIMELINE_4ISSUES_2026-04-19.md の 4 つは fix 報告されているが、SHARED_DEVICE_AND_IMAGE_CACHE_AUDIT_2026-08-11.md は 4 ヶ月経ってもなお hot path readback 禁止を喚起しており、4 issues fix 後も「体感の重さ」は構造的に残っている可能性が高い。
- 価値・懸念: DCC 感ギャップ (MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-29.md) は表面の仕上げ、操作感/安定性はホットパスの重さそのもので、対策の粒度が違う。同時進行で進めると conflict するため、優先度設計が必要: (A) VP の GPU readback 廃止/QImage キャッシュキー修正/renderOneFrame 多重実行の統合、(B) EventBus publish+Qt signal 二重発火の解消、購読者ごとの dedup キー化、(C) Property Widget の widget pool 化と icon cache 永続化、(D) タイムラインの playhead/track 同期安定化、の 4 軸を独立 milestone として起こし、Phase ごとに GPU readback 廃止→Property Widget リビルド抑制→EventBus 統合→タイムライン同期 の順で攻めるのが現実的。
- 次に確認: 個別の fix 済み/未修正を最新コードで再照合し、docs/analysis/SHARED_DEVICE_AND_IMAGE_CACHE_AUDIT_2026-08-11.md が言及する hot path readback が現在も生きているか、readbackToImage() の呼び出し点がエクスポート時限定になったか、PrimitiveRenderer2D.cppm の image.cacheKey() がレイヤー ID ベースに置換されたか、EventBus 移行が一段落したかを再確認。可能なら MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-29.md を planned/ に起こし、status index の「DCC 感ギャップ」と並列に「ホットパス安定性」を 1 軸として加える。
## 2026-08-30 — Particleの次元は可変Propertyではなくレイヤーidentityにする

- 関連: `Artifact/include/Layer/ArtifactAbstractLayer.ixx`、`Artifact/include/Layer/ArtifactParticleLayer.ixx`、Layer作成・JSON復元
- 確認できた事実: 従来は単一`LayerType::Particle`と汎用`is3D`の組合せで2D／3Dを表しており、作成導線と保存上のidentityが曖昧だった。
- 気づき: 2D／3Dではcamera、depth、transform、collisionの意味が異なるため、共通simulation／rendererを再利用してもレイヤーidentityは分けたほうが不正な中間状態を防げる。
- 価値・懸念: 既存IDを維持した追加enumと旧`is3D=true`移行で互換性を確保できる。一方、3D Particleの完全なmodel/view/projection経路はruntime未検証。
- 次に確認すべきこと: 新規2D／3D作成、JSON round-trip、旧3D指定Particle移行、3D camera orbitとdepthの実表示を同一buildで検証する。


## 2026-08-30 - マイルストーンを「2026-08-30 更新版」として別ファイルで並存させた

- 関連: docs/planned/MILESTONE_TIMELINE_STATUS_INDEX_2026-08-29.md（原本）, MILESTONE_TIMELINE_STATUS_INDEX_2026-08-30.md（更新版）, MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-29.md / _2026-08-30.md, MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-29.md / _2026-08-30.md, docs/DOC_LIFECYCLE.md
- 事実: DOC_LIFECYCLE.md は「ファイル名に日付が含まれていても、最終更新日の代わりにはならない」と明記しており、本来は「ファイル名据え置き + ファイル内 **最終更新:** のみ更新」が正規運用。今回はユーザー指示で 2026-08-29 作成の 3 ファイルを 2026-08-30 ファイルとして新規作成し、原本は「別物として残す」並存構成とした。2026-08-30 ファイル群の冒頭には「2026-08-30 更新版」但し書きと原本リンクを入れ、相互参照は 2026-08-30 系列で揃えた。
- 気づき: DOC_LIFECYCLE.md の正規運用と、ユーザー指示の「ファイル名日付も更新」は、updated by date と naming by date の 2 軸を分離する形に結果的になった。並存構成は「上書きによる履歴喪失」を避けられる一方、リンク集（status index など）が「最新版を指す」仕組みを別途用意しないと古い版と新しい版が混在して探索を阻害する。今回は status index 2026-08-30 自身と 2026-08-30 ファイル群を新系列で揃え、2026-08-29 ファイル群は自己完結として残したが、AGENTS.md にはこのルールが明文化されていない。
- 価値・懸念: 並存構成の利点は (1) 作成日と最終更新日がファイル名から読める、(2) 過去の議論経緯を物理的に保持できる、(3) Index/検索で「2026-08-30 を見る」が明示できる。懸念は (1) 同名異日付のファイルが並ぶと新規着手者がどちらを正とするか迷う、(2) 相互参照を 2 系列並走させると更新漏れが出やすい、(3) DOC_LIFECYCLE.md に「ファイル名 = 最終更新日」運用を正式採用するか、「上書きで **最終更新:** のみ更新」を徹底するかをいずれかに統一したい。
- 次に確認: ユーザー判断で「並存を続ける」か「次回以降は 2026-08-30 系列に集約して 2026-08-29 を archived/ へ移す」かを聞く。可能なら docs/DOC_LIFECYCLE.md に「ファイル名日付運用」セクションを 1 段落追加（並存 / 上書き / archived 移動の 3 選択肢）し、判断材料を残す。


## 2026-08-30 - VP 上のモーションパス描画の問題点

- 関連: Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm (42443 行), Artifact/src/Widgets/ArtifactTimelineGlobalSwitches.cppm, AGENTS.md L38, docs/planned/MILESTONE_APP_SETTINGS_WIDGET_GAP_2026-06-13.md, MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md
- 事実: (1) L35704-L35706 に「// Temporarily disable motion path overlay while debugging stray frame-like rectangles in the viewport.」とコメントで旧実装全体（170 行超）が残ったまま死蔵。L35708-L35800+ の旧ブロックは //  付きで描画パイプライン本体から呼ばれていない。(2) 新実装は L37009-L37350 と L41630-L41700 の 2 箇所に並走。L37011-37013 と L41632-41634 で同じ profile scope 名 "MotionPath" を持つブロックが 1 フレーム内に 2 回走る構造。(3) L37096-L37107 のキャッシュキーは (layerId, framePos, overlaySerial) の 3 要素のみで、複数選択時（selectedLayerIds）や layer 切替時には毎フレーム cache miss → 「300-iteration getGlobalTransformAt() loop is the main >1000ms bottleneck」と L37088-L37095 に注記された重いループが再走する。(4) L41680-L41698 で 
enderer_->setZoom(1.0f) / setPan(0,0) でレンダラ内部状態を直接書き換え、drawPastFixedPlaneMotionFrames 後に setZoom(previousZoom) / setPan(previousPanX, previousPanY) で復元するが、例外/early return 時にレンダラが zoom=1 に固定される race condition のリスク。(5) L37319-L37350 で各 keyframe に対し drawDashedRectOutline を 影 + 本体の 2 回呼ぶ。keyframe が多いと描画コールが線形増。(6) L37080 の pathColor は FloatColor{0.9f, 0.4f, 0.8f, 0.9f} のピンク系だが、AGENTS.md Visual Language のレイヤー色ガイド（Video=青, Audio=緑, Text=紫, Effect=橙）と一貫していない。タイムラインのモーションパス色はタイムライン ruler の playhead orange / accent とも別系統。
- 気づき: VP モーションパスの問題点は「機能がない」でも「描画品質が低い」でもない。**実装は揃っているが、(a) 旧実装が「一時無効」のコメントで残ったまま 170 行超の死蔵コード化し、(b) 新実装が本体パイプラインとは別の "Historical Plane frames" 経路に重複し、(c) キャッシュが 1 レイヤー前提で複数選択や layer 切替のたびに 1000ms 級の重いループが再発火し、(d) レンダラ内部状態を draw call の前後に書き換えるパターン（zoom=1→復元）が副作用リスクを抱え、(e) 色トークンが Visual Language 整備と整合していない**という 5 つの独立した問題が層になっている。AGENTS.md には「compositionShowMotionPathOverlay は timeline ボタン経由で設定更新される」と書いてあるが、その値を消費する新実装は Render Controller の奥深くに 2 箇所に分散しており、設定更新のシリアル（overlayInvalidationSerial_）で関連付けられている。
- 価値・懸念: モーションパスは DCC の必須機能（AE / Blender / Nuke / Cavalry / Cavalry すべてで「VP 上のアニメ経路可視化」を提供）で、layer の時間編集の理解に直結する。現状でも単一レイヤー単一フレームでキャッシュが効いているときは軽いが、複数選択 / 連続 scrubbing / レイヤー切り替えで 1000ms を超えるボトルネックが再発する。MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md の Phase 1〜3（GPU readback 排除 / texture cache key 安定化 / renderOneFrame 統合）が成立する前段として、モーションパスのホットループ（getGlobalTransformAt × 300）も具体的な着手対象になる。
- 次に確認: (1) 
enderMotionPathOverlayForLayer の本体定義（L37009 側と L41652 呼び出しのどちらが正典か）を追う、(2) motionPathPositionKeyTimes / motionPathAdaptiveSampleStep / motionPathPositionInterpolation / motionPathInterpolationColor のヘルパ定義を確認、(3) drawPastFixedPlaneMotionFrames の本体を確認、(4) 旧コメントアウトブロックの削除 or 復活の判断を整理。必要なら MILESTONE_VP_MOTIONPATH_OVERLAY_FIXES_2026-08-30.md を planned/ に起こし、5 つの問題を 5 Phase（コメントアウト削除 / 新実装の単一経路化 / キャッシュを複数選択対応 / draw call 削減 / 色トークン統一）で解消する計画を立てる。


## 2026-08-30 - VP モーションパスの問題を planned milestone に起こした

- 関連: docs/planned/MILESTONE_VP_MOTIONPATH_OVERLAY_FIXES_2026-08-30.md (新規), Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm (42443 行), docs/planned/MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md, docs/planned/MILESTONE_TIMELINE_STATUS_INDEX_2026-08-30.md
- 事実: VP モーションパス機能には (1) L35704-L35800+ に「// Temporarily disable motion path overlay while debugging stray frame-like rectangles」とコメントで残った 170 行+ の死蔵コード、(2) 新実装が L37009-L37350 と L41630-L41700 の 2 箇所に分散し同じ profile scope 名 "MotionPath" が 1 フレームに 2 回走る、(3) キャッシュキーが (layerId, framePos, overlaySerial) 単一で複数選択や layer 切替のたびに 300-iteration getGlobalTransformAt ループ（>1000ms bottleneck）が再発火する、(4) L41680-L41698 で renderer_ の zoom/pan/canvasSize/externalMatrices を draw call 前後に直書きして例外/early return で zoom=1 に固定される race condition リスク、(5) pathColor {0.9f, 0.4f, 0.8f, 0.9f} のピンクが AGENTS.md Visual Language のレイヤー色ガイドと非整合、(6) 各 keyframe で drawDashedRectOutline を影+本体 2 回呼ぶ O(N) 描画コールの 6 問題。MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md の Phase 1 (renderOneFrame 統合) の前段として、Motion path のキャッシュが効くことで hot-path 計測が正確になるため、依存関係を明示して並走前提で planned に起こした。
- 気づき: VP モーションパスは「機能がない」のではなく「層状に問題が積まれている」事例。layer の transform 計算 300 回ループのような重い処理を 1 レイヤー前提キャッシュで隠蔽している構造は、複数選択や layer 切替が日常の DCC では必ず破綻する。同様の「単一前提キャッシュで隠蔽した重い処理」は、Property Widget (約 400 widget 破棄/再作成)、selection reset (リフレッシュ時の選択保持漏れ) など、L2/L3 問題にも共通するパターン。AGENTS.md の「D3D12 / Diligent backend の低レベル実装を変更する場合は推測で広く触らない」ルールは Phase 6 (keyframe 描画 batch 化 / instanced 描画) で特に有効。Color token 化は MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md Phase 4 と並走させることで、timeline 側と VP 側の両方で一貫したテーマ追従が得られる。
- 価値・懸念: Phase 1 (死蔵コード削除) と Phase 2 (新実装統合) はビルド影響が局所的で着手しやすい。Phase 3 (複数選択キャッシュ) と Phase 6 (keyframe 描画 batch 化) は中〜高コストで、計測環境 (Frame Debug) の整備が先。Phase 4 (scope guard 化) は AGENTS.md の「helper 化、しかし変更範囲を最小化」を守りつつ RAII パターン導入で副作用リスクを下げられる。Phase 5 (色 token 統合) は Visual Language Phase 4 と協調が必要で、片方だけ進めると不整合が残る。
- 次に確認: ユーザー判断で「Phase 1 (死蔵コード削除) から着手」「Phase 4 (scope guard 化) のみ先行」「マイルストーン全体は別判断」「status index の未着手カテゴリの優先度を再整理」のどれか。希望があれば Phase 1 の最小着手（grep で buildMotionPathSamples / MotionPathSample / MotionPathSampleKind の参照 0 を確認）を実行し、結果を返す。

## 2026-08-30 - 操作状態の不一致は既存診断surfaceで観測する

- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx`、`docs/analysis/OPERATION_STATE_MAP_2026-08-30.md`
- 確認できた事実: ProjectService、ActiveContext、Playback は composition をそれぞれ保持し、通常の切替は ProjectService → ActiveContext → Playback の順に伝播する。`WorkspaceAutomation` は既に3サービスと選択状態をread-onlyで取得できる。
- 気づき: 同期失敗を直ちに新しいグローバルイベントで埋めるより、同一snapshotで3 owner の ID を照合し不一致を可視化するほうが、原因の切り分けと既存経路の保全を両立できる。
- 価値・懸念: `operationState` は検出だけなので既存操作を変更しない。一方で実ランタイムの切替・再生・Undo中の状態推移は未検証であり、自動修復の根拠にはできない。
- 次に確認すべきこと: 2 composition を繰り返し切替え、再生中切替と選択変更を含む操作で `COMPOSITION_STATE_MISMATCH` が発生する条件と復旧経路をruntime受入表へ記録する。

## 2026-08-30 - タイムラインのパン軸はドラッグ開始時に固定する

- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、`docs/planned/MILESTONE_TIMELINE_ZOOM_PAN_2026-04-10.md`
- 確認できた事実: 右ペインは既に中クリックによる両方向パンを持っていたが、Shift / Alt で軸を分ける処理はなかった。
- 気づき: ドラッグ開始時の修飾キーを保持すると、操作中にキーを離した場合でも移動軸が変わらず、パンの意図が安定する。
- 価値・懸念: この変更は表示offsetだけに限定され、selection、time、Undoを変えない。実機でtrackpad／高DPI環境を含む操作確認は未実施である。
- 次に確認すべきこと: 通常、Shift、Alt の3ケースで長尺compositionをパンし、navigator、scrub bar、work area との同期を確認する。

## 2026-08-30 - renderer state guard は観測可能な状態から小さく適用する

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/include/Render/ArtifactIRenderer.ixx`
- 確認できた事実: Historical Plane frames の overlay は zoom / pan を変更してから手動で復元していた。`ArtifactIRenderer` は zoom / pan の getter を持つが、canvas size と external-matrices 有効状態の getter は持たない。
- 気づき: 復元値を取得できない状態まで含めた汎用 guard を推測で追加すると、既存描画状態を逆に壊し得る。まず getter がある zoom / pan だけを RAII にして例外安全性を確保するのが安全な境界である。
- 価値・懸念: Diligent の D3D12 / Vulkan 共通抽象の上だけで完結し、backend 固有の状態には触れない。canvas size と external matrices の完全な scope 化は getter 契約を設計した後に行う必要がある。
- 次に確認すべきこと: Frame Debug で Historical Plane frames を描画した後の zoom / pan を確認し、external matrices / canvas size が後段の overlay に漏れないかをruntimeで検証する。

## 2026-08-30 - GPU readback は用途別に分けて扱う

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Export/ArtifactExportPreRenderPipeline.cppm`、`Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 確認できた事実: 現行の `readbackToImage()` は主にexport / offscreen出力と明示viewport snapshot / channel表示にある。adjustment-layer fallback だけは通常描画でGPU targetを`QImage`へ戻している。
- 気づき: すべてのreadbackを同じflagで止めると、正当なexport出力まで壊す。通常VPのfallbackだけを独立してGPU経路へ寄せる問題として切り出す必要がある。
- 価値・懸念: 分類により高コスト箇所を狭められる一方、adjustment fallbackは互換性の責務を持つため、GPU代替の挙動と品質をruntimeで比較するまで置換できない。
- 次に確認すべきこと: adjustment-layer fallbackの到達条件、GPUで代替可能な効果集合、D3D12/Vulkan両backendでの出力差をFrame Debugと実機で確認する。

## 2026-08-30 - 操作信頼性はcommand境界とruntime受入を分離する

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`docs/analysis/OPERATION_STATE_MAP_2026-08-30.md`
- 確認できた事実: Particle drag、transform drag、keyframe edit、slide / ripple editは、既存のmacroまたはsnapshot commandを使い、差分がない場合のUndo pushを抑止している。
- 気づき: 主要なUndo commandが存在することと、selection・current frame・preview cacheまで完全に復元することは別の検証項目である。静的調査は前者を示せるが後者を代替できない。
- 価値・懸念: 新しいcommand基盤を増やさず、runtime受入で追うべき状態を絞り込める。複数選択やキャンセルの全組合せは未検証である。
- 次に確認すべきこと: 4経路をUndo / Redo / Esc / 別pane移動 / 保存再読込で反復し、状態snapshotと目視結果を受入表へ追記する。

## 2026-08-30 - Property surfaceもinput contextの正規ownerにする

- 関連: `Artifact/include/Widgets/ArtifactPropertyWidget.ixx`、`Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- 確認できた事実: Viewport、Timeline、Layer Panel、Inspectorはfocus時にInputOperator contextを設定する一方、`ArtifactPropertyWidget` には同等のfocus処理がなかった。
- 気づき: Property Editorを独立surfaceとして扱うなら、focusだけでselectionやcompositionを変えず、入力routingを `Panel.Properties` として明示する必要がある。
- 価値・懸念: 新規signal/slotなしで既存InputOperatorを再利用できる。子editorにfocusがある場合を含む実際のshortcut解決順はruntime未検証である。
- 次に確認すべきこと: Property Editorの数値入力中、Viewport / Timelineへのfocus移動後、G/R/S・Undo/Redo・Escが正しいsurfaceで処理されるかを確認する。

## 2026-08-30 - Dock Panel Add Menu は実装済みで検証待ち

- 関連: `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`、`Artifact/src/Widgets/ArtifactMenuBar.cppm`、`docs/planned/MILESTONE_DOCK_PANEL_ADD_MENU_2026-08-15.md`
- 確認できた事実: Viewメニューのカテゴリ別追加／再表示、右上`+`、最近使用・お気に入り、アクセシビリティmetadataが実装されている。2026-08-30 時点で、`RecentDockIds`／`FavoriteDockIds` は表示タイトルを保存していたため、`ArtifactMainWindow` の ID 解決 API を介する Dock ID 保存と旧値の安全な正規化へ修正した。
- 気づき: 文書のNot Started表記は現行コードと矛盾しており、未実装として追加変更を重ねるよりruntime受入へ移すべき状態だった。
- 価値・懸念: 重複実装を避け、残課題を実際のdock復元・狭幅・キーボード確認に絞れる。各Dockの実機配置／保存復元は未検証である。
- 次に確認すべきこと: closed / floating / tabbed のDockを追加・再表示し、recent/favoriteとworkspace save/reloadがDock IDで安定するか確認する。

## 2026-08-30 - Modulation UIはservice境界の前にmutable routerを戻す必要がある

- 関連: `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、`Artifact/src/Service/ArtifactEffectService.cppm`、`Artifact/src/Undo/UndoManager.cppm`
- 確認できた事実: `UndoManager::push()` はcommandの`redo()`を直ちに実行する。Property Editorのmodulation追加はrouterを直接変更してからserviceを呼んでいたため、serviceが取得するbefore snapshotがafterと同一になっていた。
- 気づき: UIが編集用にmutable modelを一時利用する場合でも、正規serviceへ渡す前にbeforeへ復元しなければ、snapshot commandのUndo契約を破る。
- 価値・懸念: 追加操作は既存のservice / commandを再利用した1 transactionになる。効果の編集UI全体で同じ一時変更パターンが残っていないかは未検証である。
- 次に確認すべきこと: LFO / Random / Macroの追加でUndo / Redo、ダイアログキャンセル、source生成失敗時のrouter状態とdirty / preview通知をruntimeで確認する。

## 2026-08-30 - ArtifactScriptの位置診断はmethod宣言を最小の安定アンカーにできる

- 関連: `ArtifactCore/include/Script/ArtifactScript/ArtifactScript.ixx`、`ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm`、`tests/ArtifactCore/ArtifactScriptTest.cpp`
- 確認できた事実: `ArtifactScriptHost::installCompositionApi()` はComposition API第一弾を既に登録する。パーサーはmethod宣言を行単位で読む一方、method body ASTにはstatementごとのsource locationを保持していなかった。
- 気づき: 全ASTの位置追跡へ広げずとも、method宣言のline / columnを保存して評価器の入口でerrorを補足すれば、既存のtree-walkを壊さずに実用的な診断起点を出せる。
- 価値・懸念: host呼出しや未知関数の失敗を該当methodへ結びつけられる。body内の正確なstatement位置、nested callの重複prefix、構文解析時の厳密な診断は未検証である。
- 次に確認すべきこと: nested method呼出し、loop内エラー、parse失敗を含むテストを追加し、必要ならAST node単位のsource spanを別スライスとして導入する。

## 2026-08-30 - Foundation拡張は集約exportと明示source manifestを同時に更新する

- 関連: `ArtifactCore/include/Core/ArtifactFoundation.ixx`、`ArtifactCore/cmake/ArtifactCoreSources.cmake`、`tests/ArtifactCore/CMakeLists.txt`
- 確認できた事実: ArtifactCoreは新規moduleを自動探索せず、明示source manifestで収集する。Foundationへのexportだけではコンパイル対象にならない。
- 気づき: template-onlyのユーティリティでも、公開導線・module manifest・個別test targetを一つの変更単位で揃える必要がある。
- 価値・懸念: 新型を既存Resultや所有Functionの置換なしで段階導入できる。C++20 modulesの実際のBMIスケジューリングと各test targetは未ビルドである。
- 次に確認すべきこと: `ArtifactCore` buildと新規3 test targetを実行し、MSVCのmodule scanとborrowed callableの寿命契約を確認する。

## 2026-08-30 - Noise Layer の GPU 化は接続済みで runtime parity が残る

- 関連: `Artifact/src/Layer/ArtifactNoiseLayer.cppm`、`docs/planned/MILESTONE_NOISE_LAYER_2026-08-24.md`
- 確認できた事実: color mapping 無効時、Noise Layer は `ProceduralTextureComputePipeline` で RGBA16F texture を生成し、既存の texture-view sprite draw に渡す。設定署名キャッシュ、device 切替時の再初期化、生成失敗時および color mapping 時の `ImageF32x4_RGBA` fallback も実装済みである。
- 気づき: GPU 経路を「未着手」としたまま追加実装を重ねるより、Diligent の D3D12/Vulkan 共通抽象にある既存経路を正典として、backend 出力・device reset・cache 統合の runtime 受入へ課題を絞るべきである。
- 価値・懸念: 重複したGPU資源や同期機構を増やさずに済む。一方、GPU生成結果の実機品質・復旧・CPUとの差は未検証である。
- 次に確認すべきこと: D3D12/Vulkan で各noise kindを表示し、設定変更・device reset後の再生成とCPU fallback出力を確認する。

## 2026-08-30 - Shape Extrude はカード経路と別の mesh ownership を要する

- 関連: `ArtifactCore/include/Geometry/ShapeExtrude.ixx`、`ArtifactCore/src/Geometry/ShapeExtrude.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`tests/ArtifactCore/ShapeExtrudeTest.cpp`
- 確認できた事実: Shape Layer の3D表示は `DirectShape3DCard` が2D輪郭を受け取り `draw3DShape()` する経路で、生成 `Mesh` やmaterialを所有しない。一方 `extrudeContourMesh()` は position / normal / uv を持つ通常Meshを生成できる。
- 変更: 閉じた矩形、bevel、不正入力時のoutput保持を `ArtifactCoreShapeExtrudeTest` の回帰対象にした。
- 価値・懸念: 押し出し core の変更を守りつつ、既存のカード表示を退行させない。Shape→mesh 接続には3D render queueでのmesh cache・material・property/JSONの明示設計が必要であり、カード抽出へ暗黙に混ぜてはならない。
- 次に確認すべきこと: test build後、Shape Layerの3D modeに独立したExtrude設定を導入し、通常カードとの切替、keyframe再生成、D3D12/Vulkanのmesh描画を確認する。

## 2026-08-30 - Text Animator の selector combine は Layer state で明示する

- 関連: `ArtifactCore/include/Text/TextAnimator.ixx`、`Artifact/src/Layer/ArtifactTextLayer.cppm`、`docs/planned/MILESTONE_TEXT_ANIMATOR_SYSTEM_2026-03-25.md`
- 確認できた事実: Core の `AnimatorSelectorSet` と `evaluateAnimatorWeights()` は Multiply/Add/Subtract/Min/Max を実装済みだったが、Layer の `TextAnimatorState` は combine を持たず、評価時に常に Multiply を設定していた。
- 変更: animator state に `combine` を追加し、Property `text.animators.<n>.combine`、JSON root field、評価用 selector set に接続した。旧 JSON の未指定値は Multiply に復元する。
- 価値・懸念: 既存の Animator UI/Undo/JSON 経路をそのまま使い、selector 合成を実際のレイヤー評価へ届かせる。animator 内の複数 range/wiggly/expression selector を配列として編集する構造は別スライスである。
- 次に確認すべきこと: 2 selectorを持つ animator の UI を導入する前に、各combine値で Range + Expression selector のweightとJSON round-tripをtestで確認する。

## 2026-08-30 - Timelineの固定幅フォントはOS解決へ寄せる

- **関連:** `Artifact/src/Widgets/Timeline/TimelineScaleWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm`、`docs/planned/MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md`
- **確認できた事実:** タイムラインの ruler と timecode が Windows 固有の `Consolas` を直接指定していた。`DccStyleTheme` にはフォント token がなく、そこへ広げると Core の公開型とテーマ読み込みまで変更範囲が広がる。
- **変更:** Qt の `QFontDatabase::systemFont(QFontDatabase::FixedFont)` を使い、固定幅という表示契約を維持したままOSごとのフォント解決へ置換した。
- **価値または懸念:** Linux/macOSを含む環境で文字幅の不一致を避けられる。一方、テーマごとにフォントを選ぶ仕様は未導入であり、必要なら別途明示的なテーマ token 設計が必要。
- **次に確認すべきこと:** 各プラットフォームの ruler / timecode の桁幅、DPIスケーリング、フォント未提供時の代替表示をruntimeで確認する。

## 2026-08-30 - Property 面の focus context は子 editor への移動を境界にしない

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、`docs/planned/MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md`
- **確認できた事実:** `ArtifactPropertyWidget` は focus-out で `Panel.Properties` を `Global` に戻していたが、focus の移動先が同じ面の子 editor かどうかを判定していなかった。
- **変更:** 次の focus widget が Property 面自身またはその子孫なら context を維持し、面の外へ移った場合だけ `Global` に戻す条件を追加した。
- **価値または懸念:** 数値欄や検索欄へ移動しただけで shortcut の入力面が失われる可能性を減らせる。他の panel の同型 focus-out、Qt の focus event 順序、子 editor 内のショートカット優先順位は未検証である。
- **次に確認すべきこと:** Property 面内の tab 移動、別 panel への移動、Esc、G/R/S の入力先を runtime で確認する。

## 2026-08-30 - 無効な layer 選択は空の解決結果を通知する

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`docs/planned/MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md`
- **確認できた事実:** `selectLayer()` は現在 composition に存在しない ID を受けると selection manager をクリアする一方、解決済み layer がない場合に要求IDを `LayerSelectionChangedEvent` へ返していた。
- **変更:** `resolvedCurrent` がない場合の通知IDを常に空の `LayerID` とし、selectionの実状態とイベントpayloadを一致させた。
- **追加確認:** 同じ `LayerID` が別 composition に残る場合も、現在 composition に実在することを確認してから早期再選択するようにした。
- **価値または懸念:** Inspectorやselection bridgeが無効な対象を保持し続ける可能性を減らせる。既存のイベント購読者が invalid reason と空IDをどう扱うか、runtimeでのcomposition切替中の競合は未検証である。
- **次に確認すべきこと:** 存在しないID、別compositionのID、composition切替直後の選択を試し、InspectorがNoLayerになり selection manager / event payload が一致することを確認する。

## 2026-08-30 - Status API は一時メッセージではなく常設値を保持できる

- **関連:** `Artifact/src/Widgets/ArtifactMainWindow.cppm`、`docs/planned/MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md`
- **確認できた事実:** `setStatusZoomLevel()`、座標、メモリ、FPS は `QStatusBar::showMessage()` を使っていたため、別の状態更新で表示が上書きされ、常時参照できなかった。
- **変更:** 既存の setter API とイベント経路を変えず、各値を `QStatusBar::addPermanentWidget()` の専用 `QLabel` に保持する lazy initializer を追加した。有限値でない zoom / FPS は安全な表示値へ正規化する。初期化前の setter 呼び出しや status bar 差し替え後も値を再表示できるよう、最新値と有効フラグを `Impl` に保持する。
- **価値または懸念:** 操作中に複数の診断値を同時に読める土台ができる。現在のコードでは zoom／FPS等の供給元が限定的で、playhead・selection count・ruler unitの同期は未実装である。
- **次に確認すべきこと:** status setterの呼び出し元を既存の controller／widget APIだけで接続できるか確認し、画面幅・focus mode・status bar差し替え時の表示をruntimeで受入する。

`ArtifactStatusBar` が既に専用の permanent label を持つことも確認したため、MainWindow の fallback setter が同じ status bar 上へ重複ラベルを作らないよう、既存 label に安定した object name を付け、同名 label を再利用する境界を追加した。

## 2026-08-30 - 保存確認経路は失敗理由を表示してから操作を止める

- **関連:** `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`、`docs/planned/MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md`
- **確認できた事実:** 未保存変更の保存確認で `saveToFile()` が失敗すると `false` を返して操作を中止していたが、`ArtifactProjectExporterResult::errorMessage` は表示されていなかった。
- **変更:** 保存成功時は従来どおり続行し、失敗時は exporter のエラー理由または復旧案内を警告表示してから `false` を返すようにした。
- **価値または懸念:** 新規作成・切替・終了・再起動が保存失敗を黙ってキャンセルしたように見える状態を減らせる。実際の exporter エラー文面とダイアログの runtime 可読性は未検証である。
- **次に確認すべきこと:** 書き込み不可パス、空パス、validation failureで、元プロジェクトが閉じず、エラー理由と再試行手段が表示されることを確認する。

## 2026-08-30 - ScrubBar の ruler 入力を WorkArea と同じ有限・非負契約へ揃える

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`、`docs/planned/MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md`
- **確認できた事実:** `ArtifactTimelineScrubBar::setRulerPixelsPerFrame()` と `setRulerHorizontalOffset()` は、WorkArea 側と違って負数を受け入れ、非有限値では比較結果により不正値を保持し得た。
- **変更:** 両 setter で有限値を確認し、0 未満と非有限値を 0 に正規化してから更新するようにした。
- **価値または懸念:** navigator／zoom 同期や状態復元から不正な ruler mapping が侵入する可能性を減らせる。非有限値を受けた場合に 0（ruler無効／offset初期値）へ戻す仕様の runtime確認は未実施である。
- **次に確認すべきこと:** ズームの最小値、ruler無効化、offset復元で handle・seek・cache range の位置が一致することを確認する。

`ArtifactStatusBar` の Zoom／FPS setter も同じ診断値境界として有限値・範囲正規化を揃え、fallback の MainWindow status setter と custom status bar の入力契約を一致させた。

## 2026-08-30 - MainWindow closeEvent も未保存確認の正本経路へ入れる

- **関連:** `Artifact/src/Widgets/ArtifactMainWindow.cppm`、`Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`、`docs/planned/MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md`
- **確認できた事実:** File Menu の終了・再起動・プロジェクト切替には未保存確認があったが、MainWindow の closeEvent は一般的な終了確認だけで、ウィンドウ close 操作から未保存変更が失われ得た。メニュー経路が `QApplication::quit()` を呼ぶため、closeEvent に同じ確認を単純追加すると二重確認になる。
- **変更:** closeEvent に save／discard／cancel の確認と保存失敗理由表示を追加した。File Menu の終了・再起動は既存確認済み property を MainWindow に設定し、closeEvent の未保存確認だけを一度スキップする。
- **価値または懸念:** ウィンドウ close、メニュー終了、再起動の全入口で未保存変更の保護を揃えられる。property 名の経路一致、Qt の quit→closeEvent 順序、discard 後の終了挙動は runtime 未検証である。
- **次に確認すべきこと:** close button、File→Quit、File→Restart の各入口で dirty project を使い、Save／Discard／Cancel が一度だけ表示され、Cancel後にウィンドウが残ることを確認する。

## 2026-08-30 - 非同期プロジェクト読込は最新要求の世代だけ適用する

- **関連:** `Artifact/src/Project/ArtifactProjectManager.cppm`、`Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`、`docs/planned/MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md`
- **確認できた事実:** `loadFromFileAsync()` は完了順だけで現在プロジェクトを置換し、複数要求を識別していなかった。後発の別ファイルを開いた後に先発の遅い読込が完了すると、古いプロジェクトが適用され得た。
- **変更:** 新規 async/sync load、新規作成、close で進む `projectOperationGeneration_` を追加し、完了 callback と main-thread apply の双方で要求世代を照合する。古い成功・失敗結果は UI callback を実行せず破棄する。
- **価値または懸念:** プロジェクト切替・再読込の順序が完了順に反転する可能性を抑えられる。バックグラウンド importer 自体のキャンセル、進捗表示の stale 文言、同時保存との相互作用は未検証である。
- **次に確認すべきこと:** 大きいファイル→小さいファイルの順で連続 open、load中の新規作成／closeを行い、最後の要求だけが current project と UI に反映されることを確認する。

## 2026-08-30 - 非同期保存の完了結果も project 境界を越えて適用しない

- **関連:** `Artifact/src/Project/ArtifactProjectManager.cppm`、`Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`、`docs/planned/MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md`
- **確認できた事実:** `saveToFileAsync()` は保存開始時の `projectPtr` を worker へ渡していたが、完了時は現在の project の path を変更し dirty を false にしていた。保存中に load／create／close／別 save が起きると、古い保存結果が新しい状態へ混入し得た。
- **変更:** 保存要求と同期保存も `projectOperationGeneration_` を進め、完了 callback と success apply で世代および現在 `ArtifactProjectPtr` の一致を確認する。不一致の stale 結果は hook／callback／current state 更新を行わず破棄する。
- **価値または懸念:** project切替境界で path と dirty state が古い保存に巻き戻る可能性を抑えられる。同一 project の保存中編集を検出する revision／snapshot 契約、worker中の exporter thread safety、stale progress表示は未検証である。
- **次に確認すべきこと:** 大きな保存中の load／新規作成／別名保存を行い、最後に選択した project と path、dirty state、通知が古い完了で変化しないことを確認する。

## 2026-08-30 - Timeline timecode は生成だけでなく playhead 同期まで必要

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm`、`docs/planned/MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md`
- **確認できた事実:** `ArtifactTimeCodeWidget` は Timeline の左ヘッダーへ生成されていたが、`updateTimeCode()` の呼び出しがコードベースに存在せず、timecode と frame number は初期値から更新されなかった。
- **変更:** 既存の全 playhead 更新が通る `setCurrentFrameForAll()` に timecode widget の更新を追加した。表示用 frame は丸めたうえで `int` の範囲へ安全に収め、新しい signal/slot は追加していない。
- **追加変更:** `ArtifactTimeCodeWidget::setFps()` と現在 frame の保持を追加し、composition 切替時に既存 ScrubBar の正規化済み FPS を共有する。30 固定だった表示計算を実FPSへ切り替えた。
- **価値または懸念:** scrub、keyboard、playback、curve editor、keyframe jump から同じ表示同期点へ収束でき、24/25/29.97/60fps等で frame-to-timecode 変換が実データに近づく。非整数FPSは既存の整数 FPS 表示契約に丸められる。
- **次に確認すべきこと:** scrub の sub-frame 表示、整数 seek、playback、複数FPS、負値／長時間 frame で timecode と frame number が playhead と一致することを runtime で確認する。

## 2026-08-30 - VPフレームラベルも固定幅フォントをOS解決へ揃える

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`docs/planned/MILESTONE_VP_MOTIONPATH_OVERLAY_FIXES_2026-08-30.md`
- **確認できた事実:** モーションパスの選択／hoverフレームラベルと一部のVP HUDが `Consolas` を直接指定していた。
- **変更:** 共通の `fixedWidthFont()` helperから `QFontDatabase::systemFont(QFontDatabase::FixedFont)` を使い、フォントサイズと描画位置を維持した。
- **価値または懸念:** Windows固有フォントへの依存を減らせる。Diligentの描画経路やrenderer stateは変更していない。
- **次に確認すべきこと:** VP上でラベル幅、DPIスケーリング、light/dark themeの可読性をruntime確認する。

## 2026-08-30 - Motion Path キャッシュは単一スナップショットを壊さずレイヤー別に温存する

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`docs/planned/MILESTONE_VP_MOTIONPATH_OVERLAY_FIXES_2026-08-30.md`
- **確認できた事実:** 既存の Motion Path cache は layer / frame / overlay serial を持つ単一エントリで、複数レイヤーを同じ overlay pass で処理すると前のレイヤーの結果を上書きしていた。
- **変更:** コンポジション ID と layer ID の組み合わせごとの `QHash<QString, MotionPathCacheEntry>` を追加し、描画前に該当 layer の snapshot を復元する。適応サンプル密度に影響する zoom も cache 条件へ含めた。`invalidateMotionPathCache()` で単一・レイヤー別の両方を消去し、通常の overlay invalidation と高頻度ドラッグ中の stale データ防止を一致させた。
- **価値または懸念:** 複数選択・layer 切替時の `getGlobalTransformAt()` 再計算を同一 invalidation 世代内で抑制でき、描画ループ後の tangent hit-test も選択 layer の snapshot を参照できる。キャッシュ値のコピーコスト、カメラ経路、実際の frame-time 改善は未検証である。
- **次に確認すべきこと:** 複数選択で overlay pass が同一 frame に複数 layer を通ること、cache hit 時に表示と hit-test が一致すること、invalidating edit 後に再計算されることを runtime で確認する。

## 2026-08-30 - 実行されない旧 Motion Path ブロックは現行経路と分離して削除できる

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`docs/planned/MILESTONE_VP_MOTIONPATH_OVERLAY_FIXES_2026-08-30.md`
- **確認できた事実:** 描画関数内に旧 Motion Path 実装がコメントアウトされたまま残っていたが、現行の `renderMotionPathOverlayForLayer()` と `buildMotionPathSamples()` は別の実コード経路として使用されていた。
- **変更:** コメントアウトされた旧実装だけを削除し、描画呼び出し、ProfileScope、renderer API、キャッシュ契約は変更しなかった。
- **価値または懸念:** 二重実装の再有効化や古い Profile 名の誤参照を防ぎ、描画関数の責務を読みやすくできる。単一経路になったことのビルド・runtime確認は未実施である。
- **次に確認すべきこと:** ビルド後に MotionPath profile が現行経路からのみ出ること、VPの見た目に差分がないことを確認する。

## 2026-08-30 - タイムラインの frame grid は zoom に応じて 1/2/5 系列へ間引ける

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、`docs/planned/MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md`
- **確認できた事実:** 右ペインの縦 frame grid は major=10、minor=5 に固定されており、ズームアウトしても同じ密度で線を描いていた。
- **変更:** 既存の paintEvent 内で、major 間隔を 1/2/5 系列から選び、medium=major/2、minor=major/5 として zoom に応じて間引くようにした。線の grid だけを対象にし、ruler の単位や値の変換は変更していない。
- **価値または懸念:** 長い時間範囲を表示した際の視覚ノイズと不要な線描画を抑えられる。主要ラベルが別経路にある場合の見た目の整合、DPI、runtime性能は未検証である。
- **次に確認すべきこと:** ppf の最小・標準・最大付近で major／medium／minor の間隔が安定し、frame seek／selection overlay とずれないことを確認する。


## 2026-08-30 - Layer Effect トランジション追加 (Wipe / Slide / Dissolve / Zoom 13 個) を planned に起こした

- 関連: docs/planned/MILESTONE_LAYER_EFFECT_WIPE_SLIDE_DISSOLVE_ZOOM_2026-08-30.md (新規), Artifact/src/Effects/LinearWipe/ (雛形), docs/planned/MILESTONE_TIMELINE_STATUS_INDEX_2026-08-30.md
- 事実: Artifact/src/Effects/ 配下には 70+ のレイヤー effect があるも、Transitions ディレクトリ自体が存在せず、LinearWipe 1 個だけが独立ディレクトリで GPU effect として露出している。ArtifactCore/include/Video/Transitions/ には NLE 用動画トランジション 16 種が揃うも、Artifact/ からの参照は 0 件で UI 露出なし。本マイルストーンは Wipe 5 個 (RadialWipe / IrisWipe / GradientWipe / ClockWipe / BlockDissolve) と Slide/Dissolve/Zoom 8 個 (Slide / Push / SlidingDoors / CrossDissolve / DipToBlack / DipToWhite / Zoom / ZoomBoxes)、計 13 effect を LinearWipe 雛形 + 雛形からの差分のみで追加し、AE の Transitions カテゴリ 4 系統 18 個中 14 個 (Linear 既存 + 13 追加) を揃える計画。AGENTS.md「D3D12 / Diligent backend 触るときは慎重」「QImage の本流投入禁止」「QPainter::CompositionMode による合成実装禁止」「新規 signal/slot の追加禁止」を遵守し、ComputeMode::AUTO で CPU/GPU 両実装を持つパターンを雛形に固定。
- 気づき: ユーザーは「トランジションを追加できないか」と発話したが、ArtifactCore 側 16 種 (NLE 用) は実装済みで未露出、LinearWipe は 1 個だけレイヤー effect として露出、Layer Styles 系は多く実装されているが Wipe/Slide/Dissolve/Zoom 系の露出は LinearWipe 1 個のみという状態。案の提示で A 段階 (Wipe 5 個) のみ着手する選択肢と A+B 両方を 1 milestone にまとめる選択肢があったが、ユーザは A+B を選択。LinearWipe 雛形が完全パターンとして確立されているおかげで 13 個の追加コストが「雛形 copy + shader 差分 + property 拡張」で済み、C 段階 (Layer Styles 5 個 + 3D 4 個) より低リスクで進められる構成。
- 価値・懸念: 13 effect 追加で Inspector / Undo / JSON 連携の検証範囲が広がる。
enderOneFrame() 経路の重さが微増するため MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md Phase 3 (renderOneFrame 統合) との順序関係が要。B 段階の slide/push 系は単一 image mask ではなく 2 layer 合成の表現が必要で、ArtifactAbstractEffect::applyCPU(src, dst) の規約に合わせるため dst の alpha を 0 にして背景を露出させる形で表現する方針を Design Principles 7 に明記。
- 次に確認: Phase 1 (RadialWipe) の雛形 copy 着手可否をユーザーに確認。AGENTS.md に従いビルド・runtime 受入れはユーザー指示待ち。着手するなら LinearWipe/ → RadialWipe/ の cppm/ixx コピー + class rename + applyCPU の中心座標+円弧判定差分 + HLSL shader 差分 + Artifact/CMakeLists.txt への GLOB/force list 追加が最小手順。

## 2026-08-30 - Timeline playhead の非有限値を共有 setter 境界で止める

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimelineNavigatorWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactWorkAreaControlWidget.cppm`
- **確認できた事実:** `std::clamp` は `NaN` を自動的に除外しないため、Timeline の共有 playhead setter、ScrubBar の visual frame、Navigator、WorkArea に非有限値が入ると `NaN` を保持し得た。共有 setter ではさらに、整数 frame へ変換する前に値を収める処理が必要だった。
- **変更:** 非有限値を 0 に戻し、共有 `setCurrentFrameForAll()` では非負かつ `int` 上限内へ正規化した。Navigator には `totalFrames - 1` の上限と total-frame 短縮時の再クランプを追加し、ScrubBar／WorkArea／右ペインと範囲外表示の契約を揃えた。WorkArea の total frames／FPS／ruler mapping も有限・非負へ正規化した。
- **価値または懸念:** 異常な再生／同期入力が timecode、ScrubBar、Navigator、WorkArea の状態へ伝播することと、整数変換前の未定義動作を防げる。実際の異常入力発生源と runtime 表示確認は未検証である。
- **次に確認すべきこと:** 通常の seek、sub-frame scrub、playback、composition 切替で表示が変わらないこと、および異常入力を注入した場合に playhead が 0 へ復帰することを runtime で確認する。

## 2026-08-30 - 既存 selection event から status bar の選択数を更新する

- **関連:** `Artifact/src/AppMain.cppm`、`Artifact/include/Widgets/ArtifactStatusBar.ixx`、`Artifact/src/Widgets/ArtifactStatusBar.cppm`、`docs/planned/MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md`
- **確認できた事実:** custom `ArtifactStatusBar` には Zoom／Frame／FPS／Layer などの常設項目があったが、選択数の表示項目はなく、Timeline 内部の selection summary だけが選択状態を表示していた。既存の `LayerSelectionChangedEvent` 購読では selection manager を参照できる。
- **変更:** status bar に `Selection` item と `setSelectionCount()` を追加し、既存 selection event と composition-change event の処理時に選択集合の件数を更新する。composition が無くなった場合は 0 へ戻す。新しい signal／slot やグローバルイベント経路は追加していない。
- **価値または懸念:** Timeline focus 外でも選択数を確認でき、選択変更後の状態フィードバックが増える。status bar の横幅、初期化直後、composition切替時の runtime 表示は未検証である。
- **次に確認すべきこと:** 単一／複数／全解除／別 composition 切替で `SEL` が正しい件数を示し、既存の status item 表示切替と狭幅レイアウトを壊さないことを確認する。

## 2026-08-30 - composition FPS の UI 変換前に有限値を確認する

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm`
- **確認できた事実:** composition 切替時に `frameRate().framerate()` を直接 `int` へキャストして ScrubBar の FPS へ渡していた。異常な非有限値が入ると、整数変換前に入力契約を検査できない状態だった。
- **変更:** 有限・正値を確認し、1〜10000 FPS に収めてから丸めるようにした。無効値は既存 UI の 30 FPS 契約へ戻す。
- **追加変更:** TrackPainterView の keyframe 編集で散在していた FPS scale 変換も同じ helper へ統一した。
- **価値または懸念:** timecode の除算と ScrubBar の tick 計算へ異常 FPS が伝播することを防げる。非整数 FPS を整数 UI 契約へ丸める制約と、実設定の保存／再読込は runtime 未検証である。
- **次に確認すべきこと:** 24／25／29.97／60 FPS と無効値の composition 切替で、ScrubBar と timecode の表示・seek が一致することを確認する。

## 2026-08-30 - Noise Layer の Workspace Automation を Python bridge へ露出する

- **関連:** `Artifact/src/Script/ArtifactPythonHookManager.cppm`、`Artifact/include/AI/WorkspaceAutomation.ixx`、`docs/planned/MILESTONE_NOISE_LAYER_2026-08-24.md`
- **確認できた事実:** Workspace Automation には `createNoiseLayer`／`addNoiseLayer` と kind／preset の引数契約が存在したが、`artifact.workspace` の Python 登録一覧には noise layer 用 bridge がなかった。
- **変更:** 既存の `registerWorkspaceMethod()` を使い、両 Python 名を同じ callback に登録した。composition ID、名前、サイズ、seed、kind を既存 Automation へ渡し、結果は既存 API と同じ compact JSON で返す。
- **価値または懸念:** Python hook／自動化から noise layer 作成を利用でき、UI／Automation／Python の作成導線が同じ責務へ収束する。Python engine の実行、引数省略、作成後の保存・再読込は未検証である。
- **次に確認すべきこと:** `artifact.workspace.createNoiseLayer()` と `addNoiseLayer()` を current／明示 composition、各 kind、サイズ省略で呼び、成功 JSON と project round-trip を確認する。

## 2026-08-30 - Noise Layer の Automation 引数を実設定へ反映する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/src/Layer/ArtifactLayerFactory.cppm`、`docs/planned/MILESTONE_NOISE_LAYER_2026-08-24.md`
- **確認できた事実:** `WorkspaceAutomation::createNoiseLayer()` は `kind`／preset を `ArtifactNoiseLayerInitParams` へ設定していたが、手動生成した `ArtifactNoiseLayer` には seed しかコピーしていなかった。`ArtifactLayerFactory` には preset 生成と kind 設定を行う正規パターンが既にあった。
- **変更:** Automation 側の手動生成でも、preset 時は `ProceduralTextureGenerator::makePreset()` を使い、通常 kind 時は `settings.primary.kind` を設定してから Noise Layer へ渡すようにした。
- **価値または懸念:** Python／Workspace Automation から指定した Perlin 以外の kind と procedural preset が、実際の生成結果と保存設定へ反映される。各 kind の画素差、JSON round-trip、GPU／CPU parity は runtime 未検証である。
- **次に確認すべきこと:** kind／preset ごとに生成後の `settings.primary` と JSON の値を確認し、保存・再読込後も指定した noise 表現が維持されることを確認する。

## 2026-08-30 - workspace diagnostics の Python read-only bridge を補完する

- **関連:** `Artifact/src/Script/ArtifactPythonHookManager.cppm`、`Artifact/include/AI/WorkspaceAutomation.ixx`、`docs/planned/MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md`
- **確認できた事実:** `WorkspaceAutomation` には `workspaceDiagnostics()` と `agentPreflight()` があり、AI UI は C++ 側の診断を利用していたが、`artifact.workspace` の Python 登録は `agentPreflight()` のみで、診断単体の read-only 入口が欠けていた。
- **変更:** 既存の `registerWorkspaceMethod()` パターンで `workspaceDiagnostics` を Python module に登録し、compact JSON を返すようにした。状態変更、signal／slot、イベント経路の追加は行っていない。
- **価値または懸念:** Python hook／外部自動化が、操作後に composition state mismatch、selection、input context を単体観測できる。Python engine の実行、実際の不一致検知、長時間 hook 運用は未検証である。
- **次に確認すべきこと:** `artifact.workspace.workspaceDiagnostics()` の返却 JSON が `operationState` と `warningCodes` を含み、agentPreflight の診断と同じ snapshot を返すことを確認する。

## 2026-08-30 - Python workspace の read-only discovery surface を Automation と揃える

- **関連:** `Artifact/src/Script/ArtifactPythonHookManager.cppm`、`Artifact/include/AI/WorkspaceAutomation.ixx`、`docs/planned/MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md`
- **確認できた事実:** `WorkspaceAutomation::methodDescriptions()` と dispatch には agent contract、command vocabulary、selection／render queue snapshot、`get_*` の read-only alias が存在したが、Python bridge は一部の snapshot と write wrapper に限定されていた。
- **変更:** `agentContract`、`commandVocabulary`、`selectionSnapshot`、`get_selected_layers`、`renderQueueSnapshot`、`get_render_queue_summary`、`get_project_overview`、`get_active_composition` を既存の JSON bridge へ登録した。既存の状態変更責務や signal／slot は変更していない。
- **価値または懸念:** Python hook／外部自動化が、実行前の契約確認と実行後の selection／queue／composition 観測を同じ Automation surface で行える。Python engine の実行、API互換性、返却 JSON の実機確認は未検証である。
- **次に確認すべきこと:** 各 `artifact.workspace` method の返却値が C++ `invokeMethod()` と一致し、`agentContract` の discovery／safe execution order を Python から取得できることを確認する。

## 2026-08-30 - Python Workspace bridge の read-only／validation 欠落を機械比較で補完する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/src/Script/ArtifactPythonHookManager.cppm`、`docs/planned/MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md`
- **確認できた事実:** `methodDescriptions()` には 220 件の Automation method がある一方、監査開始時の Python bridge 登録は 47 件だった。既存のC++ dispatchに存在する read-only／validation methodでも、Pythonから直接観測できないものが残っていた。
- **変更:** 既存の `registerWorkspaceMethod()` と compact JSON変換を再利用し、viewport設定、project item／layer／effect／audio情報、render queue job照会、export形式、effect preset、playback read-only state、remove の dry-run／confirmation message、`validateCommand`／`validateViewportSettings` を追加登録した。比較後、今回選んだ候補の未登録は0件になり、Python登録は97件になった。
- **価値または懸念:** Python hook／外部自動化が、書き込み前のvalidationと書き込み後の状態照会をC++／AI UIと同じ責務で実行できる。Python engine実行、引数JSONの不正入力、各返却値の実機互換性は未検証である。
- **次に確認すべきこと:** Pythonから各read-only methodとvalidation methodを呼び、C++ direct invokeと同じJSON schema／error codeが返ることを確認する。

## 2026-08-30 - Timeline hover は右ペインの入力・表示経路まで既に接続済み

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、`Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`、`docs/planned/MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md`
- **確認できた事実:** 右ペインには `setMouseTracking(true)` があり、`mouseMoveEvent()` が clip／marker／keyframe area を hit-test して hover index、cursor、tooltip、dirty repaint を更新している。ドラッグ中の preview tooltip も既存経路にある。
- **判断／変更:** hover tracking 自体を新規実装する必要はないためコード変更は行わず、マイルストーンを実装済み範囲と未検証範囲へ更新した。新しい signal／slot、QtCSS、別の hover event 経路は追加していない。
- **価値または懸念:** 同じ課題を重複実装せず、残る空白領域 read-out、専用 edge visual、theme／DPI の runtime 受入へ焦点を絞れる。QToolTip の表示遅延や painter overlay との仕様差は未検証である。
- **次に確認すべきこと:** runtime で marker／area／clip edge／空白領域を順に hover し、tooltip、cursor、局所再描画、Alt／slide 操作表示の期待値を確認する。

## 2026-08-30 - Timeline 内部の keyframe 時間スケールも有限 FPS 境界へ揃える

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`docs/planned/MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md`
- **確認できた事実:** Timeline の表示側だけでなく、keyframe snapshot 復元、layer slide、選択 keyframe の検索、curve editor の write-back、pattern／preset 生成などにも `frameRate().framerate()` の直接 `llround` が散在していた。`std::max(1.0, NaN)` は NaN を除外しない。
- **変更:** `safeTimelineFrameRate()` と `timelineFrameRateScale()` を追加し、有限・正値・1〜10000 FPS の共通契約へ置換した。pattern の BPM は無効 FPS 時に従来の 120 BPM を維持するため 60 FPS fallback を使う。
- **価値または懸念:** 不正な composition FPS が `RationalTime` の time scale や整数変換へ流れる経路を減らせる。実際の異常 FPS の生成・保存・再読込と runtime の keyframe 操作は未検証である。
- **次に確認すべきこと:** 通常の 24／29.97／60 FPS と無効値で、keyframe 復元、slide、curve editor、pattern／preset、再生表示の時間位置が崩れないことを runtime で確認する。

## 2026-08-30 - Property／pre-compose の FPS scale 変換も異常値を遮断する

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`
- **確認できた事実:** expression の keyframe bake、共有 playback time、pre-compose の child work area 生成にも FPS の直接 `lround`／`llround` が残っていた。Property Editor 側には 30 FPS fallback が既にあり、pre-compose 側は最小 scale 1 を保証していた。
- **変更:** 有限・正値・1〜10000 FPS を確認してから整数 scale へ変換し、無効値は各既存契約（Property／playback は 30、pre-compose は 1）を維持するようにした。必要な `<cmath>` も使用ファイルへ直接追加した。
- **価値または懸念:** 異常な composition／playback FPS が expression bake、再生時刻、pre-compose work area の `RationalTime` に入り込む経路を減らせる。保存された不正 FPS の由来と runtime の bake／pre-compose は未検証である。
- **次に確認すべきこと:** expression bake、再生、pre-compose を通常 FPS と無効 FPS の composition で確認し、既存の時間位置と child work area が変わらないことを確認する。

## 2026-08-30 - Layer Panel／Playback Control の timecode FPS 入力を有限化する

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`、`Artifact/src/Widgets/Control/ArtifactPlaybackControlWidget.cppm`、`Artifact/src/Widgets/Control/ArtifactPlaybackControlTestWidget.cppm`
- **確認できた事実:** Layer Panel の frame event と keyframe marker 時刻、Playback Control の timecode parser／range表示が playback service の FPS を `std::max` または無検証で利用していた。NaN は `std::max` だけでは除外できない。
- **変更:** 各 UI ファイルに有限・正値・1〜10000 FPS のローカル helper を追加し、無効値は 30 FPS へ戻してから `RationalTime` と frame boundary に渡すようにした。
- **価値または懸念:** 再生サービスの異常 FPS が Layer Panel の現在時刻、timecode 入力、範囲表示へ伝播することを防げる。異常値を実際に注入した runtime の表示・編集確認は未検証である。
- **次に確認すべきこと:** 通常再生、timecode 入力、composition 切替、診断用 Playback Control で 24／29.97／60 FPS と無効値を確認する。

## 2026-08-30 - Preview timer と Text／Subtitle の FPS 変換を有限化する

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- **確認できた事実:** preview timer の `1000 / fps`、Text keyframe の `RationalTime`、SRT／WebVTT の integer timebase が composition FPS を直接使用していた。NaN は単純な `fps <= 0` 判定を通過し得る。
- **変更:** preview timer は有限・正値・1〜10000 FPS を確認してから interval を計算し、Text／Subtitle は同じ範囲で安全な double／integer scale へ変換する。無効値時の既存 16ms／30 FPS fallback を維持した。
- **価値または懸念:** 異常 FPS が preview timer の整数変換や字幕の時間基準へ伝播することを防げる。実ファイルの字幕 import／export、preview 再生、invalid FPS の runtime 受入は未検証である。
- **次に確認すべきこと:** Text keyframe 編集と SRT／WebVTT import／export を 24／29.97／60 FPS と無効値で確認し、字幕位置と preview の応答性が変わらないことを確認する。

## 2026-08-30 - Transform Gizmo の keyframe rate に有限値上限を追加する

- **関連:** `Artifact/src/Widgets/Render/TransformGizmo.cppm`、`docs/planned/MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md`
- **確認できた事実:** Transform Gizmo は FPS が正値ならそのまま transform keyframe の time scale に使っていたため、無限大が `RationalTime` へ入る余地があった。NaN は fallback へ落ちるが、上限はなかった。
- **変更:** 有限・正値を確認し、1〜10000 FPS に収めてから keyframe rate として返すようにした。Diligent renderer／GPU path は変更していない。
- **価値または懸念:** 異常な transform FPS が keyframe 時刻の time scale を壊す経路を塞げる。invalid FPS の runtime 注入と transform 編集の受入は未検証である。
- **次に確認すべきこと:** Viewport transform の keyframe 作成・移動を通常 FPS と異常 FPS で確認し、frame position と保存結果が安定することを確認する。

## 2026-08-30 - レイヤー評価と Layer Menu の FPS scale を有限化する

- **関連:** `Artifact/src/Layer/ArtifactSolidImageLayer.cppm`、`Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/src/Layer/ArtifactAudioLayer.cppm`、`Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`
- **確認できた事実:** Solid／Shape／Audio の animatable property 評価と Layer Menu の放射状 transform が composition FPS を直接 `llround` して `RationalTime` の scale にしていた。これらは通常レイヤー操作の局所経路だが、NaN／無限大が整数変換へ到達し得た。
- **変更:** 有限・正値・1〜10000 FPS を確認してから scale 化し、無効値は既存の 30 FPS fallback へ戻すようにした。
- **価値または懸念:** 静止画・シェイプ・音声の評価時刻とレイヤー一括変形で、異常 FPS による time scale 破壊を減らせる。各レイヤーの保存／再読込と invalid FPS の runtime 受入は未検証である。
- **次に確認すべきこと:** 各レイヤーの animated property、current-frame preview、放射状 transform を通常 FPS と異常 FPS で確認し、keyframe の位置と undo が維持されることを確認する。

## 2026-08-30 - Timeline 監査の J/K と Easy Ease 判定を現行ショートカットへ合わせる

- **関連:** `docs/planned/MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md`、`Artifact/include/Widgets/Timeline/ArtifactTimelineKeyBinding.ixx`、`ArtifactCore/src/UI/ShortcutBindings.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- **確認できた事実:** 旧監査は J/K キーフレームジャンプを未実装、Easy Ease の速度ベース計算を未実装としていたが、現行コードには Previous／Next action、Timeline／Animation の context-safe shortcut、F9 系 shortcut、`tryComputeEasyEaseHandles()` がある。裸の J/K は Contents Viewer の shuttle／pause と競合する。
- **判断／変更:** 既存実装を追加せず、監査表を「実装済みだが runtime 未確認」へ更新した。裸の J/K を新規上書きせず、既存の汎用変換・再生ショートカットとの整合を維持した。
- **価値または懸念:** 未実装と誤認して重複 shortcut や競合する単一キーを追加することを防げる。modifier shortcut の実際の focus routing、F9 の各値型、Easy Ease の保存／再読込は未検証である。
- **次に確認すべきこと:** Timeline／Animation／Contents Viewer の各 focus context で既定 shortcut を確認し、J/K の再生動作と keyframe navigation が相互に奪い合わないことを確認する。

## 2026-08-30 - Transform Undo の固定 time scale を composition FPS へ戻す

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`ArtifactCore/src/Animation/AnimatableTransform3D.cppm`
- **確認できた事実:** `MoveLayerCommand`、Align の復元、Viewport の Center in Comp が `RationalTime(frame, 30000)` を使っていた。`AnimatableTransform3D::setPosition()` は受け取った時刻を `toFrameCount(24)` で内部 keyframe 位置へ変換するため、30000 は composition の通常 frame scale と一致しない。
- **変更:** 対象 composition の FPS を有限・正値・1〜10000 へ収めて time scale にし、Move／Align の Undo/Redo では Transform dirty flag／reason、既存 `changed`、既存 `LayerChangedEvent` を通知するようにした。Center in Comp も同じ scale と dirty flag を使う。
- **価値または懸念:** 24／30／60 FPS 等で Undo/Redo や Center in Comp が別フレームへ keyframe を書く不整合を減らせる。複数選択、current frame 変更中の Undo、保存／再読込、runtime 表示更新は未検証である。
- **次に確認すべきこと:** 24／29.97／60 FPS の composition で transform keyframe を作成し、drag／arrow nudge／Center／Align を Undo・Redo して、同じ frame の値と dirty／preview 更新が保たれることを確認する。

## 2026-08-30 - SetLayerPropertyValueCommand が virtual setter を迂回していた

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Layer/ArtifactParticleLayer.cppm`
- **確認できた事実:** `SetLayerPropertyValueCommand` は `getProperty()->setValue()` を直接呼んでいた。一方、Particle の `particle.emitter.*`／`particle.effectors.*` は `ArtifactParticleLayer::setLayerPropertyValue()` で実データ、frame cache、専用通知を更新する設計になっている。
- **変更:** Undo／Redo は virtual `setLayerPropertyValue()` を先に呼び、未対応の汎用propertyだけ旧直接更新へフォールバックする共通 helperへ変更した。
- **価値または懸念:** Particle viewport dragとProperty resetのUndo/Redoで、表示用property cacheだけが戻り実データが残る不整合を減らせる。各Particle path、通常Property reset、保存／再読込はruntime未検証である。
- **次に確認すべきこと:** emitter位置・方向、effector位置・radiusを変更し、Undo／Redo後に実システム、Inspector、保存JSONが同じ値になることを確認する。

## 2026-08-30 - Keyframe Undo command の layer ID 初期化漏れを修正する

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`Artifact/include/Undo/UndoManager.ixx`
- **確認できた事実:** `SetLayerPropertyKeyframesCommand` は `commandType()`、`serialize()`、`deserialize()` を持つ一方、通常コンストラクタで `layerId_` を設定していなかった。そのため新規commandの `canSerialize()` が layer ID 空で失敗する。
- **変更:** コンストラクタで対象 layer IDを保存し、serialize判定に weak pointer の有効性も加えた。
- **価値または懸念:** keyframe編集commandがoffload／永続化対象から意図せず除外される問題を減らせる。JSON round-trip、offload閾値、Undo stack再起動復元は未検証である。
- **次に確認すべきこと:** keyframe編集後にserialize payloadへ layer IDが入り、deserialize後の対象layer解決とUndo／Redoが成立することを確認する。

## 2026-08-30 - Keyframe Undo の dirty／cache 通知を property path と揃える

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- **確認できた事実:** keyframe snapshot復元と汎用property fallbackは property cache を変更した後、`changed()` のみを呼んでいた。通常の layer mutation 経路が行う dirty flag、dirty reason、`LayerChangedEvent` の通知が欠けていた。
- **変更:** `transform.*`／`mask.*`／`source.*`／その他のproperty pathに応じて dirty flag を選ぶ共通通知を追加し、keyframe復元と直接property fallbackから利用した。
- **価値または懸念:** Undo/Redo後に preview cache、保存対象、依存UIが古いまま残る可能性を減らせる。全property種別のdirty flag選択とruntime cache更新は未検証である。
- **次に確認すべきこと:** transform／mask／source／effect系keyframeをUndo／Redoし、dirty state、preview再描画、保存JSON、LayerChangedEventの対象が一致することを確認する。

## 2026-08-30 - Undo の保存済み判定を履歴位置の state token で管理する

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`docs/planned/MILESTONE_OPERATION_RELIABILITY_DCC_2026-08-30.md`
- **確認できた事実:** `version_` は push 時だけ増加し、Undo／Redoでは更新されていなかった。そのため保存後に Undo してから Redo した場合や、Undo 後に別の編集を行った場合、保存済み位置と現在位置の比較が履歴状態を表さない可能性があった。
- **変更:** Undo／Redo stack と並行する state ID を導入し、Undo は移動元 IDを Redo 側へ渡し、Redo は同じ IDを Undo 側へ戻すようにした。Undo 後の新規 push は Redo を破棄して新しい IDを割り当てる。履歴保存 JSONには現在位置を `currentVersion` として保持し、旧形式では `savedVersion` を fallback にする。
- **価値または懸念:** `hasUnsavedChanges()` が「操作回数」ではなく履歴位置の同一性を判定でき、保存後のUndo／Redoと分岐編集の dirty 表示を安定させられる。履歴上限で古い command が破棄された場合や state ID の長期運用、runtime の保存確認は未検証である。
- **次に確認すべきこと:** 編集→保存→Undo→Redo、編集→保存→Undo→別編集、履歴上限による古い command 破棄、saveSessionHistory/loadSessionHistory の各経路で `hasUnsavedChanges()` と保存確認が期待どおりになることを確認する。

## 2026-08-30 - Layer property value Undo を session history の対象にする

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- **確認できた事実:** `SetLayerPropertyValueCommand` は Property Editor の reset と Particle の viewport drag で使われていたが、layer IDを持たず、`commandType()`、`canSerialize()`、serialize／deserialize、factory登録もなかった。通常の Undo stack では動くが、macroの永続化・offload・session historyの再構成には参加できない状態だった。
- **変更:** layer IDをconstructorで保持し、JSON表現可能な QVariantだけを serialize可能と判定するAPI、JSON round-trip、UndoManager factory登録を追加した。before／after値は既存の virtual `setLayerPropertyValue()` を通るため、Particleの実データ更新経路は維持している。
- **価値または懸念:** layer property編集の履歴保持範囲を keyframe編集や他の layer command と揃えられる。QtがJSON化できない型、Particle専用型の保存表現、実際のoffload／session reloadは未検証である。
- **次に確認すべきこと:** 数値・bool・文字列の layer property、Particle emitter／effector property、macro childについて、serialize→deserialize後の対象 layer解決と Undo／Redo結果が元の値・実データ・dirty stateに一致することを確認する。

## 2026-08-30 - SetTextAnimatorStack の Undo factory 登録漏れを補完する

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- **確認できた事実:** `commandType()` を持つ全 Undo command と `commandFactories_` の登録名を静的比較した結果、`SetTextAnimatorStackCommand` だけが factory未登録だった。serialize／deserialize本体は存在するため、保存データは作れても session load 時の `createCommand()` が失敗する経路があった。
- **変更:** 既存 constructorの `beforeStack`／`afterStack`／label を空値で初期化する factoryを追加した。JSON deserializeが layer IDを解決する既存責務は変更していない。
- **価値または懸念:** Text Animator stackのsession history再構成が他のserializable commandと同じ登録経路へ入る。stack JSONの内容検証、text layerの復元、実際のsave／loadは未検証である。
- **次に確認すべきこと:** Text Animator編集後のmacro／offload／session historyで command typeがfactoryへ到達し、対象text layerのstackがUndo／Redoで before／afterへ戻ることを確認する。

## 2026-08-30 - 通常 Property Editor の複数選択値を Undo snapshot 化する

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`、`Artifact/include/Undo/UndoManager.ixx`
- **確認できた事実:** 通常の Property Editor 行と Channel Box は preview 中に `propertyPtr` と選択 target を直接変更し、commit 時も setter を直接呼んでいたため、複数選択を含む通常編集に before 値を持つ Undo command がなかった。
- **変更:** 初回 preview／commit 前に各 target layer の property 値を path 別に snapshot し、commit 時は既存 `SetLayerPropertyValueCommand` を `MacroUndoCommand` にまとめて記録する callback 経路を追加した。preview の追従と virtual setter は維持している。
- **価値または懸念:** 通常の layer property 編集を、選択レイヤー単位の before／after と一操作単位の Undo 境界へ揃えられる。keyframe mode／auto-key、キャンセル後の長時間 stale snapshot、runtime 表示更新は未検証である。
- **次に確認すべきこと:** 単一・複数選択の数値／bool／文字列を編集し、preview→commit、slider／scrub、Undo／Redo、キャンセル後の再編集で各 layer の値と dirty 表示が一致することを確認する。

## 2026-08-30 - Property keyframe action の直接変更を snapshot Undo 化する

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- **確認できた事実:** Property 行の全 keyframe Anchor／Color Label変更と Channel Box の Key All／Key Selected は、keyframe を直接変更していたが Undo command を作っていなかった。
- **変更:** Anchor／Color Label は before／after keyframe sequence を `SetLayerPropertyKeyframesCommand` へ渡し、Channel Box の一括キー操作は選択 target ごとの child command を `MacroUndoCommand` にまとめた。
- **価値または懸念:** keyframe metadata と追加操作を通常の Undo 履歴へ入れられる。`setAnimatable(true)` の flag 復元、expression bake、auto-key／keyframe mode の複合編集、runtime の selection／preview 更新は未検証である。
- **次に確認すべきこと:** 既存 keyframe の anchor／label変更、Key All／Key Selected を Undo／Redoし、keyframe本体・animatable状態・timeline表示が一致することを確認する。

## 2026-08-30 - Expression 操作の Undo 状態を分離して保存する

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- **確認できた事実:** active expression の Clear／Convert／Bake は expression または keyframe を直接更新しており、通常の Undo 履歴へ状態を登録していなかった。
- **変更:** expression before／after を持つ `SetLayerPropertyExpressionCommand` と factory／JSON codec を追加し、Clear／Convert／Bake を既存 keyframe command と組み合わせて Undo 可能にした。
- **価値または懸念:** expression と sampled keyframe の復元単位を明示できる。sampled value の非対応型、animatable flag、複数選択、runtime／session reload は未検証である。
- **次に確認すべきこと:** expression Clear、Convert、Bake をそれぞれ Undo／Redoし、expression、keyframe、current frame、dirty state、timeline表示が一致することを確認する。

## 2026-08-30 - Expression Copilot の確定処理を Undo 経路へ戻す

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- **確認できた事実:** Expression Copilot は apply callback の前に `propertyPtr->setExpression()` を直接実行していたため、Inspector の行メニュー経路だけでなく Copilot 経由の式変更も履歴から外れていた。
- **変更:** Copilot 共通 launcher の直接更新を除去し、layer-owned の通常行・Property Widget の callback 側で before／after expression を取得して `SetLayerPropertyExpressionCommand` を push するようにした。同値入力は no-op とした。Effect-owned property は layer command の対象外なので既存の直接更新を維持した。
- **価値または懸念:** Layer expression の確定を Clear／Convert／Bake と同じ Undo／Redo・dirty 通知経路へ揃えられる。Effect expression は従来どおり専用履歴がなく、Copilot の実画面操作、式評価エラー時の表示、session reload は未検証である。
- **次に確認すべきこと:** 通常行と参照ドロップの両方で式を適用し、applyを連続実行した場合の履歴数、Undo／Redo、式評価結果、再読込後の復元を確認する。

## 2026-08-30 - Keyframe snapshot のメタデータ復元を補強する

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- **確認できた事実:** `SetLayerPropertyKeyframesCommand` の復元は value／補間／制御点／roving までは渡していたが、Anchor と Color Label を復元・永続化していなかった。今回追加した metadata 操作では、Undo 後に見た目と内部状態が一致しない余地がある。
- **変更:** `SetLayerPropertyKeyframesCommand` と composition resolution remap の keyframe snapshot 復元時に Anchor／Color Label を適用し、JSON codecにも roving／anchor／colorLabel を追加した。旧JSONで欠落するフィールドは既定値を使うため、形式の後方互換は維持する。
- **価値または懸念:** Anchor／Color Label の Undo／Redo と session history が同じ keyframe 状態を扱える。JSON enum値の不正入力、実際の session reload、全 property 型は未検証である。
- **次に確認すべきこと:** 既存 keyframe の Anchor／Color Label を変更し、Undo／Redoと履歴保存／再読込の各段階で値・metadata・Timeline表示が一致することを確認する。

## 2026-08-30 - 複数選択の keyframe mirror を履歴化する

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- **確認できた事実:** 複数選択時、Property 行の keyframe toggle／auto-key後に他の選択レイヤーへ keyframeを直接追加・削除する mirror 処理があり、対象レイヤーの変更は独立した Undo command を持っていなかった。
- **変更:** mirror 前後の keyframe sequence を target ごとに snapshotし、変更がある場合だけ `MacroUndoCommand` と `SetLayerPropertyKeyframesCommand` を pushするようにした。
- **価値または懸念:** mirror 対象の変更が履歴から消える問題を減らせる。primary layer の toggle command と target mirror command が完全な一つの履歴境界にはまだ統合されておらず、animatable flagも別管理していない。
- **次に確認すべきこと:** 単一／複数選択で toggle on／off、auto-key、既存 keyframe 上書き、Undo／Redo、選択解除後の再編集を行い、全 target の keyframeと履歴単位を確認する。

## 2026-08-30 - Keyframe Undo の animatable 状態も snapshot する

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`
- **確認できた事実:** keyframe を追加する操作は `setAnimatable(true)` を伴うが、`SetLayerPropertyKeyframesCommand` は keyframe 列だけを復元していた。そのため、元が非アニメーション property の Undo 後に `animatable` flagだけが残る可能性があった。
- **変更:** command に optional な before／after `animatable` snapshot と JSON field を追加し、Property／Channel Box／Key All／Key Selected／複数選択 mirror／keyframe toggle の各経路から値を渡すようにした。通常値の macro は専用 setter 実行後の実値・実 `animatable` 状態を after として記録する。旧 command payloadでは optional fieldを適用せず、従来挙動を維持する。
- **価値または懸念:** 新規に対象化した操作では keyframe列とアニメーション可否を同じ Undo境界で戻せる。Timeline の全編集経路、Effect-owned property、runtime／session reload は未検証である。
- **次に確認すべきこと:** 非アニメーション propertyへ keyframeを追加・削除し、Undo／Redoと履歴保存／再読込後に `isAnimatable`、keyframe、row／Timeline表示が一致することを確認する。

## 2026-08-30 - Timeline ローカル snapshot の復元状態を揃える

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`
- **確認できた事実:** Timeline の `TimelineKeyframeSnapshotCommand` は keyframe 列を保存していたが、`animatable` と Anchor／Color Label を snapshot／restore していなかった。復元時も `changed()` のみで、keyframe cache を明示的に dirty にしていなかった。
- **変更:** Timeline 共通 snapshot に `animatable` を加え、復元時に keyframe metadata、animatable flag、`LayerDirtyFlag::Property`、既存 `LayerChangedEvent` を適用するようにした。
- **価値または懸念:** pattern／paste／area edit／curve editorを含む Timeline のローカル Undo でも、表示と内部 keyframe 状態の復元漏れを減らせる。通常操作直後の全キー変更経路、runtime cache、保存／再読込は未検証である。
- **次に確認すべきこと:** 非アニメーション propertyへ Timeline からキーを生成・貼付・削除し、Undo／Redo後の `isAnimatable`、metadata、preview cache、dirty 表示が一致することを確認する。

## 2026-08-30 - Keyframe Undo JSON の時刻精度を保持する

- **関連:** `Artifact/src/Undo/UndoManager.cppm`
- **確認できた事実:** `SetLayerPropertyKeyframesCommand` の JSON は keyframe 時刻を `rescaledTo(30)` の frame 値だけで保存していたため、30fps以外の RationalTime を session history に出すと位置が変わり得た。
- **変更:** JSONへ `timeValue`／`timeScale` を追加し、decodeは新形式を優先、旧 `frame` payloadは30fpsとして読む後方互換を維持した。
- **価値または懸念:** 24／60fpsや小数fpsの keyframe Undo／Redo・session復元で時刻を正確に保持しやすくなる。実際のJSON round-tripと異常scale入力は未検証である。
- **次に確認すべきこと:** 異なるFPSの compositionで keyframe commandを保存・再読込し、元の RationalTime と frame表示が一致することを確認する。

## 2026-08-30 - Expression action の layer／effect ownership を分離する

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- **確認できた事実:** Property Widget の active expression action と Copilot callback は layer commandを使う際、表示名から対象を推定しており、Effect-owned propertyでは layer側に存在しない pathへ no-op commandを積む余地があった。
- **変更:** layerの実property pointerと対象 pointerが一致する場合だけ `SetLayerPropertyExpressionCommand` を生成し、それ以外は Effect-owned propertyの既存直接更新を維持した。Clear／Convert／Bakeにも同じ境界を適用した。
- **価値または懸念:** 誤対象の Undo履歴と、見かけ上成功する no-op layer commandを減らせる。Effect-owned expression専用の永続Undo commandは未導入で、runtime確認も未実施である。
- **次に確認すべきこと:** layer propertyとeffect propertyの両方で Copilot、Clear、Convert、Bakeを行い、履歴件数、値、式、keyframe、再読込結果を比較する。

## 2026-08-30 - AI automation の直接 layer／composition mutation を Undo 境界へ接続する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Service/ArtifactEffectService.cppm`
- **確認できた事実:** AI automation の代表 setter には、可視・ロック・Solo・Shy・Blend・Opacity、2D Transform、Parent、Layer／Composition Note を直接変更する経路が残っていた。Template Variation は同じ layer の Note を複数 slot 分更新するため、遅延した macro では中間状態を失う可能性があった。Effect scalar parameter もサービス内で直接変更されていた。
- **変更:** 既存 layer state／blend／opacity command、`SetLayerPropertyValueCommand`、新規 `SetCompositionNoteCommand`、effect の `SetPropertyCommand` を利用し、同値入力を除外した。Parent／Note を汎用 layer property path として適用できるようにし、Template Variation は layer ごとの pending Note を追跡して macro child の before／after を正しく連鎖させた。
- **価値または懸念:** AI 経由の通常編集が Undo／Redo と session history の対象へ揃い、同一 variation 内の slot 上書き事故を避けられる。Property Widget の Effect-owned 編集全体、グループ階層をまたぐ batch 移動、runtime／session reload は未検証であり、直接 setter は command の redo／fallback 内に限定して残る。
- **次に確認すべきこと:** AI API の state／transform／parent／note／effect scalar を単独・連続・同値入力で実行し、履歴件数、Undo／Redo、dirty state、保存／再読込、同一 layer 複数 slot の最終 Note を確認する。

## 2026-08-30 - AI の timing／layer create 操作を既存 command で一操作化する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`
- **確認できた事実:** group／solid／noise の作成、指定時刻の split、ripple delete、sequential align は直接 layer list／timing を変更しており、AI経由では Undo 単位がなかった。遅延 macro を組む場合、align の次の layer は push 前の out pointを見てしまい、同じ処理内の基準時刻がずれる可能性もあった。
- **変更:** 作成を `AddLayerCommand`、split を timing property＋`AddLayerCommand`＋index移動、ripple delete を timing property＋`RemoveLayerCommand`、sequential align を timing property macroへ接続した。align は更新後の out pointを次の基準値として保持する。direct fallback は UndoManager 不在時だけ残す。
- **価値または懸念:** AI の非破壊編集と layer生成・削除・時間配置を一つの Undo 操作として戻せる。group move／ungroup の階層 membership・順序、runtimeでの選択／cache／保存再読込は未検証である。
- **次に確認すべきこと:** split、ripple delete、alignを複数 layer・無効時刻・同値入力で実行し、Undo／Redo後の layer ID、index、in／out／start time、selection、dirty state を比較する。

## 2026-08-30 - AI effect parameter 編集に専用の永続 Undo snapshot を追加する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Service/ArtifactEffectService.cppm`
- **確認できた事実:** Effect scalar parameter は既存の `SetPropertyCommand` で値を保存できる一方、keyframe 列や expression は effect 所有 property の状態全体を復元する既存 command がなく、AI 経路では直接 setter が残っていた。
- **変更:** effect keyframe 列を metadata 込みで復元する `SetEffectPropertyKeyframesCommand` と、expression を復元する `SetEffectPropertyExpressionCommand` を追加し、factory／JSON session historyへ登録した。AI の追加・削除・式変更から利用し、同値変更は履歴へ積まない。
- **価値または懸念:** AI経由のEffect編集も scalar／keyframe／expressionをそれぞれUndo可能な境界に揃えられる。Property WidgetのEffect-owned全編集、runtime／session reload、property実装ごとのkeyframe順序は未検証である。
- **次に確認すべきこと:** Effect parameterを異なるFPS、既存keyframe、Anchor／Color Label、expression付きで編集し、Undo／Redoとsession保存／再読込後の値・metadata・式が一致することを確認する。

## 2026-08-30 - AI group hierarchy 編集を既存 command の macro に接続する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`
- **確認できた事実:** group move／ungroup は composition の layer list と parent ID を直接変更しており、複数子の所属、groupの存在、orderを同じUndo境界で戻す経路がなかった。
- **変更:** group moveは各対象の `layer.parent` を一つの `MacroUndoCommand` にまとめ、ungroupは子の親解除と `RemoveLayerCommand` を同じmacroへ収めた。親ID変更だけで既存のglobal orderを維持するため、従来の順序情報を別snapshotに複製せず復元できる。
- **価値または懸念:** 階層 membershipとgroupの存在・位置を一操作としてUndo／Redoできる。選択状態、runtime cache、session reload、部分失敗時のUI受入は未検証である。
- **次に確認すべきこと:** 複数子、既存親、空group、順序が異なるケースでmove／ungroup後にUndo／Redoし、parent ID、global order、selection、dirty stateが一致することを確認する。

## 2026-08-30 - 共通ProjectServiceのlayer move／remove／duplicateをUndo境界へ揃える

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`
- **確認できた事実:** AI wrapperが利用する共通ProjectServiceのlayer move／remove／duplicateは、move／removeが直接compositionを変更し、duplicateも完全な複製を作成・配置した後に履歴を作らなかった。
- **変更:** moveは`MoveLayerIndexCommand`、removeは`RemoveLayerCommand`、duplicateは既存サービスで生成した複製layerをdetachして`AddLayerCommand`＋index移動macroへ接続した。同値移動は履歴へ積まない。
- **価値または懸念:** AIと通常サービスのlayer list mutationが同じUndo境界へ揃う。duplicateは生成済みオブジェクトのdetach／再接続を伴うため、selectionイベントと保存時のcommand可搬性はruntime確認が必要である。
- **次に確認すべきこと:** move／remove／duplicateを選択中・子layer・matte／parent参照付きで実行し、Undo／Redo後のlayer object、index、参照、selection、project dirty stateを比較する。

## 2026-08-30 - AI一般keyframe APIをproperty snapshot Undoへ統一する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`
- **確認できた事実:** `setKeyframe`、`batchSetKeyframes`、`deleteKeyframe` は `ArtifactTimelineKeyframeModel` を通じて直接keyframe列を変更しており、AI呼出し単位のUndo境界がなく、単発APIはcomposition FPSを使う一方、一括APIは30fps固定だった。
- **変更:** propertyごとのbefore／after `KeyFrame`列とanimatable flagを作り、`SetLayerPropertyKeyframesCommand`（一括はmacro）へ接続した。単発keyframeのRationalTimeはcomposition FPSを保持する。
- **価値または懸念:** AIの一般keyframe操作も既存Timeline／Property系と同じsnapshot復元経路になる。入力valueの型変換、重複時刻、runtime cache、session reloadは未検証である。
- **次に確認すべきこと:** 24／30／60fps、既存キー上書き、複数property、削除後の最後のキーでUndo／Redoし、keyframe列、animatable、表示、保存履歴が一致することを確認する。

## 2026-08-30 - AIの汎用keyframe commandとモーション生成を共通Undoへ揃える

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`
- **確認できた事実:** CommandIR経由の`set_keyframes`／`batch_set_keyframes`は独自の直接スナップショットcommandを使っており、通常の`SetLayerPropertyKeyframesCommand`が持つmetadata復元・dirty通知・session serializationの恩恵を受けていなかった。モーションスケッチ／自動向き生成もサンプルごとに`setKeyframe`を呼び、1操作が複数Undoへ分裂していた。
- **変更:** 汎用commandのsnapshot変換を`SetLayerPropertyKeyframesCommand`へ置き換え、Anchor／Color Label／rovingを含むkeyframeとanimatable状態を共通codecへ渡すようにした。モーション生成は入力検証後に`batchSetKeyframes`を一度だけ呼び、位置または回転キー全体を一つのUndo境界へまとめた。batch interpolationのEaseInOut／Bezier名も受け付けるようにした。
- **価値または懸念:** AIのcommand APIと専用AI APIで履歴・復元経路が分裂しにくくなる。CommandIRの古いtimeScale省略payloadは30fps fallbackを維持し、実runtime／session reload／異常入力は未検証である。
- **次に確認すべきこと:** 24／60fps、既存キー上書き、metadata付きキー、空／不正サンプル、Undo／Redo、session history再読込で、件数と時刻・metadata・animatableが一致することを確認する。

## 2026-08-30 - AIのeffect複製を既存の追加effect Undoへ接続する

- **関連:** `Artifact/src/Service/ArtifactEffectService.cppm`
- **確認できた事実:** AIのeffect複製は複製オブジェクトを作成後、`addEffectToLayerInCurrentComposition()`で直接追加しており、同じサービスの追加・削除・有効状態変更が使うUndo境界から外れていた。
- **変更:** 複製の追加を`addEffectToLayerWithUndo()`へ統一した。既存の複製ID／基本プロパティ生成は維持し、同じAddEffect commandでUndo／Redoできるようにした。
- **価値または懸念:** AI経由のeffect追加系操作で履歴の責務が分裂しにくくなる。複製が全keyframe／expressionを完全コピーする仕様ではなく、runtime／session reloadは未検証である。
- **次に確認すべきこと:** layer effectの複製を既存effect、同名effect、Rasterizer以外のpipelineで行い、ID、順序、値、Undo／Redo、保存／再読込を確認する。

## 2026-08-30 - Command IR keyframe実行を一括transactionへ統一する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/src/AI/CommandIRExecutor.cppm`
- **確認できた事実:** 外部Command IR executorの`set_keyframes`／`batch_set_keyframes`は、入力検証後も各キーを個別に`setKeyframe()`へ送り、WorkspaceAutomation側の一括Undo境界を迂回していた。WorkspaceAutomationのbatchも、property欠落や不正値を後続処理まで許す余地があった。
- **変更:** executorは全キーを検証してから`batchSetKeyframes()`を一度だけ呼ぶようにし、結果詳細・件数・`valid`を返すようにした。WorkspaceAutomation側にも全件preflightを追加し、1件でも不正ならmutation前に拒否する。
- **価値または懸念:** Command IRの「1 transaction・partial mutationなし」という契約に近づく。対象property setterの実行失敗やruntime cache、外部executorの全commandの実操作は未検証である。
- **次に確認すべきこと:** 複数property、欠落property、不正frame/value、既存キー上書きで、実データが一部だけ変わらず、Undoが1回で戻ることを確認する。

## 2026-08-30 - AI別名APIの入力拒否とUndo経路を補強する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/src/AI/CommandIRExecutor.cppm`
- **確認できた事実:** 固体レイヤーのcurrent-composition別名は既存のUndo-awareな生成APIへ委譲できた一方、Command IRの空／非有限keyframe値は下位APIまで到達する余地があった。汎用property pathには`transform.opacity`の表記差もあった。
- **変更:** 固体別名を`createSolidLayer("current", ...)`へ委譲し、Command IRとWorkspaceAutomationのkeyframe入力を空配列・非負frame・正のtimeScale・有限数値まで事前検証した。汎用property setterは`transform.opacity`を`opacity`と同じUndo経路で扱う。
- **価値または懸念:** AI操作の入力不備による部分変更と、同じ操作の別名ごとの挙動差を減らせる。画像／SVG／音声／Text／Nullの既存作成別名は中央サービスの生成責務を保つため未変更で、runtime／session reloadは未検証である。
- **次に確認すべきこと:** 各別名APIとCommand IRに対して、空値・NaN・無効frame、同値入力、Undo／Redo、dirty stateを実行確認する。

## 2026-08-30 - AI音声編集をproperty／range snapshot Undoへ接続する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/include/Layer/ArtifactAudioLayer.ixx`、`Artifact/src/Layer/ArtifactAudioLayer.cppm`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`
- **確認できた事実:** AIのtrim、playback rate、de-click設定、de-click範囲は音声レイヤー／サービスを直接変更しており、AI呼出し単位のUndo境界がなかった。音声の`audio.*`数値propertyには既存のvirtual setter経路がある。
- **変更:** trimとde-click設定はproperty commandのmacro、playback rateはproperty commandへ接続した。範囲列には`SetAudioDeClickRangesCommand`と音声レイヤーの正規化・マージsetterを追加し、Undo／Redo時にresampled cacheを無効化する。
- **価値または懸念:** AI音声編集も一操作一Undo、JSON session history、cache invalidationの同じ境界へ揃えられる。通常の音声サービス呼出し、runtime／session reload、再生中cacheの受入は未検証である。
- **次に確認すべきこと:** trim／rateの境界値、範囲の重複・逆順・空値、add／clearのUndo／Redo、dirty state、保存／再読込後の範囲列とcache更新を確認する。

## 2026-08-30 - 共通layer renameを既存Undo commandへ接続する

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`
- **確認できた事実:** AIが利用する共通 `renameLayerInCurrentComposition()` は、空名を拒否する以外はlayer nameを直接変更しており、既存の`RenameLayerCommand`を利用していなかった。
- **変更:** 同値名をno-opとして除外し、UndoManagerが存在する場合は旧名／新名を保持する`RenameLayerCommand`、不在時だけ直接setterを使うfallbackへ切り替えた。
- **価値または懸念:** AIと通常サービスのlayer renameが一回のUndo／Redoとsession historyの同じ境界へ揃う。selection、project item表示、runtime／session reloadは未検証である。
- **次に確認すべきこと:** 通常名・同値名・前後空白・Undo／Redo、selection、Project View表示、保存／再読込後の名前を確認する。

## 2026-08-30 - AI layer作成別名の成功結果を実体で検証する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/src/Service/ArtifactProjectService.cppm`
- **確認できた事実:** 画像／SVG／音声／Text／NullのAI作成別名は共通ProjectServiceへ委譲した後、サービスの失敗や作成対象の不在を確認せず`success=true`を返していた。
- **変更:** active compositionのlayer ID集合を作成前にsnapshotし、作成後に同じcompositionへ新規layer IDが増えた場合だけ成功と`layerId`を返す共通helperへ置き換えた。既存ProjectServiceの生成・selection責務は維持した。
- **価値または懸念:** AI側が失敗を成功として次の操作へ進むリスクを減らせる。作成処理そのもののUndo境界、selection、runtime／session reloadは未検証である。
- **次に確認すべきこと:** 有効／無効な画像・SVG・音声path、空project、active composition切替競合、layer ID返却、Undo／Redo、保存／再読込を確認する。

## 2026-08-30 - composition renameを本体とProject ViewのUndo境界へ揃える

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/include/Service/ArtifactProjectService.ixx`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`
- **確認できた事実:** 共通 `renameComposition()` はcomposition本体とProject Viewの`CompositionItem`を直接変更していた。既存のUndoManagerには、両方の名前を一緒に戻すcommandがなかった。
- **変更:** composition本体とproject itemを再帰的に同じ名前へ適用する`RenameCompositionCommand`をUndoManagerへ追加し、コマンドファクトリへ登録した。同値をno-op、UndoManager不在時を直接適用fallbackとした。
- **価値または懸念:** AI／通常サービスのcomposition renameで、表示名の二重管理が一回のUndo／Redoへまとまり、履歴シリアライズ後もcomposition resolverで再接続できる。別project切替後の履歴、Project View選択、session reloadは未検証である。
- **次に確認すべきこと:** root／nested folder内のcomposition、同値名、Undo／Redo、Project View表示、別composition切替、保存／再読込を確認する。

## 2026-08-30 - 通常ProjectServiceのlayer state／parent変更を既存Undoへ接続する

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`
- **確認できた事実:** 通常のProjectServiceの表示・ロック・Solo・Shy・親付け・親解除は、AI側に既存commandがあるにもかかわらず直接layer setterを呼んでいた。Solo Only／Smart Soloは複数layerを一括変更するため、単一setterだけではbefore stateをまとめて戻せない。
- **変更:** 単一状態変更を既存の`SetLayerVisibilityCommand`／`SetLayerLockCommand`／`SetLayerSoloCommand`／`SetLayerShyCommand`へ接続した。Solo Only／Smart Soloは変更対象だけを`MacroUndoCommand`の子へ記録し、親変更にはbefore／after parent IDを保持する`ChangeLayerParentCommand`とJSON factoryを追加した。
- **価値または懸念:** AIと通常サービスでレイヤー状態・階層変更のUndo境界が分裂しにくくなる。親レイヤー削除後の履歴、selection、runtime cache、session reloadは未検証である。
- **次に確認すべきこと:** 表示・ロック・Solo・Shyの同値no-op、複数Solo状態、cycle拒否、parent解除、Undo／Redo、保存／再読込を確認する。

## 2026-08-30 - split Undoの失敗時部分変更を抑止する

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`
- **確認できた事実:** `SplitLayerUndoCommand::redo()`は元layerのout pointを先に短縮してからProject取得とduplicate結果を確認していた。duplicate失敗時も`splitLayerWithUndo()`が無条件に成功を返していた。
- **変更:** project／frame範囲／timing lockを事前確認し、duplicateが成功してtiming lockでない場合だけ元layerを短縮するよう順序を変更した。commandに成功状態を持たせ、Undo付き呼出しは結果を返す。失敗時に返されたtiming-locked duplicateはdetachして残さない。
- **価値または懸念:** duplicate backendの失敗で元layerだけが短縮される部分変更を減らせる。`UndoManager::push()`後の失敗command保持仕様、backend例外、selection、runtime cache、session reloadは未検証である。
- **次に確認すべきこと:** 範囲外frame、timing lock、空project、duplicate失敗、Undo／Redo、選択状態、保存／再読込を確認する。

## 2026-08-30 - Project View項目の改名・移動をIDベースUndoへ接続する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/include/Service/ArtifactProjectService.ixx`、`Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`
- **確認できた事実:** 非CompositionのProject View項目の改名とフォルダ移動は直接変更で、AI操作のUndo／Redo履歴から外れていた。履歴再適用時に古いraw pointerを使うと、project切替後に誤対象へ作用する危険がある。削除のsafe-write表示はUndo可能と宣言していたが、実装は所有項目を破棄するだけだった。
- **変更:** 改名・移動を項目ID／親IDのcommandへ変更し、Undo／Redoごとに現在project treeから再解決するようにした。Composition項目の改名は既存`RenameCompositionCommand`を使う。削除とComposition削除のsafe-write `undoAvailable`はfalseへ補正した。
- **価値または懸念:** Project ViewのAI操作で履歴境界と対象再解決の責務が明確になる。項目削除のUndo snapshot、selection、別project切替、session reloadは未検証である。
- **次に確認すべきこと:** folder／footage／solidの改名、root／nested移動、cycle拒否、同一parent no-op、Undo／Redo、保存／再読込を確認する。

## 2026-08-30 - AIフォルダ作成の成功結果を実体確認へ揃える

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`
- **確認できた事実:** `createFolderInProject()`は戻り値を持たない`ArtifactProject::createFolder()`を呼んだ後、作成された項目を確認せず常に成功を返していた。
- **変更:** 作成前後のProjectItem ID集合を比較し、新規項目がFolderで、指定名と親を満たす場合だけ成功を返すようにした。
- **価値または懸念:** AIが作成失敗や別状態を成功扱いして後続操作へ進む可能性を減らせる。作成Undo、selection、session reload、runtimeは未検証である。
- **次に確認すべきこと:** root／nested folder、同名folder、空project、作成後のID、Undo／Redo、保存／再読込を確認する。

## 2026-08-30 - Project View一括改名・移動を一操作macroへ揃える

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`
- **確認できた事実:** Project Viewの一括改名・フォルダ移動は、対象ごとのservice呼出しを繰り返すと途中失敗時にpartial mutationが残り、Undoも項目単位へ分裂し得た。
- **変更:** 全対象を先に検証し、無効項目があれば変更を開始しないようにした。成功した改名・移動は`MacroUndoCommand`一つへまとめ、同値項目はno-opとして履歴へ積まない。
- **価値または懸念:** AIの一括Project View操作で、部分変更とUndo境界の分裂を減らせる。項目IDのsession再解決、selection、runtime表示は未検証である。
- **次に確認すべきこと:** 有効／無効項目の混在、同値入力、root／nested移動、cycle拒否、Undo／Redo、別project切替後の履歴復元を確認する。

## 2026-08-30 - Group階層Undoの親変更を専用commandへ統一する

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/include/Undo/UndoManager.ixx`
- **確認できた事実:** AIのgroup move／ungroupは親変更を`SetLayerPropertyValueCommand`の汎用property pathへ渡していたが、通常ProjectService側には親IDを明示的に保持する`ChangeLayerParentCommand`が存在していた。
- **変更:** group move／ungroupのmacro childと、単一layerのAI parent setterを`ChangeLayerParentCommand`へ置き換え、old／new parent IDを`LayerID`として保存する経路へ揃えた。Ungroupではgroup削除commandを後段に残し、Undoの逆順復元を維持した。
- **価値または懸念:** 親変更が専用のcycle-aware setterとJSON履歴境界を通り、汎用property表現との差異を減らせる。選択状態、composition order、runtime cache、session reloadは未検証である。
- **次に確認すべきこと:** 複数子layerのmove、nested group、ungroup後のgroup再生成、Undo／Redo、selection、保存／再読込を確認する。

## 2026-08-30 - 親変更Undoでもtransform変更通知を通す

- **関連:** `Artifact/src/Undo/UndoManager.cppm`
- **確認できた事実:** `ChangeLayerParentCommand`は親IDを復元してUndo通知を出していたが、通常のlayer transform変更で使う既存のdirty／`LayerChangedEvent`通知経路を明示的には通していなかった。
- **変更:** Undo／Redo後に実際のparent IDが期待値になった場合だけ、既存`notifyLayerTransformChanged()`を呼ぶようにした。無効な親IDでsetterが拒否された場合は成功通知を出さない。
- **価値または懸念:** 親変更後のtransform系dirty状態・cache無効化・既存イベント観測を、通常の移動操作と同じ境界へ寄せられる。runtime cacheの再生成と二重changed通知の受入は未検証である。
- **次に確認すべきこと:** parent設定／解除、cycle拒否、Undo／Redo、複数子layer、preview cache、保存／再読込を確認する。

## 2026-08-30 - Timelineのplayhead分割・trimを共通Undo経路へ寄せる

- **関連:** `Artifact/src/Application/ActiveContextService.cppm`、`Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/src/Service/ArtifactProjectService.cppm`
- **確認できた事実:** Timelineのplayhead分割とActiveContextのIn／Out／Trim操作は、共通のUndo対応serviceやcommandを使わず直接layerを変更していた。AIのplayhead分割は成功判定も常にtrueだった。
- **変更:** AIとActiveContextのplayhead分割を`splitLayerWithUndo()`へ接続し、In／Out／Trimは既存`SetLayerPropertyValueCommand`（Trim Inはmacro）へ接続した。同値・timing lockはno-opとして履歴へ積まない。UndoManager不在時も分割前後のlayer IDを比較して成功判定し、timing-lockedな複製はdetachする。
- **価値または懸念:** UI操作とAI操作の分割・タイミング編集でUndo境界と失敗判定を共有できる。selection、current frame、cache、runtime／session reloadは未検証である。
- **次に確認すべきこと:** playhead境界外、timing lock、In／Out／TrimのUndo／Redo、keyframed layerのretime、複数選択、保存／再読込を確認する。

## 2026-08-30 - AIのWork Area／marker操作をPlaybackServiceのsnapshotへ寄せる

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/src/Service/ArtifactPlaybackService.cppm`、`Artifact/include/Undo/UndoManager.ixx`
- **確認できた事実:** AIのIn／Out point、marker、chapter、marker clearは`ArtifactInOutPoints`を直接変更しており、既存PlaybackServiceのUndo snapshot経路を迂回していた。PlaybackServiceの一部marker操作はUndoManager不在時に無条件pushする可能性もあった。
- **変更:** PlaybackServiceにbefore／after JSON比較を共通化し、In／Out pointとmarker系操作を`InOutPointsSnapshotCommand`へ接続した。AIはPlaybackServiceへ委譲するようにし、同値・空操作は履歴へ積まない。
- **価値または懸念:** AIと通常Playback操作のUndo境界・JSON履歴・no-op判定が揃い、UndoManager不在時のnull pushも避けられる。Work Area本体、current frame、runtime cache、session reloadは未検証である。
- **次に確認すべきこと:** In／Outの設定・解除、marker上書き・削除・clear、Undo／Redo、別composition切替、保存／再読込を確認する。

## 2026-08-30 - レイヤー作成を初期配置込みの一操作Undoへ揃える

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`
- **確認できた事実:** 共通ProjectServiceのlayer作成はProjectManagerで実体を生成した後、初期表示・時刻・選択layer基準の順序・親を直接調整していたため、AIと通常UIの作成操作がUndo履歴から外れていた。
- **変更:** 作成後の完全なlayerを一度detachし、既存の`AddLayerCommand`、必要なindex移動、親ID変更を`MacroUndoCommand("Create Layer")`へまとめた。UndoManager不在時は従来の直接経路を維持し、detachに失敗した場合はcommandを積まない。
- **価値または懸念:** 作成と初期配置・親付けを一回のUndo／Redo境界へ揃え、AIの成功結果確認と実体履歴の差を減らせる。selection、作成イベント、runtime cache、session reloadは未検証である。
- **次に確認すべきこと:** image／SVG／audio／text／null／group、selected layer基準の順序、nested parent、current-frame placement、Undo／Redo、保存／再読込を確認する。

## 2026-08-30 - Work Area変更を再生範囲同期付きUndoへ揃える

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/include/Service/ArtifactPlaybackService.ixx`、`Artifact/src/Service/ArtifactPlaybackService.cppm`
- **確認できた事実:** Work Areaの開始・終了・現在frameへの移動はCompositionを直接変更し、Undo後にPlayback Engineの範囲を再同期する共通境界もなかった。
- **変更:** `SetCompositionWorkAreaCommand`でbefore／afterの開始・終了frameを保持し、Undo／Redo時に既存`WorkAreaChangedEvent`とPlayback Engine同期を実行するようにした。同値操作はno-op、UndoManager不在時は既存の直接経路を維持する。
- **価値または懸念:** In／Out、marker、Work Areaのタイムライン範囲操作が、UI更新と再生範囲同期を含む一操作Undoへ揃う。commandはPlaybackServiceの同期callbackを持つためsession history対象外であり、composition切替後の履歴・runtime cacheは未検証である。
- **次に確認すべきこと:** Work Area 3操作、Undo／Redo、再生中の範囲同期、別composition切替、保存／再読込を確認する。

## 2026-08-30 - Group／UngroupのUndo中に履歴を入れ子にしない

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`
- **確認できた事実:** `groupSelectedLayersWithUndo()` と `ungroupSelectedGroupWithUndo()` の専用commandは、UndoManagerのredo／undo中に通常のProjectService操作を呼んでいた。その操作もUndo対応済みになったため、履歴の入れ子、部分変更、成功結果の誤判定が起き得た。
- **変更:** グループ作成はAdd／親変更、Ungroupは親解除／Removeをそれぞれ一つのmacroへ直接構成した。対象compositionと全layerを事前確認し、適用後の実体を検証する。失敗時は元の親IDへ戻し、作成されたgroupを除去する。
- **価値または懸念:** 通常UI・Timeline・AIが同じグループ操作のUndo境界を使い、Undo中の追加履歴を避けられる。selection、order、予算超過時の全復元、runtime cache、session reloadは未検証である。
- **次に確認すべきこと:** 複数選択、nested group、空group、失敗・予算超過、Undo／Redo、選択復元、保存／再読込を確認する。

## 2026-08-30 - Group／UngroupのUndoでselection snapshotも復元する

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`
- **確認できた事実:** group／ungroupの構造macroを一つにしても、group作成後のUndoでは削除されたgroupがselection managerのcurrent layerに残り、Redoではselectionが暗黙に戻らない余地があった。
- **変更:** 既存`restoreLayerSelection()`を使う`LayerSelectionSnapshotCommand`を追加し、group／ungroup macroの最後にbefore／after selectionを記録した。Undoでは構造変更前、Redoではgroupまたは子layerの選択を適用する。
- **価値または懸念:** 構造変更とselectionのUndo境界を一操作へ揃え、削除済み対象をInspectorが参照し続ける可能性を減らせる。selection managerが別compositionをactiveにしている場合、snapshotは意図的に適用しない。
- **次に確認すべきこと:** 複数選択順、current layer、nested group、別composition切替中のUndo／Redo、保存／再読込を確認する。

## 2026-08-31 - Project Viewのフォルダ作成をID保持Undoへ接続する

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/include/Service/ArtifactProjectService.ixx`、`Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- **確認できた事実:** フォルダ作成はvoid APIを直接呼ぶだけで、AI側は作成前後のID差分を確認していたが、通常Project Viewと共通のUndo境界を持っていなかった。また同名フォルダを名前・親だけで探すと既存項目を誤認し得る。
- **変更:** 作成前のID集合から新規Folderを特定し、`CreateProjectFolderCommand`へID・親ID・名前・tagを渡す。Undoは空フォルダだけを削除し、Redoは`addProjectItemsFromJson()`で同じIDを復元する。AIとProject Viewの両方をProjectService経由へ揃えた。
- **価値または懸念:** 同名フォルダを誤って履歴対象にせず、フォルダ作成とRedo復元を一操作として扱える。後続項目を持つフォルダのUndoは安全のため拒否し、selection、session reload、runtimeは未検証である。
- **次に確認すべきこと:** root／nested folder、同名既存項目、Undo／Redo、作成直後の子項目追加、保存／再読込を確認する。

## 2026-08-31 - 非CompositionのProject item削除をサブツリーUndoへ接続する

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/include/Service/ArtifactProjectService.ixx`、`Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/include/AI/WorkspaceAutomation.ixx`
- **確認できた事実:** Project item削除はCompositionのrender queue cleanupを含む専用経路と、その他の`removeItem()`直接経路に分かれていた。非Composition項目も削除後のUndo snapshotを持たず、safe-write計画は一律`undoAvailable=false`だった。
- **変更:** Folder／Footage／Solidについて、親ID・兄弟indexとサブツリーのJSON snapshotを`RemoveProjectItemCommand`へ保存し、Undoは`addProjectItemsFromJson()`で同じIDを元位置へ復元、RedoはIDで再解決して削除する。Compositionを含むFolderは復元範囲がrender queueまで及ぶため対象外とし、snapshotがUndo単一エントリ上限を超える場合は削除前に拒否する。safe-writeのUndo可否も対象型へ合わせた。
- **価値または懸念:** 通常のProject View／AI削除で、素材ツリーを一操作として戻せる。Composition、render queue、selection、session reload、runtimeは未検証であり、対象外項目は従来経路を維持する。
- **次に確認すべきこと:** nested folder、sequence footage、solid、同一ID復元、Composition混在Folderの拒否、Undo／Redo、保存／再読込を確認する。

## 2026-08-31 - Mask reorder commandをsession対応のsnapshotへ拡張する

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- **確認できた事実:** `MoveMaskCommand`はUndo／Redo自体は存在したが、layerをpointerだけで保持し、command type／JSON codec／factoryがなかった。またUndo／Redo中のlayer changed通知もなかった。
- **変更:** layer IDと前後indexを保持するserialization／deserializeを追加し、履歴load時は既存`UndoManager::resolveLayer()`で再解決する。移動後に`layer->changed()`を呼ぶ。
- **価値または懸念:** Mask reorderをsession history／offload対象へ含められ、Undo後の表示更新契約を揃えられる。maskの同一性が別操作で変わる場合、selection、runtime、session reloadは未検証である。
- **次に確認すべきこと:** mask up／down、複数mask、Undo／Redo、履歴JSON round-trip、別操作後の再適用を確認する。

## 2026-08-31 - Composition解像度Remapのsnapshotをsession codecへ接続する

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- **確認できた事実:** `ChangeCompositionResolutionCommand`はmask・transform property・keyframeのbefore snapshotを保持していたが、composition ID、サイズ、policy、snapshotのJSON codec／factoryがなく、session history／offloadから外れていた。
- **変更:** composition ID、旧／新サイズ、`RemapPolicy`、layer snapshot、mask codec、許可されたkeyframe valueをJSON化し、load時に既存composition resolverへ再接続する。unsupported valueはserializable対象から外す。
- **価値または懸念:** 解像度変更のUndo情報をsession境界へ持ち越せる。maskの完全な入力検証、runtime remap表示、別composition、session reloadは未検証である。
- **次に確認すべきこと:** mask／animated transform付きcomposition、各RemapPolicy、Undo／Redo、JSON round-trip、履歴load後の再適用を確認する。

## 2026-08-31 - モジュレーションsnapshotのsession codec化

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Audio.Modulation.Router`
- **確認できた事実:** effect／layerのモジュレーションUndoはsource定義とassignmentをsnapshotで保持していたが、pointer依存でJSON codec／factoryがなかった。assignmentの`targetId`は64bitで、JSON数値として扱うと精度を失う可能性がある。
- **変更:** sourceの全設定、assignment、smoothingTimeをJSON化し、`targetId`は文字列として保存する。非有限値、重複source ID、存在しないsource参照、範囲外enumをdeserialize／serializable判定で拒否する。
- **価値または懸念:** モジュレーション変更をsession historyへ含めても、source再構成時のIDとassignmentの対応を保持できる。音響runtime、session reload、別composition復元は未検証である。
- **次に確認すべきこと:** LFO／ADSR／Random／Macro各source、assignmentのAdd／Multiply、64bit target ID、Undo／Redo、JSON round-trip、session再読込を確認する。

補足として、Effect modulation setterのbefore／after snapshot比較も追加した。同一snapshotを受けた場合はUndo履歴・dirty通知・変更イベントを増やさず、DCC操作のno-op契約を維持する。runtime通知順序は未検証である。

## 2026-08-31 - AI group操作のpartial mutation防止

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`
- **確認できた事実:** `moveLayersToGroup()` は無効ID・自己移動・循環対象を黙ってスキップし、Undo pushが予算で拒否されても対象数を成功として返す可能性があった。`ungroupLayers()` もgroup削除や全子layerの親解除を確認せず成功を返していた。
- **変更:** mutation前に全対象を検証し、移動後／ungroup後の親関係を検証する。適用失敗時は履歴済みならUndoし、未記録の場合は保持した旧親IDまたはgroup実体から復元する。
- **価値または懸念:** AIの一括階層操作が部分成功を隠さず、失敗時に次の操作へ壊れた親関係を残しにくくなる。selection、order、runtime cache、session reloadは未検証である。
- **次に確認すべきこと:** 正常な複数移動、重複ID、欠落ID、自己／循環移動、予算拒否、group内外のUndo／Redoを確認する。

## 2026-08-31 - AI mutation後の実状態検証

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`
- **確認できた事実:** AIのtransform、note、keyframe、Project View一括rename／moveはUndo commandを呼んでいても、UndoManagerの単一entry予算でpushが拒否された場合に成功を返し得た。batch macroの部分適用後も結果を確認していなかった。
- **変更:** scalar値、keyframe存在／値、project itemの名前／親をpush後に検証する。batch失敗時は、macroが履歴に残っていればUndoし、残っていなければ保持した旧値・旧親・旧indexから復元する。
- **価値または懸念:** API結果が実状態と一致し、予算制約やcommand適用失敗を隠さない。復元時のUI通知、selection、runtime cache、session reloadは未検証である。
- **次に確認すべきこと:** 極小Undo予算、複数選択、keyframe既存置換、composition itemと一般assetのrename、同一parent移動、Undo／Redoを確認する。

## 2026-08-31 - 共通ServiceのUndo postcondition統一

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/src/Service/ArtifactEffectService.cppm`、`Artifact/include/AI/WorkspaceAutomation.ixx`
- **確認できた事実:** Project／Effect serviceの一部はUndo commandをpushした後、予算拒否やredo失敗を確認せず成功通知・dirty通知へ進んでいた。複製、layer／folder作成では実体を先に生成・移動するため、履歴登録失敗時にdetached objectが残る余地もあった。
- **変更:** 既存commandのserializable／budget条件を確認してからpushし、layer／effect値、状態toggle、parent、audio range／trim、keyframe、expression、modulation、並び順、project treeをpush後に再読込する共通ヘルパーへ揃えた。作成・複製経路は失敗時に実体を元のtreeへ再接続する。
- **価値または懸念:** APIの成功結果、Undo履歴、実データ、変更通知の乖離を減らせる。service全経路のruntime挙動、selection、通知順序、session reloadは未検証である。
- **次に確認すべきこと:** 極小Undo予算、command適用失敗、layer複製・作成・削除、project folder作成／移動、effect scalar／modulation、audio設定のUndo／Redoと通知順序を確認する。

## 2026-08-31 - session履歴とdisk offloadの破損境界を厳格化

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`docs/planned/MILESTONE_UNDO_FRAMEWORK_HARDENING.md`
- **確認できた事実:** session historyのsave／loadは、serializable commandの空payloadや不正・未知entryを黙ってスキップし、部分的な履歴を成功として扱う余地があった。disk offloadの復元も、退避 envelopeのtype／label／estimatedBytesとwrapperの一致を確認していなかった。
- **変更:** serializable commandのtype欠落・空payloadを保存失敗とし、load時は不正entry・復元不能command・不正versionを履歴全体の失敗として扱う。offloadのenvelopeとfactory生成commandのtypeも照合する。
- **価値または懸念:** 履歴の欠落や別commandの誤復元を成功扱いしにくくなる。意図的に`canSerialize() == false`のcallback／selection snapshotは引き続きsession対象外で、実runtimeの破損復旧は未検証である。
- **次に確認すべきこと:** 正常なsave／load、旧形式の`currentVersion`欠落、未知type、空payload、改変済みoffload JSON、Undo／Redoとdirty stateを確認する。

補足として、退避wrapperが履歴から外れるときに対応ファイルをデストラクタで削除するようにした。budget eviction、session load、UndoManager破棄の各経路で退避ファイルが蓄積しない契約を揃えるが、権限エラーを含む実filesystem cleanupは未検証である。

履歴envelopeのversion／estimatedBytes／state versionも、単なる数値変換ではなく有限・非負の整数として検証するようにした。小数や文字列を黙って切り捨てて状態ID・予算を解釈しないための境界である。

keyframeの時刻・補間・制御点・metadata、mask／matte／text animator／effect maskの必須配列・オブジェクトも型検証するようにした。maskのpath数値・enum・Bezier頂点も有限値と要素数を検証する。旧keyframeのframe-only payloadは30fps fallbackで保持し、未対応のQVariantは`canSerialize()`で除外する。

## 2026-08-31 - 追加codecの数値境界を厳格化

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`Artifact/include/Undo/UndoManager.ixx`
- **確認できた事実:** Align snapshotは配列要素・座標・scaleを暗黙変換し、opacityとcomposition resolutionも文字列や小数を数値として扱える余地があった。
- **変更:** Alignのsnapshot配列・layer ID・有限floatを検証し、旧scale欠落だけは1.0へfallbackする。opacityは有限値のみ、composition resolutionは正の整数サイズと整数RemapPolicyのみ受け入れる。
- **追加:** session entryの`estimatedBytes`も復元後commandの実測値と照合し、サイズmetadataだけが改変された履歴を受け入れないようにした。旧形式でfieldがない場合は許容する。
- **価値または懸念:** session／offloadから復元した履歴が、NaN相当や小数切り捨てによる座標・解像度でUndo／Redoを実行しにくくなる。runtimeのcodec受入は未検証である。
- **次に確認すべきこと:** Alignの旧payload、新旧解像度、各RemapPolicy、opacity境界、JSON round-trip、Undo／Redoを確認する。

## 2026-08-31 - 外部ファイル操作のUndo失敗を履歴へ反映

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`MoveAssetFileCommand`
- **確認できた事実:** `UndoManager::push()`／`undo()`／`redo()` はcommandの`void`操作後に常にstackを進めるため、asset rename／moveの`QFile::rename()`失敗でも成功した履歴へ移される可能性があった。
- **変更:** 任意commandが直前操作の成否を返す`lastOperationSucceeded()`を追加し、MoveAssetFileCommandでrename結果を記録する。失敗時はpush／undo／redoのstack位置を維持し、Macroとdisk offload wrapperも子command／復元commandの失敗を伝播する。
- **価値または懸念:** 外部filesystemの失敗で、実ファイル状態とUndo履歴が乖離しにくくなる。既存のvoid commandは互換性のため成功既定値であり、個別の適用失敗検出は今後の対象である。
- **次に確認すべきこと:** 同名衝突、権限拒否、欠落source、Macro内move、offload後のrename、失敗後のdirty stateと再試行を確認する。

## 2026-08-31 - Asset Browserの外部操作をUndo成否へ接続

- **関連:** `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`、`UndoManager::push()`、Asset Browserのrelink／delete／import
- **確認できた事実:** Asset Browserのbatch relinkは実変更後にcommandを登録し、Undo予算でpushが拒否されても変更を残す可能性があった。deleteはredoのたびにbackupを上書きし、command破棄時のbackup cleanupもなかった。
- **変更:** `UndoManager::push()`を成否を返すAPIへ拡張し、relink／import登録はpush失敗時に実データをrollbackする。deleteはbackupを一度だけ作成して再利用し、copy失敗時の部分ファイルを掃除し、destructorでbackupを削除する。batch relinkはfile／layerの適用失敗とpush拒否をrollbackする。
- **価値または懸念:** 外部filesystem／project registryとUndo stackの乖離を減らし、削除用backupが履歴操作後に蓄積しにくくなる。実runtimeの権限・同名・サービス失敗は未検証である。
- **次に確認すべきこと:** relink／delete／importの同名衝突、権限拒否、欠落ファイル、Undo予算拒否、Macro／redo／manager破棄後のfilesystemとproject treeを確認する。

## 2026-08-31 - Undo push拒否をpostconditionより先に扱う

- **関連:** `Artifact/include/AI/WorkspaceAutomation.ixx`、`Artifact/src/Service/ArtifactProjectService.cppm`、`Artifact/src/Service/ArtifactEffectService.cppm`
- **確認できた事実:** 共通`pushUndoCommandAndVerify()`と一部のAI／ProjectService経路は、`UndoManager::push()`の失敗後もpostconditionや前回のoperation outcomeを評価していた。事前に実体を生成・変更する経路では、履歴へ登録できなくても成功を返す余地があった。
- **変更:** `push()`のbool結果をpostcondition評価前に確認し、keyframe、batch、layer／effect、folder、precompose／ungroup操作で履歴登録失敗を即時失敗として返す。project item削除は先行する直接削除をやめ、既存`RemoveProjectItemCommand`に実行を委譲した。
- **価値または懸念:** Undo履歴・実状態・API結果の三者が、budget拒否や初回redo失敗で食い違いにくくなる。runtime、selection、通知順序、session reloadは未検証である。
- **次に確認すべきこと:** 極小Undo予算、初回redo失敗、folder／effect／precompose／ungroupの成功・拒否、失敗直後のdirty stateと再試行を確認する。

## 2026-08-31 - Preview編集をUndo push拒否から復元

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
- **確認できた事実:** Property／Channel BoxとTimelineの一部操作は、値やkeyframeを先に直接変更してからUndo commandをpushしていたため、Undo予算拒否時に実状態だけが残り、通知・selection・成功表示が先へ進む余地があった。
- **変更:** Property側はkeyframe列、animatable、Anchor、Color Label、Expression、Text Animator、opacityのbefore snapshotを保持し、push拒否時に復元する。Timeline側もkeyframe／curve／trajectory／fringe／area value／pasteを復元し、ripple／slide／easingのcommand-only操作はpush結果だけを成功扱いにする。
- **価値または懸念:** 履歴へ登録できない操作が編集結果だけを残す不整合を減らし、失敗時の再描画・成功メッセージを実状態に合わせられる。selection、cache、dirty state、実runtimeの極小Undo予算受入は未検証である。
- **次に確認すべきこと:** Property／Timelineのpreview→commit、keyframe metadata、curve selection、ripple／paste、Undo予算拒否、失敗後の再試行とsession reloadを確認する。

## 2026-08-31 - TimelineTrackPainterViewの選択編集を同じ失敗境界へ統合

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、Timelineのkeyframe range／drag／curve／context menu
- **確認できた事実:** 選択keyframeの反転・削除・値・Anchor・Color Label・範囲変換・ドラッグ・接線・重複整理・ソーステキストは、編集後にUndo commandをpushする経路があり、budget拒否時に編集結果だけが残る可能性があった。
- **変更:** 共通rollback helperでbefore keyframe snapshotを復元し、選択も戻す。補間・roving・ripple・slideはcommand-onlyのpush結果を件数、通知、成功表示へ伝播する。
- **価値または懸念:** Timelineの主要なkeyframe編集で、履歴のない変更や誤った成功通知を減らせる。runtime、極小Undo予算、selection、dirty／cache受入は未検証である。
- **次に確認すべきこと:** range／drag／curve／context menu各操作のpush拒否、metadata保持、selection復元、Undo／Redo、失敗後の再試行とsession reloadを確認する。

## 2026-08-31 - PlaybackとMotion Sketchの先行変更を復元

- **関連:** `Artifact/src/Service/ArtifactPlaybackService.cppm`、`Artifact/src/Service/ArtifactPlaybackShortcuts.cppm`、`Artifact/src/Application/ActiveContextService.cppm`、`Artifact/src/Tool/ArtifactMotionSketchTool.cppm`
- **確認できた事実:** in/out point・marker・Motion Sketchは実状態を先に変更してからUndo commandを登録する経路があり、push拒否時に変更だけが残る可能性があった。ActiveContextのtrim系はcommand push結果を見ずに成功ログを続けていた。
- **変更:** InOutPointsはbefore JSONへ、Motion Sketchはbefore position snapshotへ復元し、ActiveContextのlayer in/out／trimはpush拒否時に処理を中断する。
- **価値または懸念:** playback範囲・marker・motion pathの実状態とUndo履歴・成功通知がbudget拒否で乖離しにくくなる。runtime、playback engine同期、極小Undo予算は未検証である。
- **次に確認すべきこと:** marker／in-out／trim／motion sketchの拒否、Undo／Redo、engine frame range同期、dirty state、失敗後の再試行を確認する。

## 2026-08-31 - Project Viewとsource/effect commandのpush結果を伝播

- **関連:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`
- **確認できた事実:** composition resolution remap、source relink／localize、composition effect追加はcommand-onlyのUndo pushを呼ぶが、拒否時も後続処理や成功結果へ進む経路が残っていた。
- **変更:** pushのbool結果を確認し、拒否時は警告・処理結果・project mutation通知へ進まないようにした。
- **価値または懸念:** resolution／source／effectの実状態と履歴・dirty通知の乖離を減らせる。runtime、composition／render cache、極小Undo予算は未検証である。
- **次に確認すべきこと:** resolution policy、source path衝突・権限、effect追加のbudget拒否、dirty state、Undo／Redoを確認する。

## 2026-08-31 - InspectorとTimeline左ペインのUndo拒否境界を統一

- **関連:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`
- **確認できた事実:** Inspectorのmatte／mask／effect mask／Surface FXとTimeline左ペインのvariant／状態列／mask削除／layer移動は、command-onlyのpush拒否後も再描画・成功扱いへ進む経路があり、mask presetとmask削除は実状態を先に変更していた。
- **変更:** push結果を確認して更新処理を止め、mask presetは`MaskEditCommand`へ接続した。先行変更するmask削除はbefore snapshotへ復元し、Surface FXの選択indexもpush拒否時に戻す。
- **価値または懸念:** matte・mask・effect mask・layer stateとUndo履歴、selection表示の乖離を減らせる。runtime、極小Undo予算、dirty／cache同期は未検証である。
- **次に確認すべきこと:** mask preset／削除、matte drag、effect mask画像、Surface FX、複数選択macroのpush拒否、Undo／Redo、selection復元、再試行を確認する。

## 2026-08-31 - Render Layer Widget v2のdrag commitを復元可能化

- **関連:** `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`、mask／polygon／parametric shape editing
- **確認できた事実:** mask path、custom polygon、corner radius、star inner radiusはドラッグ中に実状態を変更し、release時にUndo commandをpushしていたため、push拒否時に編集だけが残る可能性があった。
- **変更:** mask／polygon／corner radius／star inner radiusのpush結果を確認し、拒否時は保持していたbefore geometryへ戻す。
- **価値または懸念:** render editingとUndo履歴の乖離を減らせる。maskの要素追加・削除を伴う構造変更、runtime、render cacheは未検証である。
- **次に確認すべきこと:** mask path drag、polygon／tangent drag、corner／star dragの極小Undo予算、Undo／Redo、dirty state、render cache同期を確認する。

## 2026-08-31 - Layer Menuの整列系先行変更をUndo拒否から復元

- **関連:** `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`
- **確認できた事実:** 整列・分布・spacing・衝突解消はレイヤーのposition／scaleを先に書き換えてから`AlignLayersUndoCommand`をpushしていたため、拒否時に編集だけ残る可能性があった。layer作成macroもpush結果をpostconditionへ明示的に反映していなかった。
- **変更:** 既存`AlignLayerSnapshot`のbefore位置／scaleを使う復元ヘルパーを追加し、4操作のpush拒否時に復元する。layer作成macroは`pushed`を検証結果へ含める。
- **価値または懸念:** 複数レイヤーの配置実値とUndo履歴の乖離を減らせる。runtime、selection、dirty／cache同期は未検証である。
- **次に確認すべきこと:** 整列／分布／衝突解消の極小Undo予算、Undo／Redo、選択保持、失敗後の再試行を確認する。

## 2026-08-31 - Viewport transform系のUndo拒否復元を拡張

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/TransformGizmo.cppm`、`Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
- **確認できた事実:** viewportの3D／2D transform、anchor、複数選択transform、motion path、shape path、live field、camera POI、corner radiusは先に実状態を変更してからUndo commandをpushする経路が残っていた。
- **変更:** 既存before snapshotと既存適用関数でpush拒否時に状態を復元し、失敗時のpublish／成功扱いを抑制した。shape path／operator変換とLayer Editorのpath transactionも同じ境界へ接続した。
- **価値または懸念:** viewport編集とUndo履歴・通知・render dirtyの乖離を減らせる。runtime、極小Undo予算、selection／cache同期は未検証である。
- **次に確認すべきこと:** multi-transform、anchor、motion path、shape conversion、operator stack、POI、corner radiusの拒否、Undo／Redo、selection復元、再試行を確認する。

## 2026-08-31 - Rigとマスク一括操作のUndo拒否復元

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、Rig編集、mask編集、Puppet pin
- **確認できた事実:** Rigの骨／制御点／ウェイト／ポーズと、選択レイヤーへのmask一括操作は実状態を先に変更してからUndo commandをpushし、拒否時にも変更・通知・選択更新を残す経路があった。mask色変更はtransaction開始なしでUndoへ接続されていなかった。
- **変更:** push結果を確認し、拒否時はRig snapshot、Puppet pin、line endpoint、mask全体を復元する。複数レイヤーmask macroもbefore snapshotを保持して全対象をrollbackし、mask色変更を`MaskEditCommand`へ接続した。
- **価値または懸念:** Rig／mask編集の実状態、Undo履歴、失敗結果の乖離を減らせる。runtime、極小Undo予算、selection／cache、Puppet deform同期は未検証である。
- **次に確認すべきこと:** Rig drag／weight／pose、mask一括delete／toggle／reorder／geometry、Puppet pin、line endpointのpush拒否、Undo／Redo、失敗後の再試行を確認する。

## 2026-08-31 - Dock layout snapshotのUndo拒否復元

- **関連:** `Artifact/src/Widgets/ArtifactMainWindow.cppm`、Dock layout snapshot
- **確認できた事実:** Dock layoutは画面状態を先に変更した後でsnapshot commandをpushするため、Undo予算拒否時に変更後レイアウトだけが残る可能性があった。
- **変更:** snapshot pushのbool結果を確認し、拒否時は既存の`restoreDockManagerState()`で変更前状態へ戻す。
- **価値または懸念:** Dock配置と履歴の乖離を減らせる。runtime、QADS backend差、dock状態のselection／cacheは未検証である。
- **次に確認すべきこと:** dock移動・浮動化・閉じる／再表示、Undo予算拒否、Undo／Redo、アプリ再起動後のlayout復元を確認する。

## 2026-08-31 - Text editorのAnimator snapshot復元

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、inline text editor
- **確認できた事実:** Text editorはAnimator stackを直接更新した後で`SetTextAnimatorStackCommand`をpushしており、履歴登録拒否時にstackだけ残る可能性があった。
- **変更:** command pushの失敗時に既存の`restoreTextAnimatorStack()`で変更前snapshotへ戻す。通常のstyle編集経路は今回の対象外とした。
- **価値または懸念:** Animator stackとUndo履歴の部分的な乖離を減らせる。style項目全体の一操作Undo、runtime、極小Undo予算は未検証である。
- **次に確認すべきこと:** Animator count／preset、style同時変更、push拒否、Undo／Redo、editor再open後のstack表示を確認する。

## 2026-08-31 - Particle viewport dragのUndo拒否復元

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、particle emitter／effector drag
- **確認できた事実:** Particleのemitter位置・方向、effector位置・影響半径はdrag中に実体を更新し、release時にproperty commandをpushするため、登録拒否時にdrag結果だけが残る可能性があった。
- **変更:** emitter／effectorのbefore値へ戻すrollbackを、4つのrelease commitへ追加した。
- **価値または懸念:** Particle viewport操作とUndo履歴の乖離を減らせる。runtime、particle cache／simulation同期、極小Undo予算は未検証である。
- **次に確認すべきこと:** emitter／direction／effector／radius dragの拒否、Undo／Redo、再生中のsimulation再同期を確認する。

## 2026-08-31 - Composition Render Widgetのlayer nudge通知整合

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、layer drag／arrow-key nudge
- **確認できた事実:** realtime dragは一度beforeへ戻してから`MoveLayerCommand`をpushする構造だったが、push拒否でも最終変更通知を出していた。arrow-key nudgeもcommand-onlyのpush結果を見ずにchanged通知を出していた。
- **変更:** push成功時だけ最終通知を出すようにした。push拒否時はdragがbefore位置のまま残る既存構造を維持する。
- **価値または懸念:** viewportのlayer位置、履歴、変更通知の整合を高める。runtime、selection、cache、極小Undo予算は未検証である。
- **次に確認すべきこと:** drag／arrow nudgeの拒否、通知回数、Undo／Redo、selectionとrender cache同期を確認する。

## 2026-08-31 - Composition Editorの安全削除結果伝播

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、Safe Delete Layers
- **確認できた事実:** Safe DeleteはRemoveLayer macroのpush結果を確認せず、Undo登録拒否後もselection clearと削除完了ダイアログを進めていた。
- **変更:** macro pushが失敗した場合はselectionと完了表示を変更せず、処理を終了する。
- **価値または懸念:** UI上の完了表示とUndo履歴の状態を一致させる。runtime、削除対象の依存関係、selection復元、極小Undo予算は未検証である。
- **次に確認すべきこと:** Safe Deleteのpush拒否、RemoveLayer macroの部分失敗、Undo／Redo、selection保持を確認する。

## 2026-08-31 - Layer Menuの作成・方針変更をUndo境界へ統合

- **関連:** `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`、Quick Layer、cache policy、proxy quality
- **確認できた事実:** Quick Layerは生成済みlayerを一度compositionから外してmacroへ再包装するため、push拒否時にlayerが消える可能性があった。cache policyとproxy qualityは直接setterで変更し、履歴へ登録していなかった。
- **変更:** Quick Layerのpush拒否時は元indexへlayerを復元し、cache policy／proxy qualityは既存`SetLayerPropertyValueCommand`へ接続した。
- **価値または懸念:** Layer Menuの作成・表示方針変更で実体とUndo履歴が乖離しにくくなる。runtime、selection、proxy/cache同期、極小Undo予算は未検証である。
- **次に確認すべきこと:** Quick Layerのmask／envelope／配置、cache policy、proxy qualityの拒否、Undo／Redo、selection保持を確認する。

## 2026-08-31 - Composition Editorの一括タイミング操作とPaste復元

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、Paste Layers、Sequence Layers、Match Layer Duration、Center Layer
- **確認できた事実:** Paste Layersは既存レイヤーを一時的にcompositionから外してからUndo macroへ再包装していたため、push拒否時に貼り付け済みレイヤーが消える余地があった。また、一括Sequence／Match DurationとCenter Layerは実状態を直接変更するだけで、同じ操作単位のUndo境界がなかった。
- **変更:** Pasteのpush拒否時にレイヤーを元indexへ戻し、Sequence／Match Durationを既存`LayoutSnapshotCommand`へ接続して拒否時はbefore snapshotへ戻す。Center Layerは既存`MoveLayerCommand`へ接続し、cleanup候補のbool APIもpush結果を返すようにした。マスク単発編集はUndo commit成功後にだけ変更通知を発行する順序へ揃えた。
- **価値または懸念:** 貼り付け・タイミング一括編集・中央配置の実状態とUndo履歴を一致させやすくする。Paste後のselection、timeline cache、runtime、極小Undo予算での実動作は未検証である。
- **次に確認すべきこと:** Pasteの複数レイヤー順序・親／matte参照、Sequence／Match DurationのUndo／Redo、Center Layerのkeyframe時刻とselection保持を確認する。

## 2026-08-31 - Pen確定とPending Maskの失敗境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、Pen tool、Shape Path、矩形／楕円mask
- **確認できた事実:** Shape Path確定は直接pathを更新して通知していた。Pending MaskはUndo commit前にpending stateを消していたため、登録拒否後の再試行状態を失う余地があった。閉じパス・segment挿入・矩形maskもcommit前通知が残っていた。
- **変更:** Shape Pathを`ShapePathVertexEditCommand`へ接続し、2頂点以上のpathを保持する。Pending Maskはcommit成功後にpending stateを消去し、閉じパス・矩形maskの通知をcommit成功後へ移動した。segment挿入はrelease時のcommit結果だけを通知する。
- **価値または懸念:** Pen編集の実状態・Undo履歴・失敗後の再試行を一致させやすくする。path／mask cache、selection、runtime、極小Undo予算は未検証である。
- **次に確認すべきこと:** Shape Pathの2／3頂点、Pending Maskの拒否後再試行、閉じパス、segment挿入、矩形／楕円maskのUndo／Redoを確認する。

## 2026-08-31 - 一括編集の失敗表示整合

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、Auto Stagger／Adaptive Text Fit／Quick Replace Sources
- **確認できた事実:** これらの一括操作はcommand-onlyで実体を先に変更しないため、Undo push拒否時に状態が残る問題はなかったが、戻り値を無視して後続の完了処理へ進む経路があった。
- **変更:** push結果を確認し、失敗時は成功後の案内を出さず、操作を中断して失敗メッセージを表示するようにした。
- **価値または懸念:** 実状態・Undo履歴・UI結果表示の不一致を減らせる。runtime、極小Undo予算、ダイアログ連続操作は未検証である。
- **次に確認すべきこと:** 各一括操作のpush拒否、通常成功時の完了表示、Undo／Redoを確認する。

## 2026-08-31 - 複数選択レイヤー順序の一操作化

- **関連:** `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`、Bring／Send layer order actions
- **確認できた事実:** 複数選択の前後移動・最前面／最背面移動が個別commandを順番にpushしており、Undoが複数エントリへ分裂していた。初期indexをそのまま再利用するため、複数選択の相対順序が反転し得た。
- **変更:** 現在順序をシミュレーションし、実行時点の旧indexを持つ既存`MoveLayerIndexCommand`を一つの`MacroUndoCommand`へまとめた。操作方向ごとに安全な適用順を選び、無効indexは除外する。command自身も移動後indexを検証し、macroへ失敗を伝播する。
- **追加確認:** 共通の`AnimationLayerStackSnapshotCommand`と`LayoutSnapshotCommand`も復元後のsnapshot／callback結果を`lastOperationSucceeded()`へ返すようにし、初回redo失敗をUndoManagerが見逃さないようにした。復元callbackが部分変更して失敗した場合は、逆snapshotを補償適用してから失敗を返す。
- **価値または懸念:** 複数選択の順序保持と一操作Undoを同時に扱える。runtime、selection、部分的なlayer移動失敗、保存／再読込後の順序は未検証である。
- **次に確認すべきこと:** 非連続／連続選択での4操作、Undo／Redo、最上段・最下段混在、selection保持を確認する。

## 2026-08-31 - Property commandの適用失敗伝播

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、layer／effect property command
- **確認できた事実:** layer property valueのvirtual setter、layer／effectのkeyframe・expression復元は失敗可能な対象解決やproperty lookupの結果をcommandの成功状態へ返していなかった。
- **変更:** 既存のbool setter／property存在確認を`lastOperationSucceeded()`へ伝播し、対象消失・不正propertyの初回redoやmacro内復元をUndoManagerが検知できるようにした。
- **価値または懸念:** stale layer／effectに対して履歴だけ進む状態を抑えられる。keyframe setter内部の要素単位失敗、runtime、session reloadは未検証である。
- **次に確認すべきこと:** layer／effect propertyのUndo／Redo、対象削除後の履歴、macro部分失敗、通知とdirty stateを確認する。

## 2026-08-31 - Text commandの適用失敗伝播

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、Text Layer／Text Animator
- **確認できた事実:** Text Layerの`text.value`復元とText Animator stack復元は、対象消失や対象型不一致をvoid適用として扱い、UndoManagerへ成功状態を返していなかった。
- **変更:** 既存`setLayerPropertyValue()`のbool結果と`ArtifactTextLayer`へのdynamic cast結果を`lastOperationSucceeded()`へ伝播した。
- **価値または懸念:** stale layerやText以外のlayerに対して履歴だけ進む状態を抑えられる。Animator内部JSONの妥当性、runtime、session reloadは未検証である。
- **次に確認すべきこと:** Text LayerのUndo／Redo、対象削除後、Text Animatorの対象型不一致、macro部分失敗を確認する。

## 2026-08-31 - 残存Undo commandの状態検証

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、layer／mask／matte／modulation command
- **確認できた事実:** mask、matte、de-click、effect mask、source置換、modulation、layer追加／削除、visibility／lock／solo／shy／blend、rename／parentのcommandは、対象消失やsetter拒否を成功として扱う余地があった。
- **変更:** 既存のbool戻り値、getterによる適用後検証、compositionのcontains判定、modulation snapshotの比較を`lastOperationSucceeded()`へ接続し、失敗時の変更通知を抑制した。追加／削除は挿入・除去後の存在状態も検証する。
- **価値または懸念:** stale対象や部分的なsnapshot適用で履歴・UIだけが進む状態を抑えられる。mask／matteの要素内容、setter内部の副作用、runtime、session reloadは未検証である。
- **次に確認すべきこと:** 各commandの初回redo、Undo／Redo、macro部分失敗、対象削除後、selection・dirty・cache同期を確認する。

## 2026-08-31 - 残存Undo commandとProject itemの失敗伝播

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、整列／opacity／Variant／Project item／in-out／work area
- **確認できた事実:** 整列は複数対象を順に適用し、途中の対象消失やsetter不成立を検知せず通知していた。Project item、in/out、work area、Variantもvoid適用や存在確認なしで履歴操作が成功扱いになり得た。
- **変更:** 整列は全対象をpreflightし、各layerの位置・scaleを検証、失敗時は適用済み分を逆snapshotへ戻す。opacity／Variant／Project item／in-out／work areaは対象、getter、JSON、親・存在状態を`lastOperationSucceeded()`へ伝播した。
- **価値または懸念:** 一括編集の部分適用とProject Viewの履歴・実体の乖離を抑える。opacityはanimation／variant／modulationの評価値が絡むため対象存在までをcommand内の保証とし、外側のpostconditionに委ねた。runtime、session reload、同期callbackの実動作は未検証である。
- **次に確認すべきこと:** 整列の途中失敗rollback、Variant境界、Project itemの親・兄弟順、in/out・work areaの正規化、Undo／Redoとmacro伝播を確認する。

## 2026-08-31 - Resolution Remapの復元結果伝播

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`ChangeCompositionResolutionCommand`
- **確認できた事実:** resolution remapのundo／redoはcompositionやlayerが消えていても処理を打ち切るだけで、サイズ・mask・transform snapshotの復元結果を履歴状態へ返していなかった。
- **変更:** undo／redo前に全snapshot layerをpreflightし、サイズ適用後の一致、mask数、復元対象propertyとkeyframe数を確認する。redo失敗時は旧サイズとbefore snapshotへ戻す。
- **価値または懸念:** remapの部分復元を成功扱いしにくくする。mask要素内容・keyframe値の完全比較、runtime、session reloadは未検証である。
- **次に確認すべきこと:** 異なるaspect比、mask／animated transform、対象layer消失、redo失敗後の再undoを確認する。

## 2026-08-31 - Source localization callbackの結果伝播

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、`Artifact/src/Service/ArtifactProjectService.cppm`
- **確認できた事実:** Localize／RelinkのUndo callbackはasset APIのbool結果を捨ててvoid callbackとして実行していたため、asset managerの失敗でも履歴pushが成功扱いになり得た。
- **変更:** callbackをbool型へ揃え、対象layerのlock失敗または`localizeSourceIdentity()`／`relinkSourceIdentityToShared()`のfalseを`lastOperationSucceeded()`へ返す。成功時だけ変更通知を行う。
- **価値または懸念:** source identityの実体と履歴・ProjectService結果を一致させやすくする。外部asset managerの権限・同名・filesystem状態、runtime、session reloadは未検証である。
- **次に確認すべきこと:** Localize／Relinkのasset拒否、対象消失、Undo／Redo、push予算拒否、source cache同期を確認する。

## 2026-08-31 - Undo復元の部分適用補償

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`Artifact/src/Undo/UndoManager.cppm`、mask／property／Text Animator／Layout command
- **確認できた事実:** mask復元は要素数不一致でも適用済み内容を残す可能性があり、property setter／keyframe／expressionとText Animator stackはsetter呼出しだけで完全適用を確認していなかった。Layout snapshotはrestore callback失敗時にも変更通知を発行していた。
- **変更:** mask、matte、Text Animator、keyframe、value、expressionは適用後のgetter／snapshotを確認し、不一致時は直前スナップショットへ戻してfalseを返す。Layoutは成功時だけ変更通知を発行する。Add／Remove Layerの参照復元・解除とselection復元結果も成功状態へ反映した。
- **価値または懸念:** Undo履歴の失敗だけでなく、部分的に残る実データ変更を抑えやすくする。setter内部の副作用、選択APIの一部適用、runtime、session reloadは未検証である。
- **次に確認すべきこと:** mask要素追加失敗、keyframe metadata不一致、Text Animator型不一致、Layout callback失敗、Remove Layerのselection復元失敗後の再Undoを確認する。

## 2026-08-31 - Layer関係復元とEffect scalarの検証境界

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、Add／Remove Layer、`SetPropertyCommand`
- **確認できた事実:** Add／Remove Layerは本体の追加・削除後にmatte／parent／selectionを復元するため、依存関係の復元失敗を本体成功として履歴へ残す余地があった。Effect scalarは対象存在だけを確認し、editable propertyのsetter拒否を検証していなかった。
- **変更:** Add／Remove Layerで関係状態を事前snapshotし、本体追加／削除後の参照・親・selectionの復元／解除結果を集約する。依存状態の不一致時は本体と関係状態を操作前へ補償し、成功通知を抑制する。Effectの既存editable propertyは適用後の値を比較し、不一致時に旧値へ戻す。custom Effect固有setterはAPI契約を推測せず対象存在判定を維持した。
- **価値または懸念:** レイヤー構造の一部だけが変わった状態と、Effect propertyのsetter拒否をUndo成功と扱いにくくする。構造操作全体の失敗時rollback、custom Effect固有値、runtime、session reloadは未検証である。
- **次に確認すべきこと:** Add／Removeの参照復元失敗時の再試行、editable／custom EffectのUndo／Redo、selection・dirty・cache同期を確認する。

## 2026-08-31 - Stateful Variant commandの永続化境界

- **関連:** `Artifact/include/Undo/UndoManager.ixx`、`CreateVariantCommand`、Undo offload／session history
- **確認できた事実:** `CreateVariantCommand`はUndo時に抽出した`LayerVariant`を`unique_ptr`で保持し、Redoはその実体を再挿入する。一方、既存JSONはlayer ID・name・indexしか保存せず、offload後の復元commandは抽出済みVariantを持たない。
- **変更:** Variant本体を完全に表す既存の公開snapshot／再構成契約がないため、`CreateVariantCommand::canSerialize()`をfalseにしてsession historyとoffloadから除外した。通常session内のUndo／Redo経路は維持する。
- **価値または懸念:** offload後Redoで別のVariantを生成する誤動作を防ぐ。Variantの完全な永続化は未実装で、runtime、session reloadは未検証である。
- **次に確認すべきこと:** Variantのoverride／transform／active indexを含む正式なsnapshot APIを設計レビュー後に検討する。

## 2026-08-31 - Session historyのversion順序検証

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`UndoManager::loadSessionHistory()`
- **確認できた事実:** 履歴envelopeの`savedVersion`と`currentVersion`はいずれも非負整数として検証されていたが、保存済み位置が現在位置より後にある不正な組み合わせを受け入れていた。
- **変更:** `savedVersion <= currentVersion`をload境界で必須にし、dirty判定とstate ID再構成が逆転した履歴を取り込まないようにした。
- **価値または懸念:** 壊れた履歴metadataによるdirty表示・Undo位置の逆転を抑えられる。実ファイル破損、異なるprojectとの取り違え、runtime reloadは未検証である。
- **次に確認すべきこと:** 保存位置へのUndo、Redo後保存、破損envelope、別project履歴の拒否を確認する。

## 2026-08-31 - History envelopeの保存・load対称性

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`saveSessionHistory()`、`OffloadedUndoCommand`
- **確認できた事実:** load側にはtype／labelの長さ制限と、estimatedBytesを`qint64`として扱う境界があったが、save／offload側は同じ入力制限を先に検証していなかった。
- **変更:** save／offload前にもtype 256文字、label 4096文字、estimatedBytesのqint64範囲を検証し、保存直後にloadできないenvelopeを生成しにくくした。
- **価値または懸念:** session historyとdisk offloadのcodec契約を対称化できる。ファイル権限、破損payload、runtime復元は未検証である。
- **次に確認すべきこと:** 境界長のlabel／type、最大サイズ近辺のcommand、save→loadとoffload→Undo／Redoを確認する。

## 2026-08-31 - History envelopeの型と再構成値の照合

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`loadSessionHistory()`
- **確認できた事実:** load側は`toString()`でtype／labelを読み取っていたため、JSON型そのものの不一致を明示的に拒否していなかった。また、保存時のlabelとfactory再構成後のcommand labelを照合していなかった。
- **変更:** type／labelがJSON stringであること、再構成後のlabelがenvelopeと一致すること、estimatedBytesの比較前にsize_tからqint64への変換範囲内であることを検証する。
- **価値または懸念:** 履歴表示だけを改変したpayloadや、整数幅をまたぐcommandサイズを取り込まない。旧形式でlabelが欠落する履歴は互換性影響を受けるため、現行save形式の契約として扱う。
- **次に確認すべきこと:** save→load、label改変、type／label型改変、estimatedBytes境界の拒否をruntimeで確認する。

## 2026-08-31 - JSON整数のqint64境界

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`jsonInteger()`、session／offload codec
- **確認できた事実:** 有限な整数かどうかだけを検証すると、JSONのdouble表現でqint64範囲外の値が`toInteger()`へ渡る可能性がある。
- **変更:** 共通整数decoderでqint64の下限以上・上限未満を検証してから変換する。
- **価値または懸念:** version、index、estimatedBytesなどの履歴metadataで暗黙の丸め・clampに依存しない。JSONの整数表現自体がdoubleであるため、qint64最大値近辺の保存互換性は未検証である。
- **次に確認すべきこと:** qint64境界直前、上限値、上限超過、小数、指数表記をruntimeで確認する。

## 2026-08-31 - Text GizmoのローカルUndo command失敗伝播

- **関連:** `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`、`SetTextAnimatorPropertyCommand`
- **確認できた事実:** Text Gizmoの単一Animator値commandは`lastOperationSucceeded()`を実装せず、property setterの失敗や対象消失でもUndoManagerの既定値trueを返していた。
- **変更:** setterのbool結果と適用後valueを検証し、不一致時は直前valueへ戻して失敗を返す。成功時だけ変更通知を出す。
- **価値または懸念:** ドラッグ中の先行変更が履歴登録失敗後に残る状態を抑えられる。Animator固有setterの副作用とruntimeのUI復元は未検証である。
- **次に確認すべきこと:** Text Animatorの値変更、対象消失、無効path、極小Undo予算でのドラッグ終了をruntimeで確認する。

## 2026-08-31 - ProjectService内ローカルcommandの失敗状態

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`、precompose／effect／group／split Undo command
- **確認できた事実:** service wrapperが`UndoManager::push()`の結果を確認していても、内部commandが失敗時にbaseの既定値trueを返すと、失敗操作が履歴へ残り得た。
- **変更:** precompose、unprecompose、layer／composition effect、group／ungroup、splitのcommandに成功状態を追加し、既存serviceのbool結果または適用後存在確認を履歴へ伝播する。effectのremove APIも対象存在と削除後状態を検証する。
- **価値または懸念:** 対象消失・無効なeffect・初回redo失敗を、UndoManagerが成功操作として保持しにくくなる。group／precomposeの複雑な途中状態、selection、runtime復元は未検証である。
- **次に確認すべきこと:** effect追加／削除／移動、group／ungroup、precompose、splitの対象消失とUndo予算拒否をruntimeで確認する。

## 2026-08-31 - Timeline local commandの復元失敗伝播

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、ripple trim／delete、slide、interpolation
- **確認できた事実:** Timelineのローカルcommandは適用関数のbool結果やsnapshot restoreの失敗を保持せず、対象消失時にもUndoManagerの既定成功値を返していた。
- **変更:** layer snapshot restoreをbool化し、ripple／slide／interpolation commandが対象存在・復元結果・適用関数結果を`lastOperationSucceeded()`へ返す。interpolationは全対象をpreflightしてから適用する。
- **価値または懸念:** 対象消失や部分的な復元不能を履歴成功として進めにくくなる。keyframe値・metadataの完全一致と途中setter失敗時の全体補償は未検証である。
- **次に確認すべきこと:** ripple／slide／interpolationのUndo／Redo、layer削除後、timing lock、極小Undo予算をruntimeで確認する。

## 2026-08-31 - Render Controllerローカルcommandの失敗状態

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、Gizmo／Rig／Puppet／Anchor／Shape corner radius／Motion Path／Live Field command
- **確認できた事実:** Render Controller内のローカルcommandには、対象消失や型不一致時に`UndoCommand`既定の成功状態を返し、変更通知だけを発行するものが残っていた。
- **変更:** 3D／2D Gizmo、複数選択Gizmo、Rigの骨・制御値・ウェイト・ポーズ、Puppet pin、Anchor、Shape corner radius、Motion Path、Live Fieldで適用結果をbool化し、対象不在・型不一致・主要getter不一致を`lastOperationSucceeded()`へ伝播する。成功時だけ変更通知を発行する。
- **価値または懸念:** 対象消失やsetter拒否をUndo成功として扱いにくくする。Gizmoの全transform metadata、Puppet内部のpin存在・deform結果、複数対象の途中失敗補償、runtimeは未検証である。
- **次に確認すべきこと:** 3D／2D Gizmo、Rig／Puppet、Anchor、corner radiusの対象消失・Undo予算拒否・keyframe metadata・deform同期をruntimeで確認する。

## 2026-08-31 - Command Paletteマスク復元の成功状態

- **関連:** `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`、`AddCommandPaletteMaskCommand`
- **確認できた事実:** Command Paletteのマスクsnapshot commandはlayer不在や追加後のmask数不一致を`UndoCommand`既定の成功状態として扱っていた。
- **変更:** 適用をbool化し、対象不在・復元後mask数不一致を`lastOperationSucceeded()`へ伝播する。
- **価値または懸念:** マスク要素の内容比較や途中追加失敗の全体補償は未実装・未検証である。
- **次に確認すべきこと:** Command Paletteのマスク追加、対象消失、極小Undo予算、path内容の復元をruntimeで確認する。

## 2026-08-31 - UndoManager初回redo失敗時の共通補償

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`UndoManager::push()`
- **確認できた事実:** `push()`は初回`redo()`の`lastOperationSucceeded()==false`を検出しても、commandを破棄するだけで逆操作を実行していなかった。setter後のgetter不一致や複数対象の途中失敗では、履歴に入らない部分変更が残る可能性があった。
- **変更:** 初回redo失敗時に`cmd->undo()`を一度実行してからfalseを返し、履歴へ登録されない操作の部分適用を共通境界で補償する。
- **価値または懸念:** 個別commandの補償漏れを減らす。逆操作自体の失敗、非対称な外部I/O、複雑なmacro、runtimeは未検証である。
- **次に確認すべきこと:** 初回redoの対象消失・setter拒否・複数対象途中失敗・Undo予算拒否で、状態・selection・dirty・cacheが変化しないことをruntimeで確認する。

## 2026-08-31 - Undo／Redo失敗時の対称補償とMacro境界

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`MacroUndoCommand`、`UndoManager::undo()`／`redo()`
- **確認できた事実:** 初回pushだけでなく、履歴上のundo／redoが部分適用後に失敗した場合も、失敗した状態を逆方向へ戻す共通処理がなかった。Macroは自身の失敗子と既適用子を補償するため、上位からさらに逆適用すると二重補償になる。
- **変更:** Macroは失敗した子を逆方向へ補償してから既適用子を戻す。UndoManagerは非Macro commandの失敗undo／redoを逆操作で補償し、push時はMacroへの二重undoを避ける。
- **価値または懸念:** 履歴移動失敗時に実データが履歴状態から外れる可能性を抑える。逆操作自体の失敗、外部ファイルI/O、非対称command、selection・cache・dirtyのruntimeは未検証である。
- **次に確認すべきこと:** 個別command／Macroの初回push、undo失敗、redo失敗、途中子失敗で、スタック位置とプロジェクト状態が一致することをruntimeで確認する。

## 2026-08-31 - Command-only呼び出し側の結果伝播

- **関連:** `Artifact/src/Asset/AssetDirectoryModel.cppm`、`Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`
- **確認できた事実:** Asset dropは`MoveAssetFileCommand`のpush結果を見ずにfilesystem状態だけで移動成功を判定していた。Audio Reactive bindingの追加／削除もUndoManagerが拒否した場合にtrueを返していた。
- **変更:** Asset dropはpush成功後だけ移動済みとしてモデル更新し、Audio Reactive bindingの追加／削除はmanagerのpush結果をそのまま返す。
- **価値または懸念:** Undo履歴に登録できなかったcommand-only操作を呼び出し側が成功扱いしにくくする。filesystem race、UI表示、runtimeは未検証である。
- **次に確認すべきこと:** Asset dropの権限／重複先、binding操作のUndo予算拒否、dirty・表示更新をruntimeで確認する。


## 2026-08-31 - Audio Reactive command-only結果の伝播

- **関連:** `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`、Audio Reactive bake／record commit
- **確認できた事実:** bake／record commitはUndoManagerのpush結果を無視して成功メッセージやtrueを返していた。
- **変更:** managerがある場合はpush結果、managerがない場合はcommandの`lastOperationSucceeded()`を返し、失敗時は成功案内へ進まない。
- **価値または懸念:** 履歴登録失敗とAudio Reactive UIの成功結果が食い違いにくくなる。recording state、音響結果、runtimeは未検証である。
- **次に確認すべきこと:** bake／record commitのUndo予算拒否、対象消失、部分keyframe適用、成功メッセージ表示をruntimeで確認する。

## 2026-08-31 - Audio Mixer routing失敗時のUI同期

- **関連:** `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm`、advanced routing callback
- **確認できた事実:** routing editor終了後は、Audio Mixer snapshot commandのpush結果に関係なくcomposition changedとcompact surface refreshを進めていた。
- **変更:** callbackのpush結果を保持し、失敗時はcurrent graphを再表示してdirty／成功状態を確定せず警告する。
- **価値または懸念:** Undo履歴へ登録できなかったrouting変更を成功更新として扱いにくくなる。外部AudioMixer widgetの先行変更補償とruntimeは未検証である。
- **次に確認すべきこと:** routing変更の予算拒否、snapshot不一致、dialog cancel、再表示とdirty状態をruntimeで確認する。

## 2026-08-31 - Template importのUndo transaction境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`importDroppedTemplate()`
- **確認できた事実:** template importは生成した各layerを個別に`push()`していたため、複数layerの途中で予算拒否やredo失敗が起きると、templateが部分的にだけ挿入される可能性があった。
- **変更:** instantiate済みlayerを1つの`MacroUndoCommand`へまとめ、template単位で初回redo・失敗補償・Undo境界を揃える。
- **価値または懸念:** template importを部分的な複数履歴ではなく一操作として扱える。layer順序、selection、template内参照、runtimeは未検証である。
- **次に確認すべきこと:** 複数layer templateのUndo／Redo、途中layer不正、Undo予算拒否、selection・parent／matte参照をruntimeで確認する。

## 2026-08-31 - Nested Macroの補償責務

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`MacroUndoCommand`
- **確認できた事実:** Macroの失敗子を外側Macroが常に逆適用すると、内側Macroが既に実行した失敗補償をさらに反転し、履歴状態から外れる可能性があった。
- **変更:** `handlesFailedOperationCompensation()` を導入し、Macroの入れ子では内側の補償責務を外側が二重実行しないようにした。Undo／Redo両方向で同じ契約を使う。
- **価値または懸念:** template importのようなMacro利用箇所を含め、失敗時の状態復元がMacro階層で一度だけ行われる。逆操作失敗とruntimeは未検証である。
- **次に確認すべきこと:** nested Macroの初回redo、子undo失敗、子redo失敗、Undo／Redo stack位置をruntimeで確認する。

## 2026-08-31 - Animation Layer commandのmanager不在経路

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、Animation Layer context menu
- **確認できた事実:** Animation Layer操作は一度afterへ変更した後にbeforeへ戻してcommandをpushしていたが、UndoManagerが存在しない場合はafterを再適用せず、操作が失われていた。
- **変更:** snapshot commit helperを追加し、managerがある場合はpush結果を確認し、無い場合はafter snapshotを直接復元する。push拒否時はbeforeへ戻す。
- **価値または懸念:** Undo基盤の有無で操作結果が消える不整合を減らす。Animation Layer snapshotの完全内容、selection、runtimeは未検証である。
- **次に確認すべきこと:** add／remove／bakeのmanager不在、Undo予算拒否、Undo／Redo、selection・dirty・cacheをruntimeで確認する。

## 2026-08-31 - Composition Editor layer commandのnull安全化

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、layer visibility／lock／solo／shy／center
- **確認できた事実:** context menuとlayer操作面の一部が`UndoManager::instance()`を無条件参照していた。
- **変更:** 変更前に実体を触らない既存command経路を維持したまま、UndoManagerが利用できない場合は操作を安全に中断するnull-safe guardを追加した。
- **価値または懸念:** 履歴基盤の初期化前にメニュー操作がクラッシュする可能性を下げる。manager不在時の代替編集は意図的に追加していないため、操作は適用されない。
- **次に確認すべきこと:** editor初期化順序、manager不在、push拒否、メニュー再表示をruntimeで確認する。

## 2026-08-31 - Editor／Render WidgetのUndoManager不在経路

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- **確認できた事実:** Paste LayersとComposition CleanupはUndoManagerを無条件参照していた。Render Widgetのドラッグ確定は先にリアルタイム移動を戻してからpushするため、manager不在時はクラッシュし、編集結果も失われる構造だった。
- **変更:** Paste／Cleanupはmanager不在時に安全に失敗を返す。Render Widgetはmanager不在時に確定差分を直接再適用し、通常の変更通知へ進める。
- **価値または懸念:** 初期化順序によるクラッシュとドラッグ編集の消失を抑える。manager不在時はPaste／Cleanupを適用しない方針で、runtimeは未検証である。
- **次に確認すべきこと:** manager不在・push拒否・ドラッグキャンセル、Pasteのselection／index、Cleanupのlayer lockをruntimeで確認する。

## 2026-08-31 - Asset／Playback snapshotのmanager不在経路

- **関連:** `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`、`Artifact/src/Service/ArtifactPlaybackShortcuts.cppm`
- **確認できた事実:** Asset relink／import登録とPlayback marker fallbackは先に実体を変更してからUndoManagerへpushしており、manager不在時に無条件参照してクラッシュする可能性があった。
- **変更:** push前にmanagerの存在を確認し、manager不在またはpush拒否時は既存のrelink／project／marker rollbackへ入るようにした。
- **価値または懸念:** 履歴基盤未初期化時のクラッシュと、先行変更を履歴なしで残す可能性を抑える。filesystem競合、marker復元、runtimeは未検証である。
- **次に確認すべきこと:** import／single relink／batch relink、marker追加・削除・全消去のmanager不在・push拒否・dirty・表示更新をruntimeで確認する。

## 2026-08-31 - Layer Menuのnull-safe command入口

- **関連:** `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`、visibility／lock／solo／shy／mask-to-shape
- **確認できた事実:** Layer Menuの一部command入口がUndoManagerを無条件参照しており、履歴基盤の初期化順序によってクラッシュし得た。
- **変更:** 実体を先行変更しない既存command経路を維持したまま、UndoManagerが存在する場合だけpushするguardを追加した。
- **価値または懸念:** 未初期化時のクラッシュを避ける。manager不在時は操作を適用しないため、fallback編集はruntime設計確認が必要である。
- **次に確認すべきこと:** toggle各種、mask-to-shape、manager初期化前のメニュー操作、push拒否時の再表示をruntimeで確認する。

## 2026-08-31 - Particle Render Widgetのdrag rollback整合性

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、particle emitter／effector drag commit
- **確認できた事実:** particle drag確定はUndoManagerを無条件参照していた。またEffector位置のpush拒否rollbackがEmitterの開始位置を使い、Influence Radiusには誤った開始値名を参照していた。
- **変更:** emitter／direction／effector／radius dragのpushをmanager存在時だけ実行し、Effector rollbackをEffector開始値へ修正した。keyboard nudgeもnull-safe化した。
- **価値または懸念:** manager不在時のクラッシュと、push拒否時に別の粒子要素へ戻してしまう不整合を抑える。particle property setterとcache同期はruntime未検証である。
- **次に確認すべきこと:** emitter／effector／radiusのdrag、Undo予算拒否、manager不在、再描画・粒子cacheをruntimeで確認する。

## 2026-08-31 - EditorのText／Timeline／Center command入口

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、Text編集、Sequence／Match Duration、Center Layer
- **確認できた事実:** これらの一部command入口がUndoManagerを無条件参照していた。Sequence／Matchは先行してtimeline stateを変更してからpushしていた。
- **変更:** Text／Centerはmanager不在時に安全に失敗し、Sequence／Matchはmanagerが存在する場合だけpushし、push拒否時のbefore復元を維持する。
- **価値または懸念:** 履歴基盤未初期化時のクラッシュを避け、先行変更のrollback契約を保持する。manager不在時のSequence／Matchは直接適用が残るため、runtime確認が必要である。
- **次に確認すべきこと:** Text編集、Sequence／Match、Centerのmanager不在・push拒否・selection・timeline cacheをruntimeで確認する。

## 2026-08-31 - Safe DeleteのUndoManager境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、Safe Delete Layers
- **確認できた事実:** Safe Deleteは複数RemoveLayer commandをmacroへまとめていたが、UndoManagerを無条件参照していた。
- **変更:** macroをpushする前にmanager存在を確認し、manager不在またはpush拒否時はselection変更と成功表示へ進まないようにした。
- **価値または懸念:** 履歴基盤未初期化時のクラッシュと、削除成功と誤認したUI更新を避ける。削除対象の依存関係とruntimeは未検証である。
- **次に確認すべきこと:** Safe Deleteのparent／matte／effect依存、macro途中失敗、selection、dirty stateをruntimeで確認する。

## 2026-08-31 - Timeline snapshot callbackの成功状態

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、`TimelineKeyframeSnapshotCommand`
- **確認できた事実:** snapshot commandはvoid callbackのみを保持し、対象消失や復元不能でもUndoManagerの既定成功値を返していた。
- **変更:** snapshot適用関数をpreflight付きboolへ変更し、commandにbool callbackと`lastOperationSucceeded()`を追加した。既存void callbackは互換ラッパーで維持し、Key PatternとKeyframe Area操作をbool callbackへ接続した。
- **価値または懸念:** 主要Timeline操作で対象消失・復元失敗を履歴成功として扱いにくくなる。全callbackのbool化、要素単位のsetter失敗、runtimeは未検証である。
- **次に確認すべきこと:** Key Pattern、Keyframe Area、Timelineのundo／redo、layer削除後、selection復元、極小Undo予算をruntimeで確認する。

## 2026-08-31 - Timeline Curve Tangentのsnapshot成功状態

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、Curve Editor tangent操作
- **確認できた事実:** tangent編集はcurveを先行変更してsnapshot commandへ渡していたが、復元callbackがvoidで対象消失やproperty欠落を成功扱いし得た。
- **変更:** snapshot適用をpreflight付きbool化し、Curve Editor tangentのredo／undo callbackをbool callbackへ接続した。UI refreshは適用結果の確認後も既存順序を維持する。
- **価値または懸念:** curve targetが失われた状態で履歴移動を成功扱いしにくくなる。setter内部の完全検証、selection、runtimeは未検証である。
- **次に確認すべきこと:** Auto／Flat／Linear／Broken／Unified tangentの対象削除、Undo／Redo、push拒否、curve cacheをruntimeで確認する。

## 2026-08-31 - Playhead keyframe snapshotの成功状態

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、Add／Remove Keyframe at Playhead
- **確認できた事実:** Playhead keyframe操作は先行変更後にsnapshot commandへ渡していたが、callbackがvoidで対象消失やproperty復元失敗を伝えられなかった。
- **変更:** Add／Remove Keyframe at Playheadをbool callbackへ接続し、snapshot適用のpreflight結果をUndoManagerへ返す。
- **価値または懸念:** 対象消失時にkeyframe履歴を成功扱いしにくくなる。keyframe setter内部の完全検証、selection、runtimeは未検証である。
- **次に確認すべきこと:** Playhead add／removeのUndo／Redo、対象削除、極小Undo予算、selection・cacheをruntimeで確認する。

## 2026-08-31 - Keyframe Area Valueのsnapshot成功状態

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`applyValueToSelectedKeyframeArea()`
- **確認できた事実:** Keyframe Area Valueはafter snapshotを先行適用してからvoid callback commandへ登録し、snapshot適用失敗を無視していた。
- **変更:** after適用自体をboolで確認し、redo／undo callbackをbool callbackへ接続した。selection復元とUI refreshは既存の順序を保つ。
- **価値または懸念:** 対象propertyが消失した状態でarea value変更を履歴成功として扱いにくくなる。keyframe setter完全検証、selection、runtimeは未検証である。
- **次に確認すべきこと:** area valueの対象削除、Undo／Redo、push拒否、selection・curve cacheをruntimeで確認する。

## 2026-08-31 - Layer Menu残存pushのnull安全化

- **関連:** `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`、quick layer creation、cache policy、proxy quality
- **確認できた事実:** quick layer creationは実体を一度detachしてからtransactionをpushし、cache／proxy property変更もUndoManagerを無条件参照していた。
- **変更:** manager存在を確認してpushし、quick layerはmanager不在またはpush拒否時に既存layer復元へ進む。property変更は安全に中断する。
- **価値または懸念:** 初期化順序によるクラッシュとdetach済みlayerの取り残しを抑える。manager不在時のproperty fallbackは追加していない。
- **次に確認すべきこと:** quick layerのplacement／mask／envelope、cache／proxy変更のmanager不在・push拒否・dirty／selectionをruntimeで確認する。

## 2026-08-31 - Timeline Track Painterローカルcommandの失敗状態

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、ripple／slide／interpolation／roving command
- **確認できた事実:** Track Painter側の重複ローカルcommandは、対象消失やsnapshot復元不能を無視して変更通知を出す経路が残っていた。
- **変更:** timeline snapshot restoreをbool化し、ripple／slide／interpolation／roving commandへ対象・適用件数の成功状態を伝播する。
- **価値または懸念:** Timeline Widget側とTrack Painter側で対象消失時の履歴結果が食い違いにくくなる。keyframe要素単位の完全比較、途中setter失敗時の全体補償、callback型のsnapshot commandは未検証である。
- **次に確認すべきこと:** Track Painterのripple／slide／補間／rovingを対象削除後と極小Undo予算で確認する。

## 2026-08-31 - Timeline snapshot callbackの成功状態拡張

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、Move／Paste／Trajectory／Fringe／Reverse／Set Value
- **確認できた事実:** Timeline snapshot commandの一部callbackはvoidのままで、対象property消失や復元preflight失敗をUndoManagerへ返せなかった。Move Keyframeでは失敗メッセージ用のframe文字列も失敗分岐より後で宣言されていた。
- **変更:** Motion Trajectory、Keyframe Fringe、Move Keyframe、Edit Curve Keyframes、Paste at Playhead、Track PainterのReverse／Set Valueをbool callbackへ接続し、適用結果を履歴境界へ返す。Moveのframe表示値を共通スコープへ移した。
- **価値または懸念:** snapshot対象が消えた場合の誤った履歴成功と、失敗時の表示経路の不整合を抑える。keyframe setter内部の完全検証、選択・cache、runtimeは未検証である。
- **次に確認すべきこと:** 各操作の対象削除、Undo／Redo、push拒否、selection復元、極小Undo予算をruntimeで確認する。

## 2026-08-31 - Track Painterのkeyframe metadata callback

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、Delete／Reverse／Set Value／Set Anchor／Set Color Label
- **確認できた事実:** keyframeの削除、反転、値・anchor・color label変更はsnapshot commandを使っていたが、callbackがvoidで、適用失敗を履歴境界へ返していなかった。
- **変更:** 5操作のredo／undo callbackをboolへ接続し、共通snapshot preflightの結果を`UndoManager`へ伝播する。選択同期の既存順序は維持した。
- **価値または懸念:** keyframe対象消失やproperty欠落を成功履歴として残しにくくする。metadata setter内部の完全比較、selection・cache、runtimeは未検証である。
- **次に確認すべきこと:** metadata変更のUndo／Redo、対象削除、push拒否、selection復元、極小Undo予算をruntimeで確認する。

## 2026-08-31 - Track Painterのkeyframe batch callback

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、Duplicate／Distribute／Repeat Selected Keyframes
- **確認できた事実:** 複製・均等配置・反復の各操作もsnapshot commandへ登録していたが、redo／undo callbackはvoidで、対象propertyのpreflight結果を無視していた。
- **変更:** 3操作をbool callbackへ接続し、既存のselection復元とpush拒否時rollbackを維持した。
- **価値または懸念:** batch keyframe操作の対象消失を履歴成功として扱いにくくする。複数対象のsetter途中失敗、selection・cache、runtimeは未検証である。
- **次に確認すべきこと:** 複製・均等配置・反復の対象削除、Undo／Redo、push拒否、selection復元をruntimeで確認する。

## 2026-08-31 - Track Painterのkeyframe area value callback

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、context menuのSet Keyframe Area Value
- **確認できた事実:** Track Painterのarea value編集は先行適用後にsnapshot commandへ登録していたが、callbackの適用結果を無視していた。
- **変更:** redo／undo callbackをboolへ接続し、area selection復元の既存順序を維持した。
- **価値または懸念:** 対象property消失時の履歴成功を抑える。setter完全検証、selection・cache、runtimeは未検証である。
- **次に確認すべきこと:** area valueの対象削除、Undo／Redo、push拒否、selection復元をruntimeで確認する。

## 2026-08-31 - Track Painterのtangent／range callback

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、Break／Unify Tangents、Transform Selected Keyframes
- **確認できた事実:** tangent metadataと範囲変換もsnapshot commandを使っていたが、callbackがvoidで適用失敗を返していなかった。
- **変更:** 両操作のredo／undo callbackをboolへ接続し、selection同期とpush拒否時rollbackを維持した。
- **価値または懸念:** 対象property消失時の履歴成功を抑える。keyframe setter完全検証、複数対象途中失敗、selection・cache、runtimeは未検証である。
- **次に確認すべきこと:** tangent／range変換の対象削除、Undo／Redo、push拒否、selection復元をruntimeで確認する。

## 2026-08-31 - Track Painterの残存snapshot callback整理

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、Reverse All／Clean Keyframes／Break／Unify Tangents／Transform Selected Keyframes／Keyframe Area Value
- **確認できた事実:** free-function経由のReverse Allとcontext menuのClean／Transformなどにもvoid callbackが残っており、snapshot適用失敗を履歴境界へ返していなかった。
- **変更:** 対象 callback をbool `Operation`へ接続し、既存のselection復元・event処理・before snapshot rollbackを維持した。
- **価値または懸念:** Track Painterの主要snapshot操作で対象消失を成功履歴にしにくくする。keyframe setter完全検証、複数対象途中失敗、selection・cache、runtimeは未検証である。
- **次に確認すべきこと:** 残存するvoid互換callbackの用途を確認し、全操作の対象削除・Undo／Redo・push拒否をruntimeで検証する。

## 2026-08-31 - Timeline snapshot commandのcallback契約固定

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`
- **確認できた事実:** 両ファイルの`TimelineKeyframeSnapshotCommand`利用箇所はすべてbool `Operation`へ移行済みで、void互換コンストラクタの利用箇所はなかった。
- **変更:** 未使用のvoid互換コンストラクタを削除し、snapshot commandのcallback契約をboolへ固定した。
- **価値または懸念:** 将来のsnapshot操作が適用失敗を握りつぶしにくくなる。ABI／runtime／setter内部失敗は未検証である。
- **次に確認すべきこと:** Timeline外への同名command依存がないことを確認し、Undo／Redoと対象消失をruntimeで検証する。

## 2026-08-31 - Layer Panel inline renameのcommand所有権

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`、inline layer rename
- **確認できた事実:** inline renameは`new RenameLayerCommand`をraw pointerで生成し、UndoManagerが未初期化の場合にpushされずリークしていた。manager不在時のrename中断自体は既存方針だった。
- **変更:** command生成を`std::make_unique`へ変更し、managerが存在する場合だけmoveしてpushする。
- **価値または懸念:** 履歴基盤の初期化順序に依存したcommandリークを防ぐ。renameのpush拒否・selection・runtimeは未検証である。
- **次に確認すべきこと:** manager不在／push拒否時のrename、Undo／Redo、inline editorのfocus復帰をruntimeで確認する。

## 2026-08-31 - Matte／Variant／Layer移動commandの所有権統一

- **関連:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`
- **確認できた事実:** matte reference、variant、layer move の一部経路はraw pointerを生成してから、managerが存在する場合だけ`unique_ptr`へ包んでいたため、manager未初期化時にリークし得た。
- **変更:** command生成を`std::make_unique`へ統一し、pushには`std::move`で渡す。既存のmanager不在時の安全な中断とpush成功時のUI更新は維持した。
- **価値または懸念:** Undo基盤の初期化順序に依存したcommandリークを除去する。push拒否時の状態、selection、runtimeは未検証である。
- **次に確認すべきこと:** matte／variant／layer moveのmanager不在・push拒否・Undo／Redoをruntimeで確認する。

## 2026-08-31 - Command raw pointer sweepの完了

- **関連:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`
- **確認できた事実:** Artifact の source 内で検索したraw `new *Command` は、今回対象にしたInspector／Layer Panel経路を含めて残っていない。
- **変更:** matte、variant、layer move、inline renameを`std::make_unique`と`std::move`へ統一した。
- **価値または懸念:** manager不在時の所有権リークを抑える。push拒否時の実体・selection・runtimeは未検証である。
- **次に確認すべきこと:** 別形式のraw allocation、カスタムdeleter、Undo／Redoのruntime挙動を確認する。

## 2026-08-31 - Layer PanelのUndoManager不在フォールバック

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`、Variant picker、inline rename
- **確認できた事実:** Variant切替・Variant作成・inline renameは、UndoManagerが未初期化だとcommandを生成したまま破棄し、操作が無視される経路だった。各commandの`redo()`は対象レイヤーを直接更新できる。
- **変更:** UndoManagerが存在する場合は履歴へpushし、存在しない場合はcommandの`redo()`を一度だけ実行してUIを更新するフォールバックを追加した。
- **価値または懸念:** 履歴基盤の初期化順序にかかわらず、command可能なレイヤー操作が無操作にならない。push拒否時の表示、selection、runtimeは未検証である。
- **次に確認すべきこと:** manager不在・push拒否・Undo／Redo時のVariantとrenameのモデル値、dirty通知、focus復帰をruntimeで確認する。

## 2026-08-31 - Layer Panelの状態command fallback統一

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`、blend mode、visibility／lock／solo／shy
- **確認できた事実:** Layer Panelのblend mode、単一選択トグル、複数選択macro、ショートカットトグルは、UndoManager不在時に操作を破棄していた。これらのcommandはモデルへ直接`redo()`可能だった。
- **変更:** managerが存在すればpushし、存在しない場合はcommandまたはmacroの`redo()`を一度だけ実行し、成功時だけUIを更新するよう統一した。
- **価値または懸念:** 履歴基盤の初期化順序による無操作と編集UIの取り残しを抑える。manager不在時は履歴が残らないため、Undo／Redo・dirty通知・runtimeは未検証である。
- **次に確認すべきこと:** 各トグルのモデル値、macro途中失敗時の補償、表示更新、UndoManager初期化前後の通知をruntimeで確認する。

## 2026-08-31 - Layer Panel matte helperのfallback

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`、matte type／enabled／opacity／blend／fit編集
- **確認できた事実:** matte編集helperはcommandを作成しても、UndoManagerがない場合に即座に`false`を返して編集を破棄していた。
- **変更:** managerがあれば履歴へpushし、なければ`ChangeLayerMatteReferencesCommand::redo()`を実行して`lastOperationSucceeded()`を返すようにした。
- **価値または懸念:** matte編集の適用結果と呼び出し側の成功判定を一致させる。履歴なし時のdirty通知、selection、runtimeは未検証である。
- **次に確認すべきこと:** 5種類のmatte編集でモデル値、通知、UndoManager初期化前後のUI反映をruntime確認する。

## 2026-08-31 - Empty Macroの履歴登録防止

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`MacroUndoCommand`
- **確認できた事実:** 子 command が0件のmacroでも`redo()`／`undo()`／`canSerialize()`が成功相当になり、状態を変更しない空履歴を作成できた。
- **変更:** 空macroの`redo()`／`undo()`を失敗扱いにし、`canSerialize()`もfalseにした。
- **価値または懸念:** batch対象が全て無効・消失した場合に、空のUndo履歴や保存対象を残さない。既存セッションに含まれる空macroのruntime復旧は未検証である。
- **次に確認すべきこと:** 空macroのpush拒否、非空macroの部分失敗補償、Undo／Redo、session serializeをruntimeで確認する。

## 2026-08-31 - Undo memory budgetの実行前拒否

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`UndoManager::push()`、`createCommand()`
- **確認できた事実:** `maxMemoryBytes`が0、またはcommandの推定サイズより小さい場合、push後の`enforceBudget()`が実行済みcommandを履歴から削除しても、pushはtrueを返し得た。
- **変更:** 保持不能なcommandを初回`redo()`前に拒否し、session復元時の`createCommand()`でも同じ容量条件を検証する。
- **価値または懸念:** 編集実体が適用済みなのにUndo履歴だけ消える状態を防ぐ。既存履歴のbudget縮小、offload、runtimeは未検証である。
- **次に確認すべきこと:** maxMemoryBytes=0、単一command超過、既存履歴の入替、session loadをruntimeで確認する。

## 2026-08-31 - Undo sessionの部分保存防止

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`saveSessionHistory()`
- **確認できた事実:** session保存は非シリアライズ commandを黙ってスキップしながら、履歴versionをそのまま保存していたため、保存成功後の履歴が実際より少なくなり得た。
- **変更:** undo stackにnullまたは非シリアライズ commandが含まれる場合、entry生成前に保存を失敗させる。
- **価値または懸念:** 「保存成功」と「復元可能な履歴全体」の不一致を防ぐ。非シリアライズ commandを含む既存プロジェクトの保存UXとruntimeは未検証である。
- **次に確認すべきこと:** serializable／non-serializable混在履歴、offloaded履歴、保存失敗時の既存ファイル保持、session loadをruntimeで確認する。

## 2026-08-31 - Undo memory accountingの飽和加算

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`UndoManager::Impl::stackBytes()`
- **確認できた事実:** stack内commandの推定サイズを単純加算しており、異常に大きい値の組み合わせでは`size_t` wrapによってbudget超過を見逃す可能性があった。
- **変更:** 合計が`size_t`上限を超える場合は上限値を返す飽和加算へ変更した。
- **価値または懸念:** memory budget enforcementが異常値で無効化されることを防ぐ。実際の異常command生成・offload・runtimeは未検証である。
- **次に確認すべきこと:** 通常サイズ、単一超過、合計超過、offload後のmemory accountingをruntimeで確認する。

## 2026-08-31 - Undo offload cleanupのセッション分離

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、offload file lifecycle
- **確認できた事実:** offload fileが固定の`undo_<index>.json`で、cleanupが同一directoryの`undo_*.json`を全削除していたため、別manager／別アプリの履歴ファイルを巻き込む可能性があった。
- **変更:** manager生成時のUUIDをoffload file名とcleanup globへ含め、自分のsessionが所有するファイルだけを削除するようにした。
- **価値または懸念:** 共有directoryで別履歴を破壊するリスクを抑える。既存旧形式孤児ファイルの移行・複数プロセス・runtimeは未検証である。
- **次に確認すべきこと:** 同一directoryの複数manager、offload／clearHistory／destructor、save／load後のfile lifecycleをruntimeで確認する。

## 2026-08-31 - Undo state IDの衝突回避

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`UndoManager::Impl::allocateStateId()`
- **確認できた事実:** state IDが`INT64_MAX`到達後に1へ単純 wrapしており、既存履歴IDやsaved/current versionと衝突し得た。
- **変更:** 新しいstate IDを割り当てる際、undo／redo stackの既存IDとsaved/current versionを避ける探索を追加した。
- **価値または懸念:** 長時間実行時の履歴version衝突と未保存判定の誤りを抑える。破損状態でID空間が枯渇した場合とruntimeは未検証である。
- **次に確認すべきこと:** 通常push、undo／redo、session load、ID wrap近傍のversion判定をruntimeで確認する。

## 2026-08-31 - Undo／Redo履歴のbudget統合

- **関連:** `Artifact/src/Undo/UndoManager.cppm`、`enforceBudget()`、`currentMemoryBytes()`
- **確認できた事実:** memory／entry budgetの超過判定がundo stackだけを対象にしており、undo後のredo stackが設定上限の外で保持され得た。
- **変更:** undo／redo両stackを合算してentry数・memory・memory pressureを計算し、超過時は古いredo、次に古いundoから削除する。合算はoverflowしない飽和処理にした。
- **価値または懸念:** redoを大量に積んだ状態でも履歴全体が設定budgetを超えない。budget縮小時の履歴淘汰順、offload、runtimeは未検証である。
- **次に確認すべきこと:** undo／redo連続、分岐編集によるredo破棄、budget変更、offload併用時の履歴数とmemory表示をruntimeで確認する。

## 2026-08-31 - Timeline キーフレームジャンプの責務境界

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、`Artifact/include/Widgets/Timeline/ArtifactTimelineKeyBinding.ixx`
- **確認できた事実:** `JumpToFirst/Last/Next/PreviousKeyframe` は `ShortcutBindings` と Timeline の action resolver に登録済みで、親 `ArtifactTimelineWidget::handleTimelineAction()` が `jumpToKeyframeHit()` / `jumpToFirstKeyframe()` / `jumpToLastKeyframe()` を実行する。これらは選択キーフレームのフレーム収集、ラップ、playhead・viewport・keyframe state 同期まで担っている。
- **判断:** 子の `ArtifactTimelineTrackPainterView::keyPressEvent()` に同じジャンプ処理を追加する必要はなく、追加すると選択範囲・ラップ規則・表示同期の二重実装になる。Timeline の未実装候補を静的検索するときは、子 widget の直接実装だけでなく親 widget の event propagation と action handler を先に確認する。
- **価値または懸念:** DCC 操作のショートカットを単一の command/navigation owner に保ち、子ビューが独自の seek や current-frame 更新を行って状態不整合を起こすリスクを抑える。実際の Qt focus/event propagation は runtime 未検証。
- **次に確認すべきこと:** Timeline child focus 時の Ctrl+PageUp/Down、選択 keyframe の有無、先頭／末尾での wrap、viewport center、graph editor focus 競合を runtime で確認する。

## 2026-08-31 - Timeline Bake の一操作 Undo 境界

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、Animation Layer の Bake 操作
- **確認できた事実:** 複数選択した layer の bake は、各 layer の before/after snapshot を個別の `AnimationLayerStackSnapshotCommand` として順に `UndoManager::push()` していたため、ユーザーの一回の Bake 操作が複数の Undo 単位に分かれていた。
- **変更:** 変更のあった全 layer の snapshot command を `MacroUndoCommand("Bake Animation Layers")` に収集し、Bake 完了後に一度だけ push するようにした。UndoManager がない場合は従来どおり直接適用し、macro push が拒否された場合は変更件数を 0 として UI の成功表示を出さない。
- **価値または懸念:** 複数 layer の Bake が一回の Undo で戻せる。macro の初回 redo、途中 layer の失敗補償、選択・cache・runtime表示は未検証である。
- **次に確認すべきこと:** 複数 layer の Bake → Undo/Redo、Undo budget 拒否、locked layer 混在、保存／再読込後の macro 履歴を runtime で確認する。

## 2026-08-31 - Timeline Slide の UndoManager 不在フォールバック

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、Alt+Left/Right の Slide Clips
- **確認できた事実:** 親 Timeline と右ペインの両方で、複数 layer の Slide macro は UndoManager が存在するときだけ `push()` され、manager が未初期化の場合は macro を破棄したままイベントを受理していた。そのため履歴基盤の初期化前には、ユーザー操作が無操作になっていた。
- **変更:** manager がある場合は従来どおり macro を履歴へ登録し、ない場合は macro の `redo()` を一度だけ実行して `lastOperationSucceeded()` を結果に使う fallback を両経路へ追加した。UndoManager が存在する場合の一操作 Undo 境界は維持した。
- **価値または懸念:** manager の初期化順序による Slide 操作の無操作を防ぐ。manager 不在時は履歴が残らず、複数 layer の途中失敗補償、locked layer 混在、selection／dirty／runtime表示は未検証である。
- **次に確認すべきこと:** manager 有無それぞれで Alt+Left/Right、Shift幅、複数 layer、locked layer、Undo／Redo、表示更新を runtime で確認する。

## 2026-08-31 - Property Editor の manager 不在編集フォールバック

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、Text Animator／複数レイヤー Opacity 編集
- **確認できた事実:** Text Animator の commit は `UndoManager` がない場合を失敗として snapshot を復元し、複数レイヤー Opacity の commit も manager がない場合に `recorded=false` のまま old value を再設定していた。両方とも生成した command の `redo()` はモデルへ直接適用できる。
- **変更:** manager が存在する場合は pushし、存在しない場合は command の `redo()` と `lastOperationSucceeded()` を使う fallback に変更した。push／redo が失敗した場合だけ従来の snapshot／old value 復元を行う。
- **価値または懸念:** Undo 基盤の初期化順序によって Property 編集が無操作になる状態を解消する。manager 不在時は履歴が残らず、複数対象の途中失敗、dirty／通知、runtime は未検証である。
- **次に確認すべきこと:** manager 有無、複数 text layer、Opacity の複数選択、push拒否、Undo／Redo、selection と Property UI 更新を runtime で確認する。

## 2026-08-31 - Shared Property Reset の direct apply 整合性

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`、Property Reset handler
- **確認できた事実:** keyframe を持つプロパティの Reset で UndoManager がない場合、`clearKeyFrames()` だけが実行され、editor 表示は default 値へ更新される一方、プロパティの実値は reset 前のまま残っていた。
- **変更:** manager 不在の direct path でも keyframe を消去した後に `propertyPtr->setValue(defaultValue)` を実行し、既存の layer property animation 通知を発行するようにした。UndoManager がある場合の macro（keyframe reset＋value reset）は変更していない。
- **価値または懸念:** UI表示とモデル値の乖離を防ぐ。component property、animatable flag、dirty/cache、runtime は未検証である。
- **次に確認すべきこと:** keyframe 付き scalar／text／component property の Reset、manager 有無、Undo／Redo、再描画、保存／再読込を runtime で確認する。

## 2026-08-31 - Composition Editor の destructive batch fallback

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、Safe Delete Layers／Keyframe Cleanup
- **確認できた事実:** 確認ダイアログ後の複数 layer 削除と冗長 keyframe 削除は macro を構築するが、UndoManager がない場合を `!manager` として失敗扱いにし、macro を実行せず終了していた。
- **変更:** manager が存在する場合は履歴へ pushし、存在しない場合は macro の `redo()` と `lastOperationSucceeded()` を使って直接適用するようにした。適用失敗時だけ削除後の selection 更新や成功表示へ進まない。Keyframe Cleanup の失敗文言も「Undo履歴へ記録できない」から「適用できない」へ修正した。
- **価値または懸念:** 確認済みの destructive 操作が履歴基盤の初期化順序だけで無操作になる状態を防ぐ。manager 不在時は履歴が残らず、削除対象の途中失敗補償、selection／dirty／cache、runtime は未検証である。
- **次に確認すべきこと:** manager 有無で Safe Delete／Keyframe Cleanup、キャンセル、locked layer、途中 macro failure、Undo／Redo、再描画を runtime で確認する。

## 2026-08-31 - Composition Editor の layer command fallback

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、layer context menu／Center Layer／current layer visibility
- **確認できた事実:** コンテキストメニューの表示、ロック、Solo、Shy、Center Layer、および current layer visibility は UndoManager が存在する場合だけ command を実行し、manager 不在時は操作を破棄していた。
- **変更:** command を先に `std::make_unique` で生成し、manager があれば moveして push、なければ `redo()` を実行する fallback を追加した。Center Layer は direct redo の成功状態も確認して、失敗時を成功扱いにしない。
- **価値または懸念:** layer の基本操作が履歴基盤の初期化順序に依存して無操作になる状態を防ぐ。manager 不在時は履歴が残らず、push拒否時の表示、dirty／selection、runtime は未検証である。
- **次に確認すべきこと:** manager 有無で各 context menu 操作、3D／2D layer、lock状態、Center Layer、Undo／Redo、表示更新を runtime で確認する。

## 2026-08-31 - Composition Editor Auto Stagger の direct snapshot fallback

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、Auto Stagger
- **確認できた事実:** Auto Stagger は before/after の layout snapshot と `restore(state)` を準備していたが、UndoManager がない場合は push 条件が false となり、確認済みの timing 変更を適用せず終了していた。
- **変更:** manager がある場合は従来どおり `LayoutSnapshotCommand` を pushし、ない場合は同じ `restore(afterState)` を直接実行するようにした。適用結果が false の場合だけ警告を出す。失敗文言も履歴記録限定から適用失敗へ一般化した。
- **価値または懸念:** batch timing 編集が履歴基盤の初期化順序で無操作になる状態を防ぐ。direct path の dirty/cache、衝突時の実値、runtime は未検証である。
- **次に確認すべきこと:** manager 有無、layer collision、locked layer、キャンセル、Undo／Redo、保存／再読込を runtime で確認する。

## 2026-08-31 - Composition Editor の snapshot batch fallback 拡張

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、Sequence Layers End-to-End／Match Layer Duration／Adaptive Text Fit
- **確認できた事実:** 3つの確認済み batch 操作は before/after snapshot と復元関数を持つが、UndoManager 不在時は `push()` 条件が false となり、計算済みの変更を適用せず終了していた。
- **変更:** manager がある場合は `LayoutSnapshotCommand` を pushし、ない場合は同じ復元関数へ after snapshot を渡して direct apply するようにした。direct apply または push が失敗した場合は before snapshot を再適用する。
- **価値または懸念:** timing／font-size の batch 操作が履歴基盤の初期化順序で無操作になる状態を防ぐ。snapshot復元関数の部分対象欠落、dirty/cache、runtime は未検証である。
- **次に確認すべきこと:** manager 有無、locked layer、複数対象、キャンセル、適用失敗、Undo／Redo、保存／再読込を runtime で確認する。

## 2026-08-31 - Quick Replace Sources の direct snapshot fallback

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、Quick Replace Selected Sources
- **確認できた事実:** source replacement は互換性・layer順／名前対応・確認ダイアログ・before/after snapshot を備えていたが、UndoManager 不在時は `push()` 条件が false となり、確認済みの置換を適用せず終了していた。
- **変更:** manager がある場合は `LayoutSnapshotCommand` を pushし、ない場合は既存の `restore(afterState)` を direct apply として実行するようにした。部分対象の失敗は既存の `allSucceeded` 結果で警告扱いにする。
- **価値または懸念:** source replacement が履歴基盤の初期化順序で無操作になる状態を防ぐ。direct path の asset identity／cache／dirty、部分失敗時の復元、runtime は未検証である。
- **次に確認すべきこと:** image／video／audio／SVG の互換性、複数 mapping、locked layer、manager 有無、部分失敗、Undo／Redo、保存／再読込を runtime で確認する。

## 2026-08-31 - Composition Cleanup の direct snapshot fallback

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`applyCompositionCleanupCandidate()`
- **確認できた事実:** Composition Cleanup は対象の before 値を検証し、`LayoutSnapshotCommand` 用の before/after と復元関数を構築していたが、UndoManager がない場合は常に false を返し、候補適用を実行しなかった。
- **変更:** manager がある場合は既存 command を pushし、ない場合は after snapshot を復元関数へ渡して直接適用するようにした。direct apply が失敗した場合は before snapshot を再適用する。
- **価値または懸念:** 検証・プレビュー・適用確認を経た cleanup 操作が manager 初期化順序で無操作になる状態を防ぐ。復元関数の対象欠落、dirty/cache、runtime は未検証である。
- **次に確認すべきこと:** spacing cleanup の複数 layer、locked／変更済み layer、適用失敗、Undo／Redo、再描画、保存／再読込を runtime で確認する。

## 2026-08-31 - Composition Cleanup の適用結果表示整合

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`showCompositionCleanupDialog()`
- **確認できた事実:** cleanup dialog の適用側は `applyCompositionCleanupCandidate()` の戻り値を無視し、command／direct apply の失敗時も controller overlay を常に `Composition Cleanup Applied` と表示していた。
- **変更:** 戻り値を `applied` として受け、成功時だけ Applied 表示と drop ghost preview の消去を行い、失敗時は Failed 表示にした。
- **価値または懸念:** 実体の状態とUIフィードバックの不一致を防ぐ。失敗時の overlay 継続方針、runtime表示、controller状態は未検証である。
- **次に確認すべきこと:** manager拒否、direct restore失敗、対象layer消失、再試行時の overlay と preview cleanup を runtime で確認する。


## 2026-08-31 - Template Library importの取引境界

- **関連:** `Artifact/src/Widgets/ArtifactTemplateLibraryWidget.cppm`、`applySelectedToCurrentComposition()`
- **確認できた事実:** Template Libraryは生成した各layerを個別に`push()`し、push結果を見ずに追加件数を返していたため、複数layerの途中失敗で部分挿入と誤った件数表示が起こり得た。
- **変更:** 生成layerを1つの`MacroUndoCommand`へまとめ、全layerが履歴へ登録できた場合だけ追加件数を返す。失敗時は0件とエラー文字列を返す。
- **価値または懸念:** UIの追加件数とUndo履歴・実体の境界をtemplate単位で一致させる。layer順序、selection、template内参照、runtimeは未検証である。
- **次に確認すべきこと:** 複数layer templateの初回redo失敗、Undo予算拒否、Undo／Redo、selection・parent／matte参照をruntimeで確認する。

## 2026-08-30 - 3D パーティクルレイヤーがうまく描画できない問題を最小修正

- 関連: Artifact/src/Layer/ArtifactParticleLayer.cppm:470-484, Artifact/include/Layer/ArtifactParticleLayer.ixx:181-186, Artifact/src/Layer/ArtifactParticleLayer3D.cppm, Artifact/docs/MILESTONE_3D_PARTICLE_2026-08-29.md, Artifact/src/Render/ArtifactIRenderer.cppm:589, 1279-1303, 1483, Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:8913
- 事実: 観察して判明した「3D パーティクル描画の真の問題」は (1) ArtifactParticleLayer::draw() (L441) は is3D() を見ずに常 に transformParticleRenderData(lodData, globalTransform, opacity()) (L472) を呼んで 2D QTransform で (px, py, vx, vy) を map し、(2) transformParticleRenderData は QTransform ベースで v.pz/v.vz は破壊しないが 2D map の safeCoordinate() で px/py/vx/vy を上書きし、(3) 結果として Composition 側で set3DCameraMatrices() 経由で 3D view/proj が particleViewMatrix_/particleProjMatrix_ に配線されても、Particle の world 座標が 2D 扱いになり 3D camera orbit 時に立体的に動かないこと。ArtifactParticle3DLayer は ArtifactParticleLayer を継承し draw() オーバーライドを持たないため親 draw() をそのまま通る (MILESTONE_3D_PARTICLE_2026-08-29 の 2026-08-30 追補方針「2D/3D を別 class で分離、3D は setIs3D(true) を明示、JSON migration」)。
- 対応: ArtifactParticleLayer.cppm:470-484 で is3D() による分岐を追加し、3D パーティクルは transformParticleRenderData をスキップして lodData をそのまま drawParticles に渡す。2D パーティクルは従来通り transformParticleRenderData で 2D QTransform を適用。AGENTS.md 2026-08-15「D3D12 / Diligent backend 触るときは慎重」「QImage の本流投入禁止」「QPainter::CompositionMode による合成実装禁止」を守り、.cppm のみの変更で C++20 module purview には触らず、.ixx 宣言の追加なし (C++20 module purge リスク最小化)。L3634-3647 の ArtifactParticleDebugLayer::draw() 内の呼び出しは触らない (DebugLayer は is3D()=false 想定で、AGENTS.md の変更範囲最小化原則に従う)。
- 価値: Composition 側で既に配線されている particle3DCameraActive_ 経路が活かされるようになり、3D camera orbit 時にパーティクル billboard が立体的に動くはず。LayerType::Particle3D で新規作成されたパーティクルレイヤーと、JSON migration で is3D=true になった既存プロジェクトの表示が両立する。AGENTS.md に従いビルドと runtime 受入れはユーザー指示待ち。
- 未検証: (1) 3D camera orbit 時に Particle billboard が立体的に動くか (2) 既存 2D particle プロジェクトが回帰しないか (3) AOV (emission/normal) gate 経由で Particle が乗るかは依然未確認 (MILESTONE_3D_PARTICLE_2026-08-29 P4-2 以降の作業)。今回修正で「lodData.particles.empty() チェック」のパスで v.px/v.py が 2D QTransform に潰されなくなるが、lodData 自体が pz/vz を含むかは ParticleSystem 側の生成実装に依存。AGENTS.md のルールにより本 commit は cpppm のみ、CMake 変更なし、header 変更なし、module import 追加なし。
**2026-08-31 — Composition Editor inline text fallback**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` のインライン文字編集。
- 事実: `commitTextEditorValue()` は UndoManager がない場合、通常テキストとソーステキストのどちらも編集を適用せず失敗していた。
- 対応: ソーステキストは既存の `setSourceTextAtFrame()`、通常テキストは既存の `setLayerPropertyValue()` で直接適用し、適用後の値を確認する経路に整理した。
- 価値/懸念: 履歴サービスが利用できない限定状態でも編集操作を失わない。ランタイムでの IME、キーフレーム時刻、通知経路の統合は未検証。
- 次に確認: UndoManager 無効時のインライン編集、UndoManager 復帰後の履歴境界、既存キーフレームの編集結果を実機で確認する。

**2026-08-31 — Composition Editor visibility shortcut result handling**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` の現在レイヤー可視状態ショートカット。
- 事実: 既存の Undo コマンドを実行していたが、`push()` または直接 `redo()` の成否を呼び出し側で確認していなかった。
- 対応: UndoManager 経由と直接適用の両方で成功状態を記録し、失敗時は後続処理を行わないようにした。
- 価値/懸念: 履歴追加失敗を成功操作として扱う曖昧さを減らす。ショートカットの実機入力と再描画通知は未検証。
- 次に確認: UndoManager の容量制限時とサービス終了時に、可視状態と履歴表示が一致することを確認する。

**2026-08-31 — Inspector mask context actions without undo service**

- 関連: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` のマスク一括編集・並べ替え。
- 事実: マスクの before/after を組み立てていたが、UndoManager がない場合は `MaskEditCommand` / `MoveMaskCommand` を実行せず無操作になっていた。
- 対応: 既存のレイヤー mask API で snapshot を直接適用し、並べ替えは `moveMask()` の成否を確認する経路を追加した。
- 価値/懸念: 履歴サービスの限定的な不在でも Inspector 操作を失わない。直接 snapshot の個別フィールド一致検証は未実施。
- 次に確認: マスクの追加・無効化・パスモード変更・上下移動を UndoManager 無効状態で実機確認する。

**2026-08-31 — Transform Gizmo release without undo service**

- 関連: `Artifact/src/Widgets/Render/TransformGizmo.cppm` の複数レイヤー変形ドラッグ確定。
- 事実: ドラッグ中に変形を直接反映した後、UndoManager がない場合も `cancelInteraction()` を呼び、確定操作を取り消していた。
- 対応: UndoManager がある場合だけ push 失敗時にキャンセルし、サービスがない場合は既に反映済みの変形を保持するようにした。
- 価値/懸念: Undo 履歴サービスの不在で変形が消える問題を避ける。直接変形の dirty/通知経路は未検証。
- 次に確認: 単一・複数レイヤーの移動、回転、スケールを UndoManager 無効状態で確定し、保存内容と表示が一致することを確認する。

**2026-08-31 — Motion path editing without undo service**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` のモーションパス位置・補間編集。
- 事実: 編集時に transform のキーフレームを先に変更しているのに、UndoManager がない場合も before 状態へ戻していた。
- 対応: UndoManager がある場合だけ push 失敗時に補償復元し、サービス不在時は既に適用された変更を保持するようにした。
- 価値/懸念: パス編集を Undo 履歴サービスの有無で失わない。通知・キャッシュ無効化のランタイム挙動は未検証。
- 次に確認: 位置キー追加・削除・補間変更と複数選択削除を UndoManager 無効状態で確認する。

**2026-08-31 — Render Controller drag commit fallbacks**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` のタンジェント、カメラ POI、Past Plane、Transform Field、Gizmo 確定処理。
- 事実: これらもドラッグ中に状態を直接変更した後、UndoManager 不在を push 失敗と同じ扱いにして before 状態へ復元していた。
- 対応: UndoManager が存在する場合だけ push 失敗時に復元し、不在時は直接反映済みの変更を保持するようにした。
- 価値/懸念: 描画編集の確定が履歴サービスの有無に依存しなくなる。各編集の通知・dirty 状態はランタイム未検証。
- 次に確認: 各ドラッグ種別を UndoManager 無効状態で確定し、再描画・保存・再読込の整合を確認する。

**2026-08-31 — Layer Menu direct property and layout fallbacks**

- 関連: `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm` のキャッシュ設定、Proxy 品質、整列・分布操作。
- 事実: キャッシュ／Proxy は UndoManager がないと値を変更せず、整列・分布は座標を直接変更した後に UndoManager 不在を理由に before 状態へ戻していた。
- 対応: 前者は既存の `setLayerPropertyValue()` を使い、後者は UndoManager が存在する場合だけ push 失敗時に復元するようにした。
- 価値/懸念: メニュー操作が履歴サービスの一時的不在で無操作・誤復元にならない。メニュー表示と保存後の実機確認は未実施。
- 次に確認: キャッシュ／Proxy 設定と整列・分布の直接適用、再描画、保存・再読込を確認する。

**2026-08-31 — Render Controller explicit 3D transform fallbacks**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の 3D 変形・アンカー操作。
- 事実: 変形・アンカーを先に直接反映した後、UndoManager 不在を push 失敗と同一視して before 状態へ戻していた。
- 対応: UndoManager が存在する場合だけ push 失敗時に補償復元するようにした。
- 価値/懸念: 3D 編集 API が履歴サービスの有無で編集結果を失わない。ランタイムの dirty/描画通知は未検証。
- 次に確認: 3D 変形、アンカー中央化、リセット操作を UndoManager 無効状態で確定し、表示と保存内容を確認する。

**2026-08-31 — Puppet and shape direct-edit fallbacks**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の Puppet Pin と Shape Corner Radius 操作。
- 事実: 値を先に直接変更した後、UndoManager 不在を push 失敗と同じ扱いにして before 値へ戻していた。
- 対応: UndoManager が存在する場合だけ push 失敗時に復元し、不在時は直接変更を保持するようにした。
- 価値/懸念: ピンの回転・深度・重みと角丸変更が履歴サービス不在で消えない。Puppet tool の通知・描画更新はランタイム未検証。
- 次に確認: 各ピン操作と角丸操作を直接適用し、表示・保存・再読込の整合を確認する。

**2026-08-31 — Rig editing direct-commit fallback**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の Rig Bone、Control、Skin Weight、Pose、Puppet、Shape Path 編集。
- 事実: これらは編集中に状態を直接変更してから Undo コマンドを登録する構造だが、UndoManager 不在を登録失敗として扱い before 状態へ復元していた。
- 対応: UndoManager がない場合は push 成功相当として扱い、登録失敗時だけ rollback するようにした。
- 価値/懸念: リグ編集の確定結果を履歴サービスの有無で失わない。通知・キャッシュ・変形表示の実機確認は未実施。
- 次に確認: Bone/Control、Weight/Pose、Puppet、Shape Path を直接確定し、保存・再読込と undo 復帰後の挙動を確認する。

**2026-08-31 — Shape conversion and path transaction fallback**

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm` と `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の Shape Conversion、Mask Edit Transaction、Pending Shape Path Creation。
- 事実: いずれも形状・マスク・パスを先に直接変更してから Undo コマンドを登録する構造で、UndoManager 不在時は rollback していた。
- 対応: UndoManager がない場合は直接変更を保持し、登録失敗時だけ rollback するようにした。
- 価値/懸念: 形状編集の確定結果を履歴サービスの有無で失わない。選択状態・キャッシュ・dirty 通知はランタイム未検証。
- 次に確認: Polygon/Path 変換、マスク編集確定、未完了パス確定を直接適用して確認する。

**2026-08-31 — Layer editor shape transaction fallback**

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm` の Polygon/Path 編集、角丸、Shape Operator、Mask Transaction。
- 事実: 直接変更後の rollback 条件に UndoManager 不在が含まれており、また Mask rollback は元より多いマスクを完全には削除していなかった。
- 対応: UndoManager 不在時は変更を保持し、push 失敗時だけ rollback するように変更。Mask rollback は snapshot 全体を再構築し、状態もクリアするようにした。
- 価値/懸念: 形状・マスク編集の確定と失敗時の復元境界が明確になる。選択・キャッシュ・実機入力は未検証。
- 次に確認: 各 Shape 編集、マスク頂点編集、Undo 容量超過時の復元を確認する。

**2026-08-31 — Asset Browser mutation fallbacks**

- 関連: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` の Import、Relink、Rename、Delete。
- 事実: Import 後の登録は UndoManager 不在時にプロジェクトから削除され、Relink は先に行った変更を UndoManager 不在時に戻し、Rename/Delete は UndoManager 不在時に no-op になっていた。
- 対応: Import は既存の project API で登録、Relink は直接変更を保持、Rename/Delete はコマンドの `redo()` を直接実行して成否を確認するようにした。
- 価値/懸念: Asset 操作が履歴サービスの一時的不在で消えない。Delete の復元用バックアップは UndoManager 不在時には保持されないため、実機での安全確認が必要。
- 次に確認: Import/Relink/Rename/Delete とシーケンス素材の登録、保存・再読込を確認する。

**2026-08-31 — Inspector SurfaceFX element fallback**

- 関連: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` の SurfaceFX 要素操作。
- 事実: 追加・複製・削除・上下移動で after snapshot を作成していたが、UndoManager 不在時は snapshot command を実行せず選択インデックスだけ戻していた。
- 対応: UndoManager がない場合は同じ snapshot command の `redo()` を直接実行し、適用結果を確認するようにした。
- 価値/懸念: SurfaceFX 要素の編集が履歴サービス不在で no-op にならない。SurfaceFX の描画更新と選択 UI はランタイム未検証。
- 次に確認: 要素の追加・複製・削除・並べ替えと undo/redo 後の選択状態を確認する。

**2026-08-31 — Layer Editor state toggle fallback**

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm` の Visibility / Lock / Solo 切替。
- 事実: この操作だけは UndoManager がない場合に即時 return し、状態変更が無操作になっていた。
- 対応: 既存の layer setter を直接呼び、反映後の値を確認する fallback を追加した。
- 価値/懸念: Layer Editor の基本状態切替が履歴サービス不在で使えなくならない。Solo の他レイヤー連動は既存 command と同様に個別 layer setter の範囲で、ランタイム未検証。
- 次に確認: Visibility / Lock / Solo の直接切替と UI 状態同期を確認する。

**2026-08-31 — Layer editor command postconditions**

- 関連: `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm` の専用 Shape command 群。
- 事実: layer/type が失われた場合でも command の既定 `lastOperationSucceeded()` が成功を返し、UndoManager が壊れた対象を履歴へ積む可能性があった。
- 対応: Polygon、Path、Corner Radius、Star Inner Radius、Shape Conversion、Shape Operator command に適用結果の postcondition と失敗状態を追加した。
- 価値/懸念: Undo／Redo の成功判定が実際の対象状態に近づく。JSON 正規化や shape setter の runtime 挙動は未検証。
- 次に確認: 対象 layer 削除・型不一致・setter 拒否時の push failure と rollback を確認する。

**2026-08-31 — Undo push zero-entry budget**

- 関連: `Artifact/src/Undo/UndoManager.cppm` の `UndoManager::push()`。
- 事実: `maxEntryCount == 0` でも command を一度 redo してから budget enforcement で削除し、push 成功を返す可能性があった。
- 対応: 履歴を一件も保持できない予算では redo 前に push を拒否するようにした。
- 価値/懸念: 呼び出し側が push 成功を「履歴へ記録済み」と誤認しない。各 UI fallback がこの拒否を期待通り扱うかはランタイム未検証。
- 次に確認: 件数上限0、メモリ上限0、単一エントリ超過時の直接適用と履歴表示を確認する。

**2026-08-31 — Macro serialization consistency**

- 関連: `Artifact/src/Undo/UndoManager.cppm` の `MacroUndoCommand`。
- 事実: `canSerialize()` は不正な子を拒否していたが、`serialize()` 単体では子を省略して部分的な JSON を返せた。また、子の推定サイズ加算に overflow 防止がなかった。
- 対応: シリアライズ前に全子の妥当性を確認し、型・データが空なら失敗させ、推定サイズを飽和加算するようにした。
- 価値/懸念: Macro の履歴状態と保存データの不一致やサイズ計算の wraparound を防ぐ。保存・復元の runtime 挙動は未検証。
- 次に確認: 不正な子、空 JSON、巨大な推定サイズを含む Macro の保存拒否と通常 Macro の保存・復元を確認する。

**2026-08-31 — Composition context toggle result handling**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` のレイヤー Visibility / Lock / Solo / Shy コンテキスト操作。
- 事実: コマンドの `push()` 戻り値を無視しており、履歴予算超過などで登録できない場合も操作結果を判定していなかった。
- 対応: UndoManager 経由と直接 `redo()` fallback の双方で `lastOperationSucceeded()` / `push()` の結果を確認するようにした。
- 価値/懸念: 履歴へ積めなかった操作を成功扱いしない。コンテキストメニュー上の失敗通知表示は既存責務のままで、runtime 未検証。
- 次に確認: Visibility / Lock / Solo / Shy の通常操作、履歴予算拒否時、UndoManager 不在時の状態同期を確認する。

**2026-08-31 — Macro restore symmetry**

- 関連: `Artifact/src/Undo/UndoManager.cppm` の `MacroUndoCommand::deserialize()`。
- 事実: 保存側は空 Macro をシリアライズ対象外としていたが、復元側は空の子配列を一度受け入れていた。
- 対応: 復元時にも空の子配列を拒否し、Macro の保存・復元契約を対称化した。
- 価値/懸念: 実体変更を持たない壊れた履歴エントリを復元しない。履歴ファイルの runtime 復元は未検証。
- 次に確認: 空 Macro、無効な子、正常な Macro の保存・復元結果を確認する。

**2026-08-31 — Group postcondition history compensation**

- 関連: `Artifact/src/Service/ArtifactProjectService.cppm` の Group / Ungroup Undo 経路。
- 事実: Macro の `push()` 後に親関係・group の存在を検証していたが、検証失敗時に実体だけを補修し、履歴へ残った Macro を取り消していなかった。
- 対応: push 前の履歴件数を保持し、postcondition 失敗時は直前 Macro を Undo してから、必要な場合だけ明示的な実体補修を行うようにした。
- 価値/懸念: 実体と Undo 履歴が異なる状態を防ぐ。Macro Undo 自体の途中失敗と selection 復元は runtime 未検証。
- 次に確認: Group / Ungroup の通常操作、途中失敗、履歴予算拒否後の履歴件数・親関係・選択状態を確認する。

**2026-08-31 — Structural operation result compensation**

- 関連: `Artifact/src/Service/ArtifactProjectService.cppm` の Precompose / Unprecompose / Split。
- 事実: UndoManager の `push()` 成功後に各操作固有の outcome／succeeded flag を検証していたが、検証失敗時も履歴エントリを残していた。
- 対応: push 前の履歴件数を記録し、結果検証に失敗した場合は直前の command を Undo してから false を返すようにした。
- 価値/懸念: 構造変更の実体と履歴成功状態の不一致を抑える。大規模な precompose の途中失敗・selection・cache は runtime 未検証。
- 次に確認: Precompose / Unprecompose / Split の成功、境界拒否、途中失敗、Undo／Redo と履歴件数を確認する。

**2026-08-31 — Timeline clip slide result handling**

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` の Track Painter `clipSlid` 接続。
- 事実: Slide command の `push()` 戻り値と、UndoManager 不在時の `lastOperationSucceeded()` を確認せず、失敗しても処理を完了扱いにしていた。
- 対応: manager 経由と直接 `redo()` fallback の双方で適用結果を確認し、失敗時は後続処理へ進まないようにした。
- 価値/懸念: timing lock、予算拒否、対象消失時に Timeline の成功扱いと実体状態がずれにくい。selection、current frame、cache は runtime 未検証。
- 次に確認: clip slide の通常操作、境界／lock 拒否、Undo／Redo と Track Painter の再描画を確認する。

**2026-08-31 — Text Gizmo drag rejection rollback**

- 関連: `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm` の Animator range drag commit。
- 事実: ドラッグ中に scalar／keyframe を先行変更していたが、履歴 budget 超過または `push()` 失敗時に keyframe metadata を含む before 状態へ戻していなかった。
- 対応: keyframe の interpolation、Bezier control point、roving、anchor、color label を復元する共通処理を追加し、単一エントリ／メモリ／件数拒否と push 失敗の双方で実行するようにした。
- 価値/懸念: 履歴外の Text Animator 変更が残る経路を抑える。property setter の runtime 成否と表示更新は未検証。
- 次に確認: scalar／keyframe Animator drag、budget 拒否、push 後 redo 失敗、Undo／Redo と text preview 更新を確認する。

**2026-08-31 — Remaining single-action push results**

- 関連: `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm` と `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`。
- 事実: Command Palette の Add Mask と Layer Panel の inline rename は `push()` 戻り値を捨て、履歴予算拒否や適用失敗を明示的に扱っていなかった。
- 対応: manager 経由の戻り値と manager 不在時の直接 command 実行結果を確認し、失敗時に後続処理へ進まないようにした。
- 価値/懸念: 単一操作の失敗を成功扱いしない。Mask の完全 snapshot と inline editor の UI 状態は runtime 未検証。
- 次に確認: Add Mask、inline rename の通常操作、対象消失、budget 拒否、Undo／Redo を確認する。

**2026-08-31 — Composition Settings commit gating**

- 関連: `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm` の Resolution Remap、Composition State、Master Property。
- 事実: 各 command の `push()` 戻り値を無視し、履歴予算拒否や適用失敗後もダイアログの後続変更・確定処理へ進める可能性があった。
- 対応: Resolution は direct fallback の反映値も確認し、3経路すべてで失敗時に警告して確定処理を中断するようにした。
- 価値/懸念: 複合設定の一部だけが成功した状態を新しい確定処理へ進めにくくする。ダイアログ全体の変更を完全な一つの transaction にすること、runtime rollback は未検証。
- 次に確認: Resolution／State／Master Property の同時編集、budget 拒否、対象消失、保存後の Undo／Redo を確認する。

**2026-08-31 — Composition rename command boundary**

- 関連: `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm` の Composition Settings 確定処理。
- 事実: `renameComposition()` を呼ぶ前に composition 本体名を直接変更していたため、Rename command が旧名を取得できず、Project View 同期や Undo 境界が同値 no-op になる可能性があった。
- 対応: 先行する直接 setter を除去し、既存の `ArtifactProjectService::renameComposition()` に名前変更を一本化した。
- 価値/懸念: composition 本体・Project View item・履歴の名前変更責務を一つの経路へ戻す。設定ダイアログ全体の他プロパティ transaction は別途未統合、runtime 未検証。
- 次に確認: Composition 名変更、同値名、Project View 表示、Undo／Redo、保存／再読込後の名前同期を確認する。

**2026-08-31 — Work Area and Center Layer result handling**

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm` の Work Area 操作、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` の Center Layer。
- 事実: command-only 操作で `push()` の戻り値、または manager 不在時の直接 `redo()` 結果を確認せず、処理を成功経路として終えていた。
- 対応: Work Area の開始・終了・移動と Center Layer で適用結果を確認し、失敗時に同期・後続処理へ進まないようにした。
- 価値/懸念: 履歴予算拒否や対象不在を成功扱いしにくくする。Work Area の engine/cache 同期と Center Layer の表示更新は runtime 未検証。
- 次に確認: Work Area 3操作、Center Layer、予算拒否、Undo／Redo、playback range/cache の同期を確認する。

**2026-08-31 — Transform Field direct update fallback**

- 関連: `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm` の Live Transform Field 編集／有効切替。
- 事実: UndoManager 不在時の fallback が更新対象を `addTransformField()` しており、既存 Field を編集する操作で重複 Field を作る可能性があった。
- 対応: Field ID で既存要素を検索し、配列内の該当要素を置換して `setTransformFields()` するようにした。
- 価値/懸念: 履歴サービス不在時も編集／切替が追加操作へ化けない。Field 配列 setter の通知・active selection は runtime 未検証。
- 次に確認: Field の編集、有効切替、UndoManager 不在時の重複有無、active Field と描画結果を確認する。

**2026-08-31 — Layer Menu state and radial transform fallback**

- 関連: `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm` の Visibility／Lock／Solo／Shy と Radial Transform。
- 事実: 基本状態切替は UndoManager 不在時に無操作となり、Radial Transform は `push()` 結果を無視していた。
- 対応: 4つの状態切替を command の直接 `redo()` fallback と成否確認付きにし、Radial Transform も同じ command 経路で manager 不在時に適用するようにした。
- 価値/懸念: Layer Menu の基本操作と複数レイヤー変形が履歴サービスの状態に依存しすぎない。selection、dirty、preview cache は runtime 未検証。
- 次に確認: 4状態切替、Radial Transform、予算拒否、Undo／Redo、複数選択の表示同期を確認する。

**2026-08-31 — Layer Menu command execution boundary**

- 関連: `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm` の Transform Field／Parametric 操作。
- 事実: Field 追加・選択・有効化・並べ替え・削除、および Parametric 定義変更が各所で `push()` を直接呼び、manager 不在時の直接適用と失敗判定が分散していた。
- 対応: `applyLayerMenuUndoCommand()` を追加し、既存 command の manager 経路、直接 `redo()` fallback、`lastOperationSucceeded()` を一つの境界へ統合した。
- 価値/懸念: command-only 操作の履歴拒否・対象消失・manager 不在時の無操作を同じ契約で扱える。通知・active selection・runtime は未検証。
- 次に確認: Field／Parametric 各操作の通常・失敗・Undo／Redo、manager 不在、履歴 budget 拒否を確認する。

**2026-08-31 — Layer mask conversion result handling**

- 関連: `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm` の Text→Mask、Shape→Mask、Mask→Shape。
- 事実: 変換 command の `push()` 結果を無視し、manager 不在時は一部の変換が無操作になる可能性があった。
- 対応: 共通 command 実行境界へ接続し、直接 fallback、適用成否確認、失敗時の警告を追加した。
- 価値/懸念: マスク変換の履歴拒否・対象不在を成功扱いしない。生成 Shape の全要素 rollback と selection は runtime 未検証。
- 次に確認: 3種類の変換、空／不正パス、予算拒否、Undo／Redo、生成レイヤー選択を確認する。

**2026-08-31 — Composition settings and particle sketch fallback results**

- 関連: `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Tool/ArtifactMotionSketchTool.cppm`。
- 事実: Composition Settings の直接 setter と、パーティクル／Motion Sketch のドラッグ確定処理で、適用後の値または UndoManager 不在時の処理結果を十分に確認していなかった。
- 対応: Composition Settings の responsive layout・サイズ・フレーム設定・背景色を検証し、finalize 結果も確認。パーティクルと Motion Sketch は manager 不在時の既存直接変更を保持し、manager が存在して push に失敗した場合だけ before 状態へ戻す条件に整理した。
- 価値/懸念: 履歴サービス不在を誤って無操作扱いせず、履歴予算拒否や適用失敗だけを rollback できる。Composition Settings 全体の単一 Undo transaction 化と runtime 動作は未検証。
- 次に確認: 各ドラッグ操作、manager 不在、履歴予算拒否、Composition Settings の入力不正、Undo／Redo、再生範囲同期を確認する。

**2026-08-31 — Particle and audio editor direct-commit fallback**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`、`Artifact/src/Tool/ArtifactMotionSketchTool.cppm`、`Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm`。
- 事実: これらの編集は live state を先に変更してから Undo command を記録するため、UndoManager 不在時に変更が無操作扱いになり、履歴 push 失敗時の復元条件も経路ごとに異なっていた。
- 対応: manager 不在時は検証可能な live 変更を保持し、manager が存在して push に失敗した場合だけ before 状態へ戻すよう整理。Audio Mixer は manager 不在時もシリアライズ結果を検証し、失敗時は before JSON を復元する。
- 価値/懸念: ドラッグ編集の実状態と履歴結果の不一致を減らす。UI 通知、Mixer の deserialize 後更新、runtime は未検証。
- 次に確認: Particle／Motion Sketch／Audio Routing の manager 不在、履歴 budget 拒否、対象消失、Undo／Redo、表示更新を確認する。

**2026-08-31 — Audio routing live-state verification**

- 関連: `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm` の Advanced Audio Routing。
- 事実: Routing editor は mixer を先に変更してから Undo callback を呼ぶため、UndoManager 不在をそのまま失敗扱いにすると、実際には変更済みなのに UI が失敗経路へ入る可能性があった。
- 対応: manager がない場合は変更後 JSON を検証して直接適用を成功扱いにし、manager の push または検証が失敗した場合だけ before JSON に復元するようにした。
- 価値/懸念: mixer の live state と履歴記録結果の不整合を抑える。deserialize 後のメーター／表示再構築は runtime 未検証。
- 次に確認: Routing の追加・削除・並べ替え、manager 不在、履歴 budget 拒否、Undo／Redo、Mixer 表示更新を確認する。

**2026-08-31 — Timeline matte and layer-drop command fallback**

- 関連: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` の Matte 追加／差し替えと layer index ドロップ。
- 事実: これらの command-only 操作は UndoManager が存在する場合だけ push し、manager 不在時は変更を実行しない経路になっていた。
- 対応: manager があれば push、なければ command の直接 `redo()` と `lastOperationSucceeded()` を使う共通の結果判定パターンへ揃えた。
- 価値/懸念: Matte 操作とレイヤー並べ替えが履歴サービスの有無で無操作にならない。選択・dirty・タイムライン再構築の runtime 挙動は未検証。
- 次に確認: Matte 追加／差し替え、レイヤー D&D、manager 不在、履歴 budget 拒否、Undo／Redo、選択同期を確認する。

**2026-08-31 — Motion Dynamics preset transaction**

- 関連: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` の Motion Dynamics メニュー。
- 事実: プリセットと Reset が複数の layer property setter を直接連続実行しており、Undo が一操作にまとまらず、履歴サービス不在時の結果判定もなかった。
- 対応: enabled／mode／stiffness／damping／mass／lagTau／clampOvershoot／overshootLimit を既存の `SetLayerPropertyValueCommand` の `MacroUndoCommand` にまとめ、manager 不在時は直接 redo と成否確認を行うようにした。Reset の enabled=false／mode=0 も保持した。
- 価値/懸念: プリセット適用と Reset が一回の Undo／Redo 単位になり、途中失敗時の Macro rollback 契約を利用できる。property 型の互換性と UI／runtime 通知は未検証。
- 次に確認: 各プリセット、Reset、manager 不在、履歴 budget 拒否、Undo／Redo、再描画と Motion 挙動を確認する。

**2026-08-31 — Layer Panel all-layer state transactions**

- 関連: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` の全レイヤー表示／ロック操作。
- 事実: 全レイヤー表示・非表示・ロック・ロック解除が各 layer の setter を直接反復し、単一 Undo 単位も manager 不在時の適用結果確認もなかった。
- 対応: 既存の Visibility／Lock command を MacroUndoCommand にまとめ、空 composition を除外し、manager 経路と直接 redo fallback の結果を確認するようにした。
- 価値/懸念: バッチ状態変更を一操作として戻せ、途中失敗時の Macro 補償を利用できる。selection・dirty・preview 更新の runtime は未検証。
- 次に確認: 全表示／非表示／ロック／解除、空 composition、履歴 budget 拒否、Undo／Redo、timeline 再描画を確認する。

**2026-08-31 — Layer label color command boundary**

- 関連: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` のレイヤーラベル色メニュー。
- 事実: ラベル色変更が直接 setter のみで、Undo 履歴・manager 不在時の成否確認・対象消失時の失敗判定がなかった。
- 対応: panel 内の小さな `SetLayerLabelColorCommand` を追加し、before／after の色番号、適用後検証、manager push／直接 redo fallback を持たせた。
- 価値/懸念: ラベル色の変更も他の layer state 操作と同じ Undo 境界になる。ラベル色の保存／再読込と UI 色表示は runtime 未検証。
- 次に確認: 全ラベル色、同色選択、対象消失、履歴 budget 拒否、Undo／Redo、保存／再読込を確認する。

**2026-08-31 — Layer Panel header and solo-only command reuse**

- 関連: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` の header state buttons と Ctrl+Alt+Click の solo-only 表示。
- 事実: header の表示／ロック／ソロ切替と solo-only 操作が直接 setter で、既存の state command と異なる履歴境界になっていた。
- 対応: panel 内の command 実行ヘルパーを追加し、header 3操作を既存 command へ接続。solo-only は全レイヤーの Visibility command を MacroUndoCommand にまとめた。
- 価値/懸念: 同じ UI 操作でも UndoManager 有無と push 失敗を一貫して扱える。Audio／Video 列や layer property preset の残る直接 setter は別責務として未検証。
- 次に確認: header 操作、solo-only、manager 不在、履歴 budget 拒否、Undo／Redo、選択・表示更新を確認する。

**2026-08-31 — Layer Panel selected-menu command consistency**

- 関連: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` の選択メニュー、Proxy Quality、Light Linking。
- 事実: 選択メニューの state setter、Proxy Quality、Light Linking 3操作は、同じ panel 内の他の操作と異なり直接変更または manager 依存で、Undo 単位と失敗判定が揃っていなかった。
- 対応: 選択メニューの Visibility／Lock／Solo／Shy を共通 command helper に接続し、Proxy Quality は before／after の property command、Light Linking は3プロパティを1つの MacroUndoCommand にまとめた。
- 価値/懸念: 入口による Undo 挙動の差を減らし、manager 不在・履歴拒否・property 欠落を成功扱いしにくくした。3D Material preset や一部の media setter は引き続き別途確認が必要。
- 次に確認: 選択メニュー state、Proxy Quality、Light Linking、manager 不在、履歴 budget 拒否、Undo／Redo、表示と renderer 更新を確認する。

**2026-08-31 — Layer property preset transaction helper**

- 関連: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` の 3D Material と Text Animator メニュー。
- 事実: Material の6項目プリセットと Text Animator の preset setter が直接変更され、Undo 単位・property 欠落・型差異の扱いがなかった。
- 対応: property の before 値を取得し、現在の QVariant 型へ変換して `SetLayerPropertyValueCommand` を MacroUndoCommand にまとめる共通 helper を追加。Material／Animator の適用後だけ UI 更新するようにした。
- 価値/懸念: 複数値のプリセットを一操作で Undo／Redo でき、partial property 構成を誤って成功扱いしない。3D renderer 反映と Text Animator の実動作は runtime 未検証。
- 次に確認: Material 各プリセット、Animator 各 preset／Clear、型変換、manager 不在、履歴 budget 拒否、Undo／Redo、描画更新を確認する。

**2026-08-31 — Layer Panel media and group state routing**

- 関連: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` の Video mute／video enabled、Audio mute、Group output mode。
- 事実: 専用 setter と property setter の二重適用、または直接 setter のみで、Undo 時に専用状態と property 表現が分離する可能性があった。
- 対応: 既存の `setLayerPropertyValue()` 実装が専用 setter と dirty 更新を担うことを確認し、Video／Audio の mute、Video enabled、Group output mode を property command／共通 Macro helper へ接続した。
- 価値/懸念: 専用状態と保存用 property の適用経路を一本化し、Undo／Redo と manager 不在時の結果判定を揃えた。音声再生・映像 renderer・Group compositor の runtime は未検証。
- 次に確認: Video／Audio mute、Video enabled、Group output mode、manager 不在、履歴 budget 拒否、Undo／Redo、再生・描画反映を確認する。

**2026-08-31 — Group active-child command boundary**

- 関連: `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` の Group「Single の出力先」。
- 事実: active child の選択が `setActiveChildId()` の直接呼び出しで、Output Mode と異なる Undo 境界になっていた。
- 対応: child ID から有効な child index を解決し、既存の `group.activeChildIndex` property setter を before／after command として実行するようにした。
- 価値/懸念: Group の出力先変更も Undo／Redo と manager 不在時の結果判定を持つ。composition-owned child 列挙と render 評価の runtime は未検証。
- 次に確認: Single output の child 切替、非表示 child、manager 不在、履歴 budget 拒否、Undo／Redo、Group compositor の出力を確認する。

**2026-08-31 — Text editor style transaction boundary remains separate**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` の Text Editor commit。
- 事実: Text Editor は多数の style setter をまとめて直接適用し、Animator Stack だけを既存 command で記録している。値の before snapshot は存在するが、style 全体と source text の transaction 境界は分離している。
- 判断: ここを一括 command 化するには、font／layout／stroke／shadow／paragraph など各 property path と型、source text command、Animator Stack command の順序を一つの Macro に整理する必要がある。現時点で局所的な setter 置換をすると、部分 rollback や二重 command の危険があるため、推測で変更しない。
- 価値/懸念: 未対応の Undo gap を明示し、次回は Text Layer の property path と command grouping を先に設計できる。runtime 未検証。
- 次に確認: Text Editor style の全 before／after 対応表、source text／animator stack との Macro 境界、manager 不在・push 拒否・Undo／Redo を確認する。

**2026-08-31 — Text editor style property snapshot command**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` の Text Editor commit。
- 事実: style setter 群は live state を先に変更する構造だったが、Undo 用の property 対応表と push 失敗時の復元経路がなかった。
- 対応: font／layout／stroke／shadow／paragraph の before／after を `SetLayerPropertyValueCommand` の MacroUndoCommand にまとめ、manager 不在時は既存 live change を保持し、push 失敗時は before property 群へ戻すようにした。Animator count はスタック全体の snapshot command と競合するため、この Macro から分離した。
- 価値/懸念: Text style の複数値を一操作として Undo／Redo できる。source text と Animator Stack は現在も別 command 境界で、単一 transaction 化および runtime は未検証。
- 次に確認: style 全項目、型変換、source text／Animator Stack の同時編集、manager 不在、履歴 budget 拒否、Undo／Redo、render dirty 通知を確認する。

**2026-08-31 — Text animator count must remain in stack snapshot boundary**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`, `Artifact/src/Layer/ArtifactTextLayer.cppm`。
- 確認できた事実: `text.animatorCount` はスタック要素数だけを変更する一方、Animator の各要素と animated properties は `textAnimatorStackSnapshot()`／`restoreTextAnimatorStack()` で一括復元される。
- 対応: Text style Macro から `text.animatorCount` を除外し、Animator count／preset の変更を既存のスタック snapshot command に限定した。manager 不在時は live 変更を保持し、push 失敗時はスタックを before snapshot に戻す結果フラグも修正した。
- 価値/懸念: style Undo と Animator Stack Undo が同じ状態を二重に書き換えず、スタック要素と関連 property の整合性を保ちやすい。source text／style／Animator Stack の完全な一操作化と runtime は未検証。
- 次に確認: animator count の増減、preset、style 同時変更、Undo／Redo、manager 不在、履歴 budget 拒否、render dirty 通知を確認する。

**2026-08-31 — Reset transform group preserves live state without UndoManager**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の複数選択 Transform reset。
- 確認できた事実: 各対象へ reset を live apply した後、UndoManager が存在する場合だけ `GizmoGroupTransformUndoCommand` を push しており、manager 不在時は `pushed == false` のまま before state へ rollback していた。
- 対応: manager 不在時は live apply を成功扱いにし、変更通知・render dirty を継続するようにした。manager がある場合のみ push 失敗時に rollback する。
- 価値/懸念: Undo サービス未接続時でも UI の複数選択 reset が見た目だけ取り消されない。Undo／Redo と複数レイヤーの runtime は未検証。
- 次に確認: 2D／3D、複数選択、キーフレーム、manager 不在、履歴 budget 拒否、Undo／Redo、selection と render cache を確認する。

**2026-08-31 — Inspector matte helpers apply without UndoManager**

- 関連: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` のマット参照変更補助関数群。
- 確認できた事実: matte type／layer source／project input／clear の各補助関数は `ChangeLayerMatteReferencesCommand` を作成するものの、UndoManager がない場合は `undo && push(...)` により常に false を返し、UI の操作結果を適用しなかった。
- 対応: before／after の参照配列を受ける共通 helper を追加し、manager があれば既存 command、なければ `setMatteReferences(afterRefs)` を使うようにした。
- 価値/懸念: Inspector と matte context menu の直接操作が Undo サービスの有無で挙動を変えず、既存の cycle／index 検証も維持される。manager 不在時の runtime と matte compositor の反映は未検証。
- 次に確認: type／layer source／project input／clear、cycle、multiple references、manager 不在、Undo／Redo、render cache を確認する。

**2026-08-31 — Layer order macro gets a direct fallback**

- 関連: `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm` の Bring／Send layer order 操作。
- 確認できた事実: 選択レイヤーの並び替えは `MacroUndoCommand` のみを構築し、UndoManager がない場合は実行せず false を返していた。
- 対応: 変更がある場合、manager があれば従来どおり一つの Macro として pushし、manager 不在時は同じ Macro を直接 redo して `lastOperationSucceeded()` を返すようにした。
- 価値/懸念: 単一／複数選択のレイヤー順序変更が Undo サービスの有無で消失しない。順序変更後の timeline／render cache の runtime は未検証。
- 次に確認: Bring Forward／Backward、Front／Back、複数選択順序、部分失敗、manager 不在、Undo／Redo、selection 同期を確認する。

**2026-08-31 — Standalone AlignmentWidget uses the shared undo boundary**

- 関連: `Artifact/src/Widgets/ArtifactAlignmentWidget.cppm` の Align／Distribute ボタン。
- 確認できた事実: Layer Menu 側には `AlignLayersUndoCommand` がある一方、独立 AlignmentWidget は選択レイヤーの位置を直接変更するだけで、Undo／Redo や push 失敗時の復元を持っていなかった。
- 対応: before／after の位置・scale snapshot を作成し、既存 `AlignLayersUndoCommand` を一操作として push。push 失敗時は位置を before に戻し、manager 不在時は既存の live 変更を保持する。選択 manager の null も防御した。
- 価値/懸念: 2つの整列 UI で履歴境界が揃い、履歴拒否時の部分状態を残しにくくなった。現状は既存仕様どおり frame 0／30000 time scale を使い、runtime は未検証。
- 次に確認: 各 Align／Distribute 種別、現在フレーム以外、複数選択、push 拒否、Undo／Redo、selection と render cache を確認する。

**2026-08-31 — Anchor Point Tool records anchor and visual-position changes**

- 関連: `Artifact/src/Widgets/ArtifactAnchorPointTool.cppm` の Anchor Point／Apply to Selected 操作。
- 確認できた事実: anchor の変更と、見た目位置維持のための position 補正が直接 transform を変更していたが、Undo command は作成されていなかった。
- 対応: 既存の `SetLayerPropertyValueCommand` と `MacroUndoCommand` を使い、anchor／position の before／after を記録するようにした。push 前に before へ戻し、拒否時も before を保持する。UndoManager 不在時は live 変更を保持する。
- 価値/懸念: anchor 単体と visual-position 補正を同じレイヤー操作として Undo／Redo できる。複数選択は現行のレイヤー単位 push のままで、frame rate 24／current frame の runtime 整合性は未検証。
- 次に確認: anchor 9種、visual-position on/off、keyframe、複数選択、push 拒否、Undo／Redo、selection と render cache を確認する。

**2026-08-31 — Dope Sheet keyframe batches become one undo operation**

- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeModel.cppm` の keyframe add／Bezier add／move／remove／offset／scale。
- 確認できた事実: 単一 keyframe API と Dope Sheet の offset／scale は property の keyframe 配列を直接変更していた。複数 property をまたぐ Dope Sheet 操作には Undo command がなかった。
- 対応: 単一操作は before／after keyframe 配列を既存 `SetLayerPropertyKeyframesCommand` へ接続し、Dope Sheet の複数 property 変更は `MacroUndoCommand` にまとめた。manager がある場合は push 前に before へ戻し、拒否時は全変更を before へ復元する。
- 価値/懸念: keyframe 編集の履歴境界が明確になり、部分的な Dope Sheet 変更を残しにくくなった。現行の frame scale／重複 keyframe の runtime と Undo／Redo は未検証。
- 次に確認: add／Bezier／move／remove、offset／scale、同時刻衝突、manager 不在、履歴 budget 拒否、Undo／Redo、timeline repaint を確認する。

**2026-08-31 — ArtifactPr export now matches preview (transitions / effects / audio)**

- 関連: `ArtifactPr/src/SequenceExporter.cppm`、`ArtifactPr/src/ArtifactPrMainWindow.cppm`、新規 `ArtifactPr/{include,src}/ClipEffects.*`、`ArtifactPr/{include,src}/SequenceAudioRenderer.*`。
- 確認できた事実: collectActiveClips は `store_->sequenceIds()` 全件を走査しており、NLE ストアに複数シーケンスが存在すると全シーケンスのアクティブクリップが 1 枚に重畳する既存挙動がある (SequenceExporter.cppm の sequenceIds ループ)。単一シーケンス運用では無害。
- 価値/懸念: トランジション opacity 変調・fx.* クリップエフェクト・音声ミックスを RenderPlan 凍結経由でエクスポートへ反映し、プレビューと同一のカーブ/評価順に統一した。AudioPreviewMixer の durationFrames 未使用 (ソース終端まで鳴る) は既存のまま変更していない。
- 次に確認: トランジション区間のプレビュー/エクスポート一致、WAV/MP3/動画+mux の手動再生確認 (ARTIFACT_BUILD_PR=ON ビルド後)。

**2026-08-31 — Clip effect evaluation cost sits on the preview main thread**

- 関連: `ArtifactPr/src/ClipEffects.cppm`、`ArtifactPr/src/ArtifactPrMainWindow.cppm` の onFrameDecoded。
- 価値/懸念: エフェクト評価はデコード後のソース解像度フレームに対して行われる (4K ソース + blur 時にプレビュー fps が落ちる可能性)。toCanonicalRGBA32FC4 のコピーが 1 回/フレーム発生する。実測は未検証。
- 次に確認: 重いエフェクト (gaussianBlur 半径大) + 4K ソースでのプレビュー性能。必要なら canvas スケール評価への変更を検討。

**2026-08-31 — AudioPreviewMixer waveform peaks are computed after EQ**

- 関連: `ArtifactPr/src/AudioPreviewMixer.cppm` の loadClipAudio。
- 確認できた事実: waveform peak (0.5 秒バケット) は EQ 適用後のセグメントから計算するようになった。EQ 設定が波形表示に反映される。キャッシュキーは filePath+EQ 署名のため、同一ファイルで EQ の異なるクリップは別キャッシュになりメモリが増える (EQ 未使用時は従来どおり 1 キー)。
- 次に確認: EQ 適用クリップの波形/メーター表示の整合。

**2026-08-31 — Point tracker rejects empty exports without leaving layers or effects**

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm` の tracking／planar export。
- 確認できた事実: composition 範囲内に適用できるサンプルがない場合でも Null レイヤーを先に追加し、Corner Pin も有効なキーフレームの有無を確認する前に effect を追加していた。
- 対応: 有効なサンプルを先に収集し、空の場合は false を返す。Null レイヤーは append 成功を確認してから確定し、Corner Pin は範囲内かつ有限な値のキーフレームがある場合だけ effect を追加する。
- 価値/懸念: 失敗したトラッキング適用で空のレイヤー／壊れた effect が残りにくくなった。新規レイヤー＋effect＋キーフレーム全体を一つの Undo コマンドにまとめる既存公開 API は見つからず、Undo 統合は未実装。
- 次に確認: 空範囲、NaN／巨大時刻、append failure、planar corner pin の Undo／Redo と effect 削除履歴を確認する。

**2026-08-31 — Camera tracker requires a camera layer append to succeed**

- 関連: `Artifact/src/Tool/ArtifactCameraTrackerTool.cppm` の camera tracking export。
- 確認できた事実: solve 成功後に camera layer の生成または append が失敗しても、feature layer 作成へ進み最終的に true を返す可能性があった。
- 対応: camera layer の生成と append 成功を必須化し、feature layer は append 成功数だけを数えるようにした。
- 価値/懸念: カメラ本体が存在しないのに tracking 成功と報告する状態を避けられる。camera layer／feature layer 群を一つの Undo 操作にまとめる既存 API は未確認。
- 次に確認: camera append failure、feature append 部分失敗、tracking 結果の再適用時の重複と Undo／Redo を確認する。

**2026-08-31 — Curve Editor key deletion uses the timeline snapshot command**

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` の Curve Editor `keyDeleted` ハンドラ。
- 確認できた事実: 削除操作だけは property から直接 `removeKeyFrame()` しており、Curve Editor の drag 操作で使うスナップショット履歴へ接続されていなかった。
- 対応: 削除前後の property keyframe snapshot を取得し、既存 `TimelineKeyframeSnapshotCommand` に登録した。UndoManager の push 拒否時は削除前へ復元し、UndoManager 不在時は直接変更を保持する。
- 価値/懸念: Curve Editor の削除も一操作一履歴になった。選択状態の自動復元はこの単独削除では追加していない。runtime の signal 順序と再描画は未検証。
- 次に確認: 単独削除、複数選択削除、同時刻衝突、push 拒否、Undo／Redo、curve cache 再構築を確認する。

**2026-08-31 — Audio mixer channel controls record layer property changes**

- 関連: `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm` の channel strip。
- 確認できた事実: volume／pan／mute の UI 操作はレイヤーへ値を反映していたが、UndoManager へ登録されていなかった。volume は 16ms timer でライブ更新されるため、更新ごとの command 化は履歴を細切れにする。
- 対応: 既存 `SetLayerPropertyValueCommand` を使い、volume は slider 押下〜リリースを一件に集約、pan／mute は各操作を一件として記録した。manager push 前に before へ戻し、拒否時は after を復元する。audio/video のプロパティパスを型ごとに分けた。
- 価値/懸念: フェーダー操作の履歴粒度と失敗時の復元が明確になった。Solo は既存の `setLayerSoloInCurrentComposition()` 経由で専用 `SetLayerSoloCommand` に接続し、Master Bus は Core Mixer の JSON snapshot command に接続した。runtime の signal 順序、再生中の mixer 同期、Undo／Redo は未検証。
- 次に確認: volume drag／keyboard、pan、mute、video layer、push 拒否、Undo／Redo、audio engine の再同期を確認する。

**2026-08-31 — Point tracker position export is one add-layer/keyframe macro**

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm` の `applyTrackingResult()`。
- 確認できた事実: 新規 Null 作成時の layer append と position／anchor keyframe 書き込みは、従来は直接変更で別々の履歴境界も持たなかった。
- 対応: UndoManager がある場合、既存 `AddLayerCommand` と `SetLayerPropertyKeyframesCommand` を `MacroUndoCommand("Apply Tracking Result")` にまとめた。push 前に before snapshot へ戻し、拒否時は after を復元する。manager 不在時は従来の直接適用を保持する。
- 価値/懸念: 新規 Null layer の生成と position／anchor の適用を一回の Undo で戻せる経路を追加した。planar Corner Pin の effect 追加を含む専用 tracker command、選択状態、runtime／session reload は未検証。
- 次に確認: selected layer／new Null、anchor on/off、複数 point、履歴 budget 拒否、Undo／Redo、layer selection と render cache を確認する。

**2026-08-31 — Planar Corner Pin export is one effect/keyframe macro**

- 関連: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm` の `applyPlanarResultAsCornerPin()`。
- 確認できた事実: Corner Pin effect と 8 個の animatable property の keyframe 書き込みが直接実行され、effect 追加を含む一括 Undo 境界がなかった。
- 対応: effect の追加／削除を検証する `AddLayerEffectUndoCommand` と既存 `SetEffectPropertyKeyframesCommand` を `Apply Planar Corner Pin` Macro にまとめた。UndoManager 不在時は直接適用を維持し、時刻・値・composition 範囲の検証後にだけ effect を追加する。
- 価値/懸念: planar export の effect と keyframe を一回の Undo で戻せる。effect add command は UndoManager の factory と ID resolver に接続したが、実際の session save／reload、selection／runtime は未検証。
- 次に確認: effect 重複、8 property の部分失敗、Undo／Redo、session save／reload、render cache と selection を確認する。

**2026-08-31 — Interpolation apply keeps working without UndoManager**

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` の `applyInterpolationToSelectedKeyframesImpl()`。
- 確認できた事実: UndoManager がある場合は `ApplyInterpolationCommand` を push するが、manager 不在時は records を作成した後に `0` を返し、選択キーフレームへ何も適用していなかった。
- 対応: manager 不在時は既存 `applyInterpolationChangeRecords()` を after 側・通知なしで直接実行し、成功件数を返すようにした。manager がある場合の一操作一 Undo は維持する。
- 価値/懸念: 初期化順序や限定環境でも interpolation 操作が無操作にならない。direct fallback の runtime 表示更新、部分失敗、selection は未検証。
- 次に確認: Linear／Bezier／Easy Ease、複数選択、manager 不在、push 拒否、Undo／Redo、再描画を確認する。

**2026-08-31 — Composition effect direct fallbacks verify postconditions**

- 関連: `Artifact/src/Service/ArtifactProjectService.cppm` の composition effect 追加・削除・有効状態変更。
- 確認できた事実: UndoManager 不在時の直接処理は存在したが、追加／削除後の effect 集合や enabled 状態を確認せず成功を返していた。
- 対応: effect ID の追加・削除結果と enabled 値を事後確認し、期待状態に到達しなければ失敗を返すようにした。
- 価値/懸念: Undo 経路と direct fallback の成功判定が揃う。runtime の effect container 実装と通知順序は未検証。
- 次に確認: effect add／remove／enable、失敗時の UI 同期、Undo／Redo、project mutation 通知を確認する。

**2026-08-31 — Layer parent direct fallback verifies assignment**

- 関連: `Artifact/src/Service/ArtifactProjectService.cppm` の `setLayerParentInCurrentComposition()`。
- 確認できた事実: UndoManager 不在時の親解除・親設定は直接実行されるが、実際の parent ID を確認せず成功を返していた。
- 対応: 親解除後は nil、親設定後は指定 ID になったことを確認し、失敗時は false を返すようにした。
- 価値/懸念: parent 操作の direct fallback でも postcondition を満たさない状態を成功扱いしない。runtime の Core 側 setter の失敗条件は未検証。
- 次に確認: 親設定・解除、循環拒否、UndoManager 不在、push 拒否、Undo／Redo、selection 更新を確認する。

**2026-08-31 — Marker shortcut fallback preserves edits without UndoManager**

- 関連: `Artifact/src/Service/ArtifactPlaybackShortcuts.cppm` のサービス不在時 marker fallback。
- 確認できた事実: marker add／chapter add／delete／clear は変更後に `!undo || !push` で before snapshot へ戻していたため、UndoManager 不在時は変更が常に消えていた。
- 対応: UndoManager が存在する場合だけ push 失敗時に rollback し、manager 不在時は直接変更を保持するようにした。
- 価値/懸念: 初期化順序や限定環境でも marker 操作が no-op にならない。marker persistence と UI refresh は未検証。
- 次に確認: 各 marker 操作、manager 不在、push 拒否、Undo／Redo、保存・再読込を確認する。

**2026-08-31 — Mask and asset direct fallbacks no longer rollback without UndoManager**

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の複数レイヤー mask 操作、`Artifact/src/Asset/AssetDirectoryModel.cppm` の asset file move。
- 確認できた事実: mask 操作は変更済みの状態を UndoManager 不在時にも rollback していた。asset move は command を push できない場合に直接 redo する経路がなかった。
- 対応: mask は manager が存在する場合だけ push 失敗時に rollback し、asset move は manager 不在時に command の redo を直接実行して成否を確認するようにした。
- 価値/懸念: fallback 環境でも UI 編集と asset 移動が no-op にならない。mask の複数対象部分失敗と filesystem rename の race は未検証。
- 次に確認: mask toggle／reorder／geometry、asset drag move、manager 不在、push 拒否、Undo／Redo、model refresh を確認する。

**2026-08-31 — Workspace automation batch fallbacks are transactional**

- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx` の batch rename／batch move。
- 確認できた事実: composition rename の direct fallback は composition 本体だけを更新し ProjectItem 名を同期していなかった。batch move は途中の direct 操作が失敗しても先行した移動を残す可能性があった。
- 対応: rename で composition と ProjectItem の両方を更新・検証し、失敗時は全件を旧値へ戻すようにした。move は各操作の戻り値と parent を検証し、失敗時は保存した親・位置へ逆順復元するようにした。
- 価値/懸念: AI batch 操作の成功／失敗境界が明確になった。複数操作の外部変更競合と runtime の ProjectItem tree 通知は未検証。
- 次に確認: composition rename、通常 item rename、複数 item move、途中失敗、Undo／Redo、project tree refresh を確認する。

**2026-08-31 — Active context timing fallbacks verify frame changes**

- 関連: `Artifact/src/Application/ActiveContextService.cppm` の layer In／Out／Trim 操作。
- 確認できた事実: UndoManager 不在時は timing setter を直接呼ぶが、setter が要求 frame を反映したか確認していなかった。Trim In は二つの setter の片方だけ失敗する可能性もあった。
- 対応: In／Out の direct fallback に事後確認を追加し、Trim In は inPoint と startTime の両方が一致しない場合に旧値へ復元して失敗扱いにした。
- 価値/懸念: timing 操作が範囲外・拒否状態を成功扱いしにくくなった。runtime の setter が clamp する仕様と UI refresh は未検証。
- 次に確認: Set In／Set Out／Trim In／Trim Out、境界 frame、timing lock、UndoManager 不在、再描画を確認する。

**2026-08-31 — Work-area direct fallbacks verify applied ranges**

- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm` の work area start／end／move 操作。
- 確認できた事実: UndoManager 不在時の直接 `setWorkAreaRange()` 後に、要求した start/end が反映されたか確認せず通知・engine 同期へ進んでいた。
- 対応: 3 操作で range の事後状態を検証し、反映されなければ旧 range へ戻して通知しないようにした。
- 価値/懸念: work area と playback engine の不一致を減らせる。Core 側の clamp 仕様と runtime engine 同期は未検証。
- 次に確認: start／end／move、範囲境界、manager 不在、setter 拒否、再生範囲同期を確認する。

**2026-08-31 — Duplicate layer direct fallback verifies insertion index**

- 関連: `Artifact/src/Service/ArtifactProjectService.cppm` の `duplicateLayerInCurrentComposition()`。
- 確認できた事実: UndoManager 不在時の複製は、複製 layer の index 移動後に位置を検証せず、複製・選択を成功扱いしていた。
- 対応: direct move 後に複製 layer の実 index を確認し、期待位置に到達しなければ複製を除去して false を返すようにした。
- 価値/懸念: 複製と index 移動の atomicity を direct fallback でも維持する。Core の removeLayer 後の selection／project notification は未検証。
- 次に確認: 複製位置、manager 不在、move 拒否、失敗時 layer 数、selection、Undo／Redo を確認する。

**2026-08-31 — Tangent snapshot application failure is not reported as success**

- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm` の Break／Unify Tangents。
- 確認できた事実: after keyframe snapshot の適用結果を確認せず、Undo 登録または成功メッセージへ進む可能性があった。
- 対応: after snapshot 適用を検証し、失敗時は before snapshot へ戻して処理を終了するようにした。
- 価値/懸念: tangent 編集の部分適用を成功扱いしにくくなった。keyframe setter 内部の部分失敗と runtime 表示更新は未検証。
- 次に確認: Break／Unify、複数選択、対象消失、setter 拒否、Undo／Redo、Timeline refresh を確認する。

**2026-08-31 — Track Painter snapshot helpers propagate apply failure**

- 関連: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm` の keyframe area value／numeric value／timeline layer snapshot helper。
- 確認できた事実: after keyframe snapshot の適用結果を無視して成功を返す補助関数があり、失敗時に後続の Undo 登録や表示更新へ進み得た。layer timing は keyframe 復元前に先行変更されていた。
- 対応: after snapshot 適用を検証し、失敗時は keyframe before と timing 旧値へ復元するようにした。出力 snapshot は適用成功後だけ返す。
- 価値/懸念: pre-apply 段階の部分変更を成功扱いしにくくなった。複数 property の途中 setter failure と runtime cache は未検証。
- 次に確認: area value、numeric value、slide／ripple snapshot、対象消失、setter 拒否、Undo／Redo、cache refresh を確認する。

**2026-08-31 — Timeline snapshot restore is consistent across widgets**

- 関連: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` の timeline layer snapshot 復元。
- 確認できた事実: timing setter の反映確認後も keyframe snapshot の bool 結果を無視して成功を返しており、timing の一部失敗時に旧値へ戻していなかった。
- 対応: timing 旧値を保持し、setter または keyframe 復元に失敗した場合は旧 timing へ戻して false を返すようにした。
- 価値/懸念: Track Painter と Timeline Widget の snapshot command 成功判定・補償境界が一致した。複数 layer 復元時の全体補償と runtime cache は未検証。
- 次に確認: slide／ripple／paste、対象消失、timing setter 拒否、keyframe setter 拒否、Undo／Redo、cache refresh を確認する。

### 2026-08-31 — Motion trajectory undo failure must propagate

- **関連ファイル・機能:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` / Motion Trajectory keyframe application
- **確認できた事実:** `applyTrajectoryToProperty()` は、キーフレーム適用後に `UndoManager::push()` が失敗した場合に before snapshot を復元していたが、最後は常に `true` を返していた。
- **気づき:** 履歴登録に失敗した編集を呼び出し側が成功と扱うと、UI 通知や後続処理が実状態と一致しない。復元処理後は `undoAccepted` を結果として返すべきである。
- **価値または懸念:** Undo の失敗を確実に上位へ伝え、操作成功表示や後続処理の誤実行を防ぐ。復元 API 自体の部分失敗は、別途ランタイム検証が必要。
- **次に確認すべきこと:** UndoManager が存在する状態で履歴上限・メモリ上限に達した場合、対象プロパティが before 状態へ戻り、呼び出し側が失敗を表示することをランタイムで確認する。

### 2026-08-31 — Localization mutation notification follows postcondition

- **関連ファイル・機能:** `Artifact/src/Service/ArtifactProjectService.cppm` / layer source localization and shared relink
- **確認できた事実:** localization／relink の直接経路は setter の戻り値を保持せず、状態確認と project mutation 通知へ進んでいた。失敗時にも通知が発生し得た。
- **対応:** 直接経路で操作結果を保持し、期待する localized state を確認できた場合だけ project mutation を通知して成功を返すようにした。Undo 経路も同じ postcondition を確認してから通知する。
- **価値または懸念:** 失敗操作による dirty／更新通知の誤発生を抑える。source identity の内部副作用と runtime UI 更新は未検証。
- **次に確認すべきこと:** manager 有／無、localize／relink 成功・拒否、Undo／Redo、dirty 状態、asset 表示更新を runtime で確認する。

### 2026-08-31 — Direct effect operations verify state and order

- **関連ファイル・機能:** `Artifact/src/Service/ArtifactProjectService.cppm` / layer effect direct fallback
- **確認できた事実:** UndoManager 不在時の effect enabled setter は反映後の値を検証せず通知していた。effect 並べ替えも再構築後の順序を確認せず成功扱いしていた。
- **対応:** enabled 状態を確認し、並べ替えは effect ID 列を比較して期待順序に一致しない場合は元の列へ復元して false を返すようにした。
- **価値または懸念:** direct fallback の部分失敗や表示順不一致が project／layer mutation 通知へ漏れるのを抑える。Core の effect container 更新通知と runtime 表示は未検証。
- **次に確認すべきこと:** manager 有／無、同一 pipeline stage、境界 index、setter／再構築拒否、Undo／Redo、Effects 面の表示更新を runtime で確認する。

### 2026-08-31 — Automation transform fallback verifies applied values

- **関連ファイル・機能:** `Artifact/include/AI/WorkspaceAutomation.ixx` / position, scale, rotation, and opacity writes
- **確認できた事実:** UndoManager 不在時の Automation setter は direct mutation 後に実値を検証せず、setter が拒否されても成功を返す可能性があった。
- **対応:** position／scale／rotation／opacity の direct fallback で実値を確認し、反映できない場合は旧値へ戻して false を返すようにした。
- **価値または懸念:** AI／外部自動化の成功応答と実状態の乖離を抑える。浮動小数点の変換境界と runtime の dirty／cache 通知は未検証。
- **次に確認すべきこと:** manager 有／無、通常値・境界値・setter 拒否、Undo／Redo、selection、保存／再読込、viewport 表示を runtime で確認する。

### 2026-08-31 — Automation layer state fallback verifies postconditions

- **関連ファイル・機能:** `Artifact/include/AI/WorkspaceAutomation.ixx` / visibility, lock, solo, shy, blend, opacity, parent, and note writes
- **確認できた事実:** layer state／note API の UndoManager 不在経路では direct setter 後の値を検証していない項目が残っていた。
- **対応:** direct fallback で反映後の状態を検証し、失敗時は変更前の状態へ戻して false を返すようにした。Parent は循環検証済みの旧 parent ID を復元する。
- **価値または懸念:** Automation の成功応答、通知、実状態の不一致を減らせる。Core setter の拒否時に復元 setter 自体が失敗する場合と runtime 通知は未検証。
- **次に確認すべきこと:** manager 有／無、各 state setter の拒否・境界値、parent 循環、Undo／Redo、dirty／selection／viewport 更新を runtime で確認する。

`setLayerOpacityInCurrentComposition()` には有限値の入力境界も追加し、NaN／無限大が clamp と setter を通過しないようにした。

### 2026-08-31 — Ripple delete failure restores removed layer

- **関連ファイル・機能:** `Artifact/include/AI/WorkspaceAutomation.ixx` / direct ripple delete
- **確認できた事実:** direct fallback は target layer を削除した後、後続 layer の timing 検証に失敗すると timing だけを復元し、target layer を元の composition index に戻していなかった。
- **対応:** 対象 index を削除前に記録し、複合検証失敗時に target layer を再挿入してから false を返すようにした。
- **価値または懸念:** direct fallback の構造と timing の部分適用を抑える。再挿入 setter／selection／cache の副作用は未検証。
- **次に確認すべきこと:** manager 有／無、削除拒否、timing setter 拒否、対象 index、後続 layer 複数、Undo／Redo、selection／cache を runtime で確認する。

### 2026-08-31 — Direct ungroup fallback is atomic

- **関連ファイル・機能:** `Artifact/include/AI/WorkspaceAutomation.ixx` / direct ungroup
- **確認できた事実:** direct ungroup は child の append に失敗した場合でも、先に移動できた child を外へ残し、部分成功を返していた。
- **対応:** 有効 child の全件移動を確認し、1件でも失敗した場合は composition 側の child を除去して全 child を group へ戻し、`success=false`／count 0 を返すようにした。
- **価値または懸念:** Group 構造の部分破壊と成功応答の乖離を抑える。元の layer 順序、selection、cache、復元 setter の runtime 挙動は未検証。
- **次に確認すべきこと:** manager 有／無、child append 拒否、空 group、複数 child、Undo／Redo、selection／cache を runtime で確認する。

### 2026-08-31 — Render queue Automation verifies structural mutations

- **関連ファイル・機能:** `Artifact/include/AI/WorkspaceAutomation.ixx` / render queue duplicate, move, and remove
- **確認できた事実:** queue service の一部 API は `void` を返すため、Automation は対象 index の事前妥当性だけで duplicate／move／remove を成功扱いしていた。
- **対応:** duplicate は job 数増加、move は job ID（ID がない場合は job payload）順序、remove は job 数減少と対象 ID 消失を確認してから成功を返すようにした。
- **価値または懸念:** queue service の拒否や部分反映を外部自動化へ成功として返しにくくなる。job ID の永続性、非同期 queue 更新、runtime UI 同期は未検証。
- **次に確認すべきこと:** duplicate／move／remove、境界 index、同一 index、実行中 job、queue refresh、失敗時の表示を runtime で確認する。

Queue への composition 追加と全削除も、実行前後の job 数を確認して成功応答を返すようにした。

### 2026-08-31 — Render queue scalar setters verify applied values

- **関連ファイル・機能:** `Artifact/include/AI/WorkspaceAutomation.ixx` / job name, output path, and frame range
- **確認できた事実:** queue の name／output path／frame range setter は `void` API で、Automation が呼び出し後の実値を確認せず成功を返していた。
- **対応:** 正規化後の期待値と getter 結果を比較し、失敗時は変更前の name／path／range を再適用して false を返すようにした。
- **価値または懸念:** queue 設定の拒否や clamp による成功応答の誤りを減らす。service 内部の非同期同期と frame range 正規化仕様は未検証。
- **次に確認すべきこと:** 各 setter の通常値・空値・境界値、実行中 job、失敗時の復元、UI refresh、保存／再読込を runtime で確認する。

Integrated render と audio source／codec／bitrate の queue setter も getter で適用結果を確認し、失敗時に旧値へ戻すようにした。

Render queue の output settings 一括 setter も、実際の format／codec／profile／解像度／FPS／bitrate を取得して検証し、差異があれば旧設定へ復元するようにした。

Render queue rerun reset は、Completed／Failed／Canceled 以外の job に対して成功を返さず、実行後に Pending へ戻ったことを検証するようにした。

### 2026-08-31 — Generic property response matches execution result

- **関連ファイル・機能:** `Artifact/include/AI/WorkspaceAutomation.ixx` / `setGenericLayerProperty()`
- **確認できた事実:** position／scale／rotation／opacity の setter が失敗しても、結果 map の `executed` は常に true だった。
- **対応:** setter の戻り値を一度保持し、`success` と `executed` の両方へ同じ結果を返すようにした。
- **価値または懸念:** Automation の post-operation verification が、失敗を実行済みと誤認しにくくなる。上位 batch 応答と runtime の error message は未検証。
- **次に確認すべきこと:** 正常値、無効 layer、非有限値、setter 拒否、batch 集計、Python／AI bridge の応答を確認する。
### 2026-08-31 — Render Queue backend automation reports normalized state

- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx` / render queue backend setter
- 確認できた事実: `ArtifactRenderQueueService` は指定 backend を正規化して保存し、取得 API で正規化済み値を返す。
- 気づき: Automation の setter が呼び出し完了だけで成功を返すと、無効値や内部更新失敗を成功として報告し得るため、取得値が空でないことを確認し、失敗時は旧 backend を復元するようにした。
- 価値/懸念: 自動化クライアントが実際の適用結果を誤認しにくくなる。backend の許容値そのものはサービス側の正規化責務に委ねている。
- 次に確認すべきこと: 実行時に有効な backend 値・無効値の正規化と復元を確認する（ビルド・実行未確認）。

### 2026-08-31 — Render Queue control automation rejects no-op status transitions

- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx` / render queue start, pause, cancel
- 確認できた事実: 個別の queue control service API は void で、開始は `Pending`、一時停止は `Rendering`、取消は `Pending` または `Rendering` を対象に状態を変更する。
- 気づき: Automation が対象状態を確認せず常に成功を返すと、完了済み job の取消や実行中でない job の一時停止を成功と誤認するため、許可される事前状態と変更後状態を確認するようにした。
- 価値/懸念: 呼び出し元へ no-op／対象外状態を失敗として返せる。全体操作は worker の非同期状態変化があるため、存在する対象の有無を事前に検証し、詳細な完了状態の断定は避けている。
- 次に確認すべきこと: queue worker 実行中の競合、個別／全体操作の status 通知、Automation 応答を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Playback scalar automation verifies service setters

- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx` / playhead, work area, speed, looping
- 確認できた事実: Playback service の対象 setter は void で、Automation 側は呼び出し後に成功を固定返却していた。
- 気づき: frame、work area、speed、looping の getter が既に存在するため、適用後の値を比較して失敗を返せる。speed は有限値かつ float 範囲内に限定した。
- 価値/懸念: Automation 呼び出し元が clamp／拒否／service 不在を成功と誤認しにくくなる。浮動小数点の exact compare は setter と getter が同じ float 値を扱う前提で、runtime 未確認。
- 次に確認すべきこと: composition 範囲外 frame の扱い、work area の境界、speed の丸め、looping 切替を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Playback navigation automation verifies destination state

- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx` / playhead navigation and in/out points
- 確認できた事実: Playback service の移動・in/out point setter/clearer は void だが、current frame と point getter、frame range getter が公開されている。
- 気づき: 呼び出し後の destination／point state を確認することで、範囲端での clamp や service 内部拒否を Automation の成功として返さないようにした。
- 価値/懸念: marker/chapter navigation を含む playhead 操作の応答が実状態と一致しやすくなる。再生中の非同期 frame 更新がある場合は runtime で確認が必要。
- 次に確認すべきこと: 再生中の移動、範囲端、marker/chapter の同一 frame、in/out point の履歴記録を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Playback marker automation verifies marker mutations

- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx` / marker and chapter operations
- 確認できた事実: In/Out point manager は marker lookup と marker count を公開しており、PlaybackService の marker mutation API は void だった。
- 気づき: marker追加後に現在frameの marker 実体・chapter種別を確認し、全marker削除後は marker count が0であることを確認するようにした。
- 価値/懸念: marker操作の失敗や対象manager不整合を固定成功として返しにくくなる。同一frameの既存marker更新は既存APIの仕様に従い成功と扱う。
- 次に確認すべきこと: marker snapshot undo、同一frame更新、chapter変換、外部Automationからの応答を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Playback frame-step automation rejects boundary no-ops

- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx` / next and previous frame
- 確認できた事実: frame-step service API は void で、範囲端では current frame が変化しない場合がある。
- 気づき: 実行前の frame と実行後の frame を比較し、範囲端で移動できなかった操作を成功扱いしないようにした。
- 価値/懸念: Automation の成功応答が実際の playhead 変化に一致する。同一frameを意図的に再設定する操作とは異なり、frame-step は変化を伴う操作として扱う。
- 次に確認すべきこと: work area端・composition端・再生中の frame-step 応答を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Empty-group removal propagates postcondition failure

- 関連: `Artifact/src/Service/ArtifactProjectService.cppm` / `ungroupSelectedGroupWithUndo`
- 確認できた事実: 空groupの解除は `RemoveLayerCommand` を push した後、groupがcompositionから消えたかを確認していたが、失敗時に履歴を戻していなかった。
- 気づき: push成功と実体削除成功を分離し、削除後のpostconditionに失敗した場合は直前のUndoエントリをUndoして失敗を返すようにした。
- 価値/懸念: 空group削除の部分適用が履歴に残りにくくなる。selection復元の完全性と runtime 挙動は未確認。
- 次に確認すべきこと: 空groupの削除成功、command適用失敗、Undo後のselection／current layerを runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Render Queue automation verification follows normalization contracts

- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx`, `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 確認できた事実: queue service は output format／codec／profile を既定値・trim・container変換で正規化し、解像度・FPS・bitrate・audio bitrate を所定範囲へ clamp する。
- 気づき: Automation の postcondition は入力値との単純な完全一致ではなく、service が返す正規化済み値の有効性を確認する必要がある。audio path／codecも同じ trim・既定値契約へ合わせた。
- 価値/懸念: 有効な入力を誤って失敗扱いせず、更新失敗や異常な取得値は復元対象にできる。正規化仕様が変更された場合はこの検証境界も更新が必要。
- 次に確認すべきこと: 空値、長大値、範囲外数値、container変換、復元失敗時の queue 状態を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Remove-all-assets automation verifies project scope and result

- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx` / `removeAllAssets`
- 確認できた事実: ProjectService の remove-all-assets API は void で、Automation は project 不在でも呼び出し後に固定成功を返していた。
- 気づき: 現在 project の存在を確認し、project tree の Footage／Solid 件数を変更前後で比較して、対象assetが消えたことを成功条件にした。assetが元々ない場合は no-op 成功とする。
- 価値/懸念: project不在・内部失敗・部分削除を成功と誤認しにくくなる。CompositionやFolderは削除対象外であるため、asset型だけを数えている。
- 次に確認すべきこと: nested folder、空project、削除失敗、参照中asset、safe-write confirmation後の応答を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Remove-all-assets dry run reports actual asset count

- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx` / `dryRunRemoveAllAssets`
- 確認できた事実: safe-write の dry-run が常に `wouldChange=true`、asset件数 `-1`、project不在でも失敗なしとして計画を返していた。
- 気づき: project tree を再帰走査する共通 helper を追加し、Footage／Solid の実数、変更有無、project不在時の失敗状態を dry-run に反映した。
- 価値/懸念: 確認ダイアログと監査情報が対象規模に一致する。参照関係の安全性は既存警告どおり別途 snapshot／runtime確認が必要。
- 次に確認すべきこと: nested folder、空project、compositionを含むtree、confirmation auditの表示を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Confirmed remove-all-assets uses the same preflight scope

- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx` / `removeAllAssetsConfirmed`
- 確認できた事実: dry-run はasset件数を算出する一方、confirmation実行側は service の存在だけで `wouldChange` を決め、project不在を正しく表現していなかった。
- 気づき: confirmation gate 側も同じproject／asset countを使うようにし、dry-runと実行監査の対象範囲を一致させた。
- 価値/懸念: project不在や空projectを削除操作の実行対象として誤表示しにくくなる。依存asset参照の復元は依然として自動Undo対象外。
- 次に確認すべきこと: dry-run→confirmationの連続操作、空project、nested folder、監査ログの一貫性を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Playback control automation verifies state transitions

- 関連: `Artifact/include/AI/WorkspaceAutomation.ixx` / playback start, pause, stop, toggle
- 確認できた事実: PlaybackService の制御APIは void で、engine／controllerの状態取得APIが既に存在する。再生開始は `Playing`、pauseは `Paused`、stopは `Stopped` へ遷移する。
- 気づき: Automation の固定成功をやめ、各制御操作後の `PlaybackState` を確認するようにした。toggle は実行前の状態から期待状態を決める。
- 価値/懸念: composition未接続、対象状態外、engine／controller不整合を成功と誤認しにくくなる。再生開始や停止の非同期更新がある場合は runtime 確認が必要。
- 次に確認すべきこと: engine／controller両経路、再生中のtoggle、composition未接続、停止時のcurrent frameを runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Playback state read automation uses the canonical service state

- 関連: Artifact/include/AI/WorkspaceAutomation.ixx / playbackGetState
- 確認できた事実: playbackGetState は engine専用の isPlaying()／isPaused() を使っており、PlaybackService が公開する controller対応の state() と観測経路が異なっていた。
- 気づき: read-only stateも制御操作と同じ PlaybackState 正本から "playing"／"paused"／"stopped" へ変換するようにした。
- 価値/懸念: Automation、UI、controller／engine間で状態観測が食い違いにくくなる。Buffering／Errorの公開語彙は既存の3状態契約を維持して stopped 相当とする。
- 次に確認すべきこと: controller fallback、Buffering／Error、Python bridgeの state 応答を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Edit Menu cut reports deletion outcome

- 関連: `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm` / Cut and Delete actions
- 確認できた事実: Cut は Copy 後に Delete を呼び出し、Delete は void だったため、削除失敗や部分失敗でも `ClipCutEvent` を発行していた。
- 気づき: Delete が全対象の削除結果を返すようにし、Cut は全件成功した場合だけ cut event を通知するようにした。通常のDelete actionは既存のUI呼び出しを維持する。
- 価値/懸念: Clipboard上のコピー済みデータと実際の削除状態を外部通知が誤って同一視しにくくなる。複数選択の途中失敗後の選択表示は既存のclear方針を維持している。
- 次に確認すべきこと: 複数layerの一部削除失敗、Undo後のselection、Clip Buffer表示、cut event受信側を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Edit Menu delete restores failed-layer selection

- 関連: `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm` / Delete action
- 確認できた事実: 複数選択の削除は対象ごとの結果を集計していたが、部分失敗時も最後に全選択を消去していた。
- 気づき: 削除に失敗したlayerを記録し、処理後にcompositionへ残っている失敗対象だけ再選択するようにした。Cutの失敗復旧でも同じ選択が残る。
- 価値/懸念: ユーザーが失敗対象を確認して再試行しやすくなる。削除成功対象のselectionは従来どおり解除される。
- 次に確認すべきこと: 複数layerの部分失敗、Undo／再試行、selection event、Cut後のclipboard内容を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Edit Menu paste becomes one structural Undo operation

- 関連: `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm` / Paste action
- 確認できた事実: Paste は複数layerを直接appendし、anchor位置・parent／clone／matte参照を更新していたが、Undo commandや選択snapshotを持っていなかった。
- 気づき: 貼り付け後の実レイヤー順を記録し、既存の `AddLayerCommand` と `MoveLayerIndexCommand`、`LayerSelectionSnapshotCommand` を一つの `Paste Layers` macroへまとめた。push失敗時は追加レイヤーと貼り付け後選択を復元する。
- 価値/懸念: 貼り付け全体を一回のUndoで戻せ、失敗時に部分構造を残しにくくなる。parent／clone／matteのID remapは既存の貼り付け処理を再利用しているため、session reloadと複雑な階層のruntime確認が必要。
- 次に確認すべきこと: 単一／複数paste、anchor位置、nested parent、clone／matte参照、Undo／Redo、selection復元を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Project View rename reuses ProjectService Undo boundary

- 関連: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` / Project View rename helper
- 確認できた事実: inline／context rename helper は非Composition項目の `name` を直接変更して `projectChanged()` しており、ProjectServiceの `RenameProjectItemCommand` を迂回していた。
- 気づき: helper の入力検証は維持し、実際の変更を `ArtifactProjectService::renameProjectItem()` へ委譲するようにした。これで inline／contextの両UI経路が同じUndo・postcondition経路を使う。
- 価値/懸念: rename操作のUndo境界と失敗判定をUI間で統一できる。selection、別project切替、session reload、runtimeは未確認。
- 次に確認すべきこと: inline編集、context rename、Composition／Folder／Footage、Undo／Redo、失敗時のeditor表示を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Project View tag edits reuse the project Undo boundary

- 関連: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` / Project View Edit Tags
- 確認できた事実: context menuのタグ編集だけが `ProjectItem::tags` を直接変更し、ProjectService／UndoManagerを経由していなかった。
- 気づき: `SetProjectItemTagsCommand` と `ArtifactProjectService::setProjectItemTags()` を追加し、旧タグ・新タグを一つのUndo操作として保存するようにした。UndoManagerなしの経路でも適用後のタグを検証してから変更通知する。
- 価値/懸念: タグ検索対象の更新をUndo／Redo可能なプロジェクト編集として扱える。タグ正規化、別project切替、session reload、runtimeは未確認。
- 次に確認すべきこと: 空タグ、重複タグ、大文字小文字違い、Undo／Redo、Project View検索更新を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Project View footage roles reuse the project Undo boundary

- 関連: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` / Footage Input Source Role
- 確認できた事実: Production／Render Input と Render Input Role の変更が `FootageItem` のフィールドを直接変更し、dirty通知とUndo境界をUI側で個別に処理していた。
- 気づき: `SetFootageAssetRoleCommand` と `ArtifactProjectService::setFootageAssetRole()` を追加し、usage／roleの組を一つのUndo操作として保存・復元するようにした。
- 価値/懸念: 入力ソース役割変更のUndo／Redoと失敗判定を他のProject View編集と統一できる。選択変更、再読込、runtimeは未確認。
- 次に確認すべきこと: Production、各Render Input Role、Undo／Redo、検索・再読込後の表示を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Responsive variant selection reports project changes

- 関連: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` / Project View Responsive Layout menu
- 確認できた事実: Variant切替は composition setter に直接到達していたが、変更後にProjectServiceのcomposition settings finalize経路を呼んでいなかった。
- 気づき: setter前後のactive variant IDを比較し、実際に変化した場合だけ `finalizeCompositionSettingsChange()` を呼ぶようにした。無効なIDや同一IDでは通知しない。
- 価値/懸念: Variant切替がdirty状態・プロジェクト変更通知から漏れにくくなる。Responsive Layout全体のUndoスナップショットは既存のresolution/settings commandとの重複を避ける設計検討が必要で、未実装。
- 次に確認すべきこと: Variant切替、同一／無効ID、保存・再読込、composition size連動を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Project item insertion is one transactional Undo operation

- 関連: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` / project item paste and Parametric Composition creation
- 確認できた事実: Project View の項目貼り付けとParametric Composition作成は `addProjectItemsFromJson()` を直接呼び、複数項目または生成項目を一つのUndo操作として扱っていなかった。
- 気づき: `AddProjectItemsCommand` を追加し、JSON payloadと親IDを保存してRedoで再生成、Undoでトップレベル項目を撤去するようにした。Redoの部分追加とUndoの途中失敗では、確認できた項目を撤去／スナップショット復元して原子性を保つ。Composition追加時はbefore／afterのcurrent composition IDも保存し、Undoで削除済みIDをcurrentに残さない。
- 価値/懸念: Project Viewの貼り付けとParametric Composition作成を一回のUndoで戻せる。トップレベル項目IDを持たない外部payloadは安全のため拒否され、現在のUI clipboardはIDを含む。current composition選択のUndo復元とruntimeは未確認。
- 次に確認すべきこと: 単一／複数貼り付け、folder階層、Parametric Composition、Undo／Redo、保存・再読込、部分失敗を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Multi-composition settings share one Undo transaction

- 関連: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` / `applySelectedCompositionSettings`
- 確認できた事実: 複数Compositionへのサイズ、FPS、Frame Range、背景色変更は各compositionへ直接setterを呼び、Undo境界がなく、途中失敗時に部分適用が残り得た。
- 気づき: サイズ（リマップを伴わない場合）、FPS、Frame Range、背景色の旧／新値を `SetCompositionSettingsCommand` に保存し、複数対象を一つのMacroへ集約した。コマンドは各setter後の値を検証し、Macro失敗時は既存の補償経路へ渡す。
- 価値/懸念: 単一／複数Composition設定を一回のUndoで戻せる。リマップ付き変更は専用resolution commandを同じMacroへ含めるため、レイヤー変換も保持される。current composition選択のUndo復元とruntimeは未確認。
- 次に確認すべきこと: 複数対象の一括設定、同値入力、途中失敗、Undo／Redo、再生中Compositionのframe range同期を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Context Composition Settings share the same Undo transaction

- 関連: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` / Project View context Composition Settings
- 確認できた事実: Context Menu側のSettingsダイアログだけが名前、解像度リマップ、FPS、Frame Range、背景色を直接変更し、通常のSettings編集と異なるUndo境界を持っていた。
- 気づき: 既存の `ChangeCompositionResolutionCommand`、`SetCompositionSettingsCommand`、`RenameCompositionCommand` を同じ Macroへ追加し、リマップ選択時もレイヤー変換を含む一操作に統合した。背景色のbefore値はプレビューで既にcompositionへ反映されるため、ダイアログ開始時の `originalBackgroundColor` から採取する。キャンセル時プレビュー復元は従来どおり保持する。
- 価値/懸念: Context／通常UIでComposition SettingsのUndo単位と失敗経路を統一できる。背景色プレビュー中の外部変更、current composition選択のUndo復元、runtimeは未確認。
- 次に確認すべきこと: Context Settingsのキャンセル、リマップ有／無、名前変更失敗、Undo／Redo、再生同期を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Responsive layout edits use JSON snapshot Undo

- 関連: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` / Responsive Layout menu
- 確認できた事実: Variant追加・複製・編集・切替は `setResponsiveLayout()` または active ID setterを直接呼び、Variant集合全体のUndo境界がなかった。
- 気づき: `SetCompositionResponsiveLayoutCommand` を追加し、`ResponsiveLayoutSet::toJson()` のbefore／afterを保存してRedo／Undoする共通helperへ4経路を移行した。setterの正規化後JSONを検証し、UndoManagerなしでも変更通知を行う。
- 価値/懸念: Variant集合、active variant、レイアウト由来のcomposition size変更を一回のUndoで復元できる。外部変更との競合、runtime、選択中compositionの追加同期は未確認。
- 次に確認すべきこと: Variant追加・複製・編集・切替、無効ID、Undo／Redo、保存・再読込を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Bundle IPC project-item insertion shares UI Undo

- 関連: `Artifact/src/Application/ArtifactProjectBundleIpc.cppm` / project-items and parametric-composition bundle paste
- 確認できた事実: IPC経由のProject Item／Parametric Composition貼り付けは `addProjectItemsFromJson()` を直接呼び、Project View UIのUndo経路と分かれていた。
- 気づき: UIと同じ `AddProjectItemsCommand` をローカル適用へ接続し、Parametric payloadにも項目IDを付与した。UndoManagerが利用できない場合は既存の直接追加へフォールバックする。
- 価値/懸念: UI貼り付けとIPC貼り付けのUndo単位を統一できる。Layer／Composition bundleの構造的Undo、外部送信元との競合、runtimeは未確認。
- 次に確認すべきこと: IPC project-items、Parametric bundle、Undo／Redo、保存・再読込、無効／重複ID payloadを runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — Bundle IPC composition insertion shares project-item Undo

- 関連: `Artifact/src/Application/ArtifactProjectBundleIpc.cppm` / composition bundle paste
- 確認できた事実: Composition Bundleだけが `addImportedComposition()` を直接呼び、Project Item／Parametric Bundleとは異なるUndo経路を持っていた。
- 気づき: composition JSONをProject Item payloadへ組み立て、項目IDを付与して `AddProjectItemsCommand` へ接続した。UndoManagerなしでは既存の `addProjectItemsFromJson()` を使い、成功後のcurrent composition設定を維持する。
- 価値/懸念: IPCの3種類のプロジェクト項目追加が同じ構造Undo境界になる。Composition container・render queue・current composition選択のUndo復元、runtimeは未確認。
- 次に確認すべきこと: Composition Bundleの追加、重複composition ID、Undo／Redo、current composition、保存・再読込を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — IPC layer bundle paste becomes transactional

- 関連: `Artifact/src/Application/ArtifactProjectBundleIpc.cppm` / layer bundle paste
- 確認できた事実: IPCのLayer Bundle貼り付けは生成、並び替え、選択、parent／Clone／Matte IDリマップを直接適用し、失敗時に部分構造や選択を戻すUndo境界がなかった。
- 気づき: 既存の生成・リマップ処理後に `AddLayerCommand`、`MoveLayerIndexCommand`、`LayerSelectionSnapshotCommand` を一つのMacroへまとめる経路を追加した。コマンド化前に一度取り外し、Redo後の存在・順序を検証し、push失敗や検証失敗時はレイヤーと選択を復元する。
- 価値/懸念: IPC Layer BundleもUI Paste Layersと同じUndo単位になる。複雑な親子構造、Clone／Matte参照、remote failure、runtimeは未確認。
- 次に確認すべきこと: 単一／複数layer、anchor位置、親子、Clone／Matte、Undo／Redo、部分失敗を runtime で確認する（ビルド・テスト未確認）。

### 2026-08-31 — AppMain clip paste shares the layer Undo boundary

- 関連: `Artifact/src/AppMain.cppm` / ClipPasteRequestedEvent
- 確認できた事実: AppMainのクリップ貼り付け経路はレイヤー生成、参照リマップ、選択を直接適用しており、Composition Editor／IPCの貼り付け経路とは別にUndo境界がなかった。
- 気づき: 既存の `AddLayerCommand`、`MoveLayerIndexCommand`、`LayerSelectionSnapshotCommand` をMacroへまとめ、リマップ後のレイヤーを一度取り外して再実行可能な状態にした。UndoManagerなしでは従来どおり直接状態を復元する。
- 価値/懸念: AppMain経由のクリップ貼り付けも複数レイヤーを一回でUndoできる。選択マネージャー間の同期、親子／Matte参照、runtimeは未確認。
- 次に確認すべきこと: ClipPasteRequestedEventの単一／複数layer、親子・Clone・Matte、Undo／Redo、UndoManager不在時の復元をruntimeで確認する（ビルド・テスト未確認）。

### 2026-08-31 — Debug particle menu insertion uses the shared layer command

- 関連: `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm` / Debug Particle Layer
- 確認できた事実: デバッグ用Particle Layerのメニュー追加だけが `appendLayerTop()` を直接呼び、通常のレイヤー追加と異なるUndo境界を持っていた。
- 気づき: 既存の `AddLayerCommand` を共通適用ヘルパーへ接続し、UndoManagerなしでは同じcommandの直接Redoへフォールバックするようにした。追加失敗時は選択・完了通知を行わない。
- 価値/懸念: メニューから生成するデバッグレイヤーも通常のレイヤー追加と同じUndo単位になる。runtime未確認。
- 次に確認すべきこと: Debug Particle Layerの追加、Undo／Redo、生成済みpresetと配置フレーム、UndoManager不在時の動作をruntimeで確認する（ビルド・テスト未確認）。

### 2026-08-31 — Crop/Pan quick actions use property snapshot Undo

- 関連: `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` / Crop / Pan section
- 確認できた事実: Crop/Panの有効化、リセット、無効化ボタンは複数のレイヤープロパティを直接変更していたが、通常のプロパティ行以外のUndo境界がなかった。
- 気づき: 変更対象のbefore／after値を取得し、既存の `SetLayerPropertyValueCommand` をMacroへまとめる共通処理を追加した。Undo push失敗時はbefore値へ戻し、UndoManagerなしでは直接変更を維持する。
- 価値/懸念: Crop/Panの一連のボタン操作を一回でUndoできる。プロパティのキーフレーム状態、外部同時変更、runtimeは未確認。
- 次に確認すべきこと: 初回有効化時のcrop初期化、リセット、無効化、Undo／Redo、無効プロパティやUndoManager不在時の動作をruntimeで確認する（ビルド・テスト未確認）。

### 2026-08-31 — Cloner transform stack edits gain a structural snapshot command

- 関連: `Artifact/include/Layer/ArtifactAbstractLayer.ixx`, `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `Artifact/include/Undo/UndoManager.ixx`, `Artifact/src/Undo/UndoManager.cppm`, Inspector / Property Editor
- 確認できた事実: Clonerの追加・削除・複製・並べ替えは `component.cloner.transforms.*` の特殊setterで内部vectorを変更するため、通常の単一プロパティ値Undoでは削除後の要素数やindexを復元できなかった。
- 気づき: Cloner transform配列専用のJSON snapshot APIと `ClonerTransformStackSnapshotCommand` を追加し、Inspector／Property Editorの4操作をbefore／after配列の一回のUndo境界へ接続した。復元後の配列を比較し、失敗時は反対側へ戻す。
- 価値/懸念: Cloner構造の追加・削除・複製・順序変更を安全にUndo／Redoできる。JSON正規化、外部同時変更、runtimeは未確認。
- 次に確認すべきこと: 空配列、単一／複数transform、編集済み値を含む複製、Undo／Redo、セッション履歴の保存・再読込をruntimeで確認する（ビルド・テスト未確認）。

### 2026-08-31 — Cloner snapshots are exposed through the layer boundary

- 関連: `Artifact/include/Layer/ArtifactAbstractLayer.ixx` / `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- 確認できた事実: Cloner transform内部vectorは特殊setterからしか操作できず、Undo command側から配列全体を検証付きで復元する公開境界がなかった。
- 気づき: transform配列のJSON snapshot取得・復元APIをLayerへ追加し、Undo側の専用commandがその境界だけを利用する構造にした。復元は全要素を検証してから置換し、範囲制限後の配列を比較する。
- 価値/懸念: UIが内部vectorへ依存せず構造編集をUndoできる。snapshot JSONの正規化、他コンポーネントとの同時変更、runtimeは未確認。
- 次に確認すべきこと: 不正JSON、空配列、数値境界、Undo／Redo、履歴保存・再読込をruntimeで確認する（ビルド・テスト未確認）。

### 2026-08-31 — Inspector component toggles use value Undo

- 関連: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` / component enable toggles
- 確認できた事実: Physics／Script／Layout／Cloner／Fluid の有効・無効切替はレイヤーのBoolean propertyを直接変更していた。
- 気づき: before／after値を `SetLayerPropertyValueCommand` へ渡し、UndoManagerなしでは同じcommandのRedoへフォールバックするようにした。UIのフォーカス更新はcommand成功後だけ行う。
- 価値/懸念: Componentの有効状態切替を通常の編集Undo単位へ統一できる。依存コンポーネントの副作用、runtimeは未確認。
- 次に確認すべきこと: 各Componentの切替、Undo／Redo、無効プロパティ、UndoManager不在時の表示更新をruntimeで確認する（ビルド・テスト未確認）。

### 2026-08-31 — Inspector descriptor stacks share one component snapshot boundary

- 関連: `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` / Generator・Field・Clone Modifier actions
- 確認できた事実: Generator／Field／Clone Modifier の追加・削除・並べ替えは内部descriptorコンテナを特殊setterで変更し、通常の値Undoでは要素の追加削除や順序を復元できなかった。
- 気づき: Cloner Transformを含むdescriptor配列全体のJSON snapshot APIと専用Undo commandを追加し、Inspectorの各構造操作をbefore／afterの一回のUndo境界へ接続した。復元前に全descriptorを検証し、最大要素数も制限する。
- 価値/懸念: Inspectorから編集できる複数のComponent構造を、要素数・内容・順序込みでUndo／Redoできる。Effector chainは別のCloneLayer所有配列のため未対応、runtimeは未確認。
- 次に確認すべきこと: Generator／Field／Modifierの追加・削除・順序変更、Undo／Redo、無効JSON、履歴保存・再読込、Effector chainの専用境界をruntimeで確認する（ビルド・テスト未確認）。

### 2026-08-31 — Clone Effector chain edits use typed snapshots

- 関連: `Artifact/include/Layer/ArtifactCloneLayer.ixx`, `Artifact/src/Layer/ArtifactCloneLayer.cppm`, `Artifact/include/Undo/UndoManager.ixx`, `Artifact/src/Undo/UndoManager.cppm`, `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- 確認できた事実: Clone Effectorの追加・削除は `ArtifactCloneLayer` の内部vectorを直接変更していたが、保存／読込用の型タグ付きJSON変換と復元処理は既に存在した。
- 気づき: 既存JSON形式を公開snapshot APIへ切り出し、専用Undo commandでチェーン全体を検証付き復元するようにした。未知の型、要素数超過、復元後の不一致を拒否し、push失敗時はbeforeへ戻す。
- 価値/懸念: Effectorの種類・順序・設定値を含むチェーン全体を一回のUndoで扱える。Effector内部の時刻依存評価、外部同時変更、runtimeは未確認。
- 次に確認すべきこと: 各Effector型の追加・削除、設定変更後のUndo／Redo、空チェーン、無効型、履歴保存・再読込をruntimeで確認する（ビルド・テスト未確認）。

### 2026-09-01 — Parent main references an unavailable ArtifactCore commit

- 関連: `ArtifactStudio` の `origin/main` 更新、子モジュールの開発ブランチ作成
- 確認できた事実: 最新親 `origin/main` は `ArtifactCore` の `c5c2d984...` を参照するが、子リモートから取得できず、`ArtifactWidgets` も親の gitlink と子 `origin/main` が一致しない。
- 気づき: 親子を同一開発ブランチへ作成する操作は可能だが、親最新コミットを完全に再現するには欠落した `ArtifactCore` オブジェクトの公開または参照修正が必要。
- 価値/懸念: 取得不能な gitlink を推測で置換すると親子の履歴整合性を壊すため、現状は差分を残して停止するのが安全。
- 次に確認すべきこと: `c5c2d984...` が存在するリモート／ブランチを確認し、取得後に親 gitlink を再同期する。

### 2026-09-01 — Track matte GPU pass still depends on CPU-resolved sources

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`, `ArtifactCore/include/Graphics/Shader/Compute/HLSL/MatteTrack.ixx`
- 確認できた事実: Alpha/Luma、反転、opacity、最大3ソースの合成を行うGPU compute passは存在するが、マット元は `QImage` として解決・リサイズされ、毎フレームGPUへアップロードされる。マット元のレイヤー変換・マスク・エフェクト込みのコンポジション空間レンダーではない経路が残る。
- 気づき: GPU化の次段は切り抜きシェーダーの追加ではなく、マット元を同一フレームのGPU surface／textureとして解決し、対象と同じ座標・色・アルファ契約で参照することが本質になる。
- 価値/懸念: 現状は静止画の単純な同サイズマットには使えるが、変換・ネスト・3D・動的エフェクト・高解像度連番では表示差、CPU転送コスト、欠落時の未マスク fallback が受入リスクになる。未検証の評価。
- 次に確認すべきこと: マット元の transform／mask／effect／visibility／time を含むGPU surface cache契約、Rec.601/709/2020の色空間、異サイズのUV変換、欠落時の安全な出力、CPU/GPU parityをケース別に受入確認する。

### 2026-09-01 — Simple same-size precomp mattes can reuse GPU SRV

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 確認できた事実: 既存 `renderPrecomp2DGpuOutput()` は親コンポジションと同じサイズのGPUオフスクリーンSRVを返せる。Transform、Mask、Effect、自身のmatteを含む一般ケースの座標／後処理契約は別途必要。
- 気づき: GPUトラックマットのsource入力へ、このSRVを直接渡す接続を追加した。ただし同一サイズ、global Transform identity、Mask／Effect／matteなしのPrecompだけに限定し、条件外は既存QImage経路へ戻す。
- 価値/懸念: 単純PrecompではCPU画像化と再アップロードを避け、ネスト結果をGPU上のsourceとして利用できる。完全な一般レイヤーGPU surface化ではなく、GPU出力サイズと親座標の一致をruntimeで確認する必要がある。
- 次に確認すべきこと: 同一サイズPrecompのAlpha／Luma、透明境界、フレーム切替、GPU／CPU parityを実機で確認し、次にTransform付きsourceのUV／オフスクリーン描画契約へ拡張する。

### 2026-09-01 — GPU precomp matte fast path requires neutral placement

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 確認できた事実: Precomp GPU SRVは親レイヤーのfit、global Transform、opacityを別途適用しないため、これらが非ニュートラルなsourceを直接渡すとCPU経路と結果が一致しない。
- 気づき: GPU直接参照条件を、Stretch、identity Transform、opacity 1、親子同一サイズ、GPU出力サイズ一致、Mask／Effect／matteなしへ限定した。
- 価値/懸念: GPU fast pathの適用範囲は狭くなるが、座標・濃度の黙った不一致を避けられる。Transform付きsourceは専用の親空間オフスクリーン合成が必要で、runtime未検証。
- 次に確認すべきこと: 条件境界のCPU/GPU parityを実機で確認し、viewport downsampleを含む一般source向けのUV／transform適用surfaceを設計する。

### 2026-09-01 — Matte sources now have a GPU intermediate render boundary

- 関連: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 確認できた事実: 既存の `drawGpuLayerToIntermediate()` は任意のsource layerをcolor／depthのオフスクリーンRTへ描画できる。マット適用はその後の `LayerBlendPipeline::applyTrackMatte()` で行われる。
- 気づき: source IDごとのcolor／depth出力を保持する `matteGpuOutputs_` と、frame／surface generation／sizeをキーにした再利用境界を追加し、GPU source SRVをtrack matteへ優先して渡すようにした。
- 価値/懸念: Transform込みのsource surfaceをGPUで供給できる段階になった。GPU直接経路は現在 `Stretch` 配置に限定し、他のFitModeはCPU経路へ戻す。source自身のmatteは現状のresolver経路、rasterizer effectは既存の内部fallbackに依存し、RT状態復元、source再帰、色／アルファ parityはruntime未検証。
- 次に確認すべきこと: 変換付き画像・Precomp・Mask／Effect付きsource、複数matte、viewport downsample、GPU失敗時のdiagnosticを実機で確認する。

### 2026-09-01 — Render queue still has an independent QImage matte path

- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- 確認できた事実: GPUレンダーキューのsource収集は依然として `QHash<ArtifactCore::Id, QImage>` と `renderLayerSurface()` を使い、`drawLayerForCompositionView()` へ画像マップを渡している。ビューアのGPU intermediate surface cacheとは別経路である。
- 気づき: ビューア側だけGPU source surface化しても、プレビューと書き出しでTransform／Mask／Effect付きtrack matteの結果が分岐し得る。両経路を同じsource surface resolverへ寄せる必要がある。
- 価値/懸念: レンダーキューへビューア内部クラスを直接依存させず、共通のrenderer-facing surface resolverをArtifact側のRender層へ切り出すのが妥当。未実装・未検証。
- 次に確認すべきこと: `ArtifactCompositionViewDrawing` またはRender共通層へ、source layer、frame、target size、camera、matte recursion policyを受ける共有APIを設計する。

### 2026-09-01 — Container debug notes can provide an AI-readable runtime trail

- 関連: `ArtifactCore/include/Container/ContainerDebug.ixx`, `ArtifactCore/include/Container/NamedVector.ixx`, `ArtifactCore/include/Container/ContainerDebugJson.ixx`
- 確認できた事実: 既存のコンテナ診断は mutation、failed access、source location、snapshot、JSON 出力を持つが、実行中に自由記述を残す共通APIはなかった。
- 気づき: `ContainerDebugNote` に epoch milliseconds、本文、source location を持たせ、`NamedVector::addDebugNote()` から最大32件を保持すると、将来のAI動的デバッグが時系列の判断材料を取得できる。
- 価値/懸念: JSON snapshot と既存の診断テキスト経路へ拡張しやすい。現時点では `NamedVector` のみが書き込みAPIを公開し、スレッド同期と永続化は未検証・未対応。
- 次に確認すべきこと: 他の自作コンテナへの同API展開、同時書き込み方針、note容量・個別本文サイズ制限、AIツールからのsnapshot取得経路を設計する。

### 2026-09-01 — AI container debugging needs an explicit, read-only registry before edit exposure

- 関連: `ArtifactCore/include/Container/ContainerDebugRegistry.ixx`, `ArtifactCore/include/Container/NamedVector.ixx`
- 確認できた事実: `NamedVector` は診断snapshotを生成できるが、AIや診断UIが安全に対象を列挙・取得する共通の公開境界はなかった。
- 気づき: registryは所有権を持たず、所有側が登録したsnapshot readerのみをIDで呼び出す。`Registration`はRAIIで解除し、registry消滅後も弱参照で安全に無効化する。値復帰は`NamedVector`だけに限定し、checkpointの発行元アドレスとAIが期待する現在versionを照合して他インスタンスへの誤適用・競合状態の上書きを拒否する。
- 価値/懸念: AI公開対象を明示的にホワイトリスト化でき、直接ポインタや生の要素編集を露出しない。メモは1件1024 bytes・最大32件に制限する。registryの登録・解除・列挙は同期化し、reader実行中はregistryロックを保持しない。解除処理は進行中inspectionの終了を待つため、Registrationを先に破棄してから対象コンテナを破棄できる。対象コンテナ自体のsnapshot同期、AIからの編集API、アプリ全体のUndo接続は未対応。
- 次に確認すべきこと: registryは登録IDとsnapshotを構造化JSONでexportでき、`debug.containers` MCP toolから取得できる。`debug.containers.annotate` はAI author固定の上限付きメモ追記だけを許可する。実際に公開するドメイン別adapterを明示登録し、可逆操作には専用Undo境界を置く。並行読み取りが必要な対象では所有側でsnapshot用の同期契約を定義する。
### 2026-09-01: RenderQueueの直接描画分岐はマット適用を迂回し得る
- 関連: `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` / `drawLayerForCompositionView`
- 事実: Image/SVG/Video/Text/Particle と単純な Solid の一部は、ラスタライザ効果・マスクが無い場合に GPU/直接描画へ進み、従来は有効なトラックマットの有無を判定していなかった。
- 対応: 有効なマット参照がある場合は既存の `applySurfaceAndDraw` 経路へ送り、QImage ソースマップを使ったマット適用を通すよう条件を狭く修正した。Video はGPUフレーム直接描画を避けてフレームバッファをQImage境界へ送り、Text/Image/SVG/Particle も同様に直接描画を抑制する。
- 価値/懸念: CPU/RenderQueue側で単純レイヤーのマットが無視される穴を塞げる。一方、QImage境界は残るため、これはRenderQueue全体のGPU化完了ではなく、GPU中間面へ移行できないケースの正確性確保である。
- 次に確認: マット付き各レイヤー種別（Solid/Image/SVG/Video/Text/Particle/Shape/FormParticle）で、GPUソース中間面とQImageフォールバックのアルファ結果を実フレーム比較する。未検証。

### 2026-09-01: Shapeもマット付きではサーフェス境界が必要
- 関連: `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` / `ArtifactShapeLayer`
- 事実: Shapeは通常 `layer->draw(renderer)` の汎用フォールバックへ到達するが、既存の `toQImage()` によるサーフェス表現も持つ。
- 対応: 有効なマット参照がある場合だけ `toQImage()` と既存の `applySurfaceAndDraw` を使い、マット適用を経由させた。
- 価値/懸念: ShapeのCPU/RenderQueue経路でマットが抜ける可能性を減らせる。FormParticleは汎用GPUオフスクリーン経路では対象にできるが、CPUフォールバック側の専用サーフェス化は未対応。
- 次に確認: ShapeのGPU描画結果と `toQImage()` 境界結果の座標・アンチエイリアス差を実フレーム比較する。

### 2026-09-01: RenderQueueの単一GPUフレームにもMatteTrack中間面を導入
- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` / `Impl::renderSingleFrame`
- 事実: GPUレンダーキューは従来、マットソースとターゲットをQImageへ落としてから共通描画へ渡していた。
- 対応: Stretch配置、最大3ソース、可視・アクティブ・非Adjustment・非Group親・自身のマットなしという安全条件を満たすソースをオフスクリーンGPU面へ描画し、対象レイヤーもオフスクリーン化して `ArtifactIRenderer::applyTrackMatte()` 後に本体ターゲットへ描画する経路を追加した。GPUパイプラインまたは条件が成立しない場合は既存QImage経路へ戻す。
- 価値/懸念: RenderQueueの新しい単一フレームGPU経路でも、通常の2DマットをCPU readbackなしで処理できる範囲が広がる。複雑なソースの再帰合成、非StretchのFit/Fill/Original、旧GPUループ、FormParticleの専用キャッシュは未対応。
- 次に確認: GPU経路で出力ターゲットの状態遷移、SRV/UAVエイリアス、CPUフォールバックとの画素差を実フレームで比較する。未検証。

### 2026-09-01: RenderQueueのMatteTrack PSOはレンダラー寿命で再利用する
- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` / `gpuMattePipeline_`
- 事実: MatteTrackパイプラインの初期化はシェーダー生成を伴うため、フレーム処理内で毎回生成するとGPU化のコストを増やす。
- 対応: `Impl` メンバーとして保持し、GPUレンダラーの解像度変更・再初期化時だけ破棄するようにした。
- 価値/懸念: 連番レンダー時のフレームごとのPSO初期化を避けられる。実機での初回初期化時間と再利用率は未検証。

### 2026-09-01: RenderQueue GPUマットの対象面はフラッシュ後に解放する
- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` / `GpuMatteFrameResources`
- 事実: マットソースSRVはフレーム後半の複数レイヤーから参照されるため保持が必要だが、対象レイヤー面とMatteTrack出力面は合成後に不要になる。
- 対応: GPUコマンドを `flush()` してから対象面・深度面・出力面を解放し、ソース面だけフレーム終了まで保持するようにした。
- 価値/懸念: 多数のマット付きレイヤーで不要なVRAM常駐を抑えられる。Diligent実機でのコマンド参照寿命とメモリ使用量は未検証。

### 2026-09-01: RenderQueue GPUマットは未確定カメラ状態の3Dを対象外にする
- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` / 単一GPUフレーム経路
- 事実: この経路のソース中間面描画にはViewer側の明示的な3Dカメラ行列引き渡しがなく、3Dソースの投影結果をマット座標として断定できない。
- 対応: 3Dソースおよび3D対象レイヤーはGPU MatteTrack経路から外し、既存経路へフォールバックさせた。
- 価値/懸念: カメラ不整合による誤った切り抜きを防ぐ。RenderQueueへ明示的なカメラ状態を渡せる設計になった時点で再評価する。

### 2026-09-01: RenderQueue GPUマットの中間面は実ターゲット寸法に合わせる
- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` / `Impl::renderSingleFrame`
- 事実: 解像度プリセットやCropにより、コンポジション論理サイズとGPU出力ターゲットサイズは一致しない場合がある。MatteTrackは全入力テクスチャの寸法一致を要求する。
- 対応: マット用ソース・対象・出力の中間面を、コンポジションサイズではなく `gpuRendererWidth_` / `gpuRendererHeight_` に合わせた。
- 価値/懸念: Half/Third/Quarter/Custom出力で寸法不一致によるGPUマット失敗を避けられる。実際のCrop座標・座標変換を含む画素一致は未検証。

### 2026-09-01: 実運用のGPU連番経路を呼び出し関係で確定
- 関連: `Artifact/src/Render/ArtifactRenderQueueService.cppm` / `Impl::renderSingleFrame` と `renderSingleFrameGPU`
- 事実: リポジトリ内検索では `renderSingleFrameGPU()` に定義以外の参照がなく、フレーム処理のGPU分岐は `Impl::renderSingleFrame()` から実行される。
- 対応: GPU MatteTrack導入の評価対象を `renderSingleFrame()` に絞り、未参照の旧GPU実装に残るQImageソース処理を実運用GPU経路の未適用と混同しないよう整理した。
- 価値/懸念: 実際に使われる連番GPU経路の変更範囲を明確化できる。旧APIが将来再利用される場合は別途同じ処理を共有化する必要がある。

### 2026-09-01: Noise layerの数値設定を既存アニメーション経路へ接続
- 関連: `Artifact/src/Layer/ArtifactNoiseLayer.cppm`, `docs/planned/MILESTONE_NOISE_LAYER_2026-08-24.md`
- 確認できた事実: Noiseのプロパティはpersistent propertyとして登録されていたが、生成処理は基底設定値を直接参照しており、キーフレーム補間値を使っていなかった。
- 対応: 既存のPropertyキーフレームとAnimationLayerStackを現在のcomposition frame／frame rateで評価し、scale、offset、rotation、amplitude、octaves、lacunarity、gainをCPU/GPU双方の生成設定へ渡すようにした。
- 価値/懸念: Offset／Rotationをアニメーションさせる流れるノイズの基礎経路を、専用シグナルや別アニメーション実装なしで追加できる。ビルド・runtime parity・キーフレームUIからの実機確認は未検証。
- 次に確認すべきこと: Property editorでのキーフレーム追加、フレーム移動時のキャッシュ更新、CPU/GPU出力一致、seed／kind／color mappingのアニメーション要否を確認する。
