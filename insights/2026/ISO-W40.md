**最終更新:** 2026-10-09

# Insight Register — 2026-W40

期間: 2026-09-28 – 2026-10-04

## 2026-10-04 — Build log: Artifact script field map API and Qt widget API mismatches

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` (`migrateScriptFields`), `Artifact/src/Widgets/ArtifactCompositionAudioMixerPresentation.cppm` (`showAllEffectsMenu`), `Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm` (timecode editor).
- **確認できた事実:** The pasted MSVC log showed `ArtifactScriptSerializedFields` is a `std::unordered_map`, but `migrateScriptFields` used Qt `constFind` / `value()` calls. It also showed `QVariant::toInt(-1)` passed an integer where Qt expects `bool*`, and `QTimeEdit::setSection` / `FixOffset` do not exist in the Qt 6 API in this workspace. The mixer source already included the missing `QDialog` definition; that diagnostic is consistent with the incomplete type error cascading from header/module visibility and should be rechecked after the other compile fixes.
- **対応:** Replaced the map lookup with `find` / `end` / `it->second`; changed the QVariant conversion to `toInt(&indexOk)` with an invalid-data guard; removed the unsupported QTimeEdit call while preserving the display format and widget behavior.
- **価値または懸念（未検証）:** These edits directly address the reported API errors while preserving unrelated pre-existing changes. Build/test were not run under the repository instruction, so remaining diagnostics (including QDialog/connect overload resolution) are unverified.
- **次に確認すべきこと:** After an authorized build, verify these three translation units compile and inspect subsequent diagnostics independently. The QTimeEdit `HH:mm:ss.ff` display-format semantics should also be checked at runtime because Qt's sub-second formatting is millisecond-based.



## 2026-10-04 — Qt global types in public module declarations

- **関連:** `Artifact/include/Widgets/AudioMixerWidget.ixx` (`createParameterEditor`), `Artifact/src/Widgets/ArtifactCompositionAudioMixerPresentation.cppm`.
- **確認できた事実:** `QDialog` / `QLabel` forward declarations were after `export module`, so those declarations were attached to `Artifact.Widgets.AudioMixer` even though the exported function's pointer types are Qt global types. Including Qt's complete `QDialog` definition in a consumer cannot complete a distinct module-attached class declaration.
- **対応:** Moved the forward declarations into the global module fragment so the public function and Qt headers refer to the same global class types.
- **価値または懸念（未検証）:** This addresses a class-identity mismatch in addition to the visible incomplete-type diagnostic. Build remains unverified under the repository's no-build instruction.
- **次に確認すべきこと:** On an authorized build, check `AudioMixerWidget.ixx`, its implementation unit, and presentation consumer compile with the relocated declarations.



## 2026-10-04 — Build log: standard vector API and direct OIIO type include

- **関連:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` (`ProxyWorkerSlot` batch), `Artifact/src/Render/ArtifactRenderQueueService.cppm` (bit-depth output type mapping).
- **確認できた事実:** The proxy worker batch is `std::vector<ProxyWorkerSlot::Entry>` but used Qt's `isEmpty()` spelling. The render service declared `OIIO::TypeDesc` without including an OpenImageIO header in its global module fragment.
- **対応:** Changed the vector check to `empty()` and added the focused `<OpenImageIO/typedesc.h>` include before `module Artifact.Render.Queue.Service;`.
- **価値または懸念（未検証）:** This should resolve the reported container API error and make the OIIO declaration visible at its source. Build remains unverified under repository policy.
- **次に確認すべきこと:** On an authorized build, verify both translation units compile; then check the EXR/TIFF output types at runtime.



## 2026-10-04 — Native script functions must match the host callback signature

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` (`installCompositionScriptApi`), `ArtifactCore/include/Script/ArtifactScript/ArtifactScript.ixx` (`ArtifactScriptHost::registerFunction`).
- **確認できた事実:** `registerFunction` stores callbacks shaped as `ArtifactScriptValue(std::span<const ArtifactScriptValue>)`. The new `getFrame` and `fps` callbacks were zero-argument lambdas returning scalar values, so they could not convert to that callback signature.
- **対応:** Changed both callbacks to accept the argument span and return an explicit `ArtifactScriptValue`, matching the neighboring `timeToFrame` / `frameToTime` registrations.
- **価値または懸念（未検証）:** The host can now register and invoke the functions through the same native callback ABI as its other functions. Build and script execution remain unverified.
- **次に確認すべきこと:** On an authorized build, verify the composition translation unit and evaluate `getFrame()` / `fps()` with and without a composition.



## 2026-10-04 — Script library callbacks must capture their shared argument converter

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` (`installScriptLibraryFunctions`).
- **確認できた事実:** `numberAt` is a local stateless lambda used by the registered math, random, and temporal callbacks. Those callback lambdas had empty capture lists, while `wiggle`, `loopOut`, and `posterizeTime` already captured shared helpers explicitly.
- **対応:** Added explicit `numberAt` value captures to every callback that uses it, including `linear` / `ease` alongside `sampleTemporal`.
- **価値または懸念（未検証）:** The callback closures own the tiny stateless helper by value and no longer rely on illegal implicit capture. Build and expression evaluation remain unverified.
- **次に確認すべきこと:** On an authorized build, verify `installScriptLibraryFunctions` compiles and the math / temporal functions still evaluate with optional arguments.



## 2026-10-04 — Script vector normalization narrows double results

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm`, `installScriptLibraryFunctions()` の `normalize`。
- **確認できた事実:** `ArtifactScriptVec2` / `ArtifactScriptVec3` の成分型は `float` だが、正規化長 `len` は `double`。成分を `len` で割ると `double` となり、braced aggregate initialization からの暗黙縮小変換は MSVC C2397 になる。
- **追加で確認した事実:** 同じ `normalize` コールバック内の Vec2/Vec3 補間 (`mix`) も、`double` の補間係数 `t` により各成分演算の結果が `double` になる。また、ping-pong loop の `phase` は負値補正で再代入される。
- **変更:** 正規化と補間の各成分を `float` に明示変換した。数値演算は従来どおり `double` で行い、ベクトル格納境界でのみ縮小する。`phase` から `const` を外して負値補正を可能にした。
- **未検証:** ビルドは AGENTS.md の制約に従い実施していない。
- **次に確認すべきこと:** ユーザー側の再ビルドでこのエラーが解消し、normalize の後続診断がないこと。


## 2026-10-04 — QApplication destructor access violation after completed UI teardown

- **関連:** `Artifact/src/AppMain.cppm` (`QApplication a`, shutdown diagnostics, `main` return), `%APPDATA%/Artifact/Logs/ShutdownSessions/`。
- **確認できた事実:** Visual Studio のスタックは `QApplication::~QApplication()` から `main` の `return exitCode` に戻る位置でアクセス違反を報告。2026-10-04 23:46:45 のローカル shutdown log は `aboutToQuit`、render queue / composition editor / playback / autosave の停止、event loop return、render-center と main window の teardown 完了まで記録している。ログは QApplication の破棄に入る前で終わっており、破損した Qt オブジェクトの特定情報はない。最近の Windows Application event log および `%APPDATA%/CrashDumps` に、この事象に対応する dump / event は見つからなかった。
- **仮説（未検証）:** QApplication が管理する QWidget / QObject 登録状態、または Qt のプロセス終了時 cleanup が無効なポインタを参照している可能性がある。トップレベルウィンドウの明示破棄が完了しているため、単純な main window の未破棄だけでは説明できない。
- **価値または懸念:** shutdown 経路は UI teardown までは正常に進む証拠が得られた。発生条件や first-chance exception 時の Qt 内部フレームが不明なまま cleanup 順序を変えると、別の所有権問題を隠すおそれがある。
- **次に確認すべきこと:** 毎回の終了か、特定操作後だけかを確認する。次回発生時は `shutdown_*.log` が引き続き `COMPLETE top-level UI teardown` まで進むか確認し、Visual Studio で first-chance access violation 時の call stack と `QApplication::topLevelWidgets()` の残存一覧を採取する。



## 2026-10-03 — ProxyWorker の batch モード（protocolVersion 2）とプロセス起動コスト実測

- **関連:** `Artifact/src/Worker/ArtifactProxyWorker.cpp`（`runProxyJob` / `main` の request 分岐）、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`（`ProxyWorkerSlot::Entry` / `pollProxyWorkerSlot` / `finalizeProxyEntry` / `findProxyEntry` / `processNextProxyJob` / `cancelProxyQueue` / `~Impl`）、`tools/proxy_worker_smoke_test.py`（`--batch`）。
- **実測（推測ではない）:** プロセス起動コストを powershell で 20 回実測したところ **平均 1,015.9 ms/回**。0.05 秒の無音エンコードを含む値なので、実質ほぼ起動コストである。1 ジョブ = 1 プロセス運用では素材数 N に対して N × 約 1 秒の固定コストが積み上がるため、並列化を入れても 1 ジョブあたりの固定コストは減っていなかった。これが batch 導入の根拠。
- **対応:** (1) worker 側。単一ジョブ処理を `runProxyJob(request, output)` として `main` から切り出し、`main` は request の形で分岐する。`protocolVersion:1` は従来どおり単一ジョブ（**後方互換**）、`protocolVersion:2` かつ `jobs` 配列を持つ形式は 1 プロセスで全ジョブを逐次実行し、末尾に `batchCompleted`（succeeded/total）を 1 行出す。メッセージは従来どおり各ジョブの `jobId` 付きで出るので、ホスト側の jobId 照合方式は不要になった。(2) ホスト側。`ProxyWorkerSlot` が「1 ジョブ」ではなく「1 バッチ」を持つようにし、`std::vector<Entry> entries` を導入。`processNextProxyJob()` は並列上限がスロット数である点はそのままに、キューを **1 スロット = 1 バッチ** で充填し、`.proxy-batch-<uuid>.json`（`protocolVersion:2`）を 1 枚書いてプロセス 1 本を起動する。`cancelPath` はジョブ単位のままなので、キャンセル・破棄時クリーンアップ・重複ジョブ判定はすべて `entries` を走査する形に更新した。
- **重要な判断（ホスト側 batch / 逐次実行）:** batch 内のジョブをスレッド並列にはしない。理由は (a) 既存ホストの「1 ジョブ = 1 `cancelPath`」モデルと batch が衝突する、(b) FFmpeg の HW エンコーダは基本スレッドセーフでない、(c) 初回導入として品質リスクが高いため。並列度は引き続き **プロセス数** で決まるので、`Proxy/ParallelJobs` の意味は変わらない。
- **価値または懸念:** 起動コスト実測値（1 ジョブあたり約 1 秒）が、多数ジョブの批量生成で 1 回だけRendezvous になる。ただし**バッチの粒度を「1 スロット = キュー全量」にした副作用**として、1 素材しかないときは従来と同じコストになる。`pollProxyWorkerSlot` は「プロセスが終了したら、報告の来ていないジョブは失敗扱いにしてロールバックする」判定を入れた（batch の途中でプロセスが落ちた場合に `.partial` や中途半端な出力を取り残さないため）。**未検証:** ビルド未実行のため、MSVC コンパイル通過・実際の batch 動作・並列度とバッチ粒度の組み合わせでの wall-clock は未確認。`tools/proxy_worker_smoke_test.py --batch N` を追加したので、これが自動検証の入口になる。
- **次に確認すべきこと:** ビルド許可後、(a) `--batch 1`（後方互換）と `--batch 3` の両方が通ることを確認、(b) 多数素材で並列度 1 / 4 と batch 有効・無効の 4 通りで wall-clock を実測し、1 ジョブあたり約 1 秒の削減が実際に出るか確認、(c) batch 中に `cancelProxyQueue()` した時、報告の来ていない後続ジョブが `.partial` を残さずロールバックされることを確認、(d) worker を強制終了させた時も同じロールバックが効くことを確認。



## 2026-10-03 — ギズモの静的 Transform とプロパティ保存値の同期

- **関連:** `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`、`TransformGizmo.cppm` の `syncAnimatedProperty()`、`Artifact/src/Layer/ArtifactAbstractLayerTransform.cppm`。
- **確認できた事実:** Global Transform の評価はキーなしプロパティも読み取る。TextGizmo は従来キー付きプロパティだけを更新し、静的ドラッグでは Transform とプロパティ保存値がずれていたため、今回 TextGizmo の保存値も同期した。通常 TransformGizmo の同期 helper にもキー付きだけを更新する同じ条件がある。
- **未検証の仮説:** 通常ギズモの別 caller が保存値を更新しなければ、他のレイヤーでも静的 Transform の表示や Undo に影響し得る。
- **価値／次の確認:** 他レイヤーは今回変更せず、通常ギズモの mutation caller とプロパティ再取得を調べてから、保存値同期の責務を確認する。



## 2026-10-03 — Animator の値変更と文字組みキャッシュの分離候補

- **関連:** `Artifact/src/Layer/ArtifactTextLayer.cppm` の `markDirty()`、Animator setter、`updateGlyphEvaluation()`。
- **確認できた事実:** Animator の位置・回転・Opacity 変更でも `markDirty()` が `shapedBaselineKey_` を破棄する。評価処理は元の glyph 配列を再利用して Animator を適用できる構造だが、現在の setter では文字組みのキャッシュも失われる。
- **未検証の仮説:** 長文の Animator 数値ドラッグで文字組みを繰り返し、即時プレビューの応答性に影響する可能性がある。今回の描画キャッシュ無効化・値反映の修正とは別の性能課題として扱う。
- **価値／次の確認:** 実機計測で文字組み時間を確認してから、文字・フォント・レイアウト変更と Animator 評価変更の dirty 管理を分離できるか検討する。今回この分離は実装していない。



## 2026-10-03 — Audio/Effects 3 ヶ月未更新: ハードコード 44100・tail 未申告・Limiter の毎ブロック確保

- **関連:** `Artifact/include/Audio/Effects/`（Base / Limiter / Compressor / Equalizer / Delay / Chorus / Reverb / Distortion）、`ArtifactCore/include/Audio/AudioEffect.ixx`（`latencySamples` / `tailSamples` の契約）、`ArtifactCore/src/Audio/AudioBus.cppm:241-262`（集約側）、`ArtifactCore/src/Audio/AudioMixer.cppm:274-285`（`graphTailSamples`）。
- **確認できた事実（静的読み取り、ビルド・実機未確認）:** (1) app 側 7 エフェクトは `segment.sampleRate` を一切読まず、基底の `sampleRate_`（既定 44100）だけで計算していた。Core 側の同種エフェクト（`AudioCompressor.cppm:34`、`AudioDelay.cppm:35`、`AudioHighLowPass.cppm:32`）は `segment.sampleRate` を読んでおり、**Core が正しく app がずれていた**。オフライン書き出しが 48kHz の場合、delay/chorus/reverb の遅延 Sample 数が 44100 前提になりピッチと長さがずれる。(2) 7 エフェクトとも `latencySamples()` / `tailSamples()` を未オーバーライド（基底の `return 0` のまま）。`AudioBus::tailSamples()` が 0 を返し `graphTailSamples()` も 0 になるため、オフライン書き出しが `baseSamples + 0` で終わり、reverb/delay の余韻が切り詰められる。(3) `LimiterEffect.cppm:66,78` が `std::vector<float>(numSamples)` を**毎ブロック 2 回**ヒープ確保し、`:82-89` が `lookaheadSamples_`（最大 512）まで二重ループしていた。(4) `CompressorEffect.cppm:55` の auto-makeup が `effectiveMakeup = gainReductionAtThreshold * 0.5f` で符号が逆。既定（threshold=-20, ratio=4）で **-7.5dB の減衰**になり、クラス説明の "auto make-up gain" と逆の挙動。(5) `EqualizerEffect.ixx:35` が `float sampleRate_ = 44100.0f` で基底の `int sampleRate_` を shadow していたため、継承の `setSampleRate(int)` が no-op。(6) `ChorusEffect.cppm:129` の `initializeEngine()` が `if/else` チェーンの外側にあり、未知のキー抵达即使座に 6 本の遅延バッファを再確保＋LFO 位相をリセットしていた。
- **対応（親リポジトリ `Artifact` のみ変更）:** (1) 基底 `ArtifactAudioEffectBase.ixx` に `syncSampleRate(segment)`（値が変わったときだけ `reinitOnSampleRate()` を呼ぶ）と `reinitOnSampleRate()` の virtual を追加し、7 エフェクトすべての `process()` 冒頭で呼び出すようにした。`setSampleRate` の直接代入を全廃し、必ず基底の実装を通す形にした。(2) `latencySamples()` / `tailSamples()` を実装。Limiter=先読み分、Chorus=中心遅延+depth、Delay=遅延時間×feedback 減衰の repeats、Reverb=tank ループ×減衰の repeats、Compressor/EQ/Distortion=0。(3) `LimiterEffect` を固定長リング＋単調減少 deque のスライド最大値に再構成し、`process()` 内のヒープ確保をゼロ、O(N×L) を O(N) にした。`lookaheadBuf_` / `lookaheadWritePos_` の未使用メンバは削除。(4) Compressor の auto-makeup 符号を反転し、`getParameter("makeup")` が実効値と食い違う問題があるため、auto 有効時は makeup をそのまま返す仕様にした。(5) EQ の shadow メンバを削除し、基底の `sampleRate_` を使う。`process()` と `calculateBiquadCoefficients()` の両方を更新。(6) Chorus の `setParameter` から無条件 `initializeEngine()` を外し、LFO は `setFrequency`（位相保持）で更新。初回のみ `initialized_` で位相を 0 から始める。
- **価値または懸念（未検証）:** オフライン書き出しとリアルタイム再生でサンプル数が一致し、reverb/delay の余韻が書き出しに含まれるようになる。Limiter の毎ブロック確保と O(N×L) が消え、RT 期限リスクが軽減する。ただし (a) Delay/Reverb の tail 計算は feedback/decay の**理論値**で実測していない（実測すると数倍長い可能性がある）。(b) Chorus/Limiter の `latencySamples()` を申告したことで、`AudioMixer::applyLatencyCompensation` が実際に下流をずらすようになるため、**バス間の位相が変わる**。既存プロジェクトで音が変わっている可能性がある。(c) `kMaxChannels = 32` を超えるセグメントは Limiter の遅延を素通しする。(d) ビルド・実機は未実施（AGENTS.md 制約）。
- **未修正（記録のみ・子リポジトリ `ArtifactCore`）:** app 側 7 エフェクトと Core 側 7 エフェクト（`AudioCompressor` / `AudioDelay` / `AudioReverb` / `AudioChorus` / `AudioParametricEQ` / `AudioHighLowPass` / `AudioBassTreble`）が**完全に二系統**。app 側を import する箇所は 0。Core 側の唯一の利用は `AudioBus.cppm:535` の `dynamicPointerCast<AudioCompressor>` でゲインリダクション計器용だが、app 側 `CompressorEffect` は Core 型ではないため**ミキサーのゲインリダクション表示は常に 1.0（無圧縮）**。解消には Core 側エフェクトの削除、または `AudioEffect` 基底への virtual `gainReductionDb()` 追加が必要。どちらも子リポジトリ変更になるため今回は未着手。
- **未修正（記録のみ・親リポジトリ）:** (1) app 側 7 エフェクトは `effectType()` / `toJson()` / `fromJson()` を未オーバーライド。基底の `effectType()` は `"unknown"` を返し、`AudioBus::toJson`（`:589`）がそれで書き出すため、バス FX スロットのパラメータが**保存時に消失**する。(2) 3 つの Reverb パラメータ（`density` / `damping_freq` / `decay_lf_mult`）が UI に出るが DSP で一度も読まれない（Dattorro の damping は `kDampCoeff = 0.4f` ハードコード）。(3) denormal 対策（FTZ/DAZ）が Audio 経路に一切ない。(4) `ArtifactAbstractAudioEffects.ixx` と `ArtifactAudioOutput.ixx` は dead モジュールだが CMake 登録されている。(5) `AudioMixerWidget.cppm:400-418` は `Enum` タイプを `QDoubleSpinBox` に落としており、`mode` / `algorithm` が任意小数を入力できる。
- **次に確認すべきこと:** (a) 48kHz のComposition を書き出して delay/reverb の余韻が含まれること。(b) 44100 と 48000 で同じ素材を書き出してピッチが一致すること。(c) Chorus のスライダ操作でクリックノイズが出なくなること。(d) Compressor の既定値で減衰が -7.5dB ではなく +7.5dB の makeup になること。(e) 複数バス構成で Limiter/Chorus を含むときに位相ずれが生じないこと。



## 2026-10-03 — Components 3 ヶ月未更新: script descriptor 欠落と Undo スナップショットの componentHost 漏落

- **関連:** `Artifact/include/Layer/ArtifactLayerComponentSystem.ixx`（factory 一式）、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`（`syncBuiltinComponentDescriptors` / `syncBuiltinBoolsFromHost` / `componentDescriptorSnapshot` / `restoreComponentDescriptorSnapshot`）、`Artifact/src/Layer/ArtifactAbstractLayerPersistence.cppm`、`Artifact/src/Layer/ArtifactAbstractLayerComponentRouting.cppm`、`Artifact/include/Components/FieldComponent.ixx`（削除）。
- **確認できた事実（静的読み取り、ビルド・実機未確認）:** (1) `artifact.component.script` には reader（`syncBuiltinBoolsFromHost` の `boolFromHost`、`ArtifactAbstractLayer.cppm:483`）と writer（`setComponentDescriptorPropertyValue` の `component.script.enabled`、`ArtifactAbstractLayerComponentRouting.cppm:147-157`）があるのに、descriptor を生成する `makeScriptComponentDescriptor` が存在しなかった。`syncBuiltinComponentDescriptors`（`:203-469`）は cloner / layout / crowd / motion-dynamics / sequence-player / collision / joint / fracture / particle-emitter / fluid / pyro と動的 source を作るが script を作らない。そのため `findByType` が常に null を返し `boolFromHost` は false を返す。`fromJson` は legacy `components` ブロック（`Persistence.cppm:1097` で `scriptEnabled=true` を復元）を先に読み、続けて `componentGraph` を適用して `syncBuiltinBoolsFromHost()`（`:1684`）を呼ぶため、**`componentGraph` を持つファイル（＝現行シリアライザが書く全ファイル）では script が常に false に戻る**。(2) `componentDescriptorSnapshot()`（`:2904-2922`）は generators / fields / cloneModifiers / clonerTransforms の 4 つだけをシリアライズし、`componentHost_`（builtin descriptor 12 種）を丸ごと含めていなかった。そのため `LayerComponentDescriptorSnapshotCommand` 経由では Cloner / Layout / Crowd / Collision / Fluid / Pyro / Script / Particle Emitter / Joint / Fracture の toggle が Undo されない。(3) `Artifact/include/Components/FieldComponent.ixx`（231 行、6 クラス）は `import Artifact.Component.Field` の呼び出し元が 0 で、CMake 登録のみ。フィールドの現行実装は文字列 `typeId` + `LayerFieldDescriptor` であり、このクラス階層は旧設計の残骸だった（キー schema も現行 `centerX` / `outerRadius` / `falloffWidth` 体系と不一致）。
- **対応（親リポジトリ `Artifact` のみ変更）:** (1) `makeScriptComponentDescriptor(bool)` を `ArtifactLayerComponentSystem.ixx` に追加（`builtin.script` / `artifact.component.script` / phase=Intent / scope=Layer / order=800）。`syncBuiltinComponentDescriptors()` の pyro の直後に `componentHost_.upsert(...)` で登録し、reader と writer を対にした。(2) `componentDescriptorSnapshot()` に `componentGraph` として `componentHost_.toJson()` を追加。`restoreComponentDescriptorSnapshot()` は `componentGraph` を任意扱いで読み、`fromJson` → `syncBuiltinBoolsFromHost()` → `syncBuiltinComponentDescriptors()` の順で適用して bool と descriptor の双方向を一致させた。旧スナップショット（Undo 履歴に残ったもの）に `componentGraph` が無い場合は `componentHost_` に触らない後方互換にした。(3) `Artifact/include/Components/` を削除し、`cmake/ArtifactSources.cmake:55` の登録も除去。
- **価値または懸念（未確認）:** script の有効性が保存/再読込を往復しても保持されるようになる。Components パネルの toggle が Undo/Redo の対象になる。ビルド・実機は未実施（AGENTS.md 制約）。script descriptor を追加したことで `componentGraph` に `builtin.script` が新規に現れるため、**既存プロジェクトを読み込むと componentGraph の descriptor が 1 件増える**。共同作業のグラフ diff（`UndoManager.cppm:6433` が `componentDescriptorSnapshot()` を比較）に影響する可能性がある。
- **未修正（記録のみ）:** (1) `jointComponentEnabled_` は descriptor に書くだけで、`syncBuiltinBoolsFromHost` 側の読み戻しが無い。(2) source component は factory を迂回して ID を文字列連結し、cloner と `order = 100` が重複（`:459-473`）。(3) `component.fields.<index>` 系パスが `count() + 1` で ID を生成するため、削除→追加で `extra.*` ID が衝突する（`ComponentRouting.cppm:161,356,522`）。(4) `syncBuiltinComponentDescriptors()` が `const` アクセサから呼ばれ UI スレッドとレンダースレッドで `NamedVector` を競合しうる（`:3004` の `validateLayerComponents() const` など）。(5) `autoFixValidationIssues`（`LayerComponentSystem.ixx:610-653`）が 44 行あるが呼び出し元ゼロで、validate エラーを直す UI 手段が無い。(6) `sequence-player` は `makeSequencePlayerComponentDescriptor(false)` で常に disabled、3 つの settings が書かれるだけで読まれない。
- **次に確認すべきこと:** (a) script component を有効にしたレイヤーを保存→再読込して有効のままか。(b) Cloner / Layout / Fluid 等の toggle を Undo/Redo して元に戻るか。(c) 既存プロジェクトを読み込んで `componentGraph` に `builtin.script` が増えても問題がないか、共同作業のグラフ diff が壊れないか。(d) `validateLayerComponents() const` を UI とレンダーの両スレッドから同時に呼んで競合しないか。


## 2026-10-02 — Components 専用プロパティの UI 呼び出し漏れ

- **関連:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidget.cppm`、`Artifact/src/Layer/ArtifactAbstractLayerPropertyGroups.cppm`。
- **確認できた事実:** 通常の `getLayerPropertyGroups()` はコンポーネント固有グループを生成しない。Cloner ヘッダーは通常グループだけを準備して `getProperty("component.cloner.enabled")` を読み、キャッシュ未生成時は切替処理が途中で終了していた。ヘッダーの状態取得で必要時に `getComponentPropertyGroups()` を呼ぶ修正を追加した。
- **別途の懸念:** `ArtifactPropertyWidget` のグループ取得も通常グループだけを使い、Components 専用グループを取得する呼び出しが検索上見つからない。また Cloner の count 等の setter は存在するが、現行 Components グループに Cloner 設定群の登録は見つからない。設定面の到達性への影響は実機未検証。
- **価値／次の確認:** Components 専用面でだけ専用グループを取得する導線と Cloner 設定登録を確認する。通常 Properties へコンポーネント項目を再露出させず、変更は別途の依頼範囲で行う。ビルド・実機は未実施。



