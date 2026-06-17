# M-TXT-3 Source Text Keyframe Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Layer/ArtifactTextLayer.cppm`,
      `Artifact/src/Widgets/Inspector/ArtifactTextLayerPanel.cppm`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeModel.cppm`,
      `Artifact/src/Undo/*`,
      `ArtifactCore/src/Text/TextAnimator.cppm`,
      `ArtifactCore/include/Text/TextAnimator.ixx`,
      `Artifact/src/Project/ArtifactProjectManager.cppm`
位置づけ: 既存 `Text Animator` (glyph 単位) に対して **layer 単位** の source text 時間変化を追加。AE の "Source Text" キーフレーム互換の foundation。
参照:
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` (#18)
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` (P0)
- `docs/analysis/WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §5
- `docs/planned/MILESTONE_TEXT_EFFECT_WORKFLOW_BRIDGE_2026-05-25.md`
- `docs/planned/MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md`
- `docs/planned/MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md`
- `docs/planned/MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md`

---

## 1. 目的

`FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` (#18):

> Source Text keyframe
> `setSourceText` 等無し。テキスト編集はインライン編集のみ

`AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` の P0 に分類。

AE では「**Source Text**」プロパティに **時間変化する文字列**をキーフレームで打てる。例:

- 00:00:00 — "Apple"
- 00:00:30 — "Orange"
- 00:01:00 — "Banana"

これが **ない** ことで、テロップ差し替えを動画リテラルでしか行えず、広告量産時の差し替え工程が破綻する。

> 重要: 既存 `Text Animator` (`ArtifactCore/src/Text/TextAnimator.cppm`) は **glyph 単位**（位置 / 透明度 / scale 等の glyph-level アニメ）。本 milestone は **layer 単位** の source text 時間で、責務が直交する。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/` 側に閉じる。サブモジュール（`ArtifactWidgets`）は触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `ArtifactCore/src/Text/TextAnimator.cppm` — `TextAnimatorEngine` が glyph-level で `RangeSelector` / `WigglySelector` / `AnimatorProperties` を扱う
- `ArtifactCore/include/Text/TextAnimator.ixx` — 構造体 + 関数
- `Artifact/src/Layer/ArtifactTextLayer.cppm` — text layer 本体
- `MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md` — Text Animator の timeline 統合
- `MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md` — Text Animator engine と layer 統合

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Source Text データ | なし。`text` は単一値 | 時間で変化しない |
| `setSourceText` API | 不在 | プログラムからも触れない |
| Timeline 上の text content track | なし | keyframe が打てない |
| Inspector 露出 | なし | 編集導線がない |
| Text Animator との precedence | なし | 両方が active なときの挙動未定義 |
| 永続化 | `text` 単一値のみ | 履歴を保存できない |
| 永続化と reload | layer 単位 keyframe 配列は未対応 | project 保存すると消える |

### 2.3 既存 milestone との関係

- `MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md` — Text Animator。本 milestone は layer 単位の source text で **直交** する
- `MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md` — text インライン編集。本 milestone は inline editor を source text 編集に流用
- `MILESTONE_TEXT_EFFECT_WORKFLOW_BRIDGE_2026-05-25.md` — text / effect workflow。本 milestone は source text track で補完
- `MILESTONE_MARKER_FOUNDATION_2026-06-16.md` — Marker 帯。Timeline 上の text content track は別の帯として並走

---

## 3. 設計の柱

### 3.1 Source Text Keyframe データ

`ArtifactCore/include/Text/SourceTextKeyframe.ixx` を新規追加:

```cpp
namespace ArtifactCore {

struct SourceTextKeyframe {
    FramePosition frame;
    QString text;
    InterpolationType interpolation;   // Hold が default（AE と同じ）
};

class SourceTextKeyframeTrack {
public:
    void addKeyframe(const SourceTextKeyframe& kf);
    void removeKeyframe(FramePosition frame);
    void setAllKeyframes(const QList<SourceTextKeyframe>& kfs);
    QList<SourceTextKeyframe> allKeyframes() const;

    // 時刻 t での評価
    QString evaluate(FramePosition frame, const QString& defaultText) const;
    bool hasAny() const;

    // 永続化
    QJsonObject toJson() const;
    static SourceTextKeyframeTrack fromJson(const QJsonObject& obj);
};

} // namespace ArtifactCore
```

- 評価は **Hold** が default。frame の **直前の entry** の `text` を返す
- entry 不在時は `defaultText` を返す
- `QString` ベースで **CJK / 縦書き / フォント fallback** は既存 `Text` 基盤にそのまま乗る

### 3.2 ArtifactTextLayer への組み込み

`Artifact/src/Layer/ArtifactTextLayer.cppm` に:

- `SourceTextKeyframeTrack sourceTextKeyframes_` 追加
- API:
  - `void setSourceText(FramePosition frame, const QString& text)` — keyframe 追加
  - `QString sourceText(FramePosition frame) const` — 現 frame の text 取得
  - `QString defaultText() const` — ベース text
  - `void setDefaultText(const QString& text)`
  - `SourceTextKeyframeTrack& sourceTextKeyframes()` — track 取得
  - `void evaluateAt(FramePosition frame)` — layer 内部状態を更新

- 既存 `text` プロパティは **`defaultText_`** にリネーム or alias 維持。Phase 1 で互換確認

### 3.3 Timeline 上の Text Content Track

`ArtifactTimelineTrackPainterView` に **Text Content トラック** を追加:

- 各 text layer の row 直下に細い 1 行帯
- keyframe マークは通常 keyframe と同じ形状（◆）
- 帯の幅は `[inPoint, outPoint]` 範囲のみ
- クリックで inspector の `Source Text` パネルへジャンプ
- ダブルクリックで **inline editor** を開く（既存 `MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md` 経由）
- color は layer 本体とは別の theme token

### 3.4 Inspector の Source Text パネル

`ArtifactTextLayerPanel`（または `ArtifactInspectorWidget` 配下）に **`Source Text`** パネルを追加:

- `Default Text` 入力欄
- `Keyframes` 一覧
  - 各行: frame / text preview / edit / jump / remove
- `+ Add Keyframe at Playhead` ボタン
- `*` キービート（テンキー `*`）で playhead 位置に新規 keyframe
- `Ctrl+Shift+T` で panel を開く / 閉じる

### 3.5 Text Animator との precedence

- **layer 単位の `text` 決定**:
  1. `Source Text Keyframe` が存在すれば `evaluate(frame)` を使う
  2. なければ `defaultText` を使う
- **glyph 単位の Animator 適用**:
  1. layer 単位 text を確定
  2. 既存 `TextAnimatorEngine` を適用（位置 / 透明度 / scale 等）
- AE と同じ順序。**Source Text > Text Animator > Default**

### 3.6 Undo

`Artifact/Undo/SourceTextKeyframeCommand.cppm` 新規:

```cpp
class SourceTextKeyframeCommand : public QUndoCommand {
public:
    enum class Op { Add, Remove, EditText, EditFrame };

    SourceTextKeyframeCommand(const QString& layerId, Op op,
                              const SourceTextKeyframe& before,
                              const SourceTextKeyframe& after);

    void undo() override;
    void redo() override;
};
```

- 1 undo で 1 keyframe 操作
- 複数 keyframe 一括削除は `QUndoStack::beginMacro` で束ねる

### 3.7 永続化

- `ArtifactProjectManager` の project JSON に `textLayer.sourceTextKeyframes[]` 追加
- 各 entry: `frame / text / interpolation`
- 旧プロジェクトは `sourceTextKeyframes` 欠落を許容
- 復元時 `SourceTextKeyframeTrack::fromJson` で再構築

### 3.8 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `text.source.empty-keyframe` (severity=info, 空 text keyframe)
- `text.source.out-of-range` (severity=error, frame が `[inPoint, outPoint]` 外)
- `text.source.duplicate-frame` (severity=warning, 同 frame に複数 keyframe)
- `text.source.huge-text` (severity=info, 1 keyframe の text が 1 KB 超)

### 3.9 不変条件 (Guardrails)

- `QImage` / `setStyleSheet` 流入禁止
- 既存 `Text Animator` engine には **触らない**。layer 側で `evaluate` を呼び、`Text Animator` への入力は確定後
- 既存 `MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md` の inline editor を source text 編集に **流用**。二重実装禁止
- 新規 signal-slot 接続は `setSourceText` 1 個に限定
- 既存 `text` プロパティは **温存**。`defaultText_` として alias 維持
- CJK / 縦書き / フォント fallback は既存 `Text` 基盤にそのまま乗る
- text content track は **Timeline 上で 1 layer 1 track** に閉じる

---

## 4. フェーズ計画

### Phase 1: Core data + ArtifactTextLayer API (P0, 1〜2 セッション)

- `ArtifactCore/include/Text/SourceTextKeyframe.ixx` 新規
- `Artifact/src/Layer/ArtifactTextLayer.cppm` に `sourceTextKeyframes_` 追加
- `setSourceText / sourceText / defaultText / setDefaultText` 実装
- 既存 `text` プロパティは `defaultText_` として alias 維持

**Done criteria:**
- 1 keyframe 追加 → 別 frame で `sourceText(frame)` が正しい値を返す
- Hold interpolation が default
- `SourceTextKeyframeTrack::toJson / fromJson` が round-trip

### Phase 2: Timeline Text Content トラック (P0, 1〜2 セッション)

- `ArtifactTimelineTrackPainterView` に Text Content トラック描画
- keyframe マーク表示
- クリック → inspector ジャンプ、ダブルクリック → inline editor
- `color = theme token` 経由

**Done criteria:**
- text layer の row 直下に keyframe マークが見える
- 既存 Timeline 描画と干渉しない
- クリックで panel ジャンプ、ダブルクリックで inline editor

### Phase 3: Inspector Source Text パネル (P0, 1〜2 セッション)

- `ArtifactTextLayerPanel` に Source Text パネル
- keyframe 一覧、add / edit / remove / jump
- `*` キービート、`Ctrl+Shift+T` shortcut
- `SourceTextKeyframeCommand` 追加

**Done criteria:**
- panel から keyframe 追加 / 編集 / 削除 / ジャンプ
- `*` キービートで playhead 位置に新規 keyframe
- 1 undo で 1 keyframe 操作が復元

### Phase 4: Text Animator precedence + 永続化 (P0, 1 セッション)

- layer 側 `evaluateAt(frame)` で Source Text → Text Animator 順を保証
- `ArtifactProjectManager` の project JSON に `textLayer.sourceTextKeyframes[]` 追加
- 旧プロジェクトの後方互換

**Done criteria:**
- Source Text Keyframe と Text Animator が同 layer で動作
- project 保存 → 再読込で完全復元
- 旧プロジェクトが開ける

### Phase 5: Diagnostics (P1, 1 セッション)

- Problem View への `text.source.*` 健全性 contribution
- `FrameDebugSnapshot.resources` に `label=Text Source Health` を追加

**Done criteria:**
- Problem View に `text.source.empty-keyframe` 等が出る
- text out-of-range keyframe を warning として検出

### Phase 6: Expression 統合 (P2, 別 milestone 推奨)

- 既存 Expression に `text.sourceText(frame)` 相当を追加
- text 評価を expression 内で参照可能に

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_TEXT_SOURCE_EXPRESSION_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md` | Text Animator (glyph 単位)。本 milestone は layer 単位で直交。 |
| `MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md` | text インライン編集。本 milestone は inline editor を source text に流用。 |
| `MILESTONE_TEXT_EFFECT_WORKFLOW_BRIDGE_2026-05-25.md` | text / effect workflow。並走。 |
| `MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md` | engine 統合。 |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | diagnostics 文法。 |
| `MILESTONE_MARKER_FOUNDATION_2026-06-16.md` | 別 topic。Timeline 上で並走。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **Text Animator との precedence 評価順**。`evaluate` 順が逆だと Text Animator が古い text を見る。Phase 4 で実測
2. **Inline editor との接続**。`MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md` の editor を source text 編集に **そのまま** 流用できるか確認
3. **CJK / 縦書き**。`QString` ベースなので問題はないが、layout cache が source text 切替時に無効化される必要がある
4. **複数の text 評価**。`evaluateAt` の呼び出しタイミング。`renderOneFrame` 内で 1 回
5. **expression 統合**。`text.sourceText` を expression から呼べるようにするには、Phase 6 で別途 `ExpressionEvaluator` 拡張

### 6.2 契約上の未解決

- **Interpolated text**。AE 風の intermediate frame 補間（"Apple" → "Apqle"）は **未対応**。Hold 固定。Phase 6 以降で別 milestone
- **Path text / box text 区別**。text layer の種類による `evaluate` 差異。Phase 1 で確認
- **Default text と source text の precedence**。両方が active な場合、Source Text を優先。仕様確定
- **Undo 粒度**。`SourceTextKeyframeCommand` 1 個で 1 keyframe 操作。複数 keyframe 一括削除は `beginMacro` 束ね
- **Timeline track UI**。layer 1 個 1 track。複数 track に分けるかは Phase 2 で決定

### 6.3 サブモジュール境界

- `ArtifactCore/include/Text/SourceTextKeyframe.ixx` を新規追加
- `ArtifactCore/src/Text/SourceTextKeyframe.cppm` を新規追加
- `ArtifactCore/CMakeLists.txt` に登録
- `ArtifactWidgets` は触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- 1 layer に複数 keyframe を打て、`evaluate(frame)` が正しい text を返す
- Timeline 上の text content track に keyframe マークが見える
- クリックで inspector ジャンプ、ダブルクリックで inline editor
- Inspector の Source Text パネルから add / edit / remove / jump
- `*` キービート、`Ctrl+Shift+T` shortcut 動作
- 1 undo で 1 keyframe 操作復元
- Source Text > Text Animator > Default の precedence 保証
- project 保存 → 再読込で完全復元
- 旧プロジェクトは `sourceTextKeyframes` 欠落を許容
- Problem View に `text.source.*` 健全性表示
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot が増えていない
- 既存 `Text Animator` engine に触れていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §5 を正式 milestone に起こした。`Text Animator` (glyph 単位) と直交する layer 単位の source text keyframe。
