# M-AU-8 Audio Scrubbing Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `ArtifactCore/include/Audio/AudioCache.ixx`,
      `ArtifactCore/include/Audio/AudioFrame.ixx`,
      `ArtifactCore/include/Audio/AudioSegment.ixx`,
      `ArtifactCore/include/Audio/AudioMixer.ixx`,
      `Artifact/src/Service/ArtifactPlaybackService.cppm`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`,
      `Artifact/src/Widgets/AudioMixerWidget.cppm`,
      `Artifact/src/Audio/ArtifactAudioEngine*`,
      `Artifact/src/Layer/ArtifactAudioLayer.cppm`
位置づけ: Timeline scrub / playhead 移動中に **短い遅延で音声プレビュー** を再生する foundation。AE の audio scrubbing 互換。
参照:
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` (#20)
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` (P0)
- `docs/analysis/WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §6
- `docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md`
- `docs/planned/MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md`
- `docs/planned/MILESTONE_RAM_PREVIEW_RANGE_POLICY_AND_PRIORITY_2026-05-31.md`
- `docs/planned/MILESTONE_AUDIO_PLAYBACK_STABILIZATION_2026-03-28.md`
- `docs/planned/MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md`

---

## 1. 目的

`FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` (#20):

> Audio Scrubbing
> — 一致コードなし。scrub 時のリアルタイム音声 preview がない。playbackEngine への導線のみ存在

`AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` の P0 に分類。

Timeline scrub 中の音声は本来、制作上の「ここ重要」ポイントを探す最も自然な手段。AE では playhead をドラッグすると、**scrub 速度に応じて音量が変わる** 短い音声プレビューが返る。

これが **ない** ことで、scrub は silent になり、音声素材の位置確認が frame 単位の手作業になる。

> 重要: 既存 `ArtifactPlaybackService` および `ArtifactAudioEngine` の通常 playback 経路には触らず、**scrub 専用の独立 buffer** で運用する。video scrub と audio scrub の責務を分離し、playback 経路への副作用を最小化する。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/` 側に閉じる。サブモジュール（`ArtifactWidgets`）は触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `ArtifactCore/include/Audio/AudioCache.ixx` — `CachedAudioFrame { frameNumber, AudioSegment pcm, lastAccess }` の LRU cache
- `ArtifactCore/include/Audio/AudioFrame.ixx` — `AudioFrame { pcmData, sampleRate, channels, format, pts }`
- `ArtifactCore/include/Audio/AudioSegment.ixx` — channel 別 `QVector<float>` の PCM 構造
- `ArtifactCore/include/Audio/AudioMixer.ixx` — bus ベースの mixer
- `Artifact/src/Service/ArtifactPlaybackService.cppm:488` — reason ログ
- `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm` — scrub bar
- `Artifact/src/Widgets/AudioMixerWidget.cppm:122` — spectrum 表示
- `MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md` — frame cache
- `MILESTONE_AUDIO_PLAYBACK_STABILIZATION_2026-03-28.md` — playback 安定化

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Audio scrub controller | なし。`AudioScrubController` 不在 | scrub 中の音声再生不可 |
| 速度感連動 | なし | AE 風の音量調整不可 |
| Mute / solo 反映 | なし | 通常 playback 設定との不整合 |
| Video scrub との同期 | なし | frame → sample 換算が未実装 |
| 遅延目標 | なし | 10 ms / 5 ms 目標値なし |
| Audio cache 利用 | 既存 `AudioCache` があるが scrub 専用 hook なし | cache hit しない |
| 設定 UI | なし | on/off と遅延調整の導線なし |
| Diagnostics | なし | latency 監視なし |

### 2.3 既存 milestone との関係

- `MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md` — frame cache。audio scrub は **同じ frame cache** を参照してよい
- `MILESTONE_AUDIO_PLAYBACK_STABILIZATION_2026-03-28.md` — playback 安定化。本 milestone は scrub 専用で棲み分け
- `MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md` — audio layer UX
- `MILESTONE_RAM_PREVIEW_RANGE_POLICY_AND_PRIORITY_2026-05-31.md` — preview range policy

---

## 3. 設計の柱

### 3.1 AudioScrubController

`Artifact/src/Audio/ArtifactAudioScrubController.cppm` を新規追加:

```cpp
class ArtifactAudioScrubController : public QObject {
public:
    static ArtifactAudioScrubController& instance();

    // 開始
    void startScrub(const QString& compositionId);
    void stopScrub();

    // 現 playhead 位置
    void updateScrubPosition(FramePosition frame);

    // 設定
    void setEnabled(bool enabled);
    void setLatencyTargetMs(int ms);   // 既定 10ms
    void setVolumeScale(float scale);  // 既定 0.5

    // 状態
    bool isScrubbing() const;
    int  latencyMs() const;            // 実測

    // 内部 worker thread
    void onCacheHit(AudioSegment pcm, FramePosition frame);

signals:
    void scrubStarted();
    void scrubStopped();
    void latencyUpdated(int ms);
    void cacheMiss(FramePosition frame);
};
```

- 内部に `QThread` を持ち、main thread から独立
- `AudioCache` を `ArtifactCore/include/Audio/AudioCache.ixx` 経由で参照
- playhead 移動時に **過去 50 ms 〜 現在** の sample を mix して出力

### 3.2 速度感連動の音量

scrub 速度を `dt / frameDelta` から算出し、volume を調整:

| 速度 | volume |
|---|---|
| 0 (静止) | mute |
| 1 frame / 100ms 以下 | 1.0 |
| 10 frame / 100ms | 0.7 |
| 50 frame / 100ms 以上 | 0.3 |

- `volume = clamp(1.0 - log10(velocity) * 0.5, 0.0, 1.0)` をベースに
- AE 互換の **緩やかな減衰**

### 3.3 既存 playback との分離

- 通常 playback 用の `AudioEngine` とは **完全に独立した buffer** を使う
- 同時に動作しても audio output は **スクラブ controller 側で 1 個に mix**
- mute / solo / volume は **composition 設定** から取得
- video scrub 経路と **lock step** で動作（frame → sample 換算）

### 3.4 Timeline 入力

- `ArtifactTimelineScrubBar::mouseMove` 中に `updateScrubPosition(frame)` を呼ぶ
- 既存 scrub の `QTimer` を **再利用** せず、debounce 1 段で 60 fps に制限
- playhead 移動量が多いとき（jump）は `startScrub` 経由で `stopScrub` を即時実行

### 3.5 Mute / solo / volume 反映

- `ArtifactAudioMixerWidget` の **現在値** を取得
- mute: composition 単位
- solo: layer 単位
- volume: layer 単位
- scrub 中に mute / solo / volume が変わったら即時反映（既存 `mixerChanged` シグナル経由）

### 3.6 Cache 戦略

- `AudioCache::getCached(frame, out)` を利用
- ヒットなら sample を即時取得
- ミスなら `AudioSegment` を **小さく**（例: 5 frame 程度）確保して worker thread で decode
- `cacheMiss(frame)` を emit し、`M-CE-CRIT-1` の smoke に `audio.scrub.cache-miss-rate` を提供

### 3.7 Undo

- Audio scrub は **再生** なので Undo 対象ではない
- 設定変更（on/off、遅延目標）のみ `QUndoCommand` 派生

### 3.8 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `audio.scrub.disabled` (severity=info, 設定 off)
- `audio.scrub.latency-high` (severity=warning, latency > 50 ms)
- `audio.scrub.cache-miss-rate` (severity=info, miss rate > 30%)
- `audio.scrub.thread-stall` (severity=warning, worker thread stall 検出)

### 3.9 不変条件 (Guardrails)

- 通常 playback の `AudioEngine` 経路には **触らない**。scrub は独立
- 既存 `AudioCache` の API は **温存**
- `QImage` / `setStyleSheet` 流入禁止
- 新規 signal-slot 接続は `scrubStarted / scrubStopped / latencyUpdated / cacheMiss` の 4 個に限定
- worker thread 内で `ArtifactAbstractLayer` / QObject の API を呼ばない（pure data のみ）
- scrub 速度が極端に速い場合（> 100 frame / 100ms）は `volume = 0` で mute
- 既存 `MILESTONE_AUDIO_PLAYBACK_STABILIZATION_2026-03-28.md` の安定化経路と干渉しない

---

## 4. フェーズ計画

### Phase 1: AudioScrubController 骨格 (P0, 1〜2 セッション)

- `Artifact/src/Audio/ArtifactAudioScrubController.cppm` 新規
- worker thread 内部構造
- `startScrub / stopScrub / updateScrubPosition` 基礎
- `AudioCache` 接続（既存 API 経由）

**Done criteria:**
- `startScrub` 後に `updateScrubPosition` で worker が走る
- `cacheMiss` シグナルが発火
- 既存 playback 経路と独立

### Phase 2: Timeline scrub 連動 (P0, 1〜2 セッション)

- `ArtifactTimelineScrubBar::mouseMove` から `updateScrubPosition` 呼出
- debounce 1 段で 60 fps 制限
- playhead jump 時の `startScrub` 経由 `stopScrub`

**Done criteria:**
- Timeline scrub 中に `updateScrubPosition` が frame 移動ごとに発火
- jump 時に安全に start / stop
- 既存 scrub 経路と干渉しない

### Phase 3: 速度感 + mute / solo / volume (P0, 1 セッション)

- 速度感による volume 調整
- `ArtifactAudioMixerWidget` の mute / solo / volume 取得
- 即時反映

**Done criteria:**
- scrub 速度が遅いほど volume が高い（AE 互換）
- mute / solo / volume 変更が scrub 中に即時反映
- worker thread 内で QObject API を呼ばない

### Phase 4: Diagnostics + 設定 UI (P1, 1 セッション)

- Problem View への `audio.scrub.*` 健全性 contribution
- `ApplicationSettingDialog` に Audio > Scrubbing 設定ページ
- on/off、遅延目標（5 / 10 / 20 / 50 ms）、volume スケール

**Done criteria:**
- Problem View に `audio.scrub.latency-high` 等表示
- 設定ダイアログから on/off と遅延調整
- 設定は `FastSettingsStore` に保存

### Phase 5: 複数 audio layer の mix (P1, 1 セッション)

- 複数 `ArtifactAudioLayer` の同時 scrub
- `ArtifactComposition` の audio layer 一覧から mix
- mute / solo を layer 単位で適用

**Done criteria:**
- 3 audio layer の同時 scrub で mix される
- solo した layer のみが聞こえる
- mute した layer は無音

### Phase 6: 他 playback との協調 (P2, 別 milestone 推奨)

- 通常 playback と audio scrub の **同時** 動作（normal playback 中の scrub は再生を止めない）
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_AUDIO_SCRUB_PLAYBACK_COEXIST_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md` | frame cache。audio scrub は `AudioCache` を共有。 |
| `MILESTONE_AUDIO_PLAYBACK_STABILIZATION_2026-03-28.md` | playback 安定化。本 milestone は scrub 専用で棲み分け。 |
| `MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md` | audio layer UX。 |
| `MILESTONE_RAM_PREVIEW_RANGE_POLICY_AND_PRIORITY_2026-05-31.md` | preview range policy。 |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | diagnostics 文法。 |
| `MILESTONE_APPLICATION_SETTINGS_APP_INTEGRATION_2026-04-19.md` | 設定ダイアログ。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **Worker thread 境界**。scrub 専用 thread と main thread の同期。Qt の `QMetaObject::invokeMethod` 経由に統一
2. **`AudioCache` hit rate**。video scrub と比較して audio scrub は同じ frame 周辺を繰り返しアクセスするので cache hit 率は高いはず。実測
3. **Mute / solo の競合**。通常 playback と同じ mixer 設定を参照すると、scrub 中の音量変更が通常 playback に影響する可能性。scrub 用に **shadow 値** を保持
4. **Output device 排他**。OS によって通常 playback と scrub の audio output が排他になる場合。Phase 6 で検証
5. **Volume curve**。AE 互換の音量カーブは経験則。Phase 3 で実機調整

### 6.2 契約上の未解決

- **遅延目標**。5 ms / 10 ms のどちらを既定にするか。Phase 1 で実測
- **Mute / solo 取得 API**。`ArtifactAudioMixerWidget` 側に observer が無ければ新規追加
- **Audio device 抽象**。`ArtifactCore/include/Audio/AudioBackend.ixx` の抽象。Phase 1 で WASAPI / ASIO 別 latency
- **Multi-channel mix**。5.1ch / 7.1ch の mix down。Phase 5 で mono / stereo から拡張
- **Project 保存**。設定値は `FastSettingsStore` のみ。project JSON には保存しない

### 6.3 サブモジュール境界

- `ArtifactCore/include/Audio/AudioCache.ixx` 既存 API 温存。新規メソッド追加は禁止
- `Artifact/src/Audio/ArtifactAudioEngine*` には触らない
- `ArtifactWidgets` は触らない
- `Artifact/src/Audio/ArtifactAudioScrubController.cppm` を新規追加
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- Timeline scrub 中に audio scrub が動作
- 速度感に応じて volume が変わる（AE 互換）
- mute / solo / volume 変更が即時反映
- 通常 playback 経路と独立して動作
- cache miss rate < 30% が目標
- latency < 50 ms を目標
- Problem View に `audio.scrub.*` 健全性表示
- 設定ダイアログで on/off と遅延調整
- 3 audio layer の同時 mix
- 新規 `QImage` / `setStyleSheet` が増えていない
- 新規 signal-slot は 4 個に限定
- worker thread 内で QObject API を呼んでいない
- 既存 `AudioEngine` 経路に触れていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §6 を正式 milestone に起こした。AE 互換の audio scrubbing foundation。
