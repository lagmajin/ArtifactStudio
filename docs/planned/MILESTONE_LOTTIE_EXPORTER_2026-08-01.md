# Lottie / Bodymovin エクスポーター 実装マイルストーン

**日付**: 2026-08-01
**最終更新**: 2026-08-15
**ベース**: Adobe After Effects の Bodymovin プラグイン + Lottie (lottie-web / lottie-ios)
**現状**: LottieのCore型・JSON変換・Artifact側Export導線・Rig2D変換の基盤を実装済み。外部runtime互換と一部機能の実ファイル検証は未完了。
**狙い**: Artifact の2Dリグで作ったアニメーションをLottie JSONに書き出し、Web/iOS/Android/Flutterで再生可能にする

---

## Lottie とは

- **Bodymovin** = AEプラグイン。AEのシェイプアニメーションを JSON にシリアライズ
- **Lottie** = JSON をパースして各プラットフォームでレンダリングするランタイム
- 業界標準: Airbnb発、現在はコミュニティ運営。Webアニメーションの事実上の標準

---

## Lottie JSON の構造

```json
{
  "v": "5.12.0",
  "fr": 30,
  "ip": 0,
  "op": 90,
  "w": 1920,
  "h": 1080,
  "nm": "My Animation",
  "ddd": 0,
  "assets": [ ... ],
  "layers": [
    {
      "ddd": 0, "ind": 1, "ty": 4, "nm": "Shape Layer",
      "ks": { ... },  // transform keyframes
      "shapes": [
        {
          "ty": "grp",
          "it": [
            { "ty": "rc", "d": 1, "s": {...}, "p": {...}, "r": {...} },
            { "ty": "fl", "c": {...}, "o": {...} },
            { "ty": "st", "c": {...}, "o": {...}, "w": {...} }
          ]
        }
      ]
    }
  ]
}
```

---

## Phase 1: コアシリアライザ

### Step 1.1 — Lottie型定義
**新規**: `ArtifactCore/include/Export/Lottie/LottieTypes.ixx`

```cpp
export module Core.Export.Lottie.Types;

export namespace ArtifactCore::Export::Lottie {

struct LottieKeyframe {
    double t = 0.0;      // フレーム番号
    std::vector<double> s; // 開始値
    std::vector<double> e; // 終了値
    int hold = 0;         // 0=linear, 1=hold
    struct { double x1=0,y1=0,x2=1,y2=1; } bezier;
};

struct LottiePoint { std::vector<double> k; };  // [x, y] or keyframes
struct LottieColor { std::vector<double> k; };  // [r, g, b, a]

struct LottieShapeRect {
    std::string ty = "rc";
    LottiePoint p;    // position
    LottiePoint s;    // size
    LottiePoint r;    // rounded corners
};

struct LottieShapeFill {
    std::string ty = "fl";
    LottieColor c;    // color [r,g,b,a]
    LottiePoint o;    // opacity [0-100]
};

struct LottieShapeStroke {
    std::string ty = "st";
    LottieColor c;
    LottiePoint o;
    LottiePoint w;    // stroke width
};

struct LottieShapeGroup {
    std::string ty = "grp";
    std::vector<std::any> it; // items: fill/stroke/rect/ellipse/path...
};

struct LottieLayer {
    int ddd = 0;      // 3D layer flag
    int ind = 0;      // layer index
    int ty = 0;       // type: 0=precomp, 1=solid, 2=image, 3=null, 4=shape
    std::string nm;   // name
    LottiePoint ks_p; // transform position
    LottiePoint ks_a; // transform anchor
    LottiePoint ks_s; // transform scale
    LottiePoint ks_r; // transform rotation
    LottiePoint ks_o; // transform opacity
    std::vector<std::any> shapes; // for ty=4
    int ip = 0, op = 0; // in/out frame
};

struct LottieDocument {
    std::string v = "5.12.0";
    double fr = 30.0;
    int ip = 0, op = 0;
    int w = 1920, h = 1080;
    std::string nm;
    std::vector<std::any> assets;
    std::vector<LottieLayer> layers;
};

} // namespace
```

