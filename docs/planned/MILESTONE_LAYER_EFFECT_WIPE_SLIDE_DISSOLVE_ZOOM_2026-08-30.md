# マイルストーン: Layer Effect Wipe / Slide / Dissolve / Zoom (2026-08-30)

**最終更新:** 2026-08-30

> 2026-08-30 作成

## 目的

`Artifact/src/Effects/LinearWipe/` と同じパターンで、**AE の Wipe 系 5 個 + Slide / Dissolve / Zoom 系 8 個をレイヤー effect として追加**する。Effects & Presets パネルの「Transitions」カテゴリを AE 寄りに揃え、AE ライクな制作導線を整える。

参考: 関連 [MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md](MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md)（surface 演出）、[MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md](MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md)（hot path 安定性）

## 背景

Artifact の `Artifact/src/Effects/` 配下には 70+ のレイヤー effect が揃うが、**Transitions ディレクトリ自体が存在せず**、`LinearWipe/` 1 個だけが独立ディレクトリで GPU effect として露出している。AGENTS.md「動画対応は当面後回し」「GPU/Diligent 経路を優先」と整合する一方、AE の Effects & Presets パネルに相当する Wipe / Slide / Dissolve / Zoom カテゴリは **未完成**。

`ArtifactCore/include/Video/Transitions/` には NLE（Non-Linear Editing）用の動画トランジション 16 種が揃うが、`Artifact/` からの参照は 0 件で UI 露出なし（別系統の話）。本マイルストーンは **レイヤー effect としてのトランジション追加**に絞り、NLE 動画トランジション露出は別マイルストーン扱いとする。

## スコープ：13 effect

### A 段階：Wipe 系 5 個

| 追加 effect | 主要プロパティ | 視覚的効果 |
|---|---|---|
| **RadialWipeEffect** | `centerX, centerY, startAngle, feather` | 中心から放射状に広がる円形ワイプ。AE の Radial Wipe 相当 |
| **IrisWipeEffect** | `shape (Circle/Square/Diamond/Star), feather` | 幾何形状で内側から外側へ展開。AE の Iris Wipe 相当 |
| **GradientWipeEffect** | `gradientMap, softness, startPoint, endPoint` | 1D gradient 画像で白部分から順に透過。AE の Gradient Wipe 相当 |
| **ClockWipeEffect** | `centerX, centerY, feather` | 時計の針が回るように円弧ワイプ。AE の Clock Wipe 相当 |
| **BlockDissolveEffect** | `blockSize, randomness, seed` | ブロック単位でディゾルブ。AE の Block Dissolve 相当 |

### B 段階：Slide / Dissolve / Zoom 系 8 個

| 追加 effect | 主要プロパティ | 視覚的効果 |
|---|---|---|
| **SlideEffect** | `direction (N/E/S/W/NE/NW/SE/SW), softness` | 始点から終点へレイヤー自体が平行移動しながら消える。AE の Slide 相当 |
| **PushEffect** | `direction` | 次のレイヤーが手前へ押し出す。AE の Push 相当 |
| **SlidingDoorsEffect** | `direction, axis (Horizontal/Vertical)` | 二枚の扉が開くように分割。AE の Sliding Doors 相当 |
| **CrossDissolveEffect** | （`progress` のみ） | 2 レイヤー間のクロスフェード。AE の Cross Dissolve 相当 |
| **DipToBlackEffect** | `dipColor` | 黒フェードイン・アウト。AE の Dip to Black 相当 |
| **DipToWhiteEffect** | `dipColor` | 白フェードイン・アウト。AE の Dip to White 相当 |
| **ZoomEffect** | `zoomStart, zoomEnd, centerX, centerY` | 中心から拡大／縮小しながらの遷移。AE の Zoom 相当 |
| **ZoomBoxesEffect** | `boxCount, randomness, seed` | 分割ボックスでランダム順ズーム。AE の Zoom Boxes 相当 |

## Goal

- AE の Wipe 系 7 個のうち **6 個が露出**（Linear 既存 + 5 個追加）、Slide 系 3 個、Dissolve 系 3 個、Zoom 系 2 個が揃う
- 13 effect すべてが `ArtifactAbstractEffect` パターン + CPU/GPU AUTO 切替を持つ
- 13 effect すべてが `ImageF32x4RGBAWithCache` 経由でメモリ整合
- 13 effect すべてが `getProperties()` / `setPropertyValue()` で Inspector と接続、Undo/Redo が効く

## Non-Goals

