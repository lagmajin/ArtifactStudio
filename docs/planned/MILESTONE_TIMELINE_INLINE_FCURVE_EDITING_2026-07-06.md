# Timeline Inline F-Curve Editing（Maya Graph Editor ライク編集）

> 2026-07-06 作成
> スコープ: TrackPainterView のトラックレーン上で Maya Graph Editor 風のインラインカーブ編集を可能にする

## Goal

`ArtifactTimelineTrackPainterView` の各プロパティトラックレーン上で、キーフレームマーカーだけでなく **補間カーブの可視化・タンジェントハンドル操作・垂直値ドラッグ** を可能にし、Maya の Graph Editor に近いインライン編集体験を実現する。

## 現状とギャップ

### 既に完備している土台

| 資産 | 状態 | 部位 |
|------|------|------|
| `CurveKey` 構造体（inHandleFrame/Value, outHandleFrame/Value, smooth） | ✅ | `ArtifactCurveEditorWidget.ixx` |
| `ArtifactCurveEditorWidget`（Value/Speed Graph, ベジェハンドル編集） | ✅ | 818行 |
| `ArtifactTimelineTrackPainterView`（キーフレームマーカー描画・選択・移動） | ✅ | `TrackPainterView.ixx` / `.cppm` |
| `TimelineKeyframeSnapshotCommand`（Undo/Redo 基盤） | ✅ | `ArtifactTimelineWidget.cppm` |
| `snapTimelineFrameToEditTargets`（フレームスナップ） | ✅ | `ArtifactTimelineWidget.cppm` |
| EasingLab / KeyPattern / 補間プリセット | ✅ | `EasingLabWidget` / `KeyPatternDialog` |
| Auto/Flat/Linear タンジェント切替 | ✅ | CurveEditor ツールバー |

### ギャップ

現在 `TrackPainterView` はキーフレームの**菱形マーカー描画のみ**であり、Maya Graph Editor で可能な以下の操作がトラックレーン上でできない：

- 補間カーブ（ベジェ曲線）のレーン内直接表示
- キーフレームの垂直ドラッグによる値変更（水平＝時間移動のみ）
- タンジェントハンドルのレーン上での表示・ドラッグ操作
- 選択キーフレームの値・補間情報のリッチポップアップ
- 複数キーフレームのリージョンスケール（時間＋値方向）

## Scope

- `ArtifactTimelineTrackPainterView` の描画・入力拡張
- `KeyframeMarkerVisual` の拡張（補間セグメント情報、値表示用データ）
- 垂直値ドラッグ操作（Shift+ドラッグ または 修飾キーなし垂直成分）
- タンジェントハンドルのインライン描画・ヒットテスト・ドラッグ
- リッチホバーポップアップ（値・フレーム・補間タイプ・前後キー差分）
- リージョン選択＋スケール変形（領域選択からの一括時間/値スケール）
- 全操作の Undo/Redo 対応

## Phases

### Phase 1: インラインカーブ可視化と値ドラッグ（M-IC-1〜3）

**目標**: トラックレーン上に補間カーブを表示し、キーフレームの垂直値ドラッグを可能にする

- `M-IC-1` **レーン内補間カーブ描画**
  - `KeyframeMarkerVisual` に前後キーへの補間セグメント頂点データを追加
  - `paintEvent` 内で `QPainterPath::cubicTo` によるベジェカーブ描画
  - 描画キャッシュ（QPixmap バッファ）によるパフォーマンス確保
  - レーン内カーブの表示/非表示切替（ショートカット `Shift+U` など）

- `M-IC-2` **垂直値ドラッグ編集**
  - キーフレームマーカー上での Y 軸ドラッグを検出し値変更にマップ
  - 水平ドラッグ＝時間移動、垂直ドラッグ＝値変更（閾値で判定）
  - 修飾キーによる軸ロック（Shift=垂直のみ、Ctrl=水平のみ）
  - ドラッグ中のリアルタイム値プレビュー表示
  - `TimelineKeyframeSnapshotCommand` による Undo/Redo 対応

- `M-IC-3` **リッチ値ポップアップ**
  - ホバー時に現在値・フレーム番号・補間タイプ・前後キーとの差分をポップアップ表示
  - 複数選択時は最小/最大/平均値のサマリー
  - 現在の `formatHoveredKeyframeSummary` をリッチ版に拡張

### Phase 2: タンジェントハンドルインライン操作（M-IC-4〜5）

**目標**: トラックレーン上でベジェタンジェントハンドルを直接表示・編集できるようにする

- `M-IC-4` **タンジェントハンドル表示とドラッグ**
  - 選択キーフレームの in/out ハンドルをレーン内に小さく描画（6〜8px の円）
  - ハンドルヒットテスト（半径 8px）
  - ハンドルドラッグで `CurveKey::inHandleFrame/Value`, `outHandleFrame/Value` をリアルタイム更新
  - ハンドルドラッグ中の制約（Shift=角度 15° スナップ、Ctrl=長さ維持）
  - ハンドル表示の on/off 切替（既存の Handle ボタンを TrackPainterView にも反映）

- `M-IC-5` **タンジェントブレイク/アンブレイク**
  - `CurveKey` に `brokenTangents` フラグ追加
  - コンテキストメニュー「Break Tangents / Unify Tangents」
  - 左右独立ハンドル操作
  - シリアライズ（プロジェクト保存/読込）対応

### Phase 3: リージョン選択と高度編集（M-IC-6〜8）

**目標**: 複数キーフレームの一括操作とカーブ最適化

