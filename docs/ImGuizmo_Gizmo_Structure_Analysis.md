# ImGuizmoギズモ構造分析レポート

## 概要

ImGuizmoライブラリ（Dear ImGui拡張）の3Dトランスフォームギズモの形状構造を分析し、ArtifactStudioのレンダリング基盤への移植可能性を調査した。

## ImGuizmoの構造

### リポジトリ構造
- **ImGuizmo.h/ImGuizmo.cpp**: メインの実装（3Dギズモ操作）
- **GraphEditor.h/cpp**: ノードベースのグラフエディター
- **ImCurveEdit.h/cpp**: アニメーションカーブエディター
- **ImGradient.h/cpp**: グラデーションエディター
- **ImSequencer.h/cpp**: タイムラインシーケンサー
- **ImZoomSlider.h**: ズームコントロール
- **ImLightRig.h**: ライティングリグ

### ギズモの描画関数

ImGuizmo.cpp内の主要な描画関数：

#### DrawTranslationGizmo(OPERATION op, int type)
- 移動ギズモの描画
- 軸ごとの矢印：AddLine + AddTriangle
- 平面ハンドル：AddQuadFilled
- 軸の色分けと選択ハイライト

#### DrawRotationGizmo(OPERATION op, int type)
- 回転ギズモの描画
- 軸ごとの円弧：AddCircle
- 回転リング：AddCircle
- 軸ごとの色分け

#### DrawScaleGizmo(OPERATION op, int type)
- スケールギズモの描画
- 軸ごとの線：AddLine
- 終端ボックス：AddRectFilled

#### DrawScaleUniveralGizmo(OPERATION op, int type)
- ユニバーサルスケールギズモ
- 軸ごとの円と線

#### DrawHatchedAxis(const vec_t& axis)
- 軸のハッチング描画
- 軸の方向に平行線を描画

### 描画プリミティブ

ImGuizmoは以下のImGui描画プリミティブを使用：
```cpp
gContext.mDrawList->AddLine(start, end, color, thickness);
gContext.mDrawList->AddCircle(center, radius, color);
gContext.mDrawList->AddRectFilled(min, max, color);
gContext.mDrawList->AddQuadFilled(p1, p2, p3, p4, color);
gContext.mDrawList->AddTriangleFilled(p1, p2, p3, color);
```

## ArtifactStudioへの移植分析

### 利点
- 数学部分（マトリックス変換、ベクター計算）はそのまま使用可能
- ギズモの操作ロジックは独立している
- スタイル設定（色、サイズ）はカスタマイズ可能

### 移植手順
1. ImGuizmo.cppの描画関数をコピー
2. ImDrawList呼び出しをArtifactStudioの描画APIに置き換え
   - `AddLine` → `DrawLine`
   - `AddCircle` → `DrawCircle`
   - etc.
3. コンテキスト管理（gContext）を移植
4. 数学ユーティリティクラスを統合

### 考慮点
- ImGuiコンテキスト依存を排除
- 描画順序と深度管理
- 透視投影とスクリーン座標変換
- パフォーマンス最適化（描画呼び出しの削減）

## 結論

ImGuizmoのギズモ形状は比較的独立した構造であり、ImGuiの描画プリミティブをArtifactStudioのレンダリングAPIに置き換えることで移植可能。数学部分と操作ロジックは再利用できるため、実装コストは中程度と評価される。

## 参考情報
- ImGuizmoリポジトリ: https://github.com/CedricGuillemet/ImGuizmo
- 調査日: 2026-04-09
- バージョン: v1.92.5 WIP