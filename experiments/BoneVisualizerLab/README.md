# BoneVisualizerLab (3D Bone & Skeleton Viewport Experiment)

本プロジェクトは、**ArtifactStudio の Composition Viewport (VP) に 3D モデルのボーン表示機能（DCCライクな八面体ボーン & 深度制御）を実装するための独立プロトタイプ・実験環境**です。

---

## 目的・検証項目

1. **FBX スケルトン / ボーン階層のロード**
   * `third_party/ufbx` を使用し、FBX 内のボーンノード階層・トランスフォーム（バインドポーズ・現在のポーズ）を取得。
   * ルートから関節（Joint Head / Tail）への親子関係、ボーン長、およびローカル座標軸（XYZ）を構築。

2. **DCCライクなボーン幾何形状（Octahedral Bone）の生成**
   * 単純な直線（Stick）だけでなく、Maya / Blender / 3ds Max 等の DCC ツールで標準的な**八面体（Octahedral / 菱形コーン）ボーンメッシュ**を動的生成。
   * 関節球（Joint Sphere）およびローカル XYZ ギズモ軸の重畳表示。

3. **深度制御（Depth Control）パイプラインの検証**
   * **Normal (通常遮蔽)**: `GL_DEPTH_TEST` + `GL_LEQUAL` により、キャラクタモデルメッシュの内側にあるボーンは自然に隠れる。
   * **X-Ray (常時最前面)**: 深度テストをバイパスし、モデルメッシュの表面に常にボーンが重畳表示される。
   * **DCC Ghosting (Dual-Pass 遮蔽ゴースト表現)**:
     * **Pass 1 (遮蔽部分)**: `GL_GREATER` でテストし、メッシュで隠れているボーン部分を半透明（ゴースト）／薄色で描画。
     * **Pass 2 (可視部分)**: `GL_LEQUAL` でテストし、メッシュの外側や手前にあるボーン部分を鮮明なソリッド／ハイライトで描画。
     * これにより、モデルの内側にある骨格の奥行き感・重なり関係を把握可能。

---

## ビルド & 実行方法

### 必要環境
* Windows x64 (MSVC / Clang)
* CMake 3.28+ & Ninja
* Qt 6 (Core, Gui, Widgets, OpenGL, OpenGLWidgets)

### ビルド手順
```bash
cmake -S experiments/BoneVisualizerLab -B build/BoneVisualizerLab -DCMAKE_PREFIX_PATH="C:/vcpkg/installed/x64-windows" -G "Ninja"
ninja -C build/BoneVisualizerLab
```

### 実行
```bash
./build/BoneVisualizerLab/BoneVisualizerLab.exe
```

---

## 操作方法
* **ドラッグ (左クリック)**: カメラの回転（Orbit）
* **ドラッグ (右クリック / 中クリック)**: カメラの平行移動（Pan）
* **マウスホイール**: ズーム（Zoom）
* **右側パネル**:
  * `Open FBX File...`: 任意の FBX ファイルを読み込み
  * `Load Biped Humanoid Rig`: 内蔵の標準二足歩行スケルトンを読み込み
  * `Depth Display Mode`: Normal / X-Ray / DCC Ghosting の切り替え
  * `Ghost Opacity`: 隠れボーンの透過度スライダー
  * `Bone Geometry`: Octahedral Solid / Wireframe / Solid + Wire などの切り替え
  * `Bone Thickness`: ボーンの太さ調整
  * `Show Joint Spheres / Show Local Joint Axes`: 関節球・XYZ軸の表示トグル
  * `Show Character Proxy Mesh`: 深度テスト用の半透明キャラクタプロキシメッシュの表示トグル