## 2026-10-02 — 同期 EventBus からの Qt UI 更新は GUI thread に送る

- **関連:** `ArtifactCore/include/Event/EventBus.ixx` / `src/Event/EventBus.cppm`、`ArtifactCore/src/Application/ArtifactAppSettings.cppm`、`Artifact/src/AppMain.cppm`。
- **確認できた事実:** `EventBus::publish()` は subscriber callback を同期実行し、発行スレッドを切り替えない。`ArtifactAppSettings` の setter は同期 `publish(AppSettingsChangedEvent{})` を呼び、`AppMain` の subscriber は `QApplication::setFont()`、widget style の再適用、MainWindow の設定更新を行っていた。`QApplication::setFont()` は widget の FontChange 処理を同期的に進め、`QComboBox` / `QAbstractScrollArea` の再レイアウトに入る。
- **対応:** `AppMain` の設定イベント callback から UI 更新一式を `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` で QApplication の thread へ配送する。フォントが同じ場合の再設定を省き、全 widget の style 再適用では `QPointer` で破棄済み widget を再確認する。
- **価値または懸念:** 非 GUI thread から publish された場合に Qt widget API を直接呼ぶ経路を塞ぎ、無関係な設定変更での同期 FontChange / combo relayout も避ける。提示された Qt stack にアプリ側の発火元 frame と例外情報は含まれておらず、これがクラッシュ原因そのものかは未検証。設定イベントの他 subscriber も同期実行されるため、UI を触る subscriber の thread-affinity は別途確認対象。
- **次に確認すべきこと:** ビルド／実行を許可された後、設定変更を GUI thread と worker thread の両方から発行し、フォント更新・コンボ表示中・テーマ変更時にクラッシュせず MainWindow と各 dock の表示が更新されることを確認する。



## 2026-10-02 — ProxyWorker の並列化と native AAC 再エンコード

- **関連:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`（`ProxyWorkerSlot` / `pollProxyWorkerSlot` / `updateAggregateProxyProgress` / `proxyWorkerSlotLimit` / `processNextProxyJob` / `cancelProxyQueue` / `~Impl`）、`Artifact/src/Worker/ArtifactProxyWorker.cpp`（`generateWithFfmpegNative` と `main` の backend 選択）。
- **確認できた事実（確定）:** プロキシ生成は 1 ジョブ = 1 プロセス・逐次 1 本だった。`std::deque<ProxyJob> proxyJobs_` と単一の `activeProxyWorker_` を持ち、`processNextProxyJob()` が「走査中の worker が非 null なら即 return」で新ジョブを起動できなかった。`ArtifactProxyWorker.cpp` の native 経路は映像のみを再エンコードし、音声は `avcodec_parameters_copy` による stream copy のみだった。そのため `audioReencode=true` の-job は `main` で native を丸ごと回避し ffmpeg CLI にフォールバックしており（`ArtifactProxyWorker.cpp:876-879`）、HW 映像アクセラレーションの利得を完全に失っていた。native の stream copy は `avformat_query_codec` が container 非対応な codec に対し `AVERROR_MUXER_NOT_FOUND` で失敗する（`:294-297`）。
- **対応:** (1) `Artifact` 側のみで並列化。単一メンバ群を `ProxyWorkerSlot` 構造体（process / job / 各パス / 出力バッファ / 進捗 / 失敗理由）と `std::deque<ProxyWorkerSlot>` に置き換え、`pollProxyWorkerSlot()` をスロット単位の polling に、`updateAggregateProxyProgress()` を busy スロット平均の集計に進化させた。同時実行数は `Proxy/ParallelJobs`（0 なら `QThread::idealThreadCount()`）で決まる。並列化により破棄時クリーンアップ、cancel、重複ジョブ判定がすべて複数 process を前提にできるようになった。(2) native 経路に AAC 再エンコードを追加。`swr`（libswresample）＋ `av_audio_fifo` でサンプルレート／チャンネルレイアウト差を吸収しつつ FIFO に貯め、エンコーダ固定長フレームを満杯にしてから送る構造にした。末尾は `flushAudioTail()` が無音で詰める。`swr_alloc_set_opts2` は FFmpeg 世代で署名が異なるため使わず、`swr_alloc()` ＋ `av_opt_set_chlayout` / `av_opt_set_int` / `av_opt_set_sample_fmt` のオプション API に統一した。`audioReencode` は native でも受理されるようになり、backend 選択の ffmpeg CLI フォールバック分岐を削除した。Media Foundation は依然として `audioReencode` を明示拒否する（変更なし、実質 re-encode 不可）。
- **価値または懸念:** (1) の主眼は wall-clock 短縮であり、worker バイナリも `protocolVersion` も触っていないためホスト単体の変更で収束した。一方 `Proxy/ParallelJobs` の既定値が「機械コア数」である点は要注意で、HW エンコーダ（NVDEC/AMF/QSV）が GPU 競合を起こす可能性があり、正しい上限は計測なしには決められない（未検証）。`ArtifactProxyWorker` の CMake 側で `swresample` は既に link 済みだった（`Artifact/CMakeLists.txt:3317`）ため CMake 変更は不要だった。(2) はサンプルレート変換を含むため CPU コストが stream copy より上がるが、HW 映像と并存する形になったのが改善点。**未検証:** ビルド／実行は許可されていないため、MSVC の C++ コンパイル通過（特に FFmpeg ヘッダの実バージョンと `AVCodecContext::ch_layout` フィールドの世代互換性）、A/V 同期の実挙動、`swr` 経由の音声出力サンプル品質はいずれも未確認。
- **次に確認すべきこと:** ビルド許可後、(a) `ArtifactProxyWorker` と `Artifact` の両方が通ることを確認、(b) 音声 codec が MP4 非対応な素材（PCM / DTS など）で native 経路が AAC 出力に成功することを確認し、元 native が `AVERROR_MUXER_NOT_FOUND` で落ちていた挙動と比較する、(c) `Proxy/ParallelJobs` を 2 / 4 / 8 と振って wall-clock と GPU 使用率を実測する、(d) 音声付き素材を再生して lip-sync 崩れが無いか確認する。`av_opt_set_*` の戻り値は検査していないので、resampler 初期化失敗時の挙動は別途確認が要る。



## 2026-10-02 — Glyph quad を triangle strip にまとめる際の接続規則

- **関連:** `Artifact/src/Render/ArtifactTextGlyphSubmitter.cppm`、`Artifact/docs/planned/MILESTONE_GPU_TEXT_ANIMATOR_TRANSFORM_BUFFER_2026-08-13.md`。
- **確認できた事実:** 分離 glyph submitter の PSO は triangle strip を使う。各 glyph の quad を単純連結すると quad 間に意図しない三角形ができるため、縮退頂点を挿入して1 drawへまとめる試作を追加した。
- **価値または懸念（未検証）:** glyphごとの Draw 呼出しを減らせる可能性がある。一方、縮退三角形の接続、alpha blending時の順序、GPU driver間の出力一致は実機確認していない。製品 submitter への統合も別途必要。
- **次に確認すべきこと:** standalone smoke を許可された環境で実行し、複数 glyph／重なり／回転 glyph の画像差分と draw-call 数を測定する。



## 2026-10-02 — Sequence relocation は relative 配列欠落時も元 frame index を維持する

- **関連:** `Artifact/src/Project/ArtifactProjectImporter.cppm` の layer／footage sequence relative-path 復元。
- **確認できた事実:** 復元ループが `sequencePathsRelative` の要素数だけを走査していたため、同じJSONの `sequencePaths` がより長い場合、relative path未記録の後続frameが再構築配列から落ち得た。
- **対応:** 両配列の最大長を走査し、relative slot が無い要素には元の absolute path を渡すようにした。余分なrelative slotは元pathが空の候補として従来どおり解決する。
- **価値または懸念（未検証）:** 不完全な旧／外部JSONでもsequence frame数とindexを維持できる。保存・再読込のruntime round-tripは未確認。
- **次に確認すべきこと:** relative配列が短い／長い／同長で、欠番と移動済みframeを含むprojectを往復し、frame indexとmissing状態を確認する。



## 2026-10-02 — Relink Undo はmissing pathの復元を許容する

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`、Asset BrowserのUndo／rollback、`WorkspaceAutomation`。
- **確認できた事実:** relink serviceは新パスの存在を要求していたため、既に欠落している素材を有効な候補へ再リンクした後、Undoで元の欠落パスへ戻す処理が失敗する。batch rollbackにも同じ条件が影響する。
- **対応:** 通常操作ではmissing targetを拒否する既定値を保ち、Undoとrollback経路のみ明示フラグでmissing file／sequence frameの復元を許容する。既存対象の型・sequence frameの読み取り可能性検証は維持。
- **価値または懸念（未検証）:** AssetDatabase、footage、参照レイヤーの状態をUndoで元に戻しやすくする。runtimeのUndo/Redo・sequence欠落動作は確認していない。
- **次に確認すべきこと:** 素材移動→relink→Undo→Redo、および一部sequence frameが欠落した状態でのbatch失敗rollbackをruntime確認する。



## 2026-10-02 — Glyph Submitter試作を製品ホットパスへ直結しない

- **関連:** `Artifact/src/Render/ArtifactTextGlyphSubmitter.cppm`、`Artifact/CMakeLists.txt`、`Artifact/docs/planned/MILESTONE_GPU_TEXT_ANIMATOR_TRANSFORM_BUFFER_2026-08-13.md`、`docs/technical/HOT_PATH_RULES.md`。
- **確認できた事実:** `ArtifactTextGlyphSubmitterRuntime` は `EXCLUDE_FROM_ALL` の分離ターゲットであり、通常の `DiligentImmediateSubmitter` から呼び出されていない。`submit()` は呼び出しごとに動的な `std::vector<SubmitVertex>` を組み立て、GlyphAtlas画像を `QImage` としてGPU textureへ転送する。
- **気づき:** 連続Glyph quadの単一draw化は有用な試作だが、現在の実装を製品の毎フレーム描画へそのまま接続すると、ホットパスの割当規則とQImage境界の制約に抵触する。製品統合の前に、固定容量／呼び出し側scratchを含む作業領域設計とGPU atlas uploadの既存境界を確認する必要がある。
- **価値／懸念:** 実験用draw-call削減と製品の安全なTransform Buffer移行を混同せず、フレームごとの確保や画像転送を新しい恒常経路へ持ち込むことを避けられる。代替設計の性能・互換性は未検証。
- **次に確認すべきこと:** 製品submitterの現在のGlyph vertex／atlas upload所有者を追跡し、bounded scratchまたは再利用可能なGPU bufferでパケット更新を分離できるかを設計してから統合する。



## 2026-10-02 — sequence preflightは代表sourcePathをframeとして診断する

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm` の `appendMissingAssetDiagnostics()`、`docs/planned/MILESTONE_SMART_FALLBACKS_2026-06-07.md`。
- **確認できた事実:** preflight は重複診断防止のため `sourcePath` を既処理集合へ先行登録していた。代表pathが `image.sequencePaths` の欠落frameと一致すると、そのframeの専用diagnosticが重複扱いで飛ばされ、通常のmissing-file診断だけが残る。
- **追加で確認した事実:** 通常ファイルの代表path確認が `exists()` だけだったため、同名ディレクトリを有効な素材と誤認する。sequence frame側は既に `isFile()` を要求していた。
- **対応:** 代表pathがsequence listに含まれるかを判定し、sequenceのframe列挙に診断を委ねる。listに含まれない代表pathだけを通常missing-file診断にし、代表pathと各frameの両方で存在と通常ファイル種別を確認する。重複sequence pathはframe列内で引き続き一度だけ診断する。
- **価値／懸念（未検証）:** 先頭frameを含む欠落frameをすべてsequence単位で正しく示せる。runtimeでの診断表示は未確認。
- **次に確認すべきこと:** sourcePathが先頭frame／sequence外path／既存frameの場合と、重複frame pathを含むsequenceの各preflight結果を確認する。



## 2026-10-02 — GPU glyph packetはfloat化の前後で有限値を確認する

- **関連:** `Artifact/src/Render/ArtifactTextGlyphSubmitter.cppm`、`Artifact/docs/planned/MILESTONE_GPU_TEXT_ANIMATOR_TRANSFORM_BUFFER_2026-08-13.md`。
- **確認できた事実:** 分離submitterはglyph位置・scale・rotation・opacityを検査せず頂点へ変換し、NaN／Infが混ざると頂点bufferへ非有限値が入り得た。global opacityや色、計算後UVも同様に無検査だった。
- **対応:** パケット生成前に座標・各offset値、scale、rotation、opacityを検証し、float範囲外／非有限座標と負scaleのglyphを除外する。opacity／色および最終座標・UV・alphaも有限値であることを要求する。
- **価値／懸念（未検証）:** 不正なanimator値が実験GPU経路のbufferへ伝播するのを防ぐ。これは分離submitter限定の防御で、製品renderer側の契約検証やGPU描画結果は未確認。
- **次に確認すべきこと:** 不正値を含む入力のSoftware/GPU双方の扱いを比較し、同じglyphを落とすか、上流で値を正規化するかを決める。製品経路にも移す際はその経路のbounded buffer設計と合わせる。



## 2026-10-02 — Collider handleの編集幾何はCore DTOでUIから分離する

- **関連:** `ArtifactCore/include/Physics/Collider2DEdit.ixx`、`docs/planned/MILESTONE_2D_COLLIDER_VIEWPORT_EDIT_2026-09-19.md`。
- **確認できた事実:** Colliderの値DTOと一括適用入口は存在したが、Box/Circle handle位置、hit test、drag deltaから authored valueを更新する処理は無かった。
- **対応:** DTOへlocal-space handle位置、nearest hit test、offset／box辺・角／circle radiusのdelta編集関数を追加した。反対側固定、最小寸法0.001、非有限値とfloat範囲外の拒否を計算モデルに含める。
- **追加で確認した事実／修正:** 既存の物理解決ではwidth／height／radiusの0がAuto Boundsへのフォールバックを意味する。0まで縮めると見た目／保存値が崩れるため、BoxとCircleの寸法下限を0.001に揃えた。
- **価値／懸念（未検証）:** ViewportとUndoの状態機械から独立して編集幾何を使える。まだどのUI経路からも呼ばれず、physics同期回数・Undo・描画結果は未検証。
- **次に確認すべきこと:** 既存modal gizmo経路へ接続し、ドラッグ中DTO previewと確定時一度だけのsetter／Undoを行う。Parent transform、locked/hidden、Auto Bounds、Polygonについて編集を拒否する。



## 2026-10-02 — Collider編集DTOは元boundsと既存値も検証する

- **関連:** `ArtifactCore/include/Physics/Collider2DEdit.ixx`、`docs/planned/MILESTONE_2D_COLLIDER_VIEWPORT_EDIT_2026-09-19.md`。
- **確認できた事実:** handle／drag処理はpointer deltaと算出後の値を検証していたが、入力DTOの`sourceBounds`や`offset`、既存寸法・半径にNaN／Infが入った場合の入口検証は無かった。負のsource boundsも有効な幾何とは扱えない。
- **対応:** `hasFiniteGeometry()`を追加し、handle位置・hit test・dragの各入口でbounds・offset・寸法・半径を検査する。source boundsの負寸法も拒否する。
- **価値／懸念（未検証）:** 不正な復元値や呼び出し側の破損値がhandle位置へ伝播したり、offset操作で有効化されたりするのを防ぐ。実際の永続値がこの不正値を取り得るか、呼出し経路での拒否が期待動作かはruntime未確認。
- **次に確認すべきこと:** 永続値の読み込み境界とViewport側のエラー／編集拒否表現を確認する。UI・Undo接続後にinvalid DTOで編集開始されないことを確認する。
- **追加対応:** QPointF上で有限でもrenderer／物理側float表現の範囲を超えるhandle座標は後段でoverflowし得る。また既存setterはoffsetを±100000、寸法／半径を0〜100000に制限する。DTO入口とdrag結果をこの永続値域に一致させ、保存確定時のclampでpreview形状が変わるケースを避ける。



## 2026-10-02 — Asset Browserはdirectoryを素材ファイル扱いしない

- **関連:** `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`、`docs/planned/MILESTONE_ASSET_BROWSER_OPTIMIZATION_2026-03-29.md`。
- **確認できた事実:** browserのmissing判定は`QFileInfo::exists()`のみだったため、既存directoryがsource／sequence frame pathになった場合はMissingでないと表示された。Render Queue preflightは既に通常ファイル種別まで要求する。
- **対応:** Asset Browserのpath status判定を`QFileInfo::isFile()`へ変更し、同名directoryをmissingとして扱う。
- **価値／懸念（未検証）:** Asset BrowserとRender Queueの基本的なfile-type判定が一致する。特殊なvirtual/network sourceがAsset Browserの通常filesystem一覧へ混在するかは未確認。
- **次に確認すべきこと:** missing／sequence statusのUIで既存file、missing file、同名directoryを比較する。並列filteringとは独立した正確性修正。
- **追加対応:** `isUnusedAssetPath()` のcanonical path ternaryは`canonicalFilePath()`を条件・結果で二度呼んでいた。`QFileInfo`とresolved pathを一度ずつ保持し、逐次一覧走査の重複filesystem問い合わせを除去した。性能改善幅は未計測。



## 2026-10-02 — Find Referencesはsource path fieldだけを照合する

- **関連:** `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`、`docs/planned/MILESTONE_ASSET_BROWSER_RELINK_WORKFLOW_2026-06-28.md`。
- **確認できた事実:** Find Referencesはlayer JSONの全ての文字列値に対してcanonical／absolute path化を行い、素材path集合と照合していた。JSON内の任意テキストがfilesystem pathと偶然一致した場合も参照と誤認し得る。
- **対応:** `sourcePath`、`*.sourcePath`、`sequencePaths`、`*.sequencePaths`、`filePath`の値だけ（sequence array要素も含む）をsource参照として比較する。
- **価値／懸念（未検証）:** 任意のユーザー文字列をpathとしてfilesystem正規化しない。relative path fieldはproject rootを使った解決が別途必要なため対象外。参照dialogの実素材確認は未実施。
- **次に確認すべきこと:** static image、sequence、nested source groupの既知キーで参照を検出し、同じパス文字列を持つtag／label／expressionでは誤検出しないことを確認する。



## 2026-10-02 — Find Referencesの結果から所有layerへ移動できる

- **関連:** `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`、`ArtifactProjectService`。
- **確認できた事実:** Project Serviceにはcomposition切替とlayer選択APIがあり、既存のProblem Viewでも順番に呼び出して参照先へ移動している。
- **対応:** Asset BrowserのFind References結果を選択式にし、compositionを切り替えてからlayerを選択する。
- **価値／懸念（未検証）:** 結果を発見するだけだった導線から、編集対象へ移動できる。実行時の切替後選択保持は未確認。
- **次に確認すべきこと:** 複数compositionに参照がある場合の選択、composition切替失敗時の表示、選択対象のハイライトをruntimeで確認する。



## 2026-10-02 — Shaping順序mapはglyph ordinalの逆写像にする

- **関連:** `ArtifactCore/src/Text/TextShapingBackend.cppm`、`ArtifactCore/include/Text/TextShapingBackend.ixx`、複雑文字 shaping。
- **確認できた事実:** shaping結果の`logicalToVisual`と`visualToLogical`へ同じ値を追加しており、逆写像ではなかった。また配列長はglyph countなのに、変換処理ではcodepoint ordinalをglyph ordinalとして扱っていた。合字や複数glyph clusterでは両数が一致しない。
- **対応:** source cluster indexの安定ソートで論理glyph順を作り、backend出力配列ordinalとの相互逆写像を構築する。公開型コメントに配列の単位を明記する。
- **価値／懸念（未検証）:** マッピングの相互整合性とglyph単位の契約を保証できる。mixed-bidi段落、異体glyph cluster、および呼び出し側がこの配列を使う動作は実行確認していない。
- **次に確認すべきこと:** LTR/RTL/mixed-bidi、ligature、1 cluster複数glyphで両配列が完全な逆permutationになること、およびQt backendとの意味差をruntimeで確認する。
- **追加対応:** `TextClusterSpan.visualStart/visualLength`もlogical-order grapheme連番のままで、mixed-bidiとmulti-codepoint graphemeを表現できなかった。ICU bidiのlogical-to-visual codepoint mapからspanを作り、公開コメントにcodepoint ordinalでありglyph ordinalではないことを記載した。Indic conjunctのglyph cluster mapping自体は未解決。



## 2026-10-02 — Asset Browserのscan中はentry metadataを再利用する

- **関連:** `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`、Asset Browserのfilter／sequence走査。
- **確認できた事実:** `applyFilters()` は一覧entryに対してdirectory判定とabsolute path構築を、folder分類・sequence収集・standalone行作成の各走査で繰り返していた。
- **対応:** 1回のfilter scan内にentry名→`QFileInfo`／directory判定の小さなlookupを作り、各passで共有する。
- **価値／懸念（未検証）:** 1entryにつき`isDir()`をscan開始時に一度だけ呼ぶ。mapによる追加メモリの一方で、削減できたOS metadata I/Oと全体性能は未計測。並列化や非同期化はこの変更には含まない。
- **次に確認すべきこと:** 同じ種類／検索／タグ／status filterの結果一致と、大規模directoryでのUI応答時間・filesystem probe回数をruntime計測する。



## 2026-10-02 — Material presetのUI間定義を共通化する

- **関連:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`、`ArtifactCore/src/Material/Material.cppm`、`Artifact/include/AI/MaterialAutomation.ixx`、3D Material Browser候補。
- **確認できた事実:** Layer PanelはMetalをmetallic 0.9／roughness 0.24、Glassをroughness 0.08／transmission 0.82／IOR 1.5で適用する。Core factoryはMetal 1.0／0.2、Glass roughness 0／opacity 0.2。AI AutomationはMetal 1.0／0.2、Glass 0.05／specular alpha 220として保持する。Plastic roughnessもLayer Panel 0.3、Core factory 0.4で異なる。
- **対応:** 旧汎用factoryを変更せずに`Material::makeStudioPreset()`を追加し、Layer PanelとAI Automationの4 preset適用を共通化した。Automationはspecular／transmission／IORも保存済みlayer propertyへ渡し、presetに無い emission color は上書きしない。
- **対応:** Layer Panelの3D Material menuにpreset browser pickerを追加し、共有factoryから値ラベルを作って選択適用できるようにした。適用は既存property／Undo経路を使い、新規signal-slot接続は作らない。
- **価値／懸念（未検証）:** preset値を参照・選択しやすくしたが、これは永続Material assetの作成／一覧／編集を持つ完全なBrowserではない。runtimeのUndo／property反映／renderer shadingは未確認。AI Automationに同名のユーザー定義materialが既に存在する場合は、従来どおりその保存値を維持する。
- **次に確認すべきこと:** 保存可能Material assetの所有モデルとproject保存場所を設計し、preset適用のruntime・Undo・D3D12/Vulkan shadingも確認する。



