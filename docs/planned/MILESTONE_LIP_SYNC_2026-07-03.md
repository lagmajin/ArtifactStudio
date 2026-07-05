# M-LIPSYNC-1 Lip Sync Animation Milestone

**作成日:** 2026-07-03
**ステータス:** Draft
**関連:**
- `ArtifactCore/include/Audio/AudioAnalyzer.ixx`
- `ArtifactCore/src/Audio/AudioAnalyzer.cppm`
- `docs/planned/MILESTONE_MARKER_FOUNDATION_2026-06-16.md`
- `docs/planned/MILESTONE_FRAME_BY_FRAME_ANIMATION_2026-07-03.md`

---

## 1. 目的

オーディオから音素（phoneme）を検出し、自動的に口形状のキーフレームを生成する
リップシンク機能を追加する。Moho / AE のリップシンクに相当。

---

## 2. 現状

| 要素 | 状態 | 詳細 |
|------|:----:|------|
| `AudioAnalyzer` (FFT/スペクトル) | ✅ 実装済 | `AudioAnalyzer.cppm` |
| `AudioAnalyzer` (波形/音量) | ✅ 実装済 | 同上 |
| 音素検出 (phoneme detection) | ❌ 未実装 | — |
| 口形状マッピング | ❌ 未実装 | — |
| Timeline リップシンクマーカー | ❌ 未実装 | Marker Phase 1 完了後が前提 |
| Switch Layers (表情切替) | ❌ 未実装 | 口形状切替に最適 |
| `AudioAnalyzer` の強度検出 | ✅ `volumeRMS()` / `spectrumAt()` | ベースはある |

---

## 3. 設計

### 3.1 音素検出パイプライン

```
AudioBuffer → FFT → Formant Detection → Phoneme Classification → Mouth Shape
```

| ステップ | 方法 | 備考 |
|---------|------|------|
| **FFT** | `AudioAnalyzer::computeFFT()` 使用 | ✅ 既存 |
| **フォルマント検出** | FFTピークから F1/F2/F3 を抽出 | 新規実装 |
| **音素分類** | フォルマント位置 → 母音 (A/I/U/E/O) + 子音 | 5-8 形状に分類 |
| **口形状マッピング** | 音素 → 口形状インデックス | ユーザー定義可能 |

### 3.2 データモデル

```cpp
// 音素イベント
struct PhonemeEvent {
    FramePosition frame;
    QString phoneme;     // "A", "I", "U", "E", "O", "M", "F", "etc"
    float intensity;     // 0.0-1.0
    int mouthShapeIndex; // 0=閉じる, 1=あ, 2=い, 3=う, 4=え, 5=お
};

// リップシンクトラック（レイヤーにアタッチ）
class LipSyncTrack {
    AudioAnalyzer* analyzer_ = nullptr;
    std::vector<PhonemeEvent> events_;
    int mouthGroupId_;     // Switch Layer のグループID
    int defaultShape_ = 0; // 無音時の口形状
    
    bool analyze(const AudioSegment& segment, double frameRate);
    bool analyzeFromLayer(ArtifactAudioLayer* audioLayer);
    void applyToSwitchLayer(ArtifactSwitchLayer* switchLayer);
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};
```

### 3.3 フォルマント検出アルゴリズム

```cpp
struct FormantSet {
    float f1 = 0; // 300-1000Hz (開き具合)
    float f2 = 0; // 800-2800Hz (舌位置)
    float f3 = 0; // 2000-3500Hz (唇丸め)
};

// 母音判定基準:
// /a/: F1≈800, F2≈1200
// /i/: F1≈300, F2≈2300
// /u/: F1≈350, F2≈1300
// /e/: F1≈500, F2≈1800
// /o/: F1≈450, F2≈900
```

### 3.4 Switch Layer 連携

リップシンクの出力先として Switch Layer を使用:

```
SwitchLayer "Mouth"
  ├── Frame 0: "Closed" (閉じる)
  ├── Frame 1: "A/E" (開く)
  ├── Frame 2: "I" (横に引く)
  ├── Frame 3: "U/O" (丸める)
  ├── Frame 4: "M/B" (閉じる→開く)
  └── Frame 5: "F/V" (下唇)
```

---

## 4. 実装フェーズ

### Phase 1: フォルマント検出 (6-8h)
- `FormantExtractor` クラス (`ArtifactCore/src/Audio/`)
- FFT→フォルマントピーク抽出
- 5母音分類 (A/I/U/E/O)
- テスト with 既存 AudioAnalyzer

### Phase 2: LipSyncTrack データモデル (4-6h)
- `LipSyncTrack` クラス
- `PhonemeEvent` 構造体
- JSON 保存/復元
- Unit test

### Phase 3: Switch Layer 連携 (6-8h)
- Switch Layer のデータモデル実装
- LipSyncTrack → SwitchLayer の自動キーフレーム設定
- Timeline 表示

### Phase 4: UI (4-6h)
- Inspector にリップシンク解析ボタン
- 生成された口形状キーフレームの編集
- 音声波形 + 音素マーカーの Timeline 表示

**合計工数:** ~24h

---

## 5. 依存関係

| 依存 | 理由 |
|------|------|
| `AudioAnalyzer` | FFT + スペクトル解析の基盤 |
| Marker Foundation | 音素を Timeline マーカーとして表示 |
| Switch Layer | 口形状の切替先レイヤー |
| `ArtifactAudioLayer` | 解析対象のオーディオソース |

---

## 6. ガードレール

- `AudioAnalyzer` に新規メソッド追加は許可（private Impl の拡張）
- `QImage` / QtCSS / 新規 signal-slot は追加しない
- 音素検出の精度は「それっぽく見える」レベルで Phase1 完了とする
- `ArtifactWidgets` は触らない
