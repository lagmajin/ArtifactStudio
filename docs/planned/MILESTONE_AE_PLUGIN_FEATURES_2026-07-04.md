# AE プラグイン機能の Artifact Studio への取り込み (2026-07-04)

**最終更新:** 2026-08-15

> 無料・有料 AE プラグインの独自機能を Artifact でネイティブ実装する案。

> 2026-07-08 更新: `FX Console` 相当の `Ctrl+Space` クイックエフェクト起動をインスペクタ側に実装済み。`Motion 4` 相当の 9点アンカーポイント操作も既存ツールとして取り込み済み。`EaseCopy` 相当のキーフレーム easing コピペと `Elastic / Bounce / Back` のプリセット、`Animation Composer` 相当の内蔵プリセットライブラリも追加済み。

## 実装状況

### 2026-08-15 現行コード照合

- AE 風の小規模 workflow（Effect Picker、Ease Copy／Paste、easing／preset library、anchor point 操作）は既存 UI として実装済み。
- Core には `PluginRegistry`、共通 Plugin ABI、`ILayerPlugin`、VST3 loader、CLAP host の基盤がある。
- ただし現行コード上、AE ネイティブプラグインの互換ホスト、外部エフェクトを通常の Effect surface へ安全にロードする完成経路、CLAP の実運用・UI統合は確認できない。
- Duik／Overlord／Saber 等の大規模互換機能は、ネイティブ機能の個別実装候補であり、プラグイン互換としては未実装。署名、sandbox、ABI／version compatibility、runtime plugin QA も未完了。

**判定:** AE 風 workflow の一部とプラグイン基盤は実装済み。外部 AE plugin compatibility を目標にする場合は未達。

- `Ctrl+Space` で Inspector の effect picker を開けるようになった
- 既存の `+ Add Effect` ボタンと同じ導線を共有している
- `Motion 4` 系のアンカーポイントは既存ツールとして確認済み
- `EaseCopy` 系のコピペは timeline の `Ease Copy` / `Ease Paste` で追加済み

### 2026-07-25 実装監査

上記のワークフロー機能は既存の Inspector／Timeline 導線として実装済みと整理できる。一方、表の「実装済み」は AE プラグインをロードして機能互換することを意味しない。Core には汎用 `PluginRegistry` と VST3 のロード基盤があるが、CLAP はインスタンス生成が未実装の骨格であり、AE ネイティブプラグインホストや Overlord／Duik／Saber 等の互換機能は確認できない。したがって本マイルストーンは「小規模な AE 風ワークフロー機能は実装済み、大規模機能と外部プラグイン互換は未実装」とする。runtime のショートカット、既存導線、プリセット適用は未確認。

## カテゴリ別

### ワークフロー高速化

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **FX Console** | 無料 | Ctrl+Space でエフェクト名インクリメンタル検索→即適用。スクリーンショット取得 | Artifact では EffectPickerDialog に `Ctrl+Space` クイック起動を追加済み |
| **Motion 4** | $49 | アンカーポイント一発設定(9点)、Easing プリセット(Elastic/Bounce/Back)、キーフレームクローン、シーケンスレイヤー | EasingLab は既存。アンカーポイントツールは既存導線あり、Easing プリセットは実装済み、Keyframe Clone は新規 |
| **EaseCopy** | 無料 | キーフレーム間でイージングをコピペ。選択キーフレームに一括適用 | 小規模。Timeline に Ease Copy / Ease Paste を追加済み |
| **Flow** | $30 | カスタムイージングカーブエディタ。プリセット保存/共有 | CurveEditor 拡張で対応可 |
| **Overlord** | $45 | Illustrator→AE シェイプレイヤー転送。パス/色/グラデーション維持 | Artifact が SVG/AI インポートで同等機能をネイティブ化 |
| **Track to New Null** | $5 | トラッキングデータをNullレイヤーにベイク。ワンクリック | トラッキングシステムの出力先としてNull自動作成 |

### AI 支援

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **Klutz GPT** | 無料 | AE 内で ChatGPT と会話。スクリプト/エクスプレッション生成 | AICloudWidget で既に対応。Slash Commands で強化済み |
| **MATE for AE** | $20 | 自然言語でレイヤー整理、テキスト置換、アニメーション追加、レンダーキュー管理 | AICloudWidget + ToolBridge で対応可。DCC 特化コマンド追加 |

### アニメーション

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **Duik** | 無料 | キャラクターリギング、IK、歩行サイクル自動生成、ボーン/コントローラー | **大規模新機能。** IK ソルバー＋ボーンシステム |


## マイナー／ニッチプラグイン・スクリプト

