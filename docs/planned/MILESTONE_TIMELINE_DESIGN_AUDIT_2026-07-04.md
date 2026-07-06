# マイルストーン: タイムラインエディタ 機能監査 (2026-07-04)

> 作成: 2026-07-04
> 元依頼: 「タイムラインエディタの機能提案をいろいろのアプリから調べて」

## 監査サマリー

Artifact のタイムラインは `ArtifactTimelineWidget` を中核に、左ペイン（`ArtifactLayerPanelWidget`）＋右ペイン（`ArtifactTimelineTrackPainterView`）＋Navigator＋ScrubBar＋WorkAreaControl＋DopeSheet で構成。owner-draw 移行済みで基盤は堅い。

以下、AE / Premiere / Resolve / Blender / Maya / C4D / Houdini / Nuke / DAW（Ableton/Logic/Pro Tools）/ Cavalry / Rive / Unity / Unreal / Apple Motion / Final Cut / Avid の **16 アプリ群**から、Artifact のタイムラインに不足している機能を収集。**📋 は既存マイルストーンで計画済み。**

---

## 🔴 P0: レイヤー操作・スイッチ列

AE のレイヤースイッチ列を基準に。Artifact のレイヤーパネルは左ペインでカバーしているが、以下の「即切替」要素が不足。

### AE レイヤースイッチ相当

| スイッチ | 概要 | 状態 |
|---|---|---|
| **Shy（控えめレイヤー）** | タイムラインから非表示にしつつ comp には表示。大量レイヤー整理に必須 | ❌ |
| **Solo（単独表示）** | 選択レイヤーのみ comp に表示。複数 Solo 可 | ❌ |
| **Lock（ロック）** | 選択/変形/キーフレーム編集を防止 | ❌ |
| **Video（表示切替）** | レイヤー単位の comp 表示 ON/OFF | ⚠️ 既存 |
| **Audio（音声切替）** | レイヤーの音声出力 ON/OFF | ❌ |
| **Adjustment Layer** | 下位レイヤー全体にエフェクト適用 | ✅ 実装済み |
| **3D Layer** | 2D↔3D 切替 | ⚠️ |
| **Collapse Transformations** | pre-comp の変形を継承 | ❌ |
| **Quality（Draft/Best）** | レイヤー単位のプレビュー品質切替 | ❌ |
| **FX（エフェクト一時無効）** | レイヤーの全エフェクト一括 ON/OFF | ❌ |
| **Frame Blend** | フレーム補間 ON/OFF | ❌ |
| **Motion Blur** | モーションブラー ON/OFF | ❌ |

### カラム表示

| 機能 | 概要 | 状態 |
|---|---|---|
| **Label Color 列** | レイヤーに色ラベルを割り当て、タイムライン上で色分け表示 | ❌ |
| **Comment 列** | レイヤーに自由記述コメントを付与。タイムライン上に表示 | ❌ |
| **In/Out/Duration/Stretch 数値列** | レイヤーの開始/終了/長さ/伸縮率を数値列として表示＆直接編集 | ⚠️ |
| **Parent 列** | 親レイヤー名を表示。ドラッグで親変更（pick whip） | ⚠️ Pick Whip 未実装 |
| **Track Matte 列** | マット参照先レイヤーとモード（Alpha/Luma/Invert）を表示 | ❌ |
| **レイヤー番号（#）列** | レイヤーに自動採番されたインデックス。レイヤー位置の絶対参照 | ❌ |
| **カラム表示/非表示のカスタマイズ** | 必要な列だけ表示する選択機能 | ❌ |

---

## 🔴 P0: キーフレーム編集・ナビゲーション

