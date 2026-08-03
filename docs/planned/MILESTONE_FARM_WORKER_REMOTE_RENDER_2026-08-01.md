# リモートレンダーファームワーカー 実装マイルストーン

**日付**: 2026-08-01（修正）
**現状**: ファーム基盤（RPC通信・ジョブ割り当て・結果報告）は完成。**リモートワーカーがダミースタブ**で、レンダリングせずに即座に完了報告している。
**発見箇所**: `Artifact/src/Worker/FarmWorkerMain.cppm` L40-44
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

## 検証チェックリスト

- [ ] Worker プロセスが起動し Master に登録される
- [ ] Master がジョブを Worker に割り当て、Worker が実際にレンダリングする
- [ ] Worker のレンダリング結果が Master 側のレンダリング結果とピクセルレベルで一致
- [ ] Worker の障害（クラッシュ）時に別 Worker でリトライされる
- [ ] Worker のメモリがジョブ間で解放される（リークしない）
- [ ] GPU ヘッドレスモードで Diligent が正常動作する
- [ ] 2台の PC で Master + Worker の分散レンダリングが動作する
