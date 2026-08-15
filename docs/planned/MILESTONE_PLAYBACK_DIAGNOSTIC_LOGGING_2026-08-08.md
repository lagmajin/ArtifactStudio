# プレビュー再生診断ログ自動出力 & デバッグ基盤整備 (2026-08-08)

**最終更新:** 2026-08-15

**状態:** Phase 1〜4 とレンダー／エンコード診断の静的実装済み（実動作確認待ち）。再生要求から Paused / Stopped / 開始拒否まで、およびレンダーキュー開始からエンコーダ終了までを常時保存し、終了理由と自動診断を付与する。

## 現行コード監査 (2026-08-15)

`ArtifactPlaybackService::beginPlaybackSessionCapture()`／`finishPlaybackSessionCapture()` が再生開始・停止・一時停止・サービス破棄・開始拒否を記録し、`Logs/PlaybackSessions/playback_session_*.log` を7日または最大100件で保持する。ログにはフレーム同期数、公開 tick、RAM preview の hit/fail、preview route のサンプル、fallback 理由、自動診断が含まれる。`AppMain` 側には別途 `PlaybackDebugReports` の終了レポートもある。よって診断計装はコード上かなり実装済みだが、正常再生・停止・開始拒否・レンダー失敗の各ケースで実ファイルが生成されること、保持上限、内容の UI 表示は未検証である。

## レンダー／エンコード診断の追加（2026-08-09）

- 出力先: `%APPDATA%/ArtifactStudio/Logs/EncodeSessions/encode_session_*.log`
- ジョブごとの出力先、形式、codec、profile、encoder/render backend、解像度、fps、bitrate、フレーム範囲、音声mux設定を開始時に記録する。
- backend選択とfallback、フレーム描画失敗、エンコーダへのフレーム投入失敗、画像保存失敗、音声mux失敗を段階別に記録する。
- `ffmpeg.exe` pipe backendでは、実行引数、プロセス状態、終了コード、プロセスエラー、末尾の出力を記録する。
- native FFmpeg backendでは、open/add/finalizeの失敗理由を記録し、flush、遅延packet、trailer、output stream closeの失敗を最終ジョブ結果へ反映する。
- セッション末尾にジョブ結果と失敗根拠をまとめ、7日または100件を超えた古いログを削除する。

## 概要

プレビュー再生ボタンを押すたびに、再生系の詳細な診断ログを自動的にファイル出力する仕組みを導入する。
また、再生が動かない／フレームが進まない問題を調査するために必要なデバッグ計装を追加する。

## 現状分析

### 再生フローの構造

```
UI押下 → togglePlaybackPreview()
  → PlaybackService::play()
    → ① ensureCurrentCompositionBound() : コンポジション・フレームレンジを Engine に設定
    → ② ramPreviewBuild リクエスト（非同期。キャッシュ未完了でも再生開始をブロックしない）
    → ③ startAudioClock()
    → ④ composition->play() / engine_->play()
       → PlaybackEngine ワーカースレッドで runPlaybackLoop()
         → 経過時間から targetFrame を計算
         → updateFrame(targetFrame) → frameChanged シグナル発火（QImageは常にnull）
           → PlaybackService: syncCurrentCompositionFrame(position)
           → PlaybackService: publishFrame → storeFrameImageInRam / FrameChangedEvent
         → updateAudio()
       → CompositionRenderController 側:
         → previewPipeline_.render() が各フレームで走る
         → RAMプレビュー画像があればそれを使い、なければ通常レンダリング
```

### 既存の診断基盤

| 要素 | 状態 |
|------|------|
| `compositionViewLog` (Qt logging) | 存在。`QT_LOGGING_RULES="artifact.*=true"` で有効化。ただしファイル出力は未設定 |
| `ArtifactCore::Logger::instance()` | in-memory 循環バッファ + ファイル出力対応。`LogCategory::RenderVP` 他あり |
| `beginPlaybackErrorCapture()` / `finishPlaybackErrorCapture()` | play時に自動開始、stop時にエラーがあればファイル保存。**正常時・途中経過は保存されない** |
| `AppDebuggerWidget` Playback タブ | 状態サマリ表示のみ。ログ出力機能なし |
| PlaybackEngine の `qDebug()` | 開始/停止/状態遷移のみ出力。ループ内部の定期ログはなし |

