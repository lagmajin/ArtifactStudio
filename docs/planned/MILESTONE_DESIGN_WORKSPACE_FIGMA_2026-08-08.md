# Design Workspace — Figma-style UI Design Features (2026-08-08)

**最終更新:** 2026-08-08
**状態:** 計画

## 概要

ArtifactStudio の Design ワークスペースに Figma の主要 UI デザイン機能を取り入れ、モーション＋UIデザインの統合環境を実現する。After Effects のアニメーションと Figma のレイアウトデザインを単一ツールで完結させるのが目標。

## 現状のスコアカード

| Figma 機能 | 状態 | 備考 |
|-----------|------|------|
| Design/Animate モード | ✅ | `WorkspaceMode::Design` / `Animate` |
| Auto Layout（縦/横スタック） | ✅ | `parentAutoLayoutOffset()`。ギャップ、アライメント、セーフエリア |
| 絶対位置除外 | ✅ | `isAbsoluteLayoutLayer()`。1レイヤー単位で Auto Layout から離脱可能 |
| Design 時のレイヤー並べ替え | ✅ | Move ドラッグで `designReorderActive_` 発動 |
| 矩形・楕円シェイプ | ✅ | `ArtifactShapeLayer`。fill, stroke, cornerRadius |
| グラデーション塗り | ✅ | リニア/ラジアル（`fillType`） |
| カスタムパス | ✅ | ベジェ頂点編集 |
| テキストレイヤー | ✅ | フォント、サイズ、行間、字間 |
| 画像レイヤー | ✅ | インポート、crop、blend |
| マスク・アルファマット | ✅ | |
| エクスポート（Lottie/RmlUi/Gameface/Unity/Noesis） | ✅ | `Artifact/src/Export/` 以下に全実装済み |
| **制約システム（左/右/上/下ピン）** | ❌ | |
| **シェイプブーリアン演算** | ❌ | |
| **ドロップシャドウ（レイヤー効果）** | ❌ | |
| **コンポーネント/インスタンス** | ❌ | |
| **フレームツール** | ❌ | |
| **スタイルライブラリ** | ❌ | |
| **スライスツール** | ❌ | |

---

## Phase 1: 制約システム（Constraints）

**Figma の Constraints** は「親フレームのサイズが変わったとき、この子レイヤーはどう振る舞うか」を定義する。UI のレスポンシブデザインに必須。

**仕様**:

```
プロパティ:
  constraintLeft  : bool   // 親の左端からの距離を維持
  constraintRight : bool   // 親の右端からの距離を維持
  constraintTop   : bool   // 親の上端からの距離を維持
  constraintBottom: bool   // 親の下端からの距離を維持

挙動ルール:
  - 対向する辺の両方をピン → 引き伸ばし（stretch）
  - 片側のみピン → その辺への距離を維持（固定サイズ + 位置調整）
  - 左右も上下もピンなし → 中央維持
```

**親フレーム**は Auto Layout レイヤー（Layout Component enabled）またはコンポジション。
親サイズ変更時に `constraintOffset` を再計算して子レイヤー位置＋サイズを更新。

**実装**:
- `ArtifactAbstractLayer` に `constraintLeft_` / `constraintRight_` / `constraintTop_` / `constraintBottom_` メンバ追加
- `component.layout.constraintLeft` 等のプロパティパスで JSON シリアライズ
- Auto Layout レイヤーの `applyConstraints()` を `parentAutoLayoutOffset()` の後段に追加
- Property Widget に制約エディタ（9パターンのトグルマトリクス UI）

**ファイル**:
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- `Artifact/include/Layer/ArtifactAbstractLayer.ixx`
- Property Widget（既存）

---

## Phase 2: フレームツール

**Figma の Frame** は「他のレイヤーを内包するコンテナ」。Design モードで Frame を描いて、その中に子レイヤーを配置する。Frame それ自体が Auto Layout や制約のコンテキストを持つ。

**現状との差分**:
- 現在は「任意のレイヤーに Layout Component をアタッチ」する方式。Figma は「Frame を作ってから中に入れる」。
- Frame ツールを追加して、ドラッグ操作で Frame レイヤーを作成できるようにする。

