# マイルストーン: レイヤー専用ビューポート（Layer View）機能キャッチアップ案

> 作成: 2026-07-08 / 状態: Draft（提案・未着手）
> 関連: `docs/planned/MILESTONE_VIEWPORT_ENHANCEMENT_PROPOSAL_2026-07-08.md`（§12 の共有基盤を前提）
> スコープ: 単一レイヤー専用ビューポート `ArtifactLayerEditorWidgetV2` / `ArtifactRenderLayerEditor` / `ArtifactSoftwareLayerTestWidget`
> 目的: 「レイヤー単体専用ビューポートがあるが機能が大幅に遅れている」という指摘への更新案。

---

## 1. 現状診断

### 1.1 実体
- 専用ウィジェット: `ArtifactLayerEditorWidgetV2`（`Artifact/include/Widgets/Render/ArtifactRenderLayerWidgetv2.ixx`）、ラッパー `ArtifactRenderLayerEditor`、ソフトウェア版 `ArtifactSoftwareLayerTestWidget`（`AppMain.cppm:2269` で "Layer View (Diligent)" として起動）。
- 基底は合成ビューポートと**同じ `ArtifactIRenderer`**（`renderer_` を直接使用）。

### 1.2 実装済み（調査根拠 `ArtifactRenderLayerWidgetv2.cppm`）
| 機能 | 所在 | 状態 |
|---|---|---|
| パン/ズーム（zoom 0.05–32） | `:892–938, :3855–3869` | ✅ |
| チェッカーボード背景 | `:2309 drawCheckerboard` | ✅ |
| マスク編集（プロポーショナルドラッグ） | `:1038–1208` | ✅ |
| シェイプ編集（頂点/セグメント/角丸/星内半径/ベジェ） | `:1363–1948` | ✅ |
| トランスフォーム gizmo + XYWH HUD | `:1474–1652` | ✅ |
| ターゲットレイヤー tint（チャンネル疑似カラー） | `:3791–3795` | ✅ 最小 |
| `EditMode` / `DisplayMode` 切替 | `:3871–3934` | ✅ |
| `resetView()` / `fitToViewport()` / `setPan()` / `zoomAroundPoint()` | `:3837–3869` | ✅ |
| スクリーンショット | `grabScreenShot()` | ✅ |

### 1.3 致命的欠陥
1. **検査/比較/状態表示の薄さ**: 編集コアはあるが、合成ビューポート（`CompositionRenderController`）が持つ下記を**まだ共有していない**。
   - チャンネル分離表示（RGBA/法線/速度/オブジェクトID/アルベド/PBR 分離）
   - カスタマイズ HUD（fps/zoom/名前/解像度スケール）
   - グリッド / セーフエリア / ガイド線
   - ROI / サンプルポイント / Zebra（露出） / バッファ可視化
   - 比較・ワイプ（リファレンス vs 現在）
   - オニオンスキン / アイソレーション / X-Ray / アンカー中心 / カメラフラスタム / モーションパス / 密度ヒートマップ / カラーサンプラー
2. **`LayerPreviewPipeline` が orphan の空スタブ**（`Artifact/src/Preview/ArtifactLayerPreviewPipeline.cppm`）：`Impl` が空、`grep` では自身の定義以外から**一切参照されていない**。一方、実際の単層 GPU パイプラインは `RenderPipeline`（`ArtifactRenderLayerPipeline.ixx`：layerSRV/UAV/RTV/FloatSRV/FloatUAV + normal/velocity/objectId/materialId/albedo ターゲット）に実装済み。つまり「レイヤー用プレビュー基盤」は**死コードのまま放置**され、実パイプラインは別系統で存在する二重構造。
3. **`DisplayMode` と合成側の可視化軸が乖離**（`Tool.ixx:18`）：`Color/Alpha/Mask/Wireframe` のみで、合成側の `ViewportChannelDisplayMode` や HUD/overlay 密度に追従していない。
4. **`Inspect / Impact / Compare` の導線不足**: 設計上の主役だが、状態要約と比較表示がまだ薄い。
5. **UI 統合がテスト枠**（`AppMain.cppm:2269` の standalone ウィンドウ）。コンポジションエディタのドック内パネルとしての第一級統合なし。

