# Timeline Keyframe Area Editing (2026-06-15)

## 目的

同じ値が続くキーフレーム区間を、タイムライン上では「エリア」として掴んで編集できるようにする。

内部表現は既存のキーフレームポイントのまま維持し、保存形式・評価系・補間系を大きく変えない。たとえば `1 -> 10 -> 10 -> 1` のような値変化では、中央の `10 -> 10` を 1 つのフラット区間として選択、移動、伸縮できるようにする。

## 基本方針

- データモデルには新しい area keyframe 型を増やさない
- `ArtifactTimelineKeyframeModel` から得られる既存 keyframe point 群を view model 側で area 化する
- 正規編集面は `ArtifactTimelineTrackPainterView` とする
- undo/redo は既存の snapshot 系 keyframe 編集コマンドに寄せる
- QtCSS、`QColorDialog`、新規 signal/slot、`QImage` は増やさない

## 用語

- **Keyframe Point**
  既存の 1 フレーム上のキーフレーム。
- **Keyframe Area**
  同じ property lane 上で、隣接する 2 点以上の keyframe が同値または同値相当で続く区間を UI 上でまとめたもの。
- **Plateau**
  value graph 上で値が変化しない区間。Phase 1 では Keyframe Area とほぼ同義に扱う。

## 例

```text
frame 0  : value 1
frame 10 : value 10
frame 20 : value 10
frame 30 : value 1
```

表示:

```text
0        10========20        30
◆--------[  area  ]---------◆
```

内部:

```text
keyframe(frame=10, value=10)
keyframe(frame=20, value=10)
```

操作:

- area 中央ドラッグ: `frame 10` と `frame 20` を同じ delta で移動
- area 左端ドラッグ: `frame 10` のみ移動
- area 右端ドラッグ: `frame 20` のみ移動
- area 値変更: 両端 keyframe の value を同じ値へ更新

## 範囲

### In

- 同一 property lane 内の隣接 keyframe から area 候補を検出する
- linear / hold / auto など、安全にフラット扱いできる interpolation だけを対象にする
- area の owner-draw 表示を `ArtifactTimelineTrackPainterView` に追加する
- area の hit test を keyframe point hit test と競合しない形で追加する
- area 移動、左右端伸縮、選択表示を追加する
- 操作結果は既存 keyframe point の frame/value 更新として反映する
- undo/redo は 1 操作としてまとまる

### Out

- 新しい保存形式としての area keyframe 型
- Bezier tangent を含む完全な value graph plateau 判定
- 複数 lane をまたぐ area grouping
- Graph Editor 風の value curve 編集
- expression / time remap / source text など特殊値の area 編集
- auto ripple / downstream shift との統合

## 判定ルール

Phase 1 では保守的に判定する。

1. 同じ layer / property path / channel に属する keyframe である
2. frame 順で隣接している
3. value が同じ、または数値型なら許容誤差内で等しい
4. interpolation がフラット区間として安全に見える
5. 中間に別の keyframe がない

数値以外の property は Phase 1 では対象外にする。将来、色・ベクトル・テキストなどへ広げる場合は type ごとの equality / display label / edit affordance を定義する。

## UI 仕様

### 表示

- 既存 keyframe diamond は残す
- 同値区間は diamond 間に薄いバーとして描く
- 選択中 area は枠線と内側の塗りを強める
- hover 時は area 全体ではなく、中央移動・左端・右端の affordance が分かるようにする
- 色は `MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md` の keyframe 色ルールに寄せる

### Hit Test 優先順位

1. keyframe point
2. area edge handle
3. area body
4. clip / layer bar
5. empty lane

keyframe point の既存操作を奪わないため、point の hit rect を優先する。

### 操作

- area body drag: 両端 keyframe の frame をまとめて移動
- left edge drag: start keyframe の frame を変更
- right edge drag: end keyframe の frame を変更
- modifier なし: frame snap に従う
- `Alt` 等の特殊 modifier は Phase 1 では増やさない
- context menu は Phase 1 では最小限にし、必要なら `Select Area` / `Delete Area Points` 程度に留める

## 実装候補

### 1. View Model

`ArtifactTimelineTrackPainterView` 付近に、描画と hit test 用の軽量構造を追加する。

```cpp
struct TimelineKeyframeArea {
    QString layerId;
    QString propertyPath;
    int channelIndex = -1;
    int startFrame = 0;
    int endFrame = 0;
    int startKeyIndex = -1;
    int endKeyIndex = -1;
    QVariant value;
    QRectF bodyRect;
    QRectF leftHandleRect;
    QRectF rightHandleRect;
};
```

永続化しない一時構造として扱う。keyframe model 側へ入れる場合も、保存対象ではなく query helper に留める。

