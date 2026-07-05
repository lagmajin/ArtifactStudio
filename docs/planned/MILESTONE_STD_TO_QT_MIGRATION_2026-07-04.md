# マイルストーン: std → Qt ライブラリ置換計画 (2026-07-04)

> 2,197 ファイル / 551,516 行をスキャン。std 使用状況と Qt 代替案。

## 現状スキャン結果

| std | 使用回数 | Qt 相当 | 使用回数 | 判定 |
|---|---|---|---|---|
| `std::vector` | 3,069 | `QVector` | 1,360 | 🔴 混在 |
| `std::string` | 1,548 | `QString` | 14,383 | 🟡 QString 優勢 |
| `std::shared_ptr` | 741 | `QSharedPointer` | 0 | 🟡 Qt 未使用 |
| `std::make_unique` | 553 | — | — | 🟢 C++14 標準、問題少 |
| `std::mutex` | 426 | `QMutex` | 33 | 🔴 std 過剰 |
| `std::function` | 406 | — | — | 🟢 Qt に完全代替なし |
| `std::unique_ptr` | 391 | `QScopedPointer` | 9 | 🟡 Qt ほぼ未使用 |
| `std::lock_guard` | 382 | `QMutexLocker` | 150 | 🔴 混在 |
| `std::chrono` | 290 | `QElapsedTimer` | — | 🟡 用途次第 |
| `std::array` | 230 | — | — | 🟢 C++14 標準 |
| `std::optional` | 171 | — | — | 🟢 C++17 標準 |
| `std::unordered_map` | 148 | `QHash` | 184 | 🟡 混在 |
| `std::atomic` | 126 | `QAtomicInt` | 1 | 🟡 Qt ほぼ未使用 |
| `std::map` | 93 | `QMap` | 89 | 🟡 混在 |
| `std::filesystem` | 49 | `QDir/QFileInfo` | — | 🟡 |
| `std::thread` | 43 | `QThread` | 51 | 🟡 混在 |

## 🔴 優先度 P0: エラー削減効果の高いもの

### 1. `std::vector` → `QVector` (3,069 箇所)
**問題**: 全 `.cppm` ファイルが `#include <vector>` している。ヘッダ依存が巨大で再ビルド要因。
**効果**: 
- `#include <vector>` 削除 → プリプロセス時間短縮
- `QVector` は暗黙的共有 (COW) で戻り値のコピーコストゼロ
- Qt foreach / range-for 互換
**リスク**: `std::vector` の `data()` を生ポインタアクセスに使っている場合は `QVector::data()` で代替可

### 2. `std::mutex` + `std::lock_guard` → `QMutex` + `QMutexLocker` (808 箇所)
**問題**: 手動 `lock()`/`unlock()` のミスマッチ、例外安全性の欠如
**効果**:
- `QMutexLocker` は RAII 保証（スコープ抜けで自動 unlock）
- `QMutex::tryLock()` でデッドロック回避が容易
- デバッグビルドで mutex 所有チェック付き
**リスク**: `std::condition_variable` と組み合わせている場合は `QWaitCondition` に変更必要

### 3. `std::string` → `QString` (1,548 箇所のうち未変換分)
**問題**: UTF-8/UTF-16 変換が暗黙的に発生しエンコーディングバグの温床
**効果**: 全 Qt API とシームレス、暗黙的変換がなくなる
**リスク**: C API / template 引数に使っている箇所は要チェック

## 🟡 優先度 P1: ビルド時間・安全性向上

### 4. `std::shared_ptr` → `QSharedPointer` / `QWeakPointer` (741 箇所)
**問題**: C++20 modules 環境で `std::shared_ptr` のテンプレートインスタンス化が重い
**効果**: `QSharedPointer` はスレッドセーフな参照カウント、Qt オブジェクトと自然に統合
**リスク**: カスタムデリーターを使っている箇所は移行困難

### 5. `std::atomic` → `QAtomicInt` / `QAtomicPointer` (126 箇所)
**問題**: `std::atomic` の memory_order 指定ミスで未定義動作
**効果**: `QAtomicInt` は fetchAndAdd/compareExchange で安全な API
**リスク**: 複雑な CAS ループは要レビュー

### 6. `std::unordered_map` / `std::map` → `QHash` / `QMap` (241 箇所)
**問題**: 混在による型不一致
**効果**: Qt のイテレータパターン統一
**リスク**: `QHash` は `std::unordered_map` と内部実装が異なる（チェイニング vs オープンアドレス）

## 🔵 優先度 P2: 長期品質

| std | Qt 代替 | 効果 |
|---|---|---|
| `std::filesystem` (49) | `QDir/QFileInfo` | パス区切り文字問題解消 |
| `std::thread` (43) | `QThread` | Qt event loop 統合 |
| `std::chrono` (290) | `QElapsedTimer` | プラットフォーム非依存 |

## ⚠️ 置換すべきでないもの（C++ 標準が優位）

| std | 理由 |
|---|---|
| `std::unique_ptr` (391) | C++14 の `make_unique` は Qt に相当品なし。`QScopedPointer` は機能不足 |
| `std::function` (406) | Qt に完全代替なし。signal/slot 非推奨ルールと矛盾するが残す |
| `std::optional` (171) | C++17 標準。Qt に相当品なし |
| `std::variant` (13) | Qt に相当品なし |
| `std::array` (230) | C++14 標準。コンパイル時サイズ。QVector は非効率 |
| `std::string_view` (38) | C++17。QStringView はあるがパフォーマンス次第 |

## 段階的実行計画

| Phase | 内容 | 対象箇所 | 工数見積 | エラー削減効果 |
|---|---|---|---|---|
| **P0-1** | `std::vector` → `QVector` | 3,069 | 大（自動化可能） | ★★★ |
| **P0-2** | `std::mutex/lock_guard` → `QMutex/QMutexLocker` | 808 | 中 | ★★★ |
| **P0-3** | `std::string` → `QString` (残存分) | ~500 | 中 | ★★ |
| **P1-1** | `std::atomic` → `QAtomicInt` | 126 | 小 | ★★ |
| **P1-2** | `std::unordered_map/map` → `QHash/QMap` | 241 | 中 | ★ |
| **P2** | 長期改善 (filesystem/thread/chrono) | 382 | 大 | ★ |