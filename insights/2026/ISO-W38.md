**最終更新:** 2026-10-09

# Insight Register — 2026-W38

期間: 2026-09-14 – 2026-09-20

## 2026-09-20 — Detached Task 実装中に判明した既存構造の前提違い

- **関連:** `Artifact/src/AI/AIClient.cppm`（`tryHandleToolCallResponse` `:241-262`、`runCloudChatWithToolLoop` `:354-420`）、`Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`（承認ダイアログ）、`Artifact/src/Undo/UndoManager.cppm`（`push` `:4799-4810`、`beginActionRecording` `:4822`、`endActionRecording` `:4831`、`cancelActionRecording` `:4845`）、`Artifact/include/AI/WorkspaceAutomation.ixx:3660-3668`、`Artifact/include/AI/AgentApprovalPolicy.ixx`（本作業で新規）、`docs/planned/MILESTONE_DETACHED_TASK_2026-09-20.md`
- **確認できた事実（静的読み取り）:**
  - `AIClient` のツールループは承認を一切通さず `ToolBridge::executeToolCall` を実行する。承認判定を持つのは `ArtifactAICloudWidget` の経路だけ。したがって「承認を尊重する非モーダルな AI ツールループ」は現存しない。
  - `beginActionRecording` / `endActionRecording` は名前に反して Undo をまとめない。記録中も 1 コマンド = 1 履歴で push され、`endActionRecording` は 5〜10 件のシリアライズ済みコマンドを JSON で返すだけ（リプレイ用の観測機構）。5 件未満・11 件以上・非シリアライズ可では空を返す。`cancelActionRecording` は適用済み変更を戻さない。複数コマンドを 1 Undo ステップへまとめる「push 保留」機構は存在しない。
  - `delete_layer` は `WorkspaceAutomation::removeLayerFromCurrentComposition`（`:3660-3668`）を通り、確認 gate を通らない。`SafeWriteRemovalGate` が入っているのは `*Confirmed` 系 6 メソッドのみ。
  - コマンド実行（`WorkspaceAutomation::executeCommand`）は Qt ウィジェット・Undo・各サービスを触るため UI スレッド必須。`Core.Thread.BackgroundTaskWorkerPool` の worker thread からは実行できない。
  - `ToolApprovalMode` と `isReadOnlyToolCall` は `ArtifactAICloudWidget.cppm` の匿名 namespace に閉じており他から再利用できなかった。read-only 判定も二重で、`isReadOnlyToolCall` はツールメソッド名の接頭辞、`CommandIR::isReadOnlyType` は語彙の完全一致。
- **価値または懸念:** 承認が UI ウィジェット側にしかなく、`AIClient` 経由のツール実行は無承認で通る。自動化経路を増やすたびに承認が漏れる構造なので、承認判定は共通モジュールへ寄せ、呼び出し側ではなく実行経路で強制するのが望ましい。`delete_layer` が確認 gate の外にあることも、安全契約の適用が `*Confirmed` 系に偏っていることを示す。
- **次に確認:** `AIClient` 経由のツール実行が実際に無承認で書込みを行うこと（実機で承認設定を `AskEveryTime` にし、`AIChatWidget` から書込み系ツールを呼ぶ）。既存コードに `BackgroundTaskWorkerPool` の worker thread から `WorkspaceAutomation` を呼ぶ箇所が無いこと。



## 2026-09-20 — `<stop_token>` C1116はAPI置換だけでは除去できない

- **関連:** `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`、`ArtifactCore/include/Thread/BackgroundTaskRuntime.ixx`、`ArtifactCore/include/Thread/LightweightTask.ixx`、MSVC 14.51 C++ Modules。
- **確認できた事実:** 製品コード内に `std::stop_token`、`std::stop_source`、`std::stop_callback`、`std::jthread` の直接利用は見つからない。一方、独自の `CancelToken` と `LightweightTaskContext::requestCancel()` が既にあり、キャンセル契約は `std::atomic<bool>` ベースで実装されている。今回のC1116は `Animation.Transform3D` のIFC import中にMSVC標準ライブラリ内部の `<stop_token>` 特殊化で発生しており、`<thread>`／`<future>`／`<memory>` 等から間接的に入る経路である。
- **仮説（未検証）:** キャンセルAPIを独自型へ統一すること自体は可能だが、それだけではMSVC標準ヘッダーの間接依存を消せず、今回のIFCエラーは解消しない。解消には、IFCへ取り込まれる標準ヘッダー面の縮小、header-based STLとnamed std BMIの混在防止、または該当implementation unitの非module化／分離が必要になる可能性が高い。
- **価値または懸念:** 独自キャンセル契約の統一は設計上有益だが、C1116回避と混同すると広範な置換を行ってもビルド障害が残る。標準ライブラリ完全置換はthread、future、condition_variable、memoryまで波及し、費用対効果が悪い。
- **次に確認すること:** 許可を得て該当IFCのみ再生成し、再現する場合は `/showIncludes` とproducer／consumerのcompile optionsを比較する。その後、`Artifact.Layer.Abstract` implementation群のGMF標準ヘッダーを1つずつ最小化し、C1116を起こす具体的なinclude境界を特定する。



## 2026-09-19 — VP操作時にキー時刻スケールが実ストレージとずれるとキー付きフレームへ書けない

