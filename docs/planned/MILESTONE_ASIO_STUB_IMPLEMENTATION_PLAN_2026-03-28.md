# ASIO スタブ実装 計画案

**作成日:** 2026-03-28  
**ステータス:** 計画案  
**関連コンポーネント:** AudioBackend, WASAPIBackend, AudioRenderer

---

## 概要

既存の WASAPI 共有デバイスバックエンドと同様のインターフェースを持つ **ASIO スタブバックエンド** を実装する。

**目標:**
1. ASIO SDK を使わないスタブ実装
2. WASAPI と同じ AudioBackend インターフェース
3. 将来的な ASIO 実装への移行パスを確保

---

## 現状のアーキテクチャ

### 既存のバックエンド構造

```
AudioBackend (抽象基底クラス)
├── WASAPIBackend (共有デバイス)
└── QtAudioBackend (Qt 標準)
```

### 追加予定

```
AudioBackend (抽象基底クラス)
├── WASAPIBackend (共有デバイス)
├── QtAudioBackend (Qt 標準)
└── ASIOBackendStub (ASIO スタブ) ← 新規
```

---

## 実装方針

### 方針 1: 完全なスタブ（推奨）

**概要:** ASIO SDK を使わず、WASAPI のラッパーとして実装

**メリット:**
- ✅ ASIO SDK ライセンス不要
- ✅ 依存関係なし
- ✅ 即時実装可能

**デメリット:**
- ❌ 真の ASIO 低遅延ではない
- ❌ ASIO 専用機能は使用不可

**実装:**
```cpp
class ASIOBackendStub : public AudioBackend {
public:
    // WASAPI を内部で使用
    std::unique_ptr<WASAPIBackend> wasapi_;
    
    bool open(const QAudioDevice& device, const QAudioFormat& format) override {
        // WASAPI に委譲
        return wasapi_->open(device, format);
    }
    
    void start(AudioCallback callback) override {
        // WASAPI に委譲
        wasapi_->start(callback);
    }
    
    QString backendName() const override {
        return QStringLiteral("ASIO(stub)");
    }
};
```

---

### 方針 2: ASIO SDK 使用（将来）

**概要:** Steinberg ASIO SDK を使用した完全実装

**メリット:**
- ✅ 真の ASIO 低遅延
- ✅ ASIO 専用機能使用可能

**デメリット:**
- ❌ ASIO SDK ライセンス制約
- ❌ Windows のみ対応
- ❌ 実装コスト高

**実装時期:** 要件定義後に検討

---

## 実装計画（スタブ版）

### 段階 1: インターフェース定義（2-3h）

**ファイル:**
- `ArtifactCore/include/Audio/ASIOBackendStub.ixx`（新規）

**実装:**
```cpp
module;
#include <QtMultimedia/QAudioDevice>
#include <memory>

export module Audio.Backend.ASIOStub;

import Audio.Backend;

export namespace ArtifactCore {

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

} // namespace ArtifactCore
```

---

### 段階 2: スタブ実装（4-6h）

**ファイル:**
- `ArtifactCore/src/Audio/ASIOBackendStub.cppm`（新規）

**実装:**
```cpp
module;
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioFormat>
#include <QDebug>

module Audio.Backend.ASIOStub;

import Audio.Backend;
import Audio.Backend.WASAPI;

namespace ArtifactCore {

class ASIOBackendStub::Impl {
public:
    std::unique_ptr<WASAPIBackend> wasapi_;
    bool isOpen_ = false;
};

ASIOBackendStub::ASIOBackendStub() 
    : impl_(std::make_unique<Impl>()) 
{
    impl_->wasapi_ = std::make_unique<WASAPIBackend>();
}

ASIOBackendStub::~ASIOBackendStub() {
    close();
}

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

void ASIOBackendStub::close() {
    impl_->wasapi_->close();
    impl_->isOpen_ = false;
}

void ASIOBackendStub::start(AudioCallback callback) {
    if (!impl_->isOpen_) {
        qWarning() << "[ASIOBackendStub] start() called but not open";
        return;
    }
    impl_->wasapi_->start(callback);
}

void ASIOBackendStub::stop() {
    impl_->wasapi_->stop();
}

bool ASIOBackendStub::isActive() const {
    return impl_->wasapi_->isActive();
}

QAudioFormat ASIOBackendStub::currentFormat() const {
    return impl_->wasapi_->currentFormat();
}

QString ASIOBackendStub::backendName() const {
    return QStringLiteral("ASIO(stub)");
}

} // namespace ArtifactCore
```

---

### 段階 3: AudioRenderer 統合（2-3h）

**ファイル:**
- `ArtifactCore/include/Audio/AudioRenderer.ixx`（修正）
- `ArtifactCore/src/Audio/AudioRenderer.cppm`（修正）

**変更:**
```cpp
// AudioRenderer.ixx
enum class AudioBackendType {
    Auto,
    WASAPI,
    ASIO,      // ← 追加
    Qt
};

// AudioRenderer.cppm
std::unique_ptr<AudioBackend> createBackend(AudioBackendType type) {
    switch (type) {
        case AudioBackendType::WASAPI:
            return std::make_unique<WASAPIBackend>();
        
        case AudioBackendType::ASIO:      // ← 追加
            return std::make_unique<ASIOBackendStub>();
        
        case AudioBackendType::Qt:
            return std::make_unique<QtAudioBackend>();
        
        case AudioBackendType::Auto:
        default:
            // 自動検出
            #ifdef Q_OS_WIN
                return std::make_unique<WASAPIBackend>();
            #else
                return std::make_unique<QtAudioBackend>();
            #endif
    }
}
```

---

### 段階 4: 設定 UI 追加（4-6h）

**ファイル:**
- `Artifact/include/Widgets/Dialog/ArtifactAudioSettingsDialog.ixx`（新規）
- `Artifact/src/Widgets/Dialog/ArtifactAudioSettingsDialog.cppm`（新規）

**実装:**
```cpp
class ArtifactAudioSettingsDialog : public QDialog {
    Q_OBJECT
    
    QComboBox* backendCombo_;
    QComboBox* deviceCombo_;
    QComboBox* sampleRateCombo_;
    QComboBox* bufferSizeCombo_;
    
public:
    explicit ArtifactAudioSettingsDialog(QWidget* parent = nullptr);
    
    AudioBackendType selectedBackend() const;
    QAudioDevice selectedDevice() const;
    int selectedSampleRate() const;
    int selectedBufferSize() const;
    
private slots:
    void onBackendChanged(int index);
    void updateDeviceList();
};
```

**UI 要素:**
```
┌─────────────────────────────────┐
│ Audio Settings                  │
├─────────────────────────────────┤
│ Backend:    [ASIO(stub)    ▼]  │
│ Device:     [Default Output  ▼]│
│ Sample Rate:[48000 Hz       ▼] │
│ Buffer Size:[256 samples    ▼] │
├─────────────────────────────────┤
│          [OK]  [Cancel]         │
└─────────────────────────────────┘
```

---

## ファイル構成

### 新規ファイル（4 つ）

| ファイル | 行数 | 内容 |
|---------|------|------|
| `ArtifactCore/include/Audio/ASIOBackendStub.ixx` | 40 | ASIO スタブ宣言 |
| `ArtifactCore/src/Audio/ASIOBackendStub.cppm` | 120 | ASIO スタブ実装 |
| `Artifact/include/Widgets/Dialog/ArtifactAudioSettingsDialog.ixx` | 60 | 設定ダイアログ宣言 |
| `Artifact/src/Widgets/Dialog/ArtifactAudioSettingsDialog.cppm` | 200 | 設定ダイアログ実装 |

### 修正ファイル（2 つ）

| ファイル | 変更行数 | 内容 |
|---------|---------|------|
| `ArtifactCore/include/Audio/AudioRenderer.ixx` | +10 | BackendType 追加 |
| `ArtifactCore/src/Audio/AudioRenderer.cppm` | +20 | バックエンド作成ロジック |

**合計:** 450 行（新規 420 行、修正 30 行）

---

## 実装順序

### 第 1 週：基盤実装（8-12h）

1. **ASIOBackendStub 実装**（6-8h）
   - インターフェース定義
   - WASAPI 委譲実装
   - テスト

2. **AudioRenderer 統合**（2-4h）
   - BackendType 追加
   - createBackend() 修正

---

### 第 2 週：UI 実装（10-14h）

