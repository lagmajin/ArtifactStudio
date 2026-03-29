# Virtual Pointer Core Milestone

> 2026-03-28 作成

## 目的

モーショングラフィックや広告動画で使う「仮想マウス」を、`ArtifactCore` に再生可能なデータとして定義する。

ここでの対象は実際の入力デバイスではなく、画面上に見えるポインター演出の正となるデータモデルである。

表示名は `Pointer`、実装名は `VirtualPointer` を基本にする。`Cursor` はテキスト入力カーソルと紛れやすいため避ける。

---

## 背景

モーショングラフィックでは、マウスの移動やクリック自体が演出として扱われることが多い。

必要になるのは次のような表現である。

- ポインター位置の移動
- クリック
- ダブルクリック
- ドラッグ
- ホバー
- 軌跡
- クリック波紋
- 強調ハイライト

これらを編集ごとにベタ打ちするのではなく、Core の再生可能なイベント列として扱いたい。

---

## 方針

### 原則

1. 実デバイスのマウスとは分ける
2. 画面座標系の再生データとして定義する
3. 決定論的に再生できるようにする
4. UI 描画は Core の外に置く
5. 録画 / 編集 / 再生の三つを同じモデルで扱う

### 想定用途

- tutorial / promo 動画のマウス演出
- UI 操作デモの再生
- コースティングや easing を伴う pointer motion
- click / drag / hover の視覚化

---

## 既存資産

- `ArtifactCore/include/Animation/AnimatableValue.ixx`
- `ArtifactCore/include/Animation/AnimatableTransform2D.ixx`
- `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`
- `ArtifactCore/include/Time/TimeRemap.ixx`
- `docs/planned/MILESTONE_ANIMATION_DYNAMICS_CORE_2026-03-28.md`

---

## Phase 1: Data Model Freeze

### 目的

Pointer 演出の最小データモデルを固定する。

### 作業項目

- `VirtualPointerTrack` を定義する
- `PointerFrame` を定義する
- `PointerEventKind` を定義する
- `PointerStyle` を定義する

### 最低限の要素

- `timestamp` / `frame`
- `position`
- `visible`
- `pressedButtons`
- `eventKind`
- `pressure` / `strength` の任意値

### 完了条件

- 1 本の pointer track を保存できる
- frame ベースでも time ベースでも扱える
- 既存 animation データと衝突しない

---

## Phase 2: Recording / Playback Contract

### 目的

録画と再生の契約を固定する。

### 作業項目

- pointer track の record API を定義する
- seek 時の状態復元を定義する
- playback 時の interpolation 方針を決める
- click や drag の区間表現を定義する

### 完了条件

- 再生しても毎回同じ軌跡になる
- 途中 seek しても状態が壊れない
- 連続イベントと離散イベントを両方表せる

---

## Phase 3: Editor Bridge

### 目的

Core の pointer track を編集や自動生成の基礎にする。

### 作業項目

- keyframe と pointer event の相互変換を定義する
- ease / overshoot / lag を pointer motion に適用しやすくする
- layer や composition に紐づけられる参照を用意する

### 完了条件

- 手で打った pointer motion を後から修正できる
- レイヤー上の演出として再利用できる

---

## Non-Goals

- OS の実マウス操作をそのまま制御すること
- 入力エミュレーションの低レベル実装
- 画面録画 API の実装
- UI 描画そのもの

---

## Related

- `docs/planned/MILESTONE_ANIMATION_DYNAMICS_CORE_2026-03-28.md`
- `docs/planned/MILESTONE_ANIMATION_DYNAMICS_UI_2026-03-28.md`
- `ArtifactCore/include/Animation/AnimatableValue.ixx`
- `ArtifactCore/include/Animation/AnimatableTransform2D.ixx`
- `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`

## Current Status

2026-03-28 時点では、モーショングラフィック向けの仮想マウスは Core の独立データモデルとしては未整備。
この文書で、`Pointer` / `VirtualPointer` を独立した再生可能オブジェクトとして定義する。