7. **B 段階の slide / push 系は「parallel image blend」** — 既存 LinearWipe のような単一 image mask ではなく、layer 2 枚の合成が必要。`ArtifactAbstractEffect` の `applyCPU(src, dst)` 規約に合わせるため、dst の alpha を 0 にして背景を露出させる形で表現

## 共通実装パターン

各 effect は以下の構造を持つ（LinearWipe を雛形に差分のみ）:

```cpp
// Artifact/include/Effects/<Name>/<Name>Effect.ixx
export class <Name>Effect : public ArtifactAbstractEffect {
    // プロパティ getter / setter
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    bool supportsGPU() const override { return true; }
};

// Artifact/src/Effects/<Name>/<Name>Effect.cppm
class <Name>EffectCPUImpl : public ArtifactEffectImplBase {
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};
class <Name>EffectGPUImpl : public ArtifactEffectImplBase {
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};
```

`CMakeLists.txt` への登録は `Artifact/src/Effects/<Name>/` 配下の `.cppm` / `.ixx` を `file(GLOB_RECURSE)` で自動発見する AGENTS.md ルールに従う（`Artifact/CMakeLists.txt` を確認し、明示 force list 追加が必要か判定）。

## A 段階 Phases（Wipe 系 5 個）

### Phase 1: RadialWipeEffect

- 目的: 中心から放射状に広がる円形ワイプ。LinearWipe に次いで需要が高い
- 差分ポイント:
  - 中心座標 (`centerX`, `centerY`) を追加
  - `angle` を円弧 (`startAngle` 単一値) に簡略化
  - GPU shader: 各ピクセルの中心からの角度を計算し、円弧境界との内外判定
- DoD:
  - 単体 effect としてレイヤー追加できる
  - CPU/GPU 両方が動作
  - Inspector で centerX/centerY/startAngle/feather を編集できる
  - 既存 LinearWipe と視覚的に区別できる

### Phase 2: IrisWipeEffect
  - 同じ `seed` で同じパターン再現

## B 段階 Phases（Slide / Dissolve / Zoom 系 8 個）

### Phase 6: SlideEffect

- 目的: 始点から終点へレイヤー自体が平行移動しながら消える
- 差分ポイント:
  - `direction` enum 追加（N/E/S/W/NE/NW/SE/SW、計 8 方向）
  - `progress` に応じて `dst` の透明領域を縦または横に広げる
  - GPU shader: ピクセル位置が `progress` 領域内なら alpha 0、それ以外は src そのまま
- DoD:
  - 8 方向の平行移動トランジション
  - `softness` で境界 softness
  - 既存 LinearWipe と視覚的に区別できる（平行移動 vs 角度ワイプ）

### Phase 7: PushEffect

- 目的: 次のレイヤーが手前へ押し出す
- 差分ポイント:
  - `direction` enum（N/E/S/W、4 方向で十分）
  - `progress` に応じて全レイヤー平行移動
  - Slide との違いは src 全体を移動（src の alpha はそのまま、座標だけスライド）
- DoD:
  - 4 方向の押し出しトランジション
  - src の alpha は保持、座標だけ平行移動

### Phase 8: SlidingDoorsEffect

- 目的: 二枚の扉が開くように分割
- 差分ポイント:
  - `axis` enum（Horizontal / Vertical）
  - 画像中心線で 2 分割し、それぞれを外側へ移動
  - 開いた領域は背景を露出
- DoD:
  - 水平 / 垂直の二軸選択
  - 既存 SlideEffect との視覚的差別化（2 分割同時開放）

### Phase 9: CrossDissolveEffect

- 目的: 2 レイヤー間のクロスフェード。AE の Cross Dissolve 相当
- 差分ポイント:
  - `progress` パラメータのみ（最もシンプル）
  - `dst` の alpha を `1.0 - progress` にする
  - GPU shader: `dst.rgb *= (1.0 - progress); dst.a *= (1.0 - progress)`
- DoD:
  - progress 0→1 で src → 完全に透明
  - 既存 LinearWipe と視覚的に区別できる（全体フェード vs 角度ワイプ）

### Phase 10: DipToBlackEffect

- 目的: 黒フェードイン・アウト
- 差分ポイント:
  - `progress` パラメータのみ
  - `dst.rgb` を `lerp(src.rgb, black, progress)`、`dst.a *= (1.0 - progress)`
- DoD:
  - 黒へのフェード
  - `dipColor` パラメータで任意の色へ拡張可能（後続）

### Phase 11: DipToWhiteEffect

- 目的: 白フェードイン・アウト
- 差分ポイント: DipToBlack とほぼ同じ、`dipColor = white`
- DoD:
  - 白へのフェード
  - 同じ `seed` で同じパターン再現

