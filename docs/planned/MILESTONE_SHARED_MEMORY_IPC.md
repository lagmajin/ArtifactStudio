# MILESTONE: Shared Memory IPC Framework

**最終更新:** 2026-08-05
**ステータス:** In Progress
**日付**: 2026-08-04
**現状**: `QSharedMemory` 使用ゼロ。mmap ゼロ。GPU テクスチャのプロセス間共有ゼロ。全プロセス間通信が TCP（RenderFarm）、QLocalSocket（ProjectBundleIpc）、QProcess stdin/stdout（Sandbox/MCP）のストリームベース。農場ローカルワーカーの出力は全フレームがファイルシステム経由。
**目標**: GPU テクスチャのプロセス間ゼロコピー共有（Vulkan external memory + D3D11 shared handle）、ローカルレンダーファーム用共有メモリリングバッファ、汎用 `SharedMemoryRingBuffer` ユーティリティ。

## Update 2026-08-15

- 「`QSharedMemory` 使用ゼロ」という冒頭の現状記述は古い。`ArtifactCore/src/IPC/SharedMemoryRingBuffer.cppm` に SPSC 可変長リング、wrap marker、CRC32C、統計、セマフォ通知、blocking read、reconnect／persistent 状態の基盤が実装されている。
- `RenderFarmSharedBuffer` は `ImageF32x4_RGBA` の RGBA32F frame payload をリングへ書込み・読出しする producer／consumer API を持ち、`IPCChannel` には SharedMemory／QLocalSocket／QTcpSocket／QProcess の transport 抽象と shared-memory の `sendZeroCopy` 経路がある。
- ただし現行コード検索では、RenderFarm の通常実行経路、Sandbox の画像結果、GPU external memory／D3D11 shared handle／Vulkan external memory への実接続は確認できない。ファイル経由・既存ストリーム経路を置き換えたとは判定しない。
- 100,000エントリのプロセス間試験、4K 128MB wrap、persistent 異常終了復旧、4K実測、transport fallback の実測結果は確認できない。現状は `Phase 1 / P3 / P4 foundation implemented; production integration, GPU sharing, and runtime validation pending` と整理する。

### 実装進捗（2026-08-05）

- P1 の `SharedMemoryRingBuffer` を追加。共有メモリ上の SPSC 可変長リング、ラップマーカー、CRC32C、統計、セマフォ通知、タイムアウト読取、再接続 API を実装。
- P3 の `RenderFarmSharedBuffer` を追加。`ImageF32x4_RGBA` の RGBA32F 非圧縮フレームをリングへ格納・復元する producer/consumer API を実装。
- P4 の `IPCChannel` を追加。SharedMemory、QLocalSocket、QTcpSocket、QProcess stdin/stdout の同期メッセージ API と `sendZeroCopy` の共有メモリ経路を実装。
- 未完了: プロセス間 100,000 エントリ試験、4K 128MB 実測、persistent の異常終了復旧検証、GPU 共有、RenderFarm 統合、IPCChannel 統合。

## 現状のボトルネック

| シナリオ | 現行転送 | データ量 | オーバーヘッド |
|---------|---------|--------|--------------|
| Sandbox プラグイン→Compositor のレンダリング結果 | **未実装**（現在 sandbox は制御チャネルのみ） | 4K RGBA32F = 128MB/フレーム | 実装された場合: ファイルシステム往復 + GPU 再アップロード = 5-10ms |
| ローカルレンダーファーム→エンコーダー | ファイルシステム（outputPath 書出→読取） | 4K フレーム 8-64MB、シーケンス GB 級 | ディスクI/O: 5-50ms/フレーム + SSD 消耗 |
| インスタンス間コンポジション転送 | QLocalSocket + JSON | 最大 32MB | 転送 <2ms + JSON serde 50-200ms（serde が律速） |

**GPU テクスチャ共有は最優先**: 現在 sandbox プラグインはレンダリング結果を返すパスがなく、このパスを新設するなら最初からゼロコピーで設計すべき。

