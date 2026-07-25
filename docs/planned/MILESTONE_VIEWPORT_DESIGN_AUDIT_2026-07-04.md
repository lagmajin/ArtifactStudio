# マイルストーン: ビューポート（Composition Editor）デザイン監査 (2026-07-04)

> 作成: 2026-07-04 / 更新: 2026-07-04 (コード実在確認 + C4D/Maya/Houdini 比較追記)
> 元依頼: 「ビューポートでできることが少ない不満、足りないものは？」

## 監査サマリー

`ArtifactCompositionEditor.cppm`（4,500+行）は Diligent バックエンドで稼働。Shell/RenderController/RenderWidget/Gizmo/Overlay/PieMenu/ViewOrientationNavigator/Multi-Pane の責務を抱える。

**初回監査からの訂正**: コード再調査の結果、以下は実装済みまたはファイル存在を確認した。

| 機能 | 訂正後ステータス | 実体 |
|---|---|---|
| Motion Path | ✅ 実装済み | `M-UI-6` 完了。path+dot+frame rect overlay, Ctrl+Alt+M toggle, Undo 完備 |
| Audio Scrubbing | ✅ 実装済み | `ArtifactAudioScrubController.cppm` 存在。Editor が import |
| Safe Margins | ✅ 実装済み | `showSafeMargins_` / `setShowSafeMargins()` 動作 |
| Smart Guides | ✅ 実装済み | `ArtifactSmartGuidesManager.ixx` 完備 |
| Grid System | ✅ 実装済み | `ArtifactGridSystem.ixx` あり |
| Puppet Tool | ⚠️ ファイルあり | `ArtifactPuppetTool.cppm` + `.ixx` 存在。ToolType 登録済。実装深度未確認 |
| Motion Sketch | ⚠️ ファイルあり | `ArtifactMotionSketchTool.cppm` + `.ixx` 存在。実装深度未確認 |
| Track Matte データモデル | ⚠️ 部分的 | `LayerMatteReference` が `ArtifactAbstractLayer.cppm` に存在。Drag UX のみ不在 |

---

## 🔴 P0（最優先）: コード不在の欠落機能

### チャンネル分離表示（RGBA個別プレビュー）
- `Tool::ViewMode { Color, Mask, Wireframe }` は enum にあるが、**R/G/B/A の個別チャンネルをビューポート上で直接表示するモードがない**。
- AE の `Alt+2/3/4` で RGB/Alpha/RGB+Alpha を即切替できるのが基準。

### X-Ray / 透過表示
- 選択レイヤーの背後にあるものを透かして見る X-Ray モードがない。
- 3D レイヤーや重なったシェイプを編集するときに不可欠。

### Proportional Editing（プロポーショナル編集）
- 選択キーフレームや頂点を動かしたときに、周囲も滑らかに追従させる減衰編集がない。
- Puppet Tool / Motion Sketch の実装深度にも直結。

---

## 🟡 P1（高優先）: ファイルはあるが機能不完全

### Puppet Tool の実装深度不足
- `.cppm` + `.ixx` は存在し ToolManager に登録済みだが、AE の Puppet Pin 相当（メッシュ変形、ピン配置、ワープ操作）が実際に動くかは未検証。
- コード内検索では `ArtifactPuppetTool` のクラス定義は見つかるが、**メッシュ変形ソルバやピン→頂点ウェイトマッピングのロジック**が未確認。

### Motion Sketch の実装深度不足
- ファイルは存在するが、「マウス軌跡→キーフレーム一括生成」が実際に動作するか未検証。
- `FEATURE_AUDIT` の 45 hit は大部分が data model 名でロジック不在と報告されている。

### Track Matte Drag UX 不在
- データモデル `LayerMatteReference` は `ArtifactAbstractLayer.cppm:712` に存在。
- しかし **ビューポート上でドラッグして matte を割り当てる UI**（AE の pick whip 相当）は未実装。

### Viewport Render（ビューポートスナップショット出力）
- スクリーンショットボタン（`screenshotButton_`, `quickScreenshotAction_`, `advancedScreenshotAction_`）は存在。
- しかし **レンダリングパイプラインを通した高品質ビューポート出力**はない。

---

## 🔵 P2（中優先）: 未実装のまま

### Roving Keyframes
- 複数キーフレームの相対時間間隔を保ったままドラッグ移動する機能。コード 0 hit。

### Source Text Keyframe
- テキストレイヤーのソーステキスト自体にキーフレームを打てない。

### Echo/Afterimage エフェクト
- エフェクトクラス自体が存在しない。

### Wipe Compare（Composition Viewport 側）
- Contents Viewer にはあるが、Composition Viewport 上での A/B ワイプ比較がない。

### ROI (Region of Interest) 非表示
- `ArtifactRenderROI` 構造体は存在するが、ビューポート上の debug draw はコメントアウト。
- UI から ROI 設定する導線が未実装。プレビュー範囲の視覚的指定ができない。

---

## 🟡 P1（高優先）: ビューポート操作品質

### 3D Orbit / Pan が直感的でない
- 回転時に視点が飛ぶ。ツール切替を挟む必要があり煩雑。
- `MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md` 計画済みだが未着手。
- Blender 風 `Alt+Left Drag = Orbit`, `Middle Drag = Pan`, `Wheel = Zoom` の統一文法がない。

### Preview Orbit Mode 不在
- カメラレイヤーを直接編集しているのか、視点だけ動かしているのか区別できない。
- 実カメラと preview-only view state の分離がない。

### Multi-Viewport が骨格のみ
- `PaneState` / `ViewportLayoutMode::Single/TwoUp/FourUp` / viewport splitter はコードに存在。
- しかし `ViewportLayoutManager` (M-VP-1) は計画段階で未実装。
- 複数ビューポートのレンダリング直列化（IDeviceContext 競合）も未解決。

