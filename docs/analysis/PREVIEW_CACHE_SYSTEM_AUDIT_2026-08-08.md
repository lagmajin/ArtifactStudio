# プレビューキャッシュシステム監査 (2026-08-08)

**最終更新:** 2026-08-09
**状態:** アーキテクチャ監査完了。主要なスレッド境界とキャッシュ状態整合性の指摘を対応済み。性能上の遅延は継続監視。

## 対応済み (2026-08-09)

- `ArtifactPlaybackService` の再生フレーム通知で、ワーカースレッドから
  `syncCurrentCompositionFrame()` を直接呼ばないように変更。既存の
  `QueuedConnection` 内でコンポジション同期とキャッシュ公開を同じ順序で実行する。
- `markFrameRequested()` は RAM 画像マップを正として `ready` / `inRam` /
  `cacheBitmap_` を同時に更新するように変更。失敗フラグが残ったフレームや古い
  `ready` ビットによる状態の食い違いを防止する。

## 監査対象ファイル

| ファイル | 役割 |
|----------|------|
| `Artifact/src/Service/ArtifactPlaybackService.cppm` | キャッシュ全般: 要求受付、状態管理、RAM/ディスク保存、読み出し |
| `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` | 再生ワーカースレッド。フレーム計算と frameChanged シグナル発行 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 実レンダリングと非同期 readback によるキャッシュ投入 |

## キャッシュデータ構造

```cpp
// frameCacheStates_: 全フレームの状態テーブル（配列インデックス = フレーム番号）
std::vector<ArtifactRamPreviewFrameCacheState> frameCacheStates_;

// 状態フィールド
struct ArtifactRamPreviewFrameCacheState {
    bool requested;       // ビルド要求済み
    bool ready;           // 画像取得済み
    bool failed;          // 失敗
    bool inRam;           // RAM上に存在
    bool onDisk;          // ディスクキャッシュあり
    bool imageAvailable;  // tryGetRamPreviewFrameImage で取得可能
    QString reason;       // 状態理由
};

// ramPreviewImageCache_: フレーム番号 → 画像のハッシュマップ
std::unordered_map<int64_t, ImageF32x4RGBAWithCache> ramPreviewImageCache_;

// LRU管理
std::list<int64_t> ramPreviewImageLru_;
std::unordered_map<int64_t, iterator> ramPreviewImageLruIndex_;
size_t ramPreviewImageCacheBudgetFrames_ = 128;

// ビルドキュー
struct RamPreviewBuildQueue {
    uint64_t generation;                // 新規ビルド要求ごとにインクリメント
    bool active;
    FrameRange range;
    std::deque<int64_t> pendingFrames;  // 未構築フレームのリスト
};
```

## キャッシュ完全フロー

### Step 1: ビルド要求 (playボタン押下時)

```
togglePlaybackPreview()
  → PlaybackService::play()
    → ramPreviewAutoPlaybackActive_ = true  ← ★再生中のキャッシュ利用を許可する鍵
    → requestRamPreviewBuild(range, "playback-auto-preview")
      → ramPreviewBuildQueue_.generation++
      → 全フレームを markFrameRequested(frame)
      → frameNeedsRamPreviewBuild(frame) なフレームを pendingFrames に積む
        （並び順: orderedRamPreviewFramesForRange = 現在フレーム→外側へ放射状）
    → engine_->play()
```

**ポイント**: `ramPreviewAutoPlaybackActive_` が `ramPreviewPlaybackFallbackWhilePlaying()` の判定で OR されるため、ユーザー設定で fallback 無効でも再生開始後は自動的にキャッシュ利用が許可される。

### Step 2: 再生エンジン (ワーカースレッド)

```
PlaybackEngine::runPlaybackLoop()
  → 経過時間から targetFrame 計算
  → updateFrame(targetFrame)
    → emit frameChanged(FramePosition(targetFrame), QImage())  ← ★常にnull画像
```
PlaybackEngine はフレーム画像を生成しない。画像は CompositionRenderController 側で生成される。