- `M-IC-6` **リージョン選択＋スケール変形（Region Tool）**
  - `KeyframeAreaVisual` にスケールハンドル（四隅＋辺中央）を追加
  - 時間方向スケール（左右ハンドルドラッグ）
  - 値方向スケール（上下ハンドルドラッグ）
  - フォールオフ（中央から端へ減衰する変形量）オプション
  - スケール中心点のピボット移動

- `M-IC-7` **レーン内カーブの値域自動フィット**
  - 選択プロパティの値域に合わせてレーン内カーブの Y 軸表示を自動正規化
  - 複数プロパティ同時表示時のスタック/オーバーレイ表示切替
  - フレーム範囲選択による X 軸フィット

- `M-IC-8` **キーフレーム間速度可視化オーバーレイ**
  - 2 キー間のイージング強度に応じて補間カーブの色/太さを変化
  - Linear=細線・標準色、Ease=太線・暖色、EaseInOut=中間
  - Speed Graph モードとの整合

### Phase 4: 発展機能（M-IC-9〜13）

**目標**: プロフェッショナルアニメーションワークフローのための高度機能

- `M-IC-9` **バッファカーブ（参照カーブ）**
  - 別プロパティ/別レイヤーのカーブを半透明ゴースト表示
  - タイミング比較のための参照カーブ選択 UI
  - 差分ハイライト（値の差分を塗りつぶし表示）

- `M-IC-10` **キーフレームリダクション（Simplify Curve）**
  - Douglas-Peucker アルゴリズムによる冗長キーフレームの自動削減
  - 許容誤差パラメータ設定ダイアログ
  - 削減前/削減後の差分プレビュー
  - Undo 対応

- `M-IC-11` **カーブスムース/リサンプル**
  - 選択範囲のカーブをガウシアンフィルタで平滑化
  - リサンプリング（指定フレーム間隔でキー再配置）
  - フィルタ強度パラメータ

- `M-IC-12` **インフィニティカーブ（Pre/Post Infinity）**
  - 最終キーフレーム以降の Cycle/Repeat/Mirror/Offset モード
  - カーブ表示にサイクル領域を半透明で延長描画
  - プロパティへの Infinity 設定反映

- `M-IC-13` **キーフレームミュート/ウェイト**
  - 個別キーフレームの一時無効化（値スキップ、前後キー間で補間）
  - キーフレームブレンドウェイト（0.0〜1.0）の設定
  - ミュート状態の視覚表現（グレーアウト）

## 依存関係

```
M-IC-1（カーブ可視化）
 ├─ M-IC-2（値ドラッグ）
 ├─ M-IC-3（値ポップアップ）
 ├─ M-IC-7（値域フィット）
 └─ M-IC-8（速度可視化）

M-IC-4（ハンドル表示操作）
 └─ M-IC-5（タンジェントブレイク）

M-IC-6（Region Tool）
 └─ Phase 4 各機能

Phase 4（発展機能）
 ├─ M-IC-9（バッファカーブ）
 ├─ M-IC-10（リダクション）
 ├─ M-IC-11（スムース/リサンプル）
 ├─ M-IC-12（インフィニティ）
 └─ M-IC-13（ミュート/ウェイト）
```

## 既存マイルストーンとの関係

| 既存 | 関係 |
|------|------|
| `M-TKF` / `MILESTONE_TIMELINE_CURVE_EDITOR_MODE` | 本マイルストーンは Curve Editor Mode の**トラックレーンインライン編集**拡張。`Tab` によるモード切替の枠組みは `M-TKF` が担当。 |
| `M-TL-5` Timeline Keyframe Editing | キーフレーム編集の本筋。本マイルストーンは `M-TL-5` のサブワークストリーム。 |
| `M-TL-9` Timeline Visual Language | カーブ描画・ハンドル描画のビジュアルスタイルは `M-TL-9` に準拠。 |
| `M-TL-12` DAW-Style Input Surface | ドラッグ操作・修飾キーの設計は `M-TL-12` の流儀に合わせる。 |
| `WIDGET_MAP.md` | ショートカット `U`（flat filter）、`Tab`（curve mode）の定義は維持。 |

## 対象ファイル

```
Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx
  └─ KeyframeMarkerVisual 拡張（補間セグメント、値表示用フィールド）
  └─ 新規シグナル: valueDragRequested, handleDragRequested, regionScaleRequested

Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm
  └─ paintEvent: カーブ描画パス、ハンドル描画
  └─ mousePressEvent/Move/Release: 値ドラッグ、ハンドルドラッグ、リージョンスケール
  └─ 描画キャッシュ管理

Artifact/include/Widgets/ArtifactCurveEditorWidget.ixx
  └─ CurveKey に brokenTangents, muted, weight 追加（Phase 2, 4）

Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm
  └─ カーブ描画ロジックの共通化（TrackPainterView と共有する描画ユーティリティ）

Artifact/src/Widgets/ArtifactTimelineWidget.cppm
  └─ 新規シグナルの接続、Undo/Redo ラッピング
  └─ TrackPainterView のカーブ表示/ハンドル表示切替制御

Artifact/src/AppMain.cppm
  └─ ショートカット登録（Shift+U: カーブ表示切替 など）
```

## 成功基準

- [ ] トラックレーン上にキーフレーム間のベジェ補間カーブが視認できる
- [ ] キーフレームを上下ドラッグで値変更できる（Undo/Redo 完備）
- [ ] 選択キーフレームのベジェハンドルが表示され、ドラッグ編集できる
- [ ] ホバー時に値・フレーム・補間タイプがポップアップ表示される
- [ ] 複数キーフレームのリージョン選択＋時間/値スケールができる
- [ ] すべての操作が既存のキーフレーム編集 Undo スタックと統合されている
- [ ] カーブ表示/非表示、ハンドル表示/非表示が独立して切替可能
- [ ] 既存の EasingLab・KeyPattern・補間プリセットと競合しない