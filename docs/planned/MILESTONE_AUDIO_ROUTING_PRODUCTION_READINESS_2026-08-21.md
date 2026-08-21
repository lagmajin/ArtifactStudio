# マイルストーン: Audio Routing Production Readiness

**最終更新:** 2026-08-21
**ステータス:** In Progress
**優先度:** High
**関連:** `docs/planned/MILESTONE_AUDIO_ROUTING_DEPTH_2026-08-20.md`, `docs/planned/MILESTONE_AUDIO_ROUTING_HARDENING_2026-08-10.md`

## 目的

`AudioMixer` / `AudioBus` のグラフルーティングを、実装済みAPIの集合から、保存・再読込・再生・Preview・Render Queueで一貫して利用できる製品経路へ仕上げる。新しいミキサー機能を広げる前に、既存のPre/Post-Fader send、VCA、sidechain、primary routingの挙動を受入条件として固定する。

## 現状

- Audio Routing Depth の Phase 0〜3（Mixer自動有効化、Pre/Post-Fader send、VCA Core/UI）は実装済み。
- 旧形式の `sidechain_source` を現行send graphへ移行する互換処理を追加済み。
- `AudioMixerRoutingTest` にPre/Post-Fader、VCA保存復元、VCAゲイン、旧sidechain移行のケースを追加済み。
- 同一メンバーの複数VCA所属によるゲイン乗算ケースも追加済み（コミット `53f9c4e`）。
- Pre-Fader sendの保存／再読込ケースも追加済み（コミット `47ea144`）。
- `ArtifactCoreAudioMixerRoutingTest` としてCMakeのテストターゲットへ登録済み（コミット `cbbb1fb`）。
- ビルド、テスト、実素材でのPreview／Render Queue確認は未実施。

## 完了条件

1. Mixerパネルを開かず、`audioMixer` 保存データがないCompositionでも、音声再生時にgraph-based mixerが有効になる。
2. Pre-Fader sendはフェーダ変更の影響を受けず、Post-Fader sendはフェーダ後の値を送る。
3. VCAはprimary routingを変更せず、所属バスの出力ゲインだけを制御する。
4. 旧 `sidechain_source` 保存データを読み込んでも、sidechain経路が失われない。
5. 新旧JSONの保存／再読込で、routing、send、VCA所属、Pre/Post設定が保持される。
6. Preview、Software Preview、Render Queueで、同一Composition・同一フレームの音声ルーティング結果が一致する。
7. 回帰テストを実行し、失敗時にrouting状態と対象busを診断できる。

## 実施フェーズ

### Phase 1 — 静的契約とテスト固定

- `AudioMixerRoutingTest` の入力、フェーダ、send、VCA、legacy migrationの期待値を整理する。
- 旧JSON互換ケースと、複数send／複数VCA所属の境界ケースを追加する。
- 複数VCA所属のゲイン合成ケースを追加する（完了）。
- Pre/Post設定の保存／再読込ケースを追加する（完了）。
- test targetへの登録状態を確認する（完了）。
- build／test実行前に、テスト対象のモジュール依存と既存CMake登録の重複を確認する。

### Phase 2 — 実行経路の受入

- build・unit testを実行する。
- Mixer未表示Composition、保存済みMixer Composition、旧JSON Compositionをそれぞれ再生する。
- Preview／Software Preview／Render Queueの結果とdiagnosticsを記録する。

### Phase 3 — 完了判定

- 失敗した経路を修正し、保存／再読込を再確認する。
- 受入結果を本書へ追記し、全条件が満たされたら `docs/done/` へ移動する。

## 対象外

- ASIO／複数ハードウェア出力
- VST3／CLAP／MIDIルーティング
- 新しいエフェクトや音源の追加
- Audio UIの大規模な再設計

## リスクと確認方法

`AudioMixer::process` の初期化順序、VCAの多重ゲイン、sidechain bufferのフレーム寿命、保存データのbus ID／名前解決が主なリスクである。確認は既存のAudioMixerテスト、最小Composition、保存／再読込、Preview／Render Queueの同一フレーム比較を順に行う。