### 1.4 遅れの根本原因
> レイヤービューは pan/zoom/checkerboard/gizmo/mask/shape を**自前再実装**したが、合成コントローラの表示/オーバーレイ/デバッグ基盤を**共有せず並走**したため、検査機能一式が取り残された。これを「機能を一つずつ再実装する」のは非効率。共有基盤（§2）へ乗せるのが正解。

---

## 2. 更新方針：共有基盤への乗せ替え

`MILESTONE_VIEWPORT_ENHANCEMENT_PROPOSAL` の §12 が定義する 3 基盤をレイヤービューにも適用し、合成ビューポートの検査機能を**継承**する。

| 基盤 | 適用 | 得られるもの |
|---|---|---|
| `ViewportState` | `ArtifactLayerEditorWidgetV2::Impl` が保持 | 回転/解像度スケール/DPR を統一管理（M-VP-4/5 互換） |
| `ViewportOverlayCompositor` | 同 widget が所有 | HUD/ROI/サンプル点/ガイド/セーフエリア/ゼブラを登録のみで表示 |
| `DisplayFilterSet` | 同 widget が所有 | チャンネル分離・バッファ可視化・Zebra を合成と同一操作で |

**変更箇所**: `renderOneFrame()`（`cppm:2289`）のレイヤー+チェッカーボード描画の**直後**に `compositor_.drawAll(*renderer_, state_)` を追加するだけで、上記全機能がレイヤービューに出現。

---

## 3. 具体的変更（コードレベル）

### 3.1 `LayerPreviewPipeline` 空スタブの処理
- **選択 A（推奨）**: 削除済み。`ArtifactLayerEditorWidgetV2` は既存 `RenderPipeline`（`ArtifactRenderLayerPipeline`）の `layerSRV()/normalSRV()/velocitySRV()/objectIdSRV()/albedoSRV()` 等を `readbackChannelToImage`（`ArtifactIRenderer.ixx:150`）経由で直接利用すれば、単層マルチチャンネル検査が得られる。
- **選択 B**: 残す場合は `RenderPipeline` への薄いアダプタとして実体化（`renderLayer(layer, frame) → layerSRV()`）。重複実装は避ける。

### 3.2 `DisplayMode` enum の整理（`Tool.ixx:18`）
- レイヤービュー側は `DisplayFilterSet` に委譲し、`DisplayMode` は `View/Transform/Mask/Shape`（編集モード）と表示モードを分離。表示は `DisplayFilterId`（提案書 §12.4）で統一。

### 3.3 `ArtifactLayerEditorWidgetV2::Impl` へのメンバ追加
```cpp
ViewportState state_;                              // §12.2
ViewportOverlayCompositor compositor_;            // §12.3
DisplayFilterSet displayFilters_;                 // §12.4
// renderOneFrame() 末尾:
displayFilters_.applyTo(*renderer_);
compositor_.drawAll(*renderer_, state_);
```

### 3.4 レイヤービュー固有の付加価値（合成にはない）
| 機能 | 理由 |
|---|---|
| **マスク密度/カバレッジ ヒートマップ** | 単層なのでマスクの塗りむらを即確認。OverlayLayer で実装 |
| **3D マテリアル プレビュー Primitive** | 3D レイヤー選択時に球/立方体/円柱/トーラスへ切替（Substance 系要求） |
| **プロジェクト vs 単層 ワイプ** | 合成結果と単層をビューポート内で比較（B-1 Wipe のレイヤー版） |
| **アンカー/ピボット中心オーバーレイ** | 単層編集では必須。合成側 `setShowAnchorCenterOverlay` 相当 |
| **パーシャル チャンネル for single layer** | 単層の normal/velocity/objectId を直接表示（§3.1 の readback 利用） |

### 3.5 UI 統合（テスト枠 → ドックパネル）
- `AppMain.cppm:2269` の standalone `ArtifactSoftwareLayerTestWidget` を、コンポジションエディタのドック内 `ArtifactRenderLayerEditor` として第一級統合。Inspector の「単層プレビュー」ボタンから起動。

---

## 4. 影響・リスク