---

## Phase 1: SharedMemoryRingBuffer 汎用ユーティリティ

### 1.1 基本設計

複数プロセス間で使える共有メモリ上のリングバッファ。単一の書き手と単一の読み手（SPSC）でロックフリー。

```cpp
// ArtifactCore/include/IPC/SharedMemoryRingBuffer.ixx
namespace ArtifactCore::IPC {

// 共有メモリ上のヘッダ（全プロセスから同じ物理ページを参照）
struct alignas(64) RingBufferHeader {
    // キャッシュライン分離（false sharing 防止）
    alignas(64) std::atomic<uint64_t> writeIndex{0};   // 書き手のみ更新
    alignas(64) std::atomic<uint64_t> readIndex{0};    // 読み手のみ更新
    
    uint64_t totalCapacity;        // データ領域の総バイト数
    uint64_t elementSize;          // 固定サイズエントリの場合は >0
    uint32_t magic;                // バリデーション用マジックナンバー
    uint32_t version;              // レイアウトバージョン
    char name[64];                 // 識別名
};

// 各エントリのプレフィックス（可変長エントリ用）
struct alignas(8) RingBufferEntryHeader {
    uint64_t sequenceNumber;       // 単調増加シーケンス番号
    uint64_t timestampNs;          // 書き込み時刻（monotonic clock）
    uint32_t payloadSize;          // ペイロードのバイト数
    uint32_t flags;                // フラグ（圧縮、暗号化など）
    uint32_t checksum;             // CRC32C
    uint32_t reserved;
};

class SharedMemoryRingBuffer {
public:
    struct Config {
        QString name;              // 共有メモリの一意な名前（OS間で共有）
        size_t totalSize;          // 総バイト数（ヘッダ + データ領域）
        size_t maxEntrySize;       // 1エントリの最大サイズ（0=制限なし）
        bool create;               // true=作成, false=既存を開く
        bool persistent;           // true=全プロセス終了後も残す
    };

    // 作成 / 開く
    static std::unique_ptr<SharedMemoryRingBuffer> create(const Config& config);
    static std::unique_ptr<SharedMemoryRingBuffer> open(const QString& name);

    // 書き込み（単一ライター）
    struct WriteResult {
        bool success;
        uint64_t sequenceNumber;
        QString error;
    };
    WriteResult write(const uint8_t* data, size_t size, uint32_t flags = 0);
    WriteResult write(const QByteArray& data, uint32_t flags = 0);
    
    // 読み取り（単一リーダー）
    struct ReadResult {
        bool success;
        uint64_t sequenceNumber;
        uint64_t timestampNs;
        QByteArray data;
        QString error;
    };
    ReadResult read();                       // 次を読む（ブロッキングなし）
    ReadResult readBlocking(int timeoutMs);  // データ到着を待つ
    ReadResult readSequence(uint64_t seq);   // 特定シーケンス番号を読む
    
    // 状態
    uint64_t availableForWrite() const;     // 書き込み可能バイト数
    uint64_t availableForRead() const;      // 読み取り可能バイト数
    bool isEmpty() const;
    bool isFull() const;
    
    // 制御
    void reset();                           // 読み取り位置をリセット
    void close();
    
    // プロセス間同期（オプション）
    QSystemSemaphore* writeSemaphore();     // 書き込み通知用
    QSystemSemaphore* readSemaphore();      // 読み取り完了通知用
    
    // 診断
    struct Stats {
        uint64_t totalWrites;
        uint64_t totalReads;
        uint64_t totalWriteBytes;
        uint64_t totalReadBytes;
        uint64_t droppedWrites;             // バッファフルによるドロップ
        uint64_t lastWriteTimestampNs;
        uint64_t lastReadTimestampNs;
    };
    Stats stats() const;

private:
    QSharedMemory sharedMemory_;
    RingBufferHeader* header_;   // 共有メモリの先頭をマップ
    uint8_t* dataRegion_;        // header_ + sizeof(RingBufferHeader)
    Config config_;
    
    // 書き込み: 空きが足りなければ最も古いエントリをスキップ
    // 読み取り: readIndex を進める
};
```