- **関連:** `Artifact/src/Widgets/Render/TransformGizmo.cppm`（`captureTransformSnapshot`、`transformKeyframeTimeAtFrame`、`beginHandleDrag`、`handleMouseRelease`）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`gizmoTransformTime`）、`Artifact/src/Widgets/Render/ArtifactCompositionGizmoUndoCommands.cppm`（`transformTime`）、`ArtifactCore/src/Animation/AnimatableTransform3D.cppm`（`setRotation` は `offset_=initialRotation_` を加算）、`Artifact/src/Layer/ArtifactAbstractLayer.cppm:2046`（`setComposition` が `setKeyframeTimeScale`）。
- **確認できた事実（静的読み取り）:** キーは `AnimatableTransform3D::timeScale_`（層が composition へ参加する時点の `FrameRate::storageScaleForFps(fps,24)`）へ保存される。一方 VP 側のドラッグ・Undo・キャンセルはコンポジション fps から `storageScaleForFps` を再計算していた。両者が一致しない場合（例: 29.97fps で旧タイムラインが作った 29 scale のキー、fps 変更前に作られたキー、保存データの移行前キー）、`captureGizmoPropertyKeys`/`has*KeyFrameAt` の厳密比較が既存キーを見つけられず、`setInitialPosition` 系の分岐へ落ちて「そのフレームのキーが更新されない」状態になる。`setKeyframeTimeScale` は spatial tangent は再スケールするが既存キー時刻は書き換えないため、fps 変更後のキーは自動移行されない。
- **対応:** レイヤー自身がキー時刻ドメインの唯一の定義を持つようにした（`ArtifactAbstractLayer::keyframeTimeScale()` / `keyframeTimeAtFrame(frame)` / `currentKeyframeTime()`、`Artifact/include/Layer/ArtifactAbstractLayer.ixx` に宣言）。VP 側の時刻生成を、コンポジション fps からの再計算ではなくこの API へ統一した: `TransformGizmo`（`transformKeyframeTimeAtFrame`、`multiDragState_->timeScale`、`captureTransformSnapshot`、未使用の `effectiveTransformKeyframeRate` と `import Frame.Rate` を削除）、`ArtifactCompositionRenderController`（`gizmoTransformTime`）、`ArtifactCompositionGizmoUndoCommands` / `ArtifactCompositionEditUndoCommands` / `ArtifactCompositionLayerUndoCommands`（各 `transformTime`。Edit/Layer の2件は double fps をそのまま `RationalTime` へ渡す**切り捨て**バグも同時に解消）。`TransformSnapshot` も同ドメインの frame/timeScale を保持するため Undo・Redo・キャンセルが同じ時刻を指す。加えて `TransformGizmo::handleMouseRelease` で最終状態の `changed()` と `LayerChangedEvent` を1回発行する（ドラッグ中の通知は 33ms スロットルのため、最後の書き込みがタイムライン／Inspector に届かない経路があった）。回帰テスト `Artifact/src/Test/ArtifactTestPropertyKeyframe.cppm` に 29.97fps のキー付きフレーム更新、レイヤー API 一致、`setFrameRate` によるドメイン再ピン留め（キー時刻は移動しない）のケースを追加。`ArtifactTextGizmo` の `textAuthoringTimeScale` はテキストプロパティ（Transform3D チャンネル外）の時刻でありコンポ fps が正のため現状維持。
- **未検証:** ビルド・実機確認は未実施（ユーザー指示待ち）。ユーザー報告の再現条件（コンポ fps、fps 変更の有無、Auto-Key 状態、キーの作成元が左ペイン／タイムライン／過去の VP ドラッグのどれか）は未確認。2026-09-17 の項目で挙げた「シーク未反映時に編集が直前フレームへ書く」可能性（`gizmoUndoFrame_` は `comp->framePosition()` を読む）は今回の変更では扱っていない。
- **次に確認:** 30fps と 29.97fps の両方で「既存キーありフレームでの VP ドラッグ」→ キー値更新、キー数不変、Undo/Redo 一致を比較する。再現が続く場合は `gizmoUndoFrame_` と UI プレイヘッドの一致（seek 完了契約）を次の候補として調べる。残る重複: タイムライン左ペイン（`ArtifactLayerPanelWidget`）と `ArtifactTimelineTrackPainterView` は Transform3D チャンネル以外（opacity 等）の時刻も同じ算出で作っており、これらは Transform3D ドメインとは無関係なため今回の対象外。



## 2026-09-19 — i18n hardening 進捗: 翻訳キー移行と共通ラッパーの二重化

- **関連:** `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`（完了）、
  `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`・`ArtifactRenderMenu.cppm`（残6＋12を `TranslationManager::instance().tr(key, fallback)` 化）、
  `ArtifactCore/src/Localization/Localization.cppm`（連鎖フォールバック + 複数形 helper）、
  `tools/i18n/migrate_remaining_menus.py`（自動移行スクリプト）。
- **確認できた事実:** 4メニューファイルのうち `ArtifactFileMenu.cppm`・`ArtifactViewMenu.cppm` は翻訳可能なハードコード日本語0行。`ArtifactLayerMenu.cppm` 残4行は開発者コメント（翻訳対象外）、`ArtifactRenderMenu.cppm` 残2行もコメント。監査は `Keys used 1121 / Expected 1344, Coverage 100%` を維持。`--min-coverage 95` は `.github/workflows/i18n-check.yml` で既に設定済み。
- **懸念／仮説（未検証）:** 各メニューファイルで `static QString tt()`/`static QString menuText()` が独自に再定義されているが、名前は重複している。`tt()` は `TranslationManager::tr(key, fallback)` の薄いラッパー（フォールバックを必須にするだけ）で、`menuText()` は `tt()` に同じ。これらを `Core.Localization` の `inline` ヘルパーへ集約すれば、ハードコード移行の指示ミス（ラッパー定義忘れ）をコンパイルエラーで検知できる。ただし集約は呼び出しを大幅に書き換えるため、ビルド検証後に別フェーズで実施。
- **次に確認:** ビルド許可後、`tr(key, fallback)` の fallback を含む新規キー18が en/ja JSON に正しく載り、`added_to_queue` の `%1 ... .arg(added)` チェーンが維持されていることをコンパイル＋監査で確みめる。


## 2026-09-19 — 2Dフレームギズモの拡縮ゴースト基盤

- **関連:** `Artifact/src/Widgets/Render/TransformGizmo.cppm`、Composition Viewport の2D拡縮オーバーレイ。
- **確認済み事実:** 2D TransformGizmo はドラッグ開始時の global transform／local bounds／canvas bounding box を既に保持し、拡縮中の破線ゴースト、サイズバッジ、スマートガイド、Undo境界まで同じ所有者で扱っていた。新しいオーバーレイサービスやイベント配線は不要だった。
- **対応:** 元枠ゴーストを固定低透明度へ変更し、バッジを操作ハンドル外側へ配置。現在サイズ、X/Y倍率、幅・高さ差分、中央基準時の Anchor 表示を既存の `ArtifactIRenderer` 描画内へ統合した。
- **価値／懸念:** GPU資源・同期・レンダリング本流を変えずに操作フィードバックを強化できる。一方、表示値は既存 `targetBox` のcanvas-space寸法であり、回転レイヤーや特殊なsource-size編集でユーザーが期待する「素材ピクセル寸法」と一致するかは実機確認が必要。ドラッグ中の `QString` 更新は既存方式を踏襲しており、アロケーション実測は未実施。
- **次に確認:** 回転済み平面、辺／角／中央ハンドル、複数選択、Shiftによる初期サイズ編集、ズーム端、画面端でのHUDクランプを実機で確認する。



## 2026-09-19 — i18n: 言語切替のオンデマンド再翻訳はメニューの aboutToShow で既に成立していた

