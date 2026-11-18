**最終更新:** 2026-11-19

# Artifact Mir Particle Playground

Mir 風の手続き地形を粒子放出ソースとして使う単体テストプロジェクト。
`ArtifactCore::Procedural3DGenerators` の地形生成結果を
2D では ridge 上に並べた Point エミッターの位置に、3D では
`ParticleEmitter::setEmissionTriangles()` に渡すことで、地形表面から煙／火の粉が
立ち上がる表現を両経路でヘッドレスにキャプチャする。

- プロジェクトは開かない（`ArtifactStudio` の composition を読み書きしない）。
- Qt は offscreen プラットフォームで動作するため、デスクトップセッションは不要。
- 評価計画は [EVALUATION_PLAN.md](EVALUATION_PLAN.md) に固定済み。

## 構成

- `main.cpp`
  - 2D キャプチャ: Mir 地形の中央行から ridge プロファイルを取り出し、
    ridge の両端を含む 48 点に Point エミッターを置いて分布を均す。
    `ArtifactParticleLayer` を headless GPU で描画する。
    各エミッターは density 0.42 / rise 4.6 / radius 26 / 最大 4,000 粒子。
  - 3D キャプチャ: 地形メッシュ全体を 3D 合成座標系（Y up, Z = depth）に写し、
    `ArtifactParticle3DLayer` の `EmitterShape::Surface` 放出ソースにする。
    地形本体は `drawMesh`、粒子は `draw` で同じフレームに描く。
    カメラは距離を詰めて構図を詰め、2 倍解像度で描画してからダウンサンプルして
    遠景稜線の aliasing を抑える。
  - 各キャプチャは PNG と `*_report.txt`（可視ピクセル数、差分ピクセル数、
    決定性許容差との比較）を書き出す。契約違反があれば非ゼロ終了する。

## ビルド

ルートプロジェクトを configure 済みのビルドツリーから追加ターゲットをビルドする。

```powershell
cmake --build <build-directory> --config Debug --target ArtifactParticleMirPlayground
```

## 実行

```powershell
<build-directory>\bin\Debug\ArtifactParticleMirPlayground.exe --capture-2d <output-directory>
<build-directory>\bin\Debug\ArtifactParticleMirPlayground.exe --capture-3d <output-directory>
```

2D 出力: `mir_2d_frame_45.png`、`mir_2d_frame_60.png`、
`mir_2d_direct_seek_frame_45.png`、`mir_particle_2d_report.txt`。

3D 出力: `mir_3d_front_frame_45.png`、`mir_3d_orbit_frame_45.png`、
`mir_3d_frame_60.png`、`mir_particle_3d_report.txt`。

## 判定内容

- 描画が空でない（可視ピクセル > 0）。
- フレーム 45 → 60 で差分ピクセル ≥ 128（アニメーションが動いている）。
- 3D のカメラ軌道 35 度で差分ピクセル ≥ 128（視点に追従している）。
- 2D の連続再生フレーム 45 と直接シークフレーム 45 の差分が
  4096 ピクセル / 24 チャンネル差以内（決定性が保たれている）。
