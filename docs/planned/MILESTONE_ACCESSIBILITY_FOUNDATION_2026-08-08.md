# Accessibility Foundation (2026-08-08)

**最終更新:** 2026-08-08
**状態:** 計画

## 概要

ArtifactStudio のアクセシビリティ基盤。既存の `AccessibilitySettings`（利き手・色覚異常補正・大ターゲット・高コントラスト・フォントスケール・ホバー依存低減）の**コード適用網羅率を高め**、現状欠落している運動障碍者向けの**スティッキーキー**、**シングルハンドモード**、視覚障碍者向けの**ビューポート拡大鏡**を新設する。

## 現状とギャップ

### 既存の強み

| 機能 | 状態 | 備考 |
|------|------|------|
| 色覚異常補正（3種） | ✅ 実装済み | Brettel-Vienot-Mollon。StatusBar, Timeline で適用 |
| 利き手反転（コンテキストメニュー） | ✅ 実装済み | 3ファイルで適用 |
| ショートカット再割り当て | ✅ 実装済み | `ShortcutBindings` + JSON 設定 |
| スクリーンリーダー基盤 | ✅ 実装済み | `setAccessibleName/Description` が広範に使用。OS 標準 TTS に委譲 |

### 適用ギャップ（設定はあるがコード適用が不十分）

| 設定 | 設定UI | コード適用箇所 |
|------|--------|--------------|
| `preferLargeTargets` | ✅ | `scaledSize()` が数ファイルで使われるが、全てのインタラクティブ要素に未適用 |
| `preferHighContrastHints` | ✅ | **0箇所**。全く使われていない |
| `reduceHoverDependency` | ✅ | Timeline の 1箇所のみ |
| `fontScalePercent` | ✅ | 汎用的だがシステムフォントへの伝播経路が不透明 |

### 完全に欠落している機能

| カテゴリ | 機能 | 優先度 |
|----------|------|--------|
| 運動 | スティッキーキー（修飾キーのトグル保持） | 🔴 高 |
| 運動 | シングルハンドモード（マウスボタン→Ctrl/Shift 割当） | 🔴 高 |
| 運動 | キーリピート抑制（バウンスキー/スローキー） | 🟡 中（OS設定と重複しうる） |
| 視覚 | ビューポート拡大鏡 | 🔴 高 |
| 視覚 | カスタムカーソル拡大 | 🟡 中 |
| 視覚 | アニメーション無効化設定 | 🟡 中 |
| 聴覚 | 波形絶対値表示 + クリップ/無音区間視覚インジケーター | 🟡 中 |
| 認知 | 簡易モード（パネル折りたたみ + 必要最小限 UI） | 🟢 低（構造変更が大きい） |

## Phase 1: 高コントラストヒントの実適用（即時・低リスク）

**目的**: 既存設定 `preferHighContrastHints` を実際の UI に反映させる。現在 0箇所の適用を拡大。

**変更箇所**:

| 対象 | 変更 |
|------|------|
| TransformGizmo ハンドル | `preferHighContrastHints` 時に線幅 +30%、色の輝度差 1.2x |
| マスク頂点/ベジェハンドル | ハンドル半径 +3px、線色の彩度最大化 |
| Artifact3DGizmo 軸矢印 | 同様の線幅・色強調 |
| タイムラインカーソル/選択範囲 | カーソル線幅 +1px、選択範囲の縁取り追加 |
| StatusBar 状態インジケーター | すでに `adjustColorForDeficiency` 使用、コントラスト追加 |
| グリッド/ガイド線 | 線幅 +50%、不透明度 min 0.4 |

**実装方針**: `Accessibility::contrastScale()` を各描画パスのスタイル定数に掛ける。新規シグナル不要（静的な設定値の読み取りのみ）。

**ファイル**:
- `Artifact/src/Widgets/Render/TransformGizmo.cppm`
- `Artifact/src/Widgets/Render/Artifact3DGizmo.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`

---

## Phase 2: ホバー依存低減の実適用

**目的**: `reduceHoverDependency()` の適用を Timeline の1箇所から全ホバー情報表示へ拡大。

**変更**:

| 対象 | ホバー依存低減時 |
|------|------------------|
| ツールバーボタン | ツールチップを常時ステータスバーに表示（ホバー不要） |
| タイムラインマーカー情報 | 常時表示（既存1箇所を踏襲） |
| レイヤーパネル | レイヤー名・サムネイルサイズの拡大＋常時プロパティサマリ表示 |
| ビューポート情報オーバーレイ | レイヤー名・フレーム番号を常時オーバーレイ |
| コンテキストメニュー | メニュー項目間隔拡大（`scaledSize` と連動） |

---

## Phase 3: スティッキーキー

**目的**: Ctrl/Shift/Alt を押し続けられないユーザー向け。修飾キーを**トグル式**にし、1回押しで ON、もう1回で OFF。

**仕様**:

```
CapsLock ライクな挙動だが、次の通常キー入力で自動解除する方式も選択可能

設定:
  - 有効/無効
  - モード: "ラッチ"（次キーで解除） / "ロック"（手動解除のみ） / "両方"
  - 対象キー: Ctrl / Shift / Alt / Win（個別設定）
  - フィードバック: ステータスバーに修飾キー状態表示 / サウンド（OS 連携）

実装:
  - QApplication::installEventFilter で keyPress/keyRelease をフィルタ
  - トグル状態を ArtifactAppSettings に保持
  - ステータスバーにインジケーター（Phase 2 の常時表示と統合）
```

