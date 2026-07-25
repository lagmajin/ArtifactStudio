# マルチチャンネルオーディオ出力 設計書

**作成日:** 2026-07-02
**ステータス:** 設計フェーズ
**関連コンポーネント:** AudioRenderer, AudioMixer, AudioBus, AudioDownMixer, ArtifactAudioLayer, ArtifactPlaybackEngine, ArtifactAudioMixer

---

## 1. 現状分析

### 1.1 AudioSegment — データ構造

`AudioSegment` は `QVector<QVector<float>> channelData` で任意チャンネル数を表現可能。
`AudioChannelLayout` で Mono / Stereo / 5.1 / 7.1 / 10ch / Ambisonics のレイアウト情報を持つ。

```cpp
// ✅ 問題なくマルチチャンネルを保持できる
AudioSegment seg;
seg.channelData.resize(6);  // 5.1ch
seg.layout = AudioChannelLayout::Surround51;
```

### 1.2 現在のパイプライン（全て Stereo 固定）

```
ArtifactAbstractComposition::getAudio()
  ├─ outSegment.channelData.resize(2)          ← ハードコード Stereo
  ├─ layer->getAudio(layerSegment, ...)
  │   └─ ArtifactAudioLayer::getAudio()
  │       ├─ outSegment.channelData.resize(2)  ← ハードコード Stereo
  │       ├─ ソースが mono → dual mono 化
  │       └─ ソースが stereo → そのまま
  ├─ 各 layer の audio を単純加算
  └─ limiter + softClip

ArtifactPlaybackEngine::updateAudio()
  └─ audioRenderer_->enqueue(segment)          ← 常に 2ch

AudioRenderer
  ├─ Impl::channels = 2                        ← ハードコード
  └─ openDevice() → format.setChannelCount(2)  ← 2ch を要求
```

### 1.3 既存マルチチャンネル対応リソース

| リソース | 現状 | 対応度 |
|---|---|---|
| **AudioDownMixer** | 5.1→Stereo / Mono→Stereo 実装済み。7.1 未対応 | ✅ 部分的 |
| **AudioMixer** (Core) | バストポロジー + process() 完備。再生パス未接続 | ✅ 完備だが未統合 |
| **AudioBus** | layout/volume/pan/mute/solo/effect 完備 | ✅ 完備 |
| **AudioPanner** | VBAP / Ambisonics モード完備 | ✅ 完備 |
| **AudioBackendFormat** | channelCount 対応 | ✅ API 対応済み |
| **ArtifactAudioMixer** (UI) | チャンネルストリップ UI。Core Mixer と未接続 | ⚠️ UI のみ |
| **WASAPIBackend** | マルチチャンネルデバイス対応は未確認 | ❓ 要確認 |
| **QtAudioBackend** | Qt Multimedia のマルチチャンネル依存 | ❓ 要確認 |

### 1.4 マルチチャンネル出力がブロックされている箇所

1. **`ArtifactAudioLayer::getAudio()`** (Layer): 常に 2ch で出力（line 452: `channelData.resize(2)`）
2. **`ArtifactAbstractComposition::getAudio()`** (Composition): 常に 2ch で出力（line 1358: `channelData.resize(2)`）
3. **`AudioRenderer::Impl::channels`** (Renderer): `int channels = 2` 固定（line 76）
4. **`AudioRenderer::openDevice()`** (Renderer): `format.setChannelCount(2)` でデバイスを開く（line 277）
5. **`ArtifactPlaybackEngine`** (Playback): 直接 composition→renderer で mixer をバイパス


---

## 3. フェーズ別実装計画

### Phase 0: 設計確認・準備（本ドキュメント）

- ✅ 現状分析完了
- ✅ アーキテクチャ設計完了
- ✅ 各コンポーネントの責務と変更点を明確化
- [ ] 既存テストの確認
- [x] WASAPI/QtAudioBackend のマルチチャンネル対応確認（API レベル確認済み）

---

### Phase 1: AudioRenderer のマルチチャンネル出力対応

**目標:** Renderer が任意チャンネル数の AudioSegment を受け取れるようにする。

**変更ファイル:**
- `ArtifactCore/src/Audio/AudioRenderer.cppm`
- `ArtifactCore/include/Audio/AudioRenderer.ixx`

**変更内容:**

1. `AudioRenderer::openDevice(channelCount)` でチャンネル数を指定可能にする
   - デフォルトは 2 (Stereo) で後方互換維持
   - `setPreferredChannelCount(int channels)` メソッド追加

2. `AudioRenderer::Impl::channels` の固定値 2 を削除し、動的に設定

3. `openDevice()` 内の `format.setChannelCount()` を引数のチャンネル数で設定

4. AudioRenderer に `setOutputChannelLayout(AudioChannelLayout)` 追加
   - Renderer が受け取った Segment を出力レイアウトに合わせてチャンネル変換する経路を追加