- **関連:** `ArtifactCore/src/Localization/Localization.cppm`（`setLanguage` → `LocaleChangedEvent` 発火、`fallbackChainFor`、`pluralCategoryFor`）、`ArtifactCore/include/Utils/Localization.ixx`（`LocaleChangedEvent` / `TranslationsReloadedEvent`）、`Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`（`GeneralSettingPage::saveSettings`）、`Artifact/src/Widgets/Menu/*.cppm`（全メニューが `QMenu::aboutToShow` で `rebuildMenu()`）。
- **確認できた事実:** `ArtifactFileMenu` を除く全メニュー（View/Edit/Layer/Render/Time/Option/Script/Composition 等）が `aboutToShow` で `rebuildMenu()` を呼ぶ。したがって「設定 OK でアクティブ言語を切り替える」だけで、次にメニューを開いた時点で新言語のラベルになる。購読者を各メニューへ新規配線する必要はなく、`LocaleChangedEvent` は将来の即時再翻訳（常時表示UI・ステータスバー等）用の通知として用意した。
- **対応:** `ArtifactAppSettings` に `General/LanguageCode` を追加し、環境設定の Language セレクタで保存。`saveSettings` で保存＋`LocalizationManager::setLanguageCode()` 即時適用。起動時は `--lang` > 保存設定 > システムロケール > `en` の順で確定し、`[AppMain] Language decided: <code> by <reason>` を1行出力。
- **価値／懸念:** 言語切替の即時反映を新しいシグナル／スロット配線なしで実現できる（既存の `aboutToShow` 再構築に乗る）。一方、メニューバーのトップレベルやステータスバーなど「常時表示で再構築されない」UIは次回起動まで旧言語が残り得る。また `LocalizationManager` に `QReadWriteLock` と `Event.Bus` 依存が入ったため、**コンパイル未検証**（AGENTS.md のビルド禁止による）。ロックは `translate()` の読取、`loadFromDirectory`/`reload`/`addTranslation`/`clearTranslations` の書込に限定し、`loadFromFile` は呼出側がロック済み前提（再入デッドロック回避）。
- **次に確認:** ビルド許可後、`QReadWriteLock` の再入（`translatePlural` → `translate` / `pluralCategory` の読取ロック重複）が Qt の仕様どおり安全か、`reload()` 中の `availableLocales()` 呼出がデッドロックしないかを確認する。常時表示UIの再翻訳は `retranslateUi` 相当の所有者責務を決めてから着手する。



## 2026-09-19 — i18n: 同一性比較に使う表示文字列は「同じキー」で両側を翻訳しないと壊れる

- **関連:** `Artifact/src/Widgets/Dialog/ArtifactImportAssetsDialog.cppm:72`（`group.title == QStringLiteral("連番")` の比較）と `:689`（`ImportGroup sequences{QStringLiteral("連番")}` の構築）。
- **確認できた事実:** `ImportGroup.title` は UI 表示ラベルでありながら、ツールチップ判定で `== "連番"` の同一性比較にも使われていた。片側だけを翻訳するとロケールによって一致しなくなる。両側を同じキー `import.group.sequence` で `tr()` 化したため、どのロケールでも比較が成立する。
- **価値または懸念:** i18n 移行時の典型的な罠。表示文字列を識別子として流用している箇所（グループ名・種別名の `==` 比較、`switch` の対象、保存値との照合）は、単に `tr()` で包むと壊れる。移行前に「この文字列は表示専用か、識別子も兼ねているか」を必ず確認し、識別子兼用なら同じキーで両側を揃えるか、enum/ID へ置き換えるべき。
- **次に確認（実施済み 2026-09-19）:** 横断検索の結果、`==` による日本語識別子比較は次の3ファイルに限られる。
  - `Artifact/src/Widgets/ArtifactMainWindow.cppm:1562-1696`（`toolName == "ブラシ" / "消しゴム" / "コピースタンプ" / "モーションスケッチ" / "テキスト"`）
  - `Artifact/src/Widgets/ArtifactToolOptionsBar.cppm:1136-1507`（`toolName == "選択" / "移動" / "回転" / "スケール" / "アンカー" / "ペン" / "シェイプ" / "楕円" / "テキスト" / "モーションスケッチ" / "ブラシ" / "コピースタンプ" / "消しゴム"`、`primaryLabel == "点数" / "辺数"`）
  - `Artifact/src/Widgets/ArtifactLooksPresetBrowser.cppm:895`（`activeLibrary_ == "お気に入り"`）
  `startsWith` / `contains` / `endsWith` / `compare` に日本語リテラルを渡す箇所は0件。
  **結論:** これら3ファイルはツール名／ライブラリ名を識別子として流用しているため、P0-2 の単純な `tr()` 置換の対象にしてはならない。移行するなら「表示名は翻訳しつつ、識別子は enum/ID へ分離する」設計変更が必要。当面は除外リストとして扱う。
  残りの P0-2 対象（`ArtifactAnimationMenu` / `ArtifactCompositionMenu` / `CreatePlaneLayerDialog` / `CreateCameraLayerDialog` / `PrecomposeDialog` / `ColorSwatchDialog` / `QuickLayerCreationDialog`）にはこの比較が無いため、通常の移行で問題ない。



## 2026-09-17 — Core キーフレーム監査: 保存時刻と評価時刻の契約を分離しない

