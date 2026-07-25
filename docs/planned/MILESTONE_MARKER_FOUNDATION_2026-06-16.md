# M-MARKER-1 Marker Foundation Milestone

作成日: 2026-06-16
対象: `Artifact/include/Composition/ArtifactInOutPoints.ixx`,
      `ArtifactCore/include/Frame/*`,
      `Artifact/src/Composition/ArtifactInOutPoints.cppm`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`,
      `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`,
      `Artifact/src/Widgets/ArtifactCompositionEditor.cppm`,
      `Artifact/src/Project/ArtifactProjectManager.cppm`,
      `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
位置づけ: 既存 `ArtifactInOutPoints` 内の `ArtifactMarker` を、Timeline / Inspector / Playback / 永続化に繋ぐ。AE 互換の "マーカー" の **foundation** に絞り、コメントスレッドや協調編集は別 milestone。
参照:
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` (P0)
- `docs/analysis/CORE_MODULE_MISSING_FEATURES_2026-04-19.md` (Pain Points 🔝🔴)
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` (Marker は NOT FOUND だが、ArtifactInOutPoints 内に下地あり)
- `docs/analysis/MOTION_GRAPHICS_ARTIST_PAIN_POINTS_2026-04-19.md` (Marker 欠如)
- `docs/analysis/MOTION_GRAPHICS_AD_PRODUCTION_THINKING_MEMO_2026-05-28.md` (広告運用での review 導線)
- `docs/planned/MILESTONE_COMPOSITION_NOTES_SCRATCHPAD_2026-03-30.md` (notes との切り分け)
- `docs/planned/MILESTONE_TIMELINE_INDEX_2026-04-22.md`

---

## 1. 目的

`ArtifactMarker` クラスは `ArtifactInOutPoints.ixx:42-110` に **API としては揃っている** にもかかわらず、Timeline 上に表示されず、Inspector から追加できず、プロジェクト保存にも乗らない。

AE 互換の Marker は次の 3 つを担うが、現状どれも動いていない。

- **目印**: chapter / web link / color 校正点など、素材の「ここ重要」ポイント
- **操作ジャンプ**: `Ctrl+Shift+↑↓` / `J K L` 拡張の次/前マーカー
- **範囲ヒント**: work area / in-out point と並走する軽量な印

アーティストの痛点メモ (2026-04-19) でも「**マーカーが一切無い。ダミーの空レイヤーを置いて目印にする**」が 🔝🔴 で挙げられている。