**仕様**:

```
ツール: ToolType::Frame 追加
操作: ビューポート上でドラッグ → Frame レイヤー作成。Rectangle ツールと似ているが:
  - 作成されたレイヤーは自動的に Layout Component (enabled)
  - Frame 背景色は透明（オプションで設定可）
  - 作成時に clipContent = true（子レイヤーをはみ出さない）

Frame へのレイヤー追加:
  - 既存レイヤーを Frame 上にドラッグ → 自動的に parentLayerId 設定
  - Design ワークスペースでレイヤーの親子関係が視覚的に表示
  - レイヤーパネルにインデント表示
```

**既存の親子レイヤー構造がすでにある**ため、Frame ツールの実装は「Rectangle 作成 → parentLayerId 設定」の UI ラッパーで済む。

**ファイル**:
- `Artifact/include/Tool/ArtifactToolManager.ixx` — `ToolType::Frame` 追加
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` — ツールハンドラ
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` — UI 登録

---

## Phase 3: シェイプブーリアン演算

**Figma の Boolean Groups** は複数シェイプの和/差/積/排他をリアルタイムに計算する。UI デザインの自由度が大幅に上がる。

**仕様**:

```
演算:
  BoolOp::Union        // 論理和。全シェイプの外形
  BoolOp::Subtract     // 差。最初のシェイプから後続を切り抜く
  BoolOp::Intersect    // 積。全シェイプの重なり部分のみ
  BoolOp::Exclude      // 排他的論理和。重なり部分を除いた外形

シェイプグループ:
  - 複数のシェイプレイヤーを選択 → ブーリアングループ化
  - グループ内の各シェイプは個別に編集可能
  - 演算結果をフラット化（ベイク）して通常シェイプレイヤーに変換可能
  - 結果の fill/stroke はグループ全体で設定
```

**実装方針**:
1. Clipper2 ライブラリを利用（Boost ライセンス、ヘッダオンリー、C++20 対応）。2D ポリゴンのブーリアン演算に特化し高速
2. `ArtifactShapeLayer` に `boolGroupId` と `boolOp` プロパティ追加
3. レンダリング時: グループ内の全シェイプをグローバル座標に変換 → Clipper2 で演算 → 結果パスを描画
4. GPU パスの直接ブーリアンは高コストなので CPU 側（Clipper2）で処理し、結果をポリゴンとして GPU に送る
5. 編集時は個別シェイプのパスを破線表示、演算結果を実線表示

**ファイル**:
- `third_party/Clipper2/`（新規）
- `Artifact/src/Layer/ArtifactShapeLayer.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

---

## Phase 4: ドロップシャドウ（レイヤー効果）

**Figma の Drop Shadow / Inner Shadow** はレイヤースタイルとして汎用的に適用できる。現在 Artifact にはトランジションエフェクト内にしかなく、独立したレイヤー効果ではない。

**仕様**:

```
Drop Shadow プロパティ:
  shadowEnabled  : bool
  shadowColor    : QColor
  shadowOpacity  : float (0-1)
  shadowOffsetX  : float (px)
  shadowOffsetY  : float (px)
  shadowBlur     : float (px)
  shadowSpread   : float (px)  // 影の拡大

Inner Shadow プロパティ:
  同上 + 内側に描画

レイヤーブラー（背景ぼかし）:
  backgroundBlurEnabled : bool
  backgroundBlurRadius  : float (px)
  Figma の Background Blur = レイヤーの背後にある全コンテンツにガウスぼかし適用
