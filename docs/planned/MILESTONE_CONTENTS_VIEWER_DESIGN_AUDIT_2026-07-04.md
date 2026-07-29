# マイルストーン: コンテンツビューワー デザイン監査 (2026-07-04)

> 作成: 2026-07-04
> 元依頼: 「コンテンツビューワーもよろしく」

## 監査サマリー

`ArtifactContentsViewer.cpp`（2,295行）は、画像/動画/音声/3Dモデル/比較を横断する inspection viewer としての責務を果たしているが、単一クラスに全機能がフラットに詰まっており、以下の領域で改善余地がある。

---

## 🔴 P0（最優先）: QImage/QPixmap 依存と表示パイプライン

### 問題
- **QImage/QPixmap が表示の本流**: `originalImage` は `QPixmap`、`imageLabel` は `QLabel::setPixmap()` で描画。プロジェクトルールで禁止されている QImage 新規採用と矛盾し、GPU パイプラインをバイパスしている。
- **高解像度画像でメモリ爆発**: `QPixmap::scaled()` + `QLabel::setFixedSize()` で CPU 側にフルサイズ Pixmap を保持。4K/8K 画像で深刻。
- **画像変換が CPU 依存**: 回転 (`QTransform`)、ズーム (`QPixmap::scaled()`) がすべて CPU。GPU 非使用。
- **パレードスコープが QImage 変換**: `syncParadeScopeFrame()` で `QPixmap::toImage()` → `ParadeScopeWidget::updateFrame(QImage)`。往復変換コスト。

### 参照
- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp` L1325-1361, L1424-1448, L978-992

---

## 🔴 P0: 2,295行の単一クラス肥大化

### 問題
- Image / Video / Audio / 3D Model / Compare の全ロジックが `ArtifactContentsViewer::Impl` 1クラスにフラットに詰まっている。
- メンバ変数 65 個（`QLabel*` 16個、`QToolButton*` 13個、スライダー3個、音声7個、動画3個…）。
- メソッド 50+ 個。`ensure*Widgets()` が 5 系統、`update*()` が 8 系統。
- 責務分割されていないため、1つの型の修正が他に波及しやすい。

---

## 🟡 P1（高優先）: ツールバーとヘッダー UI の混雑

### 問題
- **20個近いボタンがヘッダーに詰め込まれている**: `fitButton`, `rotateLeftButton`, `rotateRightButton`, `resetButton`, `playButton`, `pauseButton`, `stopButton`, `copyPathButton`, `revealButton`, `previewButton`, `sourceButton`, `finalButton`, `compareButton`, `compareSwapButton`, `compareAssignAButton`, `compareAssignBButton`。
- **QLabel の羅列**: `titleLabel`, `typeBadgeLabel`, `viewerBadgeLabel`, `metaLabel`, `stateLabel`, `channelMetaLabel`, `surfaceMetaLabel` の7つの QLabel が情報を縦横に並べている。
- **モードボタン (Source/Final/Compare) とモード表示の重複**: ボタンで切り替え + `stateLabel` で `"State: ... | Source"` と文字列でも表示。二重。

---

## 🟡 P1: メタデータ表示が文字列連結の QLabel 任せ

### 問題
- `channelMetaLabel->setText(chips.join("  •  "))` で chip 風の表示をしているが、実際は単なる `QLabel` 文字列。ホバーやクリックで詳細展開できない。
- メタデータ（作成日、カメラ情報、カラースペース、Alpha有無、ビット深度）の表示がない。
- ファイルサイズは `humanFileSize()` で出しているが、フレームレートやコーデック情報が未表示。

### 参照
- `ArtifactContentsViewer.cpp` L1709-1774 (`updateChannelMetaSurface`), L1599-1707 (`updateSurfaceMeta`)

---

## 🟡 P1: Compare モードの機能不足

### 問題
- A/B 比較が `QSplitter` + `QLabel` 2枚のサイドバイサイド + ワイプスライダーのみ。
- ピクセル差分表示がない。
- オーバーレイ/ディゾルブモードがない。
- A/B ソースは手動割り当て（`assignCompareSource`）のみで、自動ペアリングがない。
- 比較結果の保存やエクスポートがない。

### 参照
- `ArtifactContentsViewer.cpp` L1021-1280

---

## 🟡 P1: 動画/音声再生の制限

### 問題
- **動画再生が QMediaPlayer 依存**: Qt Multimedia バックエンド。FFmpeg backend は音声のみ（`audioController_->setDecoderBackend(DecoderBackend::FFmpeg)`）。動画も FFmpeg に統一すべき。
- **フレームステップがない**: 再生/停止/シークのみで、1フレーム進む/戻るがない。
- **ループ設定がない**: 単発再生のみ。ループや ping-pong がない。
- **音声波形のリアルタイム更新がない**: 再生位置の追従が `audioWaveformWidget->setPosition()` のみ。

---

## 🟡 P1: チャンネル分離表示が UI のみで機能なし

### 問題
- `channelMetaLabel` に `"RGBA" / "RGB" / "Alpha" / "Luma"` の chip が出ているが、クリックして実際にチャンネル分離表示する UI がない。
- Parade Scope は RGB パレードモードのみ。

---

## 🔵 P2（中優先）: 不完全・未実装の機能

| 問題 | 詳細 |
|---|---|
| **Viewer Assignment (Viewer 1〜4) が表面的** | `viewerBadgeLabel` で `"Viewer XX"` と表示するだけ。マルチビューアの切り替えやミラーリングの実装が不明確 |
| **最近使ったソースが QComboBox** | 12個のリスト。スクロール必須で一覧性が悪い。サムネイル付きのグリッドやカルーセルのほうが見やすい |
| **画像チャンネル情報不足** | ビット深度、カラースペース（sRGB/Linear/ACES）、Alpha モード（Straight/Premultiplied）の表示がない |
| **スクリーンショット/エクスポート不在** | Phase 5 で言及されているが未実装 |
| **エラー表示が QLabel のみ** | `showInfoMessage()` が `QLabel::setText()` だけ。エラーの深刻度や再試行 UI がない |
| **プロファイリングウィジェットが製品 UI に露出** | Audio ページに `ArtifactPerformanceProfilerWidget` が常駐。デバッグツールがユーザーに見えている |

---

## ⚙️ コード設計上の問題

| 問題 | 詳細 |
|---|---|
| **2,295行の単一ファイル** | Image / Video / Audio / 3D Model / Compare の全ロジックが1クラスにフラット |
| **Null チェックの乱立** | ほぼ全メソッドが `if (!widget) return;` で始まる。lazy init パターンとの相性が悪い |
| **QObject::connect 多用** | プロジェクトルール違反（既存コードのため許容） |
| **QShortcut の直接 new** | `new QShortcut(...)` が散在。ショートカット管理が中央集権化されていない |
| **`using namespace ArtifactCore` 不在** | グローバルモジュールフラグメントに不要な STL include が多い（`<iostream>`, `<random>`, `<any>`, `<variant>` 等） |

---

## 改善の推奨優先順位

| 順位 | 領域 | 内容 |
|---|---|---|
| 1 | **GPU 表示パイプライン化** | QPixmap/QImage 依存を撤廃し、ImageF32x4_RGBA + GPU レンダリングに移行 |
| 2 | **クラス分割** | ImagePage / VideoPage / AudioPage / ModelPage / ComparePage に分割し、Impl を薄くする |
| 3 | **ツールバー整理** | コンテキスト依存のボタン表示/非表示、モードボタンをツールバー central area に集約 |
| 4 | **メタデータパネル強化** | カラースペース、ビット深度、チャンネル情報を owner-draw で表示 |
| 5 | **Compare モード拡張** | 差分/オーバーレイ/ディゾルブ モードを追加 |
| 6 | **動画 FFmpeg 統一** | QMediaPlayer → FFmpeg backend に統一、フレームステップ/ループ追加 |
| 7 | **プロファイラ隠蔽** | 開発ビルドのみ表示、または設定でトグル |

---

## 関連文書

- `docs/done/MILESTONE_CONTENTS_VIEWER_EXPANSION_2026-03-27.md` — Phase 1〜5 の完了マイルストーン
- `docs/planned/MILESTONE_3D_MODEL_REVIEW_IN_CONTENTS_VIEWER_2026-03-28.md` — 3D モデルレビュー連携
- `docs/planned/MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md` — 3D モデルインポート連携
- `docs/planned/MILESTONES_BACKLOG.md` — 全体バックログ
- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp` — メイン実装 (2,295行)
- `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm` — 3D モデルビューア
- `docs/WIDGET_MAP.md` — ウィジェット責務マップ

