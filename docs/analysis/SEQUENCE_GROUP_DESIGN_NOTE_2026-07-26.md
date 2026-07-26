# Sequence Group 設計メモ (2026-07-26)

**Status:** Draft

## 概要

複数レイヤーを一定の順序・時間差・フェーズで動かすための、再利用可能な `Sequence` 概念を導入する。

代表例は、カードが右から左へ順番に移動し、到着したカードから順番に裏返る演出である。個別キーフレームや多数のリアクティブイベントを手作業で調整する代わりに、グループ単位の宣言的な設定として編集できるようにする。

## 推奨アーキテクチャ

`Sequence` を動作定義、`Sequence Player` を実行主体として分離する。

```text
Sequence
  ├─ Target selection
  ├─ Order / distribution
  ├─ Timing
  └─ Phases

Layer Component: Sequence Player
  ├─ Sequence reference
  ├─ Target scope
  ├─ Trigger
  └─ Playback state
```

### Sequence が持つもの

- 対象の選択方法
- 順序（左→右、右→左、中央→外側など）
- 分配方法（同時、Stagger、ランダム、カスタム順）
- 開始遅延、要素間隔、全体時間
- フェーズ列
- フェーズごとの duration、easing、開始条件
- 完了条件

### Sequence Player が持つもの

- どのレイヤー／子レイヤーへ適用するか
- 再生開始条件
- 再生、一時停止、逆再生、リセット
- 現在の再生状態
- Sequence のローカル上書き

## フェーズモデル

「移動してから反転」のような演出は、イベント接続ではなくフェーズ列で表現する。

```text
Phase 1: Translate
  duration: 0.40s
  easing: EaseOut

Phase 2: Flip
  start: Each element reaches Phase 1 end
  duration: 0.25s
  easing: EaseInOut
```

各フェーズの開始基準は、次のいずれかとする。

- 前フェーズ完了
- グループ開始からの絶対時間
- 各要素の開始時刻
- 各要素の前フェーズ完了時刻

## 時間設計

`element interval` と `total duration` を同時に主値にしない。どちらか一方を編集値とし、もう一方は自動計算する。

```text
Timing mode: By interval / By total duration
Start delay: 0.0s
Element interval: 0.12s
Distribution curve: Linear
```

初期実装では `By interval` を標準とし、要素数が変わっても演出のリズムを維持しやすくする。

## リアクティブイベントとの責務分担

既存のリアクティブイベントは、Sequence の内部タイミングを置き換えない。外部から再生状態を制御する入口として利用する。

```text
Reactive Event
  └─ Play / Pause / Reverse / Reset / OnComplete

Sequence
  └─ 誰を、どの順番で、どの時間差で、どう動かすか
```

既存イベント設計の `TriggerReaction` や `TimelineReaction` と接続する場合も、Sequence の設定やキーフレーム定義を書き換えず、プレイヤーの状態を操作するだけにする。

## 保存と再利用

Sequence は独立アセットとして保存可能にする。ただし、小さな演出のために必ずアセットを作る必要はないため、将来的にコンポーネント内の `Inline Sequence` も許可する。

```text
Sequence Source:
  - Asset
  - Inline
```

- 複数箇所で使う演出: Sequence アセット
- その場限りの演出: Inline Sequence
- 再生・イベント接続: Sequence Player コンポーネント

## UI 方針

通常のレイヤープロパティに Sequence の全設定を混在させない。`Components` 専用面に `Sequence Player` を表示し、そこで参照・対象・トリガー・再生状態を編集する。

Sequence アセット自体は専用エディタまたは展開可能な編集面で、対象選択、順序、時間設計、フェーズを編集する。

### Sequence Editor

Sequence 本体は、レイヤーの通常 Inspector ではなく `Sequence Editor` で編集する。Sequence アセットを Project View から選択すると、中央の編集領域が Sequence Editor に切り替わる構成を基本とする。

```text
Project View
  └─ Sequences
       └─ Card Entrance + Flip

中央: Sequence Editor
  ├─ 対象
  ├─ 順序・分配
  ├─ タイミング
  ├─ フェーズタイムライン
  └─ プレビュー

右: Sequence 詳細プロパティ
```

フェーズは簡易タイムラインで縦に並べ、グループ全体の時間設計を確認できるようにする。個別要素のキーフレームを直接編集するのではなく、Sequence のパラメータ変更が全要素へ反映される。

```text
時間 →   0.0       0.4       0.8

Card 1   [移動────] [反転──]
Card 2      [移動────] [反転──]
Card 3         [移動────] [反転──]
Card 4            [移動────] [反転──]
```

Sequence Editor で編集する項目は、次を基本とする。

- 対象選択（Scope、Filter）
- 順序（Order）
- 分配方法（Distribution）
- 開始遅延、要素間隔、全体時間
- フェーズの追加・削除・並び替え
- フェーズごとの duration、easing、開始基準
- プレビュー再生と選択要素の確認

