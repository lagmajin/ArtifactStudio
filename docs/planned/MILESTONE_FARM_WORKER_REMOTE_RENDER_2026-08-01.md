# リモートレンダーファームワーカー 実装マイルストーン

**日付**: 2026-08-01（修正）
**最終更新**: 2026-08-15
**現状**: ファーム基盤と外部レンダラー実行は実装済み。**Artifact 内のコンポジションを直接ロードして描画する方式、GPU ヘッドレス初期化、実機での分散結果検証は未完了**。
**発見箇所**: `Artifact/src/Worker/FarmWorkerMain.cppm`
**狙い**: 実際のレンダリングパイプラインを接続し、分散レンダリングを完成させる

---

## 現状のコード（問題箇所）

```cpp
// FarmWorkerMain.cppm L34-45
client.setOnJobAssigned([&](const QJsonObject& jobData) {
    int startFrame = jobData["startFrame"].toInt(0);
    int endFrame = jobData["endFrame"].toInt(0);
    int step = jobData["step"].toInt(1);
    // ...
    for (int f = startFrame; f < endFrame; f += step) {
        client.sendFrameCompleted(f);  // 🔴 ここ！何もレンダリングしていない
    }
});
```

---

## Phase 1: 最小限の実レンダリング接続

### Step 1.1 — コンポジションロード
Worker プロセスは Master から `compositionId` を受け取っているので、プロジェクトを開きコンポジションを取得する。

```cpp
client.setOnJobAssigned([&](const QJsonObject& jobData) {
    QString compositionId = jobData["compositionId"].toString();
    int startFrame = jobData["startFrame"].toInt(0);
    int endFrame = jobData["endFrame"].toInt(0);
    int step = jobData["step"].toInt(1);

    // 1. プロジェクトを開いてコンポジションを取得
    auto* projectService = ArtifactProjectService::instance();
    auto comp = projectService->findComposition(CompositionID(compositionId)).ptr.lock();
    if (!comp) {
        for (int f = startFrame; f < endFrame; f += step) {
            client.sendFrameFailed(f, "Composition not found");
        }
        return;
    }

    // 2. オフスクリーンレンダラーを初期化
    const QSize compSize = comp->settings().compositionSize();
    auto offscreenRenderer = std::make_unique<ArtifactOffscreenRenderer2D>();
    offscreenRenderer->initialize(compSize.width(), compSize.height());

    // 3. フレームをレンダリング
    const auto& layers = comp->allLayerRef();
    for (int f = startFrame; f < endFrame; f += step) {
        comp->goToFrame(f);
        offscreenRenderer->clear();

        for (const auto& layer : layers) {
            if (!layer || !layer->isVisible() || !layer->isActiveAt(FramePosition(f)))
                continue;
            layer->goToFrame(f);
            layer->draw(offscreenRenderer.get());
        }

        // 4. 結果を保存
        QString outPath = QString("%1/%2_%3.png")
            .arg(jobData["outputPath"].toString(), compositionId, f);
        QImage frame = offscreenRenderer->readbackToImage();
        if (!frame.isNull() && frame.save(outPath)) {
            client.sendFrameCompleted(f);
        } else {
            client.sendFrameFailed(f, "Save failed");
        }
    }

    // 5. 後始末
    QCoreApplication::processEvents();
});
```

### Step 1.2 — 必要なインポート追加
```cpp
// FarmWorkerMain.cppm に追加
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Render.OffscreenRenderer2D;
import Frame.Position;
```

---

## Phase 2: GPU レンダリング対応

### Step 2.1 — Diligent デバイス初期化（ヘッドレス）
通常の Diligent 初期化はウィンドウハンドルが必要だが、ワーカープロセスはヘッドレス。
Diligent のオフスクリーンモード（`EngineCreateInfo` で `WindowHandle = nullptr`）を使用する。

```cpp
auto* deviceManager = DiligentDeviceManager::instance();
if (!deviceManager->initializeHeadless()) {
    for (int f = startFrame; f < endFrame; f += step) {
        client.sendFrameFailed(f, "GPU init failed");
    }
    return;
}
```

### Step 2.2 — ArtifactIRenderer のヘッドレスモード
`ArtifactIRenderer` を `setWidget(nullptr)`, `setViewportSize(w, h)` でヘッドレス初期化。

---

## Phase 3: リソース効率

### Step 3.1 — コンポジションのオンデマンドロード
全レイヤーをメモリに読み込むのではなく、レンダリングに必要なレイヤーのみロード:

```cpp
// 現状: compositionId → 全レイヤー読み込み（重い）
// 改善: compositionId → layerWhitelist をキューから受け取る
```

### Step 3.2 — バッチレンダリング（連続ジョブ）
1プロセスで複数ジョブを連続処理:

```cpp
while (true) {
    auto job = waitForNextJob();  // キューからブロッキング取得
    if (!job) break;
    renderJob(job);
}
```

### Step 3.3 — メモリリーク防止
各ジョブ完了後にレイヤーキャッシュ・テクスチャキャッシュを解放:

```cpp
comp->releaseAllLayerCaches();
GPUTextureCacheManager::instance()->clear();
```

---

## Phase 4: エラー処理と復旧

### Step 4.1 — フレームごとの例外分離
```cpp
for (int f = startFrame; f < endFrame; f += step) {
    try {
        renderFrame(f, comp, renderer);
        client.sendFrameCompleted(f);
    } catch (const std::exception& e) {
        client.sendFrameFailed(f, QString::fromUtf8(e.what()));
    }
}
```