### 1.2 ロックフリーリングバッファ実装

```cpp
// 書き込みアルゴリズム（SPSC、ロックフリー）
SharedMemoryRingBuffer::WriteResult SharedMemoryRingBuffer::write(
    const uint8_t* data, size_t size, uint32_t flags)
{
    const uint64_t totalEntrySize = sizeof(RingBufferEntryHeader) + size;
    
    // 空き容量チェック
    const uint64_t w = header_->writeIndex.load(std::memory_order_acquire);
    const uint64_t r = header_->readIndex.load(std::memory_order_acquire);
    
    uint64_t available = header_->totalCapacity - (w - r);
    if (totalEntrySize > available) {
        return {false, 0, "Buffer full"};
    }
    
    // 書き込み位置をリング内にマップ
    uint64_t writeOffset = w % header_->totalCapacity;
    uint64_t writeEnd = writeOffset + totalEntrySize;
    
    if (writeEnd <= header_->totalCapacity) {
        // ラップなし: 1回の線形書き込み
        writeEntryAt(writeOffset, data, size, flags);
    } else {
        // ラップあり: 2回に分割
        uint64_t firstPart = header_->totalCapacity - writeOffset;
        // エントリヘッダ + ペイロードの一部を末尾に
        writePartialEntry(writeOffset, data, size, firstPart);
        // 残りを先頭に
        writePayloadContinuation(0, data + (firstPart - sizeof(RingBufferEntryHeader)),
                                  size - (firstPart - sizeof(RingBufferEntryHeader)));
    }
    
    // writeIndex の更新（release ordering: データが完全に書かれた後に可視化）
    header_->writeIndex.store(w + totalEntrySize, std::memory_order_release);
    stats_.totalWrites++;
    stats_.totalWriteBytes += size;
    
    return {true, nextSequence_++, ""};
}
```

### 1.3 プロセス間セマフォ

```cpp
// 書き込み側: データを書いたら読み手に通知
auto result = ringBuffer->write(frameData, frameSize);
if (result.success) {
    ringBuffer->writeSemaphore()->release();  // 読み手を起床
}

// 読み取り側: データが来るまでブロック
ringBuffer->readSemaphore()->acquire();
auto result = ringBuffer->read();
if (result.success) {
    processFrame(result.data);
}
```

### 1.4 完了条件

- [ ] 単一プロセス内で書き込み→読み取りが正しくラウンドトリップ
- [ ] 2プロセス（writer.exe + reader.exe）間で 100,000 エントリの連続転送が欠損・破損ゼロ
- [ ] 4K フレーム（128MB）の書込がラップアラウンドを含めて正しくコピーされる
- [ ] `readBlocking(1000)` が 1秒以内にデータが来なければタイムアウト
- [ ] プロセス異常終了後、再起動したリーダーが残存データを正しく読み取れる（`persistent=true`）

---

## Phase 2: GPU テクスチャのプロセス間共有

### 2.1 D3D11 共有テクスチャ（Windows）

D3D11 の `ID3D11Texture2D` は共有ハンドル経由でプロセス間共有が可能。Diligent Engine の D3D11 バックエンドを拡張する。