### Step 1.2 — コンバーター
**新規**: `ArtifactCore/include/Export/Lottie/LottieExporter.ixx` + `.cppm`

```cpp
export class LottieExporter {
public:
    // Artifact レイヤー → Lottie 変換
    LottieLayer convertLayer(const ArtifactAbstractLayerPtr& layer,
                              double fps, int compFrameStart, int compFrameEnd);

    // キーフレーム変換
    template<typename T>
    LottiePoint keyframesToLottie(
        const AnimatableValue& anim, double fps,
        std::function<std::vector<double>(const T&)> toArray);

    // メインエクスポート
    bool exportToFile(const ArtifactCompositionPtr& comp,
                      const QString& outputPath,
                      const LottieExportOptions& options);

    // Rig2D → Lottie Shape レイヤー
    LottieLayer convertRigLayer(const ArtifactCore::Rig2D* rig,
                                 double fps);

private:
    LottiePoint convertPosition(const AnimatableTransform3D& t, double fps);
    LottiePoint convertScale(const AnimatableTransform3D& t, double fps);
    LottiePoint convertRotation(const AnimatableTransform3D& t, double fps);
    LottiePoint convertOpacity(const ArtifactAbstractLayerPtr& layer, double fps);
    LottieColor convertColor(const FloatColor& c);
};

struct LottieExportOptions {
    // 一般
    bool embedImages = true;      // 画像をBase64埋め込み
    bool prettyPrint = false;     // 整形JSON出力
    QString name;                 // 出力ファイル名

    // 2D Rig
    bool exportRigAsShapes = true; // Rig2D → Shapeレイヤー
    float rigSimplification = 0.5f; // 頂点削減率

    // 圧縮
    bool compressKeyframes = true; // 不要キーフレーム削除
    float keyframeTolerance = 0.01f;

    // 互換性
    bool strictMode = false;       // Lottie仕様に厳密準拠
};
```

### Step 1.3 — キーフレーム変換アルゴリズム

```
Artifact AnimatableTransform3D → Lottie Point
1. 全キーフレームを列挙
2. Artifact RationalTime → Lottie フレーム番号に変換
3. LottieKeyframe { t, s, e } に変換
4. キーフレーム補間を Lottie bezier に変換（Linear/Stepのみ初期対応。Bezier/Holdは後続フェーズ）
5. keyframeTolerance で冗長キーフレームを間引く
```

### Step 1.4 — Rig2D → Lottie Shape 変換

```
Artifact Rig2D → Lottie Shape Layer:
1. Rig2D::evaluate(time) を全フレームで実行
2. SkinMesh::deform() → 変形済み頂点を取得
3. 三角形 → Lottie Path ("sh" type, bezier) に変換
   ※ Lottie の Shape はベジェパスのみ。三角形はベジェ近似に変換
4. 材質 → Lottie Fill ("fl") + Stroke ("st") に変換
5. 各フレームの頂点 → Lottie Keyframe に変換
```

---

## Phase 2: 最適化・圧縮

### Step 2.1 — キーフレーム間引き
- 隣接フレーム間の差分が tolerance 未満ならキーフレーム削除
- 線形補間で十分な場合は bezier を省略

### Step 2.2 — 頂点削減（Ramer-Douglas-Peucker）
- アニメーションする頂点軌跡をRDP簡略化
- `rigSimplification` パラメータで制御

### Step 2.3 — 重複Shapeの再利用
- 同一の Shape 定義を assets として参照
- ファイルサイズ削減

---

## Phase 3: UI

### Step 3.1 — エクスポートダイアログ
```
File メニュー → Export → Lottie (.json)...

ダイアログ:
┌─ Export Lottie Animation ────────┐
│ Name: [MyAnimation__________]    │
│                                  │
│ ☑ Embed Images (Base64)          │
│ ☑ Export Rig as Shapes           │
│ ☑ Compress Keyframes             │
│   Tolerance: [0.01]              │
│ Rig Simplification: [0.5]        │
│                                  │
│ [Output Path...] [Export] [Cancel]│
└──────────────────────────────────┘
```