### 問題点

1. **エラー時しかログが保存されない** — 正常に見えても再生が進まないケースでは何も残らない
2. **PlaybackEngine のワーカースレッド内部ログが極めて少ない** — `updateFrame` やフレームスキップ検出のみ。targetFrame 計算、ループ反復頻度、emit 成否が全く見えない
3. **CompositionRenderController 側の RAMプレビュー参照ロジックが未ログ** — フレーム画像がキャッシュにあるか／readback に失敗しているかが不明
4. **フレーム同期（goToFrame）の成否が不明** — composition スレッドへのディスパッチ結果が追跡不能
5. **環境変数 `QT_LOGGING_RULES` 設定が必要** — エンドユーザーが自発的に有効化するのが難しい

## 実装計画

### Phase 1: プレイボタン押下時のセッションログ自動出力（必須・即時）

**目的**: 再生開始〜停止までの全ログを、再生ボタンを押すたびに自動的にファイル保存する。

**変更箇所**: `ArtifactPlaybackService::Impl` (`Artifact/src/Service/ArtifactPlaybackService.cppm`)

既存の `beginPlaybackErrorCapture()` / `finishPlaybackErrorCapture()` を以下のように拡張:

```cpp
// 再生セッションごとに常にログを保存する（エラーの有無にかかわらず）
void beginPlaybackSessionCapture() {
    playbackSessionCaptureActive_ = true;
    playbackSessionStartedAt_ = QDateTime::currentDateTime();
    playbackSessionStartFrame_ = engine_->currentFrame().framePosition();
    // 既存のエラーキャプチャも同時開始
    beginPlaybackErrorCapture();
}

void finishPlaybackSessionCapture() {
    playbackSessionCaptureActive_ = false;
    finishPlaybackErrorCapture();
    
    const QDateTime finishedAt = QDateTime::currentDateTime();
    // Loggerからセッション期間内のログを抽出
    const auto logs = ArtifactCore::Logger::instance()->getLogs();
    std::vector<ArtifactCore::LogMessage> sessionLogs;
    for (const auto &log : logs) {
        if (log.timestamp >= playbackSessionStartedAt_ && log.timestamp <= finishedAt) {
            sessionLogs.push_back(log);
        }
    }
    
    // 常にファイル出力
    QString outputDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(QDir(outputDir).filePath("Logs/PlaybackSessions"));
    dir.mkpath(".");
    
    QString fileName = QString("playback_session_%1.log")
        .arg(playbackSessionStartedAt_.toString("yyyyMMdd_HHmmss_zzz"));
    QSaveFile file(dir.filePath(fileName));
    // ... (既存の error log 出力と同様のフォーマットで全ログ出力)
    
    // 追加: セッションサマリ情報
    output += QString("Playback session summary:\n");
    output += QString("  Started: %1\n").arg(playbackSessionStartedAt_.toString(Qt::ISODateWithMs));
    output += QString("  Finished: %1\n").arg(finishedAt.toString(Qt::ISODateWithMs));
    output += QString("  Start frame: %1  End frame: %2\n")
        .arg(playbackSessionStartFrame_)
        .arg(engine_->currentFrame().framePosition());
    output += QString("  Dropped frames total: %1\n").arg(droppedFrameCount_);
    output += QString("  RAM preview enabled: %1\n").arg(ramPreviewEnabled_);
    output += QString("  Composition: %1\n").arg(currentComposition_->id().toString());
}
```

**追加メンバ変数**:
```cpp
bool playbackSessionCaptureActive_ = false;
QDateTime playbackSessionStartedAt_;
int64_t playbackSessionStartFrame_ = 0;
```

**シグナル接続変更**: 
```cpp
// play() → stop() の代わりに、playbackStateChanged で Playing→Stopped 遷移時に保存
QObject::connect(engine_, &ArtifactPlaybackEngine::playbackStateChanged, owner_,
    [this](PlaybackState state) {
        if (state == PlaybackState::Playing) {
            beginPlaybackSessionCapture();
        } else if (state == PlaybackState::Stopped) {
            finishPlaybackSessionCapture();  // 常時保存
        }
        // ...既存処理
    });
```

