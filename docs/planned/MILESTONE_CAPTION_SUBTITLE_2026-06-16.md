# M-CAPTION-1 Caption / Subtitle Milestone (SRT / WebVTT)

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Layer/ArtifactTextLayer.cppm`,
      `Artifact/src/Widgets/ArtifactCompositionEditor.cppm`,
      `Artifact/src/Service/ArtifactRenderQueueService.cppm`,
      `Artifact/src/Widgets/Render/ArtifactRenderOutputSettingDialog.cppm`,
      `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm`,
      `Artifact/src/Project/ArtifactProjectManager.cppm`,
      `ArtifactCore/include/Time/TimeCode.ixx`
位置づけ: 字幕 (caption) を **SRT / WebVTT 経由でインポート / エクスポート** する foundation。AE の字幕機能 / Premiere の Caption Panel 互換。
参照:
- `docs/analysis/REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.9
- `docs/analysis/DESIRED_IMPORT_FORMATS_2026-04-19.md` (`.srt / .ass` 言及)
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` (P1/P2)
- `docs/planned/MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md`
- `docs/planned/MILESTONE_SOURCE_TEXT_KEYFRAME_2026-06-16.md`
- `docs/planned/MILESTONE_RENDER_FORMAT_EXPANSION_2026-06-16.md` (SRT export 受け皿)

---

## 1. 目的

`REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.9:

> - Captions / Subtitles (CEA-708): 0 hit
> - Closed caption export (SRT): 0 hit
> - Subtitle (SRT / WebVTT) import: 0 hit

字幕は **動画納品で必須**。特に YouTube / Vimeo / Twitter / TikTok 等の web 投稿と、放送業界 (CEA-708) の両方で需要がある。

現状の `ArtifactTextLayer` は **時間変化する text** を持つが、`M-TXT-3 Source Text Keyframe` のような keyframe ベースの approach で、**SRT / WebVTT 経由の一括取り込み** には対応していない。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/` 側に閉じる。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `ArtifactCore/include/Time/TimeCode.ixx` — timecode 表現
- `Artifact/src/Layer/ArtifactTextLayer.cppm` — text layer (default text のみ、keyframe は M-TXT-3 で対応予定)
- `MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md` — text インライン編集
- `MILESTONE_SOURCE_TEXT_KEYFRAME_2026-06-16.md` — Source Text keyframe
- `MILESTONE_RENDER_FORMAT_EXPANSION_2026-06-16.md` — encoder 拡張

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| SRT import | 0 hit | 字幕付き動画の import 不可 |
| WebVTT import | 0 hit | web 字幕 import 不可 |
| SRT export | 0 hit | 字幕ファイル出力不可 |
| WebVTT export | 0 hit | web 字幕 export 不可 |
| CEA-708 export | 0 hit | 放送業界字幕 export 不可 |
| TextLayer との接続 | なし | 字幕 → text layer 自動生成なし |
| Project 保存 | なし | 字幕付きプロジェクト復元不可 |
| Diagnostics | なし | 不正 SRT / VTT 検出なし |

### 2.3 既存 milestone との関係

- `MILESTONE_SOURCE_TEXT_KEYFRAME_2026-06-16.md` — 関連。本 milestone は SRT / WebVTT からの **取り込み**、Source Text Keyframe は **手動編集**。両者を並走
- `MILESTONE_RENDER_FORMAT_EXPANSION_2026-06-16.md` — export。本 milestone は SRT / WebVTT の export 受け皿
- `MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md` — text 編集 UI

---

## 3. 設計の柱

### 3.1 Caption Track データモデル

`ArtifactCore/include/Text/CaptionTrack.ixx` を新規追加:

```cpp
namespace ArtifactCore {

struct CaptionCue {
    int index;                   // 1-based
    int64_t startMs;             // milliseconds
    int64_t endMs;
    QString text;
    QString speaker;             // 任意
    QPointF position;            // 任意 (CEA-708 style)
    QStringList tags;             // 任意 ("music", "sfx" 等)

    // JSON 永続化
    QJsonObject toJson() const;
    static CaptionCue fromJson(const QJsonObject& obj);
};

class CaptionTrack {
public:
    void addCue(const CaptionCue& cue);
    void removeCue(int index);
    void updateCue(const CaptionCue& cue);
    QList<CaptionCue> cues() const;
    CaptionCue cueAt(int64_t timeMs) const;

    // 形式変換
    QString toSrt() const;
    static CaptionTrack fromSrt(const QString& srt);

    QString toWebVtt() const;
    static CaptionTrack fromWebVtt(const QString& vtt);

    QString toCea708() const;    // CEA-708 binary
    static CaptionTrack fromCea708(const QString& bin);

    // 永続化
    QJsonObject toJson() const;
    static CaptionTrack fromJson(const QJsonObject& obj);
};

} // namespace ArtifactCore
```

- SRT: 1-based index + `HH:MM:SS,mmm --> HH:MM:SS,mmm` + text
- WebVTT: SRT と似ているが `HH:MM:SS.mmm` 形式と `WEBVTT` ヘッダ
- CEA-708: binary。本 milestone では `toCea708 / fromCea708` を **スタブ** として Phase 6 で対応

### 3.2 SRT Parser / Writer

`ArtifactCore/src/Text/SrtParser.cppm` を新規追加:

```cpp
class SrtParser {
public:
    static CaptionTrack parse(const QString& srt);
    static QString write(const CaptionTrack& track);
};
```

- SRT 形式:
  ```
  1
  00:00:01,000 --> 00:00:04,500
  Hello, world.

  2
  00:00:05,000 --> 00:00:08,000
  Next line.
  ```
- 1 行空行で cue 区切り
- `<i>...</i>` / `<b>...</b>` の簡易 inline タグ対応
- BOM / CRLF / LF すべて対応
- 不正な timestamp は `severity=warning` を emit

### 3.3 WebVTT Parser / Writer

`ArtifactCore/src/Text/WebVttParser.cppm`:

```cpp
class WebVttParser {
public:
    static CaptionTrack parse(const QString& vtt);
    static QString write(const CaptionTrack& track);
};
```

- WebVTT 形式:
  ```
  WEBVTT

  00:00:01.000 --> 00:00:04.500
  Hello, world.
  ```
- cue settings (line, position, align) は将来拡張

### 3.4 Composition への統合

`ArtifactComposition` に **`CaptionTrack` 配列** を追加:

```cpp
class ArtifactComposition {
    // 既存
    void addCaptionTrack(const QString& name, const CaptionTrack& track);
    CaptionTrack captionTrack(const QString& name) const;
    QList<QString> captionTrackNames() const;
};
```

- composition 単位で複数の caption track を持てる
- 既定の track 名: `Subtitles` (default), `Closed Captions` (broadcast)
- track ごとに **言語** (`ja`, `en` 等) を `QString` で保持可能

### 3.5 TextLayer への自動マッピング

`Composition` の `CaptionTrack` を **1 個の text layer に焼き込む** 機能:

```cpp
class CaptionTrackToTextLayer {
public:
    // composition の caption track を text layer に変換
    static ArtifactAbstractLayer* bakeToTextLayer(ArtifactComposition* comp,
                                                   const QString& trackName,
                                                   const QString& textLayerName);
};
```

- 1 cue = 1 source text keyframe
- 開始 / 終了 frame は `startMs / endMs` から導出
- text は cue の text
- 既存 `M-TXT-3 Source Text Keyframe` の `SourceTextKeyframeTrack` に変換

### 3.6 UI 露出

`ArtifactCompositionEditor` の右側に **`Caption Panel`** を追加:

- Track 一覧
- 各 track の cue 一覧
- `+ Import SRT` / `+ Import WebVTT` ボタン
- `Export SRT` / `Export WebVTT` ボタン
- `Bake to Text Layer` ボタン
- cue 選択時の編集 (start / end / text)

### 3.7 字幕焼き込み (Burn-in)

`M-EXPORT-1` の `EncoderKind` 拡張として `H264_MP4_Burnin` のような派生を追加するか、`ArtifactCompositionRenderController` の **render path 末尾** で text を焼き込む:

- `composition.captionTrackNames()` から default track を取得
- playback frame 時点で `cueAt(timeMs)` の text を overlay
- 一時 text layer を frame に composite

### 3.8 Project 保存

- `ArtifactProjectManager` の project JSON に `composition.captionTracks[]` 追加
- 各 track: name / language / cues[]
- 旧プロジェクトは captionTracks 欠落を許容

### 3.9 不変条件 (Guardrails)

- `ArtifactWidgets` 触らない
- `QImage` 流入禁止
- 新規 signal-slot 接続は `captionTrackAdded / captionTrackChanged / captionCueUpdated` の 3 個に限定
- 既存 text layer API は温存
- SRT / WebVTT parser は **strict mode / lenient mode** を切替可能。`M-CE-CRIT-1` の smoke は strict
- CEA-708 は本 milestone では stub。Phase 6 で詳細

### 3.10 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `caption.srt.invalid-timestamp` (severity=warning, 無効 timestamp)
- `caption.srt.overlap` (severity=info, cue 重複)
- `caption.srt.empty-cue` (severity=info, 空 text)
- `caption.track.missing-default` (severity=info, default track 未設定)
- `caption.burnin.fps-mismatch` (severity=warning, cue frame rate と comp frame rate 不一致)

---

## 4. フェーズ計画

### Phase 1: Core data + SRT parser (P0, 1〜2 セッション)

- `ArtifactCore/include/Text/CaptionTrack.ixx` 新規
- `ArtifactCore/include/Text/SrtParser.ixx` 新規
- `ArtifactCore/src/Text/CaptionTrack.cppm` 実装
- `ArtifactCore/src/Text/SrtParser.cppm` 実装
- 永続化 (toJson / fromJson)

**Done criteria:**
- SRT 1 ファイル (3〜5 cue) を `parse` / `write` round-trip
- BOM / CRLF / LF すべて対応
- 無効 timestamp は warning を emit

### Phase 2: WebVTT parser (P0, 1 セッション)

- `ArtifactCore/include/Text/WebVttParser.ixx` 新規
- `ArtifactCore/src/Text/WebVttParser.cppm` 実装
- SRT parser と並走

**Done criteria:**
- WebVTT 1 ファイルを `parse` / `write` round-trip
- `WEBVTT` ヘッダ保持

### Phase 3: Composition 統合 (P0, 1 セッション)

- `ArtifactComposition` に `captionTracks[]` 追加
- `addCaptionTrack / captionTrack / captionTrackNames` 実装
- project JSON に保存

**Done criteria:**
- composition 単位で複数の track を持てる
- project 保存 → 再読込で復元
- 旧プロジェクトが開ける

### Phase 4: TextLayer への焼き込み (P0, 1〜2 セッション)

- `CaptionTrackToTextLayer::bakeToTextLayer` 実装
- `M-TXT-3 Source Text Keyframe` の `SourceTextKeyframeTrack` を生成
- 1 cue = 1 keyframe

**Done criteria:**
- SRT を bake すると text layer に Source Text Keyframe が並ぶ
- 同じ SRT を再 export しても cue が一致

### Phase 5: Caption Panel UI (P0, 1〜2 セッション)

- `ArtifactCompositionEditor` 右側に Caption Panel
- Track 一覧 / cue 一覧 / import / export / bake
- 既存 menu からも起動可能

**Done criteria:**
- UI から SRT / WebVTT import できる
- export ボタンで書き出し
- bake ボタンで text layer 自動生成

### Phase 6: 字幕焼き込み (Burn-in) + CEA-708 (P0, 1〜2 セッション)

- `ArtifactCompositionRenderController` の render 末尾で default track の text を overlay
- `EncoderKind` 拡張で `H264_MP4_Burnin` 追加
- CEA-708 binary 出力は stub 実装

**Done criteria:**
- video export 時に字幕が焼き込まれる
- CEA-708 export のスタブが動作
- text layer と重複しない

### Phase 7: Diagnostics + 設定 (P1, 1 セッション)

- Problem View への `caption.*` 健全性 contribution
- ApplicationSettingDialog に字幕設定ページ
- default track / language 選択

**Done criteria:**
- `caption.srt.invalid-timestamp` 等が Problem View に表示
- 設定ダイアログから default track 変更

### Phase 8: ASR / AI 字幕 (P2, 別 milestone 推奨)

- 音声認識による自動字幕
- AI 翻訳
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_CAPTION_AI_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_SOURCE_TEXT_KEYFRAME_2026-06-16.md` | 関連。本 milestone は SRT 経由、Source Text Keyframe は手動。 |
| `MILESTONE_RENDER_FORMAT_EXPANSION_2026-06-16.md` | export 受け皿。Burn-in encoder を並走。 |
| `MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md` | text 編集 UI。 |
| `MILESTONE_OCIO_INTEGRATION_2026-06-16.md` | 別 topic。 |
| `MILESTONE_AI_ASSISTED_FEATURES_2026-03-29.md` | 将来 ASR 字幕。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **SRT timestamp フォーマット**。`,` 区切り (SRT) と `.` 区切り (WebVTT) の混在
2. **Cue overlap**。複数 track が同時間帯で重なる場合。priority 設定
3. **Burn-in と text layer の衝突**。同名の text layer が既に存在する場合の挙動
4. **CEA-708 binary format**。Phase 6 で stub。フル実装は別 milestone
5. **SRT / WebVTT 以外の字幕形式**。TTML / IMSC / ASS など。Phase 8 以降

### 6.2 契約上の未解決

- **multi-language 字幕**。composition に `captionTracks[]` を持つが、字幕切替の UX は未設計
- **字幕スタイル**。bold / italic / color / position。WebVTT のみ対応
- **字幕付き export**。Burn-in 以外 (別 track) は本 milestone のスコープ外
- **Speech bubble**。アニメーション付き吹き出し。Phase 8 以降

### 6.3 サブモジュール境界

- `ArtifactCore/include/Text/CaptionTrack.ixx` を新規追加
- `ArtifactCore/src/Text/CaptionTrack.cppm` を新規追加
- `ArtifactCore/src/Text/SrtParser.cppm` / `WebVttParser.cppm` を新規追加
- `ArtifactCore/CMakeLists.txt` に登録
- `ArtifactWidgets` は触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- SRT / WebVTT を import → CaptionTrack として保持
- CaptionTrack を export → 元の SRT / WebVTT と同等
- CaptionTrack を text layer に bake できる
- video export 時に default track の字幕が焼き込まれる
- CEA-708 export のスタブが動作
- 旧プロジェクトは captionTracks 欠落を許容
- project 保存 → 再読込で caption track 復元
- Problem View に `caption.*` 健全性表示
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot が増えていない
- 既存 `ArtifactTextLayer` API が温存
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.9 / §4 を正式 milestone に起こした。Caption / Subtitle foundation。
