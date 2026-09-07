# 左ペインのキー追加とコンポジション作成待ちの調査

**最終更新:** 2026-09-07

## 調査範囲

現作業ツリーの静的調査と保存済み実行ログの確認。追加依頼を受け、キー追加判定・Undo・再生中のプレイヘッド二重更新を修正し、作成待ちの内訳計測を追加した。ビルド、テスト、新規の実機再現は未実施。既存の未コミット変更を含むソースを対象とするため、ログを出力したバイナリと完全に一致するかは未確認。以下の行番号は初回調査時点の参照を含む。

## 1. 左ペインでキーを追加しても移動で追加されない

原因として、プロパティと Transform3D のキー状態の不一致がコード上で確認できる。

- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm:1106` の `togglePropertyKeyframeAtCurrentTime()` は `property->addKeyFrame()` / `removeKeyFrame()` のみ実行し、Transform3D を更新しない。
- `ArtifactCore/src/Property/AbstractProperty.cppm:653` のキー追加は内部配列の更新のみであり、レイヤーへの値反映コールバックはない。
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm:1699` の `changed()` は変更イベントを発行する。ここで Transform3D のキーを同期してはいない。
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:427` の `captureGizmoKeyState()` は Transform3D のキー数だけから `positionAnimated` 等を判断する。
- 同ファイル `:513` の `applyLiveGizmoTransform()` はその判定と Auto-Key 設定に従う。プロパティ側だけにキーがあり、Transform3D にキーがなく、Auto-Key が無効なら `setInitialPosition()` に進み、`:585` 以降のプロパティへのキー追加も行わない。

したがって「左ペインで位置キーを打つ → 時間を進める → ビューポートで移動」という操作に対し、既にアニメーションしている位置を未アニメーションとして扱う経路が存在する。Auto-Key が有効な場合や別の編集経路で Transform3D にキーが入っている場合は分岐が異なる。ユーザーの実操作・設定での再現は未確認。

修正方針は、移動のキー化判定に正規のプロパティ側のアニメーション状態を含め、左ペインの追加・削除と移動確定／Undo の同期方針を統一すること。現在時刻だけを同期して過去のキーを取り落とさないこと、X/Y の別チャンネル状態を保持することが必要。

## 2. コンポジション作成時の待ち

保存済み `C:/Users/lagma/AppData/Roaming/Artifact/WidgetCreationReports/` の以下のログに実測あり。

| ログ名 | service create | projectCreateMs | managerCreateMs | Timeline Ready |
|---|---:|---:|---:|---:|
| widget-creation_20260906_204743_887_38956.jsonl | 2105.50 ms | 1035.97 ms | 1026.36 ms | 364.62 ms |
| widget-creation_20260906_150716_562_25700.jsonl | 1751.01 ms | 829.30 ms | 885.99 ms | 258.74 ms |
| widget-creation_20260906_133946_789_61060.jsonl | 2173.03 ms | 915.82 ms | 1212.32 ms | 306.74 ms |

直近の作成は日本時間 2026-09-07 07:13:39。サービスの作成時間はプロジェクト作成と manager 作成を含む。Timeline Ready は後続の別区間であり、入れ子の constructor / dock 計測をさらに加算してはいけない。

直近の `change-current-composition-sync` は 0.017 ms、defaults は 0.04 ms。作成後の dock 登録は 30.30 ms。したがって、記録されたケースの主な待ちは切替や defaults ではなく、作成要求内の同期処理にある。タイムライン準備も追加の待ちを生じている。

`Artifact/src/Service/ArtifactProjectService.cppm:5905` はプロジェクト未作成時に `manager.createProject()`、続いて `manager.createComposition(params)` を同期実行する。後者の計測にはコンポジションのモデル構築だけでなく `ArtifactProject.cppm:1201` から発行する作成・プロジェクト変更イベントの処理も含まれる。

追跡した同期処理にはプロジェクトフォルダ作成、ProjectCreated の設定ロード、ProjectChanged の Python hook、ファイル監視更新などがある。`refreshFileWatcherPaths()` (`ArtifactProjectService.cppm:1841`) は既存コンポジションのレイヤーを `toJson()` して素材パスを収集するため、既存レイヤーが多い場合の追加負荷候補。ただしこれが今回の約1秒の主因であるとは未確定。

結論: 遅延の存在と大区間はログで確認済み。manager 内部の関数別・イベント購読者別の所要時間はログにないため、約1秒を占める単独関数は断定できない。次の確認箇所はモデル構築、イベント発行、各同期購読者の時間内訳。初回のプロジェクト新規作成と既存プロジェクトへの追加は分けて測定する。

## 追加依頼への対応

### キー修正

- `applyLiveGizmoTransform()` は操作開始時のプロパティキーを含めてアニメーション状態を判断する。位置・回転・スケールに適用。
- プロパティにキーがある場合はそのチャンネル単位で追記する。Xだけをキー化した操作からYへ意図しないキーを増やさない。Transform3Dにのみアニメーションがある従来経路とAuto-Key設定は維持。
- `GizmoTransformSnapshot` にチャンネル別の操作時刻のキー有無・キー実体・アニメーション有無を保持し、Undo/Redo・キャンセル・コマンド登録失敗の復元で時刻・値・補間・ハンドル・roving・anchor・color labelを戻す。他の時刻のキーは変更しない。既存キーの値編集でも補間設定を保持する。
- Undoの時刻スケールを、移動操作と同じFPS丸めに統一。29.97fpsをUndoだけ29へ切り捨てない。
- スナップショットは7チャンネルの固定配列とし、マウス移動ごとのコピーで全キーフレーム配列を複製しない。

### 作成待ちの追加計測

`ArtifactProjectManager.cppm` と `ArtifactProject.cppm` に既存 WidgetCreationDiagnostics を利用する計測を追加した。処理順やイベント配線は変更していない。

- `Project Create Breakdown`: モデル・フォルダ作成、ProjectCreated購読者、health check。
- `Composition Model Breakdown`: モデル作成、CompositionCreated購読者、ProjectChanged購読者、その他。
- ProjectChanged は `CompositionRenderController` / `ArtifactCompositionEditor` のコンポジション設定、Inspector更新、ProjectModel再構築にもつながる。各処理の支配率は未測定。
- 計測ログ出力自体のコストは外側のservice計測に含まれ得る。入れ子の時間を合算せず内訳として読む。

### プレビュー中のプレイヘッド

`ArtifactTimelineWidget.cppm` は16msタイマーで小数フレーム位置を表示している一方、FrameChanged通知では毎回 `setCurrentFrameForAll(integerFrame)` を呼んでいた。そのため通知とタイマーが同じ表示位置を交互に上書きする。

再生中は整数フレームでタイムコードだけを更新し、プレイヘッドは既存の小数フレーム表示経路へ統一した。停止中のシークは従来経路を維持。再生中の自動横スクロールも小数位置へ追従させた。

別要因として、保存済み `Logs/PlaybackSessions/playback_session_20260907_000812_293.log` は30fps・speed=1・playEveryFrame=trueで、9,167ms中167フレーム進行（セッション全体の平均約18.2fps、開始・停止時間を含む）。120 tick時点も約6.085秒であり、設定30fpsより遅い。`ArtifactPlaybackEngine.cppm` の全フレーム再生は順番にフレームを送り、GUIの同期処理完了を待つ。タイムラインの予測位置は設定FPSで進むため、差が1.5フレームを超えた補正でも逆戻りが起こり得る。この補正と実処理の遅延は今回変更していない。

同ログには `QWidget::mapFrom(): parent must be in parent hierarchy` の警告が繰り返しある。呼び出し元と所要時間は未確認のため、これを主原因と断定しない。

## 検証状況・次の確認

差分の空白検査とモジュール宣言・include配置・既存登録・コマンド前後スナップショットの静的確認を実施。ビルド、CMake実行、テストはユーザー指示がないため未実施。

実機では、Auto-Key OFFで左ペインのXのみ／XとYをキー化して別時刻へ移動、同時刻キー編集、Undo/Redo、Esc、複数選択、29.97fpsを確認する。再生は30fps・低負荷と重いプレビューを分け、停止・シーク・ループ境界も確認する。作成待ちは新計測を含むバイナリで初回と2個目のログを比較する。