### キーフレーム操作

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Roving Keyframes** | AE | ❌ 複数キーフレームの相対間隔を保ったままドラッグ移動 |
| **J/K キーフレームジャンプ** | AE/Premiere | ❌ J=前のキーフレーム、K=次のキーフレーム。全アプリ共通の文法 |
| **キーフレーム時間方向スケール** | AE/Blender | ❌ 選択キーフレーム群を時間方向に均等伸縮（Alt+ドラッグ） |
| **キーフレーム値スケール** | AE | ❌ 選択キーフレームの値を均等スケール |
| **キーフレーム反転（時間/値）** | AE/Blender | ❌ 時間反転 + 値反転 |
| **キーフレームスナップ** | Blender | ⚠️ フレーム境界/他キーフレーム/マーカーへの吸着 |
| **Auto-Keying モード** | Blender/Maya | ❌ パラメータを変更するだけで自動キーフレーム生成 |
| **Keying Set** | Blender | ❌ どのプロパティにキーを打つかをプリセット保存 |

### キーフレーム補間

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Easy Ease / Easy Ease In / Out ワンキー適用** | AE (F9/Shift+F9) | ⚠️ F9 はあるが速度ベース自動イージング未実装 |
| **補間タイプ切替（Constant/Linear/Bezier/Auto/Hold）** | 全アプリ | ⚠️ |
| **Exponential Scale** | AE | ❌ 2D ズームの指数補間（線形ズームの奥行き不自然さを解消） |
| **イージングのコピペ** | AE (EaseCopy) | ❌ キーフレーム間でイージング設定をコピー |
| **プリセットイージングライブラリ** | Motion 4/Flow | ❌ Elastic/Bounce/Back/Expo をプルダウンから一発適用 |

### キーフレーム表示

| 機能 | 参照元 | 状態 |
|---|---|---|
| **キーフレーム色分け（補間タイプ別）** | AE | ⚠️ |
| **選択キーフレームの情報ボックス** | Blender | ❌ 選択キーフレームの時刻/値/補間タイプ/ハンドル値をポップアップ表示 |
| **ゴースト（Onion Skin）表示** | MotionBuilder | ❌ 前後 N フレームを半透明オーバーレイでタイムライン上に表示 |
| **モーショントレイル** | Maya | ❌ タイムライン上に軌跡の残像表示 |
Artifact のタイムラインは `ArtifactTimelineWidget` を中核に、左ペイン（`ArtifactLayerPanelWidget`）＋右ペイン（`ArtifactTimelineTrackPainterView`）＋Navigator＋ScrubBar＋WorkAreaControl＋DopeSheet で構成。owner-draw 移行済みで基盤は堅い。


---

## 🟡 P1: グラフエディタ / カーブ編集

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Value Graph ⇔ Speed Graph 切替** | AE | ⚠️ 宣言あり。Curve Editor 本体未接続 |
| **ベジェハンドル独立操作** | AE/Blender | ⚠️ 部分的 |
| **ハンドルタイプ切替（Free/Aligned/Vector/Auto/Auto Clamped）** | Blender | ❌ |
| ** Proportional Editing（減衰編集）** | Blender | 🟡 Phase 1: 右ペイン marker drag と area body/edge の時間編集まで対応。value/graph は未対応 |
| **Pivot Point（変形基点切替）** | Blender | ❌ Bounding Box Center / 2D Cursor / Individual Origins |
| **Auto-Snap（自動スナップ）** | Blender | ❌ 編集後に自動でフレーム/秒/マーカーに吸着 |
| **Normalize Curves（正規化表示）** | Blender | ❌ 全カーブを 0-1 範囲に正規化して相対比較 |
| **Clean Keyframes（間引き）** | Blender | ❌ 閾値以下で冗長キーフレームを自動削除 |
| **Decimate Curves（大幅間引き）** | Blender | ❌ 誤差許容値を指定して積極的に間引き |
| **Smooth Curves（平滑化）** | Blender | ❌ 選択範囲のキーフレームを平滑化 |
| **Bake Curves（ベイク）** | Blender | ❌ エクスプレッション/物理演算結果をキーフレーム化 |
| **Euler Filter** | Blender | ❌ 3D 回転のジンバルロックを回避するフィルタ |
| **カーブミラー（時間/値）** | Blender | ❌ カーブを時間軸/値軸で反転 |
| **複数カーブの同時表示＆編集** | AE/Blender | ⚠️ |
| **Summary Row（全カーブの合成プレビュー）** | Blender Dope Sheet | ❌ |
| **Channel Grouping（プロパティ種別でグループ化）** | Blender/Houdini | ❌ |

