# テスト・QA 基盤 Milestone

**作成日:** 2026-03-28  
**ステータス:** 計画中  
**関連コンポーネント:** 全コンポーネント

---

## 概要

自動テスト基盤と品質保証プロセスを整備し、リリースの信頼性を向上させる。

---

## 発見された問題点

### ★★★ 問題 1: 単体テストの不足

**現状:**
- テストカバレッジ < 20%
- 手動テストに依存
- 回帰テストがない

**影響:**
- バグの早期発見が困難
- リリース前のテストに時間
- 機能追加で既存機能が壊れる

**工数:** 20-30 時間

---

### ★★★ 問題 2: UI テストの未整備

**現状:**
- UI 操作の自動テストがない
- 手動での確認のみ
- 再現テストが困難

**影響:**
- UI のバグが見逃される
- 修正の検証に時間
- ユーザー体験の低下

**工数:** 16-20 時間

---

### ★★ 問題 3: パフォーマンステストの不足

**現状:**
- 性能基準が未定義
- 負荷テストがない
- メモリリークの検出が手動

**影響:**
- 性能劣化に気づかない
- 大規模プロジェクトで問題
- ユーザー不満

**工数:** 12-16 時間

---

### ★★ 問題 4: 継続的インテグレーション

**現状:**
- 手動ビルド
- 自動デプロイなし
- テスト結果の可視化不足

**影響:**
- リリースに時間
- 人的ミスのリスク
- 品質のばらつき

**工数:** 12-16 時間

---

### ★ 問題 5: ドキュメント不足

**現状:**
- API ドキュメントが不足
- 開発者向けガイドなし
- 変更履歴が不明確

**影響:**
- 新規参入が困難
- 知識の属人化
- 重複実装

**工数:** 16-20 時間

---

## 優先度別実装計画

### P0（必須）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **単体テスト基盤** | 20-30h | 🔴 高 |
| **CI パイプライン** | 12-16h | 🔴 高 |

### P1（重要）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **UI テスト** | 16-20h | 🟡 中 |
| **パフォーマンステスト** | 12-16h | 🟡 中 |

### P2（推奨）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **ドキュメント整備** | 16-20h | 🟢 低 |

**合計工数:** 76-102 時間

---

## Phase 構成

### Phase 1: 単体テスト基盤

- 目的:
  - 自動テストの実行環境を構築

- 作業項目:
  - Google Test / Catch2 の導入
  - テストフィクスチャの整備
  - モック/スタブの作成

- 完了条件:
  - 主要コンポーネントのテストカバレッジ 60% 以上
  - コマンド一発でテスト実行

- 実装案:
  ```cpp
  // テスト例
  TEST_CASE("VideoLayer loads file", "[VideoLayer]") {
      auto layer = std::make_shared<ArtifactVideoLayer>();
      
      REQUIRE(layer->loadFromPath("test.mp4") == true);
      REQUIRE(layer->isLoaded() == true);
      REQUIRE(layer->sourcePath() == "test.mp4");
  }
  
  TEST_CASE("AudioRenderer plays segment", "[Audio]") {
      AudioRenderer renderer;
      REQUIRE(renderer.openDevice(""));
      
      AudioSegment segment;
      renderer.enqueue(segment);
      
      REQUIRE(renderer.isActive());
  }
  ```

### Phase 2: UI テスト

- 目的:
  - UI 操作の自動テスト

- 作業項目:
  - Qt Test の導入
  - 記録/再生機能
  - スクリーンショット比較

- 完了条件:
  - 主要な UI フローを自動テスト
  - 視覚的回帰の検出

- 実装案:
  ```cpp
  class UITest : public QObject {
      Q_OBJECT
      
  private slots:
      void testLayerCreation() {
          // アプリ起動
          auto* app = TestApplication::instance();
          app->show();
          
          // レイヤー追加
          QTest::mouseClick(app->addLayerButton(), Qt::LeftButton);
          
          // 検証
          QCOMPARE(app->layerCount(), 1);
          QCOMPARE(app->layerName(0), "New Layer");
      }
      
      void testScreenshotComparison() {
          // スクリーンショット撮影
          QPixmap expected = QPixmap("expected.png");
          QPixmap actual = QTest::grabAllWindows();
          
          // 比較（許容誤差 2%）
          QVERIFY(compareImages(expected, actual, 0.02));
      }
  };
  ```