5. RingBuffer のチャンネル数設定を動的にする

**インターフェース案:**
```cpp
void setPreferredChannelCount(int channels);
int preferredChannelCount() const;
int actualChannelCount() const;
void setAutoDownmix(bool enabled);
bool openDevice(int channelCount = 2, const QString& deviceName = "");
```

**done 条件:**
- ✅ 5.1ch の AudioSegment を enqueue しても正しくバッファリングされる
- ✅ デバイスが 2ch しか対応していない場合、自動ダウンミックスにフォールバック
- ✅ 既存の 2ch 再生に影響がない


---

### Phase 2: Composition → Mixer → Renderer 経路の統合

**目標:** `AudioMixer` を再生パイプラインの正規経路にする。

**ステータス: ✅ 完了

**変更ファイル:**
- `ArtifactCore/src/Audio/AudioMixer.cppm`
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`

**変更内容:**

1. `ArtifactAbstractComposition` に `AudioMixer` を保持させる
2. Composition の `getAudio()` に Mixer 経路を追加
   - Layer → Bus のマッピングは Layer ID で紐付け
   - 各 layer の `getAudio()` → `AudioBus::addInput()`
   - `AudioMixer::process()` → master bus 出力
   - `AudioDownMixer` で出力チャンネル数に合わせる
3. `ArtifactPlaybackEngine` が Composition の Mixer を使うように変更

**マイグレーション戦略:**
- 既存の `getAudio()` 経路を維持しつつ、Mixer 経路をオプショナルで追加
- 初期は Layer を自動的に Bus にマッピング

**done 条件:**
- Mixer 経由で composition の audio が再生される
- Layer の volume/pan/mute が Bus 経由で正しく適用される
- Mixer 無効時は従来動作を維持

---

### Phase 3: AudioLayer のマルチチャンネル getAudio()

**目標:** `ArtifactAudioLayer::getAudio()` がソースのチャンネル数を保持して出力する。

**ステータス: ✅ 完了**

**変更ファイル:**
- `Artifact/src/Layer/ArtifactAudioLayer.cppm`

**変更内容:**

1. `getAudio()` がソースのチャンネル数を保持する
   - 現在は 2ch 固定出力を `sourceChannelCount_` 基盤の可変に
   - Mono → 1ch / Stereo → 2ch / 5.1 → 6ch / 7.1 → 8ch
2. パンニングの適用をチャンネル数に応じて変更
   - 2ch まで → `calculateConstantPowerGains`
   - それ以上 → VBAP またはダウンミックス後パン

**done 条件:**
- 5.1ch WAV が channelCount=6 で取得できる
- Mono / Stereo ソースは従来通り動作

---

### Phase 4: AudioDownMixer の拡張

**目標:** 7.1ch 対応と自動ダウンミックスパスの完成。

**変更ファイル:**
- `ArtifactCore/src/Audio/AudioDownMixer.cppm`
- `ArtifactCore/include/Audio/AudioDownMixer.ixx`

**変更内容:**

1. 7.1ch → Stereo ダウンミックス係数追加
2. Surround71 → Stereo 変換パス
3. 任意 n→m チャンネルの汎用ダウンミックス行列
4. AudioRenderer の自動ダウンミックスに組み込み

**標準係数（ITU-R BS.775）:**
```
5.1→Stereo: L += 0.707*C + 0.707*LFE + 0.5*Ls
            R += 0.707*C + 0.707*LFE + 0.5*Rs
7.1→Stereo: L += 0.707*C + 0.707*LFE + 0.5*Ls + 0.5*Lb
            R += 0.707*C + 0.707*LFE + 0.5*Rs + 0.5*Rb
```

**done 条件:**
- 5.1/7.1 → Stereo ダウンミックスが正しく動作
- 係数パラメータ変更可能

---

### Phase 5: UI Mixer と Core Mixer の接続

**目標:** `ArtifactAudioMixerWidget` の操作が `AudioMixer` / `AudioBus` に反映される。

**変更ファイル:**
- `Artifact/src/Audio/ArtifactAudioMixer.cppm`
- `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm`

**変更内容:**

1. `AudioMixerChannelStrip` が `AudioBus` を内部保持
2. volume/pan/mute/solo が Bus に直接反映
3. Mixer Widget が Composition の `AudioMixer` を observe

**done 条件:**
- UI スライダーが Bus volume を変更する

---

## 4. 依存関係グラフ

```
Phase 1 (Renderer)
    ↓
Phase 2 (Mixer統合) ──→ Phase 3 (Layer getAudio)
    ↓                          ↓
Phase 4 (DownMixer拡張) ←──────┘
    ↓
Phase 5 (UI Mixer接続)
    ↓