---

## 🟡 P1: マーカー・注釈系

| 機能 | 参照元 | 状態 |
|---|---|---|
| **コンポジションマーカー（タイムライン全体）** | AE | ⚠️ |
| **レイヤーマーカー（レイヤー固有）** | AE | ❌ |
| **マーカーに名前とコメント** | AE/Premiere | ❌ |
| **マーカーに色ラベル** | Premiere | ❌ |
| **マーカーに duration（範囲マーカー）** | Premiere | ❌ 開始〜終了の範囲を持つマーカー |
| **マーカー間ジャンプ（Shift+J/K）** | Premiere | ❌ |
| **マーカーをキーフレームに変換** | AE | ❌ |
| **マーカー一覧パネル（クリックでジャンプ）** | Premiere | ❌ |
| **マーカーに Web リンク/チャプター情報** | Premiere | ❌ |
| **Clip Marker（クリップ単位のマーカー）** | Premiere | ❌ |
| **Flag（フラグ）色分け** | Resolve | ❌ 赤/青/緑/黄/紫のフラグ |

---

## 🟡 P1: ワークエリア・再生制御

| 機能 | 参照元 | 状態 |
|---|---|---|
| **In/Out 点のドラッグ編集** | AE/Premiere | ✅ WorkAreaControlWidget で実装 |
| **ワークエリアのループ再生** | AE | ⚠️ |
| **ワークエリアをタイムライン全体に戻す（ダブルクリック）** | AE | ❌ |
| **再生解像度切替（Full/Half/Quarter/Third）** | AE | ⚠️ Draft/Fast Draft のみ |
| **Skip Frames（再生時コマ落とし指定）** | AE | ❌ 0/1/2/5 フレーム飛ばし再生 |
| **RAM Preview の範囲指定** | AE | ⚠️ |
| **Jog/Shuttle 操作** | Premiere/Avid | ❌ JKL キーで可変速再生（J=逆再生/K=停止/L=再生、連打で加速） |
| **プリロール/ポストロール再生** | Premiere | ❌ 編集点の前後 N 秒を自動再生 |
| **Play Around（現在地の前後を再生）** | AE | ❌ |
| **Audio Scrubbing（スクラブ時のリアルタイム音声）** | AE/Premiere | ✅ AudioScrubController 実装済み |
| **フレーム単位のナッジ（Alt+←→）** | Premiere | ❌ 選択クリップ/キーフレームを 1 フレームずつ移動 |

以下、AE/Premiere/Resolve/Blender/Maya/C4D/Houdini/Nuke/DAW/Cavalry/Rive/Unity/Unreal/Apple Motion/Final Cut/Avid の 16 アプリ群から、Artifact のタイムラインに不足している機能を収集。**太字は既存マイルストーンで計画済みのもの**。


---

## 🟡 P1: クリップ編集（動画編集ツール由来）

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Ripple Edit** | Premiere/Resolve | ❌ クリップ端ドラッグ→後続クリップが自動で詰まる |
| **Rolling Edit** | Premiere/Resolve | ❌ 2クリップの編集点を同時移動、合計長不変 |
| **Slip Edit** | Premiere/Resolve | ❌ クリップ長不変、内容を時間方向にスライド |
| **Slide Edit** | Premiere/Resolve | ❌ クリップ自体を移動、隣接クリップの長さ自動調整 |
| **Razor Tool（カミソリ分割）** | Premiere/Resolve | ❌ クリックでクリップを2分割 |
| **Trim Mode（トリム編集モード）** | Premiere/Resolve | ❌ 前クリップ最終フレーム＋次クリップ先頭フレームを左右に表示する専用 UI |
| **Lift / Extract** | AE/Premiere | ❌ 選択範囲を抜き取る（隙間残す/詰める） |
| **Split Layer（Cmd+Shift+D）** | AE | ❌ レイヤーをプレイヘッド位置で2分割 |
| **Nudge Clip（Alt+←→ 1フレーム移動）** | Premiere/Final Cut | ❌ |
| **Snap トグル** | Premiere | ❌ |
| **Linked Selection** | Premiere | ❌ 映像＋音声の連動選択 ON/OFF |

