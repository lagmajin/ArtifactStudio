# ゲームエンジン 3D レンダリング技法 参照ガイド

> ArtifactStudio の 3D コンポジットパイプラインに応用可能なゲームエンジンのレンダリング技法
> 参照プロジェクトのライセンスは各リポジトリを確認

---

## 参照プロジェクト一覧

| プロジェクト | ⭐ | ライセンス | API | 何が強いか |
|---|---|---|---|---|
| [Filament (Google)](https://github.com/google/filament) | 20.3k | Apache 2.0 | Vulkan/Metal/GL | **PBR + ポスプロ設計の教科書** |
| [Godot Engine](https://github.com/godotengine/godot) | 114k | MIT | Vulkan/GL | レンダリングサーバー抽象、SSAO/SSIL/VolumetricFog |
| [Wicked Engine](https://github.com/turanszkij/WickedEngine) | 7.1k | MIT | **DX12**/Vulkan/Metal | **DX12 ネイティブ実装**、GPUドリブンレンダリング |
| [bgfx](https://github.com/bkaradzic/bgfx) | 17.3k | **BSD-2** | DX9/11/12/GL/Metal/Vulkan | バックエンド抽象、examples が豊富 |
| [The Forge](https://github.com/ConfettiFX/The-Forge) | 5.6k | Apache 2.0 | DX12/Vulkan/Metal | **レイトレーシング含む全API抽象** |

---

## 1. ポストプロセススタック設計

### Filament の設計（最も完成度が高い）

```
ColorGrading → ToneMapping → Bloom → DOF → TAA → 最終出力
     ↑            ↑          ↑       ↑      ↑
   LUT適用     ACES/Filmic  階層的    CoC     履歴バッファ
```

**ArtifactStudio への示唆**:
- パス間の依存関係を明示的に管理するグラフ構造
- `filament/src/PostProcessManager.cpp` がポスプロの全パイプラインを定義
- 各パスは独立したシェーダー + render target

### Godot の設計

```
source: servers/rendering/renderer_rd/
├── effects/
│   ├── ssao.cpp          ← SSAO + SSIL
│   ├── ss_effects.cpp    ← SSR
│   ├── bokeh_dof.cpp     ← ボケDOF
│   ├── glow.cpp          ← ブルーム
│   ├── volumetric_fog.cpp ← ボリュメトリックフォグ
│   └── taa.cpp           ← TAA
└── environment/
    └── sky.cpp           ← 物理空
```

### bgfx の examples

```
examples/
├── 09-hdr/        ← HDR + ブルーム + トーンマッピング
├── 13-stencil/     ← ステンシャルシャドウ
├── 16-shadowmaps/  ← シャドウマップ各種
├── 27-terrain/     ← テレイン
└── 37-gpudrivenrendering/ ← GPUドリブン
```

---

## 2. 各技法の ArtifactStudio 応用先

### 🌟 ブルーム (Bloom)

| 参照元 | 手法 | ArtifactStudio 応用 |
|---|---|---|
| Filament | ダウンサンプルピラミッド + アップサンプル (Kawase/Karis) | GPU ブラー基盤として既存のガウシアンより品質が高い |
| Godot `glow.cpp` | 多段階ダウンサンプル + 閾値抽出 + アップサンプル合成 | カメラエフェクトとして自然 |
| bgfx `09-hdr` | 輝度抽出 → ブラー → 加算合成の最小構成 | 参照実装として最もシンプル |

### 🌟 被写界深度 (DOF)

| 参照元 | 手法 | ArtifactStudio 応用 |
|---|---|---|
| Filament | CoC (Circle of Confusion) + 分離フィルタ（手前/奥） | 物理ベースDOF。ArtifactStudio の深度合成に直接応用可 |
| Godot `bokeh_dof.cpp` | ボケ形状カスタマイズ | OpenToonz の BokehFx と同様のアプローチ |

### 🌟 アンビエントオクルージョン (SSAO)

| 参照元 | 手法 | ArtifactStudio 応用 |
|---|---|---|
| Godot `ssao.cpp` | GTAO (Ground Truth AO) + SSIL | 3D レイヤー間の陰影による奥行き感 |
| Filament | SSAO (深度バッファベース) | 深度合成のクオリティ向上 |

### 🌟 トーンマッピング

| 参照元 | 手法 |
|---|---|
| Filament | ACES, Filmic, Linear, Reinhard など複数実装 |
| bgfx `09-hdr` | シンプルな Reinhard + 露出制御 |

### 🌟 シャドウ

| 参照元 | 手法 | ArtifactStudio 応用 |
|---|---|---|
| bgfx `16-shadowmaps` | PCF, VSM, ESM の各種シャドウマップ | 3Dレイヤーへのスポットライト/ポイントライト影 |
| Wicked Engine | レイトレーシングシャドウ (DXR) | 高品質影 (将来) |

### 🌟 GPU ドリブンレンダリング

| 参照元 | 手法 |
|---|---|
| Wicked Engine | GPU カリング、間接描画、Bindless |
| bgfx `37-gpudrivenrendering` | GPU オクルージョンカリング |

### 🌟 スクリーンスペース反射 (SSR)

| 参照元 | 手法 | ArtifactStudio 応用 |
|---|---|---|
| Godot `ss_effects.cpp` | レイマーチングSSR | 3Dレイヤーの反射 |
| The Forge | Hi-Z トレースSSR | |

---

## 3. バックエンド抽象化パターン

### bgfx のマルチバックエンド設計（ArtifactStudio に最も近い）

```
include/bgfx/bgfx.h         ← 公開API（バックエンド非依存）
src/
├── renderer_d3d11.cpp       ← DX11
├── renderer_d3d12.cpp       ← DX12
├── renderer_vk.cpp          ← Vulkan
├── renderer_gl.cpp          ← OpenGL
├── renderer_mtl.mm          ← Metal
└── renderer_gnm.cpp         ← PS4
```

ArtifactStudio は既に DiligentEngine でこの層をカバーしているため、
**DiligentEngine 上でのパイプライン構築パターン**として bgfx examples が参考になる。

### The Forge のリソース管理

```
Common_3/Graphics/
├── ResourceLoader.cpp       ← 全バックエンド共通ローダー
├── Interfaces/              ← 抽象インターフェース
└── ThirdParty/OpenSource/   ← 各バックエンド実装
```

---

## 4. 即参考にできるコード断片

### Filament: ブルームダウンサンプルパイプライン

```cpp
// filament/src/PostProcessManager.cpp より疑似コード
void PostProcessManager::bloom(FrameGraph& fg, TextureHandle input) {
    // 閾値抽出パス
    auto threshold = bloomThresholdPass(fg, input);
    // ダウンサンプルピラミッド (最大6段階)
    for (int i = 0; i < 6; i++) {
        downsample[i] = downsamplePass(fg, (i==0) ? threshold : downsample[i-1]);
    }
    // アップサンプル + 累積合成
    for (int i = 5; i >= 0; i--) {
        upsample[i] = upsamplePass(fg, downsample[i], upsample[i+1]);
    }
    // 合成
    compositePass(fg, input, upsample[0]);
}
```

### bgfx: シンプルな HDR → ブルーム → トーンマッピング

```cpp
// examples/09-hdr/hdr.cpp
// 1. シーンを HDR レンダーターゲットに描画
// 2. 輝度抽出: compute shader で平均輝度計算
// 3. ブルーム: 輝度テクスチャ → ダウンサンプル → ブラー → アップサンプル
// 4. トーンマッピング: 元画像 + ブルームを合成してトーンマップ
```

---

## 5. 実装優先順位（3Dレンダリング編）

| 順位 | 技法 | 参照元 | 難度 | ArtifactStudio での位置づけ |
|---|---|---|---|---|
| 1 | HDR ブルーム | bgfx 09-hdr / Filament | 低-中 | カメラエフェクト基盤 |
| 2 | トーンマッピング (ACES) | Filament | 低 | HDR→SDRの標準化 |
| 3 | DOF (CoCベース) | Filament / Godot | 中 | 深度合成に直結 |
| 4 | SSAO | Godot GTAO | 中-高 | 3Dレイヤーの奥行き表現 |
| 5 | シャドウマップ | bgfx 16-shadowmaps | 中 | 3Dライトの影 |
| 6 | SSR | Godot / The Forge | 高 | 反射表現 |
| 7 | ボリュメトリックフォグ | Godot | 高 | 雰囲気効果 |

---

## 6. ライセンス互換性

| プロジェクト | ライセンス | ArtifactStudio 組み込み |
|---|---|---|
| bgfx | **BSD-2-Clause** | ✅ 最も緩い。コード断片の移植も可 |
| Godot | MIT | ✅ 商用利用自由、表示のみ |
| Filament | Apache 2.0 | ✅ 商用利用自由、特許条項あり |
| Wicked Engine | MIT | ✅ |
| The Forge | Apache 2.0 | ✅ |

**推奨**: bgfx の `examples/` を実装パターン集として、Filament を設計リファレンスとして参照。
