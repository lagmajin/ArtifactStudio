# コア機能 不足分析レポート Vol.2

**作成日:** 2026-03-28  
**ステータス:** 追加分析完了  
**対象:** 見落としがちな機能・重要未実装

---

## 概要

前回の分析（`CORE_FEATURE_GAP_ANALYSIS_2026-03-28.md`）で見落とされた機能・重要な未実装機能を抽出。

---

## 🔴 重要：見落とされた未実装機能

### 1. Undo/Redo システムの統合不足

**場所:** `docs/planned/MILESTONE_UNDO_AND_AUDIO_PIPELINE_COMPLETION_2026-03-25.md`

**現状:**
```
複数の Undo システムが並存:
├── ArtifactCore::UndoManager (実行時履歴)
├── ArtifactCore::UndoCommand 系 (ドメインコマンド)
├── Command.Serializable / Command.Session (QUndoStack ベース)
└── Edit menu (入口はあるが実処理が弱い)
```

**問題点:**
- 「正の履歴」が一本化されていない
- UI 直結変更が残ると Undo と実 state がズレる
- コマンドが粒度過剰だと履歴が使いにくくなる

**影響:**
- 操作ミスの不安
- 大きい UI 変更で壊れやすい
- 製品としての安心感不足

**工数:** 20-30h  
**優先度:** 🔴高

---

### 2. Audio Pipeline の未完成

**場所:** `docs/planned/MILESTONE_UNDO_AND_AUDIO_PIPELINE_COMPLETION_2026-03-25.md`

**現状:**
```
AudioRenderer → QAudioSink への送出器
QtAudioBackend → Qt Multimedia backend
AudioSegment → ミックス単位
AudioMixer / AudioBus / AudioVolume → あるが再生経路への統合は浅い
AudioLayer → WAV 読み込みの最小実装
PlaybackEngine → 再生中に composition->getAudio() を呼ぶ
```

**問題点:**
- AudioLayer の音が出ても、再生デバイスや sample format によって壊れやすい
- mixer があるのに、composition 再生の正規ルートにまだ十分載っていない
- 単純加算ミックスなので、クリップ・パン・solo・bus が未完成

**目標アーキテクチャ:**
```
AudioLayer / VideoLayer audio / other sources
  → per-layer AudioBus
  → AudioMixer
  → AudioRenderer
  → QAudioSink
```

**工数:** 24-36h  
**優先度:** 🔴高

---

### 3. NullLayer の未実装メソッド

**場所:** `Artifact/src/Layer/ArtifactNullLayer.cppm:40`

```cpp
//throw std::logic_error("The method or operation is not implemented.");
```

**影響:** 軽微（NullLayer は特殊用途）  
**工数:** 1-2h  
**優先度:** 🟢低

---

### 4. AdjustableLayer の未実装メソッド

**場所:** `Artifact/src/Layer/ArtifactAdjustableLayer.cppm:21`

```cpp
throw std::logic_error("The method or operation is not implemented.");
```

**影響:** 調整レイヤー使用時にクラッシュ  
**工数:** 2-4h  
**優先度:** 🟡中

---

## 🟡 中優先度：拡充が必要な既存機能

### 5. Reactive Events システム（実装済みだが未使用）

**場所:** `ArtifactCore/include/Reactive/ReactiveEvents.ixx`

**実装済み機能:**
```cpp
enum class TriggerEventType {
    OnStart, OnEnd, OnEnterRange, OnExitRange, OnLoop,
    OnContact, OnSeparation, OnProximity,
    OnValueExceed, OnValueDrop, OnValueCross,
    OnFrame
};

enum class ReactionType {
    SetProperty, AnimateProperty, RandomizeProperty,
    ApplyImpulse, ApplyForce, Attract, Repel,
    PlayAnimation, PauseAnimation, GoToFrame,
    SpawnLayer, DestroyLayer,
    PlaySound
};
```

**不足:**
- ⬜ `CollisionDetector` (実体)
- ⬜ `ReactiveEventEngine` (実行エンジン)
- ⬜ `ReactionExecutor` (実行者)
- ⬜ UI エディター

**工数:** 24-32h（前回の分析と同じ）  
**優先度:** 🟡中

---

### 6. MotionTracker（実装済みだが未使用）

**場所:** `ArtifactCore/include/Tracking/MotionTracker.ixx`

**実装済み機能:**
```cpp
class MotionTracker {
    void setSettings(const TrackerSettings& settings);
    int addTrackPoint(const QPointF& point);
    bool trackForward(double fromTime, double toTime);
    bool trackRange(double startTime, double endTime, ...);
    TrackResult result() const;
    QPointF pointPositionAt(int pointId, double time) const;
    // ...
};

namespace OpticalFlow {
    std::vector<std::pair<QPointF, QPointF>> computeFlow(...);
    QImage computeDenseFlow(...);
    QImage visualizeFlow(...);
}
```