### Motion Blur のビューポート UX 不在
- `MotionBlurEffect` エフェクトは存在。シャッター角・サンプル数の設定はできる。
- しかしビューポートやタイムライン上で視覚的に確認・調整する UI がない。

### Auto-Orient 不在
- パスに沿ったレイヤーの自動回転補正が未実装。
- `MOTION_PATH_EDITING_WORKLOG_2026-04-29.md` に関連記述はあるが未完。

---

## 🆚 C4D / Maya / Houdini 比較（Blender 重複除外）

### Cinema 4D 由来の不足

| C4D 機能 | Artifact 状況 |
|---|---|
| **Interactive Render Region (IRR)** | ❌ なし。ビューポート上の矩形領域だけプログレッシブレンダリングする仕組み。重い comp の部分確認に必須 |
| **Display Tags** | ❌ なし。オブジェクトごとに「ワイヤーフレーム表示」「シェード表示」を viewport 上で上書き指定するタグ。レイヤー単位の表示切替に使える |
| **Viewport Filter（タイプ別表示切替）** | ⚠️ 部分的。レイヤータイプによるフィルタはあるが、Generator/Deformer/Light/Camera 等のカテゴリ別一括切替がない |
| **Protection Tag** | ❌ なし。特定レイヤーをビューポート選択/変形からロックする |
| **Vertex/Weight Map 可視化** | ❌ なし。ウェイトペイントをビューポート上にヒートマップ表示できない |
| **Commander（全コマンド検索）** | ❌ なし。Ctrl+F で全コマンドを検索・実行するパレット |
| **Sticky Keys** | ❌ なし。ショートカットキーを押している間だけツールを一時切替、離すと元に戻る |

### Maya 由来の不足

| Maya 機能 | Artifact 状況 |
|---|---|
| **Isolation Mode** | ❌ なし。選択オブジェクトだけを表示し、残りを自動非表示。レイヤー数が多い comp で特定レイヤーの編集に集中したいときに必須 |
| **Hotbox（Space 全メニュー）** | ❌ なし。Space キーで全メニューをマウス位置にポップアップ。Artifact の PieMenu はあるが、全メニュー網羅ではない |
| **HUD カスタマイズ** | ⚠️ 部分的。`stateLabel` で再生状態等は出るが、poly count / fps / camera name / scene name 等の要素をユーザーが選択できない |
| **Camera Bookmarks** | ❌ なし。カメラ位置を名前付きで保存・呼出し。頻繁に使う視点への即ジャンプ |
| **Playblast** | ❌ なし。ビューポートの動画プレビューを即座に書き出す。クライアントレビュー用の簡易出力として必須 |
| **Panel Toolbar カスタマイズ** | ❌ なし。ビューポートごとにツールバーをカスタマイズ不可 |

### Houdini 由来の不足

| Houdini 機能 | Artifact 状況 |
|---|---|
| **Construction Plane** | ❌ なし。任意の位置/角度に作業平面を設定し、その平面上で変形・スナップ。Artifact の Construction Layer は注釈用でこれとは別 |
| **Guide Geometry** | ❌ なし。レンダリングされない一時的なガイドジオメトリ（変形の目安、パスのプレビュー等） |
| **Viewport Visualizers** | ❌ なし。アトリビュート値を色/サイズ/ベクトルとしてビューポート上に可視化。レイヤーのパラメータ分布を視覚的にデバッグ |
| **Geometry Spreadsheet 連携** | ❌ なし。ビューポート選択 ↔ 属性テーブルの相互連動。ピクセル/頂点単位のデータ確認 |
| **Network Editor ↔ Viewport 連携** | ⚠️ 部分的。`ArtifactCompositionGraphWidget` はあるが、ノードクリック→ビューポート反映、ビューポート選択→ノードハイライトの双方向連携が弱い |
| **Render Region** | ❌ なし。C4D の IRR と類似。矩形範囲のみ再レンダリング |
| **Handle System（統一ハンドル）** | ⚠️ 部分的。TransformGizmo + Gizmo3D はあるが、全コンテキストで統一された単一ハンドルではない |

---

## 🆕 マイナーツール・特殊機能からの追加不足

上記の主要 DCC 比較に加え、より専門的/ニッチなツールから拾った不足機能。

### Resolve / Fusion / Nuke 系（合成・カラグレ視点）

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Split Screen / Before-After Wipe** | ❌ | ビューポート内で垂直/水平ラインをドラッグして処理前後を比較。Contents Viewer の Compare はあるが Composition Viewport 内にない |
| **Highlight / Clipping Zebra** | ❌ | クリップしたハイライト/シャドウをゼブラパターンで可視化。露出確認に不可欠 |
| **Gallery Still Store** | ❌ | 現在のフレームを基準静止画として保存し、後で呼び出して比較 |
| **On-Screen Color Controls** | ❌ | ビューポート上でカラーバランスのリングをドラッグして直接調整 |
| **Reference Wipe** | ❌ | 外部リファレンス画像/動画をビューポート上でワイプ比較 |
| **Contact Sheet** | ❌ | 全レイヤーをグリッド状に一覧表示。レイヤー数が多い comp の俯瞰に必須 |
| **Flipbook** | ❌ | プレビューキャッシュを連番画像で即時書き出し。クライアントチェック用 |

### アニメーション特化

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Onion Skinning / Ghosting** | ❌ | 前後 N フレームを半透明オーバーレイ表示。MotionBuilder/Blender ではアニメーションの基本。AE の `Frame Blending` はあるが onion skin とは別 |
| **Dope Sheet Viewport Overlay** | ❌ | タイムラインのキーフレーム位置をビューポート端にマーク表示。アニメーション編集中にタイムラインへ視線を動かす必要が減る |
| **Motion Trail** | ⚠️ | Motion Path は実装済みだが、残像のような「にじみ軌跡」表示はない。Maya の Motion Trail に相当 |

