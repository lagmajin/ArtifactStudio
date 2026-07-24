# SurfaceFX System Design (2026-07-22)

**ステータス:** In Progress

## 目的

カメラレンズ、窓、ガラス、鏡、画面などの「表面」に対して、映像の上に自然に乗る傷・水滴・結露・汚れを、時間変化付きで編集・プレビュー・書き出しできる SurfaceFX システムを追加する。

最初の代表ユースケースは次の二つとする。

- カメラレンズ表面の細い傷、擦り傷、汚れ、レンズフレアを伴う傷
- 窓ガラス表面を流れる水滴、合流する水滴、残る水跡

## 設計方針

- SurfaceFX はレイヤーコンポーネントではなく、`ArtifactEffectTabSurface` の effect stack で管理する。
- 対象は「カメラ」「窓」というオブジェクト名ではなく、2D/3D の surface anchor として表現する。
- 表面効果は元映像を破壊せず、入力映像・表面マスク・法線/深度情報に対する後段の effect pass とする。
- 傷や水滴の形状は素材画像の焼き込みにせず、seed 付きのパラメトリック生成または明示的なマスクで再現可能にする。
- 時間変化は乱数の再生成ではなく、composition time と固定 seed から決定的に評価する。
- 新しい Qt 合成、`QPainter`、`QImage` のホットパス採用は行わない。既存の GPU レンダーパス、または `ImageF32x4_RGBA` の専用 CPU フォールバックを使用する。

## 概念モデル

```text
Input Surface
  ├─ Surface Anchor      (画面座標 / 平面 / 3D surface)
  ├─ Surface Mask        (適用範囲・エッジ・フェザー)
  ├─ Surface Field       (傷、水滴、結露などの要素群)
  └─ Surface Response    (歪み、反射、遮蔽、ハイライト、濡れ)
          ↓
     SurfaceFX Pass
          ↓
     Composite Output
```

### Surface Anchor

SurfaceFX がどの面に貼り付くかを定義する。

- `ScreenSpace`: コンポジション画面に固定。カメラレンズ表現の初期実装に使う。
- `Planar`: 四隅または矩形の平面に固定。窓、ディスプレイ、鏡に使う。
- `TrackedPlanar`: 将来のトラッキング結果を受ける平面。初期版ではデータ契約だけ用意する。
- `WorldSurface`: 将来の 3D レンダリング対象。初期版では実装対象外。

### Surface Field

表面上の要素を共通の要素配列として持つ。要素ごとに以下を持つ。

- 種別: `Scratch`, `Droplet`, `Streak`, `Condensation`, `Dirt`
- 位置、サイズ、回転、深さ順
- 強度、透明度、粗さ、色味
- ランダム seed と seed offset
- 表示開始/終了、成長、消滅、移動速度
- マスクへの影響範囲

## エフェクト種別

### Scratch

細線、分岐、擦過痕の組み合わせで傷を生成する。

- 長さ、幅、分岐率、曲率、密度
- 傷の芯、周辺の白化、暗い溝、微細な反射
- カメラ向けには中心から放射状、窓向けにはランダム方向をプリセット化
- 適用量が高い場合でも映像を完全に隠さず、反射成分と局所コントラストで傷を見せる

### Droplet

水滴の輪郭、内部の屈折、ハイライト、背後映像の歪みを組み合わせる。

- 楕円率、体積、縁の厚み、濡れ領域
- 重力方向、速度、加速度、粘性
- 停止、流下、合流、分裂、消滅の状態
- 背後映像の屈折量と水滴のハイライト方向

水滴は完全な流体シミュレーションを初期要件にしない。まずは決定的な粒子/カーブ評価で成立させ、必要になった場合だけ GPU シミュレーションへ拡張する。

### Condensation / Streak / Dirt

共通の Field 要素として後から追加する。結露は低周波の濁りと水滴マスク、Streak は流下後に残る細長いマスク、Dirt は低コントラストの粗い遮蔽として実装する。

## 時間評価

`SurfaceFXEvaluator` は `compositionTime`, `frameRate`, `surfaceSeed`, `elementId` から各要素の状態を評価する。

- 同じフレームを再描画しても同じ結果になる。
- シーク後も水滴位置が飛ばない。
- render queue と preview で同じ評価結果を得る。
- 物理らしさは `gravity`, `viscosity`, `adhesion`, `wind` の少数パラメータで制御する。

## レンダリングパス

### GPU 優先