3. **設定ダイアログ実装**（6-8h）
   - UI デザイン
   - バックエンド切り替え
   - デバイス一覧表示

4. **設定保存・読み込み**（4-6h）
   - QSettings 統合
   - 起動時設定復元

---

## 使用例

### 基本的な使い方

```cpp
// ASIO バックエンドを作成
auto backend = std::make_unique<ASIOBackendStub>();

// デバイスとフォーマットを指定
QAudioDevice device = QMediaDevices::defaultAudioOutput();
QAudioFormat format;
format.setSampleRate(48000);
format.setChannelCount(2);

// 開く
if (!backend->open(device, format)) {
    qWarning() << "Failed to open ASIO backend";
    return;
}

// コールバックを登録して開始
backend->start([](float* buffer, int frames, int channels) {
    // オーディオ処理
    for (int i = 0; i < frames * channels; ++i) {
        buffer[i] = 0.0f;  // 無音
    }
});

// 停止
backend->stop();

// 閉じる
backend->close();
```

---

### 設定から使用

```cpp
// 設定ダイアログ
ArtifactAudioSettingsDialog dialog;
if (dialog.exec() == QDialog::Accepted) {
    // 設定を取得
    AudioBackendType type = dialog.selectedBackend();
    QAudioDevice device = dialog.selectedDevice();
    int sampleRate = dialog.selectedSampleRate();
    
    // バックエンドを作成
    auto backend = createBackend(type);
    backend->open(device, format);
    backend->start(callback);
}
```

---

## 制限事項（スタブ版）

### 実装されない機能

1. **真の ASIO 低遅延**
   - WASAPI 共有モードを使用
   - 遅延は WASAPI と同等

2. **ASIO 専用機能**
   - ASIO Direct Monitor
   - ASIO Control Panel
   - ASIO Sample Rate Conversion

3. **複数 ASIO デバイス**
   - 1 デバイスのみ対応

---

### 将来の拡張

```cpp
// 完全な ASIO 実装への移行パス
class ASIOBackend : public AudioBackend {
    // 段階 1: スタブ（WASAPI 委譲）
    // 段階 2: ASIO SDK 使用
    // 段階 3: 完全な ASIO 機能
};
```

---

## テスト項目

### 単体テスト

- [ ] `ASIOBackendStub::open()` が成功する
- [ ] `ASIOBackendStub::close()` が正常に動作する
- [ ] `ASIOBackendStub::start()` でコールバックが登録される
- [ ] `ASIOBackendStub::stop()` で再生が停止する
- [ ] `ASIOBackendStub::isActive()` が正しい状態を返す
- [ ] `ASIOBackendStub::backendName()` が "ASIO(stub)" を返す

---

### 統合テスト

- [ ] AudioRenderer で ASIO バックエンドを選択可能
- [ ] 設定ダイアログでバックエンドを切り替え可能
- [ ] 設定が保存・読み込み可能
- [ ] 起動時に設定が復元される

---

### 手動テスト

1. **バックエンド切り替え**
   - [ ] WASAPI → ASIO 切り替え
   - [ ] ASIO → Qt 切り替え
   - [ ] 切り替え後に音声が出力される

2. **デバイス選択**
   - [ ] デフォルトデバイス選択
   - [ ] 複数デバイスがある場合の一覧表示

3. **サンプルレート・バッファサイズ**
   - [ ] 44.1kHz / 48kHz / 96kHz 選択
   - [ ] 128 / 256 / 512 / 1024 samples 選択

---

## 関連ドキュメント

- `ArtifactCore/include/Audio/AudioBackend.ixx` - AudioBackend 定義
- `ArtifactCore/include/Audio/WASAPIBackend.ixx` - WASAPIBackend 定義
- `ArtifactCore/src/Audio/AudioRenderer.cppm` - AudioRenderer 実装

---

## 結論

**ASIO スタブバックエンドは 20-30 時間で実装可能。**

### 推奨アプローチ

1. **段階 1: 完全なスタブ**（8-12h）
   - WASAPI 委譲
   - 最低限の機能

2. **段階 2: UI 統合**（10-14h）
   - 設定ダイアログ
   - バックエンド切り替え

3. **段階 3: 拡張**（将来）
   - ASIO SDK 使用
   - 完全な低遅延実装

---

**文書終了**
