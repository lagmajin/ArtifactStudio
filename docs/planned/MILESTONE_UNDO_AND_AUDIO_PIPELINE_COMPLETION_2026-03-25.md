# マイルストーン: Undo/Redo と Audio Pipeline の完成度向上

> 2026-03-25 作成

## 目的

完成度を上げるために、操作の安全性と音声再生の信頼性を先に底上げする。

対象は次の 2 系統。

1. `Undo/Redo`
2. `Audio pipeline`

この 2 つは UI の見た目よりも「触った時の安心感」と「製品としての完成感」に直結するため、機能追加より優先度が高い。

---

## 1. Undo/Redo

### 現状

Undo 系はすでに複数ある。

| 系統 | 状態 | 場所 |
|---|---|---|
| `ArtifactCore::UndoManager` | 実行時履歴の中心 | `Artifact/src/Undo/UndoManager.cppm` |
| `ArtifactCore::UndoCommand` 系 | ドメイン寄りのコマンド群 | `Artifact/include/Undo/UndoManager.ixx` |
| `Command.Serializable` / `Command.Session` | `QUndoStack` ベースの別系統 | `ArtifactCore/include/Command/*` |
| `Edit` menu | 入口はあるが実処理は弱い | `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm` |
| いくつかの UI | 独自に `UndoManager::instance()` を参照 | `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` など |

つまり、概念はあるが「正の履歴」が一本化されていない。

### 方針

`ArtifactCore::UndoManager` をアプリの実行時履歴の正にする。

理由:

- 既に `historyChanged`、`undoHistoryLabels`、`redoHistoryLabels` を持つ
- `ArtifactUndoHistoryWidget` が既にそれを表示している
- レイヤー操作や property 変更を domain command として束ねやすい

`Command.Session` / `QUndoStack` 系は、将来の同期や外部ツール連携のための補助層として残すか、`UndoManager` のアダプタへ寄せる。

### 実施順

1. `Edit` menu の `Undo/Redo` を `UndoManager` に直結する
2. レイヤー追加/削除/移動/トリム/プロパティ変更をコマンド化する
3. タイムラインドラッグや gizmo 操作を単一コマンドにまとめる
4. `UndoHistory` をセッション保存と復元に対応させる
5. `Command.Session` は必要なら serialize 層として縮退させる

### 期待効果

- 操作ミスの不安が減る
- レイヤー編集の挙動が安定する
- 大きい UI 変更でも壊れにくくなる

### リスク

- UI 直結変更が残ると、Undo と実 state がズレる
- コマンドが粒度過剰だと履歴が使いにくくなる
- property 統合前に transform を全部コマンド化すると同期点が散る

---

## 2. Audio Pipeline

### 現状

音声出力は一応あるが、まだ製品品質ではない。

| 層 | 状態 | 場所 |
|---|---|---|
| `AudioRenderer` | QAudioSink への送出器 | `ArtifactCore/src/Audio/AudioRenderer.cppm` |
| `QtAudioBackend` | Qt Multimedia backend | `ArtifactCore/src/Audio/QtAudioBackend.cppm` |
| `AudioSegment` | ミックス単位 | `ArtifactCore/include/Audio/AudioSegment.ixx` |
| `AudioMixer` / `AudioBus` / `AudioVolume` | あるが再生経路への統合は浅い | `ArtifactCore/src/Audio/*.cppm` |
| `AudioLayer` | WAV 読み込みの最小実装 | `Artifact/src/Layer/ArtifactAudioLayer.cppm` |
| `PlaybackEngine` | 再生中に `composition->getAudio()` を呼ぶ | `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` |

現状の問題:

- `AudioLayer` の音が出ても、再生デバイスや sample format によって壊れやすい
- mixer があるのに、composition 再生の正規ルートにまだ十分載っていない
- 単純加算ミックスなので、クリップ・パン・solo・bus が未完成

### 方針

`AudioMixer` を composition 再生の標準経路にする。

`AudioRenderer` は最終的な sink で、`AudioMixer` が各 layer/bus の音量・パン・mute/solo を処理する役目にする。

### 目標アーキテクチャ

```
AudioLayer / VideoLayer audio / other sources
  -> per-layer AudioBus
  -> AudioMixer
  -> master bus
  -> AudioRenderer
  -> QtAudioBackend / QAudioSink
```

### 実施順

1. `AudioLayer` を `AudioBus` に接続できるようにする
2. `composition->getAudio()` を mixer ベースへ移す
3. `AudioRenderer` の output format を sink に合わせて確実に変換する
4. `volume / mute / pan / solo` を bus 側で処理する
5. `AudioLayer` と `VideoLayer` の audio を同じ経路へ流す
6. 必要ならサンプルレート変換とリミッタを追加する

### 期待効果

- 無音や format mismatch の事故が減る
- レイヤーごとの音量調整が明確になる
- UI のミキサーが実際の再生に効くようになる

### リスク

- 変換レイヤーが増えると遅くなる
- 既存の `getAudio()` 実装と mixer が二重管理になると混乱する
- sample rate と frame rate の同期を雑に扱うと、すぐ破綻する

---

## まとめ

完成度を上げるなら、まずこの 2 本を優先する。

1. `Undo/Redo` を一本化して操作の安心感を上げる
2. `Audio pipeline` を mixer ベースにして再生品質を上げる

この 2 つが固まると、UI の細かい polish が「ちゃんと効く」土台になる。