### 3ds Max / Unreal 系

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Viewport Background Image** | ❌ | リファレンス画像をビューポートの背景に表示。モデリング/レイアウトの必須機能 |
| **Viewport Clipping Planes** | ❌ | 近距離/遠距離のクリッピング面を手動設定。大きなシーンでの作業効率 |
| **Show Flags（要素別表示切替）** | ❌ | Fog/Particles/PostProcess/Light/etc の表示をチェックボックスで個別 ON/OFF。UE の Show Flags パネル相当 |
| **Immersive Mode** | ✅ | `immersiveMode_` + `immersiveAction_` あり。F11 のフルビューポートは実装済み |

### Substance / マテリアル系

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **PBR チャンネル分離表示** | ❌ | BaseColor/Roughness/Metallic/Normal/Height を単独チャンネルで表示 |
| **Environment Map Rotation** | ❌ | 環境マップをビューポート上で回転。マテリアルレビュー用 |
| **Material Preview Primitive** | ❌ | 球/立方体/円柱/トーラスでのマテリアルプレビュー切替 |

### その他特殊ツール

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Viewport Canvas Rotation** | 📋 M-VP-4 計画中 | ビューポートキャンバス自体の回転。タブレットで描くときに紙を回す感覚 |
| **Viewport Dynamic Resolution** | 📋 M-VP-5 計画中 | 再生中のみ解像度を自動低下させてフレームレートを維持 |
| **Viewport Bookmarks** | 📋 M-VP-8 計画中 | カメラ位置の保存/復元。Ctrl+1-9 で即ジャンプ |
| **Time Ruler Overlay** | ❌ | ビューポート上にタイムルーラー（時間軸）を表示。AE のコンポジションタイムルーラー相当 |
| **Layer Controls Toggle** | ❌ | AE の `View → Show Layer Controls`。レイヤーのハンドル/パス/マスクを一括表示切替 |

### AE マイナー機能（モーショングラフィックス視点）

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Brainstorm** | ❌ | 選択パラメータをランダムに振ってバリエーションのグリッドを生成。AE の隠れた神機能 |
| **The Wiggler** | ❌ | キーフレームにランダムな振動（周波数/振幅指定）を自動生成。テキストのランダム点滅などに多用 |
| **The Smoother** | ❌ | 選択キーフレームを許容誤差ベースで自動平滑化。手書きパスのクリーンアップ |
| **Exponential Scale** | ❌ | 2D レイヤーの拡大縮小を指数的に補間（線形拡大は奥行きが不自然になる問題を解決） |
| **Sequence Layers** | ❌ | 複数レイヤーを自動で時間方向に並べて配置。スライドショー系 comp の即作成 |
| **Preserve Underlying Transparency** | ❌ | 下のレイヤーのアルファを継承。複雑なマスク合成を1クリックで実現 |
| **Quality Switch per Layer** | ❌ | レイヤー単位の Draft/Best 切替。重いレイヤーだけ一時的に低品質表示 |
| **Guide Layer** | ❌ | エディタ上でのみ表示されレンダリングされない参照用レイヤー |
| **Align Panel in Viewport** | ❌ | 選択レイヤーをビューポート上で整列/分布。AE の Align パネル相当 |
| **Convert Expression to Keyframes** | ❌ | エクスプレッションをベイクしてキーフレーム化。配布前にエクスプレッションを焼く定番ワークフロー |

### Nuke 由来の特殊機能

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Sample Points** | ❌ | ビューポート上に永続的なサンプル点を配置。各点の RGBA 値が常時表示される。カラーマッチングの必須機能 |
| **Dope Sheet in Viewer** | ❌ | ビューポート下部にミニ Dope Sheet を表示。キーフレーム位置をタイムラインを見ずに確認 |
| **Layer Cop** | ❌ | マウスジェスチャーでレイヤー間をスワイプ比較。ワイプより直感的 |
| **Buffer Visualization** | ❌ | GPU バッファ（BaseColor/WorldNormal/Depth等）をビューポート上に直接表示。デバッグに必須 |
| **Pre-render Region** | ❌ | 指定範囲だけ先行レンダリングしてキャッシュ。C4D IRR の Nuke 版 |

### 3ds Max / Modo 由来の特殊機能

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Clone and Align to Surface** | ❌ | オブジェクトを複製し、別オブジェクトの法線に沿って自動配置。3ds Max の隠れた便利ツール |
| **Action Centers** | ❌ | ピボット位置を Origin/SelectionCenter/Local/Element で即切替。Modo の核となる操作哲学 |
| **Work Plane Auto-Align** | ❌ | クリックした面に作業平面を自動で平行配置。モデリング効率が大幅に変わる |
| **Isolate Selection with State Restore** | ❌ | 分離→作業→解除で直前の表示状態に完全復帰。単純なトグルより賢い |
| **SteeringWheels** | ❌ | マウス位置に追従する放射状ナビゲーションホイール。ペンタブレットと相性が良い |

### Blender 由来のニッチ表示機能

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **MatCap** | ❌ | マテリアルキャプチャ画像で簡易質感表示。テクスチャ未設定のアセットレビューに最適 |
| **Cavity Shading** | ❌ | エッジ/溝を強調するビューポートシェーディング。形状の読み取りやすさが格段に上がる |
| **Face Sets** | ❌ | メッシュの部位を色分け表示。スカルプト/ペイント時の領域管理 |
| **Polyframe** | ❌ | ポリゴンエッジを色付きワイヤーフレームで表示。ZBrush の同機能。トポロジーチェックに |
| **Header Flip** | ❌ | ツールバーをビューポート上部⇔下部に移動。画面レイアウトの自由度 |

