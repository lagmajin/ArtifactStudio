# LayerMask クラス分析レポート

**対象クラス**: `LayerMask`  
**モジュール**: `Artifact.Mask.LayerMask`  
**ファイル**: 
- 宣言: `Artifact/include/Mask/LayerMask.ixx`
- 実装: `Artifact/src/Mask/LayerMask.cppm`
- 依存: `Artifact/include/Mask/MaskPath.ixx`, `Artifact/src/Mask/MaskPath.cppm`

---

## 1. クラス定義

### 1.1 クラス概要

```cpp
// Artifact/include/Mask/LayerMask.ixx:43-72
export namespace Artifact {

/// レイヤーに付属するマスクコンテナ
/// 複数の MaskPath を保持し、合成してアルファマスクを生成する
class LayerMask {
private:
    class Impl;
    Impl* impl_;

public:
    LayerMask();
    ~LayerMask();
    LayerMask(const LayerMask& other);
    LayerMask& operator=(const LayerMask& other);

    // マスクパスの管理
    void addMaskPath(const MaskPath& path);
    void removeMaskPath(int index);
    void setMaskPath(int index, const MaskPath& path);
    MaskPath maskPath(int index) const;
    int maskPathCount() const;
    void clearMaskPaths();

    // マスク全体の有効/無効
    bool isEnabled() const;
    void setEnabled(bool enabled);

    /// 全マスクパスを合成して単一アルファマスク (CV_32FC1) を生成
    /// outMat は cv::Mat* を void* として渡す
    void compositeAlphaMask(int width, int height, void* outMat) const;

    /// RGBA画像のアルファチャンネルにマスクを乗算適用
    /// imageMat は CV_32FC4 の cv::Mat* を void* として渡す
    void applyToImage(int width, int height, void* imageMat) const;
};

}
```

### 1.2 実装 details (Pimpl)

```cpp
// Artifact/src/Mask/LayerMask.cppm:49-53
class LayerMask::Impl {
public:
    std::vector<MaskPath> paths;  // ← MaskPath コレクション
    bool enabled = true;
};
```

---

## 2. 主要メソッド一覧

### 2.1 マスクパス管理

| メソッド | 宣言箇所 | 実装箇所 | 機能 |
|---------|---------|---------|------|
| `addMaskPath(const MaskPath&)` | LayerMask.ixx:54 | LayerMask.cppm:67 | マスクパスを末尾追加 |
| `removeMaskPath(int)` | LayerMask.ixx:55 | LayerMask.cppm:69-72 | インデックス指定削除 |
| `setMaskPath(int, const MaskPath&)` | LayerMask.ixx:56 | LayerMask.cppm:74-77 | インデックス指定更新 |
| `maskPath(int) const` | LayerMask.ixx:57 | LayerMask.cppm:79-83 | インデックス指定取得 |
| `maskPathCount() const` | LayerMask.ixx:58 | LayerMask.cppm:85 | マスクパス数取得 |
| `clearMaskPaths()` | LayerMask.ixx:59 | LayerMask.cppm:87 | 全削除 |

### 2.2 有効/無効制御

| メソッド | 宣言箇所 | 実装箇所 | 機能 |
|---------|---------|---------|------|
| `isEnabled() const` | LayerMask.ixx:62 | LayerMask.cppm:89 | 有効状態取得 |
| `setEnabled(bool)` | LayerMask.ixx:63 | LayerMask.cppm:90 | 有効状態設定 |

### 2.3 マスク合成・適用

| メソッド | 宣言箇所 | 実装箇所 | 機能 |
|---------|---------|---------|------|
| `compositeAlphaMask(int, int, void*) const` | LayerMask.ixx:67 | LayerMask.cppm:92-140 | 全 MaskPath を合成し CV_32FC1 のアルファマスク生成 |
| `applyToImage(int, int, void*) const` | LayerMask.ixx:71 | LayerMask.cppm:142-156 | RGBA画像(CV_32FC4)のαチャンネルに乗算適用 |

---

## 3. MaskPath コレクションとしての役割