**既存の `beginPlaybackErrorCapture` / `finishPlaybackErrorCapture` は内部統合**。エラーログ専用の別ファイル出力は廃止し、セッションログに統合する。

**ファイル名規則**: `playback_session_YYYYMMDD_HHmmss_zzz.log`
**出力先**: `%APPDATA%/ArtifactStudio/Logs/PlaybackSessions/`

---

### Phase 2: 再生系クリティカルパスの詳細ログ追加（必須・即時）

**目的**: 再生が止まる／フレームが進まない原因を特定するためのログポイントを追加。

#### 2-1. PlaybackEngine ワーカースレッド

**ファイル**: `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`

`runPlaybackLoop()` 内に以下のログを追加:

```cpp
// ループ開始時
qDebug() << "[PlaybackEngine][Loop] enter"
         << "state=" << (int)state_.load()
         << "currentFrame=" << currentFrame_.load()
         << "fps=" << fps << "speed=" << appliedPlaybackSpeed_;

// targetFrame 計算後（120フレームごとにサマリ）
if (targetFrame % 120 == 0 || targetFrame == playbackStartFrame_) {
    qDebug() << "[PlaybackEngine][Tick]"
             << "targetFrame=" << targetFrame
             << "lastEmitted=" << lastEmittedFrame_.load()
             << "elapsed=" << elapsedSeconds << "s"
             << (targetFrame != lastEmittedFrame_ ? "EMIT" : "SKIP");
}

// ループ終了時
qDebug() << "[PlaybackEngine][Loop] exit"
         << "finalFrame=" << currentFrame_.load()
         << "droppedTotal=" << droppedFrameCount_;
```

#### 2-2. PlaybackService フレーム同期

**ファイル**: `Artifact/src/Service/ArtifactPlaybackService.cppm`

```cpp
// syncCurrentCompositionFrame 内
qDebug() << "[PlaybackService][SyncFrame]"
         << "frame=" << position.framePosition()
         << "sameThread=" << (composition->thread() == QThread::currentThread())
         << "composition=" << (composition ? composition->id().toString() : "null");

// publishFrame 内
qDebug() << "[PlaybackService][PublishFrame]"
         << "frame=" << frameNumber
         << "hasImage=" << hasConcreteFrame
         << "composition=" << compositionId;
```

#### 2-3. CompositionRenderController RAMプレビュー参照

**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

`renderFrame()` またはフレーム描画パス内（RAMプレビュー参照箇所、line 27419〜27452付近）:

```cpp
qCDebug(compositionViewLog)
    << "[PlaybackRender]"
    << "frame=" << framePos
    << "sameComposition=" << playbackSameComposition
    << "previewState.ready=" << playbackPreviewState.ready
    << "previewState.imageAvailable=" << playbackPreviewState.imageAvailable
    << "previewState.failed=" << playbackPreviewState.failed
    << "previewState.reason=" << playbackPreviewState.reason
    << "ramPreviewFallback=" << useRamPreviewFallback
    << "fallbackReason=" << ramPreviewFallbackReason;
```

#### 2-4. togglePlaybackPreview エントリポイント

**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

```cpp
qDebug() << "[PlaybackUI] togglePlaybackPreview"
         << "isPlaying=" << playback->isPlaying()
         << "currentFrame=" << playback->currentFrame();
```

---

### Phase 3: ログ出力の自動有効化（必須）

**目的**: ユーザーが環境変数を設定しなくても、再生セッション中はログが確実に出力されるようにする。

**実装**: 

`ArtifactCore::Logger::instance()` の `tryFastLog` / `appendLog` はカテゴリフィルタなしで常時収集されている（in-memory 循環バッファ）。Qt の `qDebug()` / `qCDebug()` は `QT_LOGGING_RULES` に依存するが、`beginPlaybackSessionCapture()` 時点で `QLoggingCategory::setFilterRules` を呼んで必要なカテゴリを一時的に有効化:

```cpp
void beginPlaybackSessionCapture() {
    // 再生に関連する全ログカテゴリを有効化
    QLoggingCategory::setFilterRules(
        QStringLiteral("artifact.compositionview.debug=true\n"
                       "artifact.playback.debug=true\n"
                       "artifact.render.debug=true"));
    // ...
}
```

