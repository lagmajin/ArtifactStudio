# MILESTONE_PRODUCTION_1_0_PRIORITY_AND_ROADMAP_2026-04-20

## 制作アプリ 1.0 に向けた優先順位と 6 か月ロードマップ

ArtifactStudio を実務投入できる制作ツールへ寄せるための整理メモ。
この文書は機能の総量ではなく、**1.0 に必要な土台**を先に固める順番を定義する。

---

## 判断基準

### 必須
これが無いと「1.0」と言い切りにくい、もしくは制作体験が破綻しやすいもの。

### 重要
制作効率を大きく押し上げるが、最初の移行障壁ではないもの。

### 後回し
拡張性や将来性には効くが、1.0 の到達条件としては後段に回せるもの。

---

## 1. 必須

### 1-1 Host / Context / ROI / Property Core
- RenderContext / ROI / Property Registry をホスト共通の契約にする
- effect / layer / render queue を UI から切り離して読めるようにする
- dependency declaration の入口を用意する
- 参照: [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md)

### 1-2 Timeline / Selection / Service Boundary Hardening
- selection の二重経路をなくす
- `NoLayer` の理由追跡を安定させる
- `*Service` 以外からの直参照を減らす
- v1.0 の「壊れない基本操作」を作る

### 1-3 Render Queue End-to-End
- 登録 -> 実行 -> 完了/失敗理由表示まで通す
- export / dummy output / recovery を安定化する
- 参照: [`Artifact/docs/MILESTONE_V1_0_PRODUCTION_READINESS_2026-03-11.md`](X:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_V1_0_PRODUCTION_READINESS_2026-03-11.md)

### 1-4 Recovery / Diagnostics
- crash / failure / render job の切り分けを一本化する
- ログだけで一次診断できる状態に寄せる
- セーフモード / 復旧導線を明確化する

