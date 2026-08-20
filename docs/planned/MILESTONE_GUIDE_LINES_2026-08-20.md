# M-GUIDE-1 誘導ガイドライン拡張

**最終更新:** 2026-08-20
ステータス: Planned

## 1. 目的

コンポジション上のレイアウト誘導用ラインを、レンダー素材の`LineLayer`ではなく、既存のSmart Guides／Viewport Overlay／Transform Gizmoのスナップ経路を使うエディタ補助オブジェクトとして完成させる。

対象は、静止画・平面・シェイプ・テキストの配置を補助するガイドであり、最終レンダーには出力しない。

## 2. 既存基盤

- `Artifact.Core.SmartGuidesManager`
- `GuideDefinition`／`GuideSet`
- `Artifact.Widgets.Render.ViewportOverlay::drawGuides`
- `CompositionRenderController::setShowGuides`
- `CompositionRenderController::setSnapToGuides`
- `TransformGizmo` のガイドスナップ経路

新しい`LineLayer`クラスや動画依存の描画経路は追加しない。

## 3. スコープ

### P0: 垂直／水平ガイド

- 垂直ガイドの追加・削除
- 水平ガイドの追加・削除
- ビューポート上の表示／非表示
- ガイドのドラッグ移動
- ガイドのロック
- 既存レイヤーの境界・中心・コンポジション境界へのスナップ
- Undo／Redo

### P1: 永続化と編集性

- コンポジション／プロジェクト保存へのガイド情報の含有
- 再読込時の復元
- ガイド一覧の選択・複数選択
- 色・表示強度・ラベル
- ガイド専用の表示設定

### P2: カスタムライン

- 2点で定義する斜めライン
- 端点ハンドル編集
- ライン方向・交点へのスナップ
- 一時ガイドと保存ガイドの切り替え
- 三分割・対角線・セーフマージン等のプリセット

## 4. 非スコープ

- 最終レンダーへのガイド出力
- 通常のShape／Strokeレイヤーとしてのアニメーション
- Graph Editor／Dope Sheet対応
- 動画レイヤーや音声との同期
- パーティクル・物理シミュレーションとの連携

## 5. 完了条件

- 垂直／水平ガイドを追加・移動・削除できる
- ガイド表示をViewportで切り替えられる
- Transform Gizmo操作時にガイドへスナップできる
- ガイドはレンダー結果に含まれない
- Undo／Redoが機能する
- 保存／再読込でガイド状態が復元される
- 既存のSmart Guides／Viewport Overlay経路を再利用し、`QImage`、QtCSS、新規signal/slotを追加しない

## 6. 実装順序

1. 既存`GuideDefinition`／`GuideSet`の保存・編集責務を確認
2. 垂直／水平ガイドの追加・削除APIを既存サービスへ追加
3. Viewport Overlayへの表示と選択・ドラッグ編集を追加
4. Transform Gizmoのスナップ入力へカスタムガイドを統合
5. プロジェクト保存／再読込とUndo／Redoを接続
6. 2点カスタムラインとプリセットをP2として追加

## 7. リスク

- ガイド表示とレイヤー選択枠を混同しないこと
- ガイドをレンダー対象のレイヤーとして扱わないこと
- 既存のSmart Guidesとカスタムガイドの重複スナップを避けること
- プロジェクトJSONの責務をWidget側へ漏らさないこと
