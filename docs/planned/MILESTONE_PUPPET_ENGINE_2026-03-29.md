# Milestone: OpenCV Puppet Engine (2026-03-29)

**Status:** Not Started (Stub Exists)
**Goal:** After Effectsライクなパペットピンツール（Puppet Pin Tool）を `ArtifactCore` に実装し、2D画像やレイヤーの自由変形（メッシュワープ）を可能にする。

---

## コンセプト

ユーザーが画像上の任意のポイントに「ピン」を打ち、そのピンを移動させることで周囲の画像がゴムのように自然に変形する機能。
内部的には画像のアルファチャンネルから輪郭を抽出し、ドロネー三角形分割（Delaunay Triangulation）等でメッシュを生成。ピンの移動に合わせてMoving Least Squares (MLS) や Thin Plate Spline (TPS) 、あるいは As-Rigid-As-Possible (ARAP) などのアルゴリズムを用いてメッシュを変形させる。

---

## 現状のコア実装状態

`ArtifactCore/include/ImageProcessing/OpenCV/OpenCVPuppetEngine.ixx` にインターフェースのスタブ（設計）のみが存在している状態です。

### 存在する構造
- `PuppetPin`: ピンのID、初期座標、現在の座標、ウェイトを持つ。
- `PuppetMesh`: 変形後の頂点座標、UV座標、インデックスリストを持つ。GPUレンダリング用。
- `PuppetDeformationMethod`: 以下の変形アルゴリズムの列挙。
  - `ThinPlateSpline` (TPS: スムーズな曲面変形)
  - `MovingLeastSquares` (MLS: 剛性を保ちやすい2D変形)
  - `ARAP` (As-Rigid-As-Possible: より高度な形状保持メッシュ変形)
- `OpenCVPuppetEngine` クラス:
  - `bindImage(Mat, detailLevel)`
  - `addPin()`, `removePin()`, `updatePinPosition()`
  - `renderDeformedImage(method)` (CPUワープ用)
  - `getDeformedMesh()` (GPUレンダリング用)

---

## 開発マイルストーン (Implementation Plan)

### Milestone 1: メッシュ生成 (Mesh Generation)
**目標**: 画像からアルファチャンネルを用いて輪郭を抽出し、操作用の三角形メッシュを構築する。

- **実装内容**:
  1. `bindImage` メソッドの実装。
  2. OpenCVの `findContours` を用いて、アルファ値が閾値以上の領域の輪郭を抽出する。
  3. 抽出した輪郭（および必要に応じて内部のグリッド点）を元に、OpenCVの `Subdiv2D` を使ってドロネー三角形分割（Delaunay Triangulation）を行い、初期メッシュ (`PuppetMesh` の初期状態) を生成する。
  4. `detailLevel` パラメータに応じてメッシュの細かさ（頂点数）を制御できるようにする。

### Milestone 2: ピン管理と CPU 変形 (Pin Management & Deformation)
**目標**: ピンの追加・移動と、それに基づくCPU側での画像変形処理を実装する。

- **実装内容**:
  1. ピンの追加/削除/更新メソッドの実装。
  2. `Moving Least Squares (MLS)` 変形アルゴリズムの実装（アフィン変換ベース）。ピンの移動量から各頂点の新しい座標を計算する。
  3. `Thin Plate Spline (TPS)` の実装（OpenCVの `ShapeDistanceExtractor` 等を使わず、自前でTPSの重み計算を実装するか、既存の外部ライブラリを検討）。
  4. `renderDeformedImage()` メソッドの実装。計算された変形後メッシュを使用して、OpenCVの `remap` を用いて画像をCPU上で変形する。

### Milestone 3: GPU レンダリング連携 (GPU Mesh Output)
**目標**: CPUでの変形は重いため、変形後のメッシュ頂点情報のみを返し、レンダラ側（Diligent Engine等）でハードウェア・テクスチャマッピングで描画できるようにする。

- **実装内容**:
  1. `getDeformedMesh()` メソッドの実装。
  2. メッシュの頂点座標とUV座標、インデックスバッファのデータを `PuppetMesh` 構造体として正しく構築して返す。
  3. `ArtifactAbstractLayer` またはレンダリングパイプラインと連携し、このメッシュを使用してレイヤーを描画するパスを追加する。

### Milestone 4: 高度な変形アルゴリズム (ARAP / Advanced Mechanics)
**目標**: より自然で、折り曲げ時に画像が潰れにくい ARAP (As-Rigid-As-Possible) アルゴリズムの実装。

- **実装内容**:
  1. ARAPアルゴリズムの実装（最適化問題の反復ソルバが必要）。Eigenライブラリなどを `ArtifactCore` に導入するか、独自ソルバを実装する。
  2. ピンに対する剛性（Starch）パラメータのサポート。

---

## 実装の見積もり

| マイルストーン | 推定工数 | 依存関係 |
|---|---|---|
| **M1: メッシュ生成** | 6h | なし |
| **M2: CPU ワープ (MLS/TPS)** | 10h | M1 完了 |
| **M3: GPU メッシュ連携** | 8h | M2 完了, Artifact レンダラ群 |
| **M4: ARAP アルゴリズム** | 16h | M2 完了, (Eigen等の行列ライブラリ追加の可能性) |

**合計: 約 40h**

## 結論

**実装は十分に可能です。** 
すでに `AbstractCore` 内に `OpenCVPuppetEngine.ixx` という名称でスタブが用意されており、OpenCV をベースにしたアーキテクチャが想定されています。

実装を進める場合、まずはコア部分である**「M1: メッシュ生成」**および**「M2: 移動最小二乗法 (Moving Least Squares) によるワープ処理」**の C++ 実装 (*OpenCVPuppetEngine.cppm*) から着手するのが最も確実なルートになります。