### Step 3: PlaybackService フレーム同期

```cpp
// engine_->frameChanged シグナル → (DirectConnection)
syncCurrentCompositionFrame(position);
// + QueuedConnection で publishFrame Lambda:
publishFrame() {
    if (hasConcreteFrame) {  // ★常にfalse (PlaybackEngineはQImageを送らない)
        storeFrameImageInRam(frameNumber, frameBuffer, "playback-frame");
        // + ディスク書き込みキューイング
    } else {
        markFrameRequested(frameNumber, "playback-tick");  // 要求状態マークのみ
        if (!hasFrameImageInRam(frameNumber))
            clearFrameFailure(frameNumber);
    }
    EventBus::publish<FrameChangedEvent>(...);  // ← これは常時発行
}
```

### Step 4: CompositionRenderController レンダリング

```
renderTickDriver_ (16ms間隔) → renderFrame()
  ├─ ① RAMプレビューキャッシュ参照
  │   playback->tryGetRamPreviewFrameImage(framePos, ramPreviewFrameImage)
  │   → state.ready && state.inRam && !state.failed && キャッシュマップに存在 → ヒット
  │   → キャッシュヒット時: useRamPreviewFallback = true
  │     画像をそのまま描画（レイヤーレンダリングをスキップ）
  │
  ├─ ② キャッシュミス時: 全レイヤー通常レンダリング
  │
  └─ ③ レンダリング後、ビルドキューに登録されていれば非同期readbackでキャッシュ投入
      if (isRamPreviewFramePendingBuild(framePos)) {
          renderer_->readbackTextureViewToImageAsync(
              [](QImage capturedFrame) {
                  publishPreviewFrameRequestResult(weakPlayback, request, capturedFrame);
              });
      }
```

### Step 5: 非同期 readback からキャッシュ保存

```cpp
publishPreviewFrameRequestResult(weakPlayback, request, capturedFrame) {
    // 4重バリデーション（古いビルド要求の結果を拒否）
    if (currentSummary.buildQueueGeneration != request.buildGeneration) return;
    if (currentSummary.buildQueueNextFrame != request.buildTargetFrame) return;
    if (request.buildTargetFrame != request.framePos) return;
    if (!weakPlayback->isRamPreviewFramePendingBuild(request.framePos)) return;

    weakPlayback->storeCompositionPreviewFrameImage(...);
}

storeCompositionPreviewFrameImage() {
    storeFrameImageInRam(frame, image, reason);
    // + ディスク書き込みキューイング（別スレッド）
    // + markFrameOnDisk(frame, false)  ← ディスク反映は非同期なので一旦false
}

storeFrameImageInRam(frame, image, reason) {
    ramPreviewImageCache_[frame] = ImageF32x4RGBAWithCache(image);
    touchRamPreviewImageLru(frame);      // LRU先頭に移動
    evictRamPreviewImagesIfNeeded();     // 128フレーム超過分を削除
    frameCacheStates_[frame].requested = true;
    frameCacheStates_[frame].ready = true;
    frameCacheStates_[frame].inRam = true;
    cacheBitmap_[frame] = true;
    completeRamPreviewBuildFrame(frame); // pendingFrames から削除
}
```

## アーキテクチャ検証結果: 正しい ✅

| 検証ポイント | 判定 | 根拠 |
|-------------|------|------|
| ビルド要求→キャッシュ保存 | ✅ | requestRamPreviewBuild → renderFrame → readback → storeFrameImageInRam 完結 |
| キャッシュ参照の三重チェック | ✅ | state.ready && state.inRam && !state.failed |
| 再生中のキャッシュ利用許可 | ✅ | ramPreviewAutoPlaybackActive_ で自動許可 |
| 古いビルド結果の拒否 | ✅ | publishPreviewFrameRequestResult 内の4重バリデーション |
| LRUエビクション | ✅ | 128フレーム超過時に最古を削除 |
| コンポジション変更時無効化 | ✅ | LayerChangedEvent 購読で invalidateRamPreviewForCurrentComposition |
| ディスク永続化 | ✅ | 別スレッドでプレビューディスクライターが非同期書き込み |
| ディスクキャッシュ無効化 | ✅ | previewDiskGeneration_ で旧タスクの書き込みを阻止 |
| ディスクキャッシュ予算管理 | ✅ | enforcePreviewDiskCacheBudget / GlobalBudget |
| 失敗フレームの完了処理 | ✅ | markFrameFailed も completeRamPreviewBuildFrame を呼ぶ |
| ディスクからの再水和 | ✅ | hydrateFramesFromDisk / hydratePreviewFrameFromDisk |

## 潜在的問題点

### 1. 初回フレームのキャッシュ不在（設計上の制約）

play() が `requestRamPreviewBuild` を呼ぶが、実際にレンダリングされるのは `renderTickDriver_` (16ms間隔) の次のティック。
初回数フレームはキャッシュがない状態で再生が始まる。これは設計上のトレードオフ（再生応答性 vs キャッシュ先行構築）。

### 2. PlaybackEngine は QImage を発行しない（設計通り）

`updateFrame()` が `emit frameChanged(pos, QImage())` と常に null 画像で発行するのは意図的。
コメントに「playback ticks must stay lightweight」と明記されている。
キャッシュへの画像保存は CompositionRenderController のレンダリングパスのみが担う。

### 3. frameCacheStates_ のサイズ依存（リスク極小）

`isValidFrameIndex()` が `frame < frameCacheStates_.size()` をチェックする。
`frameCacheStates_` は最初の `requestRamPreviewBuild` または `ensureFrameCacheCapacity` で拡張される。
再生開始前にビルド要求が来なければ範囲外になる可能性は理論上あるが、
`play()` → `requestRamPreviewBuild()` が必ず先に呼ばれるため実際には発生しない。

### 4. renderTickDriver_ 停止中はキャッシュ構築不可

キャッシュへの画像投入は CompositionRenderController のレンダリングループに依存する。
`controller_->stop()` 後は renderTickDriver_ が停止し、キャッシュ構築も停止する。
再生中は `controller_->start()` が呼ばれるので通常問題にならない。

### 5. 非同期 readback の遅延

`readbackTextureViewToImageAsync` は GPU → CPU の転送であり、完了には数フレームの遅延がある。
この間、再生が先に進んでしまい、キャッシュ構築が再生に追いつかない可能性がある。
これはハードウェア性能とコンポジションの複雑さに依存する。

### 6. markFrameRequested の ready フラグ非クリア

`markFrameRequested(frame, "playback-tick")` (PlaybackService::publishFrame の else 分岐) では、
既に画像がキャッシュにある場合は `state.ready` を true のまま維持する。
画像がない場合は `state.ready = false` にする。
しかし `state.failed` が true で画像がある場合は `cacheBitmap_` が false のままになる（line 1794-1803）。

```cpp
void markFrameRequested(const int64_t frame, const QString &reason) {
    state.requested = true;
    const bool hasRamImage = hasFrameImageInRam(frame);
    state.inRam = hasRamImage;
    if (!hasRamImage) {
        state.ready = false;
        cacheBitmap_[frame] = false;
    } else if (state.failed) {
        cacheBitmap_[frame] = false;  // ← ready はそのまま。failed なのに inRam なら不整合？
    } else if (state.ready) {
        cacheBitmap_[frame] = true;
    }
    // ... reason 設定
}
```

`state.failed && hasRamImage` の状態で `markFrameRequested` が呼ばれると `cacheBitmap_` は false になるが、
`state.ready` は元の値（おそらく true）のままになる。
→ `tryGetRamPreviewFrameImage` では `!state.ready` チェックがあるので通過してしまうが、
   `!state.failed` チェックもあり、そこで弾かれる。実害はなさそうだが微妙に不透明。
