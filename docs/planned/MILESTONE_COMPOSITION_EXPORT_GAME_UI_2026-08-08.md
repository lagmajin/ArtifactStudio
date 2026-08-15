# Composition Export to Game UI Middleware (2026-08-08)

**最終更新:** 2026-08-09
**最終監査:** 2026-08-15
**状態:** 初期実装済み。Writer／共通 session／pre-render の静的導線は成立、各ランタイム受入れと atlas 最適化は未完了。

## 概要

ArtifactStudio のコンポジション（レイヤー構造・テキスト・画像・シェイプ・アニメーション）を、ゲーム UI ミドルウェアが解釈可能なネイティブフォーマットにエクスポートする。デザイナーが ArtifactStudio で UI モックアップ・アニメーションを作り、ゲームエンジンに直接出力するワークフローを確立する。

## 現在の実装範囲

- 共通 `ArtifactExportSession`、Export Dialog、File Menu 経路を実装済み。
- Session 内で画像アセットを収集し、形式をまたいだ相対パスと同名衝突回避を行う。
- Lottie、RmlUi、Gameface、Unity UI Toolkit、NoesisGUI の初期 Writer を実装済み。
- Position / Rotation / Scale の基本キーフレームを各形式へ出力し、NoesisGUI は Storyboard を生成する。テキストのフォント、色、配置も各 UI 形式へ反映する。
- マスク、マット、エフェクト、3D、CPUラスター処理、複雑シェイプ、グラデーションが必要なレイヤーは、既存Diligentオフスクリーン経路を優先してフレーム列へベイクする。
- プリレンダー解像度は Export Dialog から 1x〜4x を選択でき、5形式の Writer から共通 PreRender 経路へ伝播する。
- 未完了: 画像列のアトラス最適化、実機ランタイム検証、複雑なエフェクト補間の品質確認。

## 2026-08-15 現行コード照合

`ArtifactExportSession`、共通 `ArtifactExportPreRenderPipeline`、File Menu／Export Dialog の導線と、Lottie／RmlUi／Gameface／Unity UI Toolkit／NoesisGUI の各 Writer を確認した。画像アセットの相対パス化、同名衝突回避、基本 transform／text／keyframe 出力、複雑レイヤーのベイク経路、1x〜4x の解像度指定は実装済みとして扱える。

ただし、各出力を実際のゲーム UI ランタイムで読み込む検証、形式ごとのアニメーション／フォント／座標系 parity、画像列の atlas／packing、複雑エフェクトの補間品質は未検証。Writer の存在だけで「5形式の本番互換完了」とは判定しない。

## 対象出力フォーマット

### 1. RmlUi（.rml + .rcss）

**採用ゲーム**: Beyond All Reason, 一部 Unreal/自社エンジン統合
**フォーマット**: HTML-like RML + CSS2 サブセット RCSS + オプション Lua スクリプト

**マッピング**:

| ArtifactStudio | RmlUi |
|---------------|-------|
| コンポジション | 単一 `.rml` ファイル |
| レイヤー階層 | `<div>` ネスト |
| レイヤー名 | `id="layer_name"` |
| Position | `left:` / `top:` プロパティ（または `transform: translate(x, y)`） |
| Rotation | `transform: rotate(deg)` |
| Scale | `transform: scale(sx, sy)` |
| 不透明度 | `opacity:` |
| テキストレイヤー | `<div>` + テキスト内容、`font-family`, `font-size`, `color`, `text-align` |
| 画像レイヤー | `<img src="...">` または `background-image: url(...)` |
| 矩形シェイプ | `<div>` + `background-color:` + `border-radius:` |
| 楕円シェイプ | `<div>` + `border-radius: 50%` |
| 角丸矩形 | `<div>` + `border-radius: Npx` |
| キーフレームアニメーション | `@keyframes` + `animation:` プロパティ（`.rcss`） |
| コンポジションサイズ | `<body>` の `width` / `height` |
| 背景色 | `<body>` の `background-color` |

**制約と事前処理**:
- ブレンドモード: RmlUi 非対応。事前に背景とフラット合成が必要
- マスク: 非対応。アルファマスクとして画像に焼き込み
- ベジェパス・複雑シェイプ: 非対応。ラスタ画像として事前レンダリング
- 複雑キーフレーム補間: リニア・イーズのみ対応。Bezier は分割近似（サンプルレート指定）
- レイヤーエフェクト: フィルタ非対応。事前レンダリング

**出力**:
```
output/
├── composition.rml         ← レイヤー構造 + インラインスタイル
├── composition.rcss        ← @keyframes + 共通スタイル
├── animation.lua           ← 複雑アニメーション（オプション）
└── assets/
    ├── layer_01.png        ← 事前レンダリング画像
    └── layer_02.png
```

---

### 2. NoesisGUI（.xaml）

**採用ゲーム**: Baldur's Gate 3, 各種 UE4/5 タイトル
**フォーマット**: WPF 互換 XAML（XML ベース）

**マッピング**:

| ArtifactStudio | NoesisGUI XAML |
|---------------|----------------|
| コンポジション | `<Grid>` ルート |
| レイヤー階層 | `<Canvas>` + 子要素 |
| Position | `Canvas.Left` / `Canvas.Top` |
| Rotation | `<Canvas.RenderTransform><RotateTransform Angle="..."/></Canvas.RenderTransform>` |
| Scale | `<ScaleTransform ScaleX="..." ScaleY="..."/>` |
| 不透明度 | `Opacity="..."` |
| テキスト | `<TextBlock>` + `FontFamily`, `FontSize`, `Foreground`, `TextAlignment` |
| 画像 | `<Image Source="...">` + `Width` / `Height` |
| 矩形 | `<Rectangle>` + `Fill`, `RadiusX` / `RadiusY` |
| 楕円 | `<Ellipse>` + `Fill` |
| アニメーション | `<Storyboard>` + `<DoubleAnimation>` / `<ColorAnimation>` |
| エフェクト | `<DropShadowEffect>` / `<BlurEffect>`（一部対応） |

**NoesisGUI の強み**:
- WPF の Storyboard アニメーションシステムが強力。イージング関数（Bezier, Elastic, Bounce 等）をネイティブサポート
- ドロップシャドウ・ぼかし等の基本的な GPU エフェクト対応
- データバインディング（`{Binding}`）とスタイル（`<Style>`）分離

**制約**:
- 独自エフェクト・ブレンドモードは非対応（事前レンダリング）
- マスク: `OpacityMask` は対応。ベジェマスクは `PathGeometry` で出力可能

**出力**:
```
output/
├── composition.xaml        ← UI 定義
├── composition_styles.xaml ← 共通スタイル
└── assets/
    └── *.png
```

---

### 3. Unity UI Toolkit（.uxml + .uss）

**採用エンジン**: Unity
**フォーマット**: UXML（XML）+ USS（CSS-like）+ `.meta`

**マッピング**:

| ArtifactStudio | Unity UI Toolkit |
|---------------|-----------------|
| コンポジション | `<ui:UXML>` ルート |
| レイヤー | `<ui:VisualElement>` |
| Position | `position: absolute; left: Xpx; top: Ypx;`（USS） |
| Rotation | `rotate: Xdeg;`（USS） |
| Scale | `scale: X Y;`（USS） / `transform-origin` |
| 不透明度 | `opacity:`（USS） |
| テキスト | `<ui:Label text="...">` + USS フォント設定 |
| 画像 | `<ui:Image>` + `background-image: url(...)`（USS） |
| 矩形 | `<ui:VisualElement>` + `background-color` + `border-radius` |
| アニメーション | USS `transition:` + `@keyframes`（UI Toolkit 1.0+）<br>または C# Animation / DOTween 出力 |

