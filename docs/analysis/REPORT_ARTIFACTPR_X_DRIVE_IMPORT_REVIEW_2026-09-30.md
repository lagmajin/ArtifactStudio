# Xドライブ ArtifactPr 取り込みレビュー

**最終更新:** 2026-10-01

対象: `X:/Dev/ArtifactStudio/ArtifactPr` の変更済み11ファイル・未追跡4ファイルを、`J:/dev/ArtifactStudio/ArtifactPr` の現行実装と静的比較した。X側の更新日時は作業者や動作確認の証拠としては扱わない。

## 結論

ファイル丸ごとの移植は推奨しない。X側は古い基盤へ追加した実装であり、J側の共通Dock、GPU Program Monitor、CLI、NLEProjectStore/SequenceEditor/LinkingService/ConformService、編集セッションUndoを維持して機能単位で移植した。

## 2026-10-01 の選別取り込み

J側の既存storeとUndo経路に合わせ、メディアRescan/Relink、Source MonitorのIn/Outと3/4点編集、Insert/Overwrite/Lift、再生範囲と逆再生transportを既存ファイルへ限定移植した。NLE正規経路はstore snapshot Undoを維持し、新規signalやX側の旧Dock/音声/共同編集実装は取り込んでいない。ArtifactPr用の生成済みビルドターゲットが見つからず、この一連の変更はコンパイル未確認。ArtifactCoreのFFmpeg decoderとFreeFontの修正は個別オブジェクトビルド通過を確認した。

## ファイル別判断

| X側ファイル | 確認した内容・問題 | 取り込み判断 |
|---|---|---|
| `CMakeLists.txt` | Collaboration/AudioEngineの宣言と実装4ファイルを追加。J側にはGPU Monitor/CLIと共通Dock/GPU foundationの登録がある。 | 全体置換不可。採用モジュール確定後、J側の登録へ追記する。 |
| `include/ArtifactPrEditorEngine.ixx` | 再生範囲、3/4点編集、音声FX、offline等を追加。新規公開signalを追加。J側のサービス・Undo構成との整合が必要。 | 必要APIだけ設計し直す。公開signal追加は現行AGENTSにより設計レビューが必要。 |
| `src/ArtifactPrEditorEngine.cppm` | NLEStoreBridgeによるDemo→store再構築、fingerprint、projectModified/selection通知への新規自己接続。J側は長寿命storeとサービスを保持する。setCurrentSequenceはcurrentSequenceだけ更新後、currentProjectをloadFromDemoへ渡すため、プロジェクト側の同期確認が必要。projectModifiedごとに音声busを再構築。 | 置換不可。J側storeの編集経路に沿って各機能を移す。新規接続をそのまま導入しない。 |
| `include/EditCommand.ixx` | Ripple/Overwrite/Liftのtrack snapshot、Insert ID保持を追加。 | 意図は候補。J側のCore Undo対応とID保持を優先して再設計する。 |
| `src/EditCommand.cppm` | Ripple/Liftの全状態復元は改善意図あり。ただしInsertのUndoで後続startFrameにdurationを再加算する。Overwrite/LiftのtailはsourceIn/sourceOutを更新せず、timeline位置とdurationだけ変更する。冒頭に自己importがある。J側もInsert Undoの加算式を持つためX側からの新規回帰ではなく共通の既存問題。 | そのまま不可。Undo往復、分割source範囲、速度/逆再生を直して限定移植。 |
| `include/MediaPanel.ixx` | Rescan/Relink/Online/search UI宣言。Qt型include、実装側W_OBJECT_IMPLを確認。 | UI契約と既存イベント経路を揃えて候補。 |
| `src/MediaPanel.cppm` | sourceFileを実パスに使い重複排除。J側には既にsourceFile利用の更新あり。新規button/search接続。未検査時のOnline表示、一覧再構築後の検索再適用、Relink選択の扱いを要確認。 | Rescan/Relink等を機能単位で候補。J側既存修正を保持する。 |
| `include/TransportBarWidget.ixx` | 範囲ボタン、表示、slotを追加。 | 操作の必要性は候補。新規slot/接続をそのまま採用しない。 |
| `src/TransportBarWidget.cppm` | 範囲端clampを追加。負速度でも正のstepを計算しframe+stepを使うため逆再生が前進する。固定33ms tick。Pause=1もtick自体では前進分岐に入るため停止経路確認が必要。 | そのまま不可。符号、fps、Pause、状態所有者を揃えてから。 |
| `src/PrShortcut.cppm` | 範囲、3/4点編集、track追加を登録。空キー全てをcontext menuと表記するがsource-monitor/toolbar操作も含む。 | 実機能完成後に登録を検討。既存キーとコンテキスト衝突、空キー説明を修正する。 |
| `src/ArtifactPrMainWindow.cppm` | source monitor編集等を追加。一方ads::CDockManagerを使用しJ側の共通DockとGPU Monitor基盤とは異なる。 | 全体置換不可。J側MainWindowへ操作単位で移植。 |
| `include/ArtifactPrCollaboration.ixx` | room/presence/snapshot callback API。Impl所有に新規shared_ptrを採用。 | 別設計として保留。現行PImpl所有規則に合わせる。 |
| `src/ArtifactPrCollaboration.cppm` | static mapのLoopbackHubを共有する同一プロセスroom。schemaとactive sequence検証、snapshot履歴32件上限、weak callbackあり。session drainは接続時に呼ぶ。継続受信の実際の経路、Core API整合は未検証。 | ネットワーク共同編集の完成実装とは扱わない。同一プロセス用途、受信経路、snapshot適用Undoを明確化してから。 |
| `include/AudioEngine.ixx` | bus/FX処理API。新規unique_ptr<Impl>。 | そのまま不可。既存AudioPreviewMixerとの責務と所有規則を先に整理。 |
| `src/AudioEngine.cppm` | 自己import、`import Audio.Renderer`だがJ Coreのmoduleは`AudioRenderer`。createBus戻り値はCoreのSharedPtrで、保持先はstd::shared_ptr。bus clearはローカルmapだけでmixer側removeBusなし。Masterもmixer内蔵masterとは別にcreateBus。clip音量/pan更新はtrack bus全体に適用。source URIで配るため同一sourceの複数clipは重複入力し、時間範囲や速度を考慮しない。outputBus/busRouting/clip FXは再構築で使わない。全WAVをplanar変換する。 | 採用保留。型/API修正だけでは不足。既存preview/export音声経路へclip/track/master FXとroutingを組み込む設計が必要。 |

