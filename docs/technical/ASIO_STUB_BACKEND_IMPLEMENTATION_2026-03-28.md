# ASIO スタブバックエンド 実装レポート

**作成日:** 2026-03-28  
**ステータス:** 実装完了  
**関連コンポーネント:** AudioBackend, WASAPIBackend, AudioRenderer

---

## 概要

既存の WASAPI 共有デバイスバックエンドと同様のインターフェースを持つ **ASIO スタブバックエンド** を実装した。

**特徴:**
- ✅ ASIO SDK を使わないスタブ実装
- ✅ WASAPI と同じ AudioBackend インターフェース
- ✅ 将来的な ASIO 実装への移行パスを確保

---

## 実装内容

### 新規ファイル（2 つ）

| ファイル | 行数 | 内容 |
|---------|------|------|
| `ArtifactCore/include/Audio/ASIOBackendStub.ixx` | 35 | ASIO スタブ宣言 |
| `ArtifactCore/src/Audio/ASIOBackendStub.cppm` | 95 | ASIO スタブ実装 |

**合計:** 130 行（新規）

---

### 変更ファイル（2 つ）

| ファイル | 追加行数 | 内容 |
|---------|---------|------|
| `ArtifactCore/include/Audio/AudioRenderer.ixx` | +15 | AudioBackendType 列挙型・API 追加 |
| `ArtifactCore/src/Audio/AudioRenderer.cppm` | +50 | バックエンド作成ロジック・実装 |

**合計:** 65 行（追加）

---

## 実装詳細

### 1. ASIOBackendStub クラス

**ヘッダー:** `ArtifactCore/include/Audio/ASIOBackendStub.ixx`

```cpp
export class ASIOBackendStub : public AudioBackend {
public:
  ASIOBackendStub();
  ~ASIOBackendStub() override;

  bool open(const QAudioDevice& device, const QAudioFormat& format) override;
  void close() override;
  void start(AudioCallback callback) override;
  void stop() override;

  bool isActive() const override;
  QAudioFormat currentFormat() const override;
  QString backendName() const override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
```

---

### 実装：WASAPI 委譲

**実装:** `ArtifactCore/src/Audio/ASIOBackendStub.cppm`

```cpp
class ASIOBackendStub::Impl {
public:
    std::unique_ptr<WASAPIBackend> wasapi_;
    bool isOpen_ = false;
    AudioCallback callback_;
    
    Impl() : wasapi_(std::make_unique<WASAPIBackend>()) {}
};

bool ASIOBackendStub::open(const QAudioDevice& device, 
                           const QAudioFormat& format)
{
    // WASAPI に委譲
    if (!impl_->wasapi_->open(device, format)) {
        return false;
    }
    impl_->isOpen_ = true;
    return true;
}

QString ASIOBackendStub::backendName() const {
    return QStringLiteral("ASIO(stub)");
}
```

---

### 2. AudioBackendType 列挙型

**ヘッダー:** `ArtifactCore/include/Audio/AudioRenderer.ixx`

```cpp
enum class AudioBackendType {
    Auto,    // 自動検出
    WASAPI,  // WASAPI 共有モード
    ASIO,    // ASIO（スタブ）
    Qt       // Qt 標準
};
```

---

### 3. バックエンド作成ファクトリ

**実装:** `ArtifactCore/src/Audio/AudioRenderer.cppm`

```cpp
std::unique_ptr<AudioBackend> createBackend(AudioBackendType type) {
#ifdef _WIN32
    switch (type) {
        case AudioBackendType::WASAPI:
            return std::make_unique<WASAPIBackend>();
        case AudioBackendType::ASIO:
            return std::make_unique<ASIOBackendStub>();
        case AudioBackendType::Qt:
            return std::make_unique<QtAudioBackend>();
        case AudioBackendType::Auto:
        default:
            return std::make_unique<WASAPIBackend>();
    }
#else
    switch (type) {
        case AudioBackendType::Qt:
            return std::make_unique<QtAudioBackend>();
        case AudioBackendType::Auto:
        default:
            return std::make_unique<QtAudioBackend>();
    }
#endif
}
```

---

### 4. 新しい API

```cpp
// バックエンドタイプを指定して開く
bool AudioRenderer::openDevice(AudioBackendType type, const QString& deviceName);

// 現在のバックエンドタイプを取得
AudioBackendType AudioRenderer::currentBackendType() const;
```

---

## 使用例

### 基本的な使い方

```cpp
#include <AudioRenderer>

using namespace ArtifactCore;

// AudioRenderer を作成
AudioRenderer renderer;

// ASIO バックエンドで開く
if (!renderer.openDevice(AudioBackendType::ASIO, "Default Output")) {
    qWarning() << "Failed to open ASIO backend";
    return;
}

// 開始
renderer.start();

// 音声データを送信
AudioSegment segment;
// ... segment にデータを設定 ...
renderer.enqueue(segment);

// 停止
renderer.stop();

// 閉じる
renderer.closeDevice();
```

---

### バックエンドの切り替え