## 2026-10-02 — MCP debug.*ツールの書き込み分岐は意図した2本だけに絞る

- **関連:** `ArtifactCore/include/AI/McpBridge.ixx`、`ArtifactCore/include/AI/McpTransport.ixx`、`Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`。
- **確認できた事実:** `debug.gpuMemory` と `debug.getRig` は外側の受理一覧にはあったが、読み取り専用の snapshot 分岐の条件に抜けていたため、`debug.state` 以外の書き込み段へ落ちて `paused=false`（`mcp.resume`）を上書きし、state ファイルをTruncate で書き換えていた。読み取り操作が协商中の pseudo-breakpoint を消す実バグ。別系統の `McpTransport::callTool` は `params.tool` だけを送っていたのに対し、受け側の `McpBridge` は `params.name` しか読まず、既定設定（自己ホスト）のまま Call Tool が必ず `Invalid tool call payload` になっていた。
- **対応:** 読み取り専用分岐へ2ツールを追加し、最後の書き込み段が pause／resume だけである旨をコメントで明示した。`callTool` は class.method から `name` を組み立てて送るようにし、`tool`／`class`／`method`／`arguments` は互換維持のため并列送信する。UI は選択中のツール名をメンバー `selectedMcpToolName_` に保持して `name` として送出する。debug 系は `arguments` をオブジェクト、ToolBridge 系は配列を前提とするのでツール名で受け側の期待形式を分ける。
- **価値／懸念（未検証）:** Call Tool が既定設定で通るようになり、読み取りツールが session 状態を壊さなくなった。ただし実行時確認はしていない（ビルド／テストは依頼されていないため未実施）。外部の標準 MCP クライアントとの相互運用、`debug.getTools` が毎回 `capabilityList()` を再計算するO(n²)傾向、`responseBuffer_` が代入のみで一度も読まれない点、`debug.addDataBreakpoint` の kind が Node 側enumと未検証で乖離する点は未修正。
- **次に確認すべきこと:** 標準 MCP クライアントからの `tools/list`／`tools/call` 実行、debug 系と ToolBridge 系の双方で引数が届くこと、pause 状態維持をruntimeで確認する。



## 2026-10-02 — 2つのMCPサーバは併存させ、ワイヤ形式だけを揃える

- **関連:** `ArtifactCore/include/AI/McpBridge.ixx`、`tools/debug-mcp-server/`、MCP 実装全体。
- **確認できた事実:** Artifact には C++ の `McpBridge` と Node の `tools/debug-mcp-server` の2つの MCP サーバが存在する。役割は README が「Node 版は file-backed session の互換ハーネス、C++ 版が主実装」と明言しており二重実装は意図である。つまり統合すべき欠陥ではない。一方、ワイヤ形式だけがずれていた。C++ の `capabilities.tools` はツール配列の直置きで `listChanged` がなく、ツールエントリは独自 `parameters` 配列を持っていたのに対し、Node 版は 2024-11-05 準拠（`capabilities.tools.listChanged` と JSON Schema `inputSchema`）。この形式差があるため標準 MCP クライアントは C++ 側と相互運用できなかった。state 書き込みも Node が tmp+rename、AppMain が `QSaveFile`、C++ `McpBridge` のみ素の `QFile`+Truncate で、C++ MCP サーバと poller が同時に触ると JSON 破損し breakpoint condition が黙って消えた。加えて state パスの解決（`ARTIFACT_DEBUG_MCP_STATE_FILE` と既定パス）が `McpBridge` 内で4箇所に重複していた。
- **対応:** ツール名の namespace はそのままに、形式だけを揃えた。`McpBridge` に `toInputSchema()`（既存 `parameters` 配列→JSON Schema 変換）、`toMcpTools()`、`mcpCapabilities()` を追加し、`initialize`／`tools/list`／`debug.getTools` を MCP 準拠出力へ変更。内部利用者（`ToolBridge::toolSchemaJson()` 経由の UI）は `capabilityList()` のまま据え置き、出力だけを分けることで挙動を変えていない。state 読み書きを `resolveStatePath()`／`readStateFile()`／`writeStateFile()` に集約し、書き込みを `QSaveFile` 化。Node 版は既に atomic 実装済み/read 時の空ファイルフォールバックも持つので変更不要だった。あわせて `debug.memory.*` と `debug.stress.run` の未宣言パラメータ（M3）も handler の実読引数に合わせて schema へ足した。README へ2者の関係表と MCP クライアントへの手動登録手順を追記。
- **価値／懸念（未検証）:** 標準 MCP クライアントが C++ サーバとも相互運用できるようになり、state の JSON 破損による breakpoint 損失の経路を塞いだ。ツール名の2 namespace は非互換のままなので、1クライアントから両方を使うなら設定を分ける必要がある（README に記載）。C++ `McpBridge` のツール名から Node 版への変換は行っておらず、相互変換は未検討。実行時確認はしていない（ビルド／テストは依頼されていないため未実施）。残る既知差分は `debug.addDataBreakpoint` の kind が Node 側 enum と乖離、`responseBuffer_` が未読、`debug.trace` がスタブ。
- **次に確認すべきこと:** 標準 MCP クライアントから C++ サーバの `tools/list` が inputSchema 付きで取れるか、MCP サーバと AppMain poller を同時に動かして state が壊れないか、Node 版と C++ 版を同じクライアントに同時登録して干渉しないかを確認する。



## 2026-10-02 — テキストレイヤー Gizmo が反応しない: Text ツールの押下横取りとドラッグ判定漏れ

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`handleMousePress` の Text ツール分岐、`isGizmoDragActive`）、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`（`CompositionViewport::isSpatialGizmoDragging`）、`Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`。
- **確認できた事実（静的読み取り、ビルド・実機未確認）:** (1) `handleMousePress` の Text ツール分岐（`:27587`）は 2D ギズモのヒットテスト（`:29445`）より前に無条件 return し、release で `createTextLayerAtCanvas`（`:33008`）を呼ぶ。そのため Text ツール中はギズモが描画されていても押下が必ず「新規テキスト作成」に流れていた。(2) `CompositionViewport::isSpatialGizmoDragging()`（`ArtifactCompositionEditor.cppm:8238`）が `controller_->gizmo()` と `gizmo3D()` しか見ておらず、テキスト選択時に束縛される `textGizmo_`（`sync2DGizmosForLayer` が `gizmo_->setLayer(nullptr)` にして束縛する `:16400`）と `contentGizmo_` を認識しなかった。press で SetCapture/grabMouse せず、move 中の連続更新もスキップしていた。
- **対応:** (1) `CompositionRenderController::isGizmoDragActive()` を追加し、`gizmo_` / `gizmo3D_` / `textGizmo_` / `contentGizmo_` の `isDragging()` を集約。ビューポートの `isSpatialGizmoDragging()` はこれへ委譲。(2) Text ツール分岐で、選択中テキストレイヤーのギズモに当たる押下は `textGizmo_->handleMousePress` へ渡し、当たらなければ従来どおり新規テキスト候補を立てる（ユーザー選択「ギズモ操作を優先」）。
- **価値または懸念（未検証）:** Text ツール中でもギズモのハンドルが操作可能になり、テキスト／コンテンツギズモのドラッグが他ギズモと同じくマウスキャプチャ・後始末の対象になる。副作用として、Text ツールで既存テキスト本体をクリックしても点テキストの新規作成はできなくなる（ギズモの Offset が優先）。`trackerGizmo_` は従来どおり対象外。ビルド・実機は未実施（AGENTS.md 制約）。
- **次に確認すべきこと:** Selection ツールと Text ツールの双方で移動(Offset)／回転／アンカー／ボックスリサイズがドラッグでき、カーソルがビューポート外に出ても継続すること。何も無い場所のクリック／ドラッグで新規テキストが作られること。ContentGizmo（画像／ソリッドのコンテンツ編集）の押下・キャプチャに回帰がないこと。



## 2026-10-02 — 動画レンダーが0バイトで終わる/0%で固まる: HWエンコーダ未検証とプロデューサ未解放

- **関連:** `Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`（`ffmpegExeSupportsEncoder`、`PipeFFmpegExeBackend::open`）、`Artifact/src/Render/ArtifactRenderQueueService.cppm`（`processFramesForJob`、`startAllJobs` のワーカー本体）、`ArtifactCore/src/Image/FFmpegEncoder.cppm`（未修正・別リポジトリ）、実行ログ `%APPDATA%\Logs\artifact.log`。
- **確認できた事実（ログ＋コード読み取り、ビルド・実機未確認）:** 動画ジョブ（既定 `encoderBackend=auto`）が pipe-hw (NVENC) を選び、同梱 ffmpeg は NVENC API 13.1 を要求するが実ドライバは 13.0 のため `h264_nvenc` を開けず ffmpeg が exit -40 で死亡。エンコーダ選択の `ffmpegExeSupportsEncoder` は `ffmpeg -encoders` の一覧に名前があるかしか見ないため `open` が「成功」してしまい、ソフトウェアへフォールバックしなかった。最初の `addFrame` がタイムアウトしてジョブ失敗し、ffmpeg が作成済みの 0 バイト出力が残った。さらに消費者ループが encode 失敗で `break` しても、出力バッファ満杯で待機中のプロデューサ（`renderOneFrame` 内 wait の解除条件が `shutdownRequested_` のみ）を起こさないため `renderWorkers` の join が返らず、ジョブが 0% のままアプリ終了まで固まった。ログに `[h264_nvenc] Driver does not support the required nvenc API version. Required: 13.1 Found: 13.0`、`encoder rejected frame ... Timed out while writing frame 2 to ffmpeg.exe`、`[Encode][Pipe] finalize failed ... exitCode=-40` を確認。
- **対応（親リポジトリ `Artifact` のみ変更）:** (1) `ffmpegExeCanOpenEncoder` を追加し、1 フレームの rawvideo 試しエンコードで HW エンコーダが実際に開けるか検証。失敗時は `open` を false にして Auto 経路がソフトウェア（libx264）へフォールバックできるようにした。(2) `processFramesForJob` に `producerCancel` を追加し、消費者ループ終了後に解放＋`notify_all`。`renderOneFrame` の待機条件・早期 return とプロデューサの while 条件へ組み込んだ。(3) ジョブ失敗時、`videoRenderPath`／`outputPath` が 0 バイトなら削除。
- **価値／懸念（未検証）:** 非対応 HW エンコーダでもソフトウェアで描画が完了し、0 バイトファイルを残さず、失敗時もジョブが固まらず終了する見込み。ビルド・実機は未実施（AGENTS.md 制約）。試しエンコードは HW ジョブ開始時に約 1 フレーム分の ffmpeg 起動コストが増える（未計測）。Vulkan 経路の probe は `-init_hw_device`/`-filter_hw_device` を渡すが実機確認なし。
- **未修正（記録のみ・別リポジトリ）:** `ArtifactCore/src/Image/FFmpegEncoder.cppm` の `open()` は `avio_open` 後に `avformat_write_header` 等で失敗すると `isOpen_=false` のまま false を返し、`close()` が冒頭 `if(!isOpen_) return;` で早期 return するため `avio_closep` されず 0 バイトのファイルハンドルが残る。Core 側の修正は子リポジトリのため今回は未着手。
- **次に確認すべきこと:** (a) 実機でハードウェア非対応環境の動画レンダーが libx264 にフォールバックし再生可能な mp4 が出ること。(b) HW 対応環境では NVENC が使われ続けること。(c) レンダー失敗時に 0 バイトファイルが残らずジョブが失敗表示で速やかに終わること。(d) `pipe`／`native` を明示選択した場合も同様であること。



## 2026-10-01 — Text shaping の公開フォントAPIとUTF-16境界

- **関連:** `ArtifactCore/include/Font/FreeFont.ixx`、`ArtifactCore/src/Text/TextShapingBackend.cppm`。
- **確認できた事実:** `FontManager::makeFont` はTextShapingBackendとGlyphLayoutから呼ばれている一方、FontManager内ではprivate宣言範囲にあった。Windows ICUの`UChar`は`char16_t`で、Qt `QString::utf16()`は`ushort*`を返す。HarfBuzzのbuffer language setterは文字列でなくintern済み`hb_language_t`を受け取る。
- **対応:** `makeFont`を公開側へ配置し、ICU境界では`std::u16string`のデータを渡し、HarfBuzzでは`hb_language_from_string`を通して言語を設定した。
- **価値または懸念（未検証）:** API型の境界を明示し、現在のMSVCコンパイルエラーを解消した。UTF-16の一時コピーに伴う割り当て頻度は実測していない。
- **次に確認すべきこと:** アプリのテキスト描画・複雑文字スクリプトで整形結果を実機確認し、必要ならUTF-16変換の割り当てを計測する。



## 2026-10-01 — ショートカットID配列はenumの永続ordinal順を保つ

- **関連:** `ArtifactCore/include/UI/ShortcutBindings.ixx`、`ArtifactCore/src/UI/ShortcutBindings.cppm`。
- **確認できた事実:** `ProjectRevealInExplorer` は既存ordinalをずらさないよう `ShortcutId::Count` 直前へ追加されたが、`allShortcutIds()` にはカテゴリ位置と配列末尾の両方に登録され、固定長配列の初期化子が1つ超過していた。既定キー`R`は回転操作の予約キーと重複していた。
- **対応:** 永続ordinal順の末尾登録だけを残し、Project Revealは既定キーなしとした。
- **価値または懸念（未検証）:** 設定一覧と永続ordinalの対応を保ち、基本変換キーとの衝突を避ける。プロジェクトビュー内のカスタム割り当て動作は実機未確認。
- **次に確認すべきこと:** ショートカット設定一覧で行順・保存復元・ユーザー割り当てを確認する。



## 2026-10-01 — リビールマップによるブラシストローク表示アニメーション案

- **実装（2026-10-01）:** Artifactに共通Reveal設定・Properties・保存、Linear/Radial/Noise/Brush/Custom、Timing/Support生成、32slotのGPU cache、GPU ViewportとRender QueueのCoverage適用を追加した。進行度はOpacityから独立し、GPU resident境界ではpremultiplied RGBA全成分を乗算する。Sprite出力境界は既存PSOのSRC_ALPHAに合わせてstraightへ明示変換する。初版の契約は仕様書末尾に記録した。
- **確認できた事実と次の確認:** `OffscreenCompositionRenderer` はlayerのdrawを直接呼ぶ別経路であり、今回の合成境界を通らない。Source preview等の用途ごとにRevealを適用すべきかを将来確認する。CPU生成mapは2 MiB/所有レイヤー、GPU cacheは64 MiB/pipelineであり、多数レイヤー・複数workerでの合計メモリと資源解放は未実測。ブラシ生成は編集確定時の同期処理で、最大256点での編集確定遅延を計測すること。新しい実装は未ビルド・未実機検証。

- **仕様化（2026-10-01）:** ユーザー依頼により `docs/technical/REVEAL_COVERAGE_SPEC.md` と `docs/planned/MILESTONE_REVEAL_COVERAGE.md` を作成。Opacity独立、TimingとSupportの分離、端点保証、premultiplied RGBA全成分の乗算を提案契約として定義。Text/Shapeの空間Revealと厳密なTrim Pathsは別機能として計画し、実装開始前の経路確認項目を明示した。

- **関連:** ユーザー添付 `C:/Users/kukul/Downloads/ea56a8a0-42b2-4713-b39c-63868ac13c2a.webp`、レイヤーエフェクト／マスク描画経路（実装箇所未特定）。
- **確認できた事実（画像の説明）:** 画像はレイヤー全体の不透明度を下げるのでなく、ピクセルごとのリビール値と進行度 0〜1 を比較し、マップの値に応じてブラシストロークを順次見せる概念を示す。図示例では、事前生成する白黒マップの値を使って表示順を決め、softness で境界を調整する。マップ例としてストローク方向、中心から外側、ノイズ、複数ストローク、手描き線、粒子、カスタム形状が描かれている。
- **価値または懸念（未検証）:** 静止画・ペイント系の演出に使える可能性がある一方、これは画像中の提案であり、ArtifactStudioに同等機能を追加すべきという仕様決定ではない。画像の擬似コード（`smoothstep` による比較）や「事前生成なら再生時に軽い」という説明は、既存の画像／マスク表現、キャッシュ無効化、GPU経路、ホットパス制約への適合が未検証。アニメーションのプロパティ責務や Inspector 面も未決定。
- **次に確認すべきこと:** 実装を依頼された場合、既存の Gradient/Wipe 系エフェクト、マスク・ペイントデータの保持経路、GPU shader とプロパティ／キーフレーム API を調べ、リビールマップを永続データにする必要性と事前計算・キャッシュの境界を決める。当初は提案記録にとどめたが、後続のユーザー指示により上記の仕様化とソース実装へ進んだ。



## 2026-10-01 — OFX ホスト整合性監査（描画経路が構造的に成立していなかった）

- **確認できた事実（実ファイルで確認）:** OFX 実装は `Artifact/src/Effects/Ofx/ArtifactOfxHost.cppm` と `ArtifactOfxEffectImpl.cppm` の2ファイルに閉じており、`ArtifactCore` への依存はゼロ。監査時点で `clipGetImage` が clip 名に関わらず常に `srcPixelData` を返し、`dstPixelData` は設定されるのみで読み手が存在しなかった（デッドフィールド）。`imageEffectForClip` はロード済みプラグインの `descriptorState->clips` を走査していたが、`createRenderInstance` は新しい `ClipState` を `make_unique` するためアドレスが一致せず常に `nullptr` を返していた。両者を合わせると `clipGetImage` は常に `kOfxStatFailed` で、OFX レンダリングは実質的に機能していなかった。
- **ユーザー承認:** 上記を含む P0/P1/P2/P3 の全項目を1パスで修正する指示を受け実施した。
- **修正内容:** `ClipState` に `owner` と `clipName` を追加して clip→effect の逆引きを直接解決に変更。`clipGetImage` を clip 名で振り分け、`Output` は `dstPixelData` を返す。ホストに世代番号 `generation_` を導入し、Rescan 時に bump。効果側は `isGenerationCurrent()` で世代を検査し、古い場合は描画を安全に通過（バイパス）する。デストラクタで `EndSequenceRender` / `DestroyInstance` を呼ぶ。`pluginActionRender` に `EffectContext::compositionFrame / frameRate` から算出した時刻を渡す。`kOfxParamPropAnimates` と `SupportsCustom/String/Boolean/ChoiceAnimation` を実態（0）に合わせ、`GetClipPreferences` / `IsIdentity` を実装し、`MemorySuite` / `TimeLineSuite` を追加。グループ内パラメータの修飾名フォールバック、型判定の完全一致化（`RGBA` が `RGB` に部分一致して alpha を欠落させていたバグ）、2D/3D 全成分の受け渡し、Choice の整数化、`ofx.mix` の実ブレンド化を実施。
- **自己監査で発見した二つの実装バグ:** (1) `ofxMessageFunc` の前方宣言が匿名名前空間内、定義が匿名名前空間外にあり別エンティティになっていたためリンクエラー。定義を匿名名前空間へ移動。(2) `kOfxImageEffectPropStatusMessage` は OFX 仕様のキーだがベンダーヘッダ `include/ofx/` に存在せず未宣言識別子。`#include <ofx/ofxMessage.h>` を追加し、ローカルに `#ifndef` ガード付きで定義。
- **価値または懸念（未検証）:** 描画経路が初めて実際に機能する設計になった。ただし実ビルドと実プラグインでの動作確認は未実施（AGENTS.md 方針によりビルド未実行）。特に (a) 実プラグインでの CPU 描画が正しいか、(b) `GetClipPreferences` で `float RGBA` を提示した際プラグインがそれを受理するか、(c) Rescan 後に旧エフェクトが安全に通過するかの3点は実機確認が必要。非 Windows（macOS / Linux）でのプラグインロードは `scanBinary` が no-op のままである。キーフレーム API は全滅しており、ホストはアニメーション非対応と申告するため、プラグインがアニメーションを要求しないことが前提となる。
- **次に確認すべきこと:** (1) ビルドして実プラグイン（Boris FX / Sapphire / Neat Video 等）で動作確認 (2) 非 Windows でのプラグインロード実装（`scanBinary` の no-op 解消） (3) キーフレーム API の実装（`paramGetNumKeys` / `paramSetValueAtTime`）を検討 (4) 世代ガードの挙動を実機確認。



## 2026-10-01 — OFX キーフレーム・パラメータメタデータ・追加スイート実装

- **ユーザー承認:** 残存 feature の実装範囲を 4 項目（キーフレーム API / パラメータメタデータ完全対応 / Progress・Interact スイート / GPU テクスチャ共有）で multi-select 提示し、**全 4 項目**が選択された。GPU は仕様未確定のため残り 3 項目を先に実装した。
- **前提调查结果（実装前に実コードで確定）:** `AbstractProperty` にキーフレーム機構が既に完備しており（`AbstractProperty.cppm` の `addKeyFrame` / `removeKeyFrame` / `clearKeyFrames` / `interpolateValue` と `KeyFrame` 構造体）、毎フレーム評価の駆動点も `ArtifactAbstractEffect::setContext` が既に持つ。新規モジュールは不要で、足りないのは `ParamState → AbstractProperty` の紐付けとスイート実装だけだった。`AbstractProperty` は `Impl*` 保持だがコピーコンストラクタが `new Impl(*other.pImpl)` で深いコピーすること実装で確認済み。
- **実装内容（Host）:** `ParamState` に `AbstractProperty property` を**値所有**で追加（ポインタ不可。`previewProperties` は vector で拡張時に要素が移動し、`&back()` が dangling するため）。キーフレーム5 API（`paramGetNumKeys` / `paramGetKeyTime` / `paramGetKeyIndex` / `paramDeleteKey` / `paramDeleteAllKeys`）を `*Unsupported` から実実装に置き換え、OFX 秒 ↔ `RationalTime` の変換ヘルパー（`toRationalTime` / `toOfxTime`）と多次成分解決（`resolveComponent`）を追加。`paramGetValueAtTime` は `property.interpolateValue()` で時刻評価し、`paramSetValueAtTime` は `property.addKeyFrame()` でキーフレームを積む。`toAbstractProperty` は `Min` / `Max` / `DisplayMin` / `DisplayMax` / `Increment` / `Default` を読み `setHardRange` / `setSoftRange` / `setStep` / `setDefaultValue` に反映（従来は無視されていた）。アニメーション申告値を 0 → 1 に復元。
- **実装内容（Bridge）:** `syncParametersToPlugin` はキーフレームがあれば現在のフレーム時刻で `interpolateValue()` した値をプラグインに渡す（従来は静的な `prop.getValue()` のみ）。`setPropertyValue` は編集値を `param->property` へも書き戻し、编辑器とプラグインの値を一致させた。
- **追加スイート:** `Progress`（V1 / V2 両方）と `Interact` を実装し `fetchSuiteCallback` から供給。Progress はホストに UI がないため受理して no-op にする（`Failed` を返すとプラグインが処理を中止するため）。Interact は `SupportsCustomInteract = 0` を申告しているため失敗を返す。
- **自己監査で発見した四つの実装バグ（すべて修正済み）:** (1) `AbstractProperty::getAnimatable()` は存在しない API で，实际は `isAnimatable()`（コンパイル不能）。(2) `std::llround` を使うが `<cmath>` が global module fragment に無く推移的 include 頼みだった（両ファイルに追加）。(3) `paramGetValueAtTime` が `va_start` / `va_end` なしで `va_arg` を呼んでおり**未定義動作**だった。単一 `result` 変数と `break` 代替で全 return 経路が `va_end` を通る形に書き直し。(4) `ParamState::property` と `previewProperties` の同期に関するコメントが実態より強かった（実際は describe 時のコピー 1 回で、同期は `setPropertyValue` 側）。
- **価値または懸念（未検証）:** キーフレームの機構は既存 `AbstractProperty` に完全に乗ったため将来の実装コストは「ほぼゼロ」で済んだ。**実ビルドと実機での動作確認は未実施**（AGENTS.md 方針によりビルド未実行）。特に (a) `paramSetValueAtTime` でプラグインが書いたキーが `AbstractProperty` の 1 既存補間 Curves と正しく合成されるか、(b) プラグインが `paramGetValueAtTime` で指定時刻可得値とレンダー時刻が同じ評価になるか、(c) `Min`/`Max` を読んだスライダの UI 表示が正しいか、は実機確認が必要。`paramGetDerivative` / `paramGetIntegral` / `paramCopy` は依然 `kOfxStatErrUnsupported` のまま残置（補間の微分/積分は未実装）。GPU テクスチャ共有は未着手（下記）。
- **次に確認すべきこと:** (1) ビルドしてキーフレームアニメーションの実動作確認 (2) `paramGetDerivative` / `paramGetIntegral` の要否を実プラグインの用法から判断 (3) GPU テクスチャ共有の設計（下記）。
- **GPU テクスチャ共有の結論（2026-10-01 追記）: 打ち切り。ユーザー承認済み。** GPU 共有の実装可否を調査した結果、OFX 1.5 標準の GL 経路は **技術的に到達不能** と確定した。阻断要因は三つ、いずれも実ファイルで確認：(1) `CMakeLists.txt:640` の `set(DILIGENT_NO_OPENGL ON CACHE BOOL "" FORCE)` により submodule に GL バックエンドのソース（`libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineOpenGL/`、48ファイル）は存在するがビルド対象から完全に除外されている。(2) Diligent は API 間 interop を一切提供しない。`ImportTexture` / `ExportTexture` / `OpenSharedHandle` / `CreateSharedHandle` を submodule 全体で検索して 0 件。`ITexture::GetNativeHandle`（`Texture.h:510`）は GLuint を返すが、これは GL バックエンド自身が生成したテクスチャにのみ有効（`TextureBaseGL.hpp:109`）であり、D3D12 リソースを GLuint に変換する経路は存在しない。(3) OFX パスにはそもそも GPU ハンドルが存在しない。`SetGpuResources` の実呼び出しは `ArtifactPr/src/GpuProgramMonitor.cppm:331` の 1 件のみで、`ArtifactPr` は `ARTIFACT_BUILD_PR=OFF`（`CMakeLists.txt:269`）で無効。OFX ホストの `RenderFrameData` は生の CPU ポインタ（`unsigned char*`）。また `ofxOpenGLRender.h` は 8 行の空スタブで、実体はすべて `ofxGPURender.h` にあり、`kOfxImageEffectActionGetImageData` はそこに存在しない（CPU 画像スイートのパスでホストが既に `clipGetImage` で供給している）。
- **GPU テクスチャ共有の残る代替策（いずれもゼロコピーではない／未採用）:** 案A = 既存 `UpdateCpuDataFromGpuTexture()` でダウンロードし `glTexSubImage2D` でホスト所有の `wgl` コンテキストへアップロード（Diligent 変更ゼロ、ただし毎フレーム GPU→CPU→GPU 往復）。案B = `DILIGENT_NO_OPENGL` を OFF にして `Diligent-GraphicsEngineOpenGL-static` をリンク（submodule は変更不要。親 CMake のキャッシュオプションで制御される）。ただし案B でも GL デバイスが得られるだけで D3D12→GL のゼロコピー転送は提供されない。いずれの案も実害がないため採用しない。