## Definition Of Done

- 13 effect すべてが `Artifact/src/Effects/<Name>/` 配下に揃う
- 13 effect すべてが CPU/GPU 両実装で動作
- 13 effect すべてが Inspector で編集でき、Undo/Redo が効く
- 13 effect すべてが Effects & Presets パネル（または相当 UI）から選択可能
- 13 effect すべてが AGENTS.md「D3D12 / Diligent backend 触るときは慎重」を満たす（雛形からの差分のみ）
- 既存 milestone（DCC-Feel-Gaps / VP+TL Hot-Path Stability / Property Editor 整備）の進行を妨げない
- ビルド・runtime 受入れが AGENTS.md に従いユーザー指示で実施される

## 既存 milestone との関係

- [MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md](MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-30.md) — Phase 4（Color Vocabulary）と同タイミングで、effect 内の色トークンも theme 経由に切替
- [MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md](MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md) — 13 effect 追加で `renderOneFrame()` 経路の重さが微増する想定。Phase 3 (renderOneFrame 統合) と並走する場合は事前計測

## Recommended Order

1. Phase 1（RadialWipe）— LinearWipe との差分が小さいので最速
2. Phase 2（IrisWipe）— 形状 enum 追加で enum 経由の色違いに展開できる
3. Phase 4（ClockWipe）— RadialWipe と似て非なる扇形。Phase 1 と並走可能
4. Phase 3（GradientWipe）— gradient map 入力が要る。Property Editor の image 系 UI 整備状況に依存
5. Phase 5（BlockDissolve）— 乱数 seed の JSON 永続化設計が要る。後段で着手
6. Phase 9（CrossDissolve）— 最もシンプル、B 段階の足場
7. Phase 10（DipToBlack）/ Phase 11（DipToWhite）— CrossDissolve の color 拡張
8. Phase 6（Slide）/ Phase 7（Push）— parallel image blend 系の雛形
9. Phase 8（SlidingDoors）— Slide を 2 分割同時開放に拡張
10. Phase 12（Zoom）— GPU 2D ジオメトリ
11. Phase 13（ZoomBoxes）— BlockDissolve のズーム版

## 想定効果

- AE の Transitions カテゴリのうち Wipe / Slide / Dissolve / Zoom 4 系統 18 個のうち **14 個が揃う**（Linear 既存 + 13 個追加）
- 制作側で「Wipe / Slide / Dissolve / Zoom」の選択肢から意図に合わせて選べる
- LinearWipe と同じパターンで 13 個追加するため、保守コストが予測可能
- 後続の C 段階（Layer Styles / 3D 9 個）への雛形資産になる

## Next Execution Slice

Phase 1（RadialWipeEffect）の最小着手点:

1. `Artifact/src/Effects/LinearWipe/` の cppm / ixx / ディレクトリ構造を雛形に `Artifact/src/Effects/RadialWipe/` を copy
2. `LinearWipeEffect` → `RadialWipeEffect` に rename
3. `applyCPU` の angle / softness ロジックを中心座標 + 円弧判定に差分編集
4. HLSL shader を `atan2` ベースの中心角計算に置換
5. `Artifact/CMakeLists.txt` の `GLOB_RECURSE` または force list に `RadialWipe` を追加
6. ビルドと runtime でレイヤー追加 → effect 選択 → 動作確認

完了条件: RadialWipeEffect がレイヤー effect として追加でき、CPU/GPU 両方で円形ワイプが描画され、Inspector で centerX/centerY/startAngle/feather を編集できる。

## 2026-08-30 現状確認

本マイルストーンは作成直後のため着手実績なし。Phase 1（RadialWipe）から着手可能。ビルド・runtime 受入れは AGENTS.md に従いユーザー指示待ち。

  - DipToBlack と並走し、Inspector で選択

### Phase 12: ZoomEffect

- 目的: 中心から拡大／縮小しながらの遷移
- 差分ポイント:
  - `zoomStart`, `zoomEnd`（0.0-2.0 程度）
  - `centerX`, `centerY`（基準点）
  - GPU shader: ピクセル位置を中心からの距離で scale、`progress` 補間
- DoD:
  - 0.0 → 1.0 progress で zoomStart → zoomEnd の補間
  - 中心点変更で非対称ズーム可能
  - 既存 LinearWipe と視覚的に区別できる（拡縮 vs 角度ワイプ）

### Phase 13: ZoomBoxesEffect