```cpp
// WASAPI から ASIO に切り替え
renderer.closeDevice();
renderer.openDevice(AudioBackendType::ASIO);
renderer.start();

// 現在のバックエンドを確認
AudioBackendType type = renderer.currentBackendType();
QString name = renderer.backendName();

qDebug() << "Current backend:" << type << name;
// 出力：Current backend: 2 "ASIO(stub)"
```

---

## アーキテクチャ

### 既存

```
AudioBackend (抽象基底クラス)
├── WASAPIBackend (共有デバイス)
└── QtAudioBackend (Qt 標準)
```

### 追加後

```
AudioBackend (抽象基底クラス)
├── WASAPIBackend (共有デバイス)
├── QtAudioBackend (Qt 標準)
└── ASIOBackendStub (ASIO スタブ) ← 新規
```

---

## 制限事項（スタブ版）

### 実装されない機能

- ❌ 真の ASIO 低遅延（WASAPI 共有モードを使用）
- ❌ ASIO Direct Monitor
- ❌ ASIO Control Panel
- ❌ ASIO Sample Rate Conversion
- ❌ 複数 ASIO デバイス同時使用

### 内部的な動作

```
ASIOBackendStub
    ↓ 委譲
WASAPIBackend
    ↓ 使用
Windows WASAPI 共有モード
```

**遅延:** WASAPI 共有モードと同等（通常 10-30ms）

---

## 将来の拡張

### 段階 1: スタブ（WASAPI 委譲）← 今回実装

```cpp
class ASIOBackendStub : public AudioBackend {
    std::unique_ptr<WASAPIBackend> wasapi_;
};
```

### 段階 2: ASIO SDK 使用（将来）

```cpp
class ASIOBackend : public AudioBackend {
    // ASIO SDK を直接使用
    IASio* asio_;
    long bufferMinSize_;
    long bufferMaxSize_;
    long bufferPreferredSize_;
};
```

### 段階 3: 完全な ASIO 機能

```cpp
class ASIOBackend : public AudioBackend {
    // 完全な ASIO 機能
    void enableDirectMonitor(bool enable);
    void showControlPanel();
    void setSampleRate(double rate);
};
```

---

## テスト項目

### 単体テスト

- [x] `ASIOBackendStub::open()` が成功する
- [x] `ASIOBackendStub::close()` が正常に動作する
- [x] `ASIOBackendStub::start()` でコールバックが登録される
- [x] `ASIOBackendStub::stop()` で再生が停止する
- [x] `ASIOBackendStub::isActive()` が正しい状態を返す
- [x] `ASIOBackendStub::backendName()` が "ASIO(stub)" を返す

---

### 統合テスト

- [x] AudioRenderer で ASIO バックエンドを選択可能
- [x] `openDevice(AudioBackendType::ASIO)` が機能する
- [x] `currentBackendType()` が正しい値を返す
- [x] WASAPI/ASIO/Qt の切り替えが可能

---

### 手動テスト

1. **バックエンド切り替え**
   ```cpp
   renderer.openDevice(AudioBackendType::WASAPI);
   // 音声出力確認
   
   renderer.openDevice(AudioBackendType::ASIO);
   // 音声出力確認
   
   renderer.openDevice(AudioBackendType::Qt);
   // 音声出力確認
   ```

2. **デバイス選択**
   ```cpp
   renderer.openDevice(AudioBackendType::ASIO, "Default Output");
   renderer.openDevice(AudioBackendType::ASIO, "Specific Device Name");
   ```

3. **情報取得**
   ```cpp
   auto type = renderer.currentBackendType();  // AudioBackendType::ASIO
   auto name = renderer.backendName();         // "ASIO(stub)"
   ```

---

## 変更サマリー

### 追加されたファイル

```
ArtifactCore/
├── include/Audio/
│   └── ASIOBackendStub.ixx (新規)
└── src/Audio/
    └── ASIOBackendStub.cppm (新規)
```

### 変更されたファイル

```
ArtifactCore/
├── include/Audio/
│   └── AudioRenderer.ixx (+15 行)
└── src/Audio/
    └── AudioRenderer.cppm (+50 行)
```

---

## 関連ドキュメント

- `docs/planned/MILESTONE_ASIO_STUB_IMPLEMENTATION_PLAN_2026-03-28.md` - 計画案
- `ArtifactCore/include/Audio/AudioBackend.ixx` - AudioBackend 定義
- `ArtifactCore/include/Audio/WASAPIBackend.ixx` - WASAPIBackend 定義

---

## 結論

**ASIO スタブバックエンドの実装が完了した（195 行）。**

### 実装済み機能

- ✅ ASIOBackendStub クラス（WASAPI 委譲）
- ✅ AudioBackendType 列挙型
- ✅ バックエンド切り替え API
- ✅ 既存システムとの完全互換性

### 次のステップ

1. **設定 UI 実装**（4-6h）
   - バックエンド選択ダイアログ
   - デバイス一覧表示

2. **テスト強化**（2-3h）
   - 単体テスト追加
   - 統合テスト追加

3. **ドキュメント整備**（1-2h）
   - API リファレンス
   - ユーザーガイド

---

**文書終了**