→ `clearFrameFailure` が先に呼ばれる（line 667）ので通常はこの経路に入らない。

## PlaybackEngine → PlaybackService の Qt signal/emit 問題

### 現状のディスパッチ経路（3層）

```
PlaybackEngine worker thread:
  emit frameChanged(position, QImage())  [Qt::DirectConnection]
    ↓
PlaybackService lambda (worker threadで実行):
  syncCurrentCompositionFrame(position)  ← composition->goToFrame() をワーカースレッドから呼ぶ
  invokeMethod(publishFrame, QueuedConnection)
    ↓
PlaybackService::publishFrame (main thread):
  storeFrameImageInRam / markFrameRequested
  emitRamPreviewStats()
  EventBus::publish<FrameChangedEvent>
```

### 問題1: DirectConnection によるクロススレッド呼び出し

`syncCurrentCompositionFrame()` は `composition->goToFrame()` を呼ぶ。composition はメインスレッドの QObject。
DirectConnection によりこの呼び出しがワーカースレッドで実行される。レースコンディションの元。

### 問題2: 不要な多段ディスパッチ

1フレーム更新に emit (DirectConnection) → invokeMethod (QueuedConnection) → EventBus::publish の3層。
各層でラムダの入れ子が増え、デバッグ時のコールスタック追跡が困難。
この emit はスレッド間関数呼び出しに過ぎず、Qt signal/slot の「複数購読者への multicast」も活用されていない。

### 問題3: AGENTS.md 方針違反

> 新規のシグナル＆スロット接続は絶対禁止

PlaybackEngine の W_OBJECT / W_SIGNAL 定義は既存だが、このパターンに依存した設計が再生系全体の複雑さを生んでいる。

### 推奨: 単一コールバック + invokeMethod 一発

```cpp
// PlaybackEngine: signal を廃止し、登録済みコールバックを invokeMethod 一発で投げる
void updateFrame(int64_t targetFrame) {
    QMetaObject::invokeMethod(owner_, [this, frame = targetFrame]() {
        if (frameCallback_) frameCallback_(frame);
    }, Qt::QueuedConnection);
}
```

PlaybackService がこのコールバックを受け、`syncCurrentCompositionFrame` + `FrameChangedEvent` 発行 + RAMプレビュー更新を**同期的に一括処理**する。
emit もラムダの入れ子も消え、ディスパッチは1層に。追跡も容易。

| 比較 | 現状 (signal/emit) | 推奨 (callback + invokeMethod) |
|------|-------------------|-------------------------------|
| ディスパッチ層数 | 3層 | 1層 |
| スレッド安全性 | DirectConnectionで破綻 | QueuedConnectionで常にメインスレッド |
| デバッグ追跡 | 困難（signal→lambda→invokeMethod→lambda→EventBus） | 容易（invokeMethod→単一関数） |
| AGENTS.md 準拠 | 違反（信号接続を前提とした設計） | 準拠 |

## キャッシュ構築進行の依存関係

```
play() 呼び出し
  ├── requestRamPreviewBuild()      [即時]
  ├── engine_->play()               [即時、ワーカースレッド開始]
  └── controller_->start()           [即時、renderTickDriver_ 開始]

renderTickDriver_ (16ms)
  └── renderFrame()
        ├── tryGetRamPreviewFrameImage()  [キャッシュ参照]
        └── readbackToImageAsync()        [キャッシュ構築、非同期]
              └── publishPreviewFrameRequestResult()
                    └── storeCompositionPreviewFrameImage()  [遅延あり]
```

**結論**: キャッシュアーキテクチャは正しい。問題があるとすれば、非同期 readback の遅延によりキャッシュ構築が再生速度に追いつかないケースであり、これはアーキテクチャではなく実行時性能の問題。