- **内部構造**: `std::vector<MaskPath> paths` を Pimpl 内に保持 (LayerMask.cppm:51)
- **直接アクセス**: インデックスベースの getter/setter で操作可能
- **所有権**: 値渡し (`MaskPath` はコピー可能) — 値セマンティクスで管理
- **合成順序**: `paths` の順序が合成順序。インデックス 0 から順に `MaskMode` に従って合成

---

## 4. 依存関係マップ

### 4.1 モジュール依存（import 文）

```cpp
// LayerMask.ixx:37
import Artifact.Mask.Path;          // ← MaskPath クラス必須

// LayerMask.cppm:45
import Artifact.Mask.Path;          // 実装でも使用
```

### 4.2 ライブラリ依存

| ライブラリ | 用途 | 使用箇所 |
|-----------|------|---------|
| OpenCV (`opencv2/opencv.hpp`) | アルファマスクのラスタライズ・合成 | LayerMask.cppm:39, 94-139 |
| Qt (`QPointF`) | MaskPath の頂点位置 (MaskPath 経由) | MaskPath.ixx:32 |
| C++標準 | vector, algorithm 等 | 全般 |

### 4.3 クラス依存グラフ

```
LayerMask
  ├─ depends on ──► MaskPath          (直接保持)
  │                └─ UniString, QPointF
  └─ uses ────────► OpenCV (cv::Mat)  (ラスタライズ・合成)
```

---

## 5. 使用箇所

### 5.1 ArtifactAbstractLayer (Layer クラス)

```cpp
// Artifact/include/Layer/ArtifactAbstractLayer.ixx:30, 269-276
import Artifact.Mask.LayerMask;

class ArtifactAbstractLayer : public QObject {
    // ...
    /*Masks*/
    void addMask(const LayerMask &mask);
    void removeMask(int index);
    void setMask(int index, const LayerMask &mask);
    LayerMask mask(int index) const;
    int maskCount() const;
    void clearMasks();
    bool hasMasks() const;
    /*Masks*/
    // ...
};

// 実装: Artifact/src/Layer/ArtifactAbstractLayer.cppm:1516-1565
// → Impl 内に std::vector<LayerMask> masks_ を保持
```

**役割**: レイヤーは複数の `LayerMask` を保持でき、各 `LayerMask` が複数の `MaskPath` を保持する階層構造。

### 5.2 ArtifactCompositionRenderController

```cpp
// Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:70
import Artifact.Mask.LayerMask;

// 使用例 (183行付近):
LayerMask mask = targetLayer->mask(m);

// マスク編集時のバックアップ (1273行付近):
std::vector<LayerMask> maskEditBefore_;

// 編集結果の適用 (2380, 2424, 2430, 2687, 2731, 3878行など):
LayerMask mask = selectedLayer->mask(m);
LayerMask newMask;         // 新規マスク作成
```

**役割**: マスク編集モード中的な編集・Undo のために `LayerMask` のコピーを生成・保持。

### 5.3 UndoManager

```cpp
// Artifact/include/Undo/UndoManager.ixx:46, 113-114, 120-121
import Artifact.Mask.LayerMask;

void registerMaskSnapshot(
    const ArtifactAbstractLayerPtr& layer,
    const std::vector<LayerMask>& beforeMasks,
    const std::vector<LayerMask>& afterMasks);

private:
    std::vector<LayerMask> beforeMasks_;
    std::vector<LayerMask> afterMasks_;
```

**役割**: マスク変更前後の `LayerMask` コレクションを保持し、Undo/Redo で復元。

### 5.4 Software Render Inspector

```cpp
// Artifact/src/Widgets/Render/ArtifactSoftwareRenderInspectors.cppm:50
import Artifact.Mask.LayerMask;
```

---

## 6. 責務表

### 6.1 クラス責務

