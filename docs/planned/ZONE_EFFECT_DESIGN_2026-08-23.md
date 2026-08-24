# Zone Effect 設計案

**最終更新:** 2026-08-23

## 目的

コンポジション内の特定領域にレイヤーが入ったとき、そのレイヤーへぼかしなどのエフェクトを自動適用する。

初期対象は、矩形または楕円のZoneへ侵入したレイヤーにGaussian Blurを適用する機能とする。

## 基本方針

- Zoneはレイヤーの子ではなく、コンポジション空間上の独立した要素として管理する。
- 既存のLayer Effect Stackへ、Zoneへの侵入・退出に応じてエフェクトを動的追加・削除しない。
- Zone専用のデータモデル、編集面、レンダリング評価を持つ。
- Zoneへの重なり率に応じて効果量を連続的に補間する。
- GPU/Diligent経路を主対象とし、ソフトレンダラーへの新機能追加は別途検討する。

## エフェクトモード

### Layer Mode

Zoneに入ったレイヤー自身をぼかす。

```text
Layer Source
  -> Zone Membership Evaluation
  -> Layer-local Zone Effect
  -> Layer Composite
```

初期実装はこのモードを対象とする。

### Screen Mode

Zone領域に表示された最終画像をぼかす。

```text
Composition Composite
  -> Zone Mask
  -> Region Blur
  -> Output
```

プライバシー領域や画面の一部を常時ぼかす用途向け。Layer Modeとは別モードとして扱う。

## データモデル

```text
ZoneEffect
├─ id / name
├─ shape                  Rectangle / Ellipse
├─ transform              Position / Size / Rotation
├─ mode                   Layer / Screen
├─ effectType             GaussianBlur (initial)
├─ amount                 最大ぼかし量
├─ feather                境界の遷移幅
├─ falloff                Linear / Smooth / SmoothIn / SmoothOut
├─ includedLayerIds       空なら全レイヤー
├─ excludedLayerIds
├─ priority
├─ combineMode            Max (initial), Add, Multiply (future)
└─ enabled
```

## 対象レイヤー判定

初期版では、レイヤーのワールド空間AABBとZone領域の重なりを使用する。

```text
overlap = layerBounds ∩ zoneBounds
influence = overlapArea / layerArea
effectiveAmount = amount × influence
```

判定結果は保存せず、各フレームの現在のTransformから再計算する。

対象判定の優先順位は以下とする。

1. Zoneが無効なら適用しない
2. レイヤーが除外リストにあれば適用しない
3. 包含リストが空なら全レイヤーを対象にする
4. 包含リストが空でなければ、リストにあるレイヤーだけを対象にする

将来的にはレイヤー側に `Receive Zone Effects` の共通除外フラグを追加できる。

## 影響度と境界

Zone本体とFeather領域を分ける。

```text
Zone外             0.0
Feather領域        0.0 ～ 1.0
Zone内部           1.0
```

Falloff関数によって遷移曲線を変更する。最終的な効果量は次の式とする。

```text
effectiveAmount = baseAmount × animatedAmount × spatialInfluence
```

ぼかしの処理領域は、ぼかし半径とFeather分だけ拡張する。

```text
processingBounds = layerBounds expandedBy (blurRadius + feather)
```

## 複数Zone

同一レイヤーへ複数Zoneが影響する場合、初期版ではZoneのpriority順に評価し、Blurについては最大値方式を採用する。

```text
finalRadius = max(zoneRadiusA, zoneRadiusB, ...)
```

将来的に以下の合成方式を追加できる。

```text
Max / Add / Multiply
```

## レンダリング責務

```text
Layer Source
  -> Layer-local Effects
  -> Zone Membership Evaluation
  -> Zone Layer Effect
  -> Layer opacity / blend
  -> Composition Effects
  -> Output
```

Zoneの侵入状態をLayer Effect Stackへ反映するのではなく、レンダリング評価時に一時的なZone影響値を生成する。

## UI設計

Zoneは通常のLayerとは独立したZone Effects面で管理する。

```text
Zone Effects
├─ Add Zone
├─ Zone List
│  ├─ Blur Zone
│  └─ ...
└─ Selected Zone Properties
```

Viewportでは、選択中のZoneについて次を表示する。

- Zone本体の境界
- Feather境界
- Transformハンドル
- Zone名

Zone由来の設定を通常のLayer Propertyへ混在させない。

## アニメーション対象

以下をキーフレーム対象とする。

- Position
- Scale
- Rotation
- Size
- Amount
- Feather
- Falloff
- Enabled

Zoneの移動とBlur量のアニメーションは独立して編集できるようにする。

## 保存形式の例

```json
{
  "id": "zone_blur_01",
  "name": "Focus Blur",
  "shape": "rectangle",
  "mode": "layer",
  "transform": {
    "position": [640, 360],
    "size": [400, 240],
    "rotation": 0
  },
  "effect": {
    "type": "gaussian_blur",
    "amount": 18.0,
    "feather": 64.0,
    "falloff": "smooth"
  },
  "targets": {
    "include": [],
    "exclude": ["ui_layer"]
  },
  "priority": 0,
  "enabled": true
}
```

## 初期スコープ

1. Rectangle Zone
2. Layer Mode
3. Gaussian Blur
4. AABBによる侵入判定
5. FeatherとFalloff
6. Include / Exclude
7. Transformおよび主要パラメータのアニメーション
8. Undo / Redo
9. 保存・再読込
10. GPU経路へのZone影響値・マスク連携

## 将来スコープ

- EllipseおよびPath Zone
- ピクセル単位のアルファ判定
- Screen Mode
- Blur以外のColor / Distortion / Mosaic
- 複数Zoneの合成モード
- Zoneプリセット
- Zoneの親子関係

## 責務分離

```text
Layer Effect Stack
  レイヤー固有の常時エフェクト

Mask
  画像形状・透明度による切り抜き

Zone Effect
  空間位置を条件にした一時的・条件付きエフェクト
```

この分離により、通常のレイヤーエフェクト、マスク、空間条件付きエフェクトの編集・保存・Undo責務を混在させない。

## 未決事項

- Layer Modeでの正確な部分侵入表現をAABBからアルファマスクへ拡張する時期
- Zoneが親子階層や3D空間に対応する場合の座標系
- Screen Modeを同じZoneモデルで扱うか、別のComposition Effectとして扱うか
- GPU経路とフォールバック経路での品質・速度の受け入れ基準

