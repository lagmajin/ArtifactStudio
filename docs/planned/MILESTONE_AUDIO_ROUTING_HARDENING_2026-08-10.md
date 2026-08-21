# マイルストーン: Audio Routing Hardening

**最終更新:** 2026-08-20
**ステータス:** Core/UI routing contract implemented / runtime parity pending
**優先度:** 高
**対象:** `Artifact/`, `ArtifactCore/`
**関連:** [Audio Layer Integration](MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md), [Audio Mixer Surface Phase 2](MILESTONE_AUDIO_MIXER_SURFACE_PHASE2_2026-05-25.md), [Multichannel Audio Output](MILESTONE_MULTICHANNEL_AUDIO_OUTPUT_2026-07-02.md)

## 現行コード監査 (2026-08-15)

`AudioMixer` には Master／Layer／Group／Return／VCA の bus 種別、stable layer bus resolver、primary route、Pre/Post-Fader sidechain send、cycle／self-route／Master source 拒否、solo 到達判定、JSON の bus／edge／VCA 復元が実装されている。`ArtifactAudioService` と composition 側も `layer_<LayerID>` を利用している。Composition Audio Mixerからのadvanced routing導線、route編集のundo、入力寿命の修正、VCA編集UI、回帰テスト登録まで実装済みである。一方、preview/export parity、multichannel acceptance、runtime動作確認は未完了である。

## Update 2026-08-15

- `AudioMixer` の Master／Layer／Group／Return、stable layer bus、primary route、sidechain、cycle／self-route／Master source 拒否、solo 到達判定、JSON 復元を再確認。
- `AudioRoutingResult` による拒否理由、Composition Audio Mixer からの Advanced Routing 導線、bus kind persistence、group／return 作成、route visibility は実装済み。
- route 編集の undo、layer bus cleanup、Pre/Post-Fader send、VCA assignment、preview/export parityの共通composition経路は実装済み。
- stereo／5.1／7.1 acceptance、sidechain solo policy、runtime出力は未検証。

## 目的

音声レイヤーから Master 出力までの経路を、編集・再生・保存/再読込で一貫して扱えるようにする。ユーザーが「どのレイヤーがどの bus に流れ、どの send が有効か」を確認・変更でき、その結果が実際の再生と書き出しで同じになる状態を完了条件とする。

これは新しい DSP 機能を増やすマイルストーンではない。既存の `AudioMixer` グラフを正規のルーティングモデルとして固め、レイヤー UI と composition の音声処理を安全に接続する強化である。

## 現状と問題

静的確認では、Core `AudioMixer` は primary route、sidechain send、循環 route の拒否、bus ID を使った serialize/deserialize を持つ。`ArtifactAbstractComposition::getAudio()` も mixer 有効時に audio layer ごとの `layer_<LayerID>` bus へ PCM を入力し、Mixer の出力を Master から得る。

一方で、次の境界が分かれている。

- `ArtifactAudioService::syncCurrentComposition()` と composition の再生経路は layer bus を必要に応じて作成し、layer の volume/pan/mute/solo を同期する。
- `ArtifactCompositionAudioMixerWidget` は layer channel strip を中心に扱い、Core mixer の advanced routing 編集面ではない。
- `AudioMixerWidget` には bus route と sidechain send の編集 UI があるが、composition mixer surface との役割・到達導線・変更反映が統合されていない。
- Core の mixer JSON は composition JSON に保存されるが、保存済みの graph と、再読込後に layer bus を同期・削除する処理がどの順序で整合するかは runtime で保証されていない。

このため、基盤 API はあるものの、通常の編集フローで「設定した routing が音になる」ことを説明・検証しにくい。

## 正規モデル

```text
Audio / Video layer
  -> stable layer bus (`layer_<LayerID>`)
  -> optional group / return bus
  -> Master bus
  -> playback device and export audio

sidechain send: source bus --(amount)--> target bus sidechain input
```

- layer bus は layer ID に結び付く内部 bus とし、名前変更で参照を失わない。
- group / return bus は composition 所有の bus とし、削除時は入力 route を Master に戻し、関連 send を取り除く。
- primary route は 1 bus あたり 1 destination。循環は保存時・UI 操作時・deserialize 後の全てで無効として扱う。
- sidechain は音声を出力 route に二重加算しない制御入力であり、通常の effect send / return と UI 上も区別する。
- routing の変更は composition mutation / undo の既存経路に載せる。新規の global signal/slot は追加しない。

## Phase 0: 契約と観測性

目的: 実装前に、現状の graph と操作の成否を判定できるようにする。