**Unity 固有**:
- `.meta` ファイル生成（アセット GUID）
- USS 変数（`--primary-color`）によるデザイントークン出力
- `flex` レイアウトへの変換（絶対位置 → Flexbox、オプション）

---

### 4. Coherent Gameface（HTML/CSS）

**採用ゲーム**: Star Citizen, 各種 AAA
**フォーマット**: 標準 HTML5 + CSS3 + JavaScript

**マッピング**:

| ArtifactStudio | Gameface |
|---------------|----------|
| コンポジション | `index.html` |
| レイヤー | `<div>` 階層 |
| アニメーション | CSS Animations / Web Animations API / JavaScript |
| インタラクション | JavaScript イベントハンドラ |
| レスポンシブ | CSS Grid / Flexbox に変換（オプション） |

**Gameface の強み**:
- フル CSS3 対応（フィルタ、ブレンドモード、マスクの一部をサポート）
- JavaScript で完全なインタラクティブロジック出力が可能
- Web 標準のため、ブラウザでのデバッグ・プレビューが容易

**出力**:
```
output/
├── index.html
├── styles.css
├── animations.css
├── app.js
└── assets/
    └── *.png
```

---

### 5. Lottie（JSON）

**採用先**: iOS, Android, Web, React Native, Flutter, Qt
**フォーマット**: JSON（Bodymovin/Lottie スキーマ）

**マッピング**:

| ArtifactStudio | Lottie JSON |
|---------------|-------------|
| コンポジション | ルートオブジェクト `{"v":"5.5.2", ...}` |
| フレーム | `ip` / `op`（in/out point） |
| フレームレート | `fr` |
| サイズ | `w` / `h` |
| レイヤー | `layers[]` 配列 |
| Position | `"ks": {"p": {"a": 1, "k": [{"t": 0, "s": [x,y]}, ...]}}` |
| Rotation | `"r": {"a": 1, "k": [...]}` |
| Scale | `"s": {"a": 1, "k": [...]}`（パーセント表記） |
| 不透明度 | `"o": {"k": 50}` |
| テキスト | `"t": {"d": {"k": [{"s": {"t": "Hello"}}]}}`（テキストレイヤー） |
| シェイプ | `"shapes[]"`（パス・塗り・線） |
| 画像 | `"refId": "image_0"` + `assets[]` |
| マスク | `"masksProperties": [...]` |
| アニメーション | キーフレーム配列（全プロパティ共通のタイムライン構造） |
| ブレンドモード | `"bm": 0-15`（AE互換の数値） |
| マット/トラックマット | `"tt": 1/2` |
| エフェクト | 一部対応（ドロップシャドウ等）。非対応はベイク |
| イージング | イン/アウトベジェ（cp1/cp2 制御点） |

**Lottie の強み**:
- AE のほぼ全プロパティをマッピング可能（After Effects ↔ Lottie の互換性が高い）
- ベジェシェイプ、マスク、マット、ブレンドモードをネイティブサポート
- 出力が単一 JSON ファイル。アセットは base64 埋め込みまたは外部参照
- iOS/Android/Web/Flutter/Qt すべてにレンダラーが存在

**制約**:
- ラスタ画像レイヤーは base64 埋め込みで JSON が肥大化しがち
- 一部 AE エフェクトは非互換（高度なディスプレイスメント、パーティクル等）

**出力**:
```
output/
└── composition.json        ← 単一ファイル。画像は埋め込み or assets/ 参照
```

---

## 共通実装アーキテクチャ

全出力フォーマットで共通化できる部分を分離:

```
ArtifactExportSession
├── resolveLayerTree()           ← 親子関係・可視性解決
├── resolveTransforms()          ← グローバル座標にフラット化（またはローカル維持）
├── resolveKeyframes()           ← キーフレーム補間・サンプリング
├── preRenderLayer()             ← エフェクト焼き込み・ラスタ化
├── collectAssets()              ← 画像・フォントのコピー/参照解決
└── formatWriter (strategy)
    ├── ArtifactExportRmlUiWriter       → .rml + .rcss
    ├── ArtifactExportNoesisXamlWriter   → .xaml
    ├── ArtifactExportUnityUxmlWriter    → .uxml + .uss
    ├── ArtifactExportGamefaceWriter     → .html + .css + .js
    └── ArtifactExportLottieWriter       → .json
```

**事前レンダリングが必要なケース**（全フォーマット共通）:
- エフェクト（ガウスぼかし、色補正、ディスプレイスメント）
- 複雑ブレンドモード（オーバーレイ、ソフトライト等）
- ベジェパスの複雑な塗り（フォーマットが対応しない場合）
- 3D レイヤーの 2D 投影

**事前レンダリング方針**:
- GPU ブレンドパイプライン（既存）でオフスクリーンレンダリング
- 出力解像度はコンポジションの 1x〜4x（設定可能）
- レイヤー単位またはレイヤーグループ単位（ブレンド依存）

---

## Phase 一覧

| Phase | 内容 | コスト | 効果 |
|-------|------|--------|------|
| E-1 | 共通エクスポート基盤（layerTree, transform, keyframes, assets） | 中 | 全フォーマットの土台 |
| E-2 | Lottie JSON 出力 | 中 | AE互換性最高。全プラットフォーム対応 |
| E-3 | RmlUi 出力 | 低 | HTML-like で最も単純 |
| E-4 | Gameface HTML/CSS 出力 | 低 | RmlUi 出力から派生可能 |
| E-5 | Unity UI Toolkit 出力 | 中 | .uxml + .uss 構造が独自 |
| E-6 | NoesisGUI XAML 出力 | 中 | Storyboard アニメーション変換が必要 |
| E-7 | 事前レンダリングパイプライン | 中 | 全フォーマット共通。GPU readback 利用 |

**推奨順序**: E-2（Lottie）を最初にやるのが最も実用的。After Effects のエコシステムと直接競合でき、出力フォーマットとして最も完成度が高い。次に E-1（共通基盤）→ E-3/4（HTML系）→ E-5/6（XAML系）。

---

## Lottie 優先の理由

1. **マッピング完全性**: AE ↔ Lottie がすでに存在するため、ArtifactStudio もほぼ同じマッピングを踏襲できる。シェイプ・マスク・マット・ブレンドモード・キーフレーム補間すべてに対応
2. **単一ファイル出力**: JSON 1ファイル。アセット管理不要
3. **ユビキタス**: iOS/Android/Web/Flutter/Qt/React Native すべてにレンダラーがある
4. **AE からの移行訴求**: 「AE を使わずに Lottie アニメーションを作れる」は強いセールスポイント
5. **実装の手本がある**: AE の Bodymovin エクスポーターがリファレンス実装として存在

## 変更対象ファイル一覧（全 Phase）

| ファイル | Phase |
|----------|-------|
| `Artifact/src/Export/ArtifactExportSession.cppm`（新規） | E-1 |
| `Artifact/include/Export/ArtifactExportSession.ixx`（新規） | E-1 |
| `Artifact/src/Export/ArtifactExportLottieWriter.cppm`（新規） | E-2 |
| `Artifact/src/Export/ArtifactExportRmlUiWriter.cppm`（新規） | E-3 |
| `Artifact/src/Export/ArtifactExportGamefaceWriter.cppm`（新規） | E-4 |
| `Artifact/src/Export/ArtifactExportUnityUxmlWriter.cppm`（新規） | E-5 |
| `Artifact/src/Export/ArtifactExportNoesisXamlWriter.cppm`（新規） | E-6 |
| `Artifact/src/Export/ArtifactExportPreRenderPipeline.cppm`（新規） | E-7 |
| `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm` | E-1（メニュー項目追加） |
| `Artifact/src/Export/ArtifactExportDialog.cppm`（新規） | E-1 |