```cpp
// ArtifactCore/include/IPC/SharedGPUTexture.ixx
class SharedGPUTexture {
public:
    struct Config {
        uint32_t width;
        uint32_t height;
        Diligent::TEXTURE_FORMAT format;  // TEX_FORMAT_RGBA8_UNORM, TEX_FORMAT_RGBA32_FLOAT
        uint32_t mipLevels = 1;
        uint32_t arraySize = 1;
        bool createShared;                 // true=作成側, false=読み取り側
        QString sharedName;                // 共有ハンドル名（D3D11用）
    };

    // 作成側（レンダリングするプロセス）
    static std::unique_ptr<SharedGPUTexture> createShared(
        Diligent::IRenderDevice* device,
        const Config& config
    );

    // 読み取り側（コンポジター）
    static std::unique_ptr<SharedGPUTexture> openShared(
        Diligent::IRenderDevice* device,
        const QString& sharedName
    );

    // Diligent テクスチャとして取得
    Diligent::ITexture* texture() const;
    Diligent::ITextureView* textureView() const;

    // 同期
    void signalRenderComplete();    // レンダリング完了を通知
    void waitForRenderComplete();   // レンダリング完了を待機
    
    // 共有ハンドル（他プロセスに渡すため）
    QString sharedHandleName() const;
    
    void close();

private:
    // D3D11 実装
    void createD3D11Shared(ID3D11Device* d3dDevice);
    void openD3D11Shared(ID3D11Device* d3dDevice, const QString& name);
    
    Diligent::ITexture* texture_ = nullptr;
    Diligent::ITextureView* srv_ = nullptr;
    
    // D3D11 ハンドル
    HANDLE sharedHandle_ = nullptr;
    ID3D11Texture2D* d3dTexture_ = nullptr;
    
    // 同期用ミューテックス
    QSystemSemaphore renderComplete_{"", 0, QSystemSemaphore::Create};
};
```

### 2.2 Vulkan 外部メモリ（クロスプラットフォーム）

```cpp
// Vulkan 実装（D3D11 が使えない場合の代替）
void SharedGPUTexture::createVulkanExternal(
    Diligent::IRenderDeviceVk* deviceVk)
{
    VkExternalMemoryImageCreateInfo extMemInfo = {};
    extMemInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    extMemInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    
    VkImageCreateInfo imageInfo = { /* ... */ };
    imageInfo.pNext = &extMemInfo;
    
    VkImage vkImage;
    vkCreateImage(device, &imageInfo, nullptr, &vkImage);
    
    // メモリ割り当て + エクスポート
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, vkImage, &memReqs);
    
    VkExportMemoryAllocateInfo exportInfo = {};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    
    VkMemoryAllocateInfo allocInfo = { /* ... */ };
    allocInfo.pNext = &exportInfo;
    
    VkDeviceMemory memory;
    vkAllocateMemory(device, &allocInfo, nullptr, &memory);
    vkBindImageMemory(device, vkImage, memory, 0);
    
    // NT HANDLE の取得
    VkMemoryGetWin32HandleInfoKHR handleInfo = {};
    handleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    handleInfo.memory = memory;
    handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    
    vkGetMemoryWin32HandleKHR(device, &handleInfo, &sharedHandle_);
    
    // handleInfo.pNext で名前付き NT オブジェクトにすることも可能
    VkExportMemoryWin32HandleInfoKHR nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
    nameInfo.name = L"ArtifactSharedTexture_MainOutput";
    // ...
}
```

### 2.3 Sandbox プラグイン統合

```cpp
// Sandbox プロセス起動時のテクスチャ共有セットアップ
struct SandboxTextureConfig {
    uint32_t width;
    uint32_t height;
    QString outputTextureName;     // D3D11 共有ハンドル名
    QString inputTextureName;      // 入力テクスチャ（main compositor → sandbox）
};

// main プロセス
void PluginSandbox::startWithTextureSharing(const SandboxTextureConfig& texConfig) {
    // 1. 共有テクスチャを作成
    auto outputTex = SharedGPUTexture::createShared(
        renderDevice_, {texConfig.width, texConfig.height, 
                        TEX_FORMAT_RGBA32_FLOAT, 1, 1, true, texConfig.outputTextureName}
    );
    auto inputTex = SharedGPUTexture::createShared(
        renderDevice_, {texConfig.width, texConfig.height,
                        TEX_FORMAT_RGBA8_UNORM_SRGB, 1, 1, true, texConfig.inputTextureName}
    );
    
    // 2. 共有名をコマンドライン引数で sandbox に渡す
    QStringList args;
    args << "--shared-input-tex" << texConfig.inputTextureName;
    args << "--shared-output-tex" << texConfig.outputTextureName;
    
    // 3. sandbox プロセス起動
    sandboxProcess_->start(runnerPath, args);
    
    // 4. sandbox がテクスチャを開くのを待つ（ハートビートで確認）
    waitForSandboxReady();
    
    // 5. sandbox のレンダリング完了後、出力テクスチャから読み取り
    outputTex->waitForRenderComplete();
    compositor_->drawFromSharedTexture(outputTex->textureView());
}

// sandbox プロセス（PluginRunner）
void PluginRunner::openSharedTextures() {
    auto inputTex = SharedGPUTexture::openShared(renderDevice_, inputTextureName_);
    auto outputTex = SharedGPUTexture::openShared(renderDevice_, outputTextureName_);
    
    // レンダリングループ
    while (running_) {
        // main からのデータ到着を待機
        inputTex->waitForRenderComplete();
        
        // プラグインのレンダリング実行
        plugin_->render(inputTex->textureView(), outputTex->textureView());
        
        // 完了を通知
        outputTex->signalRenderComplete();
    }
}
```