### Phase 3: パフォーマンステスト

- 目的:
  - 性能基準の確立

- 作業項目:
  - ベンチマークフレームワーク
  - メモリプロファイリング
  - 負荷テスト

- 完了条件:
  - 60fps 維持の自動検証
  - メモリリークの検出

- 実装案:
  ```cpp
  BENCHMARK("Composition rendering 100 layers") {
      auto comp = createCompositionWithLayers(100);
      auto renderer = createRenderer();
      
      Chrono chrono;
      for (int i = 0; i < 60; ++i) {
          renderer->render(comp);
      }
      
      // 60fps 維持（1 フレーム 16.67ms 以下）
      REQUIRE(chrono.elapsed() / 60 < 16.67ms);
  }
  
  TEST_CASE("No memory leak in layer creation", "[Memory]") {
      // メモリ使用量測定
      auto before = getMemoryUsage();
      
      for (int i = 0; i < 1000; ++i) {
          auto layer = std::make_shared<ArtifactVideoLayer>();
          layer.reset();
      }
      
      auto after = getMemoryUsage();
      
      // メモリリークなし（許容誤差 1MB）
      REQUIRE(after - before < 1MB);
  }
  ```

### Phase 4: CI パイプライン

- 目的:
  - 自動ビルド・テスト

- 作業項目:
  - GitHub Actions / Jenkins 設定
  - テスト結果の可視化
  - 自動デプロイ

- 完了条件:
  - コミットごとに自動テスト
  - 失敗時に通知

- 実装案:
  ```yaml
  # .github/workflows/ci.yml
  name: CI
  
  on: [push, pull_request]
  
  jobs:
    build:
      runs-on: windows-latest
      
      steps:
      - uses: actions/checkout@v2
      
      - name: Configure CMake
        run: cmake -B build -S .
      
      - name: Build
        run: cmake --build build
      
      - name: Test
        run: ctest --test-dir build --output-on-failure
      
      - name: Upload coverage
        uses: codecov/codecov-action@v2
  ```

### Phase 5: ドキュメント整備

- 目的:
  - 開発者向けドキュメント

- 作業項目:
  - API ドキュメント（Doxygen）
  - 開発者ガイド
  - 変更履歴

- 完了条件:
  - 全公開 API にドキュメント
  - 新規開発者が 1 週間で参入可能

---

## 技術的課題

### 1. レガシーコードのテスト化

**課題:**
- テストのない既存コード

**解決案:**
- 段階的なリファクタリング
- 高優先度からテスト
- カバレッジ目標の設定

### 2. UI テストの安定性

**課題:**
- 環境による差異

**解決案:**
- 許容誤差の設定
- 安定なセレクタ
- 再試行機構

### 3. テスト実行時間

**課題:**
- 大規模テストスイート

**解決案:**
- 並列実行
- 増分テスト
- スマートテスト選択

---

## 期待される効果

### 品質向上

| 指標 | 現在 | 改善後 |
|------|------|--------|
| **テストカバレッジ** | <20% | >60% |
| **バグ発見時期** | リリース後 | 開発中 |
| **リリース時間** | 1 週間 | 1 日 |

### 開発効率

- 手動テスト時間の削減
- 回帰バグの防止
- 自信を持ったリファクタリング

---

## 関連ドキュメント

- `docs/planned/MILESTONES_BACKLOG.md` - 全体バックログ

---

## 実装順序の推奨

1. **Phase 1: 単体テスト基盤** - 基礎
2. **Phase 4: CI パイプライン** - 自動化
3. **Phase 2: UI テスト** - カバレッジ拡大
4. **Phase 3: パフォーマンステスト** - 品質向上
5. **Phase 5: ドキュメント** - 知識共有

---

**文書終了**