- **関連:** `J:/dev/ArtifactStudio/ArtifactCore/src/Property/AbstractProperty.cppm:650-699,736-777,827-932`、`J:/dev/ArtifactStudio/ArtifactCore/include/Geometry/Interpolate.ixx:670-734`、`J:/dev/ArtifactStudio/ArtifactCore/include/Animation/AnimatableValue.ixx:127-154,198-235`、`J:/dev/ArtifactStudio/ArtifactCore/include/Property/PropertySerializationBridge.ixx:218-260`。
- **確認できた事実（静的監査）:** Property の値のみ addKeyFrame は既存キーの補間・ハンドル・roving を既定値へ戻す一方、AnimatableValueT は保持する。Property の Constant は中央キー時刻でも直前キー値を返すが、テンプレート側の Constant は alpha=1 で次キー値を返す。retime は Absolute にも newInPoint.scale() への丸めを行い、衝突を sort/unique で削除する。数値評価は毎回全キーを補間器へコピーして各挿入で再ソートし、evaluateValue も式なしで全キーをコピーする。既存 Property へ deserializeProperty すると、空の expression は適用されず旧式が残る。
- **懸念／設計仮説（未検証）:** RationalTime の比較・保存が厳密でも、評価用 double 時刻への変換で近接キーが同一時刻へ潰れる。時刻の一意性をストレージだけで保証しても不十分。評価区間の探索は RationalTime のまま行い、選んだ区間内の差だけを数値化する設計を検討する。time.scale() を式の FPS として使う現在の経路も、同一時刻を別 scale で表した際の評価一致と衝突するため、FPS を時刻分母から分離する契約確認が必要。
- **価値:** 「保存往復に成功」「個別 getter にロックあり」だけで production-ready と判断せず、キー時刻での値一致、編集メタデータ保持、非破壊 retime、評価スナップショットの一貫性を受入条件にできる。
- **2026-09-17 修正:** 値のみ更新は既存メタデータを保持し、Property と共通補間器は正確なキー時刻でそのキー値を返すよう変更。Absolute はリタイム対象から除外。リタイムは編集時だけ一時コピー上で処理し、衝突・範囲外は Property 全体の変更を拒否して lastError に記録する。既存の標準 vector ストレージをコピーするため追加の標準コンテナへの置換はない。フレーム評価への追加確保はない。既存 PropertyKeyframe テストへ値更新・全型 Hold 境界・Absolute・衝突・アンカーの回帰を追加。ビルド・テストは未実行。
- **2026-09-17 検証（実行済み）:** `out/build/x64-Debug`（Ninja/MSVC 14.51）で `ArtifactCore` を差分ビルド → 成功（`Interpolate.ixx` / `AbstractProperty.ixx|.cppm` 再コンパイル、警告は既存 C5202 のみ、エラーなし、`ArtifactCore.lib` 再リンク）。変更後の ifc/ライブラリを実際に import/link する検証 exe（`temp/verify_keyframe_behavior.cpp`）をビルドし実行 → 14/14 PASS（値更新のメタデータ保持、NaN 拒否、Hold の 9/10/11/20/25 フレーム境界、Color/Point2D、Absolute の 1/48・1/24・巨大整数保持、衝突時の全体拒否、既存の置換・削除・整列・線形補間）。リポジトリ追加分の回帰テストは実ビルドの単一モジュールターゲットでコンパイル確認済み（`ArtifactTestPropertyKeyframe.cppm.obj` 06:03 更新、エラーなし）。検証用スクリプトは `temp/run_verify.bat`、`temp/run_verify.ps1`、`temp/build_test_module.bat`。
- **未検証:** `Artifact.TestRunner` の全テスト実行（アプリ exe のビルドが必要なため未実施）。修正前コードでの失敗再現（意図的なロールバックビルド）は未実施で、旧挙動は差分からの読み取りに基づく。空式の復元、並行編集・評価、アロケーション計測は未着手。
- **残る境界（未検証）:** レイヤーの setInPoint/setOutPoint はリタイム前に尺を確定し、Property の lastError を確認しない。今回の拒否は Property 単位であり、レイヤー全体の尺変更はロールバックしない。複数 Property と尺変更を一括で確定／拒否する設計は別途判断が必要。
- **次に確認:** 追加した回帰テストの実行、空式の既存オブジェクト復元、並行編集・評価とアロケーションの実測。既存テストの呼び出しは Test.cppm の runAllTests 経由で、PropertyKeyframe 専用の実行フィルタは確認できていない。




## 2026-09-17 — キー編集時刻統一後に残る独立した確認事項

- **関連:** `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`（`cancelInteraction` / `pushTransformUndoIfNeeded`）、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`（`applyTimelineSeek`）、`Artifact/src/Service/ArtifactPlaybackService.cppm`（`publishFrame`）。
- **確認できた事実:** Text の anchor 操作は静的チャンネルにもキーを作るが、キャンセル／Undo 分岐は操作開始時の `before.animated` を使用する。タイムラインの表示フレーム更新は停止時の queued な composition 同期に先行する。主コントローラーの単発リセット等には今回のドラッグ修正とは別に `layer->currentFrame()` を使用する箇所が残る。
- **懸念（未検証）:** 静的 anchor のキャンセルでキーが残る可能性と、シーク未反映時の diamond toggle が直前フレームのキーを削除する可能性がある。今回報告されたアニメーション不成立との一致は未確認。既存の誤った時刻のキーは自動移行していない。
- **次に確認:** 別途、静的 anchor の Undo／キャンセル契約、編集直前の seek 完了契約、単発リセットの時刻を個別に検証する。今回のドラッグ時刻・FPS 統一とは分離し、追加配線やシーク方式変更は行わない。



## 2026-09-17 — A6 の Text offline raster cache は既存 TextLayer が所有済み

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:2268`、`Artifact/src/Layer/ArtifactTextLayer.cppm:2725`。
- **確認できた事実:** offline/Render Queue 側は `textLayer->toQImage()` を呼ぶが、`ArtifactTextLayer` は `isDirty_` または未生成の `renderedImage_` のときだけ `updateImage()` を実行し、それ以外では保持済み `renderedImage_` を返す。通常の非編集フレームで Text raster を再実行する経路ではない。
- **価値または懸念:** A6 を別の Composition View キャッシュとして重ねると、既存の TextLayer dirty invalidation と二重化し、古い画像を返すリスクがある。最適化対象は `toQImage()` 自体ではなく、Text の dirty 化頻度または後段の surface/effect 処理を計測してから決めるべきである。
- **次に確認:** 実機プロファイルで `ArtifactTextLayer::updateImage()` が非編集フレームに現れる場合だけ、dirty 要因とアニメータ時刻評価を分けて調査する。



## 2026-09-17 — A3 と A10 の一部は既に実装済み、残る有効化は runtime parity が条件

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:37546`（Composition-space GPU cache）、`:38486`（timeline transition 評価）、`:42920`（選択レイヤー overlay）。
- **確認できた事実:** composition-space GPU cache は `Render/Experimental/CompositionSpaceGpuCache` の opt-in で、キーに pan/zoom を含めず、hit 時には保持済み composition texture を現在の表示変換で描画する。A3 の camera-only reuse 自体はこの限定構成（2D solid/still、Normal、非3D、mask/effectなし）で成立している。timeline transition progress は層ループ外で一度だけ評価されているため、A10 の同呼び出しを層数比例で削減する余地は現状ない。
- **価値または懸念:** A3 の既定有効化は Diligent backend ごとの offscreen presentation / color-transform parity を runtime で確認してから行う必要がある。選択 overlay では `selectedLayersInOrder()` が `QVector` の値コピーを返すが、reference API へ変えると選択変更時の寿命・スレッド境界契約を再検討する必要がある。
- **次に確認:** D3D12 と Vulkan で pan/zoom 中および停止後の full-resolution presentation を比較する。overlay copy は selection manager の immutable snapshot 契約を明文化できる場合のみ reference/view API を検討する。



## 2026-09-17 — A4/A7/A10 残部は正本契約の整理が先行する

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:2410`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:36813`、`:40106`。
- **確認できた事実:** controller の調整レイヤー主経路には、pointwise GPU effect と interaction/draft 時の CPU readback 短絡が実装済みである。一方 exported drawing fallback の `readbackToImage()` は CPU rasterizer effect 正本へ渡す唯一の完全品質入口だった。カメラの前フレーム行列は、親子 transform と shake を含めるため composition 全体を N-1/N と往復評価している。mask overlay の選択頂点は多くの入力 controller が共有する可変 vector であり、描画時だけの hash 化は毎フレーム確保を生む。
- **価値または懸念:** 既存の GPU spatial/raster effect 完全互換パス、時刻指定の親子カメラ評価 API、または selection の immutable revisioned snapshot なしに置換すると、settled 出力、motion vector、選択操作のいずれかを壊す可能性がある。
- **次に確認:** 各 CPU rasterizer effect の GPU 対応表と adjustment mask の semantics、parent transform を含む camera-at-time API、selection snapshot の所有／寿命を先に設計レビューする。runtime parity なしにこの3項目を有効化しない。



## 2026-09-17 — A5 の GPU matte output は frame 内共有済みで、跨 frame cache には time identity が不足する

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:38232`、`:10097`。
- **確認できた事実:** `matteGpuOutputs_` は matte source ID ごとの offscreen texture を保持し、同一 frame・同一 size・同一 `surfaceGeneration` の source を複数の target layer が参照する場合に再レンダリングを避ける。CPU fallback は GPU intermediate を使えない matte に限定される。
- **価値または懸念:** `surfaceGeneration` は layer mutation を追跡するが、keyframe transform、親 transform、時刻依存 source/effect の各評価を単独では表さない。この条件から `frame == frame` を外すと、静的に見える matte が別フレームで古い位置・内容になるおそれがある。
- **次に確認:** source layer の全時間依存性（transform / parent / effect / mask / source mapping）を表す revisioned render identity を整備し、その identity が不変な matte だけ frame をまたいで再利用する。



## 2026-09-17 — 3D AOV は target reset で queue submit 済みのため per-AOV Flush を不要化できる

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:11568` 以降、`Artifact/src/Render/ArtifactIRenderer.cppm:2865`（render target override）、`:3125`（`flush()`）。
- **確認できた事実:** `setOverrideRTV` / `setOverrideDSV` は override 値を変更する前に `submitQueuedDraws()` を呼ぶ。3D mesh の AOV mode / ID 値は `ArtifactIRenderer::Impl::drawMesh()` の呼び出し時に primitive renderer へ反映される。したがって AOV の `layer->draw()` 後に target reset する順序は維持したまま、各 AOV の `IDeviceContext::Flush()` を省ける。
- **価値または懸念:** emission / normal / velocity / object ID / material ID / albedo の per-layer 強制 submission がなくなる。beauty path の MSAA resolve 前 flush は queued draw の提出順序を担うため残す。backend 共有経路なので D3D12/Vulkan の multi-channel output を runtime で確認する必要がある。
- **次に確認:** 3D layer で beauty + 全 AOV を出力し、各 channel の内容と `flushCount` を修正前後で比較する。



## 2026-09-16 — VP描画は2実装が同名で併存し、scene light lift は offline 側にしかない

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:8306`（モジュール内ローカル `drawLayerForCompositionView`、21引数）、
  `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:1460`（export 版、11引数）、
  `Artifact/src/Render/ArtifactRenderQueueService.cppm:3882/6019`（export 版の呼び元）。
- **確認できた事実:** 対話VPの `renderOneFrameImpl`（同 cppm:39599 直接パス / 11484 GPUパス）は引数個数からモジュール内ローカル実装を呼び、
  Render Queue / offline は `import Artifact.Render.CompositionViewDrawing` の export 版を呼ぶ。両者は同名・別実装で、キャッシュ・マット・マスク処理を二重に持つ。
  非3Dレイヤーへの決定的な明度リフト（`min(0.18, 0.03 * lightCount)`）は export 版の `applySurfaceAndDraw` 内にのみ存在し、
  ローカル実装は 3D（`setSceneLights`）以外にライトを適用しない。したがって lit な 2D レイヤーは VP と レンダー出力で見え方が異なる。
- **価値または懸念:** 「VPの scene light CPU ループ」という指摘は実は offline（Render Queue）側の話であり、VP のフレーム時間には効かない。
  逆に 2D ライトのパリティ差は未整理の仕様差であり、どちらが正なのかは未確定。実装を片方だけ直すと差が広がる。
- **次に確認:** VP と offline の 2D ライト適用を統一するか、VP を正として offline のリフトを撤去するかをユーザー判断で決める。
  統合する場合はローカル実装の削除（マスク・マット・GPUラスタ効果の順序parity確認が前提）が本筋の整理。



## 2026-09-16 — Normal-only コンポジションの GPU ブレンド経路（帯域見積りと opt-in 化）

- **関連:** `ArtifactCompositionRenderController.cppm`（`hasGpuBlendJustification`、`gpuBlendPathRequested`、新規 `gpuBlendForNormalCompositionEnabled()`）。
- **確認できた事実:** 既定の直接パスは、レイヤー内容がキャッシュに乗っていれば sprite draw のみ。GPUパスはレイヤー毎に
  全画面 `convertLayerToFloat` + 全画面 `blendLayers` + ping-pong を追加する。1080p・10層の概算で、GPUパスは
  約 20 回の全画面 32F read/write（≒0.6GB/frame、RTX4070Ti で約2ms超）に対し、直接パスは 10 枚の quad（≒0.17GB）で、単純な Normal 構成では逆効果になり得る。
- **仮説（未検証）:** 効果（特に spatial）・マット・マスクを持つ層、または毎フレーム内容が変わる層（アニメーション効果、video、particle）だけを
  GPUパスへ寄せれば bandwidth は正当化できる。逆に静的効果層は CPU 側キャッシュ（signature 一致）で既にスキップされているため対象外が妥当。
- **対応:** 既定挙動を変えず、`ARTIFACT_COMPOSITION_GPU_BLEND_NORMAL=1` で Normal-only も GPU パスへ入る opt-in を追加（計測用）。
- **次に確認:** 同条件（1/10/30層、Normal と Mixed、効果あり/なし）で `[CompositionView][Perf]` の frameMs/layerPassMs を A/B し、
  既定を反転してよい構成条件（層種別・効果種別）を確定する。反転する場合は 8bit sRGB 合成→float linear 合成の画質差も同時に確認する。



## 2026-09-16 — 確定できていないVPホットパス候補（未着手・要計測）

- **関連:** `ArtifactCompositionRenderController.cppm:38688/38830/38865`（層ごとの `FunctionalRenderPass` 3個）、
  `:10563`（`RenderGraph::execute` が層ごとに `std::function` を構築）、`ArtifactCore/include/Graphics/RenderGraph.ixx:108`、
  `ArtifactCompositionRenderController.cppm:36065`（30フレームごとの診断）。
- **仮説（未検証）:** 層×フレームで `std::function`（キャプチャ4参照＝SSO超過）と RenderGraph 実行記録が確保される可能性がある。
  また `captureRenderDiagnostics` が30フレーム毎に RenderCostCaptureGuard / TraceGuard / frame RenderGraph 構築+compile を走らせるため、
  周期的な stutter になり得る。`flushMs` は2026-08-15のInsight以降、累積差分として算出されるようになっており意味が変わっている。
- **価値または懸念:** どちらも描画結果を変えずに検証・修正できる低リスク候補。ただし確保量は未計測で、推測で消すと
  RenderGraph の検証経路を失う可能性がある。
- **次に確認:** allocation トレースで層数比例の確保を確認してから、`runAllWithRenderGraph` の素通し化（`run` 直呼び）と
  診断の延実行を検討する。


## 2026-09-16 — i18n P0-1：監査の実効化とtimeline tooltip 137キーの翻訳リンク

- **関連:** `tools/i18n/audit_translations.py`、`.github/workflows/i18n-check.yml`、
  `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`、
  `Artifact/src/Widgets/Menu/ArtifactScriptMenu.cppm`、`Artifact/translations/{en,ja}.json`。
- **確認できた事実:** 監査の `KEY_PATTERNS` が `tt()` を抽出できず `Keys used: 3` だった。
  CIの99.6%は虚偽で、実際には `timeline.*` 133キーがJSON未登録（JAでも英語表示）だった。
  文面そのままキー2件は `QObject::tr`（Qt側、JSON系と無関係）で常時英語だった。
  同一キーに複数フォールバック（メニュー`...`付き vs ダイアログタイトル素形）が7件あった。
- **対応:** 監査に `tt(` 抽出＋命名規約lint（`^[a-z][a-z0-9_]*(\.[a-z0-9_]+)+$`、`--max-invalid-keys` 既定0）＋
  意図的同一値の `--allow-same` を追加。文キーは正式キーへ移行（`script.*` 2件、
  `layer_panel.keyframe_value_hint`／`matte_list_hint`）。衝突5キーはメニュー形を基本キー、
  ダイアログ形を `*_title` へ分離（`rename` のみ既存 `layer_panel.rename_layer_title` 再利用）。
  ENはコードのフォールバックから採取、`timeline.*` 137件にJA訳を付けて両JSONへ追加。
- **価値または懸念:** 監査は `Keys used: 410`・coverage 100%・untranslated 4（既存技術表記のみ）で真緑になった。
  JAロケールでタイムライン tooltip が日本語表示になる。`linear`／`linear_word` のように
  大文字小文字だけが違う文脈依存キーが今後も増える余地あり（命名規約では検出不可）。
- **次に確認:** ビルド許可後に tooltip の日英表示、`--lang en/ja` 両方での監査通過。
  **注意：本ツリーには別作業の未コミット変更が混在**（`ArtifactCompositionViewDrawing.cppm`、
  `ArtifactCompositionRenderController.cppm`）。コミット時は本件分離のこと。



## 2026-09-16 — タイムライン左メニューの翻訳リンクと潜在バグ3件

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`（レイヤーパネル右クリックメニュー）、`Artifact/src/Widgets/CommonStyle.cppm`（`sizeFromContents`／`drawControl` の CT_MenuItem／CE_MenuItem）、`Artifact/translations/{en,ja}.json`（`layer_panel`）。
- **確認できた事実:** ① メニュートップが英語の `Frequent`／`All` の開発者用語2階層で、最大4階層ネスト＋「整理」の同名重複があった。② `CommonStyle` の CT_MenuItem 幅計算（68/58px予約）が CE_MenuItem 実描画（84/72px使用）よりサブメニュー項目で約16px・通常項目で約14px不足し、長い項目混在で文字が切れていた。③ `kColNames` が5要素なのに `kLayerPropertyColumnCount`（6）でループし `kColNames[5]` が範囲外参照だった（6列目は Pick Whip／親リンク列）。④ 同ブロックとラベル色ブロックが `QString::fromLatin1` で日本語を渡しており非Latin文字化けの状態だった。
- **対応:** Frequent／All を廃止して全項目をトップレベルへ＋区切り線3本＋重複整理の改名。CT_MenuItem の左右予約幅を描画と一致させた。`tt("layer_panel.*", "English")` へ約160箇所を変換し en／ja に計153＋14＋6キーを追加（既存12キーは再利用）。6列目 `Parent Link`／`親リンク` を追加し tt 化で fromLatin1 と範囲外参照を同時解消。日本語表示は従来文言と同一。
- **価値または懸念:** ロケール切替で英日メニューが切り替わる（起動時 `loadFromDirectory`＋CMake が translations を出力へ複写）。QInputDialog の種別名（Crossfade 等）は保存値と往復するため翻訳対象外に据え置き。Undo ラベルも英語のまま。
- **次に確認:** ビルド許可後に右クリックメニューの表示・幅・英日切替、`missingKeys()` が空であること。`Matte`／`<missing>` の他箇所（2010行付近・7874行付近）およびダイアログ文言は未リンクの残件。



## 2026-09-16 — Animation Layer Inspector評価とMSVC Modules ICE

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`（`getLayerPropertyGroups()`）、`ArtifactCore/include/Animation/AnimatableValue.ixx`、`ArtifactCore/include/Geometry/Interpolate.ixx`。
- **確認できた事実:** Property Group生成中の`AnimationLayerStackT<float>`値表示が`AnimatableValueT<float>::at()`を通じて汎用`interpolate<float>()`を実体化し、MSVC 14.51のIFC import環境でC1001を起こした。通常のInspector表示は評価値ではなく永続値で十分に編集可能である。
- **対応:** InspectorのAnimation Layer値を`current()`による永続値表示へ切り替え、補間評価はレンダー／専用アニメーション経路に限定した。
- **価値または懸念:** 巨大な`ArtifactAbstractLayer`を即時にABI分割せずに、該当テンプレート実体化をコンパイル単位から除去できる。フレーム評価値をProperty Inspectorへ再導入する場合は、専用状態モジュールまたは非テンプレート評価APIが必要。
- **確認:** `cmake --build out/build/x64-Debug --target Artifact --config Debug --parallel 4` が成功した。`Impl`内のAnimation Layer状態ラッパー化、およびUtilitiesモジュールへ新しい公開Animation Layer型を出す二案は、MSVC 14.51のIFC import環境でC1001を再発させたため採用していない。
- **次に確認:** 実機でAnimation Layerの値編集・保存・復元・レンダー評価が維持されること。物理分割を再開する場合は、`Impl`の共有内部表現を先に設計し、公開テンプレート型を新規IFC境界へ出さないこと。



## 2026-09-16 — Diligent版タイムライン上部バー操作負荷の最適化