### Resolve / カラグレ由来の特殊機能

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Power Window Invert/Outside toggle** | ❌ | マスク領域の内外反転をビューポート上のボタン一発で切替 |
| **Node Key** | ❌ | ノードに対してキーボードショートカットを割り当て、視覚効果の A/B テストを即実行 |
| **Offline Reference Clip** | ❌ | 外部リファレンス動画をビューポート上に半透明オーバーレイ。ルック合わせに必須 |
| **Gallery Wipe with Still** | ❌ | 保存した基準静止画と現在のフレームをビューポート上でワイプ比較 |
| **Highlight Zebra Adjustable Threshold** | ❌ | クリップ警告の閾値を IRE 値で調整可能。SDR/HDR 両対応 |

### 超マニアック・他ツール由来

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Info DAT Overlay** (TouchDesigner) | ❌ | ホバーした要素の全プロパティをポップアップテーブル表示。デバッグの強力な武器 |
| **Visualizer-based Debugging** (Houdini) | ❌ | アトリビュート値を頂点カラーとしてビューポートに直接描画。データフローの可視化 |
| **Falloff Visualization** (Apple Motion) | ❌ | ビヘイビア/エフェクトの減衰範囲を色付きリングでビューポート表示 |
| **Tracker Confidence Color** (Syntheyes) | ❌ | トラッカーのソルブ信頼度を緑→黄→赤で色分け表示。一見でトラッキング品質がわかる |
| **State Machine Preview** (Rive) | ❌ | アニメーションの状態遷移をビューポート上で直接テスト。インタラクティブモーションに必須 |
| **Duplicate Array Preview** (Cavalry) | ❌ | Duplicator の複製結果をビューポート上にリアルタイムプレビュー。パラメータ調整の即時フィードバック |
| **Pixel Preview** (Figma) | ❌ | 1x/2x のピクセル相当に拡大し、実際のピクセル境界をグリッド表示。UI/アイコン制作に便利 |
| **Roto Node Tree in Viewport** (Silhouette) | ❌ | 複雑なロトの階層をビューポートサイドにツリー表示。ロト管理の革命 |
| **Surface Plane Tool** (Mocha) | ❌ | 遠近のある面上に平面を定義し、その面上で正確なトラッキング/変形 |
| **Shading Network Inspector** (Katana) | ❌ | ピクセルをプローブすると、そのピクセルの全シェーディング寄与をノードグラフで表示 |

### Modo 由来（開発終了だがコンセプトは強力）

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Falloff Ring Preview** | ❌ | ツールの影響範囲を色付き同心円でビューポート表示。半径をドラッグで直接調整。Modo の代名詞的機能。「今どの範囲に効いているか」を常に視覚化 |
| **Tool Pipe** | ❌ | 1つのツールに複数のフォールオフ/変形をスタック。「選択をまず円形フォールオフで絞り、次にノイズで揺らし、最後にリニア減衰」のような加工パイプラインをビューポートで構築 |
| **Snap Preview** | ❌ | スナップ先をドラッグ前に半透明でプレビュー表示。「この位置にスナップする」という結果が見えてから操作できる |
| **Subdivision Cage Overlay** | ❌ | サブディビジョン適用後のスムーズ形状と同時に元のケージ（制御メッシュ）をワイヤーフレームで重畳表示。モデリングのフィードバックに必須 |
| **Edge Weight Visualization** | ❌ | エッジのサブディビジョン重みを色分け表示（赤=強い/青=弱い）。クレースの強度が一目でわかる |
| **Topology Overlay** | ❌ | ポリゴンフローを色付きの流れ線で表示。エッジループの流れを視覚的に追跡 |
| **Selection Set Viewport Toggle** | ❌ | 保存した選択セットをビューポート上のリストからワンクリックで復元＋ハイライト |
| **Morph Map Preview** | ❌ | モーフターゲットの変形をビューポート上のスライダーで即時プレビュー。100% 適用した姿を見ながら別のモーフを調整できる |
| **Replicator Preview** | ❌ | パーティクル/複製の結果をビューポート上にリアルタイムプレビュー。C4D の Cloner + Fields の可視化に相当 |
| **UV Distortion Heatmap in 3D Viewport** | ❌ | 3D ビューポート上に UV の歪みをヒートマップ表示。展開の良し悪しを別ウィンドウなしで確認 |
| **Item List ↔ Viewport 双方向ハイライト** | ❌ | アイテムリストで選択→ビューポートハイライト、ビューポートクリック→アイテムリスト自動スクロール＆ハイライト |

### Mari 由来（3D テクスチャペイント視点）

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Paint Buffer Channel Isolation** | ❌ | ペイントするチャンネル（BaseColor/Roughness/Metallic/Normal/Height/etc）をビューポート上で単独表示。特定チャンネルのみの確認に必須 |
| **Projector Frustum Visualization** | ❌ | プロジェクションペイントの投影錐台をビューポート上に表示。テクスチャがどこから投影されているか直感的に把握 |
| **Mask Stack Overlay** | ❌ | ペイントマスクの重なりをビューポート上に色分け表示。複数マスクの相互作用を視覚化 |
| **UDIM Tile Navigator** | ❌ | UDIM タイルをクリックして即座にその UV 領域にジャンプ。100 枚以上の UDIM を扱うプロダクションで必須 |
| **Texture Resolution Heatmap** | ❌ | テクスチャ密度をビューポート上にヒートマップ表示。どの UDIM が何 px/cm か一目瞭然 |
| **Paint Stroke History Scrub** | ❌ | ペイントストロークの履歴をタイムライン的にスクラブ。何をいつ描いたか遡れる |
| **Displacement Preview** | ❌ | ディスプレイスメントマップをビューポート上で実際のジオメトリ変形としてプレビュー。法線マップとの見え方の違いを即確認 |
| **Layer Opacity per Paint Layer** | ❌ | ペイントレイヤー単位の不透明度をビューポート上でスライダー調整。フォトショップのレイヤーリスト感覚 |
| **Bake Point Preview** | ❌ | ベイク処理の前に結果をプレビュー。「この設定でベイクするとこうなる」を先に見せてから実行 |
| **Clone Stamp Source Preview** | ❌ | クローンスタンプのソース位置を半透明で表示。フォトショップのクローンスタンププレビューと同等 |
| **HDR Environment Rotation in Viewport** | ❌ | 環境マップをビューポート上でマウスドラッグで回転。マテリアルの映り込み確認に非常によく使う |
| **Tiled EXR Seam Preview** | ❌ | UDIM 間のシームをビューポート上に表示。タイル境界のテクスチャの繋がりをチェック |
| **Color Managed Viewport with OCIO** | ❌ | OCIO カラーマネジメントを適用した状態でビューポート表示。ACES パイプラインでの最終色確認 |

### 🏥 医療画像ビューア由来（3D Slicer / OsiriX / Horos）

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Window/Level Presets** | ❌ | ボーン/肺/腹部のような露出プリセットをワンクリック切替。Artifact でも「ハイライト確認用」「シャドウ確認用」「通常」の輝度プリセットが有用 |
| **Crosshair Synchronized Multi-View** | ❌ | 複数ビューポート間でクロスヘア位置を完全同期。1つのビューポートで動かすと残りも追従。Multi-Viewport の上位互換 |
| **Slice Scroll with Mouse Wheel** | ⚠️ | フレーム送りはあるが、レイヤースタックの Z 方向スクロール（レイヤー間をマウスホイールで潜る）はない |
| **Hounsfield-style Calibrated Probe** | ❌ | ピクセル値を較正された単位（物理値）で永続表示するプローブ。Artifact の hover probe は hex/RGBA のみ。露出値や Nuke 式の log2 値も出せると強力 |
| **Fiducial Markers** | ❌ | ビューポート上に名前付きの基準点を配置。2点間の距離自動計算。レイアウトや位置合わせの参照点として使う |
| **Registration Checkerboard Blend** | ❌ | 2つの画像を市松模様で交互表示。CTとPETの重ね合わせなど。Artifact の Compare にチェッカーボードブレンドが加われば格段に強力 |
| **Segmentation Overlay** | ❌ | 半透明の色付き領域でセグメンテーション結果を重畳表示。Artifact ではマスク/マットの領域を色分け表示できると直感的 |
| **Cine Mode** | ❌ | スライス/フレームの自動連続スクロール再生。タイムライン再生とは別に、Z方向（レイヤー方向）の高速プレビューに便利 |
| **MIP/MinIP Toggle** | ❌ | Maximum/Minimum Intensity Projection。スタック内の最大/最小輝度を投影表示。パーティクルやボリュームの簡易可視化に |
| **Oblique/Curved Reformatting** | ❌ | 任意の断面でデータを切り直す。パスに沿って「まっすぐにする」変形。テキストをパスに沿って変形させる応用も考えられる |
| **ROI Statistics Overlay** | ❌ | 矩形/自由領域内の min/max/mean/stddev をリアルタイム表示。エフェクト前後の数値比較に必須 |

### 🗺️ GIS マップビューア由来（QGIS / ArcGIS）

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Locator Bar (Ctrl+K)** | ❌ | レイヤー名・ツール名・コマンドをインクリメンタル検索して即ジャンプ。C4D Commander より軽量で常時表示可能 |
| **Magnification Tool（倍率独立ズーム）** | ❌ | 表示倍率を変えずに拡大鏡のように一部だけ拡大。1px 単位の微調整時に解像度を変えずに作業できる。QGIS の隠れた名機能 |
| **Scale Lock + Independent Zoom** | ❌ | スケールを固定したままズームイン/アウト。コンポジションの論理解像度を保ったまま細部を拡大したいときに |
| **Overview Panel（ミニマップ）** | ❌ | 現在のビューポート範囲を全体コンテキストの中で示す小さなインセットマップ。広い comp 作業での現在地確認に |
| **Scale-Dependent Rendering** | ❌ | ズームレベルによって自動的に表示品質/詳細度を切替。拡大時のみ高解像度プレビュー、縮小時は軽量表示 |
| **Map Tips（ホバー属性ポップアップ）** | ⚠️ | ホバーでレイヤーのメタデータをポップアップ。hover preview はあるが、プロパティ値の表形式ポップアップはない |
| **Identify Tool（クリック全属性表示）** | ❌ | クリックした位置の全レイヤー情報をパネル表示。Artifact の Inspector をクリック位置に連動させられると強力 |
| **Coordinate Display with Unit Switch** | ⚠️ | カーソル位置をキャンバス座標＋ピクセル座標＋正規化座標で同時表示。現在は hover probe の XY のみ |
| **Render Suspend Checkbox** | ❌ | ビューポートのレンダリングを一時停止するチェックボックス。重い comp で UI だけ操作したいときに |
| **Graticule / Grid with Coordinate Labels** | ⚠️ | Grid System はあるが、座標値をグリッド線に沿ってラベル表示する機能はない |
| **Time Slider for Temporal Data** | ⚠️ | タイムラインはあるが、ビューポート内に埋め込まれた簡易タイムスライダー（QGIS の Temporal Controller）はない |
| **Annotation Layer with Callout Lines** | ⚠️ | Construction Layer で line/circle は描けるが、吹き出し線＋テキストのアノテーションはない |

