# M-CLIP-1 Keyframe Copy & Paste Milestone

作成日: 2026-06-16
ステータス: Completed
対象: `ArtifactCore/src/Animation/Value*`,
      `ArtifactCore/src/Animation/Transform2D*`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeModel.cppm`,
      `Artifact/src/Service/ArtifactPropertyService.cppm`,
      `Artifact/src/Service/ArtifactLayerService.cppm`,
      `Artifact/src/Undo/*`,
      `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm`
位置づけ: `MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` 全体構想から **Keyframe 部分だけを分離** して、Timeline から即座に使える 1 機能に絞った実装計画。
完了記録: [`../done/MILESTONE_KEYFRAME_COPY_PASTE_2026-06-16.md`](../done/MILESTONE_KEYFRAME_COPY_PASTE_2026-06-16.md)
参照:
- `docs/analysis/MOTION_GRAPHICS_ARTIST_PAIN_POINTS_2026-04-19.md` (⭐🌟🌟🌟)
- `docs/analysis/CORE_MODULE_MISSING_FEATURES_2026-04-19.md` (🟡)
- `docs/analysis/WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §3
- `docs/planned/MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` (Not Started)
- `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`
- `docs/planned/MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md`
- `docs/planned/MILESTONE_KEYFRAME_NUDGE_AND_TEMP_SNAP_OVERRIDE_2026-06-07.md`
- `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md`

---

## 1. 目的

`CORE_MODULE_MISSING_FEATURES_2026-04-19.md`:

> 🟡 キーフレームコピーペースト
> レイヤー間でキーを移動出来ない

`MOTION_GRAPHICS_ARTIST_PAIN_POINTS_2026-04-19.md` の歓び度 ⭐🌟🌟🌟:

> 9. 「このレイヤーのキーフレームを全部他のレイヤーにコピー」

AE では Timeline 上で `Ctrl+C / Ctrl+V` で複数 keyframe を別 layer / 別 property に移動できる。これが **ない** ことで、AE からの移行ユーザーが最も困るとされる編集操作の 1 つ。

`MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` は Layer / Effect / Property まで含む大きな構想だが、**3 年近く未着手**。本 milestone は **Keyframe だけを分離** して 1 機能に絞り、タイムリーに着手できる単位に切る。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/Timeline/` 側に閉じる。サブモジュール（`ArtifactWidgets`）は触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` 内で以下の設計が既出:
  - `KeyframeClipData` 構造体案
  - `ArtifactClipboardManager::copyKeyframes / pasteKeyframes` API 案
  - MIME type `application/x-artifact-keyframes`
- `ArtifactTimelineKeyframeModel.cppm` — Timeline 上 keyframe 表示は実装済み
- `ArtifactTimelineTrackPainterView.cpp` — `markers_` / `selection_` / 矩形選択 / batch move の足場あり
- `ArtifactAbstractLayer` 側に `AnimatableValueT<T>::at(frame)` で値評価

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Clipboard service | `ArtifactClipboardManager` 未実装 | copy/paste 不可 |
| Keyframe 抽出 API | `AnimatableValueT<T>` に `keyframes()` 一覧 API が薄い | 全 keyframe を取り出す経路が弱い |
| Timeline 入力 | `Ctrl+C / Ctrl+V` の keymap 登録なし | 入力不可 |
| 右クリック menu | `Copy Keyframes / Paste Keyframes Here` 不在 | 操作導線なし |
| 異名 property mapping | なし | 別 property に paste 不可 |
| 衝突時挙動 | なし | 既存 keyframe がある位置の挙動が未定義 |
| Undo | なし | 1 undo で paste 全体を戻す粒度 |
| 永続化 | なし | 保存しても clipboard 内容は無関係 |

### 2.3 既存 milestone との関係

- `MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` — 親構想。本 milestone は **Keyframe 部分を抜き出した Sub-milestone**
- `MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md` — Timeline 上 keyframe 編集。本 milestone は copy/paste でその上を補う
- `MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md` — right-pane 編集体験。本 milestone は `Ctrl+C/V` で完結
- `MILESTONE_KEYFRAME_NUDGE_AND_TEMP_SNAP_OVERRIDE_2026-06-07.md` — keyframe nudge。本 milestone と並走しても衝突しない
- `MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md` — shortcut 登録先。`Ctrl+C / Ctrl+V` を **Panel.Timeline.Right** context に登録

---

## 3. 設計の柱

### 3.1 KeyframeClipData 構造体

`ArtifactCore/include/Animation/KeyframeClipData.ixx` を新規追加:

```cpp
namespace ArtifactCore {

// 1 個の keyframe snapshot
struct KeyframeSnapshot {
    FramePosition frame;
    QVariant value;
    InterpolationType interpolation;   // Linear / Bezier / Hold
    float easeIn;
    float easeOut;
    BezierHandles bezierIn;           // bezier tangent (in)
    BezierHandles bezierOut;          // bezier tangent (out)
    QString label;                    // 任意のキーラベル
};

// 1 property の keyframe 群
struct KeyframeTrackClip {
    QString propertyPath;             // "Transform / Position X"
    QString propertyType;             // "float" / "vec2" / "color" 等
    QList<KeyframeSnapshot> keyframes;
};

// 1 layer の複数 property の keyframe 群
struct KeyframeClipData {
    QString sourceLayerId;
    QString sourceCompositionId;
    QList<KeyframeTrackClip> tracks;  // 空なら "all properties"
    FrameRange sourceRange;           // 元の range（paste mode 判定用）

    // round-trip
    QJsonObject toJson() const;
    static KeyframeClipData fromJson(const QJsonObject& obj);

    // payload
    QByteArray toMime() const;        // application/x-artifact-keyframes
    static KeyframeClipData fromMime(const QByteArray& mime);
};

} // namespace ArtifactCore
```

- `QVariant` 経由で `float / vec2 / color / text` 等の typed 値を保持
- MIME は既存 `MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` の `application/x-artifact-keyframes` を踏襲
- `BezierHandles` は既存 `ArtifactCore/include/Math/Bezier.ixx` があれば流用、無ければ新規に最小 struct を追加

### 3.2 ArtifactClipboardManager

`Artifact/src/Service/ArtifactClipboardManager.cppm` を新規追加:

```cpp
class ArtifactClipboardManager : public QObject {
public:
    static ArtifactClipboardManager& instance();

    // Keyframe
    bool copyKeyframes(const QString& layerId, const QStringList& propertyPaths);
    bool copyKeyframes(const QString& layerId, const FrameRange& range, const QStringList& propertyPaths);
    bool copySelectedKeyframes();                 // 現 selection を取得
    bool pasteKeyframes(const QString& targetLayerId,
                        const QStringList& targetPropertyPaths,
                        PasteMode mode,
                        FramePosition pasteFrame);
    bool hasKeyframes() const;
    KeyframeClipData currentKeyframeClip() const;

    // selection helper
    void setSelection(const QList<QPair<QString,QString>>& layerAndPath); // (layerId, propPath)

    enum class PasteMode { AtOriginalFrame, AtPlayhead, Relative };

signals:
    void clipboardChanged();
};
```

- シングルトン。system clipboard にも `setMimeData` で同期
- selection は `ArtifactTimelineKeyframeModel::selectedKeyframes()` 経由

### 3.3 AnimatableValueT への一括取得 API

`ArtifactCore/include/Animation/Value.ixx` に:

```cpp
template<typename T>
class AnimatableValueT {
public:
    // 新規
    QList<KeyframeSnapshot> allKeyframes() const;
    void setAllKeyframes(const QList<KeyframeSnapshot>& kfs);
    void setKeyframeAt(FramePosition frame, const KeyframeSnapshot& kf);
    void removeKeyframesInRange(const FrameRange& range);
    bool hasKeyframeAt(FramePosition frame) const;
};
```

- `QVariant` 経由で `KeyframeSnapshot::value` とやりとり
- これにより `ArtifactClipboardManager` は `AnimatableValueT<T>` を直接扱える

### 3.4 Timeline 入力

- ショートカット: **`Ctrl+C` / `Cmd+C`** で選択 keyframe を copy。`Ctrl+V` / `Cmd+V` で paste
- **`SHORTCUT_CONTEXT_MAP_2026-04-21.md`** の `Panel.Timeline.Right` に登録
- 既存 `Ctrl+C`（layer copy）と衝突しないよう、**keyframe 選択中のみ** keymap を切替
- 右クリック menu（`ArtifactTimelineTrackPainterView` の context menu）に:
  - `Copy Keyframes` (`Ctrl+C`)
  - `Paste Keyframes Here` (`Ctrl+Shift+V`)  — playhead 位置
  - `Paste at Original Frame` (`Ctrl+Alt+V`)
  - `Paste Relative` (`Ctrl+Alt+Shift+V`)

### 3.5 Paste 挙動の 3 モード

| Mode | 挙動 | デフォルト |
|---|---|---|
| `AtOriginalFrame` | コピー元と同じ `frame` に paste | × |
| `AtPlayhead` | 全 keyframe を `playhead` 位置にオフセット | **◯** |
| `Relative` | 元の `Δframe` 間隔を保ったまま paste | × |

### 3.6 衝突時挙動

| Mode | 既存 keyframe がある位置 |
|---|---|
| `Replace` (default) | 既存を削除して paste |
| `Merge` | 既存を維持して paste（同じ `frame` は上書き） |
| `Skip` | 衝突位置に paste しない |

`PasteKeyframeCommand` に `CollisionPolicy` を持たせる。Default は `Replace`。

### 3.7 Undo

`Artifact/Undo/PasteKeyframeCommand.cppm` 新規:

```cpp
class PasteKeyframeCommand : public QUndoCommand {
public:
    PasteKeyframeCommand(const QString& targetLayerId,
                         const QStringList& targetPropertyPaths,
                         const KeyframeClipData& clip,
                         PasteMode mode,
                         FramePosition pasteFrame,
                         CollisionPolicy policy);
    void undo() override;   // 既存状態を snapshot から復元
    void redo() override;
};
```

- paste 前に **既存 keyframe の snapshot** を保持
- 1 undo で paste 全体を戻せる
- 既存 `MoveKeyframeCommand` 等の keyframe undo と同じ family に置く

### 3.8 異名 property mapping

- paste 先 layer に同名 property があれば自動マッチ
- 同名 property がない場合、**Paste Property Mapping ダイアログ** を出す
  - 左: source property (`Transform / Position X`)
  - 右: 候補 dropdown（layer の全 property から選択）
  - `Skip` / `Auto` ボタン
- 1 回 mapping すれば、次回以降は履歴を保持（session 内のみ）

### 3.9 永続化

- clipboard 内容は **保存しない**（AE と同じ）。session 内の揮発データ
- ただし、**最後に paste した内容**は `FastSettingsStore` の `clipboard/last-paste/v1` に保存し、再起動後に「最後に paste した操作をやり直す」導線を提供
  - 任意機能。Phase 1 では省略可

### 3.10 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `clipboard.keyframe.empty` (severity=info, paste 時に clipboard が空)
- `clipboard.property-mismatch` (severity=warning, paste 先の property 型が不一致)
- `clipboard.collision-replaced` (severity=info, N 個の keyframe が Replace された)

### 3.11 不変条件 (Guardrails)

- `QImage` / `setStyleSheet` 流入禁止
- 既存 `AnimatableValueT<T>::at(frame)` の評価経路は **触らない**
- 既存 `MoveKeyframeCommand` 等の command family は **温存**。本 milestone は `PasteKeyframeCommand` を追加するだけ
- 新規 signal-slot 接続は設計レビュー必須。`clipboardChanged` シグナル 1 個のみ
- clipboard 内容は `QJsonDocument` 経由で **必ず serialize 可能** に保つ
- Mime 形式の version を持たせ、将来のスキーマ変更に備える

---

## 4. フェーズ計画

### Phase 1: Core data + clipboard service (P0, 1〜2 セッション)

- `ArtifactCore/include/Animation/KeyframeClipData.ixx` 新規
- `ArtifactCore/include/Animation/Value.ixx` に `allKeyframes / setAllKeyframes / setKeyframeAt / removeKeyframesInRange / hasKeyframeAt` 追加
- `Artifact/src/Service/ArtifactClipboardManager.cppm` 新規。`copyKeyframes / pasteKeyframes / hasKeyframes / currentKeyframeClip` 実装
- `Q_DECLARE_METATYPE(KeyframeClipData)` 登録

**Done criteria:**
- 1 layer の 1 property の keyframe を copy → 別 layer の同名 property に paste できる
- `clipboardChanged` シグナルが発火
- `KeyframeClipData::toJson / fromJson` が round-trip

### Phase 2: Timeline 入力 + 右クリック (P0, 1〜2 セッション)

- `ArtifactTimelineWidget.cpp` に `Ctrl+C / Ctrl+V` ハンドラ追加
- `SHORTCUT_CONTEXT_MAP_2026-04-21.md` の `Panel.Timeline.Right` に登録
- `ArtifactTimelineTrackPainterView` の context menu に 4 項目追加
- 選択 keyframe の境界（`frame` range）を視覚化

**Done criteria:**
- Timeline 上で `Ctrl+C` → 別 layer の Timeline 上で `Ctrl+V` が動作
- 右クリック menu から 3 つの paste モードが選べる
- 既存 `Ctrl+C`（layer copy）と衝突しない

### Phase 3: Paste 3 モード + Collision policy (P0, 1 セッション)

- `PasteMode { AtOriginalFrame, AtPlayhead, Relative }` 実装
- `CollisionPolicy { Replace, Merge, Skip }` 実装
- `PasteKeyframeCommand` 追加
- 1 undo で paste 全体を戻せる

**Done criteria:**
- 3 paste mode × 3 collision policy = 9 通りすべてが smoke 通過
- 1 undo で paste 範囲が完全復元
- 既存 keyframe が衝突した場合の挙動が選択可能

### Phase 4: 異名 property mapping (P1, 1 セッション)

- `PastePropertyMappingDialog` 新規
- source / target property の dropdown
- 1 session 内の mapping 履歴を保持
- 自動マッチ優先、なしなら dialog

**Done criteria:**
- 異名 property への paste が dialog 経由で成立
- 同名なら dialog を出さず auto
- 履歴保持により 2 回目以降は dialog 省略

### Phase 5: Diagnostics + 永続化 (P1, 1 セッション)

- `clipboardChanged` を `M-CE-CRIT-1` の smoke に組込まない。Problem View への contribution
- 任意で `FastSettingsStore` に「最後に paste した内容」を保存
- 再起動後の `Redo Last Paste` を Edit menu に追加

**Done criteria:**
- Problem View に `clipboard.*` 健全性が表示
- 再起動後に Edit > Redo Last Paste が動作（任意）

### Phase 6: Multi-property + 複数 layer 対応 (P2, 別 milestone 推奨)

- 複数 property 横断の paste
- 複数 layer 選択 → 1 度に paste
- これは `MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` 全体進捗の更新に統合

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_ADVANCED_COPY_PASTE_PHASE2_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` | 親構想。本 milestone は Keyframe 部分だけ抜き出し。 |
| `MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md` | Timeline 上 keyframe 編集。本 milestone は copy/paste で補完。 |
| `MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md` | right-pane 編集体験。本 milestone と並走。 |
| `MILESTONE_KEYFRAME_NUDGE_AND_TEMP_SNAP_OVERRIDE_2026-06-07.md` | nudge / snap。本 milestone と並走で衝突しない。 |
| `MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md` | shortcut 登録先。 |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | diagnostics 文法。 |
| `MILESTONE_BLEND_MODE_DESIGN_2026-06-16.md` | 別 topic。 |
| `MILESTONE_MARKER_FOUNDATION_2026-06-16.md` | 別 topic。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **Animation value の型拡張**。`AnimatableValueT<T>` の `T` ごとに `KeyframeSnapshot::value` を `QVariant` 経由でやりとりする必要。`Q_DECLARE_METATYPE` 漏れは致命的
2. **bezier tangent**。`BezierHandles` 型が無ければ新規追加。`ArtifactCore/include/Math/Bezier.ixx` の有無を Phase 1 開始時に再確認
3. **既存 `Ctrl+C` との衝突**。layer copy と keyframe copy の優先順位。Timeline 上で keyframe が選択されているときだけ keymap を切替
4. **Paste Property Mapping ダイアログ**。モーダル / モデルレスの選択。Phase 4 で決定
5. **永続化**。再起動後の `Redo Last Paste` は project に「未保存の変更」を生む。Phase 5 で警告経路を準備

### 6.2 契約上の未解決

- **複数 frame 範囲**。`Ctrl+C` したときの source range をどう保存するか。`KeyframeClipData::sourceRange` に保存済み
- **Undo 粒度**。paste 全体を 1 undo か、property 単位か、keyframe 単位か。Phase 3 で決定。**Default は paste 全体 1 undo**
- **Multi-property paste**。1 layer の複数 property をまとめて paste するか、property 単位のみか。Phase 1〜3 は property 単位のみ
- **MIME version**。`KeyframeClipData::toMime` に `schemaVersion` を埋め込む。Phase 1 で固定
- **System clipboard との同期**。`Ctrl+C` で `application/x-artifact-keyframes` を set。AE 同様、独自形式は option

### 6.3 サブモジュール境界

- `ArtifactCore/src/Animation/Value.ixx` への API 追加は破壊変更ではない。**新規メソッドの追加のみ**
- `ArtifactCore/CMakeLists.txt` に新規ファイルを登録
- `ArtifactWidgets` は触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- Timeline 上で `Ctrl+C / Ctrl+V` で keyframe を別 layer / 別 property に移動できる
- 3 paste mode × 3 collision policy = 9 通りすべてが smoke 通過
- 異名 property への paste が mapping dialog 経由で成立
- 1 undo で paste 全体を完全復元
- 右クリック menu に 4 項目が表示
- 同名 property なら dialog を出さず auto
- Problem View に `clipboard.*` 健全性が表示
- `MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` の Keyframe 部分を本 milestone 完了として記載更新
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot が **clipboardChanged 1 個以外** 増えていない
- `ArtifactWidgets` を触っていない
- `ArtifactCore` への bump 手順が `.github/GIT_WORKFLOW_PARENT_CHILD.md` に整合

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §3 を正式 milestone に起こした。`MILESTONE_ADVANCED_COPY_PASTE_2026-03-28.md` の Keyframe 部分を分離。
