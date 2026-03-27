# レンダーシステム包括調査レポート (2026-03-27)

## 1. Render Queue Widget UI

### 1.1 ジョブ並べ替え D&D 未実装
**場所:** `ArtifactRenderQueueManagerWidget.cpp` 全体
- `QListWidget` に `mimeData()` / `dropEvent()` / `setDragEnabled(true)` なし
- ▲/▼ ボタンのみ、D&D での並べ替え不可

### 1.2 codecProfile が出力設定フローで消失
**場所:** `ArtifactRenderQueueManagerWidget.cpp:1711, 1734-1741`
- `jobOutputSettingsAt` の呼び出しが 7 パラメータ（codecProfile 不要）
- `setJobOutputSettingsAt` の呼び出しが 6 パラメータ（codecProfile 未渡し）
- ProRes 4444 プロファイルがデフォルトにリセットされる

### 1.3 プリセット保存/読込で codecProfile 消失
**場所:** `ArtifactRenderQueueManagerWidget.cpp:515-518, 571`
- `saveCurrentSelectionAsPreset()` / `applyPresetToSelection()` が codecProfile を無視

### 1.4 出力パスがハードコード
**場所:** `ArtifactRenderQueueService.cppm:1480, 1501, 1545`
- `QDir::homePath() + "/Desktop/output.mp4"` 固定
- 非英語環境/Desktop なし環境で壊れる

### 1.5 ジョブパネルの Start ボタン未接続
**場所:** `ArtifactRenderQueueJobPanel.cpp:224-227`
- `renderingStartButton` に `connect()` 呼び出しなし
- `handleShowSaveDialog()` が空 (line 107-110)

### 1.6 `RenderQueueManagerJobDetailWidget` が空殻
**場所:** `ArtifactRenderQueueJobPanel.cpp:95-128`
- Impl にメンバなし、コンストラクタが何もしない
- デッドコード

### 1.7 歴史ログの高さ固定
**場所:** `ArtifactRenderQueueManagerWidget.cpp:1261`
- `setMaximumHeight(100)` — 100px 固定、リサイズ不可

### 1.8 プリセットダイアログのメモリリーク
**場所:** `ArtifactRenderQueueManagerWidget.cpp:1439`
- `new` で割り当て、`show()` 呼び出し。親破棄時にリーク

### 1.9 日本語/英語 UI 文字列の混在
**場所:** `ArtifactRenderQueueJobPanel.cpp:170-186, 214`
- ステータステキストが日本語 ("待機中", "レンダリング中") で他は英語

---

## 2. Render Queue Service

### 2.1 DETACH スレッドのクラッシュリスク (Critical)
**場所:** `ArtifactRenderQueueService.cppm:1914`
- `std::thread(...).detach()` — ワーカースレッドが `this` をキャプチャ
- サービス破棄中にスレッドが実行中 → dangling pointer → UB
- デストラクタに `join()` やシャットダウンメカニズムがない

### 2.2 pause/cancel がワーカースレッドを即座に停止しない
**場所:** `ArtifactRenderQueueService.cppm:1919-1925`
- ステータス変更のみ。現在のフレームレンダリング中は停止しない

### 2.3 queueManager へのスレッド競合
**場所:** `ArtifactRenderQueueService.cppm` 全体
- ワーカースレッドが `getJob()` を直接呼び出し
- UI スレッドも `getJob()` / `setJobProgress()` を呼び出し
- mutex 保護なし → データ競合

### 2.4 renderSingleFrameGPU() がダミーレンダラー
**場所:** `ArtifactRenderQueueService.cppm:1363-1394`
- ヘッドレスレンダラーを初期化 → clear() + flush() → readback
- コンポジションの内容を一切レンダリングしない
- 出力は空白フレーム

### 2.5 ワーカースレッドに try-catch なし
**場所:** `ArtifactRenderQueueService.cppm:1793-1914`
- 例外が発生すると `std::terminate()` → アプリクラッシュ