| 責務カテゴリ | 具体的责務 | 関連メソッド/データ |
|------------|-----------|-------------------|
| **コレクション管理** | `MaskPath` オブジェクトの追加・削除・更新・列挙 | `paths: std::vector<MaskPath>` |
| **有効/無効制御** | マスク全体の有効無効フラグ管理 | `enabled: bool`, `isEnabled()`, `setEnabled()` |
| **マスク合成** | 複数 MaskPath を MaskMode に従ってアルファマスクとして合成 | `compositeAlphaMask()` |
| **画像適用** | 合成したアルファマスクを RGBA 画像の α チャンネルに乗算 | `applyToImage()` |
| **コピー/代入** | 深いコピーによる値セマンティクス (Pimpl 経由) | コピーctor, 代入演算子 |

### 6.2 MaskMode 合成ロジック責務

`compositeAlphaMask()` 内での各 `MaskMode` の意味:

| MaskMode | 合成操作 | OpenCV 実装 |
|---------|---------|------------|
| `Add` | 和集合 (論理OR) | 最初の Add はそのままコピー、2回目以降は `cv::max(dst, pathMask)` |
| `Subtract` | 差集合 (論理NOT) | `dst = dst * (1 - pathMask)` |
| `Intersect` | 積集合 (論理AND) | `dst = dst * pathMask` |
| `Difference` | 对称差 | `dst = |dst - pathMask|` |

### 6.3 ArtifactAbstractLayer との責任分界

| LayerMask側責務 | ArtifactAbstractLayer側責務 |
|---------------|--------------------------|
| 単一レイヤーに属するマスクパス群の管理 | 複数 LayerMask のコレクション管理 (`std::vector<LayerMask> masks_`) |
| 自身の `paths` を合成してアルファマスク生成 | 各 LayerMask に問い合わせ、レイヤー描画時に適用 |
| `applyToImage()` による直接画像編集 | 合成パイプライン経由での間接適用 |

---

## 7. 重要な実装詳細

### 7.1 compositeAlphaMask() の動作

```cpp
// LayerMask.cppm:92-140 要約
void LayerMask::compositeAlphaMask(int width, int height, void* outMat) const {
    cv::Mat& dst = *static_cast<cv::Mat*>(outMat);

    if (!impl_->enabled || impl_->paths.empty()) {
        dst = cv::Mat::ones(height, width, CV_32FC1);  // 不透明
        return;
    }

    dst = cv::Mat::zeros(height, width, CV_32FC1);  // 完全透明から開始
    bool firstAdd = true;

    for (const auto& path : impl_->paths) {
        cv::Mat pathMask;
        path.rasterizeToAlpha(width, height, &pathMask);  // MaskPath::rasterizeToAlpha 呼び出し

        switch (path.mode()) {
            case MaskMode::Add:
                if (firstAdd) { dst = pathMask.clone(); firstAdd = false; }
                else cv::max(dst, pathMask, dst);  //  union
                break;
            case MaskMode::Subtract:
                { cv::Mat inv = cv::Scalar(1.0f) - pathMask;
                  cv::multiply(dst, inv, dst); }
                break;
            case MaskMode::Intersect:
                cv::multiply(dst, pathMask, dst);
                break;
            case MaskMode::Difference:
                cv::absdiff(dst, pathMask, dst);
                break;
        }
    }

    cv::min(dst, 1.0f, dst);  // Clamp 0~1
    cv::max(dst, 0.0f, dst);
}
```

### 7.2 applyToImage() の動作

```cpp
// LayerMask.cppm:142-156
void LayerMask::applyToImage(int width, int height, void* imageMat) const {
    cv::Mat& img = *static_cast<cv::Mat*>(imageMat);
    if (img.empty() || img.type() != CV_32FC4) return;
    if (!impl_->enabled || impl_->paths.empty()) return;

    cv::Mat alphaMask;
    compositeAlphaMask(width, height, &alphaMask);

    // αチャンネル (channels[3]) に乗算
    std::vector<cv::Mat> channels(4);
    cv::split(img, channels);
    cv::multiply(channels[3], alphaMask, channels[3]);
    cv::merge(channels, img);
}
```

---

## 8. インターフェースの利用パターン

### 8.1 マスク追加・有効化（典型的利用）