## 確定した問題の位置

- X `src/TransportBarWidget.cppm:159` 以降: 負速度分岐でも `frame + step`。
- X `src/EditCommand.cppm:103` 以降: Insert Undoの後続位置は加算。J `src/EditCommand.cppm:258` 以降にも同じ式あり。
- X `src/EditCommand.cppm:178` 付近、`:240` 付近: tail生成時にソース範囲を補正しない。
- X `src/ArtifactPrEditorEngine.cppm:1815` 以降: 新規自己接続によるstore commitとaudio rebuild。
- X `src/ArtifactPrMainWindow.cppm:2851`: `ads::CDockManager`。J側は`Artifact.DockManager`とGPU monitorを使用。
- J `ArtifactCore/include/Audio/AudioRenderer.ixx:8`: 正式module名`AudioRenderer`。
- J `ArtifactCore/include/Audio/AudioMixer.ixx:51`: `createBus`の戻り値`SharedPtr<AudioBus>`。

## 推奨順序

1. 共通するUndo問題をJ側で限定修正する。Insert位置の復元、分割のsource範囲、安定ID、duration、Core Undoとの同期を確認する。
2. J側サービス経路でoffline検査・Relink、再生範囲を移植する。Transportの負方向/fps/Pause整合を前提にする。
3. source monitorの3/4点編集をJ側NLE編集サービスとUndoへ接続する。UIとショートカットはその後に追加する。
4. 音声FX/routingと共同編集は独立設計レビューへ回す。

上記順序は提案であり、修正・取り込みの実施記録ではない。

## 選別移植の実施（2026-10-01）

J側ArtifactPrの既存ファイル9件へ機能単位で移植した。X側ファイルの全体置換、サブモジュール変更、CMake実行は行っていない。

- Mediaパネル右クリックにRescan Media / Relink Selected Mediaを追加。既存NLE source availability / relink APIとNLEStateCommandを使用する。再リンク時のmediaPoolパスもUndoスナップショットへ含める。未実在ローカル媒体をoffline表示する。
- Transport右クリックにTimeline In/Out、範囲解除、範囲始端/終端への移動を追加。Outは排他的な境界。既存fps対応timerを維持し、Reverse1x〜8xの符号と速度表示、端clampを修正する。範囲はセッション内状態で、新規/読込/シーケンス作成時に解除する。
- Source MonitorのIn/Outをソース専用状態へ分離。既存Insert/Overwriteは3点編集としてsource範囲を使用し、Fit Insert / Fit Overwriteでrecord範囲に合わせた4点編集を提供する。追加ボタンはnextCheckStateの標準活性化経路にcallbackを置き、新規signal/slot接続は追加していない。
- 正規NLE経路のInsert/Overwrite/Liftで編集範囲外のhead/tailを保持し、source範囲を速度・逆再生に応じて切り出す。Insertは挿入点を跨ぐクリップを分割後、storeのripple挿入を使う。新規tailは旧link groupへ自動加入させない。ロック対象は操作を拒否する。NLE全体のbefore/afterを既存Undoへ載せる。
- legacyコマンドのInsert Undo逆方向、Ripple Undo後続位置、Overwrite全track復元、Lift分割source範囲を修正する。Overwrite snapshotをコマンドJSONへ追加し、旧履歴には互換復元を残す。自己importを除去する。

静的確認: `git diff --check -- ArtifactPr`、宣言/実装対応、Core公開API名、追加差分の新規connect/W_SIGNAL/W_SLOT/QtCSS/QColorDialog不在を確認した。ビルド・CMake・テスト・実機確認は未実施。既存のQVectorコマンドsnapshotを使用し、新規標準コンテナは追加していない。stable_sortの一時領域とJSON snapshot/クリップ分割の確保は編集操作時に限定し、再生tickへ追加していない。

残る確認: コンパイル、Insert/Overwrite/LiftのUndo/Redo、速度/逆再生の分割source範囲、範囲端/Pause、媒体再リンク後の保存・再読込。AudioEngineと共同編集の新規ファイルは採用保留のまま。