### レイヤー操作／整理

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **True Comp Duplicator** | 無料 | コンポを依存関係ごと完全複製。pre-comp 内部まで複製し直す | プロジェクト複製の深いコピー |
| **Explode Shape Layers** | 無料 | シェイプレイヤーのグループ/パスを個別レイヤーに分解 | ShapeLayer 展開ツール |
| **Dojo Shifter** | $35 | 複数レイヤーの in/out 点を段階的にずらす。シーケンス作成 | レイヤーシーケンサー |
| **AutoCrop** | 無料 | プリコンポを内容物のサイズに合わせて自動トリミング | Comp 自動リサイズ |
| **ft-Toolbar** | 無料 | カスタマイズ可能なボタンバー。よく使う操作をワンクリック化 | Quick Action バー |

### キーフレーム／イージング

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **Keysmith** | $39 | キーフレーム曲線の高度な編集。カーブの形を他キーフレームにコピー | KeyframeCurve テンプレート |
| **Ease and Wizz** | 無料 | 多彩なイージング式をエクスプレッションで適用。Quint/Expo/Elastic/Bounce/Back の全バリエーション | EasingLab プリセット追加 |
| **Rift** | 無料 | キーフレームにランダムなオフセット/ディスプレイスメントを一括適用 | Keyframe Jitter ツール |

### カラー／スタイル

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **Ray Dynamic Color** | $49 | カラーパレットをプロジェクト全体で一元管理。変更すると全レイヤーに反映 | グローバルカラーパレットシステム |
| **Ray Dynamic Texture** | $39 | テクスチャを全レイヤーに一括適用/管理 | グローバルテクスチャ |
| **ButtCapper** | 無料 | ストロークの端点（バット/ラウンド/プロジェクティング）をワンクリック切替 | StrokeCap クイック切替 |
| **Labels** | 無料 | レイヤーラベル色を一括変更/ルール設定 | レイヤー色分け自動ルール |

### テキスト／タイポ

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **TextExploder** | $25 | テキストを1文字ずつ個別レイヤーに分解。位置維持 | Text Per-Character 分解 |
| **TextEvo** | $35 | テキストアニメーションライブラリ。手書き/グリッチ/タイプライター/スライド等 | TextAnimator プリセット拡張 |
| **TypeMonkey** | $30 | キーフレーム自動生成テキストアニメーション。揺らぎ/ランダム | プロシージャルテキストキーフレーム |

### レンダリング／出力

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **Anubis** (BattleAxe) | $25 | 超シンプルなレンダー管理。プリセット出力、履歴、バックグラウンド | RenderQueue 簡易モード |
| **BG Renderer** | 無料 | AE を閉じずにバックグラウンドでレンダリング | Background Render |
| **CompSetter** | 無料 | コンポ設定（解像度/FPS/デュレーション）を選択コンポに一括適用 | Comp 一括設定変更 |
| **rd: Comp Setter** | 無料 | 同上＋コンポ名/コメントの一括編集 | Comp メタデータ一括編集 |

### パーティクル／物理

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **Newton** | $249 | 2D 物理シミュレーション。重力/摩擦/バウンス/ジョイント | 2D Physics Engine |
| **Physics Now!** | 無料 | 簡易2D物理。重力とバウンスのみ | 簡易物理シミュレーション |
| **Plexus** | $199 | 3D パーティクル＋ライン接続。点群データ可視化 | パーティクル＋ワイヤーフレーム |
| **Stardust** | $249 | ノードベースパーティクル。3D モデルパーティクル化 | Node-based Particle |

### 音声／波形

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **St6.Audio-Splitter** | 無料 | Demucs AI で音声をステム分離（ボーカル/ドラム/ベース/他） | Audio Stem Splitter |
| **Beat assistant** | 無料 | 音声波形からBPM検出、ビートマーカー自動生成 | Audio BPM 検出＋マーカー |
| **Universal Audio** | $25 | 複数レイヤー間で音声をミックス/クロスフェード | マルチトラックオーディオエディタ |

### GPU シェーダー／特殊

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **Tweak Shader** | 無料 | GLSL/ShaderToy シェーダーをレイヤーに直接適用 | GLSL シェーダーエフェクト |
| **motionXpart** | 無料 | 自動キャプション生成、Easingプリセット、レイヤー自動作成、プロジェクト整理、グラデーションロック | オールインワン統合ツール |
| **QuickLayersMINI** | 無料 | ドッカブルパネル: レイヤー作成/エフェクト/エクスプレッション/プロジェクトツール | クイックアクションパネル |

### AI 生成

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **Seedance** | 無料 | AI 動画生成をタイムラインからワンクリック。Seedance 2.0 / Z.AI / FAL 対応 | AI 動画生成プロバイダ統合 |
| **AutoCaptions** | 無料 | Whisper で自動文字起こし→キーフレーム付きテキストレイヤー生成 | AI 文字起こし＋字幕レイヤー生成 |
| **Demucs / Audio-Splitter** | 無料 | ローカル GPU で AI 音声分離（Demucs） | AI 音声分離 |