---

## 🟡 P1: トラック・レイヤー構造

| 機能 | 参照元 | 状態 |
|---|---|---|
| **トラック高さ個別調整** | Premiere/DAW | ❌ 波形トラックは高く、キーフレームは低く |
| **トラック折りたたみ/展開** | DAW | ❌ |
| **トラックグループ/フォルダ** | DAW/Resolve | ❌ 複数トラックを束ねて一括操作 |
| **Track Stack** | Logic Pro | ❌ 複数トラックを束ねてサブミックス |
| **VCA Fader** | Pro Tools | ❌ 複数トラック相対音量の一括制御 |
| **トラックカラー** | Resolve | ❌ トラックヘッダ全体に色付け |
| **トラック複製** | DAW | ❌ |
| **トラックテンプレート保存** | Premiere/Logic | ❌ トラック構成＋エフェクトをテンプレート化 |

---

## 🟡 P1: タイムライン UI 補助

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Snap 対象選択的 ON/OFF** | Premiere/Blender | ❌ マーカー/クリップ端/フレームを個別にスナップ設定 |
| **タイムラインルーラー マーカー帯** | Premiere/Resolve | ❌ ルーラー領域のマーカーを色分け＋名前表示 |


---

## 🔵 P2: DAW 由来の高度機能（Ableton / Logic / Pro Tools）

| 機能 | 概要 | 状態 |
|---|---|---|
| **Comping（テイク切貼り）** | 複数テイクを縦に並べ、良い部分をスワイプで選択してマスターテイクに合成 | ❌ |
| **Warp Markers（伸縮マーカー）** | オーディオ波形上にマーカーを置き、ドラッグで時間伸縮。Artifact では time remap 直操作に相当 | ❌ |
| **Fade ハンドル** | クリップ端にフェードイン/アウト用のドラッグ可能なハンドル | ❌ |
| **Crossfade（クロスフェード）** | 重なった2クリップ間に自動クロスフェード | ❌ |
| **Clip Gain Envelope** | クリップ上の音量/不透明度をドラッグ可能なポイントで直接調整 | ❌ |
| **Automation Lanes** | 各パラメータのオートメーションカーブを別レーンで表示＆編集 | ⚠️ |
| **Groove Pool（グルーヴテンプレート）** | 他クリップのタイミング/ベロシティパターンを抽出し適用 | ❌ |
| **Track Freeze（トラック凍結）** | 重いエフェクトチェーンを一時的にレンダリングして負荷軽減 | ❌ |
| **Track Grouping（編集/ミックスグループ）** | 複数トラックをグループ化し一括編集/ミックス | ❌ |
| **Tempo Track** | テンポを時間軸で変化させるトラック。BPM の自動化 | ❌ |
| **Time Signature Track** | 拍子の変化を管理するトラック | ❌ |
| **Punch In/Out** | 指定範囲だけ録音/記録する範囲設定 | ❌ |
| **Strip Silence** | 無音/無変化部分を自動検出して分割/削除 | ❌ |
| **Arrangement View ⇔ Session View** | Ableton の2面モード。線形タイムライン⇔非線形クリップランチャー | ❌ |

---

