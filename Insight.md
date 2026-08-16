# Insight Log

未解決の設計判断・runtime 検証待ちだけを記録する。実装済みの局所修正と履歴は `docs/analysis/INSIGHT_ARCHIVE_2026-08-11.md` を参照する。

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
