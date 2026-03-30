# コア機能 不足分析 Vol.3 - インフラ・テスト・ドキュメント

**作成日:** 2026-03-28  
**ステータス:** 追加分析完了  
**対象:** ビルド・テスト・ドキュメント・国際化

---

## 概要

Vol.1/Vol.2 で見逃された**インフラ・テスト・ドキュメント系**の不足機能を分析。

---

## 🔴 重要：ビルド・インフラ系の不足

### 1. CMake モジュール定義の断片化

**現状:**
```
CMakeLists.txt (メイン)
├── Artifact/CMakeLists.txt
├── ArtifactCore/CMakeLists.txt
├── ArtifactWidgets/CMakeLists.txt
└── cmake/ (共通マクロ)
```

**問題点:**
- モジュール定義が各ディレクトリに散在
- `import std` 設定が複雑（clang-cl/IntelLLVM 対応）
- vcpkg 依存の自動解決が不十分

**不足機能:**
- ❌ 統合ビルド設定プリセット
- ❌ モジュール依存関係の可視化
- ❌ ビルド時間分析ツール

**工数:** 8-12h  
**優先度:** 🟡中

---

### 2. デバッグビルドの最適化不足

**現状:**
```
CMakePresets.json
├── x64-debug
├── x64-release
├── x86-debug
└── x86-release
```

**不足:**
- ❌ RelWithDebInfo プリセット
- ❌ MinSizeRel プリセット
- ❌ Sanitizer ビルド（ASan/UBSan/TSan）
- ❌ プロファイリングビルド（PGO）

**影響:**
- リリースビルドでのデバッグが困難
- メモリリーク検出が手動
- パフォーマンス解析が手動

**工数:** 4-6h  
**優先度:** 🟡中

---

### 3. 依存関係の自動管理不足

**現状:**
```
vcpkg.json (依存定義)
├── opencv
├── qt6
├── diligent-core
└── ... (多数)
```

**不足:**
- ❌ 依存関係のロックファイル（vcpkg-configuration.json の完全定義）
- ❌ オプション依存の明確化
- ❌ ビルドキャッシュの共有設定

**工数:** 4-6h  
**優先度:** 🟢低

---

## 🟡 重要：テスト・QA 系の不足

### 4. 単体テストフレームワークの不在

**現状:**
- ❌ GoogleTest / Catch2 / doctest の導入なし
- ❌ 自動テストスイートなし
- ❌ CI 統合なし

**不足:**
```
tests/
├── UnitTests/          ← 新規作成
│   ├── Audio/
│   ├── Render/
│   └── Layer/
├── IntegrationTests/   ← 新規作成
└── CMakeLists.txt      ← 新規作成
```

**工数:** 20-30h（フレームワーク導入 + 初期テスト）  
**優先度:** 🔴高

---

### 5. 回帰テストの不在

**現状:**
- ❌ 視覚的回帰テストなし
- ❌ スクリーンショット比較なし
- ❌ パフォーマンス回帰テストなし

**不足:**
```
tests/Regression/
├── Visual/             ← 新規作成
│   ├── baseline/       ← 基準画像
│   └── current/        ← 現在画像
└── Performance/        ← 新規作成
    └── benchmarks/     ← ベンチマーク
```

**工数:** 16-24h  
**優先度:** 🟡中

---

### 6. CI/CD パイプラインの不足

**現状:**
- ✅ GitHub Actions 設定あり（`.github/workflows/`）
- ⚠️ ビルドのみでテストなし
- ⚠️ 品質ゲートなし

**不足:**
- ❌ 自動テスト実行
- ❌ コードカバレッジ収集
- ❌ 静的解析（clang-tidy/cppcheck）
- ❌ フォーマットチェック（clang-format）
- ❌ ビルド成果物の自動リリース

**工数:** 12-16h  
**優先度:** 🔴高

---

### 7. コードカバレッジ計測の不在

**現状:**
- ❌ gcov/llvm-cov 設定なし
- ❌ カバレッジレポート生成なし
- ❌ カバレッジゲートなし

