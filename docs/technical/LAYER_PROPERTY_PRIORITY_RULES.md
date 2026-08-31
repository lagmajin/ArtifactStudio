# レイヤー別プロパティ優先度ルール

**最終更新:** 2026-08-31

## 目的

この文書は、通常の `ArtifactPropertyWidget` に何を優先表示し、何を
Components / Effects の専用面へ置くかを定める。レイヤー固有の主要な編集を
最初に見せ、設定項目の増加によって基本操作が埋もれることを防ぐ。

値の保存、アニメーション、property path の解決規則は
`LAYER_PROPERTY_ENVELOPE_ARCHITECTURE_2026-07-24.md` を正とする。本書は表示順と
編集導線の規約だけを扱う。

## 共通ルール

1. 通常 Properties の先頭は常に、そのレイヤーを成立させる固有項目にする。
   例: Text の本文、Image のソース、Shape のパスと見た目。
2. `Transform` は全ての可視レイヤーで共通の標準グループとし、固有項目の次に置く。
3. `Opacity`、Blend、Parent、時間範囲などの共通レイヤー制御は、固有項目と
   Transform を妨げない下位グループに置く。
4. Components 由来の詳細設定（Collision、Layout、Cloner、Crowd、Particle Emitter、
   Fluid 等）は Components 専用面を正規の導線とする。通常 Properties へそのまま
   複製しない。
5. Effects の有効化、順序、パラメータは Effects 専用面を正規の導線とする。
6. 例外として、ユーザーが通常 Properties での操作を明示的に求めた場合だけ、
   コンポーネント由来の「主要な1〜3項目」を下位の `Behavior` または `Dynamics`
   セクションに置ける。詳細値・構造設定は Components に残す。
7. 表示名の偶然一致では分類しない。property path とレイヤー型・component ownership
   を使って判定する。

## 表示優先度

| 優先度 | 内容 | 例 | 正規の編集面 |
|---|---|---|---|
| P0 | レイヤー固有の主編集 | Text 本文、Image source、Shape path | Properties |
| P1 | 見た目を決める固有値 | Font、Fill、Stroke、Crop、Model material | Properties |
| P2 | 共通 Transform | Position、Scale、Rotation、Anchor | Properties / Timeline |
| P3 | 共通レイヤー制御 | Opacity、Blend、Parent、Timing | Properties / Timeline |
| P4 | 明示許可された簡易 Behavior | Fall Profile、Gravity Scale | Properties の下位 section |
| P5 | 構造・詳細コンポーネント | Collider shape、Joint target、Emitter modules | Components |
| P6 | エフェクト stack と詳細値 | Blur、Color Correct、Mask-aware effect | Effects |

同一グループ内では、頻度が高く結果がすぐ見える値を先に置く。生の内部値、単位系を
理解しないと使えない値、破壊的または広域に作用する値は末尾に置き、`Advanced` と明示する。

## レイヤー型ごとの P0/P1

| レイヤー | P0: 最初に見せる項目 | P1: 次に見せる項目 | Components / Effects へ置くもの |
|---|---|---|---|
| Image / Image Sequence | Source、Replace / Relink | Crop、Interpretation、Sequence timing | Tracking、Collision、Color effects |
| Text | Text content | Font、Size、Fill、Paragraph、Text Animator の要約 | 詳細 Animator、Layout、Effects |
| Shape | Path / Primitive、Fill | Stroke、Corner、Repeater の要約 | 詳細 path operators、Cloner、Physics |
| Solid / Color | Color、Size | Edge / Gradient 相当の固有値 | Effects、Mask、Physics |
| 3D Model | Model source | Material slot、Render mode、基本表示 | Rig、Animation clip、詳細 material / physics |
| Camera | Projection、Focal Length | Focus、DOF の基本値 | 詳細 DOF、Constraint、Rig |
| Light | Type、Intensity、Color | Cone、Shadow の基本値 | 詳細 shadow / volumetric 設定 |
| Audio | Source、Gain | Pan、Mute、Timing | Mixer routing、analysis、effects |
| Particle | Emitter preset、Amount | Lifetime、Size、Color | Emitter module、Collision、Fluid |
| Adjustment | 対象範囲・有効状態 | Blend / opacity | Effect stack と全 effect parameter |
| Null / Controller | Name、Transform | Controller 表示値 | Constraint / expression の詳細 |

## Physics / Dynamics の例外ルール

Physics は Components が正規の編集面である。通常 Properties に出す場合は、明示要求
された「落ち方」の調整に限る。

- `Fall Profile`、`Gravity Scale`、`Air Drag`、`Angular Drag` は P4 として許可する。
- `World Gravity Y` はコンポジション内の Box2D world 全体へ作用するため、
  `Advanced` 表示にし、通常のレイヤーごとの調整には使わせない。
- Collider shape / bounds、floor、restitution、joint target、mass、friction、初速などの
  詳細値は Components に置く。
- Components に表示する値を通常 Properties に重複表示する場合は、同じ property path を
  共有し、保存値やアニメーション経路を分岐させない。

## 実装時の確認項目

- 新しいプロパティは、レイヤー固有か、共通か、component/effect 所有かを先に決める。
- P0/P1 を押し下げる新しい group を作らない。
- 通常 Properties に component 項目を追加する場合は、例外理由と表示する最小項目数を
  レビューまたは変更記録に残す。
- property path が既存の Components / Effects 導線と重複する場合、値の二重保持を作らない。
- Timeline 左ペインは `Transform` のみを標準表示とし、上記の Properties 構成を
  タイムラインへ複製しない。
