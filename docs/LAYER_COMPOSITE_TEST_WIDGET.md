# レイヤーコンポジットテストウィジェット

## 概要

レイヤーの透明度（Opacity）、ブレンドモード（Blend Mode）、可視状態（Visible）をテストするための専用ウィジェットです。

## 起動方法

**Test メニュー** → **Layer Composite Test...** を選択

またはコードから直接インスタンス化：

```cpp
auto* w = new ArtifactLayerCompositeTestWidget();
w->setAttribute(Qt::WA_DeleteOnClose, true);
w->show();
```

## 機能

### テスト項目

1. **透明度（Opacity）テスト**
   - 各レイヤーの不透明度を 0.0〜1.0 で調整可能
   - 0.05 単位で微調整
   - リアルタイムで合成結果を確認可能

2. **ブレンドモードテスト**
   - 13 種類のブレンドモードをテスト可能：
     - Normal
     - Add
     - Multiply
     - Screen
     - Overlay
     - Darken
     - Lighten
     - Color Dodge
     - Color Burn
     - Hard Light
     - Soft Light
     - Difference
     - Exclusion

3. **可視状態（Visible）テスト**
   - チェックボックスで各レイヤーの表示/非表示を切り替え
   - 非表示レイヤーは合成から除外

### テストパターン

4 層のテストパターンを自動生成：
- **Layer 1**: 赤色ベース
- **Layer 2**: 緑色ベース
- **Layer 3**: 青色ベース
- **Layer 4**: 黄色ベース

各レイヤーはグリッドパターンとグラデーションで視認性を確保。

### アニメーション機能

**Start Animation** ボタン（または `Space` キー）で、不透明度を自動アニメーション：
- 各レイヤーの不透明度が正弦波で変化
- レイヤー間で位相をずらして複雑な合成変化を再現

### キーボードショートカット

| キー | 機能 |
|------|------|
| `F5` | 手動リフレッシュ |
| `Space` | アニメーション開始/停止 |

## 画面構成

```
┌─────────────────────────────────────────────────────────┐
│  Layer Composite Test                                   │
├──────────────────┬──────────────────────────────────────┤
│  Common Settings │  Composite Preview                   │
│  ├─ Backend      │                                      │
│  ├─ Auto Refresh │  ┌────────────────────────────────┐ │
│  ├─ Refresh      │  │                                │ │
│  └─ Animation    │  │    合成結果プレビュー           │ │
│                  │  │                                │ │
│  Layer 1         │  │    (チェッカーボード背景)       │ │
│  ├─ Visible      │  │                                │ │
│  ├─ Opacity      │  └────────────────────────────────┘ │
│  └─ Blend Mode   │                                      │
│                  │                                      │
│  Layer 2         │                                      │
│  Layer 3         │                                      │
│  Layer 4         │                                      │
└──────────────────┴──────────────────────────────────────┘
```

## 使用例

### 基本的なテスト手順

1. **Layer 1** を設定
   - Visible: ON
   - Opacity: 1.0
   - Blend Mode: Normal

2. **Layer 2** を追加
   - Visible: ON
   - Opacity: 0.5
   - Blend Mode: Screen

3. 結果をプレビューで確認

4. 各パラメータを変更して、合成結果の変化を観察

### ブレンドモード比較

1. 全レイヤーの Opacity を 1.0 に設定
2. 各レイヤーの Blend Mode を順に変更
3. 違いを視覚的に確認

### 透明度アニメーション

1. **Start Animation** をクリック
2. 不透明度の自動変化を確認
3. 複雑な合成変化のデバッグに有用

## 実装詳細

### ファイル構成

| ファイル | 説明 |
|---------|------|
| `ArtifactLayerCompositeTestWidget.ixx` | ヘッダーファイル |
| `ArtifactLayerCompositeTestWidget.cppm` | 実装ファイル |
| `ArtifactTestMenu.cppm` | メニュー統合 |

### 合成エンジン

- **Backend**: Qt Painter / OpenCV（切り替え可能）
- **ブレンドモード**: `ArtifactCore::BlendMode` を使用
- **不透明度**: アルファチャンネル変換で適用

## 拡張候補

### 将来の機能

- [ ] レイヤー数の増加（現在は 4 層）
- [ ] カスタム画像の読み込み
- [ ] レイヤー位置オフセットの調整
- [ ] スケール・回転のテスト
- [ ] マスクテスト
- [ ] クリップテスト
- [ ] 結果画像のエクスポート
- [ ] テストケースの保存/読み込み

## 関連ドキュメント

- [TIMELINE_KEYBOARD_SHORTCUTS.md](./TIMELINE_KEYBOARD_SHORTCUTS.md) - タイムラインショートカット
- [LAYER_PANEL_DRAG_IMPROVEMENTS.md](./LAYER_PANEL_DRAG_IMPROVEMENTS.md) - レイヤードラッグ改善