## 2026-10-01 — OFX クラッシュ隔離・ブラックリストと残存 analysis API の実装

- **ユーザー承認:** 「クラッシュ隔離・ブラックリスト」と「paramGetDerivative / paramGetIntegral / paramCopy」の 2 グループを実装する指示。PluginSandbox 再利用の可否を先に調査し、不可と判明した場合は選択肢を提示して決定 получил。
- **PluginSandbox は OFX に再利用不可と確定（実ファイルで確認）:** (1) runner は `ArtifactPlugin_GetAPIVersion` / `ArtifactPlugin_GetPluginCount` という自作 ABI の 2 シンボルを前提にしており（`artifacts-plugin-runner/src/main.cpp:70-73`）、サードパーティの OFX DLL はエクスポートしない。(2) pong 応答のフィールド名不整合（`PluginSandbox.cppm:88-89` は `cmd` を待つが `main.cpp:95` は `event` を返す）により、監督対象が常にクラッシュ扱いになってリスタートループする。(3) ランナー実行ファイル名も不整合（`PluginLoader.cppm:171-175` は `ArtifactPluginRunner.exe` を検索、CMake は `artifacts-plugin-runner` をビルド）。(4) `PluginRegistry` は OFX とは完全に独立しており、`PluginState::Failed` は enum に存在するだけでどこからも代入されない。以上のため PluginSandbox / PluginRegistry の再利用は中止し、SEH ガード＋自作ブラックリストを実装した。
- **実装（クラッシュ対策）:** `callPluginEntryPoint` に `__try/__except` を導入。SEH と C++ アンワインディングは同一関数内で共存できない（MSVC C2712）ため、ガード関数は POD ローカルしか持たない設計にした。全 10 箇所の `mainEntry` 呼び出しを `dispatchAction` 経由に変更。識別子ごとのクラッシュカウンタ（3 回で自動ブラックリスト）、`QSettings` 永続化、`describePlugin` でのスキップ、Plugin Manager の「Disabled after repeated crashes」行と `Re-enable` ボタンを追加。
- **実装（analysis API）:** `paramGetDerivative` は中央差分（1フレーム幅。`toRationalTime` が整数フレームに丸めるためこれより小さい刻みは同じフレームに潰れる）。`paramGetIntegral` は Simpson 積分でキーフレーム境界ごとに分割（Hold / Step 区間を平滑化しない）。`paramCopy` は 8 引数版 `addKeyFrame` で補間種別と Bezier ハンドルを保持。
- **自己監査で見つけた二つの構造欠陥（修正済み）:** (1) `ArtifactOfxHost` はクラス本体より後（2464 行）に定義されているが、`pluginActionLoad` / `pluginActionDescribe` / `describePlugin` が **先に** `ArtifactOfxHost::instance().dispatchAction(...)` を呼んでいた。不完全型に対するメンバ呼び出しは ill-formed（C2027 / C2612）。無料関数 `dispatchOfxAction` / `ofxPluginIsBlacklisted` を経由するよう変更し、定義をクラス本体より後に置いた。(2) その際、4 関数の宣言を匿名名前空間内に置いたまま定義を `namespace Ofx` に置いたら、[namespace.unnamed] により別エンティティとなりリンクエラーになる可能性が高い状態になった。ファイル既存の `imageEffectForClip` と同じ「`namespace {` の前に宣言、クラス本体後に定義」というパターンに揃えた。
- **価値または懸念（未検証）:** SEH はハードウェア例外のみを捕捉する。スタックオーバーフロー、後に検出されるヒープ破壊、プラグイン内部の CRT abort は捕捉できないため、SEH は「アプリが即死しない」保証であって「プラグインを安全に隔離できる」保証ではない。**実ビルドと実機確認は未実施**（AGENTS.md 方針によりビルド未実行）。特に (a) 実際にアクセス違反するプラグインを用意して SEH が発火しブラックリストに入るか、(b) 3 回のカウントが正しく加算されるか、(c) 再起動後もブラックリストが持続するか、(d) `Re-enable` で復帰するかは要確認。
- **次に確認すべきこと:** (1) ビルドして SEH の実挙動を意図的にクラッシュするテストプラグインで確認 (2) PluginSandbox の pong フィールド名バグとランナー名不整合を本作業とは別件として修正するか判断 (3) 本格的なプロセス間隔離は複数週規模なので、他のプラグイン系統の隔離（TuttleOFX 等の既存路子）と併せて設計する。
- **未実装として残ったもの:** GPU テクスチャ共有（打ち切り）、プロセス間隔離（新規サブシステム）。



## 2026-10-01 — OFX/Plugin 残存4件の対応（pongs・runner 名・abort 記録・非 Windows ロード）

- **ユーザー承認:** 前回の報告に残っていた 4 項目（SEH の限界、PluginSandbox の pong バグとランナー名不整合、非 Windows ロードの no-op）の実装を指示。
- **前提の訂正（assistant 側）:** 報告時に「SEH はスタックオーバーフロー・CRT abort・ヒープ破壊を捕捉できない」と一括して述べたが、三者は性質が異なる。(1) スタックオーバーフローは SEH のガード領域に到達しないため原理的に捕捉不可。ガード用の予約スタックを維持する手法も SEH の枠組みでは機能しない。(2) CRT abort は例外を発生させないため `__try/__except` では捕捉できないが、**記録は可能**。(3) ヒープ破壊は破壊後の無関係なアロケータ処理時に検出されるため、リリースビルドでは in-process 捕捉が原理的に不可能。この分類を明示して実装した。
- **PluginSandbox の pong バグ修正:** 監督側は `response["cmd"] == "pong"` を待っていたが、runner は `pong["event"] = "pong"` を返す（`artifacts-plugin-runner/src/main.cpp:94-97`）。この不一致で `expectingPong` が解除されず、1 秒後に必ずクラッシュ判定されてリスタートループしていた。`event` を主判定、`cmd` を副判定として受理するように修正。
- **ランナー実行ファイル名不整合の修正:** `PluginLoader.cppm:169-172` は `ArtifactPluginRunner.exe` を検索するが、CMake は `artifacts-plugin-runner` をターゲット名にしていた。`artifacts-plugin-runner/CMakeLists.txt` に `OUTPUT_NAME "ArtifactPluginRunner"` を設定。ロード側も `runnerExecutableName()` にして、ハードコードされた `.exe` を解消（非 Windows では必ず失敗する作りだった）。
- **非 Windows ロードの実装:** `openPluginLibrary` / `closePluginLibrary` / `resolvePluginSymbol` の 3 ヘルパーを導入し、Win32 は `LoadLibraryW` / `FreeLibrary` / `GetProcAddress`、それ以外は `dlopen` / `dlclose` / `dlsym`。`scanBinary` と `clearLoadedPlugins` の `#ifdef _WIN32` ガードを外し、`ArtifactOfxEffectImpl.cppm` の `findPlugin` も `resolvePluginSymbol` 経由に変更（`GetProcAddress` の直接使用は両ファイルから消えた）。`dlfcn.h` は global module fragment の `#ifndef _WIN32` 側に include。
- **CRT abort の記録:** `AddVectoredExceptionHandler` で `STATUS_FATAL_APP_EXIT` / `STATUS_STACK_BUFFER_OVERRUN` / `STATUS_HEAP_CORRUPTION` を監視する向量ハンドラを追加。`dispatchAction` が呼び出し中プラグインの識別子を `activePluginIdentifier()`（静的参照を返す inline 関数）に公開し、ハンドラはそれを `OutputDebugStringA` で出力する。ハンドラは割り当てを一切行わない（ヒープ破壊状態で実行され得るため）。`EXCEPTION_CONTINUE_SEARCH` を返すので **捕捉はせず観測のみ**。
- **副次的な修正:** `PluginLoader.cppm` が `std::function` を `<functional>` なしで使っていた（推移的 include 頼み）。明示 include を追加。
- **自己検証:** 5 ファイルぶんの整合性を検査し 24 項目すべて PASS（EXCEPTION ハンドラのシグネチャ、割り当て不在、匿名名前空間の平衡、4 つの out-of-class 定義の位置、`GetProcAddress` の直接使用ゼロ、QtCSS 不使用など）。コンパイル阻害欠陥なし。
- **価値または懸念（未検証）:** **ビルド未実行**（AGENTS.md 方針）。特に (a) `<dlfcn.h>` が MSVC 以外のツールチェインで実際に解決できるか、(b) `dl` のリンク依存が CMake で充足されているか（glibc 2.34 以降と macOS では libc/libSystem に含まれるため `-ldl` 不要だが、最小 glibc バージョンは未確認）、(c) 非 Windows 実機でのプラグインロードが動作するか、は要確認。abort 記録は OutputDebugString への出力にとどまり、永続レポートや blacklist への自動登録にはなっていないため、**abort したプラグインが自動ブラックリストに入るわけではない**（SEH で捕捉できた場合のみ入る）。
- **次に確認すべきこと:** (1) ビルドして非 Windows 経路のコンパイル確認 (2) abort 記録を永続レポートに昇格させ、ブラックリストへ自動登録する設計をするか判断 (3) プロセス間隔離を設計するなら、他のプラグイン系統（TuttleOFX 等の既存路子）と併せて設計する。



## 2026-10-01 — レイヤーブレンドモード整合性チェック（GPU/CPU 数式・領域・PSD キー）

- **確認できた事実（実ファイルで検証）:** `BlendMode` は 34 モードで `ArtifactCore/include/Layer/LayerBlend.ixx:12-47` と `ColorBlendMode.ixx:42-77` に二重定義され、`LAYER_BLEND_TYPE`（同 `:51-86`）が相互 cast の alias になっている。表示名は 3 表が独立ハードコードで、`blendModeInfoTable`（`BlendModeInfo.ixx:42-77`）、`BlendModeUtils::toString`（`LayerBlend.ixx:101-139`）、`blendModeLabelFromKey`（`PSDDocument.cppm:215-239`）。現状 34 件は一致するがコンパイル時チェックが無い。GPU は `LayerBlendComputeShader.ixx:762-797` の `BlendShaders` が 34 モード全覆盖で、`BlendOp`/factor の固定パイプライン表は存在せず compute shader のみ。
- **誤診を訂正した点（重要）:** Stencil/Silhouette 系について「GPU は premul RGB も減衰し CPU は α のみ変更なので見た目が違う」と報告したが、これは誤り。`convertLayerToFloat`（`LayerBlendComputeShader.ixx:104-135`）が blend 前に premul→straight を復号（`straight = src.rgb / alpha`）し、`blendGpuLayerIntoAccum`（`ArtifactCompositionRenderController.cppm:12787-12796`）がその結果の `layerFloatSRV` を `preparedBlendSRV` として渡す。したがって blend シェーダの `dst.rgb` は straight であり、`dst.rgb * factor, dst.a * factor` は α のみを削る AE 的意味論に一致し、CPU 側 `applyStencilLikeBlend`（`ArtifactSoftwareImageCompositor.cppm:58-68`）と一致する。比較の結果、色空間成分だけが残る実在の差分だった。
- **修正内容（ユーザー承認 3 項目）:** (1) `Artifact/src/Render/Software/ArtifactSoftwareImageCompositor.cppm` に `sceneLinearLuma()` を追加し、`StencilLuma`/`SilhouetteLuma` の分岐（`:192-199`）を GPU の `matteSrgbToLinear` + Rec709（`LayerBlendComputeShader.ixx:729,756`）と同一色空間に統一。従来の `ColorLuminance::calculate` は sRGB のまま Rec709 を適用していた。(2) `ArtifactCore/src/Image/PSDDocument.cppm:227` の `"div "` が `Difference` を返していた誤りを `Divide` に訂正し、enum 変換 `toBlendMode`（`:541`）に `"div "` → `BlendMode::Divide` を追加（従来は PSD import で Normal に落ちていた）。(3) 部分再合成（damage region）で Normal 以外の全モードが `blend()` から `return false` され非合成になっていた。`CHECK_BOUNDS` macro（`LayerBlendComputeShader.ixx:63-68`）に `dispatchOrigin`/`dispatchExtent` 対応を追加して `pixelCoord` を定義し、34 シェーダ全ての `SrcTex[id.xy]`/`DstTex[id.xy]`/`OutTex[id.xy]` を `pixelCoord` に置換（Dissolve/DancingDissolve のハッシュ入力も座標基準に統一）。`LayerBlendPipeline.cppm:859-861` の `(!fullRegion && mode != BlendMode::Normal)` guard を削除し、`region.validFor` のみ残置。
- **残存する既知の不一致（未修正・記録のみ）:** (a) Dissolve の出力アルファ — GPU は `float4(src.rgb, 1.0)` で強制不透明（`LayerBlendComputeShader.ixx:623,645`）、CPU は `dstRow[i+3] = srcRow[i+3]` で保持（`ArtifactSoftwareImageCompositor.cppm:699`）。(b) `Classic*` 3 モード（`ClassicColorBurn`/`ClassicColorDodge`/`ClassicDifference`）は GPU・CPU 両方で非 Classic と同一式だが、`BlendModeInfo.ixx:69-72` の `isClassic=true` は別挙動を暗示している。(c) `BlendKind` のグループ分類（`BlendModeInfo.ixx:51-52`）が AE と相違し `blendModeGroupName()` は UI 側から未使用。(d) レイヤー本体の blendMode に Property Editor / Inspector の受け皿が無い（`ArtifactAbstractLayerPropertyGroups.cppm` に登録なし）。Timeline のインラインコンボ（`ArtifactLayerPanelWidget.cppm:4630-4675`）のみ。(e) `ArtifactExportLottieWriter.cppm:278` の `clamp(0,16)` で 17 以降が Luminosity に潰れる。(f) `ArtifactTimelineTrackPainterView.cppm:9604` の `editBlendModeAct` は宣言のみで代入が無いデッドコード。
- **価値または懸念（未検証）:** GPU/CPU の Stencil/Silhouette は α のみ/色保持で AE 的意味論に合流し、部分再合成が全ブレンドモードで機能するようになった。ただしビルド未実行（AGENTS.md 方針）、実機での領域再合成挙動は未確認。(a) の Dissolve アルファは 半透明背景で GPU/CPU 差が出るため実害あり、(b) のメタデータと実装の矛盾は UI が嘘をつく形になるため注意。**ArtifactCore には本作業の前から差分が存在する**（CMakeLists.txt / cmake/ArtifactCoreSources.cmake / include/Plugin/PluginRegistry.ixx / src/CLAP/CLAPHost.cppm / src/Plugin/PluginRegistry.cppm — いずれも本作業と無関係）。
- **次に確認すべきこと:** (1) ArtifactCore と Artifact をビルドしてシェーダコンパイルとリンクが通ることを確認 (2) 部分再合成（damage region）発動時の Add / Multiply 等で領域内プレビューが正しく更新されるか実機確認 (3) PSD ファイルを開き「Divide」レイヤーが `Divide` として読込まれるか確認 (4) StencilLuma / SilhouetteLuma のグラデーション matte で GPU と CPU の見た目が揃ったか確認 (5) Classic* のメタデータ `isClassic` を実装に合致させるか、別式を実装するか方針決定 (6) Dissolve のアルファ出力（GPU=1.0 / CPU=保持）のどちらを正とするか決定。


## 2026-09-30 — Motion tracking: persistence, Undo, hot path, and solver conventions

- **Scope:** `ArtifactCore/src/Tracking/MotionTracker.cppm`, `ArtifactCore/include/Tracking/MotionTracker.ixx`, `ArtifactCore/src/Video/Stabilizer.cppm`, `ArtifactCore/include/Video/Stabilizer.ixx`, `Artifact/src/Project/ArtifactProject.cppm`, `Artifact/src/Project/ArtifactProjectImporter.cppm`, `Artifact/src/Undo/UndoManager.{ixx,cppm}`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `Artifact/src/Widgets/Render/ArtifactPointTrackerGizmo.cppm`, `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`.
- **Facts (verified by reading the code):** (1) `MotionTracker::toJson/fromJson` were fully implemented but never called; `Artifact/src/Project/**` referenced `TrackerManager` zero times, so a save/reload dropped every track point, frame, homography and problem frame while the layer kept a dangling `motionTrackerId`. (2) No tracking undo command existed; `Artifact/include/Undo/` had zero tracking entries, so `ArtifactPointTrackerGizmo` could clear the whole result with one click. (3) `VideoStabilizer::estimateMotion` was an empty loop and `detectFeatures` scored corners with `dx*dy - (dx+dy)^2`, which is negative for every non-degenerate input, so the Core stabilizer always returned an empty feature list. The Effect-side `StabilizerEffect::estimateMotion` was a complete Similarity implementation and was the one actually in use. (4) `ArtifactPointTrackerGizmo::draw` copied the entire `TrackResult` three times per frame, rebuilt the motion path vector per frame, constructed three `QFont`s per frame, and rescanned the whole result for every path sample (O(N^2)). (5) Point tracking divided velocity by `|dt|` while planar tracking used a signed delta, so backward tracking reported mirrored positive velocity and inverted `removeOutliers`.
- **Fixes:** added `MotionTracker::adoptTracker` / `TrackerManager::toJson/fromJson` plus an id-restoring constructor, and wired the array into the project `assets` section on save and restore; added `TrackerResultCommand` and `SetLayerMotionTrackerCommand` and applied them to solve, reset, mode switch, smoothing, outlier removal and confidence filtering; implemented `getPrevFeatures`/`updateFeatureTracks` (their header declarations were missing) and a Similarity `estimateMotion` in the Core stabilizer, and replaced the always-empty Harris detector with a real structure-tensor response; added `resultRef()`, `frameAt()` and a buffer-filling `motionPath()` and switched the gizmo to cached scratch buffers; unified the velocity sign convention, the homography epsilon (now NaN plus an explicit failure frame instead of silently returning the source point), and the window/pyramid clamps.
- **Deliberately NOT changed:** `Track.NccTracker` and `Track.LayerTrack` remain registered even though the renderer no longer imports the former and the latter is an empty class with a misspelled namespace. Deleting CMake-registered sources forces a full rescan, and `NccTracker` is the solver named by `MILESTONE_2D_POINT_TRACKER_2026-06-16.md`, so removing it would diverge from the spec. `TrackerSettings::quality` and `subpixelAccuracy` are serialized but never read; they are a schema-compatibility concern rather than dead weight.
- **Unverified:** build and runtime were not permitted. The `.ixx` interface changes (`MotionTracker`, `Stabilizer`, `UndoManager`) need an MSVC interface compile to confirm. The newly wired project save/load path has not been exercised on a real project file.
- **Next:** after a build is allowed, confirm (a) save/reload round-trips trackers and the layer link survives, (b) solve/reset/smooth are undoable, (c) Core `VideoStabilizer` now produces motion, (d) gizmo allocation per frame drops. Note the `sourceName` round-trip still overwrites itself with the tracker name and remains unfixed.



## 2026-09-30 — エフェクト実装モデル違反2件（CPU_ONLY 誤 enum / 到達不能な applyCPU）

- **関連:** `Artifact/include/Effects/ArtifactAbstractEffect.ixx`、`Artifact/src/Effects/ArtifactAbstractEffect.cppm`、`Artifact/src/Effects/ColorCorrection/LUTEffect.cppm`、`Artifact/src/Effects/ColorCorrection/ShadowHighlightEffect.cppm`、`Artifact/include/Effects/OpticsCompensation/OpticsCompensationEffect.ixx`、`Artifact/src/Effects/OpticsCompensation/OpticsCompensationEffect.cppm`。
- **確認できた事実（確定）:** (1) `ComputeMode` の実定義は `CPU / GPU / AUTO` の3値のみで（`ArtifactAbstractEffect.ixx:161-165`）、`ComputeMode::CPU_ONLY` はリポジトリ中に存在しない列挙子だった。参照は `LUTEffect.cppm:82` と `ShadowHighlightEffect.cppm:167` の2箇所のみで、両ファイルとも `ArtifactSources.cmake` に登録済みの実ビルド対象だった。両方とも `supportsGPU()==false`（`LUTEffect.ixx:68`、`ShadowHighlightEffect.ixx:56`）なので、`CPU` へ置換しても実行時挙動は不変。(2) `OpticsCompensationEffect` は private・非virtual な `applyCPU` をメンバ宣言していただけで `setCPUImpl()` を一度も呼ばなかった。`ArtifactAbstractEffect::apply` は `mode==CPU && impl_->cpuImpl_` を要求し（`ArtifactAbstractEffect.cppm:706`）、cpuImpl_ が null のため無言で `dst = src.DeepCopy()` の素通しになっていた。一方 Factory（`ArtifactEffectService.cppm:904`）と `availableEffects()`（`:1312`）から到達可能だったため、UI 上で追加できながら常に無効という状態だった。
- **対応:** (1) は2箇所を `ComputeMode::CPU` へ置換。(2) は `ArtifactEffectImplBase` 派生の `OpticsCompensationEffectCPUImpl` を cppm 側 anon namespace に起こし、ctor で `setCPUImpl()` を呼び、4つの setter 末尾で `syncImpls()` して値を push する形へ移行。ヘッダから未使用 import（`Image.ImageF32x4_RGBA` / `ImageProcessing.Distortion` 等）を削除し、`supportsGPU()==false` を明示。実装形は同ディレクトリの Magnify / PinchBulge / Ripple / PolarCoordinates と同じ `applyDisplacement` → `setFromRGBA32F` 規約に揃えた。
- **価値または懸念:** エフェクトの「動かない」原因が3種類あることを実例で確定した。①CompileEnum  misspelling、②Impl 未登録で apply の素通し、③UI 未登録。①と②はコンパイラの警告を出しづらく、既存テストでも捕まりにくい。今回の2件はいずれも「UI から到達できるのに無効」という形で顕在化していたため、次回以降の精査では『Factory に登録済みだが `setCPUImpl`/`setGPUImpl` のどちらも呼んでいない effect』を機械的にgrep-able な検査対象にすると意義が大きい。**未検証:** ビルド／実行は許可されていないため、コンパイル通過と実際の描画結果は未確認。
- **次に確認すべきこと:** ビルド許可後、(a) 2つの ColorCorrection エフェクトがリンク・起動できることを確認、(b) Optics Compensation を Inspector から追加して FOV を変えると実際に歪みが変わることを確認、(c) 本件finder の拡張として「`ArtifactEffectService::createEffect` が生成しうる全 effect 型に対し `setCPUImpl|setGPUImpl` の呼出が1つ以上あるか」をgrepで機械検査し、②型の残存を洗い出す。



