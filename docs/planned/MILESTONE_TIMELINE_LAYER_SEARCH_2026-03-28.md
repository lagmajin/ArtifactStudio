# マイルストーン: Timeline Layer Search

> 2026-03-28 作成

## 目的

タイムライン上部の検索バーを、レイヤー名の単純フィルタではなく、入力中に即反映する検索 / 絞り込み / ジャンプの入口として整える。

このマイルストーンは、`ArtifactTimelineWidget` の検索状態を中心に、`ArtifactLayerPanelWidget` の表示制御と検索結果ナビゲーションを揃える。

---

## 背景

現在のタイムライン検索は、レイヤー名の部分一致による表示フィルタに近い。

これでは、次のような使い方に足りない。

- レイヤー名以外を探せない
- 見つけても作業文脈を失いやすい
- 結果へ素早く移動できない
- 検索状態を「表示モード」として残せない

---

## 方針

### 原則

1. 検索はインクリメンタルに反映する
2. 検索は単なる文字列一致ではなく、簡易クエリを受ける
3. 絞り込み、ハイライト、ジャンプを別の操作として扱う
4. 非一致を消しすぎず、文脈保持モードを優先する
5. 検索状態は一時入力ではなく、表示モードとして残す

### 想定対象

- layer name
- effect name
- tag
- layer type
- parent / child relation
- solo / lock / hidden / visible state
- used source asset name

---

## 簡易クエリ

### 文字列検索

- `shadow`
  - レイヤー名
  - エフェクト名
  - 関連表示名

### Prefix Query

- `type:text`
- `fx:blur`
- `tag:bg`
- `parent:none`
- `visible:false`

### 補足

- 余計な構文は増やしすぎない
- まずは `key:value` 形式の限定セットから始める
- 単独文字列は broad match として扱う

---

## 既存資産

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`

---

## Phase 1: Incremental Query Baseline

### 目的

入力中に即時反映する検索の土台を作る。

### 作業項目

- タイムライン上部に検索バーを常設する
- `ArtifactTimelineWidget` に検索状態を保持する
- `ArtifactLayerPanelWidget` のフィルタ更新を即時に反映する
- 検索中の hit count を表示する

### 完了条件

- 入力中に結果が変わる
- 検索欄を閉じなくても状態が分かる

---

## Phase 2: Search Modes

### 目的

検索を表示モードとして分離する。

### 作業項目

- 通常表示
- 検索フィルタ中
- 検索結果のみ表示中
- ハイライトのみ
- 非一致を半透明
- 非一致を非表示

### 完了条件

- 検索状態が作業を邪魔しない
- 文脈保持と絞り込みを切り替えられる

---

## Phase 3: Query Coverage

### 目的

検索対象をレイヤー名以外へ広げる。

### 作業項目

- effect name
- tag
- type
- parent / child
- solo / lock / hidden / visible
- used source asset name

### 完了条件

- 名前以外の中身検索ができる
- `parent:none` や `visible:false` が使える

---

## Phase 4: Result Navigation

### 目的

ヒットした項目へすぐ移動できるようにする。

### 作業項目

- hit count 表示
- `Enter` = 次
- `Shift+Enter` = 前
- `F3` = 次
- 現在ヒット位置の明示

### 完了条件

- いちいち目視で探さなくてよい
- hit を順送りで辿れる

---

## Phase 5: Layer State Presentation

### 目的

検索結果の見え方を、レイヤー状態に合わせて整理する。

### 作業項目

- non-match の薄化
- hidden / locked / solo の表示統一
- type / parent / source の補助表示
- 検索結果の強調ルールを統一する

### 完了条件

- 検索中でも状態が読める
- 大量レイヤーでも文脈を失いにくい

---

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

---

## Current Status

2026-03-28 時点で、タイムライン検索は layer / effect / tag / state の簡易クエリに対応している。

さらに `All Visible / Highlight Only / Filter Only` の表示モード切替を足し、検索を「見つける」から「作業導線にする」段階へ広げている。

この文書は、`ArtifactTimelineWidget` の検索バーを「入力中に即反映する検索 surface」として拡張するための専用 workstream として扱う。

---

## Related

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`
- `docs/planned/MILESTONE_APP_CROSS_CUTTING_IMPROVEMENT_2026-03-27.md`