**不足:**
```cmake
# CMakeLists.txt に追加
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE)
    target_compile_options(... -fprofile-arcs -ftest-coverage)
    target_link_options(... -fprofile-arcs)
endif()
```

**工数:** 4-6h  
**優先度:** 🟡中

---

## 🟢 重要：ドキュメント・国際化系の不足

### 8. API リファレンスの不在

**現状:**
- ❌ Doxygen/Sphinx 設定なし
- ❌ 自動生成ドキュメントなし
- ❌ オンラインリファレンスなし

**不足:**
```
docs/api/               ← 新規作成
├── Doxyfile            ← 新規作成
├── index.md            ← 新規作成
└── generated/          ← 自動生成
```

**工数:** 8-12h  
**優先度:** 🟡中

---

### 9. ユーザーガイドの不足

**現状:**
- ✅ 技術ドキュメントは充実（`docs/technical/`）
- ⚠️ 初心者向けガイド不足
- ❌ チュートリアル不足

**不足:**
```
docs/user/              ← 新規作成
├── getting-started/    ← 新規作成
├── tutorials/          ← 新規作成
└── faq/                ← 新規作成
```

**工数:** 16-24h  
**優先度:** 🟡中

---

### 10. 国際化（i18n）の不足

**現状:**
- ✅ 翻訳ファイルあり（`Artifact/translations/`）
  - `ja.json`（日本語）
  - `en.json`（英語）
- ⚠️ 翻訳システムが不完全

**不足:**
- ❌ 動的言語切り替え
- ❌ 翻訳不足の検出
- ❌ 翻訳メモリ統合
- ❌ RTL（右から左）言語対応

**工数:** 12-16h  
**優先度:** 🟢低

---

### 11. アクセシビリティ対応の不足

**現状:**
- ❌ スクリーンリーダー対応なし
- ❌ キーボードナビゲーション不完全
- ❌ ハイコントラストモードなし
- ❌ フォントサイズ調整なし

**不足:**
```cpp
// アクセシビリティ設定
struct AccessibilitySettings {
    bool highContrast = false;
    float fontSizeScale = 1.0f;
    bool screenReaderMode = false;
    bool keyboardNavOnly = false;
};
```

**工数:** 20-30h  
**優先度:** 🟡中

---

## 🔵 重要：プラグイン・拡張系の不足

### 12. プラグインシステムの骨組みのみ

**現状:**
- ✅ プラグインインターフェース定義あり
- ❌ プラグインローダー未実装
- ❌ プラグインマネージャー未実装
- ❌ サンドボックスなし

**不足:**
```
ArtifactCore/include/Plugin/
├── IPlugin.ixx         ← 新規作成
├── PluginLoader.ixx    ← 新規作成
├── PluginManager.ixx   ← 新規作成
└── PluginSandbox.ixx   ← 新規作成
```

**工数:** 24-32h  
**優先度:** 🟢低

---

### 13. スクリプティングシステムの不足

**現状:**
- ❌ Python 統合なし
- ❌ JavaScript 統合なし
- ❌ マクロシステムなし

**不足:**
```
ArtifactCore/include/Script/
├── IScriptEngine.ixx   ← 新規作成
├── PythonEngine.ixx    ← 新規作成（pybind11）
├── JSEngine.ixx        ← 新規作成（V8/QuickJS）
└── MacroRecorder.ixx   ← 新規作成
```

**工数:** 32-48h  
**優先度:** 🟢低

---

### 14. 自動化ツールの不足

**現状:**
- ❌ コマンドラインインターフェース（CLI）なし
- ❌ バッチ処理ツールなし
- ❌ ヘッドレスレンダリングなし

**不足:**
```
tools/
├── artifact-cli/       ← 新規作成
├── artifact-render/    ← 新規作成（ヘッドレス）
└── artifact-batch/     ← 新規作成
```

**工数:** 16-24h  
**優先度:** 🟡中

---

## 📊 更新された工数サマリー

### 新規発見（14 機能）

