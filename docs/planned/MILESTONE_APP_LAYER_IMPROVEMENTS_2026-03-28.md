# アプリ層改善 Milestone

**作成日:** 2026-03-28
**更新日:** 2026-07-30
**ステータス:** 一部実装済み ✅
**関連コンポーネント:** Artifact アプリケーション層（ArtifactCore ではない）

---

## 概要

アプリ層（`Artifact/src/*`）に存在する TODO・FIXME・未実装機能を整理し、優先度順に実装する。

---

## 実装済み機能 ✅

### ✅ Undo/Redo 統合 段階 1

**実装場所:** 
- `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm`
- `Artifact/include/Undo/UndoManager.ixx`
- `Artifact/src/Undo/UndoManager.cppm`

**実装内容:**
- Edit メニューの UI 状態同期
- 新規コマンドクラス（3 つ）:
  - `MoveLayerIndexCommand` - レイヤーインデックス移動
  - `RenameLayerCommand` - レイヤー名変更
  - `ChangeLayerOpacityCommand` - 不透明度変更

**効果:**
- ✅ レイヤー名変更が Undo 可能に
- ✅ インデックス移動が Undo 可能に
- ✅ 不透明度変更が Undo 可能に
- ✅ Undo 後の UI 同期が改善

**ステータス:** ✅ 段階 1 完了（135 行追加）

---

### ✅ ASIO スタブバックエンド

**実装場所:**
- `ArtifactCore/include/Audio/ASIOBackendStub.ixx`
- `ArtifactCore/src/Audio/ASIOBackendStub.cppm`
- `ArtifactCore/include/Audio/AudioRenderer.ixx`
- `ArtifactCore/src/Audio/AudioRenderer.cppm`

**実装内容:**
- `ASIOBackendStub` クラス（WASAPI 委譲）
- `AudioBackendType` 列挙型
- バックエンド切り替え API

**ステータス:** ✅ 実装完了（195 行追加）

---

## 発見された問題点（未実装）

### ✅ 問題 1: WebUI ブリッジの実装整理（静的実装済み）

**場所:** `Artifact/src/Widgets/WebUI/ArtifactWebBridge.cppm`

**旧 TODO 一覧（現行コードで解消済み）:**
```cpp
// line 75:
// TODO: construct LayerID from string

// line 93:
// TODO: Look up effect by ID from the current layer and call setPropertyValue()

// line 108:
// TODO: add composition count, layer count, etc.

// line 122:
// TODO: Get current selected layer and serialize its effects/properties to JSON
```

**現状:** `selectLayer()`, `setEffectProperty()`, `getProjectInfo()`, `getSelectedLayerProperties()` は実装済み。
LayerID の文字列化、選択レイヤー上の effect ID／表示名解決、composition／layer 統計、effect／layer property の JSON 化まで現行コードで確認できる。
runtime の WebUI 接続確認と仕様差分の将来拡張は別検証として残す。

**ステータス:** ✅ 静的実装済み（runtime 検証待ち）
**工数:** 1-2 時間

---

### ✅ 問題 2: VideoLayer::generateProxy() 実装済み（runtime 確認待ち）

**場所:** `Artifact/src/Layer/ArtifactVideoLayer.cppm:2210`

`ArtifactVideoLayer::generateProxy(ProxyQuality)` は `ArtifactProxyManager` へ委譲し、FFmpeg の scale／H.264／AAC 出力、品質別 proxy path、proxy quality 更新を行う。batch generation、`hasProxy()`、`getProxyInfo()`、`clearProxy()` も実装済み。

**ステータス:** ✅ 静的実装済み（FFmpeg 実行環境での生成・再生・失敗時 cleanup は未検証）

---

### ✅ 問題 3: プロジェクト管理（静的実装済み）

**場所:** `Artifact/src/Project/ArtifactProject.cppm`

**旧 TODO 一覧（現行コードで解消済み）:**
```cpp
// line 310:
// bool ArtifactProject::Impl::removeById() - TODO: container_.remove() not implemented

// line 353:
// TODO: ダーティ状態が変更されたときの通知を実装する

// line 925:
// void ArtifactProject::setDirty(bool dirty) - TODO: impl_->setDirty not available
```

**現状:** `removeById()` は composition container と project tree の双方から削除し、current composition の解除と dirty 化まで行う。`setDirty()` と公開 `isDirty()`／`setDirty()` も実装済みで、主要な mutation 経路から dirty 化される。

**ステータス:** ✅ 静的実装済み（保存・再読込・runtime 通知の検証待ち）  
**工数:** 4-6 時間

