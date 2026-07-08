# M-VST3-1 VST3 + CLAP Host Integration Milestone

**作成日:** 2026-07-03
**ステータス:** Draft
**ライセンス:** VST3 SDK (MIT), CLAP (MIT)

---

## 1. 目的

VST2 のスタブ実装を VST3 に置き換え、プロフェッショナルなプラグインホストを実現する。

---

## 2. VST3 アーキテクチャ

```
VST3 プラグイン (.vst3)
  └── module.so/.dll
        └── GetPluginFactory() → IPluginFactory
              ├── countPlugins()
              └── getPluginInfo()
              └── createInstance() → IPluginBase::Ptr
                    ├── IAudioProcessor (オーディオ処理)
                    │     ├── setBusArrangements()
                    │     ├── setupProcessing()
                    │     ├── process() ← メインオーディオ処理
                    │     └── getTailSamples()
                    │
                    ├── IEditController (パラメータ管理)
                    │     ├── getParameterCount()
                    │     ├── getParameterInfo()
                    │     ├── getParamStringByValue()
                    │     ├── getParamValueByString()
                    │     ├── setParamNormalized()
                    │     └── getParamNormalized()
                    │
                    └── IConnectionPoint (UI通信)
                          ├── connect()
                          ├── disconnect()
                          └── notify()
```

---

## 3. 実装フェーズ

### Phase 1: VST3 Interface Definitions (4h)
- VST3 SDK の基本インターフェース宣言 (FUID, IPluginBase, IAudioProcessor, IEditController)
- プラグインローダー (動的ライブラリ読み込み + GetPluginFactory)

### Phase 2: Audio Processing Pipeline (6h)
- IAudioProcessor の process() 呼び出し
- AudioSegment ↔ VST3 バッファ変換
- バス設定・サンプルレート設定

### Phase 3: Parameter Management (4h)
- IEditController のパラメータ一覧取得
- パラメータ値の読み書き
- パラメータUI (Inspector表示)

### Phase 4: Editor UI + Effect Chain (6h)
- VST3 エディタウィンドウ埋め込み
- エフェクトチェインへの統合 (AbstractEffect)
- Undo/Redo 対応

**合計工数:** ~20h

---

## 4. 既存コードとの関係

| 既存ファイル | 変更内容 |
|-------------|---------|
| `Artifact/src/VST/VSTHost.cpp` | VST2 → VST3 に全面書き換え |
| `Artifact/src/VST/VSTEffect.cpp` | VST3 プラグインラッパーに更新 |
| `Artifact/include/VST/VSTHost.ixx` | インターフェース更新 |
| `Artifact/include/Effects/AbstractEffect.ixx` | VST3 エフェクト追加（最小） |
| `CMakeLists.txt` | VST3 SDK インクルードパス追加 |

---

## 5. VST3 SDK インターフェース (骨格)

```cpp
// VST3 基本型
using TUID = char[16];  // FUID
using Steinberg::int16;
using Steinberg::int32;
using Steinberg::int64;
using Steinberg::uint32;

// 主要インターフェース
class IPluginBase : public FUnknown { /* initialize() / terminate() */ };
class IAudioProcessor : public FUnknown { /* process() / setupProcessing() */ };
class IEditController : public FUnknown { /* parameter handling */ };
class IConnectionPoint : public FUnknown { /* UI communication */ };
class IPluginFactory : public FUnknown { /* createInstance() */ };

// プラグインローダー
class VST3Loader {
    static IPluginFactory* loadModule(const char* path);
    static void unloadModule(IPluginFactory* factory);
};
```

---

## 6. ガードレール

- VST3 SDK ヘッダは `third_party/vst3sdk/` に配置（git submodule or copy）
- 新規 signal-slot 追加禁止（既存の Effect 経路を使用）
- QImage / QtCSS 禁止
- VST3 のGUIエディタは別ウィジェットとしてホスト


---

## 7. CLAP (CLever Audio Plugin) 対応

| 項目 | VST3 | CLAP |
|------|------|------|
| SDK | 要ダウンロード (Steinberg) | 不要 (`clap.h` 1ファイル) |
| API | COM-like C++ | 純粋C言語 |
| エントリポイント | `GetPluginFactory()` | `clap_entry` |
| モジュレーション | 複雑 | 標準搭載 |
| スレッド安全 | ホスト責任 | 設計段階で考慮済み |

### 既存コード

| ファイル | 内容 |
|---------|------|
| `ArtifactCore/include/CLAP/CLAPHost.ixx` | ✅ CLAP 型定義 + Host クラス |
| `ArtifactCore/src/CLAP/CLAPHost.cppm` | ✅ 動的ローダー + スキャナー |

### 共用できる基盤

| コンポーネント | VST3 | CLAP |
|--------------|:----:|:----:|
| `.dll`/`.so` ローダー | ✅ `VST3Loader` | ✅ `PluginLibrary` |
| エフェクトチェイン | AbstractEffect派生 | 共用 |
| パラメータUI | Inspector表示 | 共用 |
| 検索パス管理 | ✅ VSTHost | ✅ Host::scanPlugins |

### 実装Phase (CLAP追加分)

| Phase | 内容 | 工数 |
|:-----:|------|:----:|
| **C1** | `clap.h` 入手 + ホストコールバック実装 | 4h |
| **C2** | Plugin インスタンス生成 + process() | 6h |
| **C3** | パラメータ管理 + UI | 4h |
| **C4** | VST3 + CLAP 統合エフェクトブラウザ | 4h |

**CLAP 追加工数:** ~18h  
**VST3 + CLAP 合計:** ~38h

---

## Next Execution Slice

### Phase 1A の着手点

- VST3 側は `IPluginBase / IAudioProcessor / IEditController / IPluginFactory` の最小宣言と `VST3Loader` だけを先に固める
- `GetPluginFactory()` から `createInstance()` までの導線を、ローダー単体で追えるところまで整理する
- `AudioSegment` ↔ VST3 buffer 変換は Phase 2 へ送る前提で、Phase 1 では型の境界だけを明示する

### Phase 2A の着手点

- `IAudioProcessor::process()` の入出力と bus / sample rate 設定を、既存 Audio 系 API とどう接続するか確認する
- parameter / editor の責務を `IEditController` と inspector 側で分け、UI を先に膨らませない

### Phase 3 前提

- CLAP 側は `clap_entry` と `PluginLibrary` の共通ローダー整備が終わってから拡張する
- VST3 と CLAP の共用部は search path / effect chain / parameter UI に限定し、GUI ホストの責務は混ぜない
- 先に `VST3` の最小再生経路を通してから、統合ブラウザや editor 埋め込みへ進む
