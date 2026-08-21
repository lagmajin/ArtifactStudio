# MILESTONE: オーディオ・ルーティング深度の充実（Group/Return/Sidechain/VCA/Pre-Fader Send）

> 2026-08-20 作成

**最終更新:** 2026-08-20
**Status:** Phase 0 実装済み、Phase 2（Pre/Post-Fader send）実装済み、Phase 3（VCA Core/UI）実装済み、runtime検証待ち
**Priority:** Medium
**Related:** `docs/analysis/AUDIO_TEXT_ANIM_AUDIT_2026-08-02.md`, `docs/bugs/AUDIO_PLAYBACK_SYSTEM_ISSUES_2026-03-27.md`, `docs/analysis/ASSET_PROJECT_OTHER_AUDIT_2026-08-02.md`

---

## 1. 目的

本プロジェクトのオーディオ・ミキサーは `AudioMixer` / `AudioBus` に基づく**グラフ型ルーティング**を既に備えている。監査（`AUDIO_TEXT_ANIM_AUDIT_2026-08-02.md`）は「1 レイヤー = 1 バス（🟢80%）」と評価していたが、実際の実装はそれより進んでいる:

- プライマリ経路 `routing`（source → 1 target）＋ aux/sidechain 送信 `sends`（source → 多 target）
- `AudioBusKind::{Master,Layer,Group,Return}` による階層（layer→group→master は primary チェインで可）
- トポロジカルソート・サイクル検出・レイテンシ補正・グラフ単位ソロ
- `AudioBus::process` がエフェクトへ `&sideChainBuffer_` を渡し、`AudioCompressor` が sidechain を消費 → **aux send → Return バス → コンプ/リバーブの SC** は実働

しかし DAW（Reaper/Cubase/Live）並みの「ルーティングの深さ」には、以下の構造的欠落がある。本マイルストーンはこれを埋める。

---

## 2. フェーズ構成

### Phase 0: ミキサー・グラフの常時活性化（activation gating 解消）
- 現状、`AudioMixer::process` は `ArtifactAbstractComposition::getAudio` から実際に呼ばれている（runtime 動作確認済み: `:3184`）が、`impl_->audioMixer_` が非 null の場合のみ。
- `ensureAudioMixer()` は `ArtifactAudioService::syncCurrentComposition` 経由で**ミキサーパネルを開いた時**、またはプロジェクトに `audioMixer` キーがあり `deserialize` された時（`ArtifactAbstractComposition.cppm:4724`）にしか呼ばれない。
- 結果: パネルを開かず、かつ mixer データを含まない構成では `getAudio` がフラットな旧 direct-sum 経路（`:3194` 以降）へフォールバックし、group/send/sidechain が一切効かない。
- 修正: 構成に音声レイヤーがあり再生される際は `ensureAudioMixer()` を既定で呼ぶ（再生エンジン初期化／`ArtifactAudioService` 側）。これが Phase 1〜3 の前提。

### Phase 1: Sidechain 経路の単一化（デッドコード撤去）
- `AudioBus` の `sidechainSource_`（バス名 API：`setSidechainSource`/`getSidechainSource`）は書き込み＋シリアライズのみで、**実音声へは一切接続されていない**（読み出し箇所なし）。実際の sidechain 供給は `AudioMixer::addSideChainSend` のみ。
- `sidechainSource_` を `sends` 経路へ統合（API 廃止または `setSidechainSource(name)` を `addSideChainSend(findBusByName(name), 1.0f)` の糖衣構文にする）。シリアライズ互換は `sends` 側へ寄せる。
- 影響: `AudioBus.cppm:99,535-552`、UI の sidechain 扱い。

### Phase 2: Pre/Post-Fader センド切替
- 現在のセンドは常に **post-fader**（`AudioBus::process` の volume/pan 適用後）で供給される。
- 各 `SideChainSend` に `preFader` フラグを追加。`preFader` 時は `AudioBus` のエフェクト適用前（またはフェーダ前）のバッファを送信元とする。
- UI: SC メニューに「Pre/Post」切替を追加（`AudioMixerWidget.cppm:417`）。

