# M-VP-DCC-1: ビューポート DCC パリティ導入（C4D / Houdini / Maya）

**最終更新:** 2026-09-22

**ステータス:** Not Started（分析完了。未着手の欠落は P0 4項目、P1 7項目、P2 6項目。Autograph を含む）

## 目的

`docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md` で特定したビューポートの
欠落機能を、既存インフラの再利用を優先して段階的に導入する。新規機能はデータモデルを変えず、
ビューポート表示・操作の追加に限定する。

## 前提（分析で確定した事実）

- チャンネル分離表示、オニオンスキン、X-Ray、グリッド/ルーラー、セーフマージン、ガイド、
  マルチビューポート、ブックマーク、ピエメニュー、Isolation（表示のみ）は実装済み。
- 未実装の代表は、種類別ビューポートフィルタ、Interactive Render Region、Box zoom/crop、
  tumble pivot のカーソル下設定、ghosted context、isolate の状態復元、tear-off viewport。
- 既存の再利用対象: `ArtifactRenderROI`、`ProgressiveRenderer`（`ArtifactFrameCache.cppm`）、
  既存ズーム補間 API、`PaneState`、`ShortcutBindings`、Diligent オーバーレイ描画 API。

## 導入計画

| Phase | 内容 | 状態 | 主な変更対象 |
| --- | --- | --- | --- |
| P0-1 | Box zoom / Box crop | Not Started | `ArtifactCompositionEditor` / `ArtifactCompositionRenderController` |
| P0-2 | Tumble pivot under cursor | Not Started | 同上 |
| P0-3 | Interactive Render Region（ROI + Progressive） | Not Started | `ArtifactFrameCache` / ROI / overlay |
| P0-4 | ビューポート タイプ別フィルタ | Not Started | `ArtifactCompositionRenderController` |
| P1-1 | Isolate Select の状態復元 | Not Started | 同上 |
| P1-2 | Ghosted context display | Not Started | 同上 |
| P1-3 | Per-viewport 設定 + Apply to all split views | Not Started | `PaneState` / 表示設定 |
| P1-4 | Maya 風シェーディングトグル | Not Started | 同上 |
| P1-5 | Viewer exposure controls（Gain/Gamma/Saturation） | Not Started | 表示ポストプロセス |
| P1-6 | チャンネル表示の Straight / Luminance / Matte バリアント | Not Started | `ViewportChannelDisplayMode` |
| P1-7 | パス overlay の可視性モードと種類別フィルタ | Not Started | overlay / 表示フィルタ |
| P2-1 | C4D HUD 相当のパラメータ常設表示 | Not Started | オーバーレイ / 既存 modal gizmo |
| P2-2 | Hardware fog / volumetric fog / bloom | Not Started | Diligent パス |
| P2-3 | Viewport tear-off copy | Not Started | ペイン管理 |
| P2-4 | Construction plane handle | Not Started | Construction Layer / 既存ギズモ |
| P2-5 | Object Type Filter のビューポート拡張 | Not Started | レンダーフィルタ / overlay |
| P2-6 | Viewer format overriding（VP 単位の解像度/PAR） | Not Started | ペイン / レンダーターゲット |

### P0-1 Box zoom / Box crop

- ドラッグ矩形の内側へズームイン、外側へズームアウト（Houdini の Ctrl+Alt ボックス挙動に相当）。
  クロップは「画面窓」の保持として扱い、カメラパラメータは変更しない。
- 既存のマーキー選択入力、`zoomAtFactor` 系の補間、操作中プレビュー品質の downsample を再利用する。
- 新規操作は `ShortcutBindings` の Viewport ローカルコンテキストへ登録し、固定キーを追加しない。

### P0-2 Tumble pivot under cursor

- Space+Z 相当でカーソル下の点を一時ピボットにし、カメラ位置のみ移動して向きは保持する。
- ピボットはプレビュー専用とし、カメラレイヤーのパラメータや保存値は変更しない。
- ピボットの小マーカーは既存 overlay 描画（線・矩形）で表現する。

### P0-3 Interactive Render Region

- 既存 ROI（viewport ROI / scissor ROI）と `ProgressiveRenderer` を組み合わせ、
  矩形内のみを解像度スライダ付きで再レンダーする。
- 矩形の移動/拡縮ハンドルは既存ギズモ描画を再利用し、確定/取消は既存 modal 入力へ接続する。
- 負荷対策として、ドラッグ中は品質を落とし、確定後にのみ progressive アップグレードを要求する。

### P0-4 ビューポート タイプ別フィルタ

- レイヤー種別（ライト/カメラ/Null/スプライン/ジェネレータ/ボリューム等）ごとの表示可否を
  ビューポート専用 override として保持する。レイヤーの `visible` とシリアライズは変更しない。
