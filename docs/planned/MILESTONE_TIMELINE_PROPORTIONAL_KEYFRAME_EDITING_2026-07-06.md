# M-TL-17 Timeline Proportional Keyframe Editing (2026-07-06)

## Goal

`ArtifactTimelineTrackPainterView` の右ペインで、選択キーフレームをドラッグしたときに周辺キーへ減衰つきで影響を配る `Proportional Editing` を導入する。

Blender Graph Editor のように、

- 中心キーはフルに動く
- 離れたキーほど小さく追従する
- 既存の keyframe / area / graph 編集を壊さず段階導入する

ことを狙う。

## Why Here

- 正規編集面は `ArtifactTimelineTrackPainterView`
- 既に keyframe drag / area drag / bezier handle drag / marquee selection / undo の土台がある
- `CurveEditor` 側へ直接入れるより、まず右ペインの軽量編集として入れた方が既存 workflow に乗りやすい

## Scope

### In

- 右ペインでの keyframe 時間移動に対する proportional editing
- `O` で on/off
- `[` `]` で半径変更
- hover / drag tooltip への状態表示
- 既存 snapshot / move request / selection update との整合

### Out

- value graph 上の値方向 proportional editing
- speed graph 専用 proportional editing
- bezier handle 自体への減衰編集
- pivot mode 切替
- falloff 種別切替
- region scale / lattice 的な複合変形

## Product Rules

- Phase 1 は **selected keyframes only**
- Phase 1 は **time move only**
- Phase 1 の falloff は smooth 1 種のみ
- 既存の単体ドラッグと modifier lock を壊さない
- collision / merge の高度処理は後段に回す

## Phase Plan

### Phase 1: Marker Drag Foundation

対象:

- 通常の keyframe marker drag
- 複数選択時の時間移動

仕様:

- pivot は drag 開始した marker の元 frame
- 各 key の delta は `distance from pivot` と `radius` で減衰
- 単体選択時は従来と同じ挙動
- tooltip に proportional 状態と半径を表示

完了条件:

- `O` で proportional editing を切り替えられる
- `[` `]` で半径変更できる
- 複数選択 drag で近いキーほど大きく追従する
- release 後も undo / redo と selection 更新が破綻しない

### Phase 2: Keyframe Area Drag

対象:

- keyframe area body drag
- keyframe area edge resize

仕様:

- area endpoints だけでなく、同時選択中キーへも減衰配布できるようにする
- edge resize は反対側 edge を pivot にした時間スケールとして扱う

### Phase 3: Value Direction / Inline F-Curve

対象:

- 値方向ドラッグ
- inline F-curve 表示と組み合わせた proportional editing

依存:

- `docs/done/MILESTONE_TIMELINE_INLINE_FCURVE_EDITING_2026-07-06.md`

---

## 2026-07-25 現状確認

Phase 1〜2 相当は実装済み。`ArtifactTimelineTrackPainterView.cppm` に proportional editing の on/off と radius があり、`O` で切り替え、`[` / `]` で半径を調整できる。通常 marker drag、keyframe area body drag、area edge resize のいずれも、pivot からの距離に応じた時間移動／時間スケールを preview と release の双方で適用し、影響範囲の表示も行う。既存の snapshot ベース Undo と selection 更新も同じ編集経路に含まれている。

未完了・未確認:

- value direction 編集と inline F-curve への比例編集
- Curve / Graph Editor との共有 UI・共通 API
- falloff 種別、pivot mode、region scale、Bezier tangent 連動
- 実運用での衝突・merge、Undo/redo、drag preview の回帰確認

したがって「右ペインの time move / scale の first slice は実装済み、Phase 3〜4 は未着手」と整理する。

### Phase 4: Advanced Controls

- falloff type 切替
- pivot mode 切替
- region scale
- bezier handle / tangent 連動

## Implementation Notes

- まず `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm` 内で閉じる
- `.ixx` や保存形式の変更は Phase 1 では不要
- proportional radius は frame 単位で持つ
- drag preview と release commit の両方で同じ weight 計算を使う

## Current Progress

2026-07-06 の first slice:

- `TrackPainterView` に proportional on/off 状態を追加
- `O` toggle を追加
- `[` `]` radius 調整を追加
- marker drag preview / release の両方に weighted time move を追加
- tooltip / shortcut 文言へ proportional editing を反映
- area body drag でも、同一 property の複数選択 keyframes に対して proportional falloff を適用するよう拡張
- area edge resize でも、反対側 edge を pivot にした proportional time scale を適用するよう拡張
- drag 中の proportional influence band を可視化

未完了:

- value direction 編集
- graph editor 側との共有 UI

Current implementation note:

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm` の範囲で閉じる方針は維持する
- proportional editing の first slice は右ペインの marker drag / area drag まで入っており、選択キーに対する time move / scale の重み付けが動く前提になっている
- ここから先は `inline F-curve` 側へ広げる前段として、右ペインの time-move 編集を崩さずに保守する
- proportional editing の次の確認点は、selection / undo / drag preview の一致性と、graph editor 側へ共有する最小 API だけに絞る

## Target Files

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`
- 必要なら `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`
- 必要なら `docs/done/MILESTONE_TIMELINE_INLINE_FCURVE_EDITING_2026-07-06.md`

## Related Docs

- `docs/planned/MILESTONE_TIMELINE_KEYFRAME_AREA_EDITING_2026-06-15.md`
- `docs/done/MILESTONE_TIMELINE_INLINE_FCURVE_EDITING_2026-07-06.md`
- `docs/planned/MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md`
- `docs/planned/MILESTONE_GRAPH_EDITOR_DESIGN_AUDIT_2026-07-04.md`
