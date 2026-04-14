# マイルストーン: Composition Final Effect System

> 2026-04-14 作成

## 目的

コンポジション全体に適用される最終段のエフェクトシステムを実装する。

レイヤー / エフェクトスタックの後段で、**出力直前に 1 回だけ効く処理**を扱う。

---

## 背景

制作中に以下のような「コンポ全体に掛ける処理」が必要になる：

- 「全体の色調を統一したい（カラーグレーディング / LUT）」
- 「最後に全体ブラー / グロー / シャープをかけたい」
- 「HDRで制作→SDR出力のトーンマッピング」
- 「解像度・bit深度の最終調整」

現在はレイヤー単位のエフェクトしかないため、**「全レイヤー合成後」の処理**が存在しない。

---

## 概念

### Composition Final Effect

**何を保存するか:**
- エフェクトスタック（順序付き）
- 各エフェクトのパラメータ
- キーフレーム（アニメーション対応）
- 有効/無効状態

**どこに配置するか:**
```
[全レイヤー合成済み画像]
         ↓
[Composition Final Effect Stack]  ← ここ
         ↓
    [最終出力]
```

**使いどころ:**
- 全体色調（カラーグレーディング / LUT）
- 最終ブラー / グロー / シャープ
- トーンマッピング / HDR→SDR
- 出力依存処理（解像度・bit深度）

**特徴:**
- レイヤーエフェクトより後の最終段
- コンポジション単位で1つ
- before/after比較可能
- レンダー出力時にも適用される

---

## フェーズ設計

### Phase 1: Data Contract

**目的:**
最終エフェクトのデータ構造を定義する。

**作業項目:**
- `CompositionFinalEffect` struct/class
  - エフェクト種別
  - パラメータ
  - 有効/無効
  - キーフレーム対応
- `CompositionFinalEffectStack`
  - 順序付きエフェクトリスト
  - 追加/削除/並べ替え
- シリアライズ/デシリアライズ

**完了条件:**
- JSON保存/読み込み可能
- 複数エフェクトの順序保持

---

### Phase 2: Render Integration

**目的:**
レンダリングパイプラインに最終段を統合する。

**作業項目:**
- `CompositionRenderer` に final effect パスを追加
- 全レイヤー合成後に適用
- GPU post-process パス（Diligent backend）
- Software fallback パス

**完了条件:**
- プレビューに反映される
- レンダー出力にも適用される

---

### Phase 3: UI Surface

**目的:**
Final Effect を編集するUIを実装する。

**作業項目:**
- Inspector に「Composition Final Effect」セクション
- エフェクト追加/削除/並べ替え
- パラメータ編集
- before/after トグル
- 効果強度スライダー

**完了条件:**
- UIからエフェクト追加・編集・削除できる
- リアルタイムプレビュー

---

### Phase 4: Effect Pack

**目的:**
主要な final effect を実装する。

**作業項目:**
- **Color Grading**
  - Color Wheels（シャドウ/ミッド/ハイライト）
  - Curves（RGB/ルミナンス）
  - LUT適用（.cube等）
  - **Hue vs Hue / Hue vs Sat / Hue vs Luma**
    - 特定色相範囲だけを補正
    - 肌色補正など局所調整に最適
    - DaVinci系では標準、AEは弱い
  - **フィルムカーブ（Toe / Shoulder）**
    - ハイライトの粘り、シャドウの潰れを制御
    - S字じゃないフィルム特性カーブ
    - 単なるカーブでは再現困難
  - **トーン分離（Tonal Split）**
    - ハイライト/シャドウを別色に
    - LUTより軽くて直感的
    - 映像の「空気感」を作る
    - AEは弱い、映像系では常識
- **Blur / Sharpen**
  - Gaussian Blur（全体）
  - Sharpen
  - Glow
- **Tone Mapping**
  - HDR→SDR（Reinhard / ACES）
  - Exposure / Gamma
  - Vignette
- **Output**
  - Resolution scale
  - Dithering
  - Grain

**重要設計判断:**
Hue vs Hue/Sat/Luma、フィルムカーブ、トーン分離は
Composition Final Effect 専用ではなく、**レイヤーエフェクトとしても使える汎用実装**にする。

これにより：
- レイヤー単体の色調整にも使用可能
- Composition Final Effect でも使用可能
- コードの再利用性最大化

実装は `CreativeEffect` の拡張、または新 `ColorTransform` 系クラスとして行う。

**完了条件:**
- 各エフェクトがGPU/CPU両対応
- before/after比較可能

---

## Non-Goals

- レイヤー単位のエフェクト（既存のEffect Stack）
- 部分適用（Rect / Mask）
- リアルタイム共同編集
- サードパーティプラグインホスト

---

## 技術方針

### GPU Post-Process

```text
Final Effect Pipeline
├─ Full-screen quad pass
├─ Compute shader path（可能なら）
└─ CPU fallback（software renderer）
```

### Effect 定義

```text
CompositionFinalEffect
├─ type (enum)
├─ parameters (map)
├─ keyframes (optional)
├─ enabled (bool)
└─ opacity (0.0-1.0)
```

### 保存場所

```json
{
  "composition": {
    "finalEffects": [
      {
        "type": "colorGrading",
        "enabled": true,
        "parameters": { ... },
        "keyframes": { ... }
      },
      {
        "type": "vignette",
        "enabled": true,
        "parameters": { ... }
      }
    ]
  }
}
```

---

## 関連マイルストーン

- `MILESTONES_BACKLOG.md` - M-FX-8 Composition Final Effect
- `M-FX-1` Inspector Effect Stack Bridge
- `M-FX-5` GPU Effect Parity
- `M-FX-7` Partial Application
- `Artifact/src/Render/` - レンダリングパイプライン
- `ArtifactCore/include/Graphics/Effect/` - エフェクト定義

---

## 完了条件全体

- [ ] Phase 1: Data Contract
- [ ] Phase 2: Render Integration
- [ ] Phase 3: UI Surface
- [ ] Phase 4: Effect Pack

---

## 現状

2026-04-14 時点で未着手。

バックログ（M-FX-8）として存在するのみ。
実装は空き時間に段階的に進める。
