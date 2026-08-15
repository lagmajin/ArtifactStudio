# Production Volume / Voxel Pipeline (2026-08-08)

**最終更新:** 2026-08-15
**状態:** Core 参照実装あり / Artifact 統合未着手

## 概要

ArtifactStudio のボリューム・ボクセル対応状況は、`ArtifactCore` に CPU レイマーチング、メッシュ→密度場、Pyro/OpenVDB の参照実装がある一方、Artifact アプリケーション側の専用レイヤー／ビューポート接続は未確認である。まず既存資産の接続契約を固め、その後 GPU 化へ進む。

## 現行コード監査 (2026-08-15)

`ArtifactCore/src/Render/VolumeRenderer.cppm` には三線形サンプリング、transfer function、ライト遮蔽、位相関数、複数散乱近似、DOF を含む CPU renderer が存在する。`MeshToVolume`、`OpenVDBVolumeReference`、`PyroSimulation` も Core 側の独立実装として確認できる。一方、`ArtifactVolumeLayer` は存在せず、`CompositionRenderController`／`ArtifactIRenderer` から `CPUVolumeRenderer::render()` を呼ぶ経路、GPU volume texture／raymarch pass、NanoVDB 接続は確認できない。したがって現段階は「Core の参照実装が充実」までで、Phase 1 のアプリ統合は未着手と判定する。

## Update 2026-08-15

- `CPUVolumeRenderer` の三線形サンプル、transfer function、照明／遮蔽、位相関数、複数散乱近似、DOF と、`MeshToVolume`／OpenVDB／Pyro の Core 実装を再確認。
- Artifact 側には専用 `ArtifactVolumeLayer`、Composition／IRenderer からの `CPUVolumeRenderer::render()` 接続、Pyro／OpenVDB の end-to-end volume path、GPU volume texture／raymarch pass、NanoVDB 接続は確認できない。
- 判定は Core reference implementation まで。Phase 1 のアプリ統合、GPU 化、制作 UI、runtime acceptance は未着手または未検証。

## 現状監査：実装済みモジュール

### ArtifactCore 側（参照実装・独立動作）

| モジュール | ファイル | 行数(推定) | 実装内容 |
|-----------|---------|-----------|---------|
| **CPUVolumeRenderer** | `ArtifactCore/src/Render/VolumeRenderer.cppm` | ~420行 | 完全なレイマーチングボリュームレンダラー |
| **MeshToVolume** | `ArtifactCore/src/Render/MeshToVolume.cppm` | ~300行 | ポリゴンメッシュ→ボクセル密度場変換 |
| **OpenVDB I/O** | `ArtifactCore/src/Simulation/OpenVDBVolumeReference.cppm` | ~200行 | `.vdb` 読み込み・グリッド抽出 |
| **PyroSimulation** | `ArtifactCore/src/Simulation/PyroSimulation.cppm` | ~1000行 | 3D 流体シミュレーション |
| **SDFLayer** | `Artifact/src/Layer/ArtifactSDFLayer.cppm` | ~650行 | SDF プリミティブの CPU レイマーチング |

### CPUVolumeRenderer の機能（全実装済み）

| 機能 | 状態 | 詳細 |
|------|------|------|
| トライリニアサンプリング | ✅ | `VolumeScalarField::sampleTrilinear()` |
| 伝達関数 (Transfer Function) | ✅ | Fire / Smoke / Steam / Explosion / Custom callback |
| ボリューメトリックライト | ✅ | Point / Spot（コーン角・減衰・ソフトネス） |
| 位相関数 | ✅ | Henyey-Greenstein（異方性散乱） |
| 多重散乱近似 | ✅ | Octaveベースの周波数分離散乱 |
| シャドウレイマーチング | ✅ | ライト方向の自己遮蔽計算 |
| AABB レイ交差 | ✅ | Slab method |
| 被写界深度 (DOF) | ✅ | Golden Angle サンプリングのレンズDOF |
| カスタムレンダー設定 | ✅ | stepSize, maxSteps, densityScale, temperatureEmission, showVelocity |

### Artifact アプリケーション側の統合状況