## 2026-09-30 — layer bus は Routing Node：volume / pan は layer 所有、mute のみ bus が担う

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` の `getAudio`、`Artifact/src/Service/ArtifactAudioService.cppm` の `syncCurrentComposition` / `setLayerBusVolume` / `setLayerBusPan`、`Artifact/src/Audio/ArtifactAudioMixer.cppm` の `syncFromComposition` / volumeChanged / panChanged、`Artifact/src/Widgets/AudioMixerWidget.cppm` の fader / pan スライダー、`ArtifactCore/src/Audio/AudioMixer.cppm` の `deserialize`、`Artifact/src/Layer/ArtifactAudioLayer.cppm` / `ArtifactVideoLayer.cppm` / `ArtifactSpatialAudioLayer.cppm` の `getAudio`。
- **確認できた事実:** 音声を出す3レイヤー全てが、生成する PCM へ自身の volume と pan を適用済み（Audio は `ArtifactAudioLayer.cppm:1331-1338`、Video は `ArtifactVideoLayer.cppm:2873-2877`、Spatial は `ArtifactAudioLayer::getAudio` を再利用するため base を含む）。評価パス（`ArtifactAbstractComposition.cppm` の getAudio）は layer bus へ volume を再設定し、AudioLayer では `layerVol = 1.0f`（=0dB）で二乗を避けていたが、**pan には同様のリセットが無く**、かつ **VideoLayer だけは `layerVol = vl->audioVolume()` で bus にも入れていたため実数が二乗していた**。旧コメント「square the requested gain」は AudioLayer 前提で書かれていて Video 層を巻き込んでいた。
- **対応:** layer bus を「中立な routing node」と定義し、音量・pan の bus への書き写しを全経路で停止した。評価パスは layer bus の volume と pan を毎ブロック 0dB / 中央に強制し、保存済み値による二重適用の再発を防ぐ。`ArtifactAudioService` の sync / setLayerBusVolume / setLayerBusPan は layer プロパティへの書き込みのみに narrowed。`ArtifactAudioMixer` の strip も同じく narrowed。`AudioMixerWidget` の fader / pan は `busKind(bus_) == AudioBusKind::Layer` のとき書込みを skip し（Group / Return / VCA は上流段が無いので従来どおり機能）。`AudioMixer::deserialize` も `kind == Layer` のとき volume / pan を 0 に上書きして復元時の二重適用を防ぐ。
- **mute の例外（意図的に保持）:** `ArtifactVideoLayer::getAudio` には `audioMuted_` の判定がどこにも無い（状態定義 `ArtifactVideoLayer.cppm:574`、setter/getter 2506-2513、serialization 2546-2547 のみ）。そのため **Video 音声の唯一の mute は bus mute** であり、layer bus への mute 書き写しは残す。Audio / Spatial は `getAudio` 冒頭で muted を判定して false を返すため bus mute は redundant。
- **価値または懸念（未検証）:** layer bus の fader / pan が no-op になった。これは「layer の PCM に既に適用済み」という所有定義から導かれる意図的な結果であり、AE のグループ/Effect バス的trim用途には使えない。もし bus 側 trim が必要なら、別ノード（group bus や insert パス）として設計すべきで、layer bus の再利用は避けるべき。runtime での聴感・build/test は未確認。
- **次に確認すべきこと:** ビルド・テスト許可後に、Video 層の音量が二乗で減衰しなくなったこと、layer bus の fader / pan が no-op になっていること、Group bus の fader / pan が従来どおり効くこと、保存済み layer bus の pan / volume が可直接きかないことを確認する。必要なら AudioMixerWidget の layer bus の fader / pan UI を disabled 表示にして、no-op であることをユーザーに明示する。



## 2026-09-30 — Layer Editor のイベント座標と renderer viewport は物理 px

- **関連:** `Artifact/src/Widgets/Render/ArtifactLayerEditorWidget.cppm`、`Artifact/src/Widgets/LayerEditorFrameViewState.cppm`。
- **確認できた事実:** renderer viewportは`layerEditorPhysicalViewportSize()`でwidgetのwidth/heightにDPRを乗じて設定される。Qtのmouse/wheel `position()`とmouse deltaは論理pxであり、Layer Editorのpan/zoom anchor経路には従来DPR変換がなかった。renderer view backup stateもこれまでpanX/panY/zoomを生floatで保持し、key/chrome zoom controllerが同じzoom stateを生float pointerで共有していた。
- **対応:** `panBy` / `setPan`は物理screen vector、`zoomAroundPoint`は物理screen point + `ScaleFactor`を要求し、Qtイベント境界でlogical→physical変換を一度行う。`LayerEditorFrameViewState`と共有zoom stateはphysical pan vector / `ScaleFactor` zoomを保持し、frame-view backup/background描画もphysical extentを要求する。Chrome stateは論理event point・物理point/extent・composition extent・DPR factorを分離し、hit-test APIも物理point/extentを要求する。
- **価値または懸念（未検証）:** 異なるscreen scaleの座標をrendererへ直接渡せず、DPR>1でのpan量とzoom anchorがrenderer viewportと整合する。DPI別runtime挙動は未確認。
- **次に確認すべきこと:** 実行確認が許可された際にDPR 1.0 / 1.5 / 2.0でpan距離とzoom anchor維持を確認する。



## 2026-09-30 — ShadowHighlightEffect の radius 数値が処理幅に反映されない

- **関連:** `Artifact/include/Effects/ColorCorrection/ShadowHighlightEffect.ixx`、`Artifact/src/Effects/ColorCorrection/ShadowHighlightEffect.cppm`。
- **確認できた事実:** APIコメントはshadow/highlight radiusをpxと示す。現行CPU実装はradiusを0以上にした後、各radiusを`> 0`の有効判定にだけ使い、radius magnitudeを近傍幅・kernel・画像値の計算に使っていない。
- **価値または懸念（未検証）:** UIでradiusを変更しても0超の範囲では結果が同じ可能性が高い。現状処理経路を読む限りの結論であり、実行時出力比較は未実施。
- **次に確認すべきこと:** ユーザーから該当effectの挙動修正が依頼された場合、意図されたアルゴリズムと画像サイズ/DPIに対するpx半径の意味を確定し、複数radius値で画像出力を比較する。



## 2026-09-30 — 3D gizmo position setter への local/world 値混在

- **関連:** `Artifact/include/Layer/ArtifactAbstractLayer.ixx`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm` の `position3D()` / `setPosition3D()`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `syncGizmo3DFromLayer` と Undo snapshot。
- **確認できた事実:** `position3D()` は親変換を適用せず `AnimatableTransform3D` のposition channel値を返し、親込み変換は別の `getGlobalTransform4x4()` が行う。APIは`LayerLocalPoint3`化済み。`Artifact3DGizmo`のposition setter/getter、group/projected-frame world anchor/basis stateをworld point/vector型にした。`GizmoTransformSnapshot::position`もauthored local point型にし、visual gizmo開始pivotを別の`WorldPoint3` stateへ分離。通常moveのworld deltaは親global inverseの線形部でlocal vectorに変換してからlocal pointへ加算する。
- **価値または懸念（runtime未検証）:** Undo/property snapshotとvisual pivotを異なる型・stateに分け、異空間のpointを直接減算・加算できなくした。既存の式を型付き境界へ写した段階であり、親付き操作の実機・Undo parityは未確認。非可逆な親inverseではlocal位置を更新しない。
- **次に確認すべきこと:** ビルド許可後にroot/parented single・multi 3D layerのmove・hit-test・undo/redo、projected-frame scale/moveとShift制約を確認する。残るsnapshot外の`QVector3D` drag-local fieldsも座標意味を棚卸しする。

- **追加確認 (2026-09-30):** projected-frame開始軸とmulti-selection basisはglobal matrix / inverse viewから作られるworld方向だったが、controller shared stateとQt演算をまたぐQVector3Dだった。stateを`WorldVector3`にし、Qt幾何演算境界の変換を明示した。値・basis式・正規化処理は不変。コンパイル・Shift制約・複数選択操作は未検証。



## 2026-09-30 — Layer Editor event positions must match the physical renderer viewport
- **関連:** `ArtifactCore/include/Math/Vec.ixx`, `Artifact/src/Widgets/Render/ArtifactLayerEditorWidget.cppm`, `Artifact/src/Widgets/LayerEditorViewPressController.cppm`, `Artifact/src/Widgets/LayerEditorViewMoveController.cppm`, `Artifact/src/Widgets/Render/TransformGizmo.cppm`.
- **確認できた事実:** Layer Editor sets renderer viewport dimensions from `layerEditorPhysicalViewportSize()`, which multiplies widget dimensions by DPR. `TransformGizmo::hitTest` maps the incoming viewport point through `renderer->viewportToCanvas` and compares it with handle rectangles produced in renderer viewport coordinates. Before this change, mouse events supplied Qt logical positions directly to several `viewportToCanvas` calls.
- **実施:** Mouse-driven view/gizmo, shape, and mask paths now convert Qt logical event positions to `ScreenPhysicalPoint2` with the widget DPR before viewport-to-canvas mapping. Chrome hit testing keeps its logical event input because its controller applies DPR itself. Pan tracking stores its start point as `ScreenLogicalPoint2`, calculates a `ScreenLogicalVector2` delta, and converts that vector at the renderer boundary. `Math.Vec` now provides the explicit physical-point-to-`QPointF` boundary helper needed by legacy viewport APIs.
- **次に確認すべきこと:** Build and exercise Layer Editor at DPR 1.0 and >1.0, checking gizmo hit regions, shape/mask handles, modal transforms, rubber-band selection, and panning. These runtime checks are not yet performed.




## 2026-09-30 — Camera POI storage space differs from the picking ray
- **関連:** `ArtifactCore/include/Math/Vec.ixx`, `Artifact/include/Layer/ArtifactCameraLayer.ixx`, `Artifact/src/Layer/ArtifactCameraLayer.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`.
- **確認できた事実:** The existing overlay comment says POI is authored in the camera parent's coordinate space. Picking rays and viewport projection are world-space. The previous controller-only `WorldPoint3` tag was therefore incorrect for a parented camera; it has been replaced with `LayerParentPoint3` storage and explicit parent-to-world / world-to-parent conversion APIs.
- **対応:** Camera POI property state and Undo values keep the authored parent-space point. `pointOfInterestWorld()` maps it through the parent's current global transform; `setPointOfInterestWorld()` uses the inverse and fails on a singular parent transform. The two-node camera eye is mapped from authored parent-space position to world, and POI overlay/picking use the world target.
- **価値または懸念（未検証）:** This aligns parented camera aim, overlay, drag, and persistence spaces. Build/runtime behavior is not verified; camera layers nested under animated or non-uniformly scaled parents need runtime coverage.
- **次に確認すべきこと:** Compare root and parented cameras at translation/rotation/scale, then verify POI overlay hit, drag, Undo/Redo, cancel, and JSON round-trip. Build/runtime verification remains gated by the user instruction.



## 2026-09-30 — Projected Frame keeps its corrected authored position local-typed

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`projectedFrameCorrectedLocalPosition_`, press-time capture, scale/drag write-back), `Artifact/src/Widgets/Render/ArtifactCompositionGizmoUndoCommands.cppm` (`GizmoTransformSnapshot::position`).
- **確認できた事実:** The corrected position state is copied from `gizmoLayerTransformBefore_.position`, whose contract is `LayerLocalPoint3`; the value is later used to restore a projected-frame layer's authored position. Other branches map the gizmo's world point through the captured parent inverse before storing it.
- **対応:** Changed the shared corrected-position state to `LayerLocalPoint3` and removed its QVector3D unwrap/rewrap at capture and write-back sites. Parent-inverse conversion remains explicit at the boundary.
- **価値または懸念（未検証）:** The state can no longer accept a composition/world point accidentally. Static review has not verified compiler/module integration or drag parity.
- **次に確認すべきこと:** Compile when authorized; exercise projected-frame corner/edge scaling, rotation and move with and without a parent, including translated/rotated/scaled parent transforms and Undo/Redo.



## 2026-09-30 — Anchor drag snapshots share the layer-authored local space

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`gizmoAnchorDrag*`, `applyProjectedFrameAnchorDelta`, `commitProjectedFrameAnchorDrag`), `Artifact/src/Widgets/Render/ArtifactCompositionLayerUndoCommands.cppm` (`AnchorPointUndoCommand`).
- **確認できた事実:** Drag-start anchor values come from `transform3D()` anchor channels, and position comes from `position3D()`. Live updates write both back into the same layer transform; the existing Undo command still takes `QVector3D` values.
- **対応:** Changed the Controller's start/current anchor and compensated position snapshots to `LayerLocalPoint3`. Existing Qt math remains behind explicit typed-to-QVector conversion, while current snapshots remain typed until the legacy Undo boundary.
- **価値または懸念（未検証）:** Shared drag state now rejects direct world/Composition point assignment. The conversion of world anchor delta through the existing 2D `QTransform` and its behavior for 3D-rotated/scaled parents remains as-is and is not validated by this type-only change.
- **次に確認すべきこと:** Compile when authorized and exercise anchor drag with root and parented layers (including rotation/scale), then verify visual compensation, Undo/Redo and rejected-Undo rollback.



## 2026-09-30 — Group gizmo snapshots preserve rotation and scale units

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`gizmoGroupRotationBefore_`, `gizmoGroupScaleBefore_`, group drag calculation), `Artifact/src/Widgets/Render/Artifact3DGizmo.cppm` (`rotation()`, `scale()`).
- **確認できた事実:** `Artifact3DGizmo::rotation()` already returns `EulerDegrees3` and `scale()` returns `Scale3`; the controller immediately unwrapped these into QVector3D for shared drag-start state, then subtracted/divided them during group drag.
- **対応:** Store the captured values as `EulerDegrees3` and `Scale3`, keeping the original types until the existing Qt calculation boundary.
- **価値または懸念（未検証）:** The snapshot now distinguishes degrees and dimensionless scale at the shared-state boundary. Group gesture numerical/runtime parity and module compilation are unverified.
- **次に確認すべきこと:** Compile when authorized and check multi-selection translate, rotate, and scale with local/projected basis modes plus Undo/Redo.



## 2026-09-30 — Single gizmo visual snapshots keep degree and scale types

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`GizmoVisualSnapshot`, drag constraints, relative transform display), `Artifact/src/Widgets/Render/Artifact3DGizmo.cppm` (`rotation()`, `scale()`).
- **確認できた事実:** The visual snapshot captured existing typed gizmo getter results as QVector3D and then reused those values for scale ratios, rotation deltas, and HUD feedback. These values represent degrees and dimensionless scale, while authored layer transform state is separately stored in `GizmoTransformSnapshot`.
- **対応:** Stored the visual values as `EulerDegrees3` and `Scale3`, converting only at existing Qt vector calculation boundaries.
- **価値または懸念（未検証）:** Visual-start values retain their unit semantics alongside the separately typed world pivot and local Undo snapshot. Runtime gesture/HUD parity and compile/module integration remain unverified.
- **次に確認すべきこと:** Compile when authorized; verify modal and pointer-based gizmo rotate/scale, projected-frame constraints, relative transform display, and Undo/Redo.



## 2026-09-30 — Gizmo rotation deltas retain degree types until geometry APIs

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (group gizmo rotation delta and projected-frame rotate drag).
- **確認できた事実:** The gizmo getter and captured rotation are `EulerDegrees3`; group drag previously unwrapped both before subtracting, and projected-frame rotation subtracted raw QVector/scalar values.
- **対応:** Compute group deltas as `EulerDegrees3` and projected-frame Z delta as `Degrees`, then convert only at QQuaternion or legacy transform calculation boundaries.
- **価値または懸念（未検証）:** The subtraction itself rejects mixing angles with non-angle values before Qt math. Compile and interaction parity are not verified.
- **次に確認すべきこと:** Compile when authorized; verify group rotation in projected and free basis modes and projected-frame rotation with Undo/Redo.



## 2026-09-30 — Group gizmo relative scale stays a Scale3

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (group drag `gizmoScale`, `gizmoGroupScaleBefore_`, and relative `groupScale`).
- **確認できた事実:** Both current and drag-start gizmo scale are `Scale3`. Their axis-wise quotient is a dimensionless three-axis scale multiplier used for projected basis geometry and each selected layer's scale.
- **対応:** Keep current scale and the computed group multiplier as `Scale3`; unwrap only when feeding Qt vector geometry. Preserve the existing signed denominator fallback and multiplication expressions.
- **価値または懸念（未検証）:** This avoids representing an axis scale as an unqualified spatial QVector3D in shared group calculation state. Compilation and group scale interaction are unverified.
- **次に確認すべきこと:** Compile when authorized and exercise multi-layer scale in projected-basis and free-basis modes, including negative scale and near-zero start values.



## 2026-09-30 — Gizmo HUD values remain typed through formatting

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (gizmo HUD detail construction).
- **確認できた事実:** The HUD reads typed gizmo getters and typed start snapshots, but previously converted position, rotation, and scale to QVector3D before calculating position delta, rotation delta, and scale ratio.
- **対応:** Keep the HUD's world position/delta, Euler degree rotation/delta, scale, and scale ratio in their respective types until QString numeric formatting.
- **価値または懸念（未検証）:** HUD calculation can no longer subtract a local position from a world pivot or treat rotation/scale as spatial vectors. Existing expressions and displayed formatting are preserved; visual output is not runtime-verified.
- **次に確認すべきこと:** Compile when authorized; inspect HUD values during translate/rotate/scale gestures and compare before/after values and formatting.



## 2026-09-30 — Projected Frame constraint ratios are dimensionless scale factors

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (Projected Frame edge/corner Shift scaling).
- **確認できた事実:** Shift scaling computes current-axis scale divided by the press-time scale (with the existing 0.001 positive denominator fallback), then applies that dimensionless ratio to the paired axis or both corner axes.
- **対応:** Represent the ratios as `Units::ScaleFactor` and retain before/current/constrained values as `Scale3` through `Artifact3DGizmo::setScale`.
- **価値または懸念（未検証）:** Degree/length values cannot enter the ratio through typed APIs, and the gizmo no longer receives an unnecessary QVector3D round trip. Numerical/interaction parity and compilation remain unverified.
- **次に確認すべきこと:** Compile when authorized; verify Shift edge/corner constraints, Ctrl bypass, nonuniform start scales, and near-zero denominator behavior.



## 2026-09-30 — Projected Frame scale clamp stays in Scale3

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (Projected Frame minimum-dimension scale clamp).
- **確認できた事実:** The gizmo exposes current scale and accepts updated scale as `Scale3`. The controller previously converted to QVector3D for per-axis minimum clamp, then converted back; minimums come from `localBounds()` width/height.
- **対応:** Keep current and clamped scale values as `Scale3` and pass the result directly to the typed setter. Preserve sign, thresholds, and exact XYZ comparison condition.
- **価値または懸念（未検証）:** This removes a redundant interop boundary without changing the local-bounds clamp rule. Compilation and runtime behavior are unverified.
- **次に確認すべきこと:** Compile when authorized; check projected-frame corner/edge scaling clamps at small, normal, and large local bounds.



## 2026-09-30 — Gizmo Undo transform snapshot carries angle and scale units

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionGizmoUndoCommands.cppm` (`GizmoTransformSnapshot`), `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (capture, live property sync, gizmo operations, change detection, Undo restore).
- **確認できた事実:** `GizmoTransformSnapshot` already used `LayerLocalPoint3` for position, but rotation and scale remained QVector3D across keyframe capture, drag updates, and Undo snapshots. Their meanings are degrees and dimensionless axis scales.
- **対応:** Changed those fields to `EulerDegrees3` / `Scale3` and migrated snapshot producers/consumers. Added local component-wise delta/equality helpers to retain the existing thresholds and exact comparisons; unwrap angle values only at legacy float transform/property boundaries.
- **価値または懸念（未検証）:** Undo data now prevents direct interchange of rotation, scale, and spatial vectors. Module compilation and all drag/keyframe/Undo paths remain unverified because build/test execution is restricted by repository instructions.
- **次に確認すべきこと:** When authorized, compile both child modules and verify key capture/restore, single/group gizmo transform, projected-frame scale/rotate, and Undo/Redo including non-3D layers.



## 2026-09-30 — Gizmo snapshot typing reduces friction at shared state

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionGizmoUndoCommands.cppm` (`GizmoTransformSnapshot`), `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (keyframe capture, group/projected transforms, restore).
- **確認できた事実:** Snapshot consumers include keyframe capture/restore, live transform property synchronization, group and single gizmo operations, Projected Frame, and Undo change detection. Rotation/scale meanings are stable across these paths: authored degrees and dimensionless per-axis scale.
- **推測（未検証）:** The degree/scale type conversions were a recurring source of friction because controller-local shared state shed semantic types before it reached legacy APIs. Typing the shared snapshot should reduce repetitive wrappers while keeping interop explicit at the existing float and Qt boundaries.
- **価値または懸念:** Added component helpers preserve existing numeric thresholds and exact equality behavior; however, without module compilation, source-level operator compatibility is not established.
- **次に確認すべきこと:** Compile and check single/group/projected rotate and scale, keyframe restore, and Undo/Redo, including non-3D layers. Consider a small named conversion helper only if further repeated degree unwraps remain after compilation; do not add implicit conversions.



## 2026-09-30 — Anchor Undo snapshots retain layer-local space

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionLayerUndoCommands.cppm` (`AnchorPointUndoCommand`), `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (anchor drag commit and preset/reset operations).
- **確認できた事実:** The command restores anchor and position through `transform3D()` setters. Its three call sites capture these values from transform state or the typed anchor-drag state; they are layer-authored local values, including projected 2D-layer anchors stored in the 3D transform.
- **対応:** Changed command arguments and retained before/after values to `LayerLocalPoint3`. The typed drag path passes points directly; two older QVector3D capture paths explicitly convert at the command boundary.
- **価値または懸念（未検証）:** Undo state cannot be passed a world point without an explicit conversion. The source confirms the authored transform domain, but runtime Undo behavior and module integration have not been verified.
- **次に確認すべきこと:** Compile when authorized and exercise anchor preset/reset, pointer drag, Undo/Redo, and rejected-command rollback on root, parented, and projected 2D layers.



## 2026-09-30 — Rubber Band selection state carries physical screen space

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (selection gesture state, `rubberBandCanvasRect`, mouse press/move).
- **確認できた事実:** Mouse press/move converts Qt logical event positions to physical viewport coordinates by applying widget DPR. The retained start/current positions feed a viewport-to-canvas helper and are shared with selection overlay drawing.
- **対応:** Typed the retained positions as `ScreenPhysicalPoint2`, wrapped existing physical event values with the named helper, and unwrapped only when calling the legacy Qt geometry conversion. Lasso point-list storage and selection behavior were left untouched.
- **価値または懸念（未検証）:** The marquee state now identifies the renderer viewport pixel space. Compilation and HiDPI selection parity remain unverified.
- **次に確認すべきこと:** Compile and exercise rectangle selection at DPR 1 and greater than 1, checking selected layers and overlay alignment; separately review Lasso point-list typing if its geometry contract is established.



## 2026-09-30 — Lasso samples retain physical viewport coordinates

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`lassoViewportPoints_`, polygon conversion, pointer input, overlay drawing).
- **確認できた事実:** Lasso samples are appended from the same DPR-scaled `viewportPos` used by renderer viewport picking. They are converted with `viewportToCanvas` for both selection polygon and overlay, while `QLineF` enforces a 3 physical pixel minimum between stored samples.
- **対応:** Changed the point array to `QVector<ScreenPhysicalPoint2>` and pass typed coordinates through capture and viewport-to-canvas conversion. Kept the Qt line-length calculation behind an explicit QPointF conversion.
- **価値または懸念（未検証）:** The sample array now states the physical viewport space and cannot be confused with canvas-local samples. Existing Qt QVector storage remains in place; replacing it with a project container is a separate ownership/API question and was not part of this change.
- **次に確認すべきこと:** Compile and exercise Lasso selection at multiple DPR values, including sparse and dense pointer movement, then verify overlay alignment and selected layer set.



## 2026-09-30 — Shape vertex marquee state carries physical screen space

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (shape vertex marquee state and `shapeVertexMarqueeCanvasRect`).
- **確認できた事実:** Shape vertex marquee start/current positions are captured from Qt event positions after the existing DPR multiplication and then passed to viewport-to-canvas rect conversion.
- **対応:** Typed both retained positions as `ScreenPhysicalPoint2`, captured with the named physical-point helper, and converted to QPointF only for the legacy rectangle conversion.
- **価値または懸念（未検証）:** The marquee's retained state now carries the same physical viewport space as the renderer and selection input. Compilation and high-DPI vertex selection remain unverified.
- **次に確認すべきこと:** Compile and check shape vertex marquee hit selection and outline alignment at DPR 1 and greater than 1.



## 2026-09-30 — Rig interaction anchors retain layer-local space

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (Rig Weight stroke and Rig Select control/bone drag state), `ArtifactCore/include/Math/Vec.ixx` (LayerLocalPoint2 Qt conversion helpers).
- **確認できた事実:** These pointer positions are computed by mapping a viewport-to-canvas point through the selected rig layer's invertible global-transform inverse. Rig mesh vertices and control/bone local transforms consume the resulting layer-local coordinates.
- **対応:** Typed the retained Rig Weight previous dab and Rig Select drag-start point as `LayerLocalPoint2`. Added explicit QPointF conversions for the existing Qt geometry operations and overlay transform; angle, stroke distance, and influence calculations remain unchanged.
- **価値または懸念（未検証）:** Persistent rig drag state now distinguishes layer-local points from canvas and physical viewport coordinates. Parent-transform interaction and runtime parity are not verified.
- **次に確認すべきこと:** Compile and exercise weight painting, point/slider/angle control dragging, and bone rotation on root and parented rig layers; check overlay brush alignment.



## 2026-09-30 — Viewport overlays and brush release retain physical points

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (Brush release location, context/pie menu anchors, magnifier cursor).
- **確認できた事実:** Context/pie menu setters convert logical pointer positions to physical viewport points; mouse handlers already hold DPR-scaled viewport positions for brush release and magnifier cursor state. These coordinates feed renderer picking or physical overlay layout.
- **対応:** Typed all four retained positions as `ScreenPhysicalPoint2` and kept the existing physical coordinates through their consumer calculations. Removed `pieMenuMousePos_`, which had only assignments and no reads, to avoid duplicate untyped state.
- **価値または懸念（未検証）:** These retained positions now declare the renderer viewport pixel domain, reducing accidental use as logical Qt event positions. Overlay alignment and brush release behavior remain unverified without runtime checks.
- **次に確認すべきこと:** Compile; verify context/pie menu placement and selection, magnifier cursor alignment, and brush release location at DPR 1 and greater than 1.



## 2026-09-30 — Mask drag anchors retain layer-local space

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (mask vertex, handle, and feather/expansion drag start state).
- **確認できた事実:** The pointer coordinate is transformed from canvas into local space using the selected layer's global transform inverse before it is captured. Existing mask vertices and handle calculations use the same local coordinate domain.
- **対応:** Typed the retained vertex, handle, and geometry drag-start points as `LayerLocalPoint2`, using the named QPointF conversion at capture and at legacy geometry operations. Shift-constrained vertex movement, feather distance, and feather/expansion delta formulas are unchanged.
- **価値または懸念（未検証）:** These drag anchors can no longer be accidentally assigned a Composition or viewport point through the typed state. Module compilation and mask edit behavior are unverified.
- **次に確認すべきこと:** Compile and verify vertex/tangent/feather handle drags, Shift-constrained movement, modifier feather/expansion gestures, and Undo/Redo on transformed and parented layers.



## 2026-09-30 — Motion Path positions are in the layer parent frame

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (Motion Path drag start/apply), `ArtifactCore/include/Math/Vec.ixx` (`LayerParentPoint2` / `LayerParentVector2`).
- **確認できた事実:** Motion Path drag maps Composition canvas coordinates through the parent layer's global inverse when a parent exists, then applies the resulting delta to the selected layer's authored transform position keys. Without a parent, the authored root position uses the Composition frame.
- **推測／設計判断:** A dedicated `LayerParent` 2D point represents both cases: nested layer parent-local coordinates or the root's Composition-aligned parent frame. This avoids mislabeling nested authored position as the selected layer's own local geometry.
- **対応:** Added `LayerParentPoint2` / `LayerParentVector2`, named Qt and root-Composition conversion helpers, and compile-time rejection checks. Motion Path apply now accepts a `CompositionPoint2`, keeps drag start/current/delta typed through frame conversion, and unwraps only for existing Qt group geometry.
- **価値または懸念（未検証）:** The state now distinguishes authored parent-frame motion from layer-local mask geometry. Root-frame interpretation and parented group transform parity still require runtime verification; singular parent transforms retain existing reject-on-update behavior.
- **次に確認すべきこと:** Compile and test Motion Path translate/rotate/scale drag for root and nested layers, multi-key selection, undo/redo, and non-invertible parent transforms.



## 2026-09-30 — Motion Path Undo snapshots should preserve parent-frame meaning

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionMotionPathCommands.cppm` (`MotionPathPositionSnapshot`), `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (single and group key capture/restore).
- **確認できた事実:** Snapshot x/y values are read from and written to `transform3D().positionX/YAt` and their keyframe setters. Motion Path drag converts Composition canvas positions through the parent global inverse before applying deltas to these authored transform channels.
- **対応:** Replaced the snapshot's unqualified x/y pair with `LayerParentPoint2` and migrated capture, equality checks, group key snapshots, Undo restore, and drag API conversion. Scalar extraction remains only at the legacy transform key API boundary.
- **価値または懸念（未検証）:** Undo snapshots now share the same space contract as drag start/current points. Compilation and root/nested keyframe round-trip are not verified.
- **次に確認すべきこと:** Compile and verify multi-frame group translate/rotate/scale, interpolation/spatial tangent preservation, Undo/Redo, and root-versus-parented transforms.



## 2026-09-30 — Motion Path group pivot is a double-precision parent-frame point

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`draggingMotionPathGroupPivot_`), `ArtifactCore/include/Math/Vec.ixx` (`LayerParentBoundsPoint2`).
- **確認できた事実:** The group pivot is the average of selected Motion Path keyframe positions, which are authored in the parent frame. Its previous QPointF storage accumulated and divided in `qreal` precision before group rotate/scale geometry.
- **対応:** Retained the pivot as a parent-space bounds point with double coordinates, kept component-wise accumulation/division, and converted to QPointF at the legacy group geometry boundary.
- **価値または懸念（未検証）:** The pivot space is now explicit without reducing its previous double precision. Group gesture parity remains unverified until compilation and runtime checks.
- **次に確認すべきこと:** Compile and verify multi-key group transform pivot and rotate/scale behavior with fractional key positions.



## 2026-09-30 — Coordinate operator rejection checks cover dimension mismatch

- **関連:** ArtifactCore/include/Math/Vec.ixx (Coordinates operators and compile-time contract assertions).
- **確認できた事実:** Point/vector operators are constrained to one Space template parameter and dimension; existing static assertions covered several different-space cases but did not directly encode Composition 2D versus LayerLocal 2D or Composition 2D versus World 3D rejection.
- **対応:** Added compile-time negative-expression checks for these cross-space/cross-dimension operators and implicit conversion checks between Composition, SourcePixel, and ScreenPhysical extents.
- **価値または懸念（未検証）:** The intended rejection contract is now explicit in the module source, but only compiler evaluation can prove it against the active toolchain; builds are not authorized.
- **次に確認すべきこと:** When build verification is authorized, compile Math.Vec and ensure the positive same-space operator checks and new negative checks all evaluate.



## 2026-09-30 — Box Zoom state should match renderer viewport space

- **関連:** Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm (Box Zoom interaction state and overlay).
- **確認できた事実:** Begin/update APIs accept ScreenLogicalPoint2 and multiply by the widget DPR before storing positions. iewportRectToCanvasRect, drag threshold, and overlay consume viewport coordinates, while renderer host dimensions and pan are physical viewport values.
- **対応:** Store start/current positions as ScreenPhysicalPoint2, use the named logical-to-physical point conversion, calculate the drag delta as a same-space typed vector, and unwrap only at legacy Qt geometry/draw boundaries. Convert the fit center back to logical coordinates before calling the zoom anchor API.
- **価値または懸念（未検証）:** This makes the DPI space of persistent interaction state explicit and removes direct QPointF operations from the logical API boundary. Numeric and UI parity are not verified because build/runtime checks are not authorized.
- **次に確認すべきこと:** Compile and exercise Box Zoom at DPR 1 and greater than 1, including tiny drags, crop-window mode, cancellation, and fit center anchoring.



## 2026-09-30 — ArtifactPr の別ドライブ作業コピーに共通Undo問題が残る

- **関連:** `ArtifactPr/src/EditCommand.cppm`、`X:/Dev/ArtifactStudio/ArtifactPr/src/EditCommand.cppm`、`docs/analysis/REPORT_ARTIFACTPR_X_DRIVE_IMPORT_REVIEW_2026-09-30.md`。
- **確認できた事実:** 両コピーのInsert Undoは、挿入時に後続クリップをduration分加算した後、Undo時にも加算する。X側Overwrite/Liftのtail生成はtimeline位置とdurationのみ変更し、source範囲を維持する。
- **価値または懸念:** X側の取り込みだけでは共通Undo問題を解消しない。分割のソース範囲補正も必要になる。
- **未検証:** 操作時のCoreストア投影・Undo同期・速度や逆再生を含む実行結果。ソース修正とビルド・実機確認は未実施。
- **次に確認すべきこと:** J側の編集サービスとUndoへの同期を追い、Insert/Overwrite/Liftの往復とsource範囲を限定修正する。
- **対応（2026-10-01）:** J側の選別移植でInsert Undoの後続位置、Ripple Undoの位置復元、Overwrite/Liftの部分保持とsource範囲補正を実装した。正規NLE経路は既存store snapshot Undoを維持する。新規tailの旧link groupへの自動加入は避けており、split後のリンク編集方針は将来の設計確認対象。コンパイル・実機確認は未実施。



## 2026-09-30 — 前後フレーム依存エフェクトの基盤（調査とA/B/C実装）

- **関連:** `Artifact/include/Effects/ArtifactEffectFrameSampler.ixx`、`src/Effects/ArtifactEffectFrameSampler.cppm`、`include/Render/ArtifactRenderLayerPipeline.ixx`、`src/Render/ArtifactRenderLayerPipeline.cppm`、`include/Effects/ArtifactAbstractEffect.ixx`、`src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認できた事実（実ファイルで確認）:** 既に約20個の時制ラスタライザエフェクト（Echo / Ghost / Feedback / FrameBlend / TimeWarp / OpticalFlowBlur 等）が `IEffectFrameSampler::sampleCurrentLayerFrameRelative` を使っており、基盤は緑地ではない。`storeLayerFrame` は CPU ラスタライザ経路の1箇所のみ。履歴は `unordered_map<layerId, map<frame, image>>` で64フレーム上限、evict は「最小フレーム番号の線形走査」。`ImageF32x4RGBAWithCache::operator=` は `DeepCopy()` なのでサンプルごとに全画像コピー。invalidation フックは grep で0件。`TemporalHistoryRegistry`（`ArtifactCore/include/Graphics/TemporalHistory.ixx`）は `CameraCut` / `TimeDiscontinuity` 等の無効化理由を型で持つが**呼び出し元ゼロの死にコード**。`CreativeEffectManager` / `CreativeEffectContext` も `applyAll` 无人呼び出しで未接続。
- **対応:** (A) サンプラに revision 追跡（`effectRevision()` で実装パラメータ/内容を識別）、挿入順 `std::list` による O(1) evict、バイト予算（既定256MB）、`invalidateIfRevisionChanged` / `invalidateLayer` / `invalidateAll` を追加し、controller の store 前に revision 無効化と discontinuity 全無効化を配線。(B) `GpuSpatialEffectNode` に `historyFrameOffset` / `historyValid` を追加し、generic resident shader に `g_HistoryTexture`（t1）と `g_HistoryValid`（uniform）を追加。`RenderPipeline` に固定長8スロットの ping-pong 履歴プール（`recordLayerFrame` / `layerHistoryView` / `invalidateLayerHistory` / `invalidateAllLayerHistory`）を追加し、GPU→GPU copy のみで readback なし。(C) render tick で連続性を判定し、不連続時は無効化後に**1フレームだけ** pre-roll（N-1 を描いて復元）して N が N-1 をサンプルできるようにした。
- **価値または懸念（未検証）:** 時制エフェクトが `historyFrameOffset` を設定するだけで GPU 経路でも効くが、**まだ1つも設定していない**（既存エフェクトは `supportsGPU()==false` の CPU 実装のみ）。C の pre-roll は 1 フレーム固定なので、2フレーム以上戻る effect（TimeBlur 等）は 2段目以降がまだ空。履歴は深度1の ping-pong のため GPU 側は ±2 以上の参照は未対応。`LayerFrameHistory::eraseFrame` の `order` 走査は O(n)（evict 1回あたり）。全編未ビルド・未実機（AGENTS.md により明示指示なし）。`invalidateIfRevisionChanged` は毎フレーム vector 確保するため、HOT_PATH_RULES §1 に対して発生条件を明示して計測が必要。
- **次に確認すべきこと:** (1) ビルドして `ArtifactEffectFrameSampler` と pipeline の new API が通るか (2) Echo を 1 本 generic resident shader（`g_HistoryTexture` 読み）へ移して GPU 経路で works するか (3) スクラブ/逆再生で pre-roll が 1 フレーム埋めるか (4) エフェクトパラメータ変更直後に履歴が破棄されるか (5) 履歴プールの 8 スロット枯渇時に CPU フォールバックが正しく効くか (6) `invalidateIfRevisionChanged` の毎フレーム vector 確保を `SmallVector` / 固定バッファへ置き換えるべき計測。
- **次に確認すべきこと:** (1) ビルドして `ArtifactEffectFrameSampler` と pipeline の new API が通るか (2) Echo を 1 本 generic resident shader（`g_HistoryTexture` 読み）へ移して GPU 経路で works するか (3) スクラブ/逆再生で pre-roll が 1 フレーム埋めるか (4) エフェクトパラメータ変更直後に履歴が破棄されるか (5) 履歴プールの 8 スロット枯渇時に CPU フォールバックが正しく効くか (6) `invalidateIfRevisionChanged` の毎フレーム vector 確保を `SmallVector` / 固定バッファへ置き換えるべき計測。



## 2026-09-30 — 独自コンテナの基盤整備（std/Qt 内部依存の解消）

- **2026-10-01 追記（IFC 更新順序）:** NamedVector の IFC 出力失敗 C3474 後、ScriptRuntime / EventBus が修正前の `make()` 本体で C2440 を報告した。実ファイルは添字参照へ修正済みだが IFC は旧時刻だった。生成済み Ninja では両利用側が NamedVector の生成オブジェクトに依存していなかった。共通処理で手動 NamedVector / NameMap / ArtifactString / ArtifactDict 参照に対応する生成オブジェクトへの依存を追加した。更新中の IFC と読者の競合は原因候補であり、失敗時のロック所有者は未確認。ビルド停止後の IFC 排他読み取りは可能、NamedVector の生成ルールは1件だった。次のビルドで IFC 更新成功と古いテンプレート診断の解消を確認する。
- **2026-10-01 追記（NameMap / NamedVector）:** NameGenerator のログから `NameMap` 利用時の `Core.ArtifactHashMap` IFC 参照不足を確認し、共通補完処理に NameMap / ArtifactDict → HashMap / Optional の依存を追加した。EventBus の `snapshot.make()` は `NamedVector::make()` のインスタンス化で失敗していた。内部 `Array::last()` は Optional を返すため、追加前の要素数を添字にして追加した要素の参照を返すよう修正した。`make()` の公開戻り型と診断カウンタは維持。差分は静的確認済み、コンパイルでの解消は未検証。
- **2026-10-01 追記（IFC 依存）:** `NamedVector` と `ArtifactString` は `Core.ArtifactArray`、その `Array` は `Core.ArtifactOptional` を import する。手動 `/reference` を使う実装ではテンプレート利用時にこの依存も必要であり、PythonEngine / ProjectDiagnostic / ColorLUT / FrameRange に続き AnimatableTransform2D / FluidSolver2D / FluidVisualizer のログでも Array 参照不足を確認した。`ArtifactCore/CMakeLists.txt` の末尾で既存の手動参照を持つ実装へ Array / Optional の IFC 参照と生成順序を補完する共通処理に変更した。個別リストの保守漏れを防ぐ意図であり、実ビルドでの解消は未検証。次は生成されたコンパイル引数と該当実装のビルド結果を確認する。
- **関連:** `ArtifactCore/include/Core/ArtifactSet.ixx`、`src/Core/ArtifactHashMap.cppm`、`include/Core/ArtifactDict.ixx`、`include/Core/ArtifactFoundation.ixx`、`include/Container/NamedVector.ixx`、`include/Container/NameMap.ixx`、`include/Container/SmallVector.ixx`、ルート直下の `replace_std.py` / `rollout_import_std.py` / `replace_flags.py`。
- **確認できた事実（実ファイルで確認）:** `ArtifactFoundation.ixx:4` は「No std dependency. No Qt dependency.」と宣言していたが、`ArtifactDict` が `QHash`/`QMap`、`ArtifactQueue` が `QQueue<T>` を使っていた。`ArtifactHashMap::at()` は `std::out_of_range` を送出しており同設計方針（「例外を使わない」）と乖離していた。`NamedVector` は内部メンバが `std::vector<T> values_`、`NameMap` は `std::map<K,V> values_`、`SmallVector` は `std::vector<ContainerMutationRecord> mutationHistory_` で、「独自コンテナ」の名に反して内部が std 実装だった。`replace_std.py` は `import std;` を削除して代わりに `#include` を module 宣言の直後（＝purview）に挿入するため、`AGENTS.md:139` が禁じる操作そのものを実行する。`MILESTONES_BACKLOG.md` の M-AR-2 は `import std;` 導入済みと記録していたが実コードは 0 件だった。
- **対応:** `ArtifactQueue.ixx` を削除し CMake 登録も除去（実使用 0 件）。`ArtifactSet` に `insert`/`erase`/`find`（`Optional` 返却）、`Iterator::operator++(int)`、deep copy と `swap` を実装しコピー禁止を解消、`Core.ArtifactFoundation` に `export import` を追加。`ArtifactHashMap::at()` を `Optional` 返却へ変更し `findValue` / `const find` / `const_iterator` を追加、`std::unique_ptr` を生配列所有に置換。`ArtifactDict` を `QHash`/`QMap` から `ArtifactHashMap` へ移行。`NamedVector`/`NameMap`/`SmallVector` の内部メンバを `Array`/`ArtifactHashMap` へ置換。ルートの一時スクリプト3本を削除。追加確認で`ArtifactHashMap`定義の`ArtifactCore` namespaceが非exportだったため、`ArtifactDict`からテンプレートを参照できない状態と分かり、namespace exportを追加した。
- **価値または懸念（未検証）:** 内部 std/Qt 依存が解消し「独自コンテナ」の名称が実態と一致した。`ArtifactHashMap.cppm` と `ArtifactDict.ixx` の個別コンパイルは成功したが、Core全体のビルドとテストは未確認。`ArtifactHashMap` の `const_iterator` は `iterator` の別名であり const 正確性は `const_cast` で担保しているため、厳密な const イテレータではない。`ArtifactOptional` の `value()` は依然 `std::bad_optional_access` を送出する（`ArtifactOptional.cppm:118,123,128,212,257`）ため、例外排除は HashMap のみに適用され全体では未完了。`std::pair` は `Core.ArtifactTuple` への置換可能性があるが依存増加のため残置。
- **次に確認すべきこと:** (1) `ArtifactCore` をビルドして新規テスト（`ArtifactFoundationTest.cpp` の `ArtifactSetTest.StdCompatibleSpellingsAndCopySemantics` / `ArtifactHashMapTest.AtReturnsOptionalInsteadOfThrowing` / `ArtifactDictTest.SafeDictionaryRoundTrip`）を通す (2) `NameGenerator.cppm:57-60` の `NameMap` 使用箇所が挙動不変か確認する (3) `ContainerDebugText.ixx:60,200` の `NameMap` デバッグ出力が順序変化しないか確認する (4) `ArtifactOptional::value()` の例外を排除するか方針を決める (5) `NameMap` の順序性が必要になった場合に備え、`ArtifactOrderedMap` として順序付きコンテナを再設計する (6) `IdMap.ixx:449` は `std::unordered_map<K,V>` のままで残した。`ArtifactHashMap` へ置換するには K 型が `std::hash` を持つ必要があり、`LayerID` のように持たない型が実在するため（`Insight.md` の C1001 記録と関連）。独自 Hasher（`ArtifactUtility.ixx:106` の `artifactHashCombine` 相当）の導入可否を別途判断する (7) `ContainerDebug.ixx:104,163,164` と `ContainerDebugRegistry.ixx:237` も `std::vector` を保持している (8) `NamedList` の `std::list` を `Array` へ置換したため itr 安定性が失われた。既存コードが itr を保持したまま `removeAt` を呼ぶ破壊的用法がないか確認する。



## 2026-09-29 — StrongType の摩擦は暗黙変換ではなく局所的な例外手続きで下げる

- **関連:** `docs/planned/MILESTONE_TYPED_COORDINATE_SPACES_2026-09-27.md`、`ArtifactCore/include/Math/Vec.ixx`、Camera Layer / StereoCamera の距離単位。
- **確認できた事実:** StrongType移行ではQt/glm/scalar API境界で明示的な値変換が必要になり、移行範囲外の局所計算まで型で覆うと反復的な包み直しが増える。一方、camera IPDは`m`、camera位置・clip距離は`px`表記であり、XR資料には両者を結ぶワールド単位換算が記されていない。
- **判断・対応:** 摩擦低減は、意味が切り替わる境界に名前付き変換を集約し、型付き値は意味が保たれるAPI間で維持する方法に限定する。暗黙変換・異種型演算を認める場合は、具体的な摩擦、失う検査範囲、代替案、拒否条件を記録した設計判断を必要とする方針をマイルストーンへ追加した。
- **価値または懸念（未検証）:** 移行時の記述量を減らしつつ、単位・空間の取り違え防止を保てる。camera pxとIPD mの数値スケール関係は未確定で、StereoCameraのeye offset式へ単位換算を導入すると表示値を変える可能性がある。
- **次に確認すべきこと:** camera transform / 3D sceneのワールド長さ規約を仕様・保存データ・実行経路から確認し、pxとmの関係が確定するまで変換係数や距離補正を導入しない。



## 2026-09-29 — StereoCamera の IPD も camera property と同じ Meter 契約にする

- **関連:** `ArtifactCore/include/Math/Vec.ixx`、`ArtifactCore/include/Transform/Camera.ixx`、`Artifact/include/Layer/ArtifactCameraLayer.ixx`、stereo viewport setup。
- **確認できた事実:** camera Inspector はIPDを`m`単位として表示し、値は`StereoCamera::fromHmd`の左右eye offsetへそのまま使われる。fromHmdの直接callerはComposition Render Controllerのcurrent/previous camera stereo setupだけ。
- **対応:** `Units::Meters`を追加し、ArtifactCameraLayerのIPD getter/setter、`StereoCamera::ipd`保持値、`fromHmd`引数を型付け。Property/JSON境界で数値を明示し、rendererからeye matrix構築までは型付きで渡す。eye offsetでは`.value`を使用。Meters/Millimeters間の暗黙変換禁止をstatic_assertで固定。
- **価値または懸念（未検証）:** IPDをmmの焦点距離やカメラpx量として誤って渡せない。`Core.Camera`が`Math.Vec`をimportする依存変更を含み、module compile・stereo eye offset runtimeは未確認。
- **次に確認すべきこと:** ビルド許可後にCore.Camera module dependencyとIPD 0.064 m時の左右eye offset符号・半距離、JSON/Inspector往復を確認する。



## 2026-09-29 — Camera の px 距離値が layer/dialog/stereo/DOF 境界で float のままだった

- **関連:** `ArtifactCore/include/Math/Vec.ixx`、`ArtifactCore/include/Transform/Camera.ixx`、Camera Layer、Create Camera dialog、Composition render controller。
- **確認できた事実:** Camera Layer Inspectorはzoom、ortho width/height、near/far clip、focus distanceをpxと表示する。Create Camera dialogのzoomもpxコメントを持つ。near/far値はStereoCameraへ渡る。2026-09-30に投影経路を再確認し、`ArtifactCameraLayer::projectionMatrix`はnear/farをQt perspective/orthoのcamera-space depth引数に渡し、frustum guideも各値を距離としてFOVから平面寸法を計算することを確認した。focus distance/zoom由来focal lengthはCameraDOFParameters経由でDOF設定に入る。
- **対応:** `Units::Pixels`を追加し、これらのcamera layer getter/setter、dialog zoom/focus getters、CameraDOFParameters距離フィールド、StereoCamera near/far引数・stateを型付け。property/JSONとprojection/renderer float境界だけで明示的に数値化した。
- **価値または懸念（未検証）:** 異なる単位の焦点距離(mm)、IPD(m)、px距離をcamera/stereo/DOF APIへ取り違えて渡せない。ただしnear/farのpx表示・型契約とprojectionが要求するcamera-space depthの意味関係はコードから確定できず、Camera Positionとの数値スケールも未文書化。単位名をWorldLengthへ変更すると保存値の意味を誤って断定する可能性がある。Camera/Math.Vec module依存と全projection/DOF pathのコンパイル・runtimeは未確認。
- **次に確認すべきこと:** ビルド許可後にCamera/Dialog/Camera Core module依存、camera creation、property/JSON往復、near/far clip、focus clamp、stereoとDOF parameter値が旧値と一致するか確認する。near/far pxとcamera-space depthのスケールは設計資料または既存シーンの期待値で確定してから型名を選ぶ。



## 2026-09-29 — Camera FOV UI/API が degree 値を生floatで受け渡していた