### 2.6 allJobsCompleted シグナルが失敗時も発火
**場所:** `ArtifactRenderQueueService.cppm:1906-1912`
- 個別ジョブが失敗しても `allJobsCompleted()` が発火
- "全て成功" と "一部失敗" を区別できない

### 2.7 startRenderQueue / startAllRenderQueues が未実装
**場所:** `ArtifactRenderQueueService.ixx:112-113`
- ヘッダーに宣言のみ、実装なし → デッド API

### 2.8 void* ffmpegEncoder — 無効化されたコード
**場所:** `ArtifactRenderQueueService.cppm:1068`
- `void* ffmpegEncoder = nullptr; // Temporarily void* to bypass build error`
- フレームバッファ→エンコーダーパイプラインが完全に機能不全

---

## 3. FFmpeg Encoder

### 3.1 EXR シーケンスが TIFF にフォールバック
**場所:** `FFmpegEncoder.cppm:546-551`
- format="exr" で `lastError_` を設定 → TIFF にサイレントフォールバック
- `isImageSequenceFormatAvailable("exr")` が `false` を返す
- EXR プリセットが UI にあるが、実際には TIFF で出力される

### 3.2 画像シーケンスのエンコーダーを毎フレーム作成
**場所:** `FFmpegEncoder.cppm:539-626`
- `addImageSequenceFrame()` が毎フレーム `AVCodecContext` を作成/破棄
- コーデック初期化は重い処理。パフォーマンス大幅劣化

### 3.3 16bit 画像シーケンスのピクセルフォーマット不整合
**場所:** `FFmpegEncoder.cppm:506-524`
- `RGB48LE` (3ch) で 4ch 分のデータを書き込み → メモリ破損

### 3.4 DNxHD コーデックが未対応
**場所:** `FFmpegEncoder.cppm:94-112`
- コーデック ID マッピングに `AV_CODEC_ID_DNXHD` が存在しない
- DNxHD プリセットがサイレントに H.264 にフォールバック

### 3.5 スケーラー品質が低い
**場所:** `FFmpegEncoder.cppm:220, 310`
- `SWS_BILINEAR` を使用。高品質には `SWS_LANCZOS` が必要

---

## 4. Render Output Dialog

### 4.1 codecProfile が呼び出し元に返されない
**場所:** `ArtifactRenderOutputSettingDialog.cppm:467-477`
- `dialog.codecProfile()` アクセサが存在するが、マネージャーウィジェットが呼び出さない
- プロファイル変更がサイレントに破棄される

### 4.2 フォーマット/コーデックの組み合わせバリデーションなし
- 非互換の組み合わせ（EXR + H.264, WebM + ProRes 等）が選択可能
- エンコード開始時にランタイムエラーまたはサイレントフォールバック

### 4.3 解像度プリセット不足
**場所:** `ArtifactRenderOutputSettingDialog.cppm:85-107`
- 3840x2160, 1920x1080, 1280x720 のみ
- 不足: 2560x1440 (QHD), 7680x4320 (8K), 1080x1920 (モバイル/縦)

### 4.4 CRF ベースのコーデックでビットレートが無効化されない
**場所:** `ArtifactRenderOutputSettingDialog.cppm:307-312`
- H.264/H.265/VP9 ではビットレートが無視される (CRF 使用) が、UI では常に有効

---

## 5. Render Presets

### 5.1 高画質プリセットが標準と同一
**場所:** `ArtifactRenderQueuePresets.cppm:30-47`
- "H.264 MP4" と "H.264 MP4 (高画質)" の container/codec が同一
- CRF、bitrate、resolution の差がなし → "高画質" プリセットが無意味

### 5.2 プリセット構造体に品質フィールドなし
- `ArtifactRenderFormatPreset` に CRF/bitrate/resolution/fps/quality フィールドが存在しない
- プリセットはフォーマット/コーデックのみ制御、品質はデフォルトのまま