- [x] bus 種別（Master / layer / group / return / VCA）と識別子の契約を明文化する。
- [x] `connect`、`disconnect`、bus 削除、send 追加/削除、deserialize の結果を routing result として取得できるようにする。
- [x] cycle、自己 route、存在しない target、壊れた ID、禁止された Master 操作を UI に理由付きで表示する。
- [x] audio callback と device thread には UI 操作や graph の所有権変更を持ち込まない。

完了条件: routing 操作の成功・拒否・自動修復を、再生ログに依存せず確認できる。

## Phase 1: Layer Bus と Composition 同期の固定

目的: layer bus の生成・同期・削除が、編集、再生、再読込で同じ結果になるようにする。

- [x] layer ID と layer bus の対応を単一の resolver に集約する。
- [x] audio layer と audio を持つ video layer の volume / pan / mute / solo を同期する。
- [x] layer 追加、削除、複製、source 差し替え、composition 切替時の stale bus 処理を定義する。
- [x] user-created group / return / VCA bus を layer bus cleanup の対象から除外する。
- [ ] solo は layer UI、Core bus、最終 mix の全てで同じ可聴結果になるよう runtime確認する。

完了条件: layer を増減・保存・再読込しても、意図しない Master 直結、孤立 bus、古い layer bus が残らない。

## Phase 2: 統合 Routing Surface

目的: Composition Audio Mixer から、通常の layer 操作と advanced routing を途切れずに行えるようにする。

- [x] channel strip に現在の output destination を短く表示し、Master 以外への route を明示する。
- [x] selected strip から destination 選択、Master への復帰、group / return / VCA bus の作成・削除を行えるようにする。
- [x] sidechain を dedicated control として表示し、source / target / amount / Pre/Post-Fader / clear を編集できるようにする。
- [ ] route を変更する前に cycle / invalid target を UI 側でも予防し、Core 側の拒否理由を表示する。
- [ ] routing graph を読む専用の compact view を用意する。timeline 左ペインに常時バッジや集約表示は追加しない。
- [ ] 既存の `AudioMixerWidget` は duplicate implementation にせず、共通の routing controller / model を利用するか、明確に advanced panel として位置付ける。

完了条件: レイヤーを group bus へ送り、Master へ戻し、sidechain を追加・削除する基本操作を、現在の composition から完結できる。

## Phase 3: 永続化・Undo・互換性

目的: routing graph をプロジェクト状態として安全に扱う。

- [ ] serialize には stable bus ID、kind、primary target ID、send target ID、amount を含める。
- [ ] deserialize は全 bus を解決してから edge を適用し、無効 edge を安全に落として diagnostic を残す。
- [ ] legacy project では layer bus が無い場合に Master route を既定とし、ユーザー定義 bus を勝手に生成しない。
- [x] routing edit を既存の composition mutation / undo-redo 経路へ統合する。
- [ ] rename は ID 参照を壊さず、表示名の衝突を防ぐ。

完了条件: route / send / bus 削除を undo-redo でき、保存した project を再読込しても同じ graph と可聴結果を保つ。

## Phase 4: 再生・書き出し Parity と受け入れ検証

目的: UI の状態ではなく、実際の output を基準に完成を判定する。

- [ ] preview playback と export が同じ composition mixer graph を使うことをruntime確認する。
- [ ] stereo、5.1、7.1 の layout で route ごとの channel conversion と Master output を確認する。
- [ ] mute / solo / parent evaluation gain / limiter / clipping の適用順を固定し、route をまたいでも変わらないことを確認する。
- [ ] empty source、decode failure、device unavailable、underflow 時に graph state を破壊しないことを確認する。
- [ ] 次の fixture を用意する: direct-to-Master、2 layer -> group -> Master、sidechain、cycle attempt、削除された target、保存/再読込。

完了条件: fixture ごとの expected route と expected audible output を確認でき、preview/export の差分がない。

## 非対象

- 新しい音声デバイス backend、WASAPI の全面刷新
- VST/CLAP host、effect parameter automation、loudness normalization
- video decoder / audio decoder の全面再設計
- 動画対応を主目的とする大規模機能

これらは routing の正規経路が安定した後に別マイルストーンとして扱う。

## 進捗メモ

### 2026-08-10: Core solo graph semantics

`AudioMixer::process()` に、solo が存在するとき primary-route 上で solo bus から到達する bus だけを可聴にする判定を追加した。これにより、solo state を単に保存するだけだった状態を改め、direct layer bus と group bus の両方で solo を実音へ反映する。これは temporary mix decision であり、非 solo bus の `mute` 状態を変更・保存しない。

runtime 検証、sidechain-only source の solo ポリシー、preview/export parity は Phase 4 で未確認である。

### 2026-08-10: Stable layer-bus identity

`AudioMixer::layerBusName(LayerID)` を Core の正規 resolver として追加し、composition の block mixer、`ArtifactAudioService`、composition mixer surface が同じ `layer_<LayerID>` identity を用いるようにした。表示用 layer 名と routing identity を混同しないための最小の集約である。

### 2026-08-10: Master terminal invariant

`AudioMixer::connect()` と `disconnect()` は Master bus を source とする操作を拒否するようにした。Master は final output の terminal sink であり、ここから別 bus へ route すると graph と `finalOutput` が乖離するためである。

### 2026-08-10: Routing mutation diagnostics

`connect` / `disconnect` / sidechain send の Core API は `AudioRoutingResult` を返すようにし、無効 source・target、Master source、self route、cycle、非有限 send amount などの理由を取得できるようにした。既存の advanced `AudioMixerWidget` は失敗を tooltip で提示し、成功時だけ更新 callback を実行する。

### 2026-08-10: Composition mixer routing entry point

`ArtifactCompositionAudioMixerWidget` の header から `Advanced Routing…` を開けるようにし、既存の `AudioMixerWidget` を current composition の Core mixer に接続した dialog として再利用した。閉じた時点で composition を変更済みにし、compact mixer surface を同じ graph から再構築する。新しい global event 経路は追加していない。

### 2026-08-10: Bus kind persistence

`AudioBusKind`（Master / Layer / Group / Return）を mixer state に追加し、non-Master bus とともに JSON 保存するようにした。`ensureLayerBus(LayerID)` が内部 layer bus の生成・復元を一元化し、stale cleanup は name prefix ではなく kind を基準にする。kind を持たない旧 project は従来の `layer_` prefix から Layer / Group を復元する。

### 2026-08-10: Compact route visibility

Composition mixer の channel strip は fixed `Master` 表示ではなく、同期済み Core graph の route target を表示するようにした。Master 以外の route は色を変え、accessible description にも destination を含める。

### 2026-08-10: Group-solo graph semantics

solo 判定は、solo child を運ぶ group bus だけでなく、明示的に solo した group へ入力する child bus も可聴にするよう整理した。後者は downstream の primary route だけを辿るため、solo child を持つ group の別 sibling を誤って可聴にしない。

### 2026-08-10: Group / return creation

Advanced routing surface の `+ Bus` は作成時に Group または Return を選択できるようにした。Core は user-created bus に Master / Layer kind を指定できないよう正規化し、Layer kind は `ensureLayerBus()` の専用経路だけで割り当てる。

## 実施順序

1. Phase 0 の契約・diagnostic と Phase 1 の stable layer bus resolver
2. serialize/deserialize と layer cleanup の整合（Phase 3 の安全部分）
3. Composition Audio Mixer の routing surface（Phase 2）
4. preview/export parity と multichannel fixture（Phase 4）

UI を先に増やさず、layer bus の所有権と保存復元を先に固定する。これにより、routing UI が再生経路と異なる shadow state を持つことを防ぐ。

## 受け入れチェックリスト

- [ ] Audio / Video layer から Master までの route を UI で追える
- [ ] layer bus、group / return bus、Master の責務が混ざらない
- [ ] invalid route / cycle / missing target は安全に拒否または修復され、理由が読める
- [ ] route と sidechain send が保存・再読込・undo-redo 後も残る
- [ ] layer の mute / solo / pan / gain が UI・Core graph・実音で一致する
- [ ] bus 削除後に孤立 edge / stale layer bus が残らない
- [ ] preview と export の音声経路が同じである
- [ ] runtime 検証を実施し、結果を本書へ追記する

## リスク

- composition の `getAudio()` は block ごとに bus を解決・入力するため、graph 編集と再生を同じ可変コンテナへ無秩序に触れさせると drop-out や競合の原因になる。UI mutation は既存の安全な composition 更新境界で反映する。
- `AudioMixerWidget` と `ArtifactCompositionAudioMixerWidget` の両方を独立して拡張すると、route 表示と更新経路が再び分岐する。共有モデルを先に決める。
- layer 名は user-editable であり、route identity に使わない。保存・復元は stable ID を優先する。

## 実装開始時に確認するファイル

- `ArtifactCore/include/Audio/AudioMixer.ixx`
- `ArtifactCore/src/Audio/AudioMixer.cppm`
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm`
- `Artifact/src/Service/ArtifactAudioService.cppm`
- `Artifact/src/Audio/ArtifactAudioMixer.cppm`
- `Artifact/src/Widgets/AudioMixerWidget.cppm`
- `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm`
