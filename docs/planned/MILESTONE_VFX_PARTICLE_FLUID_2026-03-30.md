# Milestone: Real-time Particle & Fluid Simulation (M-VFX-1)

## 🎯 目的
`DiligentEngine` による強力な GPU 計算能力を、モーショングラフィックスにおける視覚効果（VFX）へと直接結びつける。数十万個のパーティクルや、本格的な流体シミュレーション（煙、炎、液体）をリアルタイムで操作可能にし、レンダリング時間の劇的な短縮と表現の豊かさを両立させる。

## 🏗️ アーキテクチャ構成
1. **`ParticleComputeEngine`**: 
   - パーティクルの生成 (Emit)、更新 (Update)、描画 (Draw) を全て GPU (Compute Shader) 上で行う。
2. **`FluidSolver2D`**:
   - `Stable Fluids` アルゴリズムに基づくグリッドベースの流体シミュレーター。
3. **`VFXLayerInstance`**:
   - シミュレーションの設定を保持・編集し、レイヤーの親子付けやトランスフォームに追従させる機能。

## 📅 実装フェーズ

### Phase 1: GPU パーティクルエンジン (2026-04-15 - 2026-04-25)
- [ ] 数十万個の点/スプライトを並列更新する Compute Job の実装。
- [ ] 物理フィールド（重力、乱気流、引力）による粒子の軌道計算。
- [ ] カラーグラデーション、ライフタイムによるプロパティ変化。

### Phase 2: 2D 流体シミュレーター (2026-04-26 - 2026-05-05)
- [ ] `Pressure Solve` (圧力計算) を含む 2D グリッドソルバーの実装。
- [ ] レイヤー（テキストロゴ等）をソースとした、煙やインクの発生。
- [ ] ビューポート上でのマウス操作による、流体のかき混ぜ（Interaction）。

### Phase 3: レンダリング & 合成統合 (2026-05-06 - 2026-05-20)
- [ ] 合成パイプライン（Standard / Additive / Screen 等）への VFX 描画パスの挿入。
- [ ] 被写体（他レイヤー）との深度（Z-depth）やマスクによる切り抜き対応。
- [ ] ベイク（計算結果をファイルに保存して固定化）機能。

## 🚀 期待される成果
- 従来はオフラインレンダリングが必要だった高品質な視覚効果が、作成した瞬間に確認できるようになる。
- パーティクルとオーディオを連動させることで、没入感の高いオーディオビジュアライザーを実現。

## 🔗 関連マイルストーン
- [M-FX-5 GPU Effect Parity](MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md)
- [M-AU-3 Audio Visualization](MILESTONE_AUDIO_REACTOR_SYSTEM_2026-03-30.md) (オーディオ連携との相乗効果)

## 2026-07-25 実装監査

判定: GPU particle update と layer 内 FluidSolver2D 基盤は存在するが、マイルストーンの Phase 1〜3 を一貫した VFX 経路として完了した状態ではない。

- `Graphics.ParticleCompute` は structured buffer と compute shader で粒子の加速度・drag・noise・audio reactivity・位置更新を行う基盤を持つ。ただしエミット、GPU 側の寿命再生成、物理フィールド連携、カラー/ライフタイムの体系的な描画統合は別途未確認。
- `ArtifactAbstractLayer` の Fluid component は `FluidSolver2D`、密度/速度更新、viscosity / diffusion / buoyancy / vorticity / solverIterations の設定保存、低密度 preview particle 化まで実装されている。
- 一方、レイヤーを fluid source にする入力、ビューポート mouse interaction、GPU 2D pressure solve、fluid 用の専用描画パスは確認できない。`FluidVisualizer` は CPU buffer 向けの可視化基盤に留まる。
- Standard / Additive / Screen 等への専用 VFX 合成挿入、深度/マスク連携、計算結果の bake/export は未確認。
- 次の実装単位は、既存 GPU particle compute と layer/renderer の所有関係を確定し、Fluid component の preview 表示を実際の VFX render path へ接続すること。

ビルド・実行確認はリポジトリ方針により未実施。