```cpp
LayerMask layerMask;
layerMask.setEnabled(true);

MaskPath path1, path2;
// ... path1, path2 に頂点・属性設定 ...
layerMask.addMaskPath(path1);
layerMask.addMaskPath(path2);
```

### 8.2 レイヤーへのマスク設定（ArtifactAbstractLayer 経由）

```cpp
auto layer = ArtifactAbstractLayer::fromJson(jsonObj);
LayerMask mask;
// ... mask を構成 ...
layer->addMask(mask);   // ArtifactAbstractLayer::addMask()
```

### 8.3 マスクの Undo/Redo

```cpp
// UndoManager が beforeMasks/afterMasks を保持し、
// applyMaskSnapshot() で layer のマスク集合を一括置換
```

---

## 9.  design 上の特徴

| 項目 | 内容 |
|-----|------|
| **隠蔽方式** | Pimpl イディオム (`Impl` ポインタ) — ABI 安定性・実装カプセル化 |
| **値セマンティクス** | コピーctor/operator= で深コピー (Impl のコピー) |
| **void* 渡し** | OpenCV 型を void* で受け取り (`compositeAlphaMask`, `applyToImage`) — C インターフェース互換性のためか |
| **合成順序** | `std::vector` の順序を尊重 — インデックスによる編集可能 |
| **OpenCV 依存** | rasterizeToAlpha() は MaskPath 側実装、composite/apply は OpenCV 使用 |

---

## 10. 関連クラスとの責務分担

| クラス | 責務 | LayerMask との関係 |
|-------|------|-------------------|
| `MaskPath` | 単一ベジェパス・頂点列・ラスタライズ・個別属性 | `LayerMask` が `std::vector<MaskPath>` で保持 |
| `ArtifactAbstractLayer` | レイヤー全体のプロパティ・描画・マスク集合管理 | `std::vector<LayerMask>` で複数 `LayerMask` を保持 |
| `ArtifactCompositionRenderController` | マスク編集 UI・Undo 管理 | `LayerMask` のコピーを生成し、編集結果を layer に反映 |
| `UndoManager` | マスク変更前後のスナップショット保持 | `std::vector<LayerMask>` を before/after で保持 |

---

## 11. パフォーマンス・実装上の注意

1. **コピー時コスト**: `LayerMask` のコピーは `std::vector<MaskPath>` の全要素をディープコピー。多数の頂点を含む MaskPath を多数保持する場合、コピーは高コスト。
2. **rasterizeToAlpha 呼び出し**: `compositeAlphaMask()` は各 `MaskPath::rasterizeToAlpha()` を呼び出し、頂点→アルファマスク変換を毎回実施。`MaskPath` のキャッシュがあれば改善の余地。
3. **void* インターフェース**: `cv::Mat*` を `void*` で受け取る设计。C 言語互換性か。型安全ではなく、呼び出し側で `CV_32FC1`/`CV_32FC4` 確認必須。
4. **合成順序依存**: 結果が `paths` の順序に依存。ユーザーが順序変更可能かどうかは UI 側 (CompositionRenderController) で実装。

---

## 12. ファイル一覧

```
Artifact/
├── include/Mask/
│   ├── LayerMask.ixx        (74行)   — クラス宣言
│   └── MaskPath.ixx         (107行)  — MaskPath 宣言 (依存先)
├── src/Mask/
│   ├── LayerMask.cppm       (158行)  — 実装
│   └── MaskPath.cppm        (223行)  — MaskPath 実装
└── include/Layer/
    └── ArtifactAbstractLayer.ixx      — LayerMask 利用口 (masks_ 保持)
```

---

## 13. 結論

`LayerMask` は **「複数の MaskPath を所有・合成し、1つのアルファマスクを生成する責悉」** を持つクラス。  
`ArtifactAbstractLayer` が `std::vector<LayerMask>` で複数の LayerMask を保持することで、**「1レイヤーに複数マスク」** という階層構造を実現している。  

合成ロジックは `compositeAlphaMask()` に集約され、OpenCV を用いたベクトル演算で実装。`MaskPath` からのラスタライズ結果を `MaskMode` ごとに四則演算的に合成することで、柔軟なマスク演算を実現している。