- 既存 `CompositionLayerRenderFilter` を拡張するのではなく、別の表示フィルタ状態として追加し、
  Render Queue / 出力系のフィルタとは分離する。

### P1-1 Isolate Select の状態復元

- 分離開始時に表示状態をスナップショットし、解除時に復元する。Undo は既存経路を使い、
  新しい signal/slot は追加しない。

### P1-2 Ghosted context display

- 編集対象以外を低不透明度で残す表示段階を追加する。X-Ray の全体減衰とは別状態として扱い、
  既存の overlay/render 経路へ不透明度パラメータを渡す形にする。

### P1-3 Per-viewport 設定 + Apply to all split views

- `PaneState` にビューポート表示設定の適用範囲を持たせ、Houdini の
  "Apply operations to all split views" 相当を再現する。設定の保存形式は既存の
  View テンプレート経路を再利用する。

### P1-4 Maya 風シェーディングトグル

- Wireframe on Shaded / Backface Culling / Bounding Box / Cycle rig display を、
  3D モデルの `render.mode`、LOD、X-Ray、Rig overlay を組み合わせて提供する。

### P1-5 Viewer exposure controls（Autograph Post-processing 相当）

- Gain（-6〜+6 stop）/ Gamma（0〜5）/ Saturation（0〜4）をビューポート表示にのみ掛け、
  HDR の明部・暗部を確認できるようにする。全体トグルと個別スイッチを付ける。
- 既存の linear / 32bit 合成パイプライン上の表示ポストプロセスとして実装し、
  保存・出力・カラーサンプラの読み取り値は変えない（表示専用であることを HUD に明示）。

### P1-6 チャンネル表示の Straight / Luminance / Matte バリアント

- `ViewportChannelDisplayMode` に、Unpremultiplied（RGB/R/G/B Straight）、
  Luminance（Rec 709 係数）、Matte（αを赤へ加算）を追加する。
- 既存のチャンネル表示 SRV 選択・合成表示経路を再利用し、D3D12 / Vulkan で同一結果にする。

### P1-7 パス overlay の可視性モードと種類別フィルタ（Autograph Path Overlays 相当）

- パス overlay を Shape Layer / Mask / その他（follow-path 軌跡など）の種類別に表示可否を切替。
- Shape Contour Visibility を Always / Never / Hovered or selected / Selected Layers の
  4モードで切替（既存のシェイプ・マスク overlay 描画に可視性ゲートを追加する形）。
- P0-4 のレイヤー種別フィルタとは別の、パス単位の表示制御として実装する。

### P2 以降

- P2-1 は既存 modal gizmo 入力とドラッグ HUD を再利用する。P2-2 は Diligent パス追加のため
  `MILESTONE_3D_VIEWPORT_HARDENING.md` の P4（Volume/DOF）と合流させる。
- P2-3 tear-off は Dock 分離・マルチビューポートとは別のウィンドウ管理設計が必要なため、
  既存のペイン管理方針を確認してから着手する。
- P2-6 Viewer format overriding は、ペイン単位のレンダーターゲット解像度・Pixel Aspect Ratio
  の上書きと、既存 View テンプレート/ブックマークへの保存要否の設計が先行条件。

## 受入条件

- 追加した各操作が、既存のナビゲーション・ギズモ操作と競合しない
  （マーキー、パン、ズーム、Orbit、モディファイア併用）。
- 追加した表示状態は、Composition を再読込しても既存データを変更しない
  （ビューポート専用 override は保存対象を明示する）。
- 新規 signal/slot、QtCSS、QImage、QPainter 合成を追加しない。
- ドラッグ中のフレームで新規の大きな確保を発生させない。
- D3D12 と Vulkan で同じ表示・操作結果になる。
- ビルド・実機確認はユーザーの明示指示後に実施する。

## 実装順序の推奨

- **実装順序の推奨:** P0-1 / P0-2（ナビゲーション基礎）→ P1-5 / P1-6（表示ポストプロセスと
  チャンネルバリアント。既存経路の小拡張で受入が容易）→ P0-3 IRR → P0-4 / P1-1 / P1-2 / P1-7 →
  P1-3 / P1-4、以降 P2。

## 検証状況

- 2026-09-22: 分析（`docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md`）のみ完了。
  実装・ビルド・実機確認は未実施。
- 本環境では git が起動しないため、差分・コミット・gitlink の確認は未実施。

## 関連文書

- `docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md`
- `docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md`
- `docs/planned/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_TODO_2026-09-04.md`
- `docs/planned/MILESTONE_3D_VIEWPORT_HARDENING.md`
- `Insight.md`（2026-09-22 の項目）