### 2.5 完了条件

- [ ] D3D11 共有テクスチャ経由でプロセス間転送が動作（Windows）
- [ ] 4K RGBA32F テクスチャの転送が **1ms 未満**（ゼロコピー）
- [ ] 対比: ファイルシステム経由の同一転送が 5-15ms
- [ ] sandbox プラグインが共有テクスチャにレンダリングし、main compositor が即座に表示
- [ ] Vulkan external memory パスが D3D11 パスと同等の性能

---

## Phase 3: ローカルレンダーファーム用フレームバッファ

### 3.1 設計

`RenderFarmMaster` のローカルワーカーが現在使っているファイルシステム出力を、共有メモリリングバッファに置き換える。

```cpp
// ArtifactCore/include/IPC/RenderFarmSharedBuffer.ixx
class RenderFarmSharedBuffer {
public:
    struct Config {
        QString bufferName;              // 共有メモリ名
        size_t totalSizeMB = 1024;       // 1GB デフォルト
        uint32_t maxFrameWidth = 4096;
        uint32_t maxFrameHeight = 4096;
    };

    // Master（Consumer）側
    static std::unique_ptr<RenderFarmSharedBuffer> createConsumer(const Config& config);
    
    // Worker（Producer）側
    static std::unique_ptr<RenderFarmSharedBuffer> openProducer(const QString& bufferName);

    // Producer: フレームを書き込み
    struct FrameWriteResult {
        bool success;
        uint64_t frameSequence;
        QString error;
    };
    FrameWriteResult writeFrame(
        int64_t frameNumber,
        const ImageF32x4_RGBA& frame,
        uint32_t compressionFlags = 0   // 0=非圧縮, 1=BC7, 2=BC1
    );
    
    // Consumer: フレームを読み取り
    struct FrameReadResult {
        bool success;
        int64_t frameNumber;
        std::unique_ptr<ImageF32x4_RGBA> frame;
        uint64_t timestampNs;
        QString error;
    };
    FrameReadResult readNextFrame();          // 次を非同期読み取り
    FrameReadResult readFrame(int64_t frameNumber);  // 特定フレームを待つ
    
    // 状態
    int pendingFrameCount() const;
    size_t usedMemoryMB() const;
    size_t totalMemoryMB() const;
    float memoryPressure() const;
    
    // 消費者レート制御
    uint64_t consumerFrameRate() const;   // 消費速度（fps）
    uint64_t producerFrameRate() const;   // 生成速度（fps）

private:
    SharedMemoryRingBuffer ringBuffer_;
    QSystemSemaphore frameReadySemaphore_;
    
    // 圧縮フレームのエンコード/デコード
    QByteArray compressFrame(const ImageF32x4_RGBA& frame, uint32_t flags);
    ImageF32x4_RGBA decompressFrame(const QByteArray& data, int width, int height);
};
```

### 3.2 RenderFarmMaster 統合