### Step 4.2 — クラッシュ後の自動再接続
Worker がクラッシュした場合、Master はタイムアウトで切断を検知し、未完了フレームを別ワーカーに再割り当てする。これは既に `RenderFarmMaster::onWorkerDisconnected` に実装されている。

### Step 4.3 — 進行状況の通知
定期的に進捗を Master に報告（現在はフレーム完了のみ）:

```cpp
// 10フレームごとに進捗を送信
if ((f - startFrame) % 10 == 0) {
    QJsonObject progress;
    progress["completed"] = f - startFrame;
    progress["total"] = endFrame - startFrame;
    client.sendMessage("progress", progress);
}
```

---

## Phase 5: Worker 管理 UI

### Step 5.1 — Master のダッシュボード拡張
現在の `connectedWorkers()` + `workerInfo()` から取得できる情報を UI に表示:

```
┌─ Farm Workers ─────────────────────┐
│ ID          │ Status  │ Frames     │
│ worker-1234 │ Active  │ 45/100     │
│ worker-5678 │ Idle    │ -          │
│ worker-9999 │ Offline │ 12/30 ✗    │
└────────────────────────────────────┘
```

---

## Phase 6: クロスプラットフォーム対応

### Step 6.1 — ヘッドレスプロセスのビルド
Worker は 3D ビューポートや UI が不要なため、最小限のリンクでビルド可能:

```
CMake Target: FarmWorker
  - Core + Render + FFmpeg + Network
  - QtCore, QtNetwork のみ
  - no QtWidgets, no QtSvg
  → バイナリサイズ: ~30MB → ~8MB
```

### Step 6.2 — Linux/macOS サポート
`QTcpSocket` ベースなのでクロスプラットフォーム問題なし。
ヘッドレス Diligent は Linux で Vulkan、macOS で Metal。

---

## 残タスク優先順位

| 優先度 | タスク | ファイル | 工数 |
|--------|--------|----------|------|
| **P0** | **実レンダリングパイプライン接続** | `FarmWorkerMain.cppm` L34-45 | **小**（コアロジックは既存。接続が不足しているだけ） |
| P1 | GPU ヘッドレス初期化 | `DiligentDeviceManager` + Worker | 中 |
| P1 | フレームごとの例外分離 | `FarmWorkerMain.cppm` | 小 |
| P2 | バッチレンダリング（連続ジョブ） | Worker | 中 |
| P2 | メモリ解放（ジョブ間） | Worker | 小 |
| P2 | 進行状況通知 | Worker + Master | 小 |
| P3 | Worker 管理 UI | Master + Widget | 中 |
| P3 | クロスプラットフォームビルド | CMake | 中 |

---

## 2026-08-15 現行実装照合

### 実装済み

- `Artifact/src/Worker/FarmWorkerMain.cppm` は、割り当てられたフレーム範囲を受け取り、`renderPayload` をパス変換して一時 JSON に書き出し、`rendererExecutable --job <file>` を起動する実処理になっている。従って、旧記載の「何もレンダリングせず即座に完了報告」は現行コードには当てはまらない。
- Worker は GPU/CPU/プラグイン/OCIO/パス変換などの capability と環境上書きを登録でき、ジョブ／フレーム timeout、リトライと指数バックオフ、終了コード確認、単一出力・連番出力の存在検証、フレーム単位の成功／失敗／進捗／ログ報告を持つ。
- `RenderFarmMaster` / `RenderFarmWorker` / `CheckpointStore` は、リモートスライス、進捗集計、ジョブ履歴、チェックポイント保存、再試行・通知の基盤を提供している。

### 未完了または未検証

- `FarmWorkerMain` 自体は `compositionId` から `Artifact` のコンポジションをロードして `ArtifactIRenderer` で描画する実装ではなく、外部 renderer のジョブ実行ランナーである。したがって Phase 1 のサンプル実装をそのまま実装済みとは扱えない。
- 実ファイルを別ワーカーへ転送・取得する artifact protocol、非連続範囲の checkpoint 復元、切断ワーカーの未完了フレーム再配分、worker 管理 UI はコード上の基盤と別に完成確認が必要。
- GPU ヘッドレス Diligent 初期化、2 台以上の実機分散、ローカルレンダーとのピクセル一致、ジョブ間の GPU／レイヤーキャッシュ解放、メモリリークは未検証。今回はビルド・テストを実行していない。

### 判定更新

旧 P0 は「ダミースタブ除去」では完了。現在の主な残課題は、外部 renderer 契約と Artifact の実レンダリングパイプラインを接続すること、成果物／checkpoint／再配分の運用契約を固めること、実機での分散検証である。

---

## 検証チェックリスト

- [ ] Worker プロセスが起動し Master に登録される
- [ ] Master がジョブを Worker に割り当て、Worker が実際にレンダリングする
- [ ] Worker のレンダリング結果が Master 側のレンダリング結果とピクセルレベルで一致
- [ ] Worker の障害（クラッシュ）時に別 Worker でリトライされる
- [ ] Worker のメモリがジョブ間で解放される（リークしない）
- [ ] GPU ヘッドレスモードで Diligent が正常動作する
- [ ] 2台の PC で Master + Worker の分散レンダリングが動作する