1. 入力 surface を既存の render target から受け取る。
2. Surface Mask と field atlas を生成または更新する。
3. 傷・水滴の距離場/マスクを評価する。
4. 必要な場合だけ歪み用の中間 target を作る。
5. 遮蔽、反射、ハイライト、屈折を一つの SurfaceFX pass にまとめる。
6. effect stack の blend / opacity / mask 契約に従って出力する。

### CPU フォールバック

GPU 非対応時は、既存の `ImageF32x4_RGBA` と専用の SurfaceFX 合成処理を使う。水滴の屈折は低品質近似に落とし、プレビューと書き出しの結果契約は維持する。

## データ契約案

```text
SurfaceFXEffect
  enabled
  anchorType
  anchorTransform
  surfaceMask
  fieldSeed
  response
  elements[]

SurfaceFXElement
  id
  type
  normalizedBounds
  parameters
  timing
  motion
  seedOffset
```

シリアライズでは enum を整数で保存せず、`"scratch"`、`"droplet"` のような安定した文字列を使う。未知の要素種別は読み込み時に無効化して保持し、将来バージョンで復元できるようにする。

## UI 配置

`ArtifactEffectTabSurface` に SurfaceFX effect を追加する。通常の Property Widget に SurfaceFX の詳細項目を露出させない。

必要な編集面:

- `Surface`: anchor type、四隅/矩形、mask、feather
- `Field`: element type、seed、density、preset
- `Motion`: gravity、viscosity、wind、speed
- `Response`: distortion、wetness、reflection、opacity
- `Preview`: 表示品質、field overlay、再現 seed、現在フレームの評価状態

初期プリセット:

- `Camera Lens / Fine Scratches`
- `Camera Lens / Finger Smudge`
- `Window / Light Rain`
- `Window / Heavy Rain`
- `Window / Condensation`

## 実装段階

### SFX-1: データ契約と静止 Surface

- `SurfaceFXEffect` / `SurfaceFXElement` の型と JSON 契約
- ScreenSpace / Planar anchor
- Scratch の静止マスク
- Effect stack からの有効化と保存/復元

完了条件: 画面または矩形平面に傷を固定表示し、保存/再読込して見た目が一致する。

進捗メモ（2026-07-24）: CPU fallback による Scratch / Streak / Droplet / Dirt /
Condensation の静止オーバーレイ、anchor feather、JSON 復元時の矩形正規化を実装。
Planar は正規化矩形として扱い、GPU pass と専用編集面は未実装。

### SFX-2: 決定的アニメーション

- `SurfaceFXEvaluator`
- 水滴の流下、停止、消滅
- seed とシークの安定性

完了条件: 同一フレームの preview/render queue 結果が一致し、シークで要素が再配置されない。

進捗メモ（2026-07-24）: `EffectContext::timeSeconds` と `fieldSeed` /
`seedOffset` による水滴・ストリークの決定的な流下と in/out 時間評価を実装。
独立した `SurfaceFXEvaluator` と停止・消滅・GPU parity の検証は未実装。

### SFX-3: Surface Response

- 水滴の遮蔽、ハイライト、低コスト屈折
- 傷の反射/コントラスト response
- GPU pass と CPU fallback の品質段階

完了条件: 水滴が単なる半透明円ではなく、背後映像の局所変形と反射を持つ。

進捗メモ（2026-07-24）: CPU fallback に水滴のリム／ハイライトと、アルファ場を
利用した最大±2pxの局所サンプリングを追加。GPU pass、品質段階制御、完全な屈折モデルは未実装。

### SFX-4: 編集 UX とプリセット

- Effect 専用編集面
- element 選択と viewport overlay
- 5 種の初期プリセット
- density/quality の preview 制御

完了条件: カメラレンズと窓の代表例を、コード編集なしで作成できる。

## 非目標

- 初期版での完全な流体シミュレーション
- 自動カメラトラッキングそのもの
- 3D ガラス材質や屈折レンダリングの置き換え
- SurfaceFX を通常のレイヤープロパティグループへ追加すること

## 検証項目

- 静止傷の座標、回転、mask、feather
- 水滴の連続移動、合流、画面端での消滅
- シーク、停止、逆再生、フレームステップ
- preview と render queue の同一性
- 複数 SurfaceFX の重なり順と effect stack 順序
- 高密度時の処理時間、field atlas の再利用、GPU/CPU fallback
- 不明 enum、旧バージョン JSON、欠損 preset の復元

## 次の実装判断

最初の実装対象は `SFX-1` とする。カメラと窓を別システムに分けず、`ScreenSpace` と `Planar` の二つの anchor で両方を表現できることを確認してから、水滴の時間評価を追加する。