### 🏗️ 建築 CAD 由来（Revit / AutoCAD / ArchiCAD）

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Section Box（3D クリッピングボックス）** | ❌ | 6面のクリッピング面を持つドラッグ可能な箱。面を掴んでスライドし、建物の断面を自在に切り出す。Artifact では Z 深度クリッピングや特定レイヤー範囲の断面表示に応用可能 |
| **Phase Filter（フェーズフィルタ）** | ❌ | 「新規」「既存」「解体」の状態を色分け表示＋フィルタ。Artifact では「今回追加分」「前回からの変更分」の差分ビューとして使える |
| **Temporary Hide/Isolate with Glass** | ❌ | 非表示にした要素を完全に消すのではなく半透明の「ガラス」状態で残す。構造を見失わずに作業対象に集中できる |
| **View Template（ビューポート設定の保存/適用）** | ❌ | 現在のズーム・パン・表示モード・オーバーレイ・フィルタ設定を名前付きテンプレートとして保存し、他の comp やチームメンバーと共有 |
| **Underlay（下敷き参照）** | ❌ | 別の図面/comp を薄く表示してトレースや位置合わせの下敷きに。外部リファレンス comp の減衰表示 |
| **Crop Region + Scope Box** | ❌ | ビューポートの表示範囲を矩形または名前付き領域で制限。ROI の永続版 |
| **Sun Path / Shadow Study** | ❌ | 光源の位置と影の動きを1日のタイムラインでプレビュー。Artifact の 3D ライトアニメーションのプレビューに応用可能 |
| **Wall Joins / Cleanup Visualization** | ❌ | 要素同士の接合部を強調表示。レイヤー境界でのブレンド/マスクのつなぎ目チェックに応用 |
| **Dimension Constraints** | ❌ | 要素間の距離制約を色付き線と数値でビューポート表示。レイヤー間の相対距離を常に把握 |

### 🎚️ DAW / オーディオエディタ由来（Ableton / Pro Tools / Logic）

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Spectral View（スペクトログラム表示）** | ❌ | 時間×周波数×強度の3軸を色の濃淡で表示。音声レイヤーのビューポートプレビューを波形からスペクトラムに切替可能にすると音の編集精度が格段に上がる |
| **Clip Gain Envelope** | ❌ | クリップ上の音量をドラッグ可能なポイントで直接調整。Artifact ではレイヤー opacity/volume をビューポート上で直接ドラッグ編集 |
| **Comping（テイク切り貼り）** | ❌ | 複数テイクを縦に並べ、良い部分をスワイプで選択して1つのマスターテイクに合成。複数バージョンの comp から部分的に採用するワークフローに |
| **Warp Markers（伸縮マーカー）** | ❌ | オーディオ波形上にマーカーを置き、ドラッグで時間伸縮。Artifact では time remap キーフレームのビューポート直操作に相当 |
| **Track Freeze Indicator** | ❌ | 凍結（プリレンダリング）されたトラックに雪の結晶アイコンを表示。Artifact のキャッシュ状態表示の直感的な代替案 |
| **Automation Lanes Overlay** | ❌ | パラメータのオートメーションカーブをビューポート上に半透明で重畳表示。キーフレームをタイムラインを見ずに編集 |
| **Transient Detection Markers** | ❌ | 波形のアタック点を自動検出して縦線でマーク。ビデオのカット検出やシーンチェンジ検出に応用可能 |

### 🎬 動画編集由来（Premiere / Final Cut / Avid）

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Trim Mode with Dual Preview** | ❌ | 2つのクリップの継ぎ目を前後のフレームを並べて微調整。Artifact のレイヤー境界トリミングをビューポートで直接 |
| **Slip / Slide Tool Visualization** | ❌ | クリップの長さを変えずに内容をスライド。Artifact の time remap / layer trim をビューポート上でマウスドラッグで操作 |
| **Multicam Angle Selector** | ❌ | 複数カメラアングルをグリッド表示し、クリックで即切替。Multi-Viewport の応用で複数 comp バージョンの同時プレビュー＋切替 |
| **Speed Ramp Visual Curve** | ❌ | クリップ上に速度変化のグラフを直接表示＆編集。Time remap の視覚的編集に |
| **Proxy Workflow Icon** | ❌ | プロキシ使用中であることを示す小さなアイコンをビューポートに表示。現在のプレビュー品質状態を常に把握 |
| **In/Out Point Handles on Clip** | ❌ | クリップの開始/終了点をドラッグ可能なハンドルとしてビューポート端に表示。ワークエリアの視覚的編集 |

### 📸 フォト編集由来（Photoshop / Lightroom / Capture One）

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Before/After Split Views（縦・横・斜め）** | ❌ | 処理前後を縦分割・横分割・斜め分割で比較。Contents Viewer の Compare の拡張 |
| **Black/White Clipping Preview（Alt キー長押し）** | ❌ | レベル補正中に Alt を押すとクリップした黒/白だけを表示。エフェクト調整中のクリップ警告として強力 |
| **Focus Peaking** | ❌ | ピントの合っているエッジを色付きで強調表示。シャープネス/ブラー/DoF エフェクトの調整に |
| **Soft Proofing Preview** | ❌ | 出力先のカラースペースで見たときの色の変化をシミュレーション。プレビュー LUT の応用 |
| **History Snapshot Preview on Hover** | ❌ | ヒストリーパネルの各ステップにホバーするとその時点の画像をポップアップ表示。Undo 履歴の視覚的プレビュー |
| **Select and Mask Edge Refinement** | ❌ | マスクのエッジを拡大してブラシで微調整する専用モード。Artifact の Mask 編集の精度向上に |

### 💻 コードエディタ由来（VS Code / JetBrains）

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Minimap（全体俯瞰サムネイル）** | ❌ | ファイル全体を極小サムネイルとして右端に表示。広い comp の「今どこを見ているか」を常に把握 |
| **Breadcrumb Navigation** | ⚠️ | 階層パスをクリック可能なパンくずリストで表示。Artifact にもあるが depth が浅い |
| **Peek Definition（インライン参照先表示）** | ❌ | 参照先を別ウィンドウで開かずにインラインポップアップで表示。pre-comp の中身をポップアッププレビュー |
| **Sticky Scroll（現在のスコープ固定表示）** | ❌ | スクロールしても現在のスコープ（関数/クラス名）がビューポート上部に固定表示。現在の comp 名やレイヤー名を常時表示 |
| **Zen Mode（全 UI 非表示）** | ✅ | Immersive Mode として実装済み |
| **Bracket Pair Colorization** | ❌ | 対応する括弧を同じ色で表示。入れ子の pre-comp やグループを色分け |
| **Diff View（サイドバイサイド差分）** | ❌ | 2つのバージョンを差分ハイライト付きで左右に並べて表示。Comp のバージョン比較に |