この milestone は「Marker モデルはある」を前提に、UI / Timeline / 永続化 / Undo の **foundation** を 1 つに束ねる。コメントスレッド・通知・協調編集は扱わない（別 milestone）。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../ArtifactTimelineWidget.cpp` 側に閉じる。サブモジュール（`ArtifactWidgets`）は触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存 API

`Artifact/include/Composition/ArtifactInOutPoints.ixx` に以下が揃っている:

- `enum class MarkerType { Comment, Chapter, Flash, WebLink, Color }`
- `class ArtifactMarker : public QObject`
  - `position / comment / type / color / webLink / tags`
  - `positionChanged / commentChanged / typeChanged / colorChanged / webLinkChanged / tagsChanged / markerChanged` シグナル
  - `addTag / removeTag / hasTag`
- `class ArtifactInOutPoints : public QObject`
  - `addMarker / removeMarker(position) / removeMarker(ptr) / getMarkerAt / allMarkers`
  - `markersByType / markersByTag / searchMarkers / markersInRange / chapterMarkers`
  - `nextMarker / previousMarker / nextChapter / previousChapter`
  - `importFromXML / exportToXML`
  - シグナル: `markerAdded / markerRemoved / markerChanged / allMarkersCleared / navigatedToMarker`

### 2.2 不足している導線

| 軸 | 状況 | 影響 |
|---|---|---|
| Timeline 描画 | `ArtifactTimelineTrackPainterView` に marker 描画なし | Marker の存在に気付けない |
| Timeline 入力 | 右クリックメニュー / `*` キービートで add する導線なし | 制作中に追加できない |
| Inspector 露出 | `ArtifactCompositionEditor` の右側 panel に marker list なし | 編集 / 確認 / ジャンプ不可 |
| Playback | `nextMarker / previousMarker` API はあるが、ショートカットが繋がっていない | ジャンプできない |
| Composition ↔ Layer 区別 | クラスは Composition に紐づく型のみ。Layer 単位 marker なし | 層別 Marker 不可（後段で扱う） |
| 永続化 | `exportToXML / importFromXML` スタブの可能性高、project JSON への接続なし | 保存 / 復元できない |
| Undo | Marker 操作の `W_OBJECT` シグナルはあるが `QUndoCommand` 未実装 | 戻し / やり直し不可 |
| Diagnostics | `M-APP-5 Project Health` の `goal/expected/actual` 枠に marker 健全性が無い | 壊れた marker を見過ごしやすい |

### 2.3 既存 milestone との関係

- `MILESTONE_COMPOSITION_NOTES_SCRATCHPAD_2026-03-30.md` Phase 3「Frame Note / Marker Note」と重なる部分がある。本 milestone は **Marker 単体に絞り、notes/scratchpad は notes 側で扱う**。重複タスクを作らないために、本 milestone は「notes Phase 3 への橋渡し」を担う。
- `MILESTONE_TIMELINE_INDEX_2026-04-22.md` 配下の timeline 系 milestone 群と責務分離する。Marker は Timeline の **マーカー帯** に乗る描画要素で、Timeline 本体の挙動（keyframe / clip / work area）は触らない。
- `M-IR-8 ImmediateContext Boundary / De-direct` を侵さない。Marker 描画は既存の `paintEvent` 経路に閉じて追加する。

---

## 3. 設計の柱

### 3.1 Marker Contract

内部 canonical は次で固定する。

- **座標**: `FramePosition` ベース。comp frame と timecode の二重表示は `ArtifactInOutPoints` 既存 `position()` を使う
- **色**: `QColor` だが **新規 `QColor` を Timeline 内に持ち込まない**。Marker 種別から内部トークンを引いて、theme token 経由で描画色は解決する
- **種類**: 既存 `MarkerType { Comment, Chapter, Flash, WebLink, Color }` を 5 種類として維持。`Color` 種別の "color correction point" は後段で別 milestone にする
- **永続化**: project JSON へ `composition.markers[]` として保存。`exportToXML` はそのまま予備経路として残す
- **Undo**: `AddMarkerCommand / RemoveMarkerCommand / EditMarkerCommand` を `QUndoCommand` 派生で提供

### 3.2 Timeline への露出

`ArtifactTimelineTrackPainterView` に **Marker 帯**を追加する。右ペインのタイムルーラ直下、`work area` 帯の上、またはタイムルーラと同じ行に細い帯として置く。AE 風の縦マーカー表示は **タイムルーラ上**に縦線で示す。

- 描画責務は `ArtifactTimelineTrackPainterView::paintMarkerBand()` に閉じる
- ヒットテストは `MarkerBand` の高さを 1 段で固定し、`markerHitTest(frame) -> markerId` を提供
- 既存の `M-TL-4 Owner-Draw` 正規経路に載せる。`TimelineTrackView` には触らない
- レイヤー行への波及はしない。Marker は comp 単位

### 3.3 Inspector 露出

`ArtifactCompositionEditor` 右側の inspector（`ArtifactInspectorWidget` 配下）に **`MarkerListPanel`** を追加する。

- 一覧は `chapterMarkers` とその他を分けた 2 セクション
- 各行: position / type icon / comment preview / jump / edit / remove
- `+ Add Marker at Playhead` ボタン
- `*` キービート（テンキー `*`）で playhead 位置に新規 Marker 追加
- `Ctrl+Shift+↑↓` で next/previous marker jump（既存 `nextMarker / previousMarker` API 経由）
- `M` / `Shift+M` を AE 互換の **next/prev marker** に割り当てる（`SHORTCUT_CONTEXT_MAP` へ登録）

### 3.4 Composition 単位の Marker

`ArtifactInOutPoints` は comp に紐づく型のみ。本 milestone では **Layer 単位 Marker を追加しない**。Layer 単位は別 milestone（後述の未解決論点）。

### 3.5 永続化

`ArtifactProjectManager` 経由で project JSON に `composition.markers[]` を保存する。

```json
{
  "compositions": [
    {
      "id": "comp_xxx",
      "markers": [
        {
          "id": "marker_001",
          "type": "Chapter",
          "frame": 120,
          "comment": "Logo reveal",
          "color": "#FFAA00",
          "webLink": "",
          "tags": ["intro", "logo"]
        }
      ]
    }
  ]
}
```

- 保存 / 復元は `importFromXML / exportToXML` ではなく、**project JSON schema 経由** に寄せる。XML 経由は予備経路として残す
- 復元時に marker ID が衝突したら新規 ID に付け替え、`markerChanged` を発火

### 3.6 Undo

新規 `QUndoCommand` 派生:

- `AddMarkerCommand(InOutPoints*, MarkerType, FramePosition, comment, color)`
- `RemoveMarkerCommand(InOutPoints*, MarkerID)`  ← 復元用に payload snapshot
- `EditMarkerCommand(InOutPoints*, MarkerID, Field, before, after)` ← Field は enum { Position, Comment, Type, Color, WebLink, Tags }
- `ClearAllMarkersCommand(InOutPoints*)` ← snapshot 一括

これらは `ArtifactCore` ではなく `Artifact/Undo/` 配下に置く（既存 `EditCompositionSettingCommand` 等と並走）。

### 3.7 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 文法に marker 健全性を載せる。

- `marker count > 0` で 1 行サマリ
- `marker.position < 0 or > composition.duration` を `severity=error` で報告
- `duplicate marker at frame` を `severity=warning`
- `marker without comment` を `severity=info` (任意)

これは `M-CE-CRIT-1 Critical Render` の smoke に直接は入れず、Problem View 側の contribution として別 PR。

### 3.8 不変条件 (Guardrails)

- `ArtifactInOutPoints::addMarker()` 以外から marker を生成しない
- `ArtifactTimelineTrackPainterView` 以外で marker 描画ロジックを書かない（重複防止）
- Marker 描画色は `theme token` を経由する。新規 `QColor` 直書き禁止
- 既存 `W_OBJECT` / `W_SIGNAL` の整合を保つ。新規 `W_OBJECT` 派生は追加しない（`ArtifactMarker` / `ArtifactInOutPoints` 既存を使う）
- `setStyleSheet` の追加禁止
- project JSON の後方互換性を壊さない（marker 欠落時は空 list として扱う）

---

## 4. フェーズ計画

### Phase 1: Marker model surface in Timeline (P0, 1〜2 セッション)

- `ArtifactTimelineTrackPainterView` に `MarkerBand` 描画と `markerHitTest(frame)` を追加
- タイムルーラ上に縦線で表示
- hover で comment ツールチップ
- click で `nextMarker / previousMarker` の挙動は未配線（Phase 2 で実施）

**Done criteria:**
- comp に marker が 1 個あると Timeline 上に縦線が出る
- hover で comment が見える
- 既存の Timeline 描画と keyframe / clip / work area が干渉しない

### Phase 2: Inspector 露出 + 入力 (P0, 2〜3 セッション)

- `MarkerListPanel` を `ArtifactInspectorWidget` 配下に追加
- `+ Add Marker at Playhead` ボタン
- `*` キービートで新規 marker
- 各行の jump / edit / remove
- `M` / `Shift+M` ショートカットを `SHORTCUT_CONTEXT_MAP` 経由で接続

**Done criteria:**
- Inspector から marker 追加 / 編集 / 削除 / ジャンプが動作
- `*` キービートで playhead 位置に新規 marker
- `M` / `Shift+M` で next / prev marker jump

### Phase 3: Undo + 永続化 (P0, 1〜2 セッション)

- `AddMarkerCommand / RemoveMarkerCommand / EditMarkerCommand / ClearAllMarkersCommand` 追加
- `ArtifactProjectManager` の project JSON に `composition.markers[]` セクション追加
- 復元時に `importFromXML` ではなく JSON schema 経由
- 旧プロジェクトは `composition.markers` 欠落を許容

**Done criteria:**
- marker 追加 / 編集 / 削除が 1 回の Undo で戻る
- project 保存 → 再読込で marker が完全復元
- 旧プロジェクトが開ける（後方互換）

### Phase 4: Diagnostics + harness (P1, 1 セッション)

- `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に marker 健全性を載せる
- `FrameDebugSnapshot.resources` に `label=Marker Integrity` を追加
- `DebugRenderHarness` から 1 クリックで marker 健全性を確認