| カテゴリ | 機能数 | 総工数 |
|---------|--------|--------|
| **ビルド・インフラ** | 3 機能 | 16-24h |
| **テスト・QA** | 4 機能 | 52-76h |
| **ドキュメント・i18n** | 4 機能 | 56-82h |
| **プラグイン・拡張** | 3 機能 | 72-104h |

**小計:** 196-286h

---

### 累計（Vol.1 + Vol.2 + Vol.3）

| カテゴリ | 機能数 | 総工数 |
|---------|--------|--------|
| **コア機能** | 11 機能 | 100-150h |
| **ビルド・インフラ** | 3 機能 | 16-24h |
| **テスト・QA** | 4 機能 | 52-76h |
| **ドキュメント・i18n** | 4 機能 | 56-82h |
| **プラグイン・拡張** | 3 機能 | 72-104h |

**合計:** 296-436h（約 4-6 ヶ月）

---

## 推奨実装順序（更新）

### 第 1 段階：コア安定化（48-70h）

1. **Undo/Redo 統合**（20-30h）
2. **Audio Pipeline 完成**（24-36h）
3. **AdjustableLayer**（2-4h）

---

### 第 2 段階：テスト基盤（32-46h）

4. **単体テストフレームワーク**（20-30h）
5. **CI/CD パイプライン**（12-16h）

---

### 第 3 段階：コア機能補完（32-48h）

6. **VideoLayer Proxy**（16-24h）
7. **プロジェクト管理**（4-6h）
8. **WebUI ブリッジ**（8-12h）
9. **ROI Scissor**（4-6h）

---

### 第 4 段階：ドキュメント・国際化（24-36h）

10. **API リファレンス**（8-12h）
11. **ユーザーガイド**（16-24h）

---

### 第 5 段階：新機能（88-128h）

12. **Cache システム**（16-20h）
13. **Reactive システム**（24-32h）
14. **Motion Tracking**（32-48h）
15. **アクセシビリティ**（20-30h）

---

## 特別注目：テスト・QA 基盤

### なぜテストが重要か

**現状の問題:**
- 手動テストのみ
- 回帰バグの発見が遅い
- リファクタリングの安心感不足

**テスト導入の効果:**
- ✅ バグの早期発見
- ✅ リファクタリングの安心感
- ✅ 仕様の明確化（テストがドキュメント）
- ✅ CI による品質ゲート

---

### 最小のテストセット（推奨）

```cpp
// tests/UnitTests/Audio/test_audiorenderer.cpp
#include <gtest/gtest.h>
#include <Audio/AudioRenderer.ixx>

using namespace ArtifactCore;

TEST(AudioRendererTest, OpenCloseDevice) {
    AudioRenderer renderer;
    EXPECT_TRUE(renderer.openDevice());
    EXPECT_TRUE(renderer.isDeviceOpen());
    renderer.closeDevice();
    EXPECT_FALSE(renderer.isDeviceOpen());
}

TEST(AudioRendererTest, StartStop) {
    AudioRenderer renderer;
    renderer.openDevice();
    renderer.start();
    EXPECT_TRUE(renderer.isActive());
    renderer.stop();
    EXPECT_FALSE(renderer.isActive());
}
```

**工数:** 20-30h（フレームワーク導入 + 10-20 テスト）

---

## 関連ドキュメント

- `docs/technical/CORE_FEATURE_GAP_ANALYSIS_2026-03-28.md` - Vol.1
- `docs/technical/CORE_FEATURE_GAP_ANALYSIS_VOL2_2026-03-28.md` - Vol.2
- `docs/planned/MILESTONE_TEST_QA_INFRASTRUCTURE_2026-03-28.md` - テスト基盤マイルストーン
- `docs/planned/MILESTONE_SECURITY_HARDENING_2026-03-28.md` - セキュリティ強化

---

## 結論

**さらに 14 の不足機能を発見。**

特に優先度が高いのは：

1. **単体テストフレームワーク**（20-30h）← 品質基盤
2. **CI/CD パイプライン**（12-16h）← 自動化
3. **アクセシビリティ**（20-30h）← 包括性

これらは「製品としての信頼性」に直結するため、機能追加と並行して実装すべき。

---

**文書終了**