### Phase 3: VCA / フォルダ・ゲイングループ
- 複数バスの音量を「音声の再親付け（primary 経路の変更）なし」で一括制御する VCA グループを追加。
- `AudioBusKind::Vca` を追加、`AudioMixer::process` で VCA メンバの gain に VCA バスの volume を乗算（音声は通さずゲインのみ）。
- ソロ／ミュートと同様にグラフ単位で解決。UI: ストリップの「VCA 所属」選択。

### Phase 4: マルチ出力／ハードウェア経路（将来拡張）
- 現在は Master 1 本 → `AudioRenderer` のみ。Surround51/71 レイアウトはあるが個別アウトへは非ルーティング。
- Master の出力先を複数（ハードウェア端点／ファイル）へ振り分ける `AudioOutputTarget` を追加。ASIO 実装（🟡30%）とセットで扱う。

### Phase 5: プラグイン／MIDI 経路（別レイヤーの大きな穴）
- 本マイルストーンの「ミキサー・グラフ」外だが、DAW 格差の本体。`VST3Host 🟡50%` / `CLAPHost 🟡40%` の実用化と、MIDI/オートメーション経路の追加は別マイルストーン（`MILESTONE_AUDIO_PLUGIN_MIDI_ROUTING` 候補）で扱う。

---

## 3. 実装対象ファイル