**ファイル**:
- `Artifact/src/Settings/AccessibilitySettings.cppm` — スティッキーキー設定追加
- `Artifact/src/Widgets/ArtifactStatusBar.cppm` — 修飾キー状態表示
- `Artifact/src/AppMain.cppm` — グローバルイベントフィルタ登録

---

## Phase 4: シングルハンドモード

**目的**: 片手でマウス操作中に修飾キーを押せないユーザー向け。マウスの**サイドボタン**や**右クリック長押し**に Ctrl/Shift を割り当て。

**仕様**:

```
デフォルトバインディング:
  - マウスボタン4（戻る）→ Shift
  - マウスボタン5（進む）→ Ctrl
  - 右ボタン長押し（500ms）→ Alt

ユーザー設定:
  - 全マウスボタン + 修飾キーの組み合わせを ShortcutBindings JSON で設定可能
  - 有効/無効のグローバルトグル（Alt+Shift+M 等）

実装:
  - QWidget::mousePressEvent / mouseReleaseEvent の共通フィルタ
  - マウスボタン押下中は対応する修飾キーを QApplication に注入
  - 元のマウスボタン機能はボタン単独押し（250ms以内離し）で発動、長押しで修飾キー
```

**ファイル**:
- `Artifact/src/AppMain.cppm` — グローバルマウスフィルタ
- `ArtifactCore/src/UI/ShortcutBindings.cppm` — マウスボタンバインディング追加
- `Artifact/src/Settings/AccessibilitySettings.cppm` — シングルハンド設定

---

## Phase 5: ビューポート拡大鏡

**目的**: フォントスケールの設定を超えて、ビューポートの任意領域をピクセル単位で拡大表示する。弱視ユーザーが細かいマスク頂点やギズモハンドルを正確に操作するための補助。

**仕様**:

```
起動: ショートカット（デフォルト: Alt+Z）またはツールバーボタン

表示:
  - マウス位置を中心に 200x200px の矩形領域を 2〜8x に拡大
  - 半透明オーバーレイ（ビューポート右下に固定表示）
  - 拡大率はマウスホイールで変更

挙動:
  - 拡大鏡内では通常のクリック・ドラッグ操作が透過的にビューポートに伝播
  - 座標変換: 拡大鏡の位置と拡大率から元のビューポート座標を逆算
  - カラーフィルタ: `adjustColorForDeficiency` を適用した描画も選択可能

実装:
  - CompositionRenderController のオーバーレイ描画パスで実装
  - ビューポートの最終フレームバッファから指定領域をキャプチャし拡大描画
  - Diligent readback は使わず、すでに CPU 側にあるプレビュー画像（ramPreview）を利用
```

**ファイル**:
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`

---

## Phase 6: アニメーション無効化と操作タイムアウト（低優先）

**目的**: 前庭障害・光過敏性てんかんのユーザー向け。

| 項目 | 仕様 |
|------|------|
| UI トランジション無効化 | `QWidget::setUpdatesEnabled` 一括制御。ドックのスライドアニメ等を即時切替に |
| タイムラインスクラブ時プレビュー抑制 | 現状すでに `viewportInteracting_` 中のキャッシュ利用は抑制済み。高速更新の抑止を追加 |
| ツールチップ表示時間延長 | 既存の Qt デフォルト (-1 = システム設定依存) を明示的に 5000ms 以上に設定 |

---

## 実装優先順位

| Phase | 内容 | コスト | 効果範囲 | リスク |
|-------|------|--------|---------|--------|
| 1 | 高コントラスト実適用 | 低（描画定数の変更） | 全ビューポートユーザー | 極小 |
| 2 | ホバー依存低減拡大 | 中（複数ウィジェット） | 全 UI | 小 |
| 3 | スティッキーキー | 中（イベントフィルタ） | 運動障碍者 | 中（他ショートカットとの競合） |
| 4 | シングルハンド | 中（マウスフィルタ） | 運動障碍者 | 中（右クリック長押しの誤爆） |
| 5 | 拡大鏡 | 高（オーバーレイ描画） | 弱視ユーザー | 低 |
| 6 | アニメーション無効化 | 低 | 前庭/光過敏 | 極小 |

## 変更対象ファイル一覧

| ファイル | Phase |
|----------|-------|
| `Artifact/src/Settings/AccessibilitySettings.cppm` | 3, 4 |
| `Artifact/include/Settings/AccessibilitySettings.ixx` | 3, 4 |
| `Artifact/src/AppMain.cppm` | 3, 4 |
| `Artifact/src/Widgets/ArtifactStatusBar.cppm` | 2, 3 |
| `Artifact/src/Widgets/Render/TransformGizmo.cppm` | 1 |
| `Artifact/src/Widgets/Render/Artifact3DGizmo.cppm` | 1 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm` | 1, 5 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 5 |
| `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm` | 1, 2 |
| `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm` | 3, 4 |
| `ArtifactCore/include/UI/ShortcutBindings.ixx` | 4 |
| `ArtifactCore/src/UI/ShortcutBindings.cppm` | 4 |