---

## 備考

- プロジェクトルール: QImage の新規採用は原則禁止。描画・合成・転送の本流では `ImageF32x4_RGBA` などの GPU/バッファ寄り表現を優先。
- `QPainter` / Qt `CompositionMode` を使った新規の描画・合成実装は禁止。既存の Qt 合成コードを触る場合も、増やさず縮小・撤去の方向。
- 新規シグナル＆スロット接続は禁止。既存のイベント経路やサービスを再利用。
- QtCSS / `setStyleSheet()` の新規追加禁止。

---

## Next Execution Slice

## 2026-07-30 実装監査

- Compare surface は `Wipe` / `Split` / `Difference` の表示モード、A/B source assignment、左右 swap、wipe position の設定保存・復元を実装済みとして確認した。
- Difference は画像同士の場合に限定し、非対応 source では Split へ安全に fallback する。
- スクリーンショット／export、Compare の runtime 視認性確認は未完了として残す。

P1 は、Compare モードとメタデータ表示の責務を先に切り分ける。

### P1A の着手点

1. Source / Final / Compare の mode ボタンと状態表示の重複を解消する
2. Compare モードを side-by-side / wipe / overlay / dissolve の 4 系統に整理する
3. 画像メタデータは file size / frame rate / codec / color space / alpha mode に絞って短く出す
4. channelMetaLabel の chip 風表示を、その場で読める情報カードに寄せる

### P1 完了条件

- compare の見え方が 1 本の語彙で読める
- metadata が文字列連結の羅列に見えない
- mode 表示と mode 操作が二重にならない

### P2A の着手点

1. viewer assignment の意味を 1 画面で読めるようにする
2. recent source の一覧性を QComboBox 依存から見直す
3. image / video / audio / 3D model の state 表示を揃える
4. error 表示に再試行や深刻度の区別を足す

### P2 完了条件

- 何を見ているかが 1 画面で分かる
- 診断表示が widget ごとにばらけない
- compare / metadata / error の責務が混ざらない

### P3 への前提

- GPU パイプラインの見直しは compare / metadata の責務が固まってからでよい
- 動画 FFmpeg 統一やチャンネル分離は、state 表示が安定してから後段に回す
