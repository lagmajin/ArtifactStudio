# Insight Log

未解決の設計判断・runtime 検証待ちだけを記録する。実装済みの局所修正と履歴は `docs/analysis/INSIGHT_ARCHIVE_2026-08-11.md` を参照する。

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
