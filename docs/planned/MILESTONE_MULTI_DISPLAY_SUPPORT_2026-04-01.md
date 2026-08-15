# Milestone: Multi-Display Support (2026-04-01)

**最終更新:** 2026-08-15
**Status:** Secondary preview／screen selection／fullscreen／DPR／basic display profile implemented, color profile and layout integration pending
**Goal:** デュアル/マルチディスプレイ環境での制作ワークフローを強化する

---

## 現状

| 機能 | 状態 |
|------|------|
| `QScreen` 使用 | ✅ screen list／availableGeometry を使用 |
| マルチモニター検出 | ✅ セカンドプレビューの screen selection |
| セカンドモニタープレビュー | ✅ `ArtifactSecondaryPreviewWindow` |
| フルスクリーンプレビュー | ✅ fullscreen／ESC・F11 |
| ウィンドウ画面間移動 | ✅ 選択screenへの配置 |
| モニタープロファイル連携 | ❌ 未実装 |
| セカンドプレビュー設定保存 | ✅ screen／fullscreen／FPS／geometry |

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

---

## Static audit follow-up (2026-07-25)

現行ソースでは `ArtifactSecondaryPreviewWindow` が存在し、`QGuiApplication::screens()` による画面一覧、`showOnScreen()`、`availableGeometry()`、fullscreen 切替、View Menu からの起動を確認できる。したがって文書の Phase 1〜2 を「未着手」とする記載は現状と一致しない。レンダー側には window / widget の device pixel ratio 更新処理もある。

一方、専用 `MultiDisplayManager`、画面ごとの解像度・リフレッシュレート・カラープロファイル管理、画面別 workspace profile、ドックの画面間移動とレイアウト復元、OSD・フレーム操作・プレビュー更新契約は確認できない。実際の複数画面・異なる DPI での runtime 検証も未実施である。

### Audit status

- Phase 1: 部分実装 — Secondary Preview Window / screen selection / fullscreen を確認
- Phase 2: 部分実装 — fullscreen と DPR 基盤を確認。OSD・専用入力・更新同期は未確認
- Phase 3: 未完了 — 画面情報・色プロファイル・設定ページを統合する Manager は未確認
- Phase 4: 未着手相当 — dock の画面間レイアウト記憶・profile は未確認
- Status: `Not Started` から `部分実装・マルチディスプレイ統合待ち` に更新相当

## 現行コード監査 (2026-08-15)

- `ArtifactSecondaryPreviewWindow` は `QGuiApplication::screens()` の一覧から画面を選び、`availableGeometry()` を使った配置、fullscreen 切替、タイムライン frame 更新要求を持つ。View Menu から起動できるため、Phase 1 の主要導線は実装済み。
- Composition／Diligent renderer／PrimitiveRenderer は widget／window の devicePixelRatio を読み取り、physical viewport／render target／入力座標へ反映する。異なる DPI を考慮した基盤は存在する。
- 一方、専用 `MultiDisplayManager`、refresh rate／color profile の取得・保存、display ごとの workspace profile、dock layout の monitor-aware restore、fullscreen preview の OSD／frame keyboard contract は確認できない。
- 実際の複数画面、異なる DPI、画面切替後の renderer resize／入力座標／fullscreen 復帰の runtime 検証は未実施。secondary preview の表示同期と性能上限も未受入れ。

判定: **Phase 1 の secondary preview と Phase 2 の DPR／fullscreen 基盤は実装済み。display profile、layout memory、OSD／操作契約、multi-DPI runtime parity は pending。**

## Update 2026-08-15

- `ArtifactSecondaryPreviewWindow` の screen list、`availableGeometry()` 配置、fullscreen、timeline frame 更新要求、View Menu 起動を再確認。
- renderer／PrimitiveRenderer 側の devicePixelRatio 反映と physical viewport／render target／入力座標への接続も確認できる。
- refresh rate／color profile、display workspace profile、monitor-aware dock restore、fullscreen OSD／keyboard contract、複数画面・異なる DPI の runtime parity は未完了・未検証。

## Update 2026-08-15 — 現行順序での実装確認

既存の `ArtifactSecondaryPreviewWindow` は画面一覧／配置／fullscreen／ESC・F11操作／OSD／timeline frame更新を持ち、Phase 1〜2の主要導線は実装済みだった。renderer側もDPRをphysical viewport／render target／入力座標へ反映しているため、今回は新規の重複window実装を追加しない。

次の実装単位は、画面識別子を安定化したdisplay profile、refresh rate／color profile情報、monitor-aware workspace復元である。複数画面・異なるDPIでのruntime検証は未完了。