Phase 6 (出力選択UI)
```

- **Phase 1**: 単独着手可能
- **Phase 2**: Phase 1 に依存（Renderer がマルチチャンネルを受け取れる必要あり）
- **Phase 3**: Phase 2 に依存しないが Phase 4 の前に完了必要
- **Phase 4**: Phase 1 + Phase 3 に依存
- **Phase 5**: Phase 2 に依存
- **Phase 6**: Phase 1 + Phase 5 に依存

---

## 5. リスクと注意点

### 5.1 マルチチャンネル WAV のデコード

現在の `ArtifactAudioLayer` の WAV デコードは 1/2ch のみ想定。
`SimpleWav` / `FFMpegAudioDecoder` が 5.1/7.1 WAV を読み込めるか確認が必要。

### 5.2 WASAPI / QtBackend のマルチチャンネル対応

- WASAPI shared mode: 通常 stereo。exclusive mode でマルチチャンネル可能
- QtAudioBackend: `setChannelCount()` がデバイスに受け入れられるか要確認
- フォールバックとして必ず Stereo に落とせることを保証

### 5.3 パフォーマンス

- マルチチャンネル処理はチャンネル数に比例して CPU 負荷が増加
- 5.1ch = Stereo の 3倍、7.1ch = 4倍
- リサンプリング + ダウンミックス同時発生時の最適化が必要

### 5.4 ダウンミックスのクリッピング防止

- 複数チャンネル加算でクリッピング発生しやすい
- 現在の `softClip()` に加えて、ダウンミックス固有のゲイン補償（-3dB）を適用

---

## 6. 既存コードへの影響評価

| 変更 | 後方互換性 | 影響範囲 |
|---|---|---|
| Phase 1: Renderer channelCount | ✅ デフォルト 2ch 維持 | 内部のみ |
| Phase 2: Mixer 統合 | ⚠️ getAudio() 二重経路 | Composition + PlaybackEngine |
| Phase 3: Layer getAudio | ✅ Mono/Stereo 従来通り | AudioLayer のみ |
| Phase 4: DownMixer | ✅ 既存係数変更なし | DownMixer のみ |
| Phase 5: UI→Core Mixer | ⚠️ 既存 UI 動作継続 | MixerWidget |
| Phase 6: 出力選択 UI | ✅ 新規追加のみ | MixerWidget + Renderer |

---

## 7. 工数見積もり

| Phase | 内容 | 工数 |
|---|---|---|
| Phase 1 | AudioRenderer マルチチャンネル対応 | 4-6h |
| Phase 2 | Mixer 統合 (Composition→Mixer→Renderer) | 8-12h |
| Phase 3 | AudioLayer マルチチャンネル getAudio | 4-6h |
| Phase 4 | AudioDownMixer 拡張 | 3-4h |
| Phase 5 | UI Mixer と Core Mixer 接続 | 6-8h |
| Phase 6 | 出力デバイス選択 UI | 3-4h |
| **合計** | | **28-40h** |

---

## 8. 検証項目

### Phase 1 検証
- [ ] 2ch → 2ch: 従来と同一品質
- [ ] 6ch Segment → 6ch RingBuffer 格納
- [ ] 6ch デバイスが開ける（またはダウンミックスフォールバック）

### Phase 2 検証
- [ ] Mixer 経由で全 layer audio 合成
- [ ] Layer volume/pan/mute が Bus 経由で正しく効く
- [ ] Mixer 無効時は従来パス
- [ ] Solo が正しく機能

### Phase 3 検証
- [ ] 5.1ch WAV が 6ch として getAudio
- [ ] 2ch WAV が従来通り 2ch
- [ ] 1ch WAV が従来通り 1ch

### Phase 4 検証
- [ ] 5.1ch → Stereo ダウンミックス品質
- [ ] 7.1ch → Stereo ダウンミックス品質
- [ ] 高レベル信号のクリッピング防止

### Phase 5 検証
- [ ] UI 操作が Core Mixer に反映
- [ ] Mixer 状態変更が UI に反映
- [ ] 複数 layer ストリップが正しく表示

### Phase 6 検証
- [ ] 出力レイアウト選択 → Renderer に反映
- [ ] デバイス非対応レイアウトは選択不可
- [ ] 即時切り替えが安定動作

## 2026-07-25 静的確認

設計書の旧記載から進展しており、AudioRenderer は preferred channel count、実デバイスの channel count、auto-downmix を持ち、Composition 側には 6／8ch の出力レイアウト選択と AudioMixer 経路がある。AudioLayer も mono／stereo／5.1／7.1 のチャンネル数を保持する実装が存在し、UI Mixer と Core AudioBus の同期も実装済み。

一方、AudioDownMixer の実装は確認できた範囲では Stereo／Mono 変換が中心で、7.1 の品質保証、デバイス別の実出力、出力レイアウト選択 UI の完全な接続、RingBuffer の複数チャンネル実機検証、クリッピング防止を含む Phase 4〜6 は未検証または未完了。判定は「パイプライン基盤は導入済み、実機出力・7.1品質・切替検証が残る」とする。