### 🌐 ブラウザ DevTools 由来（Chrome / Firefox DevTools）

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Element Picker（クリックで要素選択）** | ❌ | ビューポート上の任意の位置をクリックすると、その位置にある全レイヤーのスタックが表示され、選択できる。複雑な comp でのレイヤー選択が格段に容易に |
| **Box Model Visualization** | ❌ | 選択要素の margin/border/padding/content を色分けした入れ子の矩形で表示。レイヤーの bounds/マスク範囲/エフェクト範囲/マット範囲を視覚化 |
| **Flexbox/Grid Overlay** | ❌ | レイアウトグリッドラインをビューポート上に表示。レイヤーの整列分布の確認に |
| **Animation Timeline Panel** | ❌ | CSS アニメーションのタイムラインをビューポート下部に表示。簡易キーフレーム編集 |
| **Layers Panel（3D ページレイヤー俯瞰）** | ❌ | 全レイヤーを Z 方向にずらした3D 俯瞰表示。Comp 構造の直感的理解に |
| **Color Picker with Contrast Ratio** | ❌ | 色選択時にWCAGコントラスト比を表示。テキストレイヤーの可読性チェックに |

### 🎮 ゲームエンジン由来（Unity / Unreal / Godot）

| 機能 | Artifact 状況 | 価値 |
|---|---|---|
| **Draw Mode Dropdown** | ⚠️ | Unity の Shaded/Wireframe/Shaded+Wireframe/Overdraw/Mipmap 切替。Artifact は Draft/Full の2値。複数のプリセットビューモードが欲しい |
| **Gizmo Handle Pivot/Center Toggle** | ❌ | ギズモの中心をオブジェクト原点にするか、選択範囲の中心にするか。複数レイヤー選択時の変形基点切替に必須 |
| **Gizmo Handle Local/Global Toggle** | ❌ | ギズモの向きをレイヤーのローカル座標にするか、comp のワールド座標にするか。3D レイヤーの回転編集で頻繁に切替 |
| **Vertex Snapping（V キー）** | ❌ | V を押しながらドラッグで頂点スナップ。レイヤー端を別のレイヤー端にピッタリ合わせられる |
| **Surface Snapping（Ctrl+Shift）** | ❌ | ドラッグ中に Ctrl+Shift で面に吸着。3D 空間でのレイヤー配置に |
| **Frame Selected（F キー）** | ✅ | Zoom Fit として実装済み |
| **Lock View to Selected（Shift+F）** | ❌ | 選択レイヤーにビューをロック。レイヤーがアニメーションで移動しても常に中央に追従。トラッキングデータの確認に強力 |
| **Camera Preview Thumbnail** | ❌ | シーンに配置したカメラレイヤーの視点を小さなインセットウィンドウでプレビュー。メイン comp を見ながらカメラの画角を調整 |
| **Align View to Selected（Ctrl+Shift+F）** | ❌ | 選択レイヤーの向きにビューを一致させる。3D レイヤーのローカル軸に沿って作業したいときに |
| **Flythrough Mode（WASD）** | ❌ | 右クリック＋WASD でゲームのようにシーン内を飛行。3D シーンの直感的ナビゲーション |
| **Play in Editor Tint** | ❌ | 再生中はビューポート枠が色付き（Unity はオレンジ、Unreal は赤）になり、編集不可であることを示す。再生/スクラブ中の視覚的区別 |
| **Simulate Mode** | ❌ | 物理演算のみ再生し、キャラクター制御はしない。Artifact ではエフェクトのみプレビュー再生するモードに相当 |
| **Overdraw Mode** | ❌ | 透明オブジェクトの重なり回数を色の濃さで表示。Comp 内のアルファ重なりによるレンダリングコストを可視化 |
| **Mipmap Debug View** | ❌ | どのミップレベルが使われているかを色分け表示。テクスチャ解像度の適正確認 |
| **Lightmap Density Heatmap** | ❌ | テクセル密度をヒートマップ表示。Artifact ではテクスチャの解像度分布確認に |
| **Shadow Cascades Visualization** | ❌ | シャドウマップのカスケード分割を色分け表示。3D レイヤーのシャドウ品質デバッグに |
| **Collider / Physics Shape Visualization** | ❌ | 物理コリジョン形状を半透明の緑色ワイヤーフレームで常時表示。物理シミュレーションのデバッグに必須 |
| **NavMesh Visualization** | ❌ | AI パスファインディングの歩行可能領域を青色で表示。パーティクルやエージェントの移動範囲確認に |
| **Audio Source Range Sphere** | ❌ | 音源の減衰範囲を同心球でビューポート表示。3D オーディオの到達範囲確認 |
| **Reflection Probe Bounds** | ❌ | 反射プローブの影響範囲を黄色い枠で表示。間接光/反射のカバレッジ確認 |
| **Live Editing（編集中＋実行中同時）** | ❌ | Godot の Live Editing。再生中にパラメータを変更すると即反映＋停止後も編集が保持される。Comp の再生中にエフェクトを調整したいときに革命的 |
| **Remote Scene Tree** | ❌ | Godot の Remote 機能。再生中のシーン構造をツリー表示し、ノードをクリックで選択＋プロパティ確認。実行時デバッグに |
| **Sub-Viewport Rendering** | ❌ | 別のビューポートをテクスチャとして任意の面にレンダリング。pre-comp の中身を comp 内の面に映し出す |
| **Half-Resolution Preview** | ⚠️ | Godot の 1/2 解像度プレビュー切替。Artifact の interactive downsampling はあるが手動切替不可 |
| **Debug Draw API** | ❌ | スクリプトからビューポートに線/矢印/球/テキストを一時描画。カスタムツールやデバッグ表示の開発に |