- **循環参照**: レイヤービューが `Widget.Render.ViewportOverlay` / `Widget.Render.DisplayFilter` を `import` する。これらは `ArtifactIRenderer&` のみに依存し、レイヤービューへの逆依存は作らない（§12 の設計通り）。
- **`QPainter`/QtCSS/`QColorDialog` 不使用**: 全描画は `ArtifactIRenderer` プリミティブで。
- **CRLF 維持**: `ArtifactRenderLayerWidgetv2.cppm` 編集は `edit` ツール使用。
- **子リポジトリ非変更**: `ArtifactCore` 側は `RenderPipeline`（既存）を流用のみ。新規モジュールは `Artifact` 側。
- **死コード削除**: `LayerPreviewPipeline` 削除は CMakeLists の登録も確認（GLOB のため自動発見だが、参照がないことを `grep` で確認済み）。

---

## 5. マイルストーン分割

| Wave | 内容 | 依存 | 完了条件 |
|---|---|---|---|
| **LW-0 ✅** | §3.1 `LayerPreviewPipeline` 削除 or `RenderPipeline` アダプタ化 | なし | 死スタブが残らず、単層プレビューの参照経路が一つに揃う |
| **LW-1** | ViewportState / compositor / DisplayFilter を `ArtifactLayerEditorWidgetV2` に導入、`renderOneFrame` 末尾で drawAll | 提案書 W0 | 既存のレイヤー描画の後段に共有オーバーレイが一括で差し込まれる |
| **LW-2** | HUD/ROI/サンプル点/ガイド/セーフエリア/ゼブラ OverlayLayer 登録 | LW-1 | 検査系の表示を登録ベースで増やせる |
| **LW-3** | レイヤー固有: マスク密度ヒートマップ / 3D マテリアルプリミティブ / プロジェクトvs単層ワイプ / アンカーオーバーレイ | LW-1 | 単層ならではの比較・解析が合成エディタと独立に出せる |
| **LW-4** | UI 統合: ドックパネル化 | LW-1–3 | Standalone 枠を第一級導線へ寄せられる |

---

## 6. 未確認事項

1. `LayerPreviewPipeline` は削除済み。必要になった場合のみ `RenderPipeline` アダプタを再導入するか。
2. レイヤービューをドックパネル化する際、どのドック（Inspector 横 / 独立タブ）に配置するか。
3. 3D マテリアル プレビュー Primitive は M-3D 系マイルストンと重複する可能性あり → 優先度調整要。

---

## 8. 実行順

このマイルストーンは、次の順で進めると迷いにくい。

1. `ViewportState` を合成側と同じ責務境界に寄せる
2. `ViewportOverlayCompositor` をレイヤービューに接続する
3. `DisplayFilterSet` を既存 `DisplayMode` と分離して導入する
4. `LW-2` の HUD / ROI / ゼブラ系を足す
5. `LW-4` のドック統合を最後に行う

`LW-0` は完了済みなので、次の実作業は `LW-1` から入る。

### LW-1 の着手点

1. `ArtifactLayerEditorWidgetV2::Impl` に `ViewportState` を保持させる
2. `ViewportOverlayCompositor` を `renderOneFrame()` 末尾に挿す
3. `DisplayFilterSet` を `DisplayMode` から切り離して導入する
4. 既存の `renderOneFrame` のレイヤー描画順は崩さず、後段の共有基盤だけ足す

### LW-1 完了条件

- 既存の単層描画の後ろに共有オーバーレイが一括で出る
- 表示モードが編集モードと混ざらない
- レイヤービューの 3D 表示が合成側の検査基盤に寄る

### LW-2 の着手点

1. `HUDConfig` を layer view でも共通化する
2. `ROI` を単層確認に必要な最小表示として追加する
3. `Sample Points` を viewer 上で追えるようにする
4. `Zebra` は最後に置き、露出確認の補助として足す

### LW-2 完了条件

- 単層の状態を HUD で短く読める
- ROI を視覚的に切れる
- サンプル点と zebra で検査の粒度が上がる

### LW-3 への前提

- 比較やワイプを足す前に、単層確認の基礎が共有されている
- 3D マテリアルプレビューやワイプは、検査基盤が出てから追加する

---

## 7. 参照

- `docs/planned/MILESTONE_VIEWPORT_ENHANCEMENT_PROPOSAL_2026-07-08.md`（§12 共有基盤）
- `docs/planned/MILESTONE_COMPOSITION_EDITOR_LAYER_VIEW.md`（旧 Layer View マイルストン・2026-03-21、現状は古い）
- `Artifact/include/Render/ArtifactRenderLayerPipeline.ixx`（実パイプライン）
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`（実装済み編集機能）