```cpp
// 既存の renderOneFrame() を置き換え
void RenderFarmMaster::executeLocalRangeWithSharedBuffer(
    const RenderJobRequest& request,
    const RenderFrameRange& range)
{
    auto sharedBuffer = RenderFarmSharedBuffer::createConsumer({
        .bufferName = "ArtifactFarm_" + request.jobId.toString(),
        .totalSizeMB = 1024,
        .maxFrameWidth = request.width,
        .maxFrameHeight = request.height
    });
    
    // ワーカースレッド（Producer）
    std::vector<std::thread> workers;
    for (int i = 0; i < workerCount; ++i) {
        workers.emplace_back([&, i]() {
            auto producer = RenderFarmSharedBuffer::openProducer(
                "ArtifactFarm_" + request.jobId.toString()
            );
            
            for (auto frame : range.subRange(i, workerCount)) {
                ImageF32x4_RGBA output = request.renderFrame(frame);
                producer->writeFrame(frame, output);
            }
        });
    }
    
    // Consumer（同一プロセス内、メインスレッド）
    int64_t nextExpectedFrame = range.start;
    std::map<int64_t, std::unique_ptr<ImageF32x4_RGBA>> reorderBuffer;
    
    while (nextExpectedFrame <= range.end) {
        auto result = sharedBuffer->readNextFrame();
        if (result.success) {
            reorderBuffer[result.frameNumber] = std::move(result.frame);
            
            // 順序通りにエンコーダーに送る
            while (reorderBuffer.contains(nextExpectedFrame)) {
                encoder_->encodeFrame(nextExpectedFrame, 
                                      *reorderBuffer[nextExpectedFrame]);
                reorderBuffer.erase(nextExpectedFrame);
                nextExpectedFrame++;
            }
        }
    }
}
```

### 3.3 完了条件

- [ ] 8人のローカルワーカーが同時に共有バッファに書き込み、1人のコンシューマがフレーム順に読み出せる
- [ ] 4K フレームの disk I/O がなくなり、フレーム間レイテンシが **50ms → 5ms 未満** に短縮
- [ ] 1GB バッファがフルになった場合、コンシューマが追いつくまでプロデューサがブロック
- [ ] プロセス異常終了時、バッファが正しくクリーンアップされる

---

## Phase 4: IPC 抽象化レイヤー

### 4.1 統一 IPC インターフェース

既存の複数 IPC メカニズムを統一する抽象化レイヤー:

```cpp
// ArtifactCore/include/IPC/IPCChannel.ixx
enum class IPCTransport {
    SharedMemory,     // 新規: 同一マシン、大容量、ゼロコピー
    LocalSocket,      // 既存: 同一マシン、中容量（QLocalSocket）
    TcpSocket,        // 既存: リモート可能（QTcpSocket）
    Pipe              // 既存: QProcess stdin/stdout
};

struct IPCChannelConfig {
    IPCTransport transport;
    QString name;                 // 共有メモリ名 / ソケット名 / パイプ名
    size_t bufferSize = 65536;    // 受信バッファサイズ
    int timeoutMs = 5000;         // 接続タイムアウト
    bool encrypted = false;       // TLS（TCPのみ）
};

// メッセージ指向のIPC抽象化
class IPCChannel {
public:
    static std::unique_ptr<IPCChannel> create(const IPCChannelConfig& config);

    // メッセージ送受信
    virtual bool send(const QByteArray& message);
    virtual QByteArray receive();                    // ブロッキングなし
    virtual QByteArray receiveBlocking(int timeoutMs);
    
    // 大容量データのゼロコピー送信（共有メモリ選択時のみ有効）
    virtual bool sendZeroCopy(const uint8_t* data, size_t size);
    virtual bool receiveZeroCopy(uint8_t* buffer, size_t maxSize, size_t& received);
    
    // ストリーミング開始/終了（共有メモリのリングバッファモード）
    virtual bool beginStream();
    virtual bool endStream();
    
    // 状態
    virtual bool isConnected() const;
    virtual IPCTransport transport() const;
    virtual size_t pendingBytes() const;
    virtual void disconnect();
};
```