- **関連:** `Artifact/include/Layer/ArtifactCameraLayer.ixx`、`Artifact/src/Layer/ArtifactCameraLayer.cppm`、Composition Editor の camera creation、camera property restore。
- **確認できた事実:** `ArtifactCameraLayer::setFov(float fovDegrees)` は度数前提でclampし、焦点距離からの逆変換、PropertyGroup復元、Create Camera dialogの値を受けていた。Artifact内の直接呼び出しはこれら3経路に限られる。
- **対応:** cameraのFOV getter/setterとCreate Camera dialog getterを`ArtifactCore::Units::Degrees`にし、UIからcamera setterまで型を保つ。焦点距離計算とproperty復元でdegree値を明示、float描画・projection・property境界では明示的に値を取り出す。Camera/Dialog interface/implementationから`Math.Vec`をimportするmodule dependency変更を伴う。
- **価値または懸念（未検証）:** radian値など誤単位の入力をsetter境界で拒否する。module graph、property restore、dialog作成後のcamera projectionはコンパイル・実機確認前。
- **次に確認すべきこと:** ビルド許可後、Camera/Dialog module dependency、Create Camera dialog、JSON/property restoration、焦点距離更新後のFOV clampとfrustum/overlay表示を確認する。



## 2026-09-29 — Focal length は mm と表示されるが Camera API は float だった

- **関連:** `ArtifactCore/include/Math/Vec.ixx`、`Artifact/include/Layer/ArtifactCameraLayer.ixx`、`Artifact/include/Widgets/Dialog/CreateCameraLayerDialog.ixx`、Camera/Dialog implementation。
- **確認できた事実:** Inspector propertyは focal length unit `mm` を設定し、Create Camera dialogのgetterコメントもmmを明記している。camera setter/getter、dialog getter、property restore、camera menuの間では数値floatを直接受け渡していた。
- **対応:** `Units::Millimeters`を追加し、cameraとdialog getter/setter境界を型付けした。PropertyGroup/renderer scalar境界のみ`.value`へ明示変換。mm↔degreesの変換式は従来どおりで、Units間の暗黙変換禁止をstatic_assertに追加。
- **価値または懸念（未検証）:** `Millimeters`の焦点距離を`Degrees`や他のscalarとして誤って渡せない。Camera/Dialogのmodule依存、property復元、focal-length/FOV同期はビルド・実機未確認。
- **次に確認すべきこと:** 許可後にダイアログ入力からcamera生成、Inspector focal length編集、保存/再読込、overlayの数値表示を確認する。



## 2026-09-29 — Rig bone rotation nudge のdegree deltaを型で固定

- **関連:** `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、controller implementation、Composition Editor key handling。
- **確認できた事実:** `nudgeSelectedRigBoneRotation(float deltaDegrees)` は引数名で度数を明記し、editorからの呼び出しは±15.0fだけ。値は RigBone local transform のfloat rotationへ加算され、Undo commandへ渡る。
- **対応:** 公開境界を`ArtifactCore::Units::Degrees`へ変更し、editor call siteで単位を明示。legacy rig transformへ書く一点のみ`delta.value`を使用。編集・Undo経路や角度値は変えていない。
- **価値または懸念（未検証）:** 将来radian deltaや無次元値を誤って渡す呼び出しをコンパイル時に拒否する。モジュールコンパイル・キー操作確認は未実施。
- **次に確認すべきこと:** ビルド許可後、Artifact.Widgets.CompositionRenderControllerのmodule依存解決とE/Shift+Eでの回転方向・Undoを確認する。



## 2026-09-29 — Screen logical→physical 変換は点とvectorを別overloadにする

- **関連:** `ArtifactCore/include/Math/Vec.ixx` の `ScreenLogicalPoint2` / `ScreenLogicalVector2`、Composition viewport overlay、Interactive Render Region hit-test。
- **確認できた事実:** DPR変換は点座標だけでなくhit radiusやpointer deltaのような差分にも使う。どちらも数値としては倍率だが、PointとVectorは加法・原点依存の意味が異なる。
- **対応:** `toScreenPhysical` にlogical vector用overloadを追加し、IRR hit radius変換でpoint型を仮利用せずvector型を使う。
- **価値または懸念:** StrongTypeの空間区別に加えてpoint/vector意味も保つ。DPR scaling自体のruntime確認は未実施。
- **次に確認すべきこと:** DPR変換を使う他のradius/delta経路で、pointをvectorとして誤用していないか段階的に確認する。



## 2026-09-29 — Interactive Render Region のdrag座標stateが開始時だけphysicalだった

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。
- **確認できた事実:** IRR rectはrenderer canvas空間にあり、hit-testはpan/zoom後の物理viewport rectとQt mouse logical pointを直接比較していた。drag開始ではpointにDPRを掛けてstateへ保存したものの、メンバ名はlogical風で、更新時にはlogical current pointとの差を取る前に保存値を再度DPRで割っていた。drag deltaにはその後 zoom を一度掛けてcanvas rectへ適用していた。
- **対応:** hit-test / begin / update APIを`ScreenLogicalPoint2`、開始位置stateを同じlogical pointへ統一し、logical deltaを型付きvectorとして計算。hit-test比較点と既存の12 logical px hit radiusだけを、rendererのphysical viewport rectと比較する箇所でDPR変換した。zoom係数・ハンドルID・最小rect寸法・更新式は維持。
- **価値または懸念（未検証）:** 座標状態の単位混在を除き、DPR>1のhandle hit-testが期待どおりになる見込み。IRR rect/canvas自体の単位は部分レンダー側と描画側の既存契約に依存し、今回は変更していない。コンパイル・実機確認未実施。
- **次に確認すべきこと:** 許可後にDPR 1.0/1.5/2.0でIRR内移動、8 handle resize、cancel後の復元を操作し、zoom/pan併用時のdelta parityを確認する。



## 2026-09-29 — Composition viewport menu overlay API で logical/physical 座標契約が混在していた

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`ArtifactCore/include/Math/Vec.ixx`。
- **確認できた事実:** controller の host 幅・高さと renderer viewport は QWidget logical size に devicePixelRatio を掛けて保持される。menu rect生成と overlay drawing はその寸法を利用する一方、show/update/hit-test APIは無型の `QPointF` を受け取り、editor caller は Qt logical mouse/cursor position を渡していた。context/pie位置はrect配置・item hit-testの双方でoverlay寸法と直接比較されていた。
- **対応:** StrongType API 境界にlogical pointを要求し、controller内部のoverlay位置・hit-testをphysical pointへ変更。名前付き `toScreenPhysical(point, DPR)` でcontroller入口のみ変換し、各Qt call siteも明示変換に変更した。rect sizing・フォント寸法は変更していない。
- **価値または懸念（未検証）:** HiDPIでmenu位置・hover/selection hit-testがscale不整合になる経路を型とDPR境界で閉じた。既存寸法計算は固定物理値とQt font metricsが混在している可能性があり、メニュー見た目の正しいDPI scalingは未確認。コンパイル・実画面確認は未実施。
- **次に確認すべきこと:** ビルド・実画面確認が許可された後、DPR 1.0/1.5/2.0でcontext/pie menu位置・項目hover/選択と寸法を確認し、寸法スケーリングは別契約として判断する。



## 2026-09-29 — StrongType: Projected Frameの物理viewport点・bounds

- **関連:** `ArtifactCore/include/Math/Vec.ixx`、`ArtifactCore/cmake/ArtifactCoreSources.cmake`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`docs/planned/MILESTONE_TYPED_COORDINATE_SPACES_2026-09-27.md`。
- **確認できた事実:** `Math.Vec` は型定義を持つが、ArtifactCoreの明示ソースmanifestには記載がなく、Artifact controllerからのimportもなかった。Projected Frame interior hit-testの3呼び出し元は、mouse event位置にDPRを乗算した物理viewport座標を渡していた。projection済みframe cornersも同じphysical viewport内の値である。
- **対応:** manifestへ `Vec.ixx` を追加登録し、controller実装から `Math.Vec` をimport。corner/interior/selection-frame hit-test引数、projected corner/guide配列、scale dragの開始・固定・handle点、snap helperの現在／前回pointer・戻り値と保持state、過去frame plane corner配列とcenter hit-test入力を `ScreenPhysicalPoint2` にした。複数選択frame boundsはdouble精度を保持する `ScreenPhysicalBounds2` wrapperにし、selection hit-testとscale fixed-point helperへ物理screen boundsを要求させる。点差分はtyped physical vectorから倍率・距離を計算し、Qt clipping / drawing / ray picking境界だけ明示変換。DPRと幾何式は変えない。
- **適用基準の調整:** StrongTypeは空間・単位が失われる関数境界と共有stateで使う。同一関数内の幾何計算や純粋なglm演算を一律に包まず、移行境界の明示変換も意味が変わる箇所へ集約する。`Math.Vec.ixx` の既存コメントもこの役割分担に合わせた。
- **Phase 2の実装:** `Artifact3DGizmo` のhitTest / beginDrag / constrainDrag / updateDrag API入力を`WorldRay`（`WorldPoint3`原点＋`WorldVector3`方向）に変更し、controllerで既存rayから明示変換する。ギズモ内部の計算式・カメラ対・交差判定は従来どおり。モジュール依存と実コンパイルは未検証。
- **Phase 3の実装:** `CompositionRenderController::createPickingRay`の入力を`ScreenPhysicalPoint2`に変更し、既存全call siteの物理px値を名前付きhelper経由で渡す。DPR適用箇所とunproject式は保持。実コンパイル・高DPI確認は未実施。
- **角度単位の実装:** `Math.Vec`に異なる`Degrees` / `Radians`値型と名前付き相互変換を追加。Artifact3DGizmoのrotation-ring開始角、角度差、degrees snap increment、公開Euler set/get APIを`EulerDegrees3`で型付けした。controllerのlegacy state/layer vector境界でのみ明示unwrapする。実コンパイル・rotation parity確認は未実施。
- **型契約の固定:** `Math.Vec` module内に非export concept / `static_assert`を追加し、同空間point/vector演算の許可、異空間point差とpoint+pointの拒否、degrees+radians演算・暗黙相互変換の拒否をコンパイル時条件にした。これらがコンパイラで評価された証拠はビルド未実施のためまだない。
- **3D gizmo basisの強型化:** `setLocalBasis` / `setViewBasis`の軸は、global transformまたはinverse viewから作られたworld-space方向だった。公開引数と`dragAxisDirection()`戻り値を`WorldVector3`に変更し、既存Qt演算との境界だけ名前付き変換する。
- **Picking ray生成境界:** `createPickingRay`は物理画面点をunprojectしてworld near-pointとnormalized world directionを生成する。戻り値を`WorldRay`とし、gizmo操作は直接受け取る。従来RayのQt-vector演算を使うmodel/camera/past-frame picking箇所に限り、名前付きhelperで明示unwrapする。unprojection式と行列選択に変更はない（compile/runtime未確認）。
- **論理画面点のlayer picking:** `layerAtViewportPos`はQtマウス／drop位置のlogical pxを受け、内部でDPRを掛けて物理hit-testを行うAPIだった。入力を`ScreenLogicalPoint2`へ変え、Qtイベントからの名前付き変換をcontroller/editorの全有効call siteに入れた。関数内のDPR変換と選択動作は維持（compile/runtime未確認）。**精度メモ:** Qt `qreal`から既存座標値型の`float`へ一度狭めてからDPRを掛ける。通常のviewport寸法では誤差は1px未満だが、subpixel判定は実機で未確認。
- **Navigationの論理画面点:** zoom anchor、Box Zoom begin/update、Tumble Pivotを`ScreenLogicalPoint2` APIにした。Qtイベント境界に名前付き変換を置き、DPR適用はcontroller内部に留めた。Box Zoomはstart/currentを物理px保存する一方、終了時に物理centerを論理zoom APIへ渡し、同APIでDPRを再適用していた。終了時centerをDPRで割って論理点に戻してから渡すよう修正（DPR=1不変、HiDPI runtime未確認）。
- **Camera focus / projected-frame reset:** 両APIは論理viewport点からDPRを適用してphysical hit-testを行うため、引数を`ScreenLogicalPoint2`へ変更し、double-click event境界で名前付き変換。ray値・corner/interior hit-test式は変更していない（compile/runtime未確認）。
- **Gizmo hover:** `isTransformGizmoHovered`は論理画面点をconstruction handlesだけDPR変換し、Text/2D gizmoには論理点のまま渡す。APIを`ScreenLogicalPoint2`にし、Qt legacy APIへ戻す境界を局所化（compile/runtime未確認）。
- **Magnifier wheel boundary:** `adjustMagnifierScaleAt`の旧引数名・実装はlogical pxとDPR変換を明示していたため、`ScreenLogicalPoint2`化してWheelイベントから名前付き変換。loupe hit-test式は維持（compile/runtime未確認）。
- **Work Cursor placement:** 呼び出し元はQt logical eventまたはQWidget中心点だが、controllerは値をそのまま`viewportToCanvas`へ渡していた。renderer viewport/panのphysical px契約に合わせ、logical point型と明示DPR適用を導入。DPR=1不変、HiDPI位置の改善は式からの推論でruntime未確認。
- **確認できた事実:** `ArtifactAbstractLayer::position3D()`はlocal transform channelを直接返し、`getGlobalTransform4x4()`はparent transformを再帰合成する。controllerのgizmo同期はglobal matrixからbasisを作りつつ、positionには`position3D()`を渡す経路がある。Artifact3DGizmoのray hit testは`WorldRay`とgizmo positionを同じ交差計算で比較する。
- **追加確認 (2026-09-30):** model pickingは`Artifact3DLayer::transform3D().snapshotAt()`から単体model matrixを組み立てる一方、`ArtifactAbstractLayer::getGlobalTransform4x4At()`は親行列を再帰合成する。rayはviewport cameraから生成したWorldRayであるため、親付きmodelのtriangle pointをWorldPoint3と断定したり、空間タグだけを付けて安全とみなしたりできない。software render queueにも単体snapshot model matrix経路があるが、GPU render/picking parityとの関係は未検証。
- **追加確認:** `handleMouseMove`ではgizmo positionがドラッグ差分、軸制約、Undo前状態、単一・複数選択のTransform書き戻しに使われる。同期時のpositionだけworld originへ変更すると、ドラッグ計算の値域まで変わる。
- **価値または懸念（runtime未検証）:** parent transform下でlocal positionとworld ray/basisが混ざる可能性がある。StrongType化すると不整合を型境界として可視化できるが、正しい位置変換を入れると既存interaction挙動を変えるため、現在のタスクでは修正していない。`setTransform` / position APIの型設計前にparented layer契約を確認する。
- **価値または懸念（未検証）:** 物理viewport点以外を関数へ渡す誤りはcompile-timeで拒否される設計になる。一方、明示manifestとmodule dependencyの実コンパイルは未確認であり、依存解決およびMSVC module integrationはビルド許可後に確認が必要。
- **次に確認すべきこと:** 変更箇所は型境界と直接計算を静的に照合する。次はProjected Frame以外の高リスク座標境界を棚卸しし、合成空間と画面pxを混同し得る関数入出力を優先する。3D pickingはmesh-local→parent/world transformの契約とrenderer経路の整合を先に確認する。ビルド・テストはユーザーの明示指示後に限る。



## 2026-09-29 — Viewport の線はアンチエイリアスされていない（thick-line PS がパススルー、AA は 3D 限定）

- **関連:** `Artifact/include/Render/ThickLineShaders.ixx`（`g_thickLinePS`）、`Artifact/src/Render/PrimitiveRenderer2D.cppm`（`drawThickLineLocal` 809-831、`drawBezierLocal` 921-945、`drawCircle` 962-985、`drawCrosshair` 1002-1013）、`Artifact/src/Render/ArtifactIRenderer.cppm`（`drawPolyline` 1696-1702）、`Artifact/src/Render/ShaderManager.cppm`（thickLine PSO 1206-1233、コメント 808-809）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（MSAA/FXAA ゲート 41520-41527, 41987-41990、ブラシカーソル 32 分割ポリライン 48131-48146）、`Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm:907-914`。
- **確認できた事実（静的読み取り）:** 直線・ポリライン・ベジェ・円・クロスヘアはすべて `drawThickLineLocal` のクアッドに集約され、`g_thickLinePS` は `return input.color;` のパススルーで**エッジAAが無い**。`ShaderManager.cppm:808-809` の「edge-antialias PS logic」コメントは実装と不一致。MSAA 4x は `antiAliasingMode==2 && layer->is3D()` のみ、FXAA は `antiAliasingMode==1 && hasVisible3DLayer` のみに適用され、**2D コンポジションとオーバーレイはどの設定でも AA されない**。設定 UI の説明も "for 3D content"。
- **対応（提案・未実装）:** (1) 最推奨：`drawThickLineLocal` のクアッドを幅+~1px で張り、VS で線幅方向の正規化オフセットと半幅を PS へ渡して `smoothstep` でカバレッジを出す（`ThickLineShaders.ixx` + `PrimitiveRenderer2D.cppm` + ShaderManager の InputLayout）。全線に一括で効き、2D 画像/テキストの画素は不変。(2) FXAA の `hasVisible3DLayer` ゲートをオプション化（2D テキストのエッジは甘くなる）。(3) ビューポート全体を MSAA 化（既存 `createOffscreenMultisampleTexture` を最終合成＋オーバーレイへ適用して resolve）。
- **価値または懸念（未検証）:** 実機の見た目差・性能影響は未計測。thick-line PS のコメント不一致の経緯は未確認。
- **次に確認すべきこと:** 対策(1)の実装と、実機での線品質およびフレーム時間の比較。ビルド・実機は AGENTS.md によりユーザー明示指示が必要。



## 2026-09-29 — VP オーバーレイの性能：アイドルは停止、操作中は毎フレーム再構築

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（render tick 17408-17532、`drawViewportOverlayPass` 45601-、`drawViewportUiOverlay` 49813-、`updateColorSamplerOverlay` 46836-46881、`captureCurrentFrameImage` 22752-22796、`drawViewportMagnifierOverlay` 47006-、ブラシ HUD 48167-48206）、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`（`submit` 915-、Quad/Line/Tri/Sprite バッチ 1001-1134、`kSolidRectBatchValidated=false` 950）、`Artifact/src/Render/ArtifactIRenderer.cppm`（`readbackTextureViewToImage` 2149-（fence wait + staging Map の同期読み戻し）、`present` 3417-）。
- **確認できた事実（静的読み取り）:** (1) レンダーティックは `renderDirty_` が false かつ非操作なら停止する（17486-17494）。**アイドル時のオーバーレイ再構築コストはゼロ。** (2) 操作中（pan/zoom/ギズモ/ブラシ等）は毎フレーム `drawViewportOverlayPass` が走り、ギズモ・ガイド・変形フィールド・オニオンスキン・モーションパス・HUD を毎回コマンド化する。オーバーレイ専用プロファイラスコープ `ProfileScope("Overlay")` が 45617 にある。 (3) オーバーレイの線・四角・三角・スプライトはバッチ化される（`flushQuadBatch` 等）が、非バッチ packet（Sprite/SolidRect/SolidCircle 等）でバッチが切れる設計。SolidRect 一括バッチは `kSolidRectBatchValidated=false` で無効。 (4) **カラーサンプラ有効時の `updateColorSamplerOverlay` はマウス移動ごとに `captureCurrentFrameImage()` → `readbackTextureViewToImage()`（全画面 GPU→CPU 同期読み戻し、fence wait + Map）を呼ぶ。** 非同期版 `requestCurrentFrameImageAsync`（22798-）が既に存在する。 (5) 拡大鏡は `lastPresentedReadbackSRV_` の GPU ブランチのみで CPU 読み戻しなし。 (6) チャンネルオーバーレイの `composeViewportChannelOverlayImage()` は色表示時は早期 return で QImage を作らない。
- **価値または懸念（未検証）:** 「操作中のみ毎フレーム再構築」なので、高頻度入力（ブラシ/トラッキング/多数のギズモ）やカラーサンプラ有効時にボトルネック化しうる。実測は未実施。**カラーサンプラの同期読み戻しはヒッチの有力候補**（既定 OFF）。最有力の是正は非同期読み戻しへの置換と、オーバーレイ再構築の間引き。
- **次に確認すべきこと:** 実機で Profiler パネルの "Overlay"/"Submit2D" と `FrameCostStats`（drawCall / bufferUpdates / PSO スイッチ数）を、① 静止 ② pan/zoom ③ ギズモドラッグ ④ ブラシ ⑤ カラーサンプラ ON で計測・比較する。ビルド・実機は AGENTS.md によりユーザー明示指示が必要。



## 2026-09-29 — SolidRect バッチの「clear-only」は原因未特定。実行時フラグで切り分け可能にした

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm`（`solidRectBatchDebugFlag` 109-120、フラグ読み出し 979-988、`batchReady` 989-992、ready 失敗ログ 994-1004、`flushSolidRectBatch` ログ 1006-1021、indirect 分岐 1026-1028）、`Artifact/src/Render/ShaderManager.cppm`（AA 版 PSO 922-972、非AA版 870-919）、`Artifact/include/Render/DiligentImmediateSubmitter.ixx:180-187`、`libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h:487-493, 3376`。
- **訂正した誤情報:** 前回「`DrawIndexedIndirect` の args レイアウトが Diligent 契約と不一致なので clear-only の候補」と主張したが**誤り**。Diligent の `DrawIndexedIndirectAttribs` は `NumIndices, NumInstances, FirstIndexLocation, BaseVertex, FirstInstanceLocation` の5個を要求し（`DeviceContext.h:487-493`）、現行 `args = {count*6, 1, 0, 0, 0}` は**一致する**。当該仮説は撤回する。「`MapBuffer` は deferred context で不可」も誤り（`DeviceContext.h:3376` で graphics context 対応明記。Line/Quad/Sprite バッチも同一方式で稼働中）。また調査時に親リポジトリで `git log -S` を実行し空の結果を得たが、`Artifact/` は gitlink のため無効だった（子リポジトリで再実行したら squash 1件のみ anyways、経緯は復元不可）。
- **確認できた事実（静的読み取り）:** (1) submitter が使うのは **AA 版 PSO**（`ShaderManager.cppm:1630-1632` → `:922-925` の pos/color/uv 3属性、PS は `fwidth(uv)` エッジAA）。頂点 `BatchRectVertexAA{pos,color,uv}`=32B（`DiligentImmediateSubmitter.ixx:180`）、IB は rect あたり6インデックスの quad 配列（`.cppm:667-683`）、入力レイアウトも一致。**AA 版に構造的欠陥は見つからない。** (2) 同じ関数 `createBatchSolidRectPSOs` が作る **非AA版（pos/color 2属性、`ShaderManager.cppm:870-873`）は生成されるだけでどこからも参照されていない**（`batchSolidRectPsoAndSrb()` 呼び出し 0 件、submitter は AA 版のみ受領 `.cppm:765`）。死んだ PSO。 (3) 無効化コメントは「clear-only を静的レビューで再現できず」と自ら述べており、**防御的無効化**。真の原因は未特定。
- **対応:** `constexpr bool kSolidRectBatchValidated = false` を実行時環境変数へ置換し、依存する2変数を分離可能にした。`ARTIFACT_SOLIDRECT_BATCH`（既定 1、batching 自体 on/off）、`ARTIFACT_SOLIDRECT_INDIRECT`（既定 **0**、64個以上 run の `DrawIndexedIndirect` を許可するか＝切り分け先用）、`ARTIFACT_SOLIDRECT_VERBOSE`（既定 0、submit 1回だけ `[SolidRectBatch] flush count=/vbSize=/ibSize=/pso=` と `[SolidRectBatch] batchEnabled but not ready:` を出力）。ホットパスルール（`docs/technical/HOT_PATH_RULES.md`）に従い **verbose は opt-in かつ submit 1回だけ**（`solidRectBatchLogBudget`）とし、無効時は文字列整形も `qWarning` も発生しない。
- **価値または懸念（未検証）:** 切り分け手順は ① `BATCH=0` で従来挙動（回帰なし）を確認 → ② `BATCH=1 INDIRECT=0 VERBOSE=1` で直接 draw が clear になるか → ③ direct で問題なければ `INDIRECT=1` が真犯人。**実機未実行**（ビルド/実行はユーザー指示待ち）。回帰が出た場合は AA 版 PSO ではなく RTV format・ブレンド・`fwidth` の swapchain パス成立性を疑うべきだが、**これも未検証**。死んだ非AA版 PSO は**削除していない**（回帰比較の余地を残すため）。
- **次に確認すべきこと:** 上記①②③を実機で順番に実行し、`count` が 0 でないこと・`pso=ok` であることをログで確認。回帰が出たら `frameDebugPasses()`（Present/Readback pass）を併用し、batch draw が swapchain 以外のどこへ飛んでいるか追跡する。ビルド・実機は AGENTS.md によりユーザー明示指示が必要。



## 2026-09-29 — 起動設定は exe 同梱 JSON にする。settings.cbor とは別物だった

- **関連:** `ArtifactCore/include/Configuration/LayeredConfigStore.ixx`（`importSystemJson` 59-65）、`ArtifactCore/src/Configuration/LayeredConfigStore.cppm`（`System` を writable 化 50-63、`importSystemJson` 285-333）、`ArtifactCore/src/Application/ArtifactAppSettings.cppm`（`Render/SolidRect*` 登録 202-208）、`Artifact/src/AppMain.cppm`（起動読込 5159-5178）、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`（`solidRectBatchDebugFlag` 95-131、呼び出し 991-996）、`Artifact/ArtifactStartup.template.json`。
- **訂正した誤情報:** 「既存の設定は exe 同梱 JSON に書き込む仕組みがある接点」と前提にしたが**誤り**。`LayeredConfigStore` の User レイヤは `QStandardPaths::AppDataLocation` 配下の **`settings.cbor`（`FastSettingsStore` = CBOR バイナリ）**で、JSON reader はどこにも存在しない（`LayeredConfigStore.cppm:81-83`、`FastSettingsStore.cppm:88-111` は QCborMap のみ）。`importLayer` も `FastSettingsStore::open` 経由なので JSON を読めない。JSON 経路は**新規実装**であり既存設定の移行ではない。
- **確認できた事実:** `System` レイヤは `writable=false`（`:54`）かつパス指定不可で、`setValue(ConfigLayer::System, ...)` が必ず失敗していた。`ConfigSchema::applyDefaultsToLayer` も `System` 限定（`ConfigSchema.cppm:90-92`）だが `setDefaultValue` 経由で別の経路のため今回の変更の影響を受けない。
- **対応:** ① `System` を writable 化（メモリ専用なので書き込み可はプロセス内のみ。`saveLayer` は path 無し store の no-op のままで永続化しない）。② `LayeredConfigStore::importSystemJson(path)` を追加。JSON を読み、ネストしたオブジェクトを `Group/Name` にフラット化（`{"Render":{"X":1}}` → `Render/X`）して `System` へ入れる。`_` 始まりキーはドキュメント用として無視。配列は `QVariantList` として入れる。③ `ArtifactStartup.json`（exe と同階層）を起動時に読み込み（`AppMain.cppm:5159-5178`、`QApplication` 構築後・`ArtifactAppSettings::instance()` 初回取得前）。④ スキーマに `Render/SolidRectBatch`（既定 true）／`Render/SolidRectIndirect`（既定 false）／`Render/SolidRectVerbose`（既定 false）を登録。⑤ フラグ解決順を **JSON(config) > 環境変数 > 既定** にした。
- **価値または懸念（未検証）:** 設定の入口が exe 同梱 JSON になり、IDE/Explorer/パッケージ起動から環境変数なしで操作できる。**注意点: `ConfigSchema::validate` は「未登録キーを拒否」するが、バリデータは `ArtifactAppSettings::Impl` 構築時にしか設定されない（`ArtifactAppSettings.cppm:213-218`）。起動読込は `AppSettings` 初期化より前なので、JSON の型ミスは拒否されず素通りする。** ビルド・実機未実行。JSON に書いたキーが実際に効くかは未確認。
- **次に確認すべきこと:** `ArtifactStartup.json` を exe 側に置いて `SolidRectVerbose: true` → 起動ログに `[SolidRectBatch] flush count=...` が出るか、`SolidRectBatch: false` → 非バッチ経路に戻るかを確認。**型不正を弾くなら、バリデータを `LayeredConfigStore` 側へ移すか、起動読込を `AppSettings` 初期化後へ移す必要があり、設計判断を要する。** ビルド・実機は AGENTS.md によりユーザー明示指示が必要。