### 2. Area Collection

- visible row / visible keyframe collection の後段で area を組み立てる
- frame-to-x 変換後に rect を持たせる
- zoom が低い場合は body rect の最小幅を確保する
- 1px 未満の area は hit test だけ広げ、表示は潰れないようにする

### 3. Draw

- keyframe diamond より下、clip bar より上に area bar を描く
- area が選択中の場合は diamond 側の選択表現とも同期する
- owner-draw のみで実装し、QtCSS は使わない

### 4. Edit Command

- drag 開始時に対象 area の start/end keyframe snapshot を取る
- drag 中は preview state を更新する
- drag 終了時に 1 つの undo command として確定する
- cancel 時は snapshot へ戻す

既存の `TimelineKeyframeSnapshotCommand` 系に寄せる。新規 command が必要な場合も、point 操作の snapshot と同じ復元経路を使う。

## First Files

1. `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
2. `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
3. `Artifact/include/Widgets/Timeline/ArtifactTimelineKeyframeModel.ixx`
4. `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeModel.cppm`
5. `docs/planned/MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md`

## Phase Plan

### Phase 1: Detect And Draw

- visible keyframe point 群から area 候補を検出する
- area bar を描画する
- hover 表示だけを追加する
- 既存 keyframe point の選択・移動が変わらないことを確認する

進捗メモ:

- 右ペインの status summary を短くして、hover keyframe の文脈を出すようにした
- area の hover / selected / edge affordance を少し強めて、body と端の差を見分けやすくした

### Phase 2: Select And Move

- area body の選択を追加する
- body drag で両端 keyframe をまとめて移動する
- undo/redo を 1 操作にまとめる
- 0 未満 frame や keyframe 順序反転を防ぐ

### Phase 3: Edge Resize

- left/right edge handle を追加する
- 片端だけの frame 変更を実装する
- 隣接 keyframe との collision rule を固定する
- area 幅が 0 になる操作を拒否または point merge として扱う

### Phase 4: Value Edit Integration

- area 選択時に Inspector / property row 側の値変更を両端 keyframe へ反映する
- 数値型、ベクトル型、色型の equality / formatting rule を分ける
- 複数 area 選択の扱いを決める

## Collision Rule

Phase 1-3 では単純な安全ルールにする。

- start は前の keyframe frame より後ろにしか動かせない
- end は次の keyframe frame より前にしか動かせない
- start >= end になる drag は clamp する
- 同一 frame への keyframe merge は Phase 1 では行わない

## リスク

- Bezier tangent がある同値 keyframe は、見た目上フラットでない可能性がある
- point hit test と area hit test が競合すると既存編集が重く感じる
- area 選択と keyframe point 選択の状態同期を雑にすると undo/redo 後の表示がずれる
- 数値以外の value equality を急に広げると意図しない grouping が起きる

## Done Criteria

- `1 -> 10 -> 10 -> 1` の中央区間が area として表示される
- area body をドラッグすると両端 keyframe が同じ delta で移動する
- area edge をドラッグすると片端 keyframe だけが移動する
- 操作後も内部 keyframe point 群として保持される
- undo/redo で area 操作前後の状態が正しく戻る
- 既存 keyframe point の選択・移動・削除が退行しない

## 推奨順

1. area の検出だけを追加して debug/hover で確認する
2. 描画を薄く追加する
3. hit test 優先順位を固定する
4. body move を実装する
5. edge resize を実装する
6. value edit 連携は最後に回す

---

## Next Execution Slice

Phase 1 は、area 候補の検出と hover 可視化を先に固める。

### Phase 1A の着手点

1. 同一 `layerId + propertyPath + channelIndex` の keyframe を frame 順で並べる
2. 隣接 keyframe 間を segment として扱い、同値区間だけ候補化する
3. frame-to-x 変換後に bodyRect / leftHandleRect / rightHandleRect を持たせる
4. keyframe が 1 個だけの property は area を出さない

### Phase 1 完了条件

- `1 -> 10 -> 10 -> 1` の中央区間が area として検出できる
- どの条件で area が出るかが説明できる
- debug / hover だけで候補の境界が追える

### Phase 2A の着手点

1. area bar を keyframe diamond より下、clip bar より上に描く
2. hover 時に body と端の affordance を分かるようにする
3. selection / hover / current-frame 表示と競合しない alpha にする
4. zoom が低いときの最小幅だけ決める

### Phase 2 完了条件

- timeline 上で area が薄い背景帯として読める
- point の視認性が落ちない
- selection と hover の役割がぶつからない

### Phase 3 への前提

- body move / edge resize は hit test の優先順位が固まってから入れる
- undo/redo は snapshot 経路が area 候補と整合してから詰める
