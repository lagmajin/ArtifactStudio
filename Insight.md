# Insight Log

未解決の設計判断・runtime 検証待ちだけを記録する。実装済みの局所修正と履歴は `docs/analysis/INSIGHT_ARCHIVE_2026-08-11.md` を参照する。

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
