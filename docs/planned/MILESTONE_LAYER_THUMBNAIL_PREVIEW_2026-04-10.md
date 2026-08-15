# レイヤーサムネイルプレビューの実装

**最終更新:** 2026-08-15
**マイルストーン**: M-LA-5 Layer Thumbnail Preview
**作成日**: 2026-04-10
**見積もり**: 10-12h
**優先度**: Low (細かいUX改善)

## 2026-08-15 現行コード照合

- ✅ `ArtifactAbstractLayer` にサイズ別 thumbnail cache と mutation 時の invalidation があり、各 layer subclass が `getThumbnail()` を override している。
- ✅ Image／Video／Text／Shape／SVG／Solid／Clone／SDF／Null 等で内容に応じた thumbnail 生成または base fallback が確認できる。Video／Media 側には frame／timestamp thumbnail 抽出経路もある。
- ✅ Preview 設定には thumbnail 生成 ON／OFF と品質設定が登録されており、キャッシュ設定も存在する。
- ⚠️ 現行コードからは、Timeline の全レイヤー行へ 64px thumbnail を常時表示する専用 UI、hover 128px preview、クリック全画面 preview、レイヤー時刻との同期が一体化していることは確認できない。
- ⏳ 非同期 thumbnail generator、GPU thumbnail path、高DPI／memory eviction、設定 UI の完全接続、全 layer 種別の runtime QA は未完了。

## 概要

After Effects のレイヤーパネルにサムネイルを表示し、レイヤー内容を視覚的に確認できるようにする。
レイヤー管理の効率を大幅に向上。

## 機能仕様

### サムネイル生成
**自動生成:**
- レイヤー内容の小縮小表示 (64x64px)
- 動画/画像: 現在のフレームのサムネイル
- テキスト: テキスト内容のレンダリング
- シェイプ: シェイプのベクター表示
- ソリッド: 色の表示

### インタラクション
**プレビュー機能:**
- マウスホバーで拡大表示 (128x128px)
- クリックで全画面プレビュー
- タイムライン位置でのフレーム切り替え

### パフォーマンス最適化
**効率的な管理:**
- キャッシュシステムによる高速表示
- バックグラウンド生成 (非同期)
- 表示設定によるON/OFF制御

### 実装要件
- 既存レイヤーパネル拡張
- GPUアクセラレーション
- 設定保存
- 高DPI対応

### 実装場所
- `Artifact/src/Widgets/LayerPanel/ArtifactLayerPanelWidget.cppm` (拡張)
- サムネイル生成: `Artifact/src/Core/Thumbnail/ArtifactThumbnailGenerator.cppm` (新規)

## 技術的考慮
- メモリ使用量の最適化
- キャッシュの自動クリーンアップ
- マルチスレッド生成

## AEとの差別化
- より高品質なサムネイル
- インタラクティブなプレビュー
- パフォーマンスの最適化

## テストケース
- 各種レイヤー種別のサムネイル生成
- プレビュー機能の動作確認
- パフォーマンス劣化の確認
- メモリ使用量の監視
