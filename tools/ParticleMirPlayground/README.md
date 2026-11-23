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
    ridge の両端を含む 64 点に Point エミッターを置いて分布を均す。
    `ArtifactParticleLayer` を headless GPU で描画する。
    各エミッターは density 0.18 / radius 26 / 最大 3,000 粒子。
    ridge は画面高の 72% を基準に置き、地形高低差をそのまま反映して
    上方へ立ち上がる煙柱を確認する。
  - 3D キャプチャ: 地形メッシュ全体を 3D 合成座標系（Y up, Z = depth）に写し、
    `ArtifactParticle3DLayer` の `EmitterShape::Surface` 放出ソースにする。
    地形本体は `drawMesh`、粒子は `draw` で同じフレームに描く。
    カメラは注視点を地形面へ寄せ画角を広め、地形が画面下部を埋める構図にする。
    2 倍解像度で描画してからダウンサンプルして遠景稜線の aliasing を抑える。
  - 各キャプチャは PNG と `*_report.txt`（可視ピクセル数、差分ピクセル数、
    決定性許容差との比較、シミュレーション空間の XY 範囲）を書き出す。
    契約違反があれば非ゼロ終了する。

## ビルド

ルートプロジェクトを configure 済みのビルドツリーから追加ターゲットをビルドする。

```powershell
cmake --build <build-directory> --config Debug --target ArtifactParticleMirPlayground
```

## 実行

```powershell
<build-directory>\bin\Debug\ArtifactParticleMirPlayground.exe --capture-2d <output-directory>
<build-directory>\bin\Debug\ArtifactParticleMirPlayground.exe --capture-3d <output-directory>
<build-directory>\bin\Debug\ArtifactParticleMirPlayground.exe --capture-2d-software <output-directory>
```

2D 出力: `mir_2d_frame_45.png`、`mir_2d_frame_60.png`、
`mir_2d_direct_seek_frame_45.png`、`mir_particle_2d_report.txt`。

3D 出力: `mir_3d_front_frame_45.png`、`mir_3d_orbit_frame_45.png`、
`mir_3d_frame_60.png`、`mir_particle_3d_report.txt`。

`--capture-2d-software` は同一シーンを GPU 経路とソフトフォールバック経路の
両方で取得して比較する（`mir_2d_gpu_frame_45.png` と
`mir_2d_software_frame_45.png`、`mir_particle_2d_software_report.txt`）。

## 判定内容

- 描画が空でない（可視ピクセル > 0）。
- フレーム 45 → 60 で差分ピクセル ≥ 128（アニメーションが動いている）。
- 3D のカメラ軌道 35 度で差分ピクセル ≥ 128（視点に追従している）。
- 2D の連続再生フレーム 45 と直接シークフレーム 45 の差分が
  4096 ピクセル / 24 チャンネル差以内（決定性が保たれている）。

## バックエンド切替と環境間比較

バックエンドは `ARTIFACT_RENDER_BACKEND` で切り替える（`d3d12` / `vulkan` / 未指定で auto）。

```powershell
$env:ARTIFACT_RENDER_BACKEND = 'vulkan'
<build-directory>\bin\Debug\ArtifactParticleMirPlayground.exe --capture-3d <output-directory>
```

D3D12 と Vulkan の同一キャプチャを画素単位で比較する補助スクリプトを用意している。
閾値は「差分画素 ≤ 1.0%、最大チャンネル差 ≤ 16」。

```powershell
python tools/ParticleMirPlayground/compare_backends.py <d3d12-dir> <vulkan-dir> <output-json>
```

2026-11-19 実測: 2D は最大 2 チャンネル差、3D は最大 11 チャンネル差で、
いずれも許容範囲内。Vulkan 版の画像認識採点も PASS（詳細は
`captures/mir-particle-20261119-v16/IMAGE_EVALUATION_REPORT.md`）。

## 解析補助スクリプト

| スクリプト | 用途 |
|---|---|
| `compare_backends.py` | 2 ディレクトリ（例: D3D12 と Vulkan）の 6 画像を画素差で比較し JSON 出力 |
| `compare_crop.py` | 指定領域の被覆率と連続 run 長（粒径近似）を 2 画像で比較 |
| `measure_blobs.py` | 全体の被覆率・run 数・中央 run 長を計測 |
| `profile_region.py` | 領域内の単離ブローブを alpha プロファイルで solid / hollow / flat に分類 |
| `profile_luma.py` | 同上を輝度基準で分類（背景不透明な GPU キャプチャ用） |
| `profile_blob.py` | 指定座標の 1 行プロファイルを出力 |
| `find_blob_profile.py` | 単離ブローブを自動探索してプロファイルを出力 |

## ビルド前提（注意）

本プロジェクトは `ArtifactAppRuntime` にリンクするため、`Artifact` 子リポジトリの
ビルドが可能である必要がある。2026-11-21 時点で `Artifact` の HEAD には
ビルド不能な翻訳単位が 3 件あり（`ArtifactPythonAPI.cpp` の型不一致、
`ArtifactLayerPanelWidget.cpp` の未実装 API 参照、
`ArtifactShapeLayer.cpp` の MSVC 前方参照不備）、親リポジトリ側の
`ARTIFACT_DISABLE_BROKEN_PYTHON_API` オプションと `ARTIFACT_BUILD_TESTS=OFF`
で自前ターゲットのみを通す運用をしている。