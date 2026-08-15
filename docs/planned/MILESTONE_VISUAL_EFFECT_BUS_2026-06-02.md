# MILESTONE: Visual Effect Bus

**Date**: 2026-06-02  
**最終更新**: 2026-08-15
**Status**: Proposed  
**Priority**: Medium  
**Related**: `docs/MILESTONE_COMPOSITION_FINAL_EFFECT_2026-04-14.md`, `docs/MILESTONE_EFFECT_SYSTEM_BRIDGE_2026-05-25.md`, `ArtifactCore/include/Audio/AudioBus.ixx`, `ArtifactCore/include/Composition/CompositionFinalEffect.ixx`, `Artifact/include/Engine/DAG/LayerGraphBuilder.ixx`

---

## Update 2026-08-15

- `CompositionFinalEffect`／`CompositionFinalEffectStack` は実装済みで、effect の追加・削除・並べ替え・enabled 抽出・JSON 保存／復元がある。Composition の composed image／buffer と render queue から final effect 適用経路も確認できる。
- ただしこれは composition 単位の final effect stack であり、現行コード上に Visual Effect Bus の send／return、共有中間 render target、複数レイヤーからの再利用、routing loop 検出、pre／post 契約は確認できない。
- UI も通常の layer effect／final effect の操作はあるが、bus 名・input／send 先・return 先・before／after を一体で示す専用 surface は未確認。Phase 1 の基礎は部分実装、Phase 2／3 は未完了。

## Update 2026-08-15 — 現行コード再確認

- `applyCompositionFinalEffectsToBuffer()` は composition-owned effect stack を受け取り、合成後の `ImageF32x4_RGBA` に Rasterizer effect を順番どおり適用する。Render Queue のCPU／GPU分岐、Composition Editorの比較取得、thumbnail経路から共通利用されている。
- これは Phase 1 の「composition final effect を最初の bus とする」実行基盤に相当するが、send／return として名前付き中間bufferを公開するAPIではない。
- したがって、Phase 1 は実行経路のみ部分完了。shared render targetの所有権、複数consumer、routing loop拒否、pre／post契約、専用UIは未実装または未検証とする。

## 概要

DAW の send / return / bus に近い考え方を、映像のエフェクト経路にも持ち込む。

ただし音声のようにサンプル列をそのままルーティングするのではなく、映像では **共有の中間レンダーターゲット** と **出力先の routing** を組み合わせる。  
ここでいう Visual Effect Bus は、レイヤーやコンポジションの途中結果を一時的に集約し、再利用し、最後に合成へ戻すための中継点として扱う。

---

## ねらい

- レイヤー単位の effect stack だけでは表現しにくい共通処理をまとめる
- 1 回だけ効く final effect と、複数箇所から使い回せる shared effect を分ける
- 「同じブラー / グロー / カラー補正を複数レイヤーで共用したい」を自然に扱う
- AE っぽい操作感として、`pre`, `post`, `send`, `return` の区別を見せやすくする

---

## コア概念

### 1. Composition Final Effect は最初の bus

既存の `CompositionFinalEffectStack` は、すでに

```text
全レイヤー合成
  -> final effect
  -> 出力
```

という構造を持つ。  
このため、最初の Visual Effect Bus は **コンポジション全体の final effect** として実現するのが最も安全。

### 2. Group / Shared Effect Bus

次の段階では、レイヤー群やソロ対象に対して共有 bus を導入する。

```text
Layer A ----\
Layer B -----+--> [Visual Effect Bus] --> return --> Composite
Layer C ----/
```

用途:

- グループ全体のグロー
- 共通色補正
- 共通のマスク済みブラー
- 1 つの中間結果を複数レイヤーへ返す処理

### 3. Send / Return は「画像共有の契約」

audio の bus と違い、映像は時間方向ではなくフレーム単位で進む。  
そのため send / return は「音を送る」ではなく、**中間画像をどこへ流すか** の契約になる。

---

## 実現方針

### Phase 1: Composition Bus

**目標**: 既存の final effect を bus として扱えるようにする。

- `CompositionFinalEffectStack` を最終 stage として明示する
- before / after の比較を UI 側で扱えるようにする
- render output 調整と final effect を同じ文脈で見せる

**完了条件**:

- コンポジション全体に 1 回だけ効く処理が明確になる
- output 調整と effect 追加の境界が崩れない

### Phase 2: Shared Render Target Bus

**目標**: レイヤー群が共通の中間レンダーターゲットを共有できるようにする。

- 中間 buffer の所有権を明確にする
- ループ検出を入れる
- pre/post の順序を定義する
- blend / matte / mask と競合しない順序を決める

**完了条件**:

- 同一 bus を複数レイヤーで再利用できる
- 破綻しやすい循環 routing を拒否できる

### Phase 3: UI Surface

**目標**: bus の存在を timeline / inspector から読めるようにする。

- bus の名前
- input / send 先
- return 先
- enabled / bypass
- before / after の切り替え

**完了条件**:

- 「どこに送って、どこから戻るか」が読める
- 既存の effect stack との区別が分かる

---

## 非目標

- audio bus の API をそのまま映像へコピーすること
- 新しいグローバル event bus を増やすこと
- Qt の新規 signal / slot を中心にした配線を増やすこと
- hot path で `QImage` へ寄せること

---

## 設計メモ

- 既存の `LayerGraphBuilder` は、レイヤー内 effect を DAG にする足場として使える
- `CompositionFinalEffectStack` は、bus の最初の着地点として自然
- visual bus を足すなら、まずは composition 単位から始めるのが安全
- その後、group / shared bus を足すほうが、UI とレンダリングの両方で破綻しにくい

---

## 推奨順序

1. Composition final effect を bus 的に扱う
2. group/shared render target を導入する
3. send / return の UI を作る
4. 必要になってから複雑な routing を増やす
