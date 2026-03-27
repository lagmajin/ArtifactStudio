# マイルストーン: Layer Components - Physics / Behavior

> 2026-03-28 作成

## 目的

Layer 側に `Physics` と `Behavior` のコンポーネント枠を用意し、Inspector から見える設定単位として扱えるようにする。

ここでいう `Physics` は `Physics2D` の剛体シミュレーションそのものではなく、レイヤーの変形・追従・減衰・揺れ方を制御する layer-local な挙動を指す。
`Behavior` はレイヤーに紐づく簡易的な振る舞い設定やトリガー表現を指し、将来的な script / trigger / rule 系拡張の受け皿にする。

---

## 背景

現状の layer は `Layer` / `Transform` / 各種 source-specific group で構成されている。

この構成でも編集はできるが、次のような要望を一目で表せる受け皿が弱い。

- 追従して少し遅れる
- 収束時に少しオーバーシュートする
- 親や再生状態に応じて振る舞いを変える
- レイヤー固有の behavior note / trigger / rule をまとめたい

---

## 方針

### 原則

1. まずは `PropertyGroup` ベースで載せる
2. `Physics2D` の置き換えにはしない
3. layer type ごとに必要なものだけ出す
4. persistence と inspector を先に整える
5. runtime の挙動は段階的に足す

### 想定対象

- `ArtifactAbstractLayer`
- `ArtifactAudioLayer`
- `ArtifactVideoLayer`
- `ArtifactTextLayer`
- `ArtifactImageLayer`
- `ArtifactCameraLayer`
- `ArtifactParticleLayer`

---

## Phase 1: Component Surface

### 目的

Inspector に `Physics` / `Behavior` の group を出せるようにする。

### 作業項目

- `ArtifactAbstractLayer::getLayerPropertyGroups()` に component group の受け皿を追加
- `setLayerPropertyValue()` に component property の保存経路を追加
- layer ごとに必要な group だけ追加できるようにする

### 最低限のプロパティ候補

`Physics`
- `physics.enabled`
- `physics.followThroughGain`
- `physics.damping`
- `physics.stiffness`

`Behavior`
- `behavior.enabled`
- `behavior.mode`
- `behavior.note`

### 完了条件

- Inspector で component group が見える
- layer JSON に保存できる
- 既存 layer の編集を壊さない

---

## Phase 2: Physics Component

### 目的

Layer の変形に対する追従や収束の感じを layer-local に制御する。

### 作業項目

- spring / damping / follow-through の設定を layer 側に持つ
- playback 時に transform update に反映する
- parent velocity の影響を受ける設定を追加する

### 完了条件

- `Physics` を有効にした layer が、遅れ・揺れ・収束を持てる
- 既存の direct keyframe 挙動は無効化されない

---

## Phase 3: Behavior Component

### 目的

レイヤーの「振る舞い」設定をまとめて扱えるようにする。

### 作業項目

- `Behavior` group の persistence を確立する
- trigger / rule / note / preset の最小表現を定義する
- layer type 別の初期値を用意する

### 完了条件

- layer に簡易 behavior を保持できる
- inspector と search で辿れる

---

## Phase 4: UI / Workflow

### 目的

Component の編集導線を layer workflow に組み込む。

### 作業項目

- layer panel から physics / behavior を見つけやすくする
- property editor の空状態を減らす
- preset 適用の導線を追加する

### 完了条件

- component を探して編集する流れが短い
- 既存の `Transform` / `Effect` / `Audio` と衝突しない

---

## Related

- `docs/planned/IMPLEMENTATION_ANIMATION_PHYSICS_2026-03-22.md`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- `Artifact/include/Layer/ArtifactAbstractLayer.ixx`
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`
- `Artifact/src/Layer/ArtifactAudioLayer.cppm`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`

## Current Status

2026-03-28 時点では、layer は `Layer` / `Transform` と source-specific group を持っている。
`Physics` / `Behavior` はまだ共通の component surface としては存在しないため、この文書で layer-side component workstream を切り出す。

## Implementation Tasks

### Phase 1 Task Breakdown

1. `ArtifactAbstractLayer` の property group 構成を確認する
   - 既存の `Layer` / `Transform` / source-specific group と衝突しない追加位置を決める
   - component group の lifetime と生成タイミングを整理する
2. `physics.*` / `behavior.*` の property schema を固定する
   - `physics.enabled`
   - `physics.followThroughGain`
   - `physics.damping`
   - `physics.stiffness`
   - `behavior.enabled`
   - `behavior.mode`
   - `behavior.note`
3. persistence 経路を先に通す
   - JSON 保存 / 読み込み
   - 既存 layer の欠損時 fallback
   - unknown property の扱い

### Phase 2 Task Breakdown

1. `Physics` の layer-local 挙動を最小実装する
   - follow-through と damping を独立に扱う
   - parent velocity との関係を明確にする
2. runtime 反映の入口を 1 箇所に集める
   - playback 更新時の適用点を固定する
   - 既存 direct keyframe と競合しないようにする

### Phase 3 Task Breakdown

1. `Behavior` の最小表現を決める
   - note / mode / enabled をまず固定する
   - trigger / rule は文字列や enum の受け皿から始める
2. layer type 別の既定値を定義する
   - `Audio`
   - `Video`
   - `Text`
   - `Image`
   - `Camera`

### Phase 4 Task Breakdown

1. Inspector 側で group を見つけやすくする
   - property editor の空状態を減らす
   - component group の見出しを整理する
2. preset 適用の導線を作る
   - behavior note / physics preset の初期値を反映する
   - layer panel と inspector の表示名を合わせる