| 接続点 | 状態 |
|--------|------|
| `Artifact3DLayer` → ボリュームモード | ❌ 未実装。メッシュのみ |
| `ArtifactVolumeLayer`（専用レイヤー） | ❌ 不存在 |
| `CPUVolumeRenderer` → ビューポート描画 | ❌ 未接続 |
| Pyro → VolumeFieldSet → CPUVolumeRenderer | ❌ パイプライン不在 |
| OpenVDB → VolumeFieldSet → CPUVolumeRenderer | ❌ パイプライン不在 |
| SDFLayer → 3D 出力 | ⚠️ 2D QImageにレンダリングしてスプライト合成のみ |

---

## ギャップ詳細

### Gap 1: ボリュームレイヤーが存在しない 🔴

`Artifact3DLayer` はメッシュレイヤー。`ArtifactSDFLayer` は SDF レイヤー。ボリューム専用のレイヤーがないため、以下の情報を保持できない:
- ボリュームデータのソース（OpenVDBファイル / Pyroシミュレーション / MeshToVolume）
- 伝達関数設定（Fire/Smoke/Steam/Explosion）
- レンダー設定（stepSize, maxSteps, densityScale）
- ボリュームライト（Point, Spot の配置とパラメータ）

### Gap 2: CPUVolumeRenderer がアプリに接続されていない 🔴

`CPUVolumeRenderer` は完全に機能する参照実装だが、`Artifact3DLayer::draw()` も `CompositionRenderController` もこれを呼び出さない。`render(int width, int height)` は独立した `ImageBuffer` を返すのみ。

### Gap 3: エンドツーエンドパイプラインがない 🔴

```
[✅] OpenVDB読み込み → PyroFieldSet
[✅] PyroFieldSet → シミュレーション
[❌] PyroFieldSet → VolumeFieldSet → CPUVolumeRenderer → ビューポート
[❌] OpenVDB → VolumeFieldSet → CPUVolumeRenderer → ビューポート
[❌] Mesh → MeshToVolume → VolumeFieldSet → CPUVolumeRenderer → ビューポート
```

### Gap 4: GPU 化が未着手 🟡

全ボリューム計算が CPU。レイマーチング、トライリニアサンプリング、シャドウキャスト、散乱計算はすべて GPU コンピュートシェーダーとの相性が極めて良い。DiligentEngine の DXR（DirectX Raytracing）を使えば本格的な GPU ボリュームレンダリングが可能。

### Gap 5: NanoVDB / スパースボリュームがない 🟡

OpenVDB の密グリッド（数百MBになることも）をそのまま GPU にアップロードするのは非現実的。NanoVDB（NVIDIA の GPU 向けスパースボリュームフォーマット）でメモリ効率を 10〜100x 改善する必要がある。

### Gap 6: SDFLayer が 2D フラット化されている 🟢

`ArtifactSDFLayer` は自前の CPU レイマーチングで QImage に書き出し、それを `drawSpriteTransformed()` で描画している。正しい 3D 空間でのレンダリングではなく、実質 2D スプライト。

---

## Phase 一覧

### Phase 1: Volume Layer 新設 + CPUVolumeRenderer 統合 🔴

**目的**: `Artifact3DLayer` とは別の `ArtifactVolumeLayer` を作成し、CPUVolumeRenderer をビューポートに接続する。

```
ArtifactVolumeLayer プロパティ:
  - sourceType: OpenVDB | PyroSimulation | MeshToVolume
  - sourcePath: .vdb ファイルパス
  - transferFunctionKind: Fire | Smoke | Steam | Explosion | Custom
  - densityScale, stepSize, maxSteps
  - volumetricLights[]: Point/Spot ライト配列

draw(renderer):
  1. sourceType に応じて VolumeFieldSet を構築
  2. CPUVolumeRenderer に投入
  3. render(width, height) → ImageBuffer
  4. ImageBuffer → QImage → renderer->drawSpriteTransformed()
```

**ファイル**:
- `Artifact/include/Layer/ArtifactVolumeLayer.ixx`（新規）
- `Artifact/src/Layer/ArtifactVolumeLayer.cppm`（新規）
- `Artifact/src/Layer/ArtifactLayerFactory.cppm`（登録追加）
- `Artifact/src/Layer/ArtifactLayerInitParams.cppm`（型追加）

