# Milestone: Color Grading Workspace (M-SC-3)

## 🎯 目的
`ArtifactColorScienceManager` や `ArtifactHDRMonitor` の解析機能を一箇所に集約した、「カラーグレーディング専用レイアウト」を構築し、映像制作者がカラーコレクション（色補正）とグレーディング（ルック作成）を効率的に行える環境を提供する。

## 🏗️ アーキテクチャ構成
1. **`ArtifactScopes`**: 
   - Waveform, RGB Parade, Vectorscope のリアルタイムレンダリングエンジン。
2. **`GradingPanel`**:
   - リフト・ガンマ・ゲイン、3-Way カラーホイール、RGB カーブ操作用の専用 UI。
3. **`CompareModeView`**:
   - ビューポート上の Wipe (スプリット) 表示による、適用前・適用後の比較機能。

## 📅 実装フェーズ

### Phase 1: ビジュアルスコープの実装 (2026-04-01 - 2026-04-10)
- [ ] `HDRMonitor` からピクセルデータを取得し、Waveform (Luma波形) を描画。
- [ ] RGB Parade (チャンネル別波形) の実装。
- [ ] Vectorscope (色相/彩度分布) の実装。
- [ ] スコープの更新間隔調整（パフォーマンス向上のための。5-10fps）。

### Phase 2: Grading Workspace の統合 (2026-04-11 - 2026-04-20)
- [ ] 「Color Grading」用ウィンドウレイアウトプリセットの作成。
- [ ] `ColorWheelWidget` のブラッシュアップ（微細なドラッグ操作、ダブルクリックリセット）。
- [ ] インスペクタとの双方向同期（レイヤーを選択したらそのグレーディング設定を開く）。

### Phase 3: モニタリング & LUT ワークフロー (2026-04-21 - 2026-05-01)
- [ ] LUT ブラウザの実装（複数の .cube / .3dl ファイルを一覧表示、ホバーでプレビュー）。
- [ ] HDR モニタリングモードの UI 切り替え（HLG/PQ 表示）。
- [ ] マスクとの連携（特定の領域だけにグレーディングを掛ける Partial Application の UI）。

## 🚀 期待される成果
- プロのカラーリストのワークフローに近い精度の色調整が可能になる。
- リアルタイムスコープによる客観的な露出・色調整の実現。

## 🔗 関連マイルストーン
- [M-CS-1 Advanced Color Science Pipeline](MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md)
- [M-FX-6 Color Correction / Grading](MILESTONE_COLOR_CORRECTION_2026-03-27.md)