- 目的: 分割ボックスでランダム順ズーム
- 差分ポイント:
  - `boxCount` (4, 8, 16, 32)
  - `randomness` (0-100%)
  - `seed`
  - GPU shader: ボックス座標で `seed` ベース順序、`randomness` で進捗をずらす
- DoD:
  - boxCount で分割数選択
  - `randomness` でランダム性調整
  - 同じ `seed` で同じパターン再現



- 目的: 幾何形状（Circle/Square/Diamond/Star）で内側から外側へ展開
- 差分ポイント:
  - `shape` enum 追加（Circle / Square / Diamond / Star）
  - Diamond は Manhattan 距離、Star は cos + 5 角形マスク
  - GPU shader: 形状ごとの距離関数
- DoD:
  - 4 形状が選択可能
  - `feather` パラメータで境界 softness
  - 既存 LinearWipe と視覚的に区別できる

### Phase 3: GradientWipeEffect

- 目的: 1D gradient 画像（または 2D gradient map）で白部分から順に透過
- 差分ポイント:
  - `gradientMap` パラメータ（既存 `PropertyImage` 型を使うか、新規 1D gradient パラメータを追加）
  - 入力画像の輝度を見て gradient map の対応位置の alpha を参照
  - GPU shader: 入力 luminance → gradient map サンプリング → alpha 計算
- DoD:
  - 1D gradient（白→黒、白→透明など）がプリセット 4 種から選択できる
  - `softness` でグラデーション境界の softness
  - カスタム gradient map のロード経路は本マイルストーンではスコープ外、後続で扱う

### Phase 4: ClockWipeEffect

- 目的: 時計の針が回るように円弧ワイプ
- 差分ポイント:
  - `centerX`, `centerY`, `feather`（Phase 1 とほぼ同じ構造）
  - 0° から 360° まで回転する扇形ワイプ
  - GPU shader: 中心からの角度を `atan2` で計算し、扇形範囲内かを判定
- DoD:
  - 中心固定で回転する扇形ワイプ
  - 既存 RadialWipe との視覚的差別化（扇形 vs 全円）

### Phase 5: BlockDissolveEffect

- 目的: ブロック単位でディゾルブ。AE 独特の効果
- 差分ポイント:
  - `blockSize` (4, 8, 16, 32 ピクセル単位)
  - `randomness` (0-100%)
  - `seed` (乱数シード、JSON 保存対象)
  - GPU shader: ブロック座標で `seed` ベースの乱数オフセット、`randomness` でディゾルブ進捗をずらす
- DoD:
  - 4/8/16/32 ピクセルのブロックサイズで動作
  - `randomness` で 0%（順番ディゾルブ）〜 100%（完全ランダム）の調整
  - 同じ `seed` で同じパターン再現


- NLE 用トランジション（`ArtifactCore/Video/Transitions/*`）の UI 露出
- AE の全 Transitions カテゴリの実装（本マイルストーンは Wipe / Slide / Dissolve / Zoom 系のみ）
- 3D 遷移（Flip / Cube / Page Curl / Glitch）
- Layer Styles（Drop Shadow / Glow / Stroke / Bevel）
- 新規 signal/slot / QtCSS / `QColorDialog` / `QImage` 本流 / `QPainter::CompositionMode` の追加

## Design Principles

1. **LinearWipe を雛形にする** — 同じ `ArtifactAbstractEffect` 派生、CPU/GPU ImplBase パターン、`ImageF32x4RGBAWithCache` 経由
2. **雛形からの差分のみ編集** — AGENTS.md「D3D12 / Diligent backend / render path の低レベル実装を変更する場合は、推測で広く触らず、関連箇所を十分に読んで変更範囲を最小化すること」に従う
3. **プロパティは float / enum / QPointF 中心** — AGENTS.md「`QImage` の本流投入禁止」「`QPainter::CompositionMode` による合成実装禁止」を遵守
4. **Inspector / Undo / JSON ラウンドトリップ** — `getProperties()` / `setPropertyValue()` を `ArtifactAbstractEffect` の規約通り実装
5. **CPU fallback は必須** — `ComputeMode::AUTO` で GPU が無い環境でも CPU 実装が走る
6. **HLSL shader は effect 内に inline** — 各 effect の cppm 内に `static const char* kHlsl` として保持
7. **B 段階の slide / push 系は「parallel image blend」** — 既存 LinearWipe のような単一 image mask ではなく、layer 2 枚の合成が必要。`ArtifactAbstractEffect` の `applyCPU(src, dst)` 規約に合わせるため、dst の alpha を 0 にして背景を露出させる形で表現
