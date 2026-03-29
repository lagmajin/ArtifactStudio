# アプリ層改善 Milestone

**作成日:** 2026-03-28  
**ステータス:** 計画中  
**関連コンポーネント:** Artifact アプリケーション層（ArtifactCore ではない）

---

## 概要

アプリ層（`Artifact/src/*`）に存在する TODO・FIXME・未実装機能を整理し、優先度順に実装する。

---

## 発見された問題点（TODO/FIXME ベース）

### ★★★ 問題 1: WebUI ブリッジの未実装機能

**場所:** `Artifact/src/Widgets/WebUI/ArtifactWebBridge.cppm`

**TODO 一覧:**
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

**影響:**
- WebUI からのレイヤー選択が機能しない
- エフェクトプロパティの Web 制御ができない
- プロジェクト統計情報が不完全

**工数:** 4-6 時間

---

### ★★★ 問題 2: VideoLayer::generateProxy() 未実装

**場所:** `Artifact/src/Layer/ArtifactVideoLayer.cppm:696`

```cpp
// TODO: Implement actual proxy generation using FFmpeg or OpenCV
void ArtifactVideoLayer::generateProxy() {
    // 現在は何もしない
}
```

**影響:**
- プロキシ機能が使えない
- 高解像度動画の編集が重い
- ワークフローの効率化ができない

**工数:** 6-8 時間

---

### ★★ 問題 3: プロジェクト管理の未実装機能

**場所:** `Artifact/src/Project/ArtifactProject.cppm`

**TODO 一覧:**
```cpp
// line 310:
// bool ArtifactProject::Impl::removeById() - TODO: container_.remove() not implemented

// line 353:
// TODO: ダーティ状態が変更されたときの通知を実装する

// line 925:
// void ArtifactProject::setDirty(bool dirty) - TODO: impl_->setDirty not available
```

**影響:**
- コンポジションの削除が不完全
- 未保存検出が機能しない
- 自動保存が正しく動作しない

**工数:** 4-6 時間

---

### ★★ 問題 4: インスペクターの機能不足

**場所:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`

**TODO 一覧:**
```cpp
// line 478:
// TODO: 他のレイヤータイプも判定

// line 974:
// TODO: projectClosed シグナルがあれば接続
```

**影響:**
- 一部のレイヤータイプでインスペクターが正しく表示されない
- プロジェクトクローズ時の状態整理が不完全

**工数:** 3-4 時間

---

### ★★ 問題 5: プロジェクトインポーターの改善

**場所:** `Artifact/src/Project/ArtifactProjectImporter.cppm`

**問題:**
- エラーハンドリングが不十分
- 進行状況の表示がない
- 大きなプロジェクトのインポートでタイムアウト

**工数:** 4-6 時間

---

### ★ 問題 6: プロジェクトパッケージャーの改善

**場所:** `Artifact/src/Project/ArtifactProjectPackager.cppm`

**TODO:**
```cpp
// line 64:
// TODO: プロジェクトファイルのパスを Assets/xxx に書き換えた
```

**影響:**
- パッケージ化後のパス解決が不完全
- 他環境での開包に失敗する可能性

**工数:** 2-3 時間

---

### ★ 問題 7: レイヤー追加コマンドの未実装

**場所:** `Artifact/src/Project/ArtifactProjectManager.cppm`

**関連:** `MILESTONE_APP_LAYER_COMPLETENESS.md`

**未実装:**
- AddLayerCommand の完全実装
- RemoveLayerCommand の完全実装
- Undo/Redo との完全統合

**工数:** 6-8 時間

---

## 優先度別実装計画

### P0（必須）

| 項目 | 工数 | 優先度 | 依存 |
|------|------|--------|------|
| **VideoLayer::generateProxy()** | 6-8h | 🔴 高 | なし |
| **プロジェクト管理 TODO** | 4-6h | 🔴 高 | なし |
| **レイヤー追加コマンド** | 6-8h | 🔴 高 | なし |

### P1（重要）

| 項目 | 工数 | 優先度 | 依存 |
|------|------|--------|------|
| **WebUI ブリッジ** | 4-6h | 🟡 中 | なし |
| **インスペクター改善** | 3-4h | 🟡 中 | なし |
| **プロジェクトインポーター** | 4-6h | 🟡 中 | なし |

### P2（推奨）

| 項目 | 工数 | 優先度 | 依存 |
|------|------|--------|------|
| **プロジェクトパッケージャー** | 2-3h | 🟢 低 | なし |

**合計工数:** 29-41 時間

---

## Phase 構成

### Phase 1: VideoLayer Proxy 機能

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