**Done criteria:**
- Problem View に marker 健全性が並ぶ
- 重複 / 範囲外 / 空 comment の marker が warning として出る

### Phase 5: Layer marker 拡張 (P2, 別 milestone 推奨)

- 既存 `ArtifactInOutPoints` を comp 単位として維持しつつ、Layer 単位 marker のデータモデルと UI を別 milestone で扱う
- ここは本 milestone のスコープ外。本 milestone の Phase 5 は「Layer marker 拡張の **設計ノート** だけ」

**Done criteria (本 milestone 内):**
- `docs/planned/MILESTONE_LAYER_MARKER_2026-XX-XX.md` (将来用) のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_COMPOSITION_NOTES_SCRATCHPAD_2026-03-30.md` | notes / scratchpad 側。Phase 3 (Frame Note) と本 milestone の marker は重なるが、notes は自由記述、marker は構造化属性。本 milestone が marker を提供し、notes Phase 3 は notes UI を担当する分業。 |
| `MILESTONE_TIMELINE_INDEX_2026-04-22.md` | timeline 全体の索引。本 milestone は timeline に marker 帯を 1 行追加するだけ。timeline 本体挙動は触らない。 |
| `MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md` | right-pane の keyframe 編集体験。marker はルーラ上の縦線と inspector list で別 surface なので干渉しない。 |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | diagnostics 文法。本 milestone Phase 4 が contribution する。 |
| `MILESTONE_COMPOSITION_FINAL_EFFECT_2026-04-14.md` | 別 topic。 |
| `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` | `CompositionRenderController` に low-level call site を増やさない方針。marker 描画は Timeline 側で完結し、controller を触らない。 |
| `MILESTONE_BLEND_MODE_DESIGN_2026-06-16.md` | 別 topic。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **`ArtifactInOutPoints` の W_OBJECT 整合**。既存 `ArtifactMarker` / `ArtifactInOutPoints` のシグナルに `W_OBJECT_IMPL` の整合が取れているか Phase 1 開始時に再確認する
2. **Timeline 描画コスト**。marker 数 100+ での paint 重複を避ける。`MarkerBand` の描画は `paintEvent` 内で 1 度だけ走らせる
3. **プロジェクト JSON 互換**。後方互換を保ったまま `composition.markers[]` を追加する。旧プロジェクトの読み込み時に missing field は許容
4. **検索 / tag フィルタ**。`markersByTag / searchMarkers` API はあるが UI 露出は別。`Problem View` 側のフィルタに混ぜるかは Phase 4 で決定

### 6.2 契約上の未解決

- **Layer 単位 marker** のデータモデル。`ArtifactInOutPoints` を comp 単位として残しつつ、Layer 側に `markers_` を持たせる案と、comp 単位の marker に `layerId` を持たせる案の 2 案がある。Phase 5 で決定
- **Marker 数の上限**。comp あたり 1000 marker 程度は軽量に動く必要がある。Phase 1 の smoke で実測
- **`*` キービートとテンキー `*`**。テンキー `*` は別の用途で使われている可能性あり、Phase 2 開始時に再確認
- **Marker 共有**。複数 comp に同じ marker を複製する導線。Phase 5 以降
- **AE Marker 互換 XML**。`importFromXML / exportToXML` のスタブ実装の調査が必要。JSON 経由に寄せても XML 入出力を残すかどうかは Phase 3 で判断

### 6.3 サブモジュール境界

- `ArtifactCore/include/Frame/*` の `FramePosition / FrameRange` は **既存型をそのまま使用**（`ArtifactInOutPoints.ixx:7-9` で import 済み）
- `ArtifactCore/CMakeLists.txt` は触らない
- `ArtifactWidgets` は触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- `ArtifactInOutPoints::addMarker` で追加した marker が Timeline 上の縦線として表示される
- Inspector の `MarkerListPanel` から追加 / 編集 / 削除 / ジャンプが動作
- `*` キービート、`M` / `Shift+M` のショートカットが登録され、context map に反映される
- `QUndoCommand` 派生 4 種が `Artifact/Undo/` に追加され、Undo 1 回で元に戻る
- project JSON に `composition.markers[]` が保存 / 復元され、後方互換が保たれる
- Problem View に marker 健全性が表示され、重複 / 範囲外 / 空 comment が warning として出る
- 新規 `QImage` / `setStyleSheet` / 新規 `W_OBJECT` 派生 / 新規 global signal が増えていない
- `ArtifactCore` への bump 手順が `.github/GIT_WORKFLOW_PARENT_CHILD.md` に整合している

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`MILESTONE_COMPOSITION_NOTES_SCRATCHPAD_2026-03-30.md` Phase 3 との分業を明示。

---

## Static audit follow-up (2026-07-25)

`ArtifactInOutPoints`、Timeline、Project保存、Undo、AI automation の現行ソースを照合した。ビルド・実機操作確認は未実施。

| Done criteria | 現状 | 判定 |
|---|---|---|
| Marker model／追加・検索・前後移動 | `ArtifactMarker`、`addMarker`、type/tag/range検索、next/previous、XML入出力を確認した。 | 実装済み基盤 |
| Timeline表示・入力 | Timeline の `marker` は主に keyframe visual を指す。Composition marker の専用縦線帯・hit-test・追加導線は確認できない。 | 未完了 |
| Inspectorで追加・編集・削除・ジャンプ | 専用 `MarkerListPanel` は確認できない。AI automation の `add_marker` は存在するが、通常UIの完了条件を満たさない。 | 未完了 |
| project JSON保存／復元 | `ArtifactInOutPoints` の XML 経路はあるが、`composition.markers[]` の project JSON接続は確認できない。 | 未完了 |
| Undo／ショートカット／Problem View | Marker専用 command、`*`／`M`／`Shift+M`、marker健全性warningの接続は確認できない。 | 未完了 |

### 現在の判定

MarkerのCore APIは実装済みだが、Timeline／Inspector／JSON／Undo／diagnostics の foundation 接続が未完了。現状は「Core基盤実装済み／UI統合未完了」とする。