- **関連:** ArtifactTimelineWidget.cppm（syncPlayheadOverlay、syncGpuTimelineSnapshot、uildGpuTimelineSnapshot、TimelineScrubFinishedEvent）。
- **確認できた事実:** ① syncPlayheadOverlay() において gpuTimelinePreviewEnabled_ を判定していなかったため、GPU描画モード中にもかかわらずシーク毎に Qt 側の TimelinePlayheadOverlayWidget が再有効化・二重描画されていた。② スクラブ・ナビゲータードラッグ操作による連続マウス移動ごとに syncGpuTimelineSnapshot() がキュー投入され、GUIスレッドでスナップショット生成が過剰実行されていた。③ uildGpuTimelineSnapshot() 内の staticHit 判定で !view->isInteracting() を要求していたため、上部バーのシーク・スクラブ操作時にも全トラック行・全グリッド・全クリップ・UniString 含む静的ジオメトリが全件再生成されていた。
- **対応:** ① syncPlayheadOverlay() で gpuTimelinePreviewEnabled_ を加味し、GPUモード時は Qt 側オーバーレイを無効化。② syncGpuTimelineSnapshot() に最小16ms（約60FPS相当）のタイマースロットリングを導入し、TimelineScrubFinishedEvent で最終フレームを確実に同期。③ uildGpuTimelineSnapshot() の staticHit 条件から !view->isInteracting() を外して静的キャッシュのヒット率を高め、プレイヘッド移動のみの再構築負荷を最小化。
- **価値または懸念:** スクラブ・シーク中の不要なオーバーレイ描画と大量のスナップショット再構築が消え、上部バー操作の追従性・フレームレートが大幅に改善される。
- **次に確認:** ビルド許可後、Diligentプレビュー時のスクラブバー・ナビゲーター操作時の軽快感、シーク終了時のプレイヘッド正確性、通常QWidgetモードへの切り替え時の動作整合性を確認すること。



## 2026-09-16 — スクラブ中VP draft＋timeline Present(0)

- **関連:** `ArtifactCompositionRenderController.cppm`（`TimelineSeekRequestedEvent` 購読→`notifyViewportInteractionActivity`）、`ArtifactDiligentTimelineRenderWindow.cppm`（`Present(0)`）。
- **確認できた事実:** VP 本流は controller 経路で、既存の interacting/draft 機構（effect 解像度0.25・CPU readback 回避・LOD）が viewport 操作時だけ有効だった。timeline スクラブは seek を publish するが interacting に入らないためフル描画だった。また Diligent の `Present()` 既定は vsync 有効で、timeline の Present が GUI スレッドを止めていた。VP worker 経路（`CompositionRenderWidget`）は `AppMain` で生成されておらず、スクラブ時の描画本流は controller 側。
- **対応:** 全スクラブ身振り（scrub bar／overlay／track view）が publish する seek を interacting へ流し、120ms 無 seek で自動復帰（既存 tick 処理を再利用、bracket 不要）。timeline 面は `Present(0)` で GUI ブロックを除去（tearing 許容、VP は vsync 維持）。
- **価値または懸念:** スクラブ中の VP 1描画あたりが軽くなり、timeline 操作の応答が上がる。tearing が目立つ場合は要調整。ビルド・実機 runtime は未実施（AGENTS.md 制約でユーザー許可待ち）。
- **次に確認:** スクラブ中の描画負荷・tearing の見え方、120ms 復帰の体感、release 直後のフル画質復帰を確認すること。



## 2026-09-16 — Timeline GPU面のPresent間引き（30Hz pacer）

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm`（`Impl::render`、`kMinPresentInterval`）。
- **確認できた事実:** 再生・スクラブ中は snapshot 毎に submit＋Present が走り最大60Hz。静動分離で CPU 再構築は軽くなったが GPU 投入は毎 tick 残っていた。
- **対応:** `render()` 入口で前回 Present から33ms未満なら描画を捨て、残り時間後に1回だけ wakeup して最新 snapshot を描く。snapshot 差し替え自体は継続されるため中間フレームは自然に間引かれる。タイマーは window をコンテキストにし、破棄時は自動キャンセル。resize/expose 直後も最大33ms遅延する。
- **価値または懸念:** 独立 device 上の submit 量が半減し、VP との GPU 奪い合いが減る。単発シークの表示遅延は最大33ms。ビルド・実機 runtime は未実施（AGENTS.md 制約でユーザー許可待ち）。
- **次に確認:** 再生・スクラブ中の滑らかさと遅延感、30Hz で不足なら間隔調整、Qt fallback 面には影響なし（同クラスだが pacer は GPU render 経路のみ）。



## 2026-09-16 — GPU timeline snapshot の静動分離（static cache＋dynamic tail）

- **関連:** `ArtifactTimelineTrackPainterView`（`timelineVisualRevision`／`isInteracting`／`touchTimelineVisuals`）、`ArtifactTimelineWidget::buildGpuTimelineSnapshot`（`gpuTimelineStatic_`）。
- **確認できた事実:** 従来は再生 tick 毎（16ms）に snapshot 全体（行・グリッド・全クリップ・UniString タイトル・波形64本・マーカー）を作り直していた。text クリップのタイトル等は tick 毎に変わらない（`refreshTracks` 時のみ更新）ため static 化しても描画は一致する。
- **対応:** 行・グリッド・クリップ・comp/marker を `(ppf, h/vOff, viewport, revision)` キーでキャッシュし、再生 tick は frame 依存のマーカー強調＋playhead のみ再構築（QVector 暗黙共有で static 部は無コピー）。revision は View 内の全 visual 変更点＋`mouseReleaseEvent` で bump、操作中（drag/marquee/scrub/pan）は `isInteracting()` で毎回再構築。`audioMuted` のみ snapshot に効くためそこだけ個別 bump（rate/pan/gain/reverse は snapshot 非消費）。
- **価値または懸念:** tick 毎の O(n) 再構築と UniString 生成が消える。GPU 全描画＋Present 自体は残る（次の Present 間引き対象）。atCurrentFrame 強調は dynamic へ移動し画素等価を維持。
- **次に確認:** ビルド・`check_module_hygiene`・実機で再生中の表示一致と負荷減を確認すること。**注意：本ツリーでは別作業の未コミット変更が同一関数に重なっている**（色・グリッド密度等）。コミット時は分離し、本キャッシュのキー／bump 点を壊さないこと。



## 2026-09-16 — Timeline GPU面を独立D3D12 deviceへ分離（shared immediate競合の解消）

- **関連:** `Artifact/src/Render/DiligentDeviceManager.cppm`（`createIndependentRenderDevice`、`createSwapChainForIndependentDevice`）、`Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm`（`Impl::initialize`）。
- **確認できた事実:** shared device は device＋immediate各1個のプロセス共用で、submit 時の同期が acquire/release の mutex だけだった。VP worker と timeline GUI が同一 immediate へ並行 submit していた（viewport/RTV ステート踏みの可能性あり）。
- **対応:** DeviceManager に独立払い出し（共有なし・失敗時フォールバックなし・契約コメント付き）を追加し、timeline 初期化を独立 device へ切替。失敗時は従来どおり Qt painter へ戻る（`setGpuTimelinePreviewEnabled` の既存処理）。curve 面は同クラスなので自動で独立する（＝timeline＋curve＋VP で最大3 device）。
- **価値または懸念:** context 競合のクラスごと消える。代償に VRAM・PSO・シェーダコンパイルが device 数分、D3D12 trim 通知は shared のみ（独立分は未登録・未検証）、起動時間増の可能性。ビルド・実機 runtime は未実施（AGENTS.md 制約でユーザー許可待ち）。
- **次に確認:** ビルド・`check_module_hygiene`、timeline/curve GPU 初期化・表示、VP と同時操作時の停滞解消、複数 device 時の VRAM・起動時間、trim 登録の要否。



## 2026-09-16 — Timelineトランジション範囲編集だけUndo経路が分離している

- **関連:** `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`TimelineClipMoveRequestedEvent`、`TimelineClipResizeRequestedEvent`。
- **確認できた事実:** 通常レイヤーのクリップ移動／トリムは既存の`MoveLayerToFrameCommand`／`TrimLayerToFrameCommand`へ接続できる一方、トランジションは同じイベント購読内で`setTimelineTransitionRange()`を直接呼び、Undo snapshotを作成していない。
- **価値または懸念:** Diligent面とQPainter面は同じ入力経路を共有するため、トランジションだけUndo不能という差は両表示面に現れる。今回の通常クリップ編集対応へ混在させるとcomposition-owned transition契約まで範囲が広がる。
- **次に確認すること:** transition範囲・関連レイヤー・重なり制約を復元できる既存command／snapshot所有者を調べ、単一ドラッグを1 Undoへまとめる。未検証のため今回の実装対象外。