## 🔵 P2: アニメーション特化機能（Maya / MotionBuilder / Unity）

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Animation Layers** | Maya/MotionBuilder | ❌ 複数アニメーションレイヤーを加算/上書き合成。個別に mute/solo/weight 調整 |
| **Pose Blending** | Maya | ❌ 2つのポーズをブレンドして中間ポーズ生成 |
| **Time Warp Curve** | Maya | ❌ クリップに適用する時間歪曲カーブ。コマ撮り/スロー/逆再生を非破壊で |
| **Animation Clip Looping** | Unity/Maya | ❌ クリップをループ設定（回数指定/無限） |
| **Additive Blending** | Maya | ❌ 複数クリップの差分だけを加算合成 |
| **Animation Events** | Unity | ❌ クリップの特定フレームに関数呼出しイベントを埋め込み |
| **Avatar Mask** | Unity | ❌ 部位別にアニメーションの適用/非適用を制御 |
| **Ghosting（Onion Skin）** | MotionBuilder | ❌ 前後 N フレームのポーズを半透明重畳表示 |
| **Motion Trail** | Maya | ❌ 軌跡の残像表示 |
| **Retime Curve** | Maya/Houdini | ❌ クリップ/レイヤーの時間軸をカーブで歪曲。非線形 time remap |

---

## 🔵 P2: 特殊アプリケーション由来

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Behavior-based Animation** | Apple Motion/Cavalry | ❌ キーフレームではなく Behavior（挙動）でアニメーション。Spring/Pendulum/Throw/Rotate 等をドラッグ＆ドロップで適用 |
| **Duplicator Timeline** | Cavalry | ❌ Duplicator の複製アニメーションをタイムラインで表示・編集 |
| **State Machine + Timeline 統合** | Rive | ❌ アニメーション状態遷移図とタイムラインが連動。状態切替の補間をグラフで編集 |
| **Non-destructive Clip-based Animation** | Maya Time Editor | ❌ 元アニメーションを破壊せず、クリップとして配置・編集・ブレンド |
| **Sequencer Shot Track** | Unreal | ❌ カメラカットをタイムライン上に配置し、ショット間のブレンド/フェードを管理 |
| **Material Parameter Track** | Unreal | ❌ マテリアルパラメータをタイムライン上でアニメーション |
| **Sub-sequence Track** | Unreal | ❌ シーケンスの中に別シーケンスを入れ子配置 |


---

## 🔵 P2: DAW スタイル入力・検索・フィルタ

| 機能 | 参照元 | 状態 |
|---|---|---|
| **DAW-Style Input Surface** | 📋 M-TL-12 計画済み | タイムライン上で直接数値入力＋MIDI コントローラ対応 |
| **Layer Search（Ctrl+F レイヤー名検索）** | 📋 M-TL-6 計画済み | |
| **Keyframe Search（キーフレーム値/時刻検索）** | 📋 M-TL-7 計画済み | |
| **Flat Keyframe View（フラット表示）** | 📋 U キーで切替 | Dope Sheet 的な全キーフレーム一覧 |
| **Filter by Selected Layer** | Blender/AE | ❌ 選択レイヤーのキーフレーム/プロパティのみ表示 |
| **Search Filter with Regex** | Blender | ❌ 正規表現でチャンネル名検索 |
| **Only Show Animated Properties** | Apple Motion | ❌ キーフレームが存在するプロパティのみ表示 |
| **Property Column Filter** | AE | ❌ プロパティカラムのフィルタ（Position/Rotation/Scale だけ表示等） |

---

## 🔵 P2: NLE マルチカム・比較

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Multicam Source Sequence** | Premiere/Final Cut | ❌ 複数アングルを同期してグリッドプレビュー、クリックで即切替 |
| **Comparison View（リファレンス動画オーバーレイ）** | Resolve | ❌ 外部リファレンス動画をタイムライン上に半透明で重畳 |
| **Offline Reference Clip** | Resolve | ❌ リファレンス動画をビューポート＋タイムライン両方に表示 |

---

## 📊 優先度マトリクス