---

## ⚙️ パフォーマンス問題（ビューポート体験に直結）

| 問題 | 深刻度 | 詳細 |
|---|---|---|
| **N+1 per-layer レンダリング** | 🔴 高 | 1レイヤーごとに setOverrideRTV→clear→draw→flush→unbind→convertToFloat→blend→swap。30レイヤーで 31 回の barrier/dispatch |
| **dynamic_cast 連鎖 54K/sec** | 🟡 中 | `CompositionViewDrawing.cppm` で 30+ casts/layer/frame |
| **setCanvasSize/Zoom/Pan の重複設定** | 🟡 中 | フレーム内で同じ値を N+1 回設定 |
| **操作中ダウンサンプル 1/4** | 🟡 中 | `interactivePreviewDownsampleFloor_ = 4`。操作中は解像度が1/4に |
| **GPU プロファイリング未接続** | 🟢 低 | `beginFrameGpuProfiling()` 未呼出し。GPU時間計測不能 |

参照: `docs/reports/PERF_COMPOSITION_EDITOR_DILIGENT_INVESTIGATION_2026-06-16.md`

---

## ⚙️ コード設計上の問題

| 問題 | 詳細 |
|---|---|
| **Shell 4,500行 + Controller 7,700行** | 2ファイルで合計12,000行超。単一責任の原則違反 |
| **Qt 依存が深い** | `QPainter`, `QPixmap`, `QImage`, `QSvgRenderer` が global module fragment に残存 |
| **WA_PaintOnScreen ハイブリッド** | CompositionViewport が Diligent Native + Qt Paint のハイブリッド。移植性懸念 |
| **Gizmo が 2D/3D で分離** | `TransformGizmo` (2D) + `Gizmo3D` (3D) で責務分散。統合されていない |

---

## 改善の推奨優先順位

| 順位 | 領域 | 内容 |
|---|---|---|
| 1 | **チャンネル分離表示** | RGBA/RGB/Alpha 個別チャンネルビューポート（AE Alt+2/3/4 相当） |
| 2 | **3D Orbit/Pan 統一** | Alt+Drag=Orbit, Middle=Pan, Wheel=Zoom の統一文法 |
| 3 | **Isolation Mode** | 選択レイヤー以外を自動非表示（Maya/C4D 方式） |
| 4 | **Puppet Tool 実装** | メッシュ変形＋ピン操作。キャラクターアニメの要 |
| 5 | **X-Ray / 透過表示** | 遮蔽物越しに選択・編集可能にする |
| 6 | **Interactive Render Region** | C4D IRR 相当。矩形範囲のプログレッシブレンダリング |
| 7 | **Camera Bookmarks** | カメラ位置の名前付き保存・呼出し |
| 8 | **Viewport Visualizers** | Houdini 式。アトリビュート値の色/サイズ/ベクトル可視化 |

---

## 関連文書

- `docs/COMPOSITION_EDITOR_CONTRACT.md` — Editor/Controller/Renderer 責務境界
- `docs/reports/PERF_COMPOSITION_EDITOR_DILIGENT_INVESTIGATION_2026-06-16.md` — パフォーマンス分析
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` — モーションデザイン機能監査
- `docs/planned/MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md` — 3D 視点操作の統一計画
- `docs/planned/IMPLEMENTATION_PLAN_MULTI_VIEWPORT_2026-06-27.md` — マルチビューポート計画 (M-VP-1)
- `docs/planned/MILESTONE_MULTI_VIEWPORT_LAYOUT_2026-06-01.md` — マルチビューポートレイアウト
- `docs/planned/MILESTONE_LIVE_FIELD_AUTHORING_UX_2026-07-04.md` — Live Field Authoring UX (M-LC-3)
- `docs/worklog/MOTION_PATH_EDITING_WORKLOG_2026-04-29.md` — Motion Path 編集作業記録
- `docs/planned/FEATURE_EFFECT_LEVEL_MASK_2026-07-02.md` — エフェクトレベルマスク
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` — Editor Shell (~4,500行)
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` — Render Controller (~7,700行)
- `docs/WIDGET_MAP.md` — ウィジェット責務マップ

---

## 備考

- QImage/QPixmap の新規採用禁止。既存コードからの段階的撤去を推奨。
- `QPainter` / Qt `CompositionMode` を使った新規描画・合成実装は禁止。
- Diligent バックエンドは AI にとって読み違えやすいシビアなコード扱い。推測で広く触らず変更範囲を最小化。
- モーションデザイナー視点の P0: Roving Keyframes / Motion Sketch / Auto-Orient / Source Text / Track Matte Drag / Audio Scrubbing（FEATURE_AUDIT より）。

---

## 2026-07-25 現状確認

初回監査の一部記述は現行ソースと差分がある。3D Orbit / Pan / Wheel、Preview Orbit Mode、Camera Bookmarks、Motion Path、Audio Scrubbing、3D selection wireframe、3D compositing の基礎は関連マイルストーンとソースで実装を確認済みであり、「不在」とは判定しない。

現時点でも、チャンネル分離の拡張、X-Ray、Interactive Render Region、Display Tags、Isolation、Construction Plane、Viewport Visualizers、Composition Viewport 内 Wipe、Roving Keyframes、Auto-Orient、Track Matte Drag などは完全な実装根拠が不足している。Puppet / Motion Sketch はファイルと導線の存在だけでは完了とせず、runtime を未確認として扱う。本監査は「基礎viewport機能は大幅に実装済み、DCC比較で列挙された高度機能は未完了または未検証」と更新する。

確認範囲: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`、関連する M-VP / 3D milestone 文書。ビルド・実機操作による動作確認は未実施。
