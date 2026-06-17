# M-TL-16 In/Out Slide Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`,
      `Artifact/src/Widgets/Timeline/ArtifactWorkAreaControlWidget.cppm`,
      `Artifact/src/Layer/ArtifactAbstractLayer.cppm`,
      `Artifact/src/Service/ArtifactLayerService.cppm`,
      `Artifact/src/Undo/*`,
      `ArtifactCore/include/Frame/FrameRange.ixx`,
      `ArtifactCore/include/Frame/FramePosition.ixx`
位置づけ: `MILESTONE_TIMELINE_RIPPLE_EDIT_PHASE1_EXECUTION_2026-06-04.md` と並ぶ基礎編集機能として、**source の取り直しを伴わない「時間窓だけを動かす」slide** を 1 つの表にまとめる。
参照:
- `docs/analysis/CORE_MODULE_MISSING_FEATURES_2026-04-19.md` (⚠️ 中優先)
- `docs/analysis/WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §4
- `docs/analysis/MOTION_GRAPHICS_AD_PRODUCTION_THINKING_MEMO_2026-05-28.md` (尺合わせ)
- `docs/planned/MILESTONE_TIMELINE_RIPPLE_EDIT_PHASE1_EXECUTION_2026-06-04.md`
- `docs/planned/MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md`
- `docs/planned/MILESTONE_TIMELINE_INDEX_2026-04-22.md`
- `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md`

---

## 1. 目的

`CORE_MODULE_MISSING_FEATURES_2026-04-19.md`:

> ⚠️ 中優先 イン点 / アウト点 スライド
> 現在は切り取りしかない

`MILESTONE_TIMELINE_RIPPLE_EDIT_PHASE1_EXECUTION_2026-06-04.md` の親 milestone `M-TL-15 Timeline Ripple Edit / Downstream Shift` は **後続 layer を詰める** ripple を入れたが、**slide（時間窓だけを平行移動）** は未対応。

AE の Slide 機能の本来の役割は「同じソース素材の表示窓を 0.5 秒後ろにずらして、尺は変えず、開始タイミングだけ調整する」という制作上頻出の操作。これが **ない** と、`Trim + Insert` の 2 ステップに分解してUndo スタックを汚すことになる。

`MOTION_GRAPHICS_AD_PRODUCTION_THINKING_MEMO_2026-05-28.md` の尺合わせとも合致する:
> 6 秒、15 秒、30 秒への再構成

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/Timeline/` 側に閉じる。サブモジュール（`ArtifactWidgets`）は触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `MILESTONE_TIMELINE_RIPPLE_EDIT_PHASE1_EXECUTION_2026-06-04.md` の `RippleTrimOutCommand / RippleTrimInCommand / RippleDeleteCommand` — **ripple は実装済み**
- `ArtifactWorkAreaControlWidget.cppm` — work area 専用の左 / 右ハンドル。**layer の in/out point には触らない**
- `ArtifactTimelineTrackPainterView.cpp` — clip edge の描画は限定的（grep 0 hit）
- `ArtifactAbstractLayer` 側に `inPoint() / outPoint() / startTime()` あり
- `ArtifactCore/include/Frame/FrameRange.ixx` — `FrameRange { start, end }`

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Slide mode | なし | 時間窓を平行移動する手段がない |
| Trim mode | 部分的（ripple のみ） | 通常 trim も弱い |
| Edge handle 描画 | `ArtifactTimelineTrackPainterView` に clip edge handle なし | 編集点が UI で見えない |
| Mouse drag | なし | ドラッグで in/out 編集不可 |
| Keyboard | `Alt+←/→` 等未登録 | 1 frame / 10 frame 移動なし |
| ソース範囲クランプ | なし | source を超える slide を防ぐ機構なし |
| Slide vs Ripple | UI 上 mode 切替なし | 混同事故の危険 |
| Undo | ripple は対応、slide はなし | 1 undo で slide 全体を戻せない |

### 2.3 既存 milestone との関係

- `MILESTONE_TIMELINE_RIPPLE_EDIT_PHASE1_EXECUTION_2026-06-04.md` — 上位。ripple の algorithm は **slide の superset** として扱える。本 milestone は ripple を **呼ばずに** 同じ layer だけ動かす
- `MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md` — right-pane 編集体験。本 milestone は clip 端の編集で補完
- `MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md` — shortcut 登録先
- `MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md` — keyframe 編集

---

## 3. 設計の柱

### 3.1 4 つの基本操作

| 操作 | 意味 | AE 相当 |
|---|---|---|
| **Slide** | in/out を平行移動。source 範囲は変えず時間窓だけ動かす | `Alt+Shift+,/.` |
| **Ripple Slide** | slide + 後続 clip を詰める | ripple |
| **Trim** | in/out を伸ばす/縮める。source の境界を内側に向ける | `,/.` |
| **Ripple Trim** | trim + 後続 clip を詰める | `Alt+,/.` |