**注意**: セッション終了時に元のルールに戻す必要があるか検討。アプリ全体に影響を与えるので、スコープを限定するには `qInstallMessageHandler` のカスタムハンドラを使うほうが安全かもしれない。

より安全な代替案: `qInstallMessageHandler` でセッション中だけ追加のファイル出力ハンドラをインストールする。

---

### Phase 4: プレイバック停止時の自動診断レポート（推奨）

再生セッション終了時に、以下の診断情報をログファイル末尾に追記:

```cpp
// 診断レポート
output += "\n=== Diagnostic Report ===\n";
output += QString("Composition frame range: %1 - %2\n")
    .arg(currentComposition_->frameRange().start().framePosition())
    .arg(currentComposition_->frameRange().end().framePosition());
output += QString("Composition current frame: %1\n")
    .arg(currentComposition_->framePosition().framePosition());

// RAMプレビューサマリ
auto summary = ramPreviewSummary();
output += QString("RAM preview enabled: %1\n").arg(ramPreviewEnabled_);
output += QString("RAM preview ready frames: %1 / %2\n")
    .arg(summary.readyFrames).arg(summary.requestedFrames);
output += QString("RAM preview hit rate: %1%%\n").arg(summary.hitRate * 100.0f);

// プレイバックエンジン
output += QString("Engine current frame: %1\n")
    .arg(engine_->currentFrame().framePosition());
output += QString("Engine state: %1\n").arg((int)engine_->state());
output += QString("Dropped frames: %1\n").arg(droppedFrameCount_);

// レンダラ状態
if (auto* controller = /* CompositionRenderController のシングルトン取得方法 */) {
    output += QString("Renderer initialized: %1\n").arg(controller->isInitialized());
    output += QString("Renderer running: %1\n").arg(controller->isRunning());
}
```

---

### Phase 5: 再生不能時の視覚的通知（任意・後日）

再生が停止した理由（RAMプレビュー未完了、レンダリング失敗など）をビューポートに小さなインジケーターで表示する。これは Phase 1-4 完了後に検討。

---

## デバッグ手順（ユーザー向け）

実装後、以下の手順で原因特定を行う:

1. プレビューボタンを押す
2. 数秒待って停止する
3. `%APPDATA%/ArtifactStudio/Logs/PlaybackSessions/` を開く
4. 最新の `playback_session_*.log` を確認

**ログで確認する主要ポイント**:

| 確認項目 | キーワード |
|----------|-----------|
| 再生ループが開始したか | `[PlaybackEngine] Starting high-precision playback loop` |
| フレームが emit されているか | `[PlaybackEngine][Tick] ... EMIT` |
| フレーム同期が成功しているか | `[PlaybackService][SyncFrame]` |
| RAMプレビュー画像が利用可能か | `[PlaybackRender] ... imageAvailable=false` |
| フレームスキップが発生しているか | `realtime policy ... dropped` |
| コンポジションが正しく設定されているか | `[PlaybackService] play ignored: no current composition bound` |

---

## ファイル一覧（変更対象）

| ファイル | 変更内容 |
|----------|---------|
| `Artifact/src/Service/ArtifactPlaybackService.cppm` | Phase 1, 2-2, 3, 4 |
| `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` | Phase 2-1 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | Phase 2-3 |
| `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | Phase 2-4 |

新規ファイル: なし（既存ファイルの編集のみ）

---

## リスクと注意点

1. **ログファイルの蓄積**: 再生のたびにログファイルが生成される。古いファイルの自動削除（例: 7日以上経過したものを削除、または最大100ファイルまで）を実装すること。
2. **パフォーマンス**: `qDebug()` の追加によるワーカースレッドへの負荷。軽微だが、`[Tick]` ログは 120 フレームごとの間引きにした。
3. **スレッド安全性**: `qDebug()` はスレッドセーフ。`Logger::instance()->tryFastLog()` はロックフリー設計。
4. **`qInstallMessageHandler` の競合**: 既存の Logger が既にインストールしている可能性がある。共存確認が必要。