**不足:**
- ⬜ 実装ファイル（.cppm）
- ⬜ OpenCV 統合
- ⬜ UI エディター

**工数:** 32-48h  
**優先度:** 🟡中

---

## 🟢 低優先度：将来的な拡張

### 7. Physics システム（拡充）

**現状:** 最小限の実装のみ

**不足:**
- ❌ RigidBody（剛体シミュレーション）
- ❌ SoftBody（軟体シミュレーション）
- ❌ Fluid（流体シミュレーション）

**工数:** 40-60h  
**優先度:** 🟢低

---

### 8. Crowd システム（未使用）

**場所:** `ArtifactCore/include/Crowd/`

**状況:** ディレクトリのみ存在、実装なし

**用途:** 群衆シミュレーション

**工数:** 48-72h  
**優先度:** 🟢低

---

### 9. VST システム（未使用）

**場所:** `ArtifactCore/include/VST/`

**状況:** ディレクトリのみ存在、実装なし

**用途:** VST プラグイン対応

**工数:** 60-80h  
**優先度:** 🟢低

---

## 📊 更新された工数サマリー

### 新規発見（4 機能）

| 優先度 | 機能 | 工数 |
|--------|------|------|
| 🔴高 | Undo/Redo 統合 | 20-30h |
| 🔴高 | Audio Pipeline 完成 | 24-36h |
| 🟡中 | AdjustableLayer 実装 | 2-4h |
| 🟢低 | NullLayer 実装 | 1-2h |

**小計:** 47-72h

---

### 前回からの累計

| 優先度 | 機能数 | 総工数 |
|--------|--------|--------|
| 🔴高 | 6 機能 | 52-78h |
| 🟡中 | 5 機能 | 80-114h |
| 🟢低 | 6 機能 | 100-156h |

**合計:** 232-348h

---

## 推奨実装順序（更新）

### 第 1 段階：コア安定化（48-70h）

1. **Undo/Redo 統合**（20-30h）
   - Edit menu の Undo/Redo を UndoManager に直結
   - レイヤー操作をコマンド化
   - UndoHistory をセッション保存

2. **Audio Pipeline 完成**（24-36h）
   - AudioMixer を正規ルートに
   - クリップ・パン・solo・bus 実装
   - 再生デバイス安定化

3. **AdjustableLayer 実装**（2-4h）
   - 未実装メソッドの補完

---

### 第 2 段階：コア機能補完（32-48h）

4. **VideoLayer Proxy**（16-24h）
5. **プロジェクト管理**（4-6h）
6. **WebUI ブリッジ**（8-12h）
7. **ROI Scissor**（4-6h）

---

### 第 3 段階：新機能（56-80h）

8. **Cache システム**（16-20h）
9. **Reactive システム**（24-32h）
10. **Motion Tracking**（32-48h）

---

## 特別注目：2 つの「未完成システム」

### 1. Reactive Events

**特徴:**
- ✅ 定義は完璧（Trigger/Reaction 両方）
- ❌ 実体がいない（エンジンなし）

**比喩:**
> 「設計図は完璧だが、建設されていない家」

**実装順序:**
1. `CollisionDetector`（衝突検出エンジン）
2. `ReactiveEventEngine`（イベント評価エンジン）
3. `ReactionExecutor`（リアクション実行者）
4. UI エディター

---

### 2. MotionTracker

**特徴:**
- ✅ 定義は完璧（TrackPoint/TrackResult/Settings）
- ❌ 実体がいない（OpenCV 統合なし）

**比喩:**
> 「エンジンはあるが、車体がない車」

**実装順序:**
1. OpenCV 統合（オプティカルフロー）
2. 特徴点検出・追跡
3. ホモグラフィ計算
4. UI エディター

---

## 関連ドキュメント

- `docs/technical/CORE_FEATURE_GAP_ANALYSIS_2026-03-28.md` - 第 1 回分析
- `docs/planned/MILESTONE_UNDO_AND_AUDIO_PIPELINE_COMPLETION_2026-03-25.md` - Undo/Audio マイルストーン
- `docs/planned/MILESTONE_REACTIVE_EVENT_SYSTEM_2026-03-28.md` - Reactive システム
- `docs/planned/MILESTONE_MOTION_TRACKING_SYSTEM_2026-03-25.md` - Motion Tracking

---

## 結論

**さらに 4 つの重要未実装機能を発見。**

特に以下の 2 つは製品品質に直結：

1. **Undo/Redo 統合**（20-30h）
   - 操作の安全性向上
   - ユーザーの安心感

2. **Audio Pipeline 完成**（24-36h）
   - 音声再生の信頼性向上
   - プロユースへの対応

これらを優先的に実装すべき。

---

**文書終了**