## 2026-09-16 — Diligent TimelineのPhase 3は既存GPUテキスト経路を再利用する

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm`、`Artifact/src/Widgets/ArtifactTimelineWidget.cppm`、`docs/planned/MILESTONE_TIMELINE_DILIGENT_GPU_SURFACE_2026-08-29.md`。
- **確認できた事実:** Timeline snapshotの`texts`は既に`PrimitiveRenderer2D::drawGlyphText()`へ渡されており、Diligent側にラベル描画の入口が存在する。新しいQt描画経路や個別GPU実装は不要。
- **価値または懸念:** Phase 3のglyph atlasは既存の共通glyph／shader管理を拡張する形で進めるべきで、Timeline専用のテキスト資源を増やすとD3D12／Vulkan parityとキャッシュ寿命が二重化する。
- **次に確認すること:** `PrimitiveRenderer2D`のglyph atlasキャッシュ、atlas更新タイミング、device loss後の再生成契約を確認してからTimelineラベルの実機受入条件を定義する。未検証のため実装は次段階とする。



## 2026-09-16 — Timeline primitiveの色変換はrender-local cacheで共有できる

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm` のDiligent draw loop。
- **確認できた事実:** snapshotは`QColor`を保持し、描画時にlinear `FloatColor`へ変換していた。同一フレーム内ではrow／clip／markerの色が繰り返し現れる。
- **対応:** 32スロットの固定長cacheをrender-localに置き、`QColor::rgba()`が一致するprimitiveは変換結果を再利用する。cacheはヒープを使わず、snapshotの所有権やD3D12／Vulkan resource lifetimeを変更しない。
- **価値または懸念:** 色変換の`pow`回数を減らせる一方、色数が32を超える場合は循環置換される。未検証: 実機のprimitive分布とGPU submit時間への寄与はプロファイルで確認する。
- **次に確認すること:** static／dynamic両laneを同一renderで記録する場合のcache hit率と、D3D12／Vulkan別のCPU submit時間を計測する。低hit率ならスロット数を増やす前に、色tokenの共有化を検討する。



## 2026-09-16 — Timeline glyph描画のフォント解決をrenderer寿命へ寄せる

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm` の `drawGlyphText()`、`Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm`。
- **確認できた事実:** Diligent Timelineのstatic snapshotはprimitive配列を再利用できても、各presentでラベルをglyphへ展開する際にコードポイントごとの一時vector、`QFont`解決、`GlyphKey`構築を行っていた。既存のrenderer寿命フォントキャッシュは実装済みだが、この入口では使われていなかった。
- **対応:** 既存のフォントキャッシュを利用し、コードポイントの重複解決用scratch容量をrenderer寿命で再利用するようにした。glyph atlas、command buffer、D3D12／Vulkanのresource lifetimeは変更していない。
- **価値または懸念:** staticラベルを含むTimeline再描画で一時確保とフォントフォールバック問い合わせを減らせる。`UniString::toStdU32String()`とglyph packet appendは現状維持で、完全なゼロアロケーションを意味しない。
- **次に確認すること:** ビルド許可後、長いクリップ名・CJK・emojiを含むTimelineでatlas更新、ラベル表示、D3D12／VulkanのCPU submit時間を確認する。未検証のため、キャッシュ変更だけで表示 parityを断定しない。



## 2026-09-16 — Timeline waveform fallbackは表示幅で線分数を上限化できる

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm` のwaveform fallback描画。
- **確認できた事実:** immutableなピークpayloadは最大64本へ縮約していたが、表示幅が狭いclipでも同じ本数をsubmitしていた。細いバーは隣接線と同じpixelへ重なり、視認情報を増やさない。
- **対応:** fallback線分数を`min(64, peakCount, clipWidthInPixels)`でbounded化し、波形色のlinear変換結果も波形単位で共有する。texture payload、Diligentのresource所有、Qt fallbackは変更しない。
- **価値または懸念:** 小さいaudio clipやズームアウト時のcommand buffer append量を減らせる。1px未満の幅は1本に丸めるため、極端に狭いclipの表現はruntimeで確認が必要。
- **次に確認すること:** ビルド許可後、ズーム・スクロール中のaudio clipで波形の連続性、選択色、D3D12／Vulkan submit時間を比較する。



## 2026-09-16 — TimelineラベルのTextStyle一時値をsnapshot描画内で再利用する

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm` の`drawSnapshot()`。
- **確認できた事実:** static／dynamic laneのラベル描画で、同じ`TextStyle`の値をclip／markerごとに構築していた。glyph cacheはstyle一致を前提にしているため、描画側での一時値生成は不要だった。
- **対応:** snapshot単位で`ArtifactCore::TextStyle`を1つ再利用し、ラベルのpixel sizeだけ更新して既存`drawGlyphText()`へ渡すようにした。
- **価値または懸念:** ラベル数に比例する小さな一時オブジェクト生成を抑え、既存glyph atlasのキャッシュ契約を維持する。フォント選択やラベル内容の意味は変更しない。
- **次に確認すること:** ビルド許可後、clip／markerの異なるpixel sizeが混在するケースで表示とglyph cache更新が正しいことを確認する。