### 4.2 自動トランスポート選択

```cpp
std::unique_ptr<IPCChannel> IPCChannel::create(const IPCChannelConfig& config) {
    switch (config.transport) {
    case IPCTransport::SharedMemory:
        if (SharedMemoryRingBuffer::isAvailable()) {
            return std::make_unique<SharedMemoryChannel>(config);
        }
        // フォールバック: 共有メモリが使えなければ LocalSocket
        [[fallthrough]];
        
    case IPCTransport::LocalSocket:
        return std::make_unique<LocalSocketChannel>(config);
        
    case IPCTransport::TcpSocket:
        return std::make_unique<TcpSocketChannel>(config);
        
    case IPCTransport::Pipe:
        return std::make_unique<PipeChannel>(config);
    }
}
```

### 4.3 既存コードの移行マップ

| 既存コード | 新 IPCChannel |
|-----------|-------------|
| `RenderFarmMaster` → `NetworkPCServer` | `IPCChannel(SharedMemory)` for local, `IPCChannel(TcpSocket)` for remote |
| `ProjectBundleIpc` → `QLocalServer` | `IPCChannel(SharedMemory)` で大容量転送高速化 |
| `PluginSandbox` → `QProcess` stdin/stdout | 制御チャネルは `IPCChannel(Pipe)` 維持、データチャネルは `IPCChannel(SharedMemory)` |
| `McpTransport` → `QProcess` | 変更不要（小容量） |

### 4.4 完了条件

- [ ] `IPCChannel` が全4トランスポートで動作
- [ ] `sendZeroCopy()` が共有メモリでゼロコピー、他トランスポートでは通常コピーにフォールバック
- [ ] 自動トランスポート選択が動作し、共有メモリが使えない環境でも LocalSocket で動作

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | `ArtifactCore/include/IPC/SharedMemoryRingBuffer.ixx` | 新規: ロックフリーリングバッファ |
| P1 | `ArtifactCore/src/IPC/SharedMemoryRingBuffer.cppm` | 新規: QSharedMemory 実装 |
| P1 | `tests/ipc/shared_memory_ringbuffer_test.cpp` | 新規: 単一/複数プロセステスト |
| P2 | `ArtifactCore/include/IPC/SharedGPUTexture.ixx` | 新規: D3D11/Vulkan 共有テクスチャ |
| P2 | `ArtifactCore/src/IPC/SharedGPUTextureD3D11.cppm` | 新規: D3D11 実装 |
| P2 | `ArtifactCore/src/IPC/SharedGPUTextureVulkan.cppm` | 新規: Vulkan 実装 |
| P2 | `Artifact/src/Plugin/PluginSandbox.cppm` | 共有テクスチャ起動引数追加 |
| P3 | `ArtifactCore/include/IPC/RenderFarmSharedBuffer.ixx` | 新規: ファーム用フレームバッファ |
| P3 | `ArtifactCore/src/Render/RenderFarmMaster.cppm` | ファイルシステム→共有メモリ切替 |
| P4 | `ArtifactCore/include/IPC/IPCChannel.ixx` | 新規: 統一IPC抽象化 |
| P4 | `ArtifactCore/src/IPC/IPCChannel.cppm` | 新規: 自動トランスポート選択 |
| P4 | `Artifact/src/Service/ArtifactProjectBundleIpc.cppm` | QLocalServer→IPCChannel 移行 |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P1: RingBuffer | **P0** | 中 | 全後続の基盤。SPSCロックフリー実装が主 |
| P2: GPUテクスチャ共有 | **P0** | 大 | Sandbox プラグインのレンダリングパス開設の前提。D3D11/Vulkan 両対応が重い |
| P3: ファームフレームバッファ | **P1** | 中 | P1の上に構築。Disk I/O 排除で即効性 |
| P4: 統一IPCレイヤー | **P2** | 中 | 既存コードの移行が主 |
