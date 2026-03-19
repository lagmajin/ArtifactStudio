# エフェクトシステム仕様書

## 概要

AfterEffectsにC4DのMoGraph的な要素を統合するためのエフェクトシステム仕様。

## 設計思想

AfterEffectsは基本的に2Dソフトウェアであるが、C4DのMoGraph的な要素（ジェネレーター、エフェクター、マテリアルなど）を統合することで、より高度なモーショングラフィックス作成を可能にする。

## エフェクトの5つの種類

エフェクトは以下の5つの種類に分類され、インスペクタのエフェクトラックに順番に表示される。

### 1. Generator（生成エフェクト）

**目的**: コンテンツを生成するエフェクト

**例**:
- テキストジェネレーター
- シェイプジェネレーター
- パーティクルジェネレーター
- プリミティブジェネレーター

**特徴**:
- 最初に適用されるエフェクト
- レイヤーの内容を生成する
- C4Dのジェネレーターに相当

### 2. Geometory Transform（ジオメトリ変換エフェクト）

**目的**: ジオメトリ（形状）を変換するエフェクト

**例**:
- 3D変換（位置、回転、スケール）
- ワープ
- ディストーション
- メッシュ変形

**特徴**:
- Generatorの後に適用される
- 3D的な変換をサポート
- C4Dの変換エフェクターに相当

### 3. Material Render（マテリアルレンダーエフェクト）

**目的**: マテリアル（材質）を適用するエフェクト

**例**:
- マテリアル適用
- テクスチャマッピング
- ライティング
- シェーディング

**特徴**:
- Geometory Transformの後に適用される
- マテリアルシステムをサポート
- C4Dのマテリアルシステムに相当

### 4. Rasterizer（ラスタライザエフェクト）

**目的**: 最終画像にかかるエフェクト

**例**:
- ブラー（ガウス、放射状、方向）
- シャープ
- ノイズ
- グレイン
- グロー
- ドロップシャドウ

**特徴**:
- Material Renderの後に適用される
- 最終画像に適用される
- C4Dのレンダラーに相当
- 「Blurのように画像全体にかかるエフェクトの総称」

### 5. LayerTransform（レイヤートランスフォームエフェクト）

**目的**: レイヤーを変換するエフェクト

**例**:
- レイヤーの位置、回転、スケール
- レイヤーの不透明度
- レイヤーのブレンドモード
- レイヤーのマスク

**特徴**:
- 最後に適用されるエフェクト
- レイヤー全体の変換を担当
- C4Dのトランスフォームに相当

## エフェクトの適用順序

```
Generator → Geometory Transform → Material Render → Rasterizer → LayerTransform
```

この順序は、3Dソフトのパイプラインと類似している。

## エフェクト基盤クラス

### ArtifactAbstractEffect

エフェクトの基底クラス。

```cpp
class ArtifactAbstractEffect {
public:
    virtual ~ArtifactAbstractEffect() = default;
    
    // エフェクトの種類
    virtual EffectType type() const = 0;
    
    // エフェクトの名前
    virtual QString name() const = 0;
    
    // エフェクトの適用
    virtual void apply(ArtifactAbstractLayer* layer) = 0;
    
    // パラメータの取得/設定
    virtual QVariant parameter(const QString& name) const = 0;
    virtual void setParameter(const QString& name, const QVariant& value) = 0;
    
    // シリアライズ
    virtual QJsonObject toJson() const = 0;
    virtual void fromJson(const QJsonObject& json) = 0;
};
```

### ArtifactAbstractRasterEffect

ラスタライザエフェクトの基底クラス。

```cpp
class ArtifactAbstractRasterEffect : public ArtifactAbstractEffect {
public:
    EffectType type() const override { return EffectType::Rasterizer; }
    
    // ラスタライザエフェクト固有のメソッド
    virtual QImage process(const QImage& input) = 0;
};
```

## エフェクトの実装例

### ArtifactBlurEffect

ブラー（ガウス）エフェクト。

```cpp
class ArtifactBlurEffect : public ArtifactAbstractRasterEffect {
public:
    QString name() const override { return "Blur (Gaussian)"; }
    
    QImage process(const QImage& input) override {
        // ガウスブラーの実装
        float radius = parameter("radius").toFloat();
        return applyGaussianBlur(input, radius);
    }
    
    // パラメータ
    // - radius: ブラーの半径（float）
};
```

### ArtifactTextGeneratorEffect

テキストジェネレーターエフェクト。

```cpp
class ArtifactTextGeneratorEffect : public ArtifactAbstractEffect {
public:
    EffectType type() const override { return EffectType::Generator; }
    QString name() const override { return "Text Generator"; }
    
    void apply(ArtifactAbstractLayer* layer) override {
        // テキストの生成
        QString text = parameter("text").toString();
        QFont font = parameter("font").value<QFont>();
        // テキストレイヤーの生成
    }
    
    // パラメータ
    // - text: テキスト内容（QString）
    // - font: フォント（QFont）
    // - color: 色（QColor）
};
```

## エフェクトマネージャー

### ArtifactEffectManager

エフェクトを管理するクラス。

```cpp
class ArtifactEffectManager {
public:
    // エフェクトの登録
    void registerEffect(std::shared_ptr<ArtifactAbstractEffect> effect);
    
    // エフェクトの取得
    std::shared_ptr<ArtifactAbstractEffect> getEffect(const QString& name) const;
    
    // エフェクトの適用
    void applyEffects(ArtifactAbstractLayer* layer);
    
    // エフェクトの順序
    QVector<EffectType> effectOrder() const;
};
```

## インスペクタでの表示

インスペクタのエフェクトラックには、エフェクトが適用順序で表示される。

```
[Generator]
  - Text Generator
  - Shape Generator

[Geometory Transform]
  - 3D Transform
  - Warp

[Material Render]
  - Material
  - Texture

[Rasterizer]
  - Blur (Gaussian)
  - Sharpen
  - Noise

[LayerTransform]
  - Position
  - Rotation
  - Scale
  - Opacity
```

## 今後の拡張

1. **エフェクトのプリセット**
   - よく使うエフェクトの組み合わせをプリセットとして保存
   - プリセットの読み込み/保存

2. **エフェクトのアニメーション**
   - エフェクトのパラメータをアニメーション
   - キーフレームのサポート

3. **エフェクトのエクスプレッション**
   - エフェクトのパラメータをエクスプレッションで制御
   - JavaScriptベースのエクスプレッション

4. **エフェクトのGPUアクセラレーション**
   - GPUを使ったエフェクトの高速化
   - Compute Shaderのサポート

## 参考

- AfterEffectsのエフェクトシステム
- Cinema 4DのMoGraph
- Nukeのノードベースのエフェクトシステム