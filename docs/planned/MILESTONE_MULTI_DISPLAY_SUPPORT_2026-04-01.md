# Milestone: Multi-Display Support (2026-04-01)

**Status:** Not Started
**Goal:** デュアル/マルチディスプレイ環境での制作ワークフローを強化する

---

## 現状

| 機能 | 状態 |
|------|------|
| `QScreen` 使用 | ⚠️ 一部（ダイアログ位置決めのみ） |
| マルチモニター検出 | ❌ 未実装 |
| セカンドモニタープレビュー | ❌ 未実装 |
| フルスクリーンプレビュー | ❌ 未実装 |
| ウィンドウ画面間移動 | ❌ 未実装 |
| モニタープロファイル連携 | ❌ 未実装 |

---

## Phase 1: セカンドモニタープレビューウィンドウ

### 実装内容
- コンポジションを別ウィンドウで表示する `ArtifactSecondaryPreviewWindow`
- フルスクリーンモード対応
- リアルタイム更新（コンポジションビューと同期）
- 画面選択ダイアログ

### 対象ファイル
| ファイル | 内容 |
|---------|------|
| `Artifact/include/Widgets/ArtifactSecondaryPreviewWindow.ixx` | 新規 |
| `Artifact/src/Widgets/ArtifactSecondaryPreviewWindow.cppm` | 新規 |
| `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm` | 起動項目追加 |

### 機能
- **ウィンドウモード** — 浮遊ウィンドウとして表示
- **フルスクリーンモード** — 選択したモニターでフルスクリーン
- **自動更新** — タイムライン再生に追従
- **モニター選択** — 利用可能なモニター一覧から選択

### 見積: 6h

---

## Phase 2: フルスクリーンプレビュー

### 実装内容
- 現在のコンポジションビューをフルスクリーン表示
- キーボードショートカット対応（Space または専用キー）
- ESCで解除
- OSD表示（フレーム番号、時間コード、解像度）

### 対象ファイル
| ファイル | 内容 |
|---------|------|
| `Artifact/include/Widgets/ArtifactFullscreenPreview.ixx` | 新規 |
| `Artifact/src/Widgets/ArtifactFullscreenPreview.cppm` | 新規 |

### 機能
- **モニター選択** — プレビュー表示先のモニターを選択
- **OSD表示** — フレーム番号、時間コード、解像度（3秒後にフェードアウト）
- **キーボード操作** — ESC解除、←→でフレーム移動、Space再生/停止
- **スケーリング** — モニター解像度に合わせた自動スケーリング（ニアレストネイバー/バイリニア選択可能）

### 見積: 6h

---

## Phase 3: マルチモニター検出 & 設定

### 実装内容
- 利用可能なモニター一覧の取得と情報表示
- 各モニターの解像度、スケーリング、リフレッシュレート、プライマリ/セカンダリ判定
- モニターごとのカラープロファイル検出（Windows Color API）
- 設定ダイアログへの「ディスプレイ」ページ追加

### 対象ファイル
| ファイル | 内容 |
|---------|------|
| `ArtifactCore/include/Display/MultiDisplayManager.ixx` | 新規 |
| `ArtifactCore/src/Display/MultiDisplayManager.cppm` | 新規 |
| `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm` | ディスプレイページ追加 |

### 機能
- **モニター一覧** — 名前、解像度、スケーリング、リフレッシュレート
- **プライマリモニター** — 自動検出
- **カラープロファイル** — sRGB/Display P3/Adobe RGB 等の自動検出
- **ワークスペース記憶** — 各モニターのウィンドウ配置を記憶

### 見積: 4h

---

## Phase 4: ウィンドウ画面間移動 & レイアウト記憶

### 実装内容
- ドッキングパネルの別モニターへのドラッグ&ドロップ
- ウィンドウ位置のモニター間自動調整（スケーリング差異対応）
- レイアウトプリセット（1モニター用/2モニター用/3モニター用）

### 対象ファイル
| ファイル | 内容 |
|---------|------|
| `Artifact/src/Widgets/Dock/DockStyleManager.cppm` | 画面間移動対応 |
| `ArtifactCore/include/UI/WorkspaceLayoutManager.ixx` | レイアウト記憶拡張 |

### 機能
- **ドラッグ&ドロップ** — パネルを別モニターにドラッグ可能
- **スケーリング補正** — 125%/150% 等不同のスケーリング間での位置補正
- **レイアウトプリセット** — 1/2/3モニター構成のプリセット切替
- **セッション記憶** — 終了時のウィンドウ配置を復元

### 見積: 6h

---

## Recommended Order

| 順序 | フェーズ | 見積 | 優先度 |
|---|---|---|---|
| 1 | **Phase 1: セカンドモニタープレビュー** | 6h | P0 |
| 2 | **Phase 2: フルスクリーンプレビュー** | 6h | P0 |
| 3 | **Phase 3: マルチモニター検出 & 設定** | 4h | P1 |
| 4 | **Phase 4: ウィンドウ画面間移動** | 6h | P2 |

**総見積: ~22h**

---

## 既存の関連ファイル

| ファイル | 内容 |
|---------|------|
| `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` | `QScreen` 使用例（ダイアログ位置決め） |
| `Artifact/src/Widgets/Dock/DockStyleManager.cppm` | ドッキング管理 |
| `ArtifactCore/include/Display/MultiDisplayManager.ixx` | （新規作成予定） |

---

## 技術的注意点

1. **Qt マルチモニターAPI**
   - `QGuiApplication::screens()` — 利用可能なモニター一覧
   - `QScreen::availableGeometry()` — タスクバーを除いた作業領域
   - `QScreen::geometry()` — 物理的なモニター領域
   - `QScreen::devicePixelRatio()` — スケーリング係数

2. **スケーリング差異**
   - モニターごとに異なる DPI スケーリング（100%/125%/150%）に対応
   - `Qt::AA_EnableHighDpiScaling` と `Qt::AA_UseHighDpiPixmaps` を適切に設定

3. **フルスクリーン**
   - `QWidget::showFullScreen()` — フルスクリーン表示
   - `QWindow::setScreen()` — 表示先モニター切り替え
   - Windows: `SetWindowLong` でボーダーレスウィンドウ化も検討

4. **パフォーマンス**
   - セカンドモニタープレビューは解像度を落とした更新も可能にする
   - 更新レート制限（30fps/60fps 選択可能）