本 milestone は **Slide** に絞る。Trim / Ripple Slide / Ripple Trim は Phase 6 で別 milestone に分離。

### 3.2 Slide の不変条件

- `inPoint` を `Δ` ずらすと `outPoint` も `Δ` ずらす
- `startTime` は **変えない**（source 素材の source time を保持）
- source 範囲 `[sourceStart, sourceEnd]` を超えない
- 後続 clip には **触らない**
- composition duration を超えない
- layer 内 keyframe は **平行移動に追従**（`inPoint` の slide なら keyframe も `Δ` 移動）

### 3.3 Edge handle 描画

`ArtifactTimelineTrackPainterView::paintClip` に edge handle を追加:

```
┌─[═══════]─┐         ← 通常の clip edge
   ↑       ↑
   left    right handle (4 px 幅)
```

- `left handle`: `inPoint` を `frame range [0, outPoint - 1]` 内でスライド
- `right handle`: `outPoint` を `frame range [inPoint + 1, duration]` 内でスライド
- 中央 drag: in/out を平行移動
- hover 時に cursor を `SizeHorC` に変更

### 3.4 Mouse drag 入力

- `mousePress` → drag start
- `mouseMove` → `SlideClipCommand` を preview モードで実行
- `mouseRelease` → `commit()` で確定
- `Escape` 中止

### 3.5 SlideClipCommand

`Artifact/Undo/SlideClipCommand.cppm` 新規:

```cpp
class SlideClipCommand : public QUndoCommand {
public:
    SlideClipCommand(const QString& layerId, FramePosition delta,
                     const QString& mode);   // "slide" / "ripple-slide" / "trim" / "ripple-trim"

    void undo() override;   // snapshot 復元
    void redo() override;

    // drag preview 用
    void setDelta(FramePosition delta);
};
```

- snapshot は `ArtifactAbstractLayer` の `inPoint / outPoint / startTime / keyframes_` を **まるごと保存**
- 1 undo で layer 全体を完全復元
- 複数 layer 選択時の **batch slide** は複数 command を `QUndoStack::beginMacro` で束ねる

### 3.6 ソース範囲のクランプ

`ArtifactAbstractLayer` に **source range メタ** を追加（既存 `sourceStartTime_` / `sourceEndTime_` があれば温存、無ければ `duration_` から導出）:

```cpp
struct SourceRange {
    FramePosition start;
    FramePosition end;     // exclusive
};

// 新規 API
SourceRange sourceRange() const;
FramePosition clampInPoint(FramePosition proposed) const;
FramePosition clampOutPoint(FramePosition proposed) const;
```

- `clampInPoint`: `[0, outPoint - 1]` かつ `sourceRange` 内
- `clampOutPoint`: `[inPoint + 1, duration]` かつ `sourceRange.end + startTime` 内

### 3.7 Mode 切替 UI

- Timeline ツールバーに **`Slide` ツール** を追加
- アイコンは `Artifact/App/Icon/Studio/` 配下のオリジナル SVG（既存 `filemenu_*.svg` 等の太めシルエット）
- `S` キーで切替
- 選択中の tool が `Slide` の間だけ edge handle が **太く** なる

### 3.8 Keyboard shortcut

- `Alt+← / Alt+→` — 選択 clip の `inPoint` を 1 frame 左 / 右に slide
- `Shift+Alt+← / Shift+Alt+→` — 10 frame
- `Ctrl+Alt+← / Ctrl+Alt+→` — `outPoint` を 1 frame 左 / 右に slide
- `Ctrl+Shift+Alt+← / Ctrl+Shift+Alt+→` — 10 frame
- `SHORTCUT_CONTEXT_MAP_2026-04-21.md` の `Panel.Timeline.Right` に登録

### 3.9 不変条件 (Guardrails)

- 既存 `RippleTrimOutCommand / RippleTrimInCommand / RippleDeleteCommand` は **温存**。本 milestone は新規 `SlideClipCommand` を追加するだけ
- `QImage` / `setStyleSheet` 流入禁止
- 新規 signal-slot 接続は設計レビュー必須
- source range を超える slide は **クランプ** で吸収（黙って clamp せず `severity=info` を `Project Health` に上げる）
- layer 端の mouse drag は `paintEvent` 内に閉じて `QGraphicsView` には依存しない
- `ArtifactTimelineTrackPainterView` 以外で edge handle ロジックを書かない
- 1 layer = 1 command。複数選択は `QUndoStack::beginMacro` で束ねる

---

## 4. フェーズ計画

### Phase 1: Edge handle 描画 (P0, 1 セッション)

- `ArtifactTimelineTrackPainterView::paintClip` に edge handle 描画
- hover 検出と cursor 変更
- handle 範囲を `hitTestClipEdge(frame, y) -> ClipEdge { Left, Right, None }` で返す

**Done criteria:**
- clip 端に 4 px 幅の handle が見える
- hover 時に cursor が `SizeHorC` に変わる
- 既存の paint 経路と干渉しない

### Phase 2: Mouse drag + SlideClipCommand (P0, 1〜2 セッション)

- `mousePress / mouseMove / mouseRelease` ハンドラ追加
- preview モードで `SlideClipCommand::setDelta` を連続呼出
- `commit()` で確定
- 1 undo で完全復元

**Done criteria:**
- left handle をドラッグすると `inPoint` だけ動く
- right handle をドラッグすると `outPoint` だけ動く
- 中央ドラッグで in/out が平行移動
- `Escape` で drag 中止
- 1 undo で 1 slide が完全復元

### Phase 3: Source range クランプ (P0, 1 セッション)

- `ArtifactAbstractLayer` に `sourceRange_` 追加
- `clampInPoint / clampOutPoint` 実装
- クランプ発生時は `Project Health` に `info` 通知

**Done criteria:**
- source 範囲を超える slide ができない
- 境界ぴったりまで slide できる
- クランプ発生が Problem View に反映

### Phase 4: Keyboard shortcut + Mode 切替 (P0, 1 セッション)

- `Alt+←/→ / Shift+Alt+←/→ / Ctrl+Alt+←/→` 実装
- `SHORTCUT_CONTEXT_MAP_2026-04-21.md` の `Panel.Timeline.Right` に登録
- `Slide` ツールを Timeline ツールバーに追加
- `S` キーで切替

**Done criteria:**
- 8 種の shortcut すべて動作
- `Slide` ツール選択中のみ edge handle が太くなる
- `S` キーで tool 切替

### Phase 5: 複数選択 + Project 保存 (P1, 1 セッション)

- 複数選択時の batch slide を `QUndoStack::beginMacro` で束ねる
- 既存 layer 1 個に 1 command は変えない
- project JSON への永続化は **layer 1 個の in/out として既に永続化済み**。スライド結果はそのまま保存

**Done criteria:**
- 3 layer 選択 → 同時に slide → 1 undo で 3 layer 全て復元
- project 保存 → 再読込で slide 結果が完全復元

### Phase 6: Trim / Ripple Slide / Ripple Trim (P2, 別 milestone 推奨)

- 本 milestone の **Phase 6** はエントリポイントだけ
- 別 `MILESTONE_TIMELINE_TRIM_RIPPLE_2026-XX-XX.md` を起こす

**Done criteria (本 milestone 内):**
- 将来 milestone のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_TIMELINE_RIPPLE_EDIT_PHASE1_EXECUTION_2026-06-04.md` | ripple。本 milestone は ripple を呼ばない slide で棲み分け。 |
| `MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md` | right-pane 編集体験。並走。 |
| `MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md` | shortcut 登録先。 |
| `MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md` | keyframe 編集。 |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | diagnostics 文法。 |
| `MILESTONE_BLEND_MODE_DESIGN_2026-06-16.md` | 別 topic。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **Ripple との分離**。Slide のつもりで ripple が走ると後続が崩れる。tool 切替で明示分離
2. **Source range 未設定 layer**。image layer 等は `sourceStartTime_` を持っていない場合がある。`clampInPoint` は `duration` から導出する fallback
3. **ドラッグ中の preview 性能**。`paintEvent` 連続呼出で CPU 負荷。`setDelta` 内部で dirty フラグ管理
4. **複数選択時の command family**。`QUndoStack::beginMacro` の使い回し。既存 ripple と同形式で
5. **`Slide` ツールのアイコン**。`Artifact/App/Icon/Studio/` 配下に新規 SVG を置く。既存 `filemenu_*.svg` 等の太めシルエットを参照

### 6.2 契約上の未解決

- **Trim mode との UI 区別**。trim は AE で `,/.` キーだが、Artifact で何に割り当てるか。Phase 4 で `T` 系との衝突を確認
- **Audio / video layer の slide 挙動**。`ArtifactAudioLayer` は `inPoint` で再生位置を切るが、source range は別途扱う。Phase 3 で audio / video 別に動作確認
- **Source を超える slide**。AE では `trim source` で `source range` を内側に縮める。本 milestone は **クランプ** が default。`Force source` オプションを Phase 6 で検討
- **Nested composition**。`ArtifactCompositionLayer` 内の layer を slide した場合、parent への伝播は別途

### 6.3 サブモジュール境界

- `ArtifactCore/include/Frame/FrameRange.ixx` は **既存型をそのまま使用**。破壊変更なし
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp` に追加実装
- `ArtifactWidgets` は触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- clip 端に edge handle が見え、hover で cursor が変わる
- mouse drag で in/out 平行移動ができる
- 1 undo で 1 slide が完全復元
- source 範囲を超える slide はクランプされ、Problem View に `info` で通知
- 8 種の keyboard shortcut が動作
- `Slide` ツールを `S` キーで切替できる
- 3 layer 選択時の batch slide が 1 undo で完全復元
- project 保存 → 再読込で slide 結果が完全復元
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot が増えていない
- 既存 `RippleTrim* / RippleDelete` command は温存
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §4 を正式 milestone に起こした。