| 優先 | カテゴリ | 件数 | 代表機能 |
|---|---|---|---|
| 🔴 **即実装価値あり** | レイヤースイッチ列 | 12 | Shy/Solo/Lock/FX/FrameBlend/MotionBlur/Quality per layer |
| 🔴 | キーフレーム編集 | 8 | Roving/JKジャンプ/時間スケール/Auto-Keying |
| 🔴 | マーカー | 10 | レイヤーマーカー/名前/色/duration/一覧パネル |
| 🟡 | グラフエディタ | 16 | Value↔Speed Graph/Proportional Editing/Normalize/Bake |
| 🟡 | クリップ編集 | 11 | Ripple/Rolling/Slip/Slide/Razor/Trim Mode |
| 🟡 | トラック構造 | 8 | 高さ調整/折りたたみ/フォルダ/テンプレート |
| 🟡 | タイムライン UI | 9 | ミニマップ/クリップサムネイル/タイプライタースクロール |
| 🔵 | DAW 機能 | 14 | Comping/Warp/Fade/Groove Pool/Track Freeze |
| 🔵 | アニメ特化 | 10 | Animation Layers/Time Warp/Ghosting/Retime |
| 🔵 | 特殊アプリ | 15 | Behavior-based/State Machine/Speed Point/Strip Silence |

---

## 関連文書

- `docs/planned/MILESTONE_TIMELINE_INDEX_2026-04-22.md` — タイムライン計画インデックス
- `docs/planned/MILESTONE_TIMELINE_FEATURE_IMPLEMENTATION_2026-04-03.md` — 上位計画
- `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md` — キーフレーム編集本筋
- `docs/planned/MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md` — カーブエディタ (M-TL-13)
- `docs/planned/MILESTONE_DAW_STYLE_INPUT_SURFACE_2026-04-08.md` — DAW 入力面 (M-TL-12)
- `docs/planned/MILESTONE_TIMELINE_LAYER_SEARCH_2026-03-28.md` — レイヤー検索 (M-TL-6)
- `docs/planned/MILESTONE_TIMELINE_RIGHT_PANE_FULL_OWNER_DRAW_2026-04-08.md` — Owner-Draw 基盤
- `docs/planned/MILESTONE_TIMELINE_TRANSFORM_KEYFRAME_EDITING_2026-04-12.md` — Transform キーフレーム
- `docs/WIDGET_MAP.md` — ウィジェット責務
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp` — 実装
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm` — 右ペイン
| **Camera Cut Track** | Unreal | ❌ アクティブカメラを切り替えるカットトラック |
| **Take Selector** | Resolve | ❌ クリップに複数テイクを紐付け、プルダウンで切替 |
| **Speed Point 編集** | Resolve | ❌ クリップ上に速度変化点を追加し、区間ごとに変速。speed ramp の直感的編集 |
| **Compound Clip** | Resolve | ❌ 選択範囲を compound clip にパッケージ化。pre-compose の簡易版 |
| **Fascia カスタマイズ** | Resolve | ❌ トラックヘッダの表示項目をカスタマイズ |
| **Stacked Timeline** | Resolve | ❌ タイムラインをスタック表示。複数タイムラインの同時プレビュー |
| **Groove Template 抽出** | Ableton | ❌ 任意のクリップからタイミングパターンを抽出しライブラリ化 |
| **Strip Silence** | Pro Tools/Logic | ❌ 無音区間を自動検出して一括削除。無変化区間の除去に応用 |
| **Show Audio Time Units** | Premiere | ❌ サンプル単位表示 |
| **Typewriter Scrolling** | DAW | ❌ 再生中プレイヘッドが常に中央 |
| **Follow Playhead ON/OFF** | Premiere | ❌ |
| **タイムライン背景縞模様** | Premiere | ❌ フレーム/秒の視覚的区切り |
| **タイムライン ミニマップ** | Premiere | ❌ 全体俯瞰スクロールバー |
| **クリップバッジ（FX/Proxy 適用済み表示）** | Premiere | ❌ |
| **クリップサムネイル表示切替** | Premiere/Resolve | ❌ 連続フレームサムネイル ON/OFF |