**コスト**: 中（新規レイヤー + 3つのデータソースパイプ接続）

---

### Phase 2: Pyro ↔ Volume パイプライン 🔴

**目的**: PyroSimulation の出力を VolumeLayer に流し込み、シミュレーション結果をレンダリング可能にする。

```
PyroSimulation::step(dt)
  → PyroFieldSet（全グリッドデータ）
  → convertToVolumeFieldSet()（新規ユーティリティ）
    → VolumeScalarField density
    → VolumeScalarField temperature
    → VolumeVectorField velocity（オプション）
  → VolumeLayer::setFieldSet()
  → CPUVolumeRenderer::render()
```

**変換マッピング**:

| PyroFieldSet | VolumeFieldSet |
|-------------|---------------|
| density grid | `fields.density` (float) |
| temperature grid | `fields.temperature` (float) |
| velocity grid | `fields.velocity` (Vec3) |
| resolution | `VolumeResolution` |
| domain bounds | `VolumeAABB` |

**ファイル**:
- `ArtifactCore/src/Simulation/PyroToVolume.cppm`（新規・変換ユーティリティ）
- `Artifact/src/Layer/ArtifactVolumeLayer.cppm`（Pyro ソース対応）

**コスト**: 低〜中（変換ブリッジの実装のみ）

---

### Phase 3: OpenVDB 直接読み込みパイプライン

**目的**: `.vdb` ファイルを VolumeLayer に直接読み込み、Houdini/EmberGen 等の出力をレンダリング可能にする。

```
OpenVDBVolumeReference(path, densityGrid, temperatureGrid)
  → loadOpenVDBDensitySnapshot()
  → OpenVDBScalarSnapshot（width, height, depth, values[]）
  → VolumeScalarField（コピー）
  → VolumeLayer::setFieldSet()
```

**ファイル**:
- `Artifact/src/Layer/ArtifactVolumeLayer.cppm`（OpenVDB ソース対応）

**コスト**: 低（既存の OpenVDB I/O の薄いラッパー）

---

### Phase 4: MeshToVolume パイプライン

**目的**: 任意の `Artifact3DLayer` のメッシュをボクセル化し、VolumeLayer としてレンダリング可能にする。

```
Artifact3DLayer::mesh()
  → extractTriangles()（頂点 → SimpleTriangle[]）
  → MeshToVolumeConverter::convertToScalarField(bounds, field)
  → VolumeLayer::setFieldSet()
  → CPUVolumeRenderer::render()
```

**ファイル**:
- `Artifact/src/Layer/ArtifactVolumeLayer.cppm`（MeshToVolume ソース対応）
- `ArtifactCore/src/Render/MeshToVolume.cppm`（三角形抽出ユーティリティ追加）

**コスト**: 中（Artifact3DLayer の Mesh データを MeshToVolume の SimpleTriangle 形式に変換するブリッジ）

---

### Phase 5: GPU ボリュームレイマーチング（Diligent DXR / Compute Shader）

**目的**: 全 CPU レイマーチングを GPU 化し、リアルタイムボリュームレンダリングを可能にする。

**アプローチ A: コンピュートシェーダー（推奨）**
- `raymarch()` の全ループを1つのコンピュートシェーダーに移植
- 3D テクスチャとしてボリュームデータを Diligent にアップロード
- 8×8 スレッドグループでレイマーチング（キャッシュ局所性が高い）
- 伝達関数は 1D ルックアップテクスチャに焼き込み

**アプローチ B: DXR レイトレーシング**
- AABB との交差は DXR の `TraceRay()` でハードウェア加速
- ボリューム内のサンプリングは自作（DXR はボリューム専用の交差判定を持たない）
- 利点: シャドウレイをハードウェアで投げられる
- 欠点: DXR 対応 GPU 必須。コンピュートシェーダーより制約が多い

**NanoVDB 統合**:
- ボリュームデータの GPU アップロード時に NanoVDB エンコード
- スパースアクセラレーション構造（ツリー）で空領域をスキップ
- VRAM 使用量 10〜100x 削減