```

**実装方針**:
1. Drop Shadow は `shadowOffsetX/Y` だけオフセットした位置に、アルファ + `shadowBlur` のガウスぼかしを適用した黒/色付きシルエットを描画
2. GPU ブラーパス（既存のガウスぼかしシェーダー）を利用
3. レイヤー効果は「そのレイヤーの描画前後に追加描画」として実装。既存のレイヤー描画パイプラインに `drawLayerEffects()` を挿入
4. Inner Shadow はレイヤーアルファを反転マスクとして使用し、内側のみにぼかしを適用

**ファイル**:
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm` — レイヤー効果プロパティ追加
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` — 描画パス

---

## Phase 5: コンポーネント / インスタンス

**Figma の Components** は再利用可能な UI 要素。インスタンスはコンポーネントの参照コピーで、一部プロパティのみ上書き可能。

**DCC 的文脈**: After Effects のプリコンポーズ + Essential Properties が近い概念。Artifact ではこれを「コンポーネントレイヤー」として表現できる。

**仕様**:

```
Component（マスター）:
  - レイヤーまたはレイヤーグループを「Component 化」
  - Component は特殊なコンポジション（内部に子レイヤーを持つレイヤーグループ）
  - Component 編集モード: ダブルクリックで内部へ移動（Figma と同様）

Instance（インスタンス）:
  - Component を参照するプレースホルダーレイヤー
  - デフォルトでは Component の全プロパティを継承
  - オーバーライド可能なプロパティ:
    - テキスト内容
    - 色（fill/stroke）
    - 表示/非表示（レイヤー単位）
    - 画像ソース
  - オーバーライドプロパティは Instance 固有のキーフレームとして保存

変更の伝播:
  - Component の編集 → 全 Instance に自動反映（オーバーライドを除く）
  - Instance のオーバーライドリセット → Component の値に戻す
  - Component と Instance の diff 表示（Property Widget）
```

**実装**:
- Component は内部に子レイヤーを持つ特殊な `ArtifactComponentLayer`（既存の `ArtifactAbstractCompositionLayer` に近い）
- Instance は Component の ID を参照する軽量レイヤー
- オーバーライドは Instance の `transform3D()` + カスタムプロパティマップに保存
- レンダリング時: Instance → Component の子レイヤーを再帰的に描画、オーバーライドを適用
- Undo/Redo: Component 編集の undo が全 Instance に波及するため、GizmoTransformUndoCommand 相当のバルク undo が必要

**ファイル**:
- `Artifact/include/Layer/ArtifactComponentLayer.ixx`（新規）
- `Artifact/src/Layer/ArtifactComponentLayer.cppm`（新規）
- `Artifact/include/Layer/ArtifactComponentInstanceLayer.ixx`（新規）
- `Artifact/src/Layer/ArtifactComponentInstanceLayer.cppm`（新規）
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`

---

## 実装優先順位

| Phase | 内容 | コスト | Figma 機能カバー率向上 | 実装依存 |
|-------|------|--------|----------------------|---------|
| 1 | 制約システム | 中 | +12% | なし |
| 2 | フレームツール | 低 | +8% | Phase 1 が先だとより良い |
| 3 | ブーリアン演算 | 中 | +10% | Clipper2 導入 |
| 4 | ドロップシャドウ | 中 | +6% | なし |
| 5 | コンポーネント | 高 | +15% | なし |

Phase 1〜3 完了時点で Figma 機能カバー率 **約70%**。Phase 5 で **約85%**。

---

## 変更対象ファイル一覧

| ファイル | Phase |
|----------|-------|
| `Artifact/src/Layer/ArtifactAbstractLayer.cppm` | 1, 4 |
| `Artifact/include/Layer/ArtifactAbstractLayer.ixx` | 1 |
| `Artifact/include/Tool/ArtifactToolManager.ixx` | 2 |
| `Artifact/src/Tool/ArtifactToolManager.cppm` | 2 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 2, 3, 4 |
| `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | 2 |
| `Artifact/src/Layer/ArtifactShapeLayer.cppm` | 3 |
| `third_party/Clipper2/`（新規） | 3 |
| `Artifact/include/Layer/ArtifactComponentLayer.ixx`（新規） | 5 |
| `Artifact/src/Layer/ArtifactComponentLayer.cppm`（新規） | 5 |
| `Artifact/include/Layer/ArtifactComponentInstanceLayer.ixx`（新規） | 5 |
| `Artifact/src/Layer/ArtifactComponentInstanceLayer.cppm`（新規） | 5 |
| `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm` | 5 |