---

## 実装済みサマリー

| 機能 | ステータス | 工数 |
|------|-----------|------|
| **Undo/Redo 統合 段階 1** | ✅ 完了 | 20-30h（うち段階 1:10h） |
| **ASIO スタブバックエンド** | ✅ 完了 | 12-18h |
| **WebUI ブリッジ** | ✅ 静的実装済み・runtime 確認待ち | 1-2h |
| **VideoLayer Proxy** | ✅ 静的実装済み・runtime 確認待ち | — |
| **プロジェクト管理** | ✅ 静的実装済み・runtime 確認待ち | 4-6h |

**完了率:** 約 40%

---

### ✅ 問題 4: インスペクターの機能不足（静的実装済み）

**場所:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`

**旧 TODO／残件:**
```cpp
// line 478:
// TODO: 他のレイヤータイプも判定

// line 974:
// TODO: projectClosed シグナルがあれば接続
```

**現状:** `describeLayerPresentation()` と capability summary により、レイヤー実体に合わせたタイプ表示・マスク／matte／proxy／component 状態の表示を実装済み。既存 `ProjectChangedEvent` 受信時に `hasProject()` を再確認し、close/reset 後は stale state を `setNoProjectState()` へ戻す防御も実装済み。runtime の project 切替・選択解除確認は残る。

**工数:** 3-4 時間

---

### ✅ 問題 5: プロジェクトインポーター（静的実装済み）

**場所:** `Artifact/src/Project/ArtifactProjectImporter.cppm`

**現状:** JSON の open／parse エラー、必須フィールド検証、format version／minimum version、color pipeline version の互換性検査、非同期 import と validation diagnostics が実装済み。進行状況の実機表示と大規模ファイルの runtime 検証は残る。

**工数:** 4-6 時間

---

### ✅ 問題 6: プロジェクトパッケージャー（静的実装済み）

**場所:** `Artifact/src/Project/ArtifactProjectPackager.cppm`

**旧 TODO:**
```cpp
// line 64:
// TODO: プロジェクトファイルのパスを Assets/xxx に書き換えた
```

**現状:** `Assets/` への外部ファイル収集・重複回避・任意 hash rename・`filePath` の `Assets/<name>` への JSON 再書換え・`project.json` 出力まで実装済み。他環境での再読込と copy failure の runtime 検証は残る。

**工数:** 2-3 時間

---

### ✅ 問題 7: レイヤー追加コマンド（静的実装済み）

**場所:** `Artifact/src/Project/ArtifactProjectManager.cppm`

**関連:** `MILESTONE_APP_LAYER_COMPLETENESS.md`

**現状:** `AddLayerCommand` は top／bottom 追加と undo、`RemoveLayerCommand` は元 index 保存・削除・undo 復元を実装済み。`UndoManager` の command stack から利用可能で、label に対象 layer ID を含む。実際の UI 操作・複数選択・runtime undo/redo の検証は残る。

**工数:** 6-8 時間

---

## 優先度別実装計画

### P0（必須）

| 項目 | 工数 | 優先度 | 依存 |
|------|------|--------|------|
| **VideoLayer::generateProxy()** | ✅ 静的実装済み・runtime 確認待ち | 🔴 高 | なし |
| **プロジェクト管理 TODO** | ✅ 静的実装済み・runtime 確認待ち | 🔴 高 | なし |
| **レイヤー追加コマンド** | ✅ 静的実装済み・runtime 確認待ち | 🔴 高 | なし |

### P1（重要）

| 項目 | 工数 | 優先度 | 依存 |
|------|------|--------|------|
| **WebUI ブリッジ** | 4-6h | 🟡 中 | なし |
| **インスペクター改善** | ✅ 静的実装済み・lifecycle 確認待ち | 🟡 中 | なし |
| **プロジェクトインポーター** | ✅ 静的実装済み・runtime 確認待ち | 🟡 中 | なし |

### P2（推奨）

| 項目 | 工数 | 優先度 | 依存 |
|------|------|--------|------|
| **プロジェクトパッケージャー** | ✅ 静的実装済み・runtime 確認待ち | 🟢 低 | なし |

**合計工数:** 29-41 時間

---

## Phase 構成

### Phase 1: VideoLayer Proxy 機能（静的実装済み・runtime 確認待ち）

- 目的:
  - 高解像度動画の編集を軽量化

- 作業項目:
  - FFmpeg または OpenCV を使用したプロキシ生成
  - 低解像度（1/2, 1/4）のサムネイル作成
  - プロキシ切り替え機能
  - プロキシファイルの管理

- 完了条件:
  - 1920x1080 動画が 960x540 プロキシで編集可能
  - プロキシ/オリジナル切り替え可能
  - プロキシファイルはプロジェクトに紐づく

現行コードでは `ArtifactVideoLayer::generateProxy()` と `ArtifactProxyManager` の生成・管理経路まで実装済み。上記条件の FFmpeg 実行、再生切り替え、失敗時 cleanup は runtime 確認待ちとする。

- 実装案:
  ```cpp
  bool ArtifactVideoLayer::generateProxy(ProxyQuality quality) {
      if (quality == ProxyQuality::None) {
          clearProxy();
          return true;
      }
      
      // FFmpeg でプロキシ生成
      QString ffmpegPath = findFFmpeg();
      QStringList args;
      args << "-i" << impl_->sourcePath_;
      
      switch (quality) {
          case ProxyQuality::Low:
              args << "-vf" << "scale=iw/4:ih/4";
              break;
          case ProxyQuality::Medium:
              args << "-vf" << "scale=iw/2:ih/2";
              break;
          default:
              args << "-vf" << "scale=iw/2:ih/2";
      }
      
      args << "-c:v" << "libx264" << "-crf" << "23";
      args << impl_->proxyPath_;
      
      return QProcess::execute(ffmpegPath, args) == 0;
  }
  ```

### Phase 2: プロジェクト管理改善

- 目的:
  - コンポジション削除の完全実装
  - ダーティ状態の通知

- 作業項目:
  - `container_.remove()` の実装
  - ダーティ状態変更通知の追加
  - `setDirty()` の完全実装
  - 自動保存との連携

- 完了条件:
  - コンポジションが完全に削除される
  - 未保存時にインジケーター表示
  - 自動保存が正しく動作

### Phase 3: WebUI ブリッジ実装

- 目的:
  - WebUI からのフルコントロール

- 作業項目:
  - LayerID の文字列からの構築
  - エフェクト ID からのプロパティ設定
  - プロジェクト統計情報の拡充
  - 選択レイヤーの JSON シリアライズ

- 完了条件:
  - WebUI からレイヤー選択可能
  - WebUI からエフェクト制御可能
  - プロジェクト情報が Web で取得可能

### Phase 4: インスペクター改善

- 目的:
  - 全レイヤータイプ対応
  - 状態管理の改善

- 作業項目:
  - 未対応レイヤータイプの判定追加
  - projectClosed シグナルの接続
  - 状態整理の改善

- 完了条件:
  - 全レイヤータイプでインスペクター表示
  - プロジェクトクローズで正しくクリーンアップ

---

## 技術的課題

### 1. Proxy 生成のパフォーマンス

**課題:**
- 大量の動画ファイルのプロキシ生成に時間がかかる
- バックグラウンド処理が必要

**解決案:**
- ジョブキューで管理
- 進捗表示
- 一時停止/再開機能

### 2. WebUI との同期

**課題:**
- WebUI とネイティブ UI の状態同期
- 競合の防止

**解決案:**
- 単一方向のデータフロー
- イベントベースの更新
- 競合検出

### 3. Undo/Redo との統合

**課題:**
- レイヤー操作の Undo/Redo 完全対応
- 状態のシリアライズ

**解決案:**
- コマンドパターンの完全実装
- Memento パターンでの状態保存

---

## 関連ドキュメント

- `docs/planned/MILESTONE_APP_LAYER_COMPLETENESS.md` - アプリ層完成度
- `docs/planned/MILESTONE_VIDEO_LAYER_INTEGRATION_2026-03-27.md` - ビデオレイヤー統合
- `docs/planned/MILESTONES_BACKLOG.md` - 全体バックログ

---

## 実装順序の推奨

1. **Phase 1: VideoLayer Proxy** - 性能改善で効果大
2. **Phase 2: プロジェクト管理** - 安定性向上
3. **Phase 3: WebUI ブリッジ** - 機能拡充
4. **Phase 4: インスペクター** - UX 改善

---

## 今後の拡張

### Phase 5: アセット管理改善

- 未使用アセットの自動検出
- アセットのタグ付け
- スマートフォルダー

### Phase 6: プロジェクトテンプレート

- テンプレートプロジェクトの作成
- 初期設定の自動化
- ジャンル別プリセット

### Phase 7: 共同編集基盤

- プロジェクトロック
- 変更履歴
- マージ機能

---

**文書終了**