一方、レイヤー側の `Sequence Player` では、Sequence 本体の編集ではなく、次の実行設定だけを扱う。

```text
Sequence: Card Entrance + Flip
Target: This Group's Children
Trigger: On Start
Playback: Once
```

Sequence 本体と適用先を混同しないため、Sequence Editor と Player の境界を UI 上でも明確にする。

### 編集フロー

基本操作は次の流れとする。

1. Project View で Sequence アセットを作成または選択する。
2. Sequence Editor で対象、順序、時間差を設定する。
3. フェーズを追加し、各フェーズの動作と開始基準を設定する。
4. Sequence Player を持つレイヤーをプレビュー対象として選択する。
5. Sequence Editor のプレビューで、実際の子レイヤー数と配置に対する結果を確認する。
6. 必要に応じて Player 側で適用先やトリガーだけを調整する。

Sequence アセット単体では具体的なレイヤーを所有しないため、プレビュー時だけ `Preview Target` を解決する。

```text
Sequence Asset
  + Preview Target Layer
  → 対象解決
  → 順序付け
  → 時刻割り当て
  → フェーズ評価
  → プレビュー
```

### 対象解決

対象指定は、次の優先順位で具体化する。

```text
1. 明示された子レイヤー集合
2. Filter 条件に一致する子レイヤー
3. Sequence Player が指定するスコープ
```

Sequence が特定レイヤー ID を直接保持することは避ける。レイヤーの複製、差し替え、名前変更で Sequence が壊れないようにするためである。

### インスタンス上書き

共有 Sequence の再利用性を維持するため、Player 側で上書きできる項目を限定する。

```text
Sequence Asset: Card Entrance + Flip

Player Override:
  Element Interval: 0.16s
  Direction: Reverse
```

初期段階で上書き可能にするのは、次のような適用時パラメータに限定する。

- 再生開始時刻
- 再生速度
- 方向
- 対象スコープ
- 要素間隔の倍率
- 反復回数

フェーズ構造、プロパティパス、合成規則などの構造的設定は Sequence 本体で管理し、Player から変更させない。

### プレビューと編集の境界

Sequence Editor のプレビューは、編集対象の Sequence を一時的に評価するだけとする。プレビュー操作によって、対象レイヤーのキーフレームや永続プロパティを書き換えない。

```text
編集値 → Sequence 評価 → Preview Overlay → 表示
```

実再生時も同じ評価経路を使い、プレビューと本番でタイミング結果が変わらないことを目標とする。

### 初期 UI の最小構成

最初の実装では、複雑なノードグラフを導入せず、次の3領域に限定する。

```text
左: Sequence 構造
  - Target
  - Order
  - Distribution
  - Phase list

中央: フェーズタイムラインとプレビュー

右: 選択項目のプロパティ
```

フェーズ間の条件分岐やイベントノードは、基本的な Sequence と Player の評価契約が安定した後に拡張する。

## 初期ユースケース

```text
対象: 子レイヤー（Card）
順序: 左→右
分配: Stagger
間隔: 0.12s

Phase 1: Translate（右から配置位置へ）
Phase 2: Flip（各カードの到着時に開始）
```

このユースケースで、カード数や配置を変更してもタイミング調整をやり直さずに済むことを受け入れ基準とする。

## 未決事項

- Sequence が直接プロパティを書き換えるか、既存の PropertyOverlay として評価するか
- 対象選択を子階層、タグ、型、名前、明示 ID のどこまで対応するか
- フェーズ間の値の競合をどの層で解決するか
- Sequence のプレビューをタイムライン上でどのように可視化するか
- インスタンスごとのローカル上書き範囲

## 実装状況

2026-07-26 時点で、ArtifactCore に `Sequence.SequenceGroup` の初期データモデルを追加した。

- `SequenceDefinition`: 順序、分配、タイミング、フェーズ列
- `SequencePhase`: フェーズの duration、開始基準、easing
- `elementStartTime()`: 要素ごとの Stagger 開始時刻
- `phaseStartTime()`: 要素とフェーズを組み合わせた開始時刻
- `durationForElementCount()`: 対象数を含む全体時間
- `SequencePlayer`: 再生、停止、一時停止、逆再生、リセット、経過時間の管理
- `SequencePlayback`: 実行状態と現在時刻の保持
- `SequenceTargetSelector`: children、filter、明示 ID による対象指定
- `SequenceOverrides`: Player 単位の間隔、速度、方向の限定上書き
- Artifact の `LayerComponentHost` に `builtin.sequence-player` の標準ディスクリプタを追加（初期状態は無効）

現時点では UI、Sequence アセットの永続化、Sequence Player のレイヤー適用、プレビュー評価は未実装である。次段階では Core モデルを既存のアセット／コンポーネント登録経路へ接続する。

## 設計上の注意

Sequence はレイヤー固有の巨大なプロパティグループにしない。再利用可能なデータ定義と、レイヤーに付随する再生制御を分離することで、レイヤーの責務と通常の Property Editor の密度を維持する。
