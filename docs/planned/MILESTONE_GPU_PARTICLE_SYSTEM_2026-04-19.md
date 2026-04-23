# MILESTONE: GPUパーティクルシステム統合
作成日: 2026-04-19
ステータス: 計画中
対象バージョン: M11

---

## ✅ 現状調査結果

### ✔️ 既存実装済みコンポーネント (100% 完成)

| モジュール | 状態 | 備考 |
|-----------|------|------|
| [`ArtifactCore/include/Particle/ParticleSystem.ixx`] | ✅ 完成 | CPU パーティクルシステム 完全実装 |
| [`ArtifactCore/include/Graphics/ParticleCompute.ixx`] | ✅ 完成 | GPU Compute Shader パーティクル更新 |
| [`ArtifactCore/include/Graphics/ParticleRenderer.ixx`] | ✅ 完成 | GPU インスタンス描画 1Mパーティクル対応 |
| [`ArtifactCore/include/Particle/FluidForce.ixx`] | ✅ 完成 | 流体ソルバー 安定流体アルゴリズム |
| [`ArtifactCore/include/Particle/*`] | ✅ 完成 | エミッター、エフェクター、コライダー、カーブ |

### ❌ 未接続箇所

✅ **全てのコア機能は既に書かれている**
❌ **App 層の [`ArtifactParticleLayer`] では一切使われていない**

現在の ParticleLayer は:
- CPU のみで 100% シミュレーション実行
- CPU 側で頂点バッファ生成
- 1フレームごとに全パーティクルをGPUへ転送
- **最大でも 20,000 パーティクル が限界**

---

## 🎯 目標

GPU パーティクルパスを有効化し、**1,000,000 パーティクルを 60fps** でリアルタイム描画可能にする

---

## 📋 実装ステップ

### Phase 1: バッファブリッジ実装
- [ ] `ArtifactParticleLayer` に GPU モードフラグ追加
- [ ] `ParticlePool` のデータを `ParticleCompute` へ転送するブリッジ実装
- [ ] 既存 CPU エミッター / エフェクターのデータを GPU 定数バッファへマッピング
- [ ] ハイブリッドモード実装: CPU側で生成したパーティクルをGPUへ注入

### Phase 2: GPU シミュレーションパス統合
- [ ] `ParticleCompute` の初期化を `ArtifactCompositionRenderController` で実行
- [ ] レンダーループ内で Compute Shader Dispatch を呼び出し
- [ ] `ParticleRenderer` を `ArtifactIRenderer` パイプラインへ統合
- [ ] 投影行列 / ビュー行列の自動同期

### Phase 3: 機能パリティ確保
- [ ] 既存全てのエフェクターを GPU 側で再実装
  - [ ] 重力 / 風 / 抗力
  - [ ] アトラクター / リペラー
  - [ ] ボルテックス / タービュランス
  - [ ] オーディオドリブン
- [ ] コライダー GPU 実装
  - [ ] 平面
  - [ ] 球
  - [ ] メッシュ

### Phase 4: 最適化と制御
- [ ] CPU/GPU 自動切り替えロジック
  - パーティクル数 < 10,000: CPU
  - パーティクル数 > 10,000: GPU
- [ ] シミュレーション品質設定
- [ ] デバッグ表示 / 統計表示
- [ ] プリワーム機能 GPU 対応

### Phase 5: 拡張機能
- [ ] パーティクル - パーティクル衝突 (GPU ブロードフェーズ)
- [ ] 流体拘束 3D 対応
- [ ] スプライトアトラス対応
- [ ] メッシュパーティクル対応

---

## 📊 性能目標

| パーティクル数 | CPU のみ | GPU パス |
|---------------|----------|----------|
| 10,000 | 60fps | 60fps |
| 50,000 | 12fps | 60fps |
| 100,000 | 5fps | 60fps |
| 500,000 | 1fps | 55fps |
| 1,000,000 | ❌ 不可 | 45fps |

---

## ⚠️ 注意点

1. **既存実装を壊さない**ことが最優先
2. 全ての既存パーティクルプリセットが完全に同じ挙動で動作すること
3. シリアライズ互換性を維持
4. エクスプレッションによるパーティクル制御は引き続き動作すること

---

## 🔗 依存関係

✅ Diligent Engine の Compute Pipeline は既に動作しています
✅ 既存のレンダーパイプラインにそのまま統合可能
✅ 追加の外部依存は一切不要

---

## 🕒 実装見積もり

**全工程 72 時間程度**
- Phase 1: 8h
- Phase 2: 12h
- Phase 3: 24h
- Phase 4: 16h
- Phase 5: 12h

最小限動作版は Phase 2 完了時点で 20 時間で利用可能になります。