### BattleAxe 独自

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **Overlord** | $75 | Illustrator↔AE の双方向転送。シェイプ/カラー/グラデーションを維持 | SVG/AI インポートの強化 |
| **Rubberhose** | $65 | ストレッチ/ベンド可能なボーンリギング。IK 不要の直感的リグ | ストレッチボーン |
| **Anubis** | $25 | 超シンプルレンダリング管理 | 上記 |
| **無料スクリプト** | 無料 | ストロークキャップクイック切替 / BPMタップ→キーフレーム / エコー効果改善 / ソリッド代替 | 小規模ツール群 |
| **Animation Composer** | 無料 | プリセットアニメーションライブラリ。ワンクリック適用。テキスト/シェイプ/トランジション | Timeline のプリセットライブラリでワンクリック適用済み |
| **AutoCrank** | $35 | 機械系アニメーション自動生成（ピストン/ギア/クランク） | プロシージャルアニメーションツール |

### エフェクト・VFX

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **Saber** | 無料 | エネルギービーム/グロー/ライトセーバー。マスクパスに沿って発光。コア/グロー分離制御 | Glow エフェクト拡張でネイティブ化。PathBlurFX 相当 |
| **Orb** | 無料 | 3D 球体エフェクト。フォトリアルな金属/ガラス質感 | プロシージャルマテリアル＋球体プリミティブ |
| **Rift** | 無料 | ディスプレイスメント/シフトエフェクト | Artifact の DisplacementMap 拡張 |
| **Twitch** | $30 | グリッチ/ちらつき/スキャッタ等のランダム操作 | プロシージャルノイズエフェクト |
| **AlphaForge** | $49 | マット生成器。200プリセット。キーフレーム不要でトランジション | プロシージャルマットエンジン |

### 3D / レンダリング

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **Element 3D** | $199 | AE 内 3D レンダリング。OBJ/C4D インポート。PBR マテリアル | DiligentEngine で対応可。3D アセットパイプライン拡張 |
| **Foldspace** | $150 | GPU 加速 3D 曲面生成・編集 | Geometry Nodes 相当 |
| **BlenderAe2** | $49 | Blender↔AE 3D データ双方向転送 | Artifact↔Blender 連携プラグイン |

### テキスト / タイトル

| プラグイン | 価格 | 機能 | Artifact 実装案 |
|---|---|---|---|
| **TextEvo** | $35 | テキストアニメーションライブラリ。手書き風、タイプライター、グリッチ等 | TextAnimator 拡張 |
| **TypeMonkey** | $30 | キーフレーム自動生成テキストアニメーション。揺らぎ/ランダム | プロシージャルテキストトラッキング |

---

## 優先実装案（小〜中工数）

| 順位 | 機能 | 元プラグイン | 工数 | 既存基盤 |
|---|---|---|---|---|
| 1 | **Ctrl+Space クイックエフェクト起動** | FX Console | 小 | EffectPickerDialog 既存・実装済み |
| 2 | **アンカーポイント 9点スナップツール** | Motion 4 | 小 | Transform / Quick Toolbox 既存・取り込み済み |
| 3 | **Keyframe Easing コピペ** | EaseCopy | 小 | Timeline 既存・実装済み |
| 4 | **Elastic/Bounce/Back イージング** | Motion 4 | 小 | EasingLab 既存・実装済み |
| 5 | **プリセットアニメーションライブラリ** | Animation Composer | 中 | Timeline / Animation Menu 既存・実装済み |
| 6 | **AI によるレイヤー整理・一括操作** | MATE | 中 | AICloudWidget+ToolBridge 既存 |
| 7 | **Saber 相当のパス沿いグロー** | Saber | 中 | Glow+Mask 既存 |

## 大規模（長期）

| 機能 | 元プラグイン | 工数 |
|---|---|---|
| IK ボーン / キャラクターリギング | Duik | 大 |
| 3D ジオメトリ編集 | Element 3D / Foldspace | 大 |
| プロシージャルマット生成 | AlphaForge | 大 |

---

## 実行順

このマイルストーンは、実装済み項目と未着手項目の境界を分けたうえで、次の順で進めると迷いにくい。

1. `Keyframe Clone` の有無を確認し、`Motion 4` の残件を確定する
2. `Flow` 相当のカスタムイージング体験を timeline と一体で詰める
3. `Overlord` 相当の SVG / AI インポート強化を、シェイプ転送と分けて整理する
4. `Ray Dynamic Color` / `Ray Dynamic Texture` のようなプロジェクト全体資産管理を、色とテクスチャで切り分ける
5. `Duik` 系の大規模機能は、ボーン / IK の土台が揃ってから別枠で扱う

### 直近の着手候補

- `Keyframe Clone`
- `Flow` 風のカスタムカーブ再利用
- `Overlord` 風のインポート強化

### 完了条件の見方

- 既存実装済みの `Ctrl+Space`、`Ease Copy/Paste`、`Animation Composer` と重複しない
- 実装案が「便利そう」ではなく、既存の編集導線に乗る
- 大規模機能を先に増やさず、小〜中工数の制作体験改善から詰める