- `ArtifactCore/src/Audio/AudioMixer.cppm`（routing/sends/process: `:68-72`, `:558`, `:609`）
- `ArtifactCore/include/Audio/AudioMixer.ixx`（`SideChainSend`, `AudioBusKind`, `AudioRoutingResult`）
- `ArtifactCore/src/Audio/AudioBus.cppm`（process/sidechain: `:303-310`, `:535-552`）
- `ArtifactCore/include/Audio/AudioBus.ixx`（sidechainSource API の統合）
- `ArtifactCore/include/Audio/AudioCompressor.ixx`（sidechain 消費確認）
- `Artifact/src/Widgets/AudioMixerWidget.cppm`（route コンボ・SC メニュー: `:400`, `:417`, `:785`）
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm`（`ensureLayerBus` のみの自動生成をグループ作成ヘルパへ拡張: `:3131`）

---

## 4. 成功条件 (Done Criteria)

0. ミキサーパネルを開かなくても、音声構成の再生で `AudioMixer` グラフが既定で有効（フラット経路へフォールバックしない）。
1. バス名 `sidechainSource_` API を使わずとも、`addSideChainSend` のみで sidechain がエフェクトへ届く（既存動作維持）＋ デッドコードが排除されている。
2. 各センドで Pre/Post-Fader を切り替え、Pre 時はフェーダ変更の影響を受けずに送信される。
3. VCA グループを作成し、所属バスの音量が VCA フェーダのみで一括制御できる（音声の再親付けなし）。
4. 既存のシリアライズ（`AudioMixer::serialize/deserialize`）が Phase 1〜3 の拡張後も旧プロジェクト互換。
5. サイクル検出・レイテンシ補正・グラフ単位ソロが拡張後も正しく動作する。

---

## 5. 実装監査 (2026-08-20)

### Phase 0 実装 (2026-08-20)

- `ArtifactAbstractComposition::getAudio()` を音声評価の mixer 活性化ポイントに変更。
- `audioMixer` の保存データや Audio Mixer パネルを事前に経由しなくても、音声を持つ composition は `AudioMixer` グラフ経路を使用する。
- これにより playback / scrub / render queue の `getAudio()` 呼び出しが、同じ graph-based mixing 経路へ入る。

### Pre/Post-Fader send 実装 (2026-08-20)

- `AudioBus` が FX 後・フェーダ前の `preFaderBuffer_` を保持するようにした。
- `SideChainSend` に `preFader` を追加し、JSON 保存・復元に対応した。
- Audio Mixer UI の Sidechain 設定で Post-Fader / Pre-Fader を選択できるようにした。
- 既存プロジェクトで `preFader` がない send は従来どおり Post-Fader として復元する。

### VCA Core 実装 (2026-08-20)

- `AudioBusKind::Vca` と VCA member assignment API を追加した。
- VCA は音声を primary graph に流さず、所属busの処理ゲインだけを制御する。
- VCA所属を JSON 保存・復元し、bus削除時には所属参照も掃除する。
- Audio Mixer UIからVCA busを作成し、所属busをチェック式メニューで追加・解除できる。
- VCA UI操作と runtime parity の確認は未完了。

### Mixer input lifetime fix (2026-08-20)

- `AudioMixer::process()` が処理開始時に全busをクリアしていたため、compositionが事前に積んだlayer inputを消してしまう順序不整合を修正した。
- composition側でlayer busを評価ブロックごとに初期化し、mixer側ではMasterおよびprimary routeの派生busだけを処理前に初期化する。
- `tests/ArtifactCore/AudioMixerRoutingTest.cpp` にPre-Fader送信とVCA保存復元の回帰テストを追加した（未実行）。

- **現状の強み:** `AudioMixer::Impl` の `routing`（1:1 プライマリ）＋ `sends`（1:N aux）モデルは DAW の main-out / aux-send セマンティクスと一致。`getSortedBuses` のトポロジカルソート（`:86`）と `connect` のサイクル検出（`:558`）は実装済み。`AudioBus::process`（`:303`）で FX 適用後に `&sideChainBuffer_` をエフェクトへ渡し、`AudioCompressor` が `sideChain` 引数で受けるため、Return バス経由の sidechain は動作する。
- **Phase 1 の根拠:** `sidechainSource_` は `AudioBus.cppm:535-552` で set/get されるが、読み出しは `toJson/fromJson` のみ。Audio 全体・Artifact 全体で `getSidechainSource` の読み出しは 0 件 → デッドコード確定。
- **Phase 2 の根拠:** `AudioMixer::process`（`:740-744`）のセンド供給は `bus->getOutputBuffer()`（post-fader）のみ。pre-fader には `bus` のエフェクト適用前バッファへの参照が必要。
- **Phase 3 の根拠:** `AudioBusKind` に VCA がなく、ゲイン合成は `AudioBus::process` の `linearGain` のみ。VCA は primary 経路を変えず別計算が必要。
- **Phase 4/5:** ASIO 🟡30%、VST3 🟡50%、CLAP 🟡40%（監査）。ミキサー・グラフ自体は DAW 相当に近く、真の格差はこれらプラグイン／MIDI 層。

判定: **監査完了。Phase 0〜3 は低リスクで DAW らしさを効かせる。実装は未着手。**

追加調査（2026-08-20）:
- **runtime 動作確認:** `ArtifactAbstractComposition::getAudio`（`:3093`）は `impl_->audioMixer_` が有効なら `mixer.process(mixOutput)`（`:3184`）を呼ぶ。各レイヤーは `ensureLayerBus` でバスを取得し `addInput` で供給、Master バス出力が最終混合となる。したがって routing/sends/sidechain は**実際に再生時に処理される（デッドコードではない）**。
- **activation gating の発見:** `ensureAudioMixer` の呼び出し元は (a) `ArtifactCompositionAudioMixerWidget::refreshFromCurrentComposition` → `ArtifactAudioService::syncCurrentComposition`（`:2349`）、(b) `deserialize` 時の `audioMixer` キー存在時（`:4724`）。再生エンジン（`ArtifactPlaybackEngine::getAudio`）は `ensureAudioMixer` を呼ばない。→ ミキサーパネル未開＋mixer データ非保存の構成では `getAudio` がフラット旧経路（`:3194`）にフォールバックし、ルーティングが沈黙する。Phase 0 で解消。