## 2026-09-29 — 起動設定仕様を docs/technical/STARTUP_FLAGS_CONTRACT_2026-09-29.md に固定

- **関連:** `docs/technical/STARTUP_FLAGS_CONTRACT_2026-09-29.md`（新規）、`AGENTS.md`（起動設定ルールを追記）、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`（[StartupFlags] 警告 112-146）。
- **確認できた事実（静的読み取り）:** 実行時 env 変数として実際に `qEnvironmentVariable` / `getenv` で読まれているものは **36 個**（`ARTIFACT_RENDER_BACKEND`、`ARTIFACT_GPU_ADAPTER`、`ARTIFACT_XPU*` 全11個、`ARTIFACT_SOLIDRECT_*` 新規3個、`ARTIFACT_ENABLE_*` 群、`ARTIFACT_RUN_*_TESTS` 群 等）。一方 `ARTIFACT_HAS_ANGELSCRIPT` / `ARTIFACT_WITH_OPENUSD` / `ARTIFACT_RESTORE_QT_EMIT_MACRO` / `ARTIFACT_BUILD_GIT_HASH` 等は grep で拾えるが **実環境変数ではなく CMake `target_compile_definitions`**（`ArtifactCore/CMakeLists.txt:4660` 等）であり、移行対象外。**ビルド時マクロと起動時フラグを混同すると誤った移行対象を作る。**
- **対応:** 仕様書を作成し、AGENTS.md に条文を追加。確定事項は (1) 起動時 1 回だけ読む（ファイル監視・再読み込みなし）(2) 優先順位 **JSON > 環境変数 > 既定**、両方に同じキーがある場合は `[StartupFlags]` 警告を 1 回だけ出す（**実装済み**）(3) キーは `Group/Name`、ネストは `/` 平坦化、`_` 始まりは無視 (4) `settings.cbor` とは別系統で、保存先は変えない (5) ホットパスで毎フレーム設定を読み直さない。移行は SolidRect → GPU 系 → 診断系 → XPU 系 → テスト起動の 5 段階に分割。
- **価値または懸念（未検証）:** 入口が exe 同梱 JSON に 1 本化され、IDE/Explorer/パッケージ起動から環境変数なしで操作できる。**残る未確定は型検証。起動読込は `AppSettings` 初期化より前なので `ConfigSchema::validate` を通らず、型ミスは静かに既定値になる。** ビルド・実機未実行。
- **次に確認すべきこと:** (1) 契約 §9 の検証手順を実機で通す。(2) 型ガードを厳密にするなら、リーダを `AppSettings` 初期化後へ移すか（起動は 1 回なので影響は小さい）、`LayeredConfigStore` 側にバリデータを置くか（責務が Core へ広がる）を選び、実装する。(3) 移行対象を検査スクリプトに載せられるか検討。ビルド・実機は AGENTS.md によりユーザー明示指示が必要。



## 2026-09-29 — 平面レイヤーのアンカードラッグを 3D gizmo の AnchorPoint モードで実装（A案）

- **関連:** `Artifact/include/Widgets/Render/Artifact3DGizmo.ixx`（`GizmoMode::AnchorPoint` / `GizmoAxis::Anchor` / `GizmoOperation::Anchor` / `anchorDragDelta()`）、`Artifact/src/Widgets/Render/Artifact3DGizmo.cppm`（`anchorHandleWorldRadius` 581-582、`operationForMode` 427、`hitTest` 809-829、`beginDrag` 1149-1156、`updateDrag` 1313-1323、`endDrag`+アクセサ 1821-1832、`draw` 1937-1975）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`setGizmoMode` 18149-18155、3D/2D の setTransform 16019-16033 / 16096-16111、press 基準値捕捉 28701-28720、move 適用 30961-30970、write-back 除外 33116-33123、`applyProjectedFrameAnchorDelta` 24801-、`commitProjectedFrameAnchorDrag` 24863-）、`Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx:514-517`、`Artifact/src/Widgets/Render/ArtifactCompositionLayerUndoCommands.cppm:64-74`。
- **確認できた事実:** アンカー機能は既に存在した（`ToolType::AnchorPoint`、既定キー `Y` = AE と同じ、`TransformGizmo::Mode::AnchorPoint` の描画/hitTest/ドラッグ、Undo コマンドまで）。**平面系レイヤー（画像/ソリッド/3D Plane）は `layerUsesProjectedFrameGizmo() == true` のため、2026-09-27  所有権整理で入れた `legacy2DGizmoShouldOwnPress`（`:21191-21197`）が false を返し、2D gizmo の press パス（`:28683-28686`）に到達しなかった。** つまり「`Y` でアンカーをドラッグ」は平面系で機能していなかった。加えて**アンカー適用関数 `resetSelected3DAnchorToCenter`（`:24795-`）が `!layer->is3D()` を要求していた**ため、2D レイヤーにはアンカー書き込み経路そのものが存在しなかった。
- **対応（A案、所有権の一貫性を崩さない）:** アンカーを 3D gizmo の単独モードとして実装。① `GizmoMode::AnchorPoint` / `GizmoAxis::Anchor` / `GizmoOperation::Anchor` を追加。② `hitTest` は AnchorPoint モードのとき冒頭で分岐し、軸/平面/BoundingBox の全判定を迂回して単一点のみ拾う。③ `draw` は AnchorPoint のとき `draw3DQuad`/`draw3DLine` で十字だけを描いて早期 return（`draw3DPoint` は renderer に存在しないため、中心ドットは quad で代用）。④ `beginDrag` はカメラ平行面上での平面ドラッグを予約し、`updateDrag` は `dragStartHitPoint` 基準の**絶対オフセット**（累積しないのでポインタサンプル取りこぼしでドリフトしない）を `anchorDragDelta` に公開。⑤ コントローラは press 時に元の anchor/position を捕捉し、move で **press 時基準値**から毎回書き直す（read-modify-write の累積を避ける）。⑥ 位置は anchor 移動と逆向きに補償するので、アkoa画像そのものは動かない。⑦ Undo は move 毎ではなく release 時に 1 回だけ push し、`GizmoTransformUndoCommand` との二重登録を除外。⑧ `AnchorPointUndoCommand::apply` の `!layer->is3D()` ガードを撤去（2D レイヤーも transform3D anchor を持つため必要）。
- **訂正した誤情報:** 最初に「Undo を毎フレーム push すれば既存パターンに乗る」と考えたが、`applyProjectedFrameAnchorDelta` は mouse-move ごとに呼ばれるため、毎フレーム push すると Undo スタックがサンプル数だけ肥大する。これは確実に誤りなので release 専用 `commitProjectedFrameAnchorDrag()` を分離した。また `anchorDragDelta` を `position += delta` の累積形式にしたのも誤りで、`dragStartHitPoint` 基準の絶対オフセットに修正した。
- **価値または懸念（未検証）:** 平面系で `Y` → アンカー表示 → ドラッグできる。所有権は 3D gizmo に一本化され、2026-09-27 の整理と矛盾しない。**ビルド・実機は未実行。** 残る未確認: (a) 2D レイヤーで `transform3D().setPositionZ()` を呼ぶ副作用（`applyPlanarGizmoTransform` は 2D で positionZ を触らない設計）(b) `QTransform::map(QPointF)` による world→local 変換が親 Transform を含むケースの正確性 (c) anchor の z 軸成分を維持すべきか。
- **次に確認すべきこと:** 実機で (1) 画像/ソリッド/3D Plane それぞれで `Y` → アンカーがlayer アンカー位置に表示されるか (2) ドラッグすると画像が動きつつアンカーだけ追従するか（Anchor Overrides / Set Mask Anchor 相当）(3) Undo/Redo が 1 回で元に戻り、Redo で再適用されるか (4) 親子 Transform 入れ子の下でアンカーが正确的 locally 動くか (5) 縦横スケール・回転状態でアンカーが正确的 locally 動くか。ビルド・実機は AGENTS.md によりユーザー明示指示が必要。



## 2026-09-29 — マスクトラッキング（Planar Track をマスクのベジェ頂点へ適用）を実装

- **関連:** `Artifact/src/Tool/ArtifactPointTrackerTool.cppm`（`applyPlanarResultAsMask` 390-、homography helper 33-110、`import Artifact.Mask.LayerMask` / `Artifact.Mask.Path` 25-26）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`trackerApplyToMask` 37687-、Corner Pin 実装 37659- への着火）、`Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx:799-801`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:4607-4610`（トラッカパネルのメニュー項目）。
- **確認できた事実（静的読み取り）:** マスクトラッキングは**完全に欠落**していた。トラッカの適用先は Position / Anchor / AllPoints / Corner Pin の4種のみで、`maskTrack|trackMask|trackerApplyToMask` |**symbols ゼロ**。一方前提となるインフラは揃っていた: `MaskPath::setAnimationKeyframe` / `sampleAtFrame`（補間込み、`MaskPath.cppm:329-` / `406-450`）、`MotionTracker::projectRegionAt`（単一時刻の4隅投影homography、`MotionTracker.ixx:303-304`）、`MaskEditCommand`（`UndoManager.ixx:618-622`）。したがって既存構造に乗り只需要「homography をマスク頂点へ適用する1関数」を足すだけで成立した。
- **設計判断（重要）:** AE のマスクトラッキングは点トラッキングの延長ではなく、**平面トラッキングの homography でマスク（shape）を毎フレーム変形させる**もの。よって `projectRegionAt` で得た毎フレーム4隅と、変換元4隅から homography を解き、全ベジェ頂点を通す設計にした。平行移動だけでなく回転・スケール・スキューも反映される点が point 方式との差。
- **実装:** (1) anonymous namespace に `solveHomography4Point`（8x8 線形系 + 部分ピボット付き掃き出し法、degenerate/共線なら false）と `applyHomography3x3`（w≈0 や非有限で null QPointF）を追加。(2) `applyPlanarResultAsMask(comp, tracker, sourceRect, targetLayer, maskIndex, pathPathIndex)` を追加。(3) **全フレーム先に解いてから初めて layer を触る**（途中で失敗しても「半分追跡されたマスク」が残らない。かつ `MaskPath` は欠けたフレームを跨いで補間してしまうため、1フレームでも失敗したらそのフレームを捨てる）。(4) **タンジェントは方向ベクトルとして扱う**: 絶対点ではなく `position + inTangent` を写して差分を取る（投影行列でオフセット自体を変換するのは誤り）。(5) Undo は既存の `MaskEditCommand(layer, beforeMasks, afterMasks)` を使い、スタックが拒否したら **fail-closed でマスクを復元**。
- **訂正した誤情報:** 実装過程で「`SetLayerMaskCommand` を使う」と考えたが**このクラスは存在しない**（grep 0 件）。正しくは `MaskEditCommand`。また最初のドラフトでは homography をフレーム開始時に1回だけ解いて全フレームに使い回す構造と書いたが、**各フレームの tracked quad が違う**ため破綻する（`projectRegionAt` は時刻依存）。最終版は毎フレーム解く。另外 `applyHomography` と `applyHomography3x3`、`solveUnitQuadHomography` と `solveHomography4Point` という未定義ヘルパの二重定義になったドラフトも書き直した。
- **価値または懸念（未検証）:** 4点平面トラッキングとCorner Pinが既に動いていたので、既存構造から素直に導ける機能。**ビルド・実機は未実行。** 未確認: (a) `projectRegionAt` の返す4隅の頂点順序（source と target の対応順が同じ保証があるか。AE の 4点登録は TL,TR,BR,BL 順が慣習だが、コード側の保証は未確認）(b) マスクが親 Transform を持つ場合の layer-local 変換 (c) 数千頂点のマスクで毎フレーム homography 解いて/keyframe 積む計算量。**`ArtifactStartPoint` 系の既存 UI がないため、対象マスクは現状「選択レイヤーの第1マスク・第1パス」に固定**している（マスク選択 UI は後続）。
- **次に確認すべきこと:** 実機で (1) 平面トラッキング実行後に「Apply Planar Track to Mask」でマスクが毎フレーム追従するか (2) Undo が1回で完全に復元され Redo が再適用されるか (3) 回転・スケールを含むEnumで追従するか (4) `projectRegionAt` の頂点順序が想定と異なっていないか（ずれた場合は変換が破綻するので最初に見るべき）。ビルド・実機は AGENTS.md によりユーザー明示指示が必要。



## 2026-09-29 — 調整レイヤー（Adjustment Layer）の GPU パス欠陥を修正：マスク/不透明度を「効果の範囲指定」として扱う

- **関連:** `ArtifactCore/include/Render/PointwiseEffectFusion.ixx`（`requiresMaskMix` 86/98、CompileKey 107、`originalResource`/`maskResource` 130-133、`kMaskMixParameterSlot`/`kMaxNodeParameterSlot` 170-172、シェーダ生成 418-421/453-457）、`ArtifactCore/include/Graphics/Shader/Compute/LayerBlendPipeline.ixx:185-192`、`ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`（`applyPointwise` 344-365、variables[8] 384-415、mask バインド 478-484）、`Artifact/include/Render/ArtifactRenderLayerPipeline.ixx:82-97`、`Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`（`adjustmentOriginal_` 1093-1096、createTextures 2686-2694、applyPointwise 2119-2185）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`renderAdjustmentMaskToImage` 3811-3907、`uploadOpaqueAdjustmentMask` 3909-3934、veto 撤去 11792-11838、opacity パラメータ 11973-11978、スロット溢れガード 12148-12153、mixView 引き渡し 12154-12170）。
- **確認できた事実（静的読み取り）:** 調整レイヤーの GPU パスは `drawGpuLayerToIntermediate` の分岐内で `pointwiseApplied` の時点で `return` し、`prepareGpuLayerForBlend` / `blendGpuLayerIntoAccum` に**到達しない**ため、**ブレンドモードと不透明度が無言で無視されていた**。さらに `:11799`（旧）で `if (layer->hasMasks() || |opacity-1|>1e-6) canApplyPointwise = false;` により、**マスクを付けるだけでエフェクトが完全に消え**、コメント自身が「until an explicit adjustment mask is available」と未実装を認めていた。transform も同様に無効化されていた。
- **根本原因の理解:** AE の調整レイヤーのマスクは**結果をCCEする作用ではなく、効果の適用範囲（.where）を指定する**もの。既存実装は「マスクを画素に掛ける」扱い（`LayerMask::applyToImage` が RGB を乗算）だったため，即使 Fallback に落ちると grade 前の画素を暗くしてしまう、という二重の問題があった。
- **実装（mix 方式）:** pointwise シェーダに `OriginalTexture`（調整前の accum）と `MaskTexture` を追加し、最終段で `color = lerp(original, color, saturate(mask * opacity))` する形にした。これで (a) マスクは「適用範囲」として働く (b) 不透明度は纯粹なフェードになる (c) **ブレンドモードは依然未対応**（下の懸念）。不透明度のみのレイヤーは不透明な白マスクを 1 枚アップロードして mix に流すことで、毎フレームの新規確保を避ける。
- **設計判断（_parameter slot の予約）:** 不透明度はテクスチャではなく**予約パラメータスロット 63**（`kMaskMixParameterSlot`）で渡す。效应ノードが 62 を超えると予約スロットを踏むため、`kMaxNodeParameterSlot` を上限として明示的に検出して Fallback する。シェーダは `Parameters[63].x` を読む。
- **aliasing 対策:** `applyPointwise` は `accum_` を `temp_` と swap するため、**調整前の accum は上書きされる前にスナップショットが必要**。専用 `adjustmentOriginal_` TextureBundle（RGBA16F、composition 寸法）を無条件確保し、pass 前に `CopyTexture` でコピーする。複数 segment があっても 1 枚で全 segment の参照になる。
- **訂正（調査エージェントの報告）:** サブエージェントが挙げた「操作中（スクラブ/ズーム）はエフェクトが消える」（欠陥 #6）は**誤り**。`pointwiseApplied` の時点で既に `return` するため、pointwise 経路は操作中でも GPU 上で生き残る（`ArtifactCompositionRenderController.cppm:12186-12192`）。影響するのは pointwise へ落ちなかった場合（CPU fallback）のみ。
- **訂正した誤情報:** 実装途中で `ArtifactIRenderer::instance()` で device context を取得できると思い込んだが、**このシングルトンは存在しない**（grep 0 件）。`uploadOpaqueAdjustmentMask` は context を引数で受け取る形に改めた。また `CopyTextureAttribs` の使い方（`pSrcBox`/`pSrcTexture` ポインタ形式）も既存コード（`ArtifactIRenderer.cppm:5290-5300`）を確認して修正した。
- **価値または懸念（未検証）:** 調整レイヤー＋マスク（最も一般的なワークフロー）が GPU パスで機能するようになる。**残る未解決/未実装:** (a) **ブレンドモードは依然として無視される**（`blendGpuLayerIntoAccum` を bypass したままだため。Screen/Multiply は未対応）。(b) マスク描画 `renderAdjustmentMaskToImage` は毎フレーム CPU（cv::Mat + QImage）。ただし `LayerMask::compositeAlphaMask` を再利用する形に書き換えてあり、MaskMode / inverted / feather / expansion / アニメ済みパスサンプリングを正しく反映する（自作 QPainter fill 版はこれらを黙って落とすため却下）。既存 `MaskCutoutPipeline`（GPU マスク合成）が遅延初期化（`QTimer::singleShot(1500)`）で調整レイヤー経路から利用できない。(c) 8-bit マスクの alpha を mix 係数に使うため、feather/8bit 精度の界限で微妙なにじみが出る可能性がある。(d) マスクの `inverted`/`expansion`/`feather` の完全な反映は未検証。
- **次に確認すべきこと:** 実機で (1) 調整レイヤー+マスクでエフェクトがマスク范围内だけ効くか (2) 不透明度 50% で正しくフェードするか (3) マスク的铁菱（feather）で滑らかにつながるか (4) 画面遷移while（スクラブ/ズーム）でも生效するか（interaction 中の早期 return がまだあるか要確認）。ビルド・実機は AGENTS.md によりユーザー明示指示が必要。




## 2026-09-29 — 調整レイヤーの残項目2件を修正：ブレンドモード対応とマスクラスタライズのキャッシュ

- **関連:** `Artifact/include/Render/ArtifactRenderLayerPipeline.ixx`（`foldAdjustmentBlend` 99-112）、`Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`（`adjustmentBlend_` 1097-1100、createTextures 2734-2739、destroy 1231、`foldAdjustmentBlend` 2196-2231）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`AdjustmentMaskCacheKey` 3811-3826、`adjustmentMaskContentHash` 3828-3866、キャッシュメンバ 10935-10938、キャッシュ付きマスク生成 12001-12023、ブレンド呼び出し 12157-12168）、`ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx:244-264`。
- **確認できた事実（静的読み取り）:** ブレンドシェーダの演算は `blended = BlendFunc(dst.rgb, src.rgb)` で、`LayerBlendPipeline::blend(src, dst, out, mode, opacity)` は「src を前景、dst を背景」として合成する。調整レイヤーは pointwise が accum をその場で書き換えてしまったため、**ブレンド関数を適用する余地がどこにも無く** Screen/Multiply 等が常に no-op になっていた。既存の36種シェーダはそのまま再利用でき、**新しいシェーダは不要**だった。
- **実装（ブレンド）:** pointwise の後に**2段目の折り畳みパス**を追加。`adjustmentOriginal_`（調整前スナップショット）を背景、`accum_`（調整後）を前景として `blend` し、専用の第3スクラッチ `adjustmentBlend_` に書き出してから accum へ copy back する。`RenderPipeline::foldAdjustmentBlend()` として専用 API にしたのは、テクスチャ詳細を controller に露出させず aliasing 事故を避けるため。**当初 `temp_` を入出力の両方に使う実装を書いて aliasing バグを作りかけた**ので、専用ターゲットを追加してやり直した。`mode == Normal` は即 true を返し、不要なパスは増やさない。
- **実装（マスクの CPU コスト）:** `MaskCutoutPipeline` も QImage 入力で GPU ラスタライズではなく（しかも scene 適用用の API である）。よって「GPU 化」ではなく**毎フレーム再生成の排除**が正しい解釈。`AdjustmentMaskCacheKey`（layerId / frame / width / height / 頂点内容ハッシュ）でキャッシュし、**同一キーでは CPU の fillPoly を一切走らせない**。パン・ギズモホバー・アイドルの再描画がすべてこの経路を通るため、ここが実際に効く。キーの指紋には頂点座標・接線・feather・expansion・mode・inverted・パス数を含めたので、キーフレームを含むいかなる変更でも cache miss になる（false hit を出さないため）。
- **訂正した誤情報:** 実装途中で `hasBlendPipeline()` / `blendPipeline()` / `adjustmentOriginalSRV()` / `tempRef()` / `contextPtr` 等の**存在しない API を書き込んでしまった**。`RenderPipeline` に実在するのは `accumSRV/accumUAV/tempSRV/tempUAV/accumRTV` と `swapAccumAndTemp()` のみで、ブレンド変換は `ArtifactCore::toBlendMode(layer->layerBlendType())` だった。最終的には `foldAdjustmentBlend` に集約して解消している。
- **価値または懸念（未検証）:** 調整レイヤーの主要3点（マスク範囲・不透明度・ブレンドモード）が GPU パスで機能する。**残る未確認:** (a) 36種すべての blend 関数が調整レイヤー文脈で正しい結果になるか（Screen/Multiply は代表として確認したいが未実測）。(b) `blend` の alpha 合成式（`src.a + dst.a*(1-src.a)`）は通常レイヤー前提であり、調整レイヤーは実質 src.a=1 相当になるため、**完全透明の蓄積領域で意図しない alpha 変化が起きる可能性**があり未検証。(c) マスクラスタライズは依然として CPU だが、キャッシュヒット時はゼロコストになった。
- **次に確認すべきこと:** 実機で (1) 調整レイヤーの blend mode を Screen / Multiply / Overlay / Add にして正しい結果になるか (2) マスクと不透明度が引き続き機能するか（回帰なし）(3) パン中の CPU 負荷が改善したか (4) マスク頂点を動かした直後にキャッシュが invalid されて即反映されるか。ビルド・実機は AGENTS.md によりユーザー明示指示が必要。