### 1-5 Track Matte / Mask / Shape Core
- track matte の表現力を実務レベルに近づける
- mask / roto / shape path を同じ編集基盤に寄せる
- shape layer を「表示できる」だけでなく「編集できる」側に寄せる
- 参照: [`docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md)

### 1-6 Keyframe Interpolation / Easing Graph
- 線形補間だけの状態から抜ける
- speed graph / value graph / bezier handle を整理する
- v1.0 の制作感を支える核

### 1-7 Motion Blur
- シャッター角度 / 位相 / サンプル数を揃える
- アニメーション品質を大きく底上げする

### 1-8 Default Workspace Layout
- Main / Timeline / Inspector / RenderQueue を初期状態で作業可能にする
- 初回起動時の「どこを触ればいいか分からない」を減らす

---

## 2. 重要

### 2-1 Adjustment Layer / Layer Styles
- 全体に効く調整レイヤー
- Drop Shadow / Glow / Stroke などの基本レイヤースタイル

### 2-2 Work Area / Preview Range
- ループ / プレビュー / レンダー範囲を読みやすくする

### 2-3 Parent / Child / Precompose / Frame Blending
- 親子伝搬の完全化
- プリコンポーズ
- フレームブレンディング

### 2-4 Markers
- composition marker / layer marker
- 作業の目印とレビュー導線を整える

### 2-5 Expression Engine v1
- 基本の式とプロパティ参照
- property registry とつながる前提で入れる

### 2-6 Color Management
- linear / OCIO / LUT の整理
- export / preview の見え方を合わせる

### 2-7 Proxy / Cache / Playback Stability
- preview の再現性
- frame cache / RAM preview / proxy の導線整理

---

## 3. 後回し

### 3-1 OFX / Plugin SDK の完全互換
- Host-Plugin boundary の adapter は先に必要だが、完全な外部 SDK 化は後段でよい

### 3-2 Deep Composite / Multi-channel / HDR の上位拡張
- 重要ではあるが、1.0 の最低ラインにはまだ直結しない

### 3-3 Collaboration / Team Project
- 将来価値は高いが、1.0 の核ではない

### 3-4 AI Helpers
- 補助としては魅力的だが、制作基盤の完成を先にする

### 3-5 Advanced 3D / Simulation / Experimental Effects
- Particle / fluid / fracture / advanced rig などは別ブロックで進める

---

## 4. 6 か月ロードマップ

### Month 1
#### 目標
基盤の契約を固定して、壊れやすい UI 経路を安定させる。

#### 着手順
1. Host / Context / ROI / Property Core
2. Timeline / Selection / Service Boundary Hardening
3. Render Queue End-to-End の前半

### Month 2
#### 目標
合成の核を作る。

#### 着手順
1. Track Matte
2. Mask / Roto / Shape Core
3. Adjustment Layer の土台

### Month 3
#### 目標
アニメーションの「気持ちよさ」を補強する。

#### 着手順
1. Keyframe Interpolation / Easing Graph
2. Motion Blur
3. Work Area / Preview Range

### Month 4
#### 目標
実務運用に必要な安定性を揃える。

#### 着手順
1. Recovery / Diagnostics
2. Render Queue End-to-End の後半
3. Default Workspace Layout

### Month 5
#### 目標
大きめの制作物を扱うための編集機能を足す。

#### 着手順
1. Parent / Child
2. Precompose
3. Frame Blending
4. Markers

### Month 6
#### 目標
1.0 の外側に繋がる拡張層を固める。

#### 着手順
1. Expression Engine v1
2. Color Management
3. Proxy / Cache / Playback Stability
4. OFX / Plugin SDK の adapter 仕込み

---

## 5. 実装方針

- まず read-only / adapter を入れる
- 既存の動作は急に壊さない
- UI の見た目修正より、契約と順序の統一を先にやる
- らしさは「機能数」より「迷わず使える順序」で作る

---

## 6. 参照

- [`docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md)
- [`docs/planned/MILESTONE_PRODUCTION_FEATURE_ENHANCEMENT_ROADMAP_2026-04-12.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE_FEATURE_ENHANCEMENT_ROADMAP_2026-04-12.md)
- [`docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md)
- [`Artifact/docs/MILESTONE_V1_0_PRODUCTION_READINESS_2026-03-11.md`](X:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_V1_0_PRODUCTION_READINESS_2026-03-11.md)
- [`docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md)

---

## Static audit follow-up (2026-07-25)

現行ソース上の到達状況を再確認した。ビルド・実機操作による完了判定はまだ行っていない。

| 区分 | 状況 | 判定 |
|---|---|---|
| 必須 1-1 Host / Context / ROI / Property | `ArtifactRenderContext`、`ArtifactRenderROI`、`ExposedPropertyRegistry`、render queue の分離基盤が存在する。全 effect/layer の dependency declaration 統一は未確認。 | 部分実装 |
| 必須 1-2 Timeline / Selection | `ArtifactLayerSelectionManager`、Core `SelectionManager`、Playback/Timeline のサービス境界がある。二重経路と `NoLayer` の全経路整理は未確認。 | 部分実装／確認待ち |
| 必須 1-3 Render Queue E2E | `ArtifactRenderQueueService`、manager widget、preset、job panel と render queue テストが存在する。実ファイル export、失敗理由、recovery の E2E は未確認。 | 部分実装／E2E待ち |
| 必須 1-4 Recovery / Diagnostics | CrashHandler、DiagnosticEngine、ProjectDiagnostic、SessionLedger、各種 debug/profiler widget がある。セーフモードから復旧までの一連の導線は未確認。 | 基盤あり／統合待ち |
| 必須 1-5 Track Matte / Mask / Shape | Mask/Roto/Shape のコアと rasterizer/cutout pipeline、PreCompose 関連がある。matte 順序・複数マスク・編集同期の実運用は未確認。 | 部分実装 |
| 必須 1-6 Keyframe / Easing | KeyframeEditingTools、EasingCurveUtil、CurveEditor、Bezier/hold/roving の基盤がある。speed/value graph の完全な保存・編集整合性は未確認。 | 部分実装 |
| 必須 1-7 Motion Blur | Core `Graphics.MotionBlur` と Artifact 側 `ArtifactMotionBlur` が存在する。shutter/phase/sample のレンダー統合は未確認。 | 部分実装／統合待ち |
| 必須 1-8 Default Workspace | MainWindow、WorkspaceManager/Modes、Timeline/Inspector/RenderQueue の widget 群がある。初回起動の標準レイアウトと復元は未確認。 | 部分実装 |
| 重要 2-1〜2-7 | Adjustment/LayerStyle、Work Area、Parent/PreCompose/Frame Blending、Markers、Expression、OCIO/LUT、Proxy/Cache/Playback の個別基盤は複数存在するが、制作フローを通した統合完了は確認できない。 | 基盤あり／順次統合 |
| 後回し 3-1〜3-5 | Plugin/OFX、Deep Composite/HDR、Collaboration、AI、3D/Simulation は個別の実装・計画が混在する。1.0 必須の完了条件には含めない。 | 後段扱い |

### 現時点の優先順位

ロードマップの順序は維持する。次に着手するのは、必須 1-1〜1-3 の契約を実行経路で確認できる診断・E2E 検証と、必須 1-5〜1-7 の合成・補間品質の確認。ソース上のクラス存在だけでは 1.0 完了とは扱わない。