### Step 3.2 — Lottie プレビュー
- エクスポート後の JSON を WebView で即時プレビュー
- lottie-web 埋め込み

---

## Phase 4: 拡張機能

### Step 4.1 — Lottie Import（逆方向）
- Lottie JSON → Artifact レイヤー + アニメーション
- 外部アニメーションアセットの取り込み

### Step 4.2 — マスクエクスポート
- Artifact のマスクパスを Lottie のマスクに変換
- MatteMode 対応

### Step 4.3 — テキストエクスポート
- TextAnimator のキーフレームを Lottie テキストアニメーションに変換

---

## ファイル一覧

| フェーズ | ファイル | 新規/変更 | 内容 |
|---------|----------|----------|------|
| P1 | `ArtifactCore/include/Export/Lottie/LottieTypes.ixx` | 新規 | Lottie JSON 型定義 |
| P1 | `ArtifactCore/include/Export/Lottie/LottieExporter.ixx` | 新規 | エクスポーター インターフェース |
| P1 | `ArtifactCore/src/Export/Lottie/LottieExporter.cppm` | 新規 | コア変換ロジック |
| P1 | `ArtifactCore/src/Export/Lottie/LottieSerializer.cppm` | 新規 | JSON シリアライズ |
| P2 | `ArtifactCore/src/Export/Lottie/LottieKeyframeOptimizer.cppm` | 新規 | キーフレーム間引き |
| P2 | `ArtifactCore/src/Export/Lottie/LottieMeshSimplifier.cppm` | 新規 | 頂点削減 |
| P3 | `Artifact/include/Widgets/Dialog/LottieExportDialog.ixx` | 新規 | エクスポートUI |
| P3 | `Artifact/src/Widgets/Dialog/LottieExportDialog.cppm` | 新規 | UI実装 |
| P3 | `ArtifactFileMenu.cppm` | 変更 | Exportメニュー追加 |
| P4 | `ArtifactCore/include/Export/Lottie/LottieImporter.ixx` | 新規 | Import |
| P4 | `ArtifactCore/src/Export/Lottie/LottieImporter.cppm` | 新規 | Import実装 |

---

## 検証チェックリスト

- [ ] 単純シェイプ（矩形+塗り+線）が正しくJSON出力される
- [ ] 位置・スケール・回転・不透明度のキーフレームが正しい
- [ ] 出力JSONを Lottie Web Player で再生できる
- [ ] 出力JSONを LottieFiles で検証できる
- [ ] Rig2DのSkinMeshがShapeレイヤーに変換される
- [ ] キーフレーム間引きでJSONサイズが削減される
- [ ] 複数レイヤーの順序が正しい
- [ ] マスクパスが正しくエクスポートされる

## Update 2026-08-15

- `ArtifactCore` に `Export.Lottie.Types`、`Export.Lottie.Exporter`、`LottieRigExporter` があり、Lottie document／layer／transform／keyframe／shape／image／precomp の型、JSON serialize／validate、file export、import、keyframe compression、embedded image asset の経路を確認できる。
- `LottieRigExporter` は Rig2D のボーンを時系列サンプルし、position／scale／rotation keyframe と rectangle／fill、skin mesh の path shape を document に追加する。したがって旧記述の「Lottie完全不在」は現状と一致しない。
- `ArtifactExportLottieWriter` と `ArtifactExportDialog` に Composition → Lottie JSON の書き出し導線があり、画像埋め込み、shape／precomp／raster fallback、frame range などを変換する基盤がある。
- 一方、現行コード検索だけでは lottie-web／LottieFiles／各OS runtime での再生互換、mask／matte、TextAnimator のネイティブ変換、RDP mesh simplification、専用 Lottie Export Dialog の全オプションが受入れ済みとは確認できない。既存 UI は汎用 Export Dialog の Lottie 分岐である。
- よって現状は `Phase 1 core serialization/export/import: implemented / Rig2D and common shape export: partial-to-implemented / Phase 2 optimization, Phase 3 dedicated UI, masks/text and external-player validation: pending` と整理する。