**ファイル**:
- `Artifact/App/shaders/volume_raymarch_cs.hlsl`（新規）
- `Artifact/src/Render/ArtifactVolumeRenderer.cppm`（新規・GPU版）
- `third_party/NanoVDB/`（新規・ヘッダオンリー）
- `ArtifactCore/src/Render/VolumeRenderer.cppm`（NanoVDB アップロード追加）

**コスト**: 高（コンピュートシェーダー + NanoVDB + データ転送）

---

### Phase 6: Volume Layer の Property UI + プリセット

**目的**: ボリュームレイヤーの伝達関数・ライト・レンダー設定を Property Widget で編集可能にする。

**プロパティグループ**:
- **Source**: type, filePath, frame
- **Transfer Function**: kind, densityScale, temperatureScale, カスタムカーブ
- **Render**: stepSize, maxSteps, phaseAnisotropy, multipleScatteringStrength
- **Lights**: 配列（追加/削除/編集）、各ライトの type/position/intensity/range/cone
- **Display**: showVelocity, backgroundColor

**コスト**: 中（PropertyGroup 追加 + カスタムカーブエディタ）

---

### Phase 7: NanoVDB スパースボリュームの完全サポート

**目的**: `PyroFieldSet` / `VolumeFieldSet` を NanoVDB として GPU にアップロードし、スパースアクセスで 256³ 以上のボリュームをリアルタイムに。

**仕様**:
- NanoVDB は単一ヘッダファイル。ビルド依存なし
- `VolumeScalarField` → `nanovdb::FloatGrid` 変換
- Diligent `ITexture3D` の代わりに `IShaderResourceBinding` でバッファ経由アクセス
- コンピュートシェーダー内で NanoVDB ツリーをトラバース

**コスト**: 中〜高（NanoVDB 統合 + シェーダーバインド変更）

---

## 優先順位

| Phase | 内容 | コスト | 効果 |
|-------|------|--------|------|
| 1 | VolumeLayer 新設 + CPU統合 | 中 | ボリュームが初めて見えるようになる |
| 2 | Pyro ↔ Volume パイプライン | 低〜中 | シミュレーション結果の可視化 |
| 3 | OpenVDB 直接読み込み | 低 | 外部DCCとの相互運用 |
| 4 | MeshToVolume パイプライン | 中 | メッシュ→ボリューム変換 |
| 5 | GPU ボリュームレイマーチング | 高 | リアルタイム性能 |
| 6 | Property UI + プリセット | 中 | アーティスト向け操作性 |
| 7 | NanoVDB スパースサポート | 中〜高 | 高解像度ボリューム対応 |

Phase 1〜3 までで OpenVDB の読み込みから表示までが動く。Phase 5 で実用速度になる。

## 変更対象ファイル一覧

| ファイル | Phase | 種別 |
|----------|-------|------|
| `Artifact/include/Layer/ArtifactVolumeLayer.ixx` | 1 | 新規 |
| `Artifact/src/Layer/ArtifactVolumeLayer.cppm` | 1,2,3,4,6 | 新規 |
| `Artifact/src/Layer/ArtifactLayerFactory.cppm` | 1 | 改変 |
| `Artifact/src/Layer/ArtifactLayerInitParams.cppm` | 1 | 改変 |
| `ArtifactCore/src/Simulation/PyroToVolume.cppm` | 2 | 新規 |
| `ArtifactCore/include/Simulation/PyroToVolume.ixx` | 2 | 新規 |
| `ArtifactCore/src/Render/MeshToVolume.cppm` | 4 | 改変 |
| `Artifact/src/Render/ArtifactVolumeRenderer.cppm` | 5 | 新規（GPU版） |
| `Artifact/include/Render/ArtifactVolumeRenderer.ixx` | 5 | 新規 |
| `Artifact/App/shaders/volume_raymarch_cs.hlsl` | 5 | 新規 |
| `Artifact/App/shaders/volume_nanovdb_access.hlsli` | 7 | 新規 |
| `third_party/NanoVDB/` | 5, 7 | 新規 |