### 5.3 ProRes バリアント不足
- ProRes 422 HQ / ProRes 4444 のみ
- 不足: Proxy, LT, Standard（正規化関数は対応済みだがプリセットが未作成）

### 5.4 DNxHD プリセットが FFmpeg エンコーダー非対応
- プリセット `codec = "dnxhd"` が定義されているが、エンコーダーにマッピングなし

### 5.5 Audio カテゴリが未実装
**場所:** `ArtifactRenderQueuePresets.cppm:217`
- `case ArtifactRenderFormatCategory::Audio: return false;`

### 5.6 グローバルシングルトンがリーク
**場所:** `ArtifactRenderQueuePresets.cppm:176-193`
- `g_presetManagerInstance` が `new` だが `delete` なし

---

## 6. 画像シーケンス

### 6.1 フレーム番号が開始フレームを無視
**場所:** `ArtifactRenderQueueService.cppm:1869`
- `arg(f, 4, 10, QChar('0'))` — 絶対フレーム番号 (例: 1000 → `render_1000.png`)
- AE/Nuke は 0 または 1 始まりの連番を期待

### 6.2 画像シーケンスの品質設定が未渡し
**場所:** `ArtifactRenderQueueService.cppm:1721-1724`
- `ImageExporter::write()` に `ImageExportOptions` のデフォルトを使用
- 圧縮レベル、品質、ビット深度がジョブ設定から渡されない

### 6.3 ファイル名パターンがハードコード
- `basename_0001.ext` のみ。`%04d` パターン、フレームオフセット、カスタム命名規則非対応

---

## 7. クロスカッティング

### 7.1 デストラクタがワーカースレッドを待たない
**場所:** `ArtifactRenderQueueService.cppm:1452`
- `delete impl_` が detach スレッドの完了を待たない → use-after-free

### 7.2 未使用の `#include` が大量
- 3つのファイルがそれぞれ ~30 の標準ライブラリヘッダーを未使用でインクルード
- `<tbb/parallel_for.h>` がサービスファイルで2回インクルード

---

## 優先度別改善案

### Critical (クラッシュ/データ損失)
| # | 改善 | 見積 |
|---|------|------|
| 2.1 | detach スレッド → join/wait メカニズム追加 | 2h |
| 2.3 | queueManager アクセスに mutex 追加 | 1h |
| 2.5 | ワーカースレッドに try-catch 追加 | 30min |
| 3.3 | 16bit 画像シーケンスのメモリ破損修正 | 1h |

### High (品質/機能)
| # | 改善 | 見積 |
|---|------|------|
| 1.2 | codecProfile を出力設定フローに渡す | 1h |
| 5.1 | 高画質プリセットに CRF=18 等を設定 | 30min |
| 5.2 | プリセット構造体に品質フィールド追加 | 2h |
| 3.1 | EXR のサイレントフォールバックを警告に変更 | 30min |
| 6.1 | 連番フレーム番号を 0 始まりに修正 | 30min |

### Medium (UX/パフォーマンス)
| # | 改善 | 見積 |
|---|------|------|
| 1.5 | ジョブパネルの Start ボタン接続 | 1h |
| 3.2 | 画像シーケンスのエンコーダー再利用 | 2h |
| 4.2 | フォーマット/コーデック組み合わせバリデーション | 2h |
| 4.3 | 解像度プリセット追加 (QHD, 8K, 縦) | 30min |
| 6.2 | 画像シーケンスの品質設定渡し | 1h |

### Low (コード衛生)
| # | 改善 | 見積 |
|---|------|------|
| 1.6 | 空殻ウィジェット削除 | 15min |
| 7.2 | 未使用 #include 削除 | 30min |
| 1.9 | UI 文字列の言語統一 | 1h |
| 5.6 | シングルトンリーク修正 | 15min |
