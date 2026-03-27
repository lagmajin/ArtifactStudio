# コード品質問題レポート (2026-03-25)

## Critical: Use-After-Free / クラッシュリスク

### 1.1 AIClient デタッチスレッドが this をキャプチャ

**場所:** `Artifact/src/AI/AIClient.cppm:42-57`

`std::thread` が `this` をキャプチャして `detach()` される。
`AIClient` が破棄されると `this->partialMessageReceived()` /
`this->messageReceived()` が dangling ポインタをデリファレンス。

**対策案:** `shared_from_this()` またはデストラクタで `thread.join()`。

### 1.2 RenderQueueService デタッチスレッドが this をキャプチャ

**場所:** `Artifact/src/Render/ArtifactRenderQueueService.cppm:1601-1722`

レンダリングワーカースレッドが `this` をキャプチャし `detach()`。
`QMetaObject::invokeMethod(this, ...)` を使用。サービス破棄中に
スレッドが実行中だと UB。

**対策案:** スレッド完了を待つシャットダウンメカニズムを追加。

### 1.3 ImageExporter デタッチスレッドが this をキャプチャ

**場所:** `ArtifactCore/src/IO/Image/ImageExporter.cppm:270-273, 283-286`

非同期書き込みスレッドが `this` をキャプチャし `detach()`。
`ImageExporter` 破棄中に `write()` が dangling `this` をデリファレンス。

**対策案:** 同上。

### 1.4 reinterpret_cast<ProjectItem*> を QVariant に格納

**場所:** `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` (13箇所: 414, 559, 870, 1485, 1523, 1599, 1622, 1776, 1961, 2178, 2348, 2420, 2817)
**場所:** `Artifact/src/Project/ArtifactProjectModel.cppm:242`

`ProjectItem*` ポインタを `reinterpret_cast<quintptr>` で `QVariant` に格納。
オブジェクトが削除された後も Variant が残っていれば use-after-free。

**対策案:** ID ベースのルックアップシステムに変更。または `QPointer` / weak reference を使用。

---

## High: スレッド競合

### 2.1 Condition Variable のウェイクアップ漏れ

**場所:** `Artifact/src/Playback/ArtifactPlaybackEngine.cppm:144-149`

`paused_` (atomic) を mutex 外でチェックしてから `condition_.wait()` に進む。
その間に `stopped_` がセットされて通知が来るとウェイクアップ漏れ。

**対策案:** wait の前後に mutex ロックを一貫して使用。

### 2.2 フレームバッファス왑で QImage 不完全コピー

**場所:** `Artifact/src/Playback/ArtifactPlaybackEngine.cppm:227-234`

`frontBuffer_` / `backBuffer_` のスワップは `bufferMutex_` で保護されているが、
`QMetaObject::invokeMethod` でラムダが `frontBuffer_` を値コピーする時点で
次のフレームスワップとの競合が発生する可能性。

**対策案:** スワップ前に QImage をコピーしてから渡す。

### 2.3 failureReason のデータ競合

**場所:** `Artifact/src/Render/ArtifactRenderQueueService.cppm:1634, 1664`

`failureReason` (QString) をワーカースレッドから書き込み
(line 1664)、メインスレッドから読み取り (line 1701)。
`QString` は同時読み書きに対してスレッドセーフではない。

**対策案:** mutex で保護するか、QMetaObject::invokeMethod 経由でのみアクセス。

---

## Medium: 空スタブ・無限ループ

### 3.1 空メソッド（サイレントに何もしない）

| 場所 | メソッド |
|------|----------|
| `ArtifactAbstractLayer.cppm:195-201` | `goToStartFrame()`, `goToEndFrame()`, `goToNextFrame()`, `goToPrevFrame()` |
| `ArtifactAbstractLayer.cppm:445,448` | `setTimeRemapEnabled()` |
| `ArtifactTimelineWidget.cpp:1696,1698,1700` | `paintEvent()`, `mousePressEvent()`, `mouseMoveEvent()` (イベントを飲み込む) |
| `ApplicationSettingDialog.cppm:672-677` | `ImportSettingPage::loadSettings()`, `saveSettings()` 等 6メソッド |
| `ArtifactProject.cppm:1147` | `createCompositions(const QStringList&)` |

**影響:** 呼び出し側が成功したと思っても何も起きない。

### 3.2 while(true) デコーダーループ

**場所:**
- `MediaPlaybackController.cppm:968-987` (`getNextVideoFrame`)
- `FFMpegVideoDecoder.cppm:240`
- `FFMpegAudioDecoder.cppm:244, 330`

デコーダー状態が不整合になると無限ループ。タイムアウトやキャンセルの仕組みがない。

### 3.3 const_cast で const メソッドから状態変更

**場所:**
- `ArtifactVideoLayer.cppm:467` — `const_cast<>(this)->seekToFrame()`
- `ArtifactTextLayer.cppm:326` — `const_cast<>(this)->updateImage()`
- `PlaybackClock.cppm:306` — `const_cast<>(impl_)->updateCurrentFrame()`

const 正当性に違反。オブジェクトが真に const だと UB。

---

## Low: コード品質

### 4.1 ディレクトリ名の typo

- `Artifact/src/Effetcs/` → 正: `Effects`
- `Artifact/src/Effetcs/Blur/GauusianBlur.cpp` → 正: `GaussianBlur`

### 4.2 TODO 残骸

- Artifact 側: 41 箇所
- ArtifactCore 側: 9 箇所
- 注目すべき未実装:
  - `GauusianBlur.cpp:64` — HLSL シェーダー未実装
  - `WaveEffect.cpp:113` — HLSL シェーダー未実装
  - `SpherizeEffect.cpp:114` — HLSL シェーダー未実装
  - `GlowEffect.cpp:79` — HLSL シェーダー未実装
  - `AbstractGeneratorEffector.cppm:98,106` — コア機能未実装
  - `FractureEngine.cppm:49` — 空リターン

---

## 推奨対応順

| 順序 | 対応 | 理由 |
|---|---|---|
| 1 | デタッチスレッド修正 (1.1-1.3) | クラッシュの直接原因 |
| 2 | QVariant ポインタ修正 (1.4) | メモリ安全性 |
| 3 | failureReason 競合修正 (2.3) | データ破損 |
| 4 | 空スタブの明示化 (3.1) | デバッグ容易性 |
| 5 | while(true) タイムアウト追加 (3.2) | フリーズ防止 |
| 6 | typo 修正 (4.1) | コード衛生 |
