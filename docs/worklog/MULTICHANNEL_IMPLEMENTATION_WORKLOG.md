# マルチチャンネルレンダラー 実装作業ログ

作成日: 2026-04-18

---

## ✅ 実施済み作業

| 日時 | 作業内容 | ステータス | 備考 |
|------|----------|-----------|------|
| 2026-04-18 | `ArtifactIRenderer.ixx` にマルチチャンネル制御用公開インターフェース追加 | ✅ 完了 | |
| 2026-04-18 | `ChannelType` 列挙型をヘッダー内にローカル定義 | ✅ 完了 | Core.Channel モジュールの循環参照回避のため |
| 2026-04-18 | `ArtifactIRenderer.cppm` にマルチチャンネル状態フラグ実装 | ✅ 完了 | `setMultiChannelEnabled` / `setChannelEnabled` / `isChannelEnabled` |
| 2026-04-18 | `ArtifactCore` に `MultiChannelImage` コンテナ追加 | ✅ 完了 | RGBA + 任意チャンネルの基本コンテナ |
| 2026-04-18 | `VideoFrame` に const `getChannel()` 追加 | ✅ 完了 | `MultiChannelImage` との相互変換補助 |
| 2026-04-18 | renderer に深度バッファ添付を追加 | ✅ 完了 | swapchain / headless の depth view を確保 |
| 2026-04-18 | depth readback API を追加 | ✅ 完了 | `readbackDepthToImage()` |
| 2026-04-18 | `MultiChannelImage` へのまとめ入口を追加 | ✅ 完了 | `readbackToMultiChannelImage()` |
| 2026-04-18 | コンパイルエラー修正 | ✅ 完了 | |

### 実装済みインターフェース

```cpp
// ==== マルチチャンネルレンダリング ====
void setMultiChannelEnabled(bool enabled);
bool isMultiChannelEnabled() const;

enum class ChannelType {
    Red, Green, Blue, Alpha,
    Depth,
    NormalX, NormalY, NormalZ,
    VelocityX, VelocityY,
    ObjectId, MaterialId,
    Emission,
    Custom
};

void setChannelEnabled(ChannelType channel, bool enabled);
bool isChannelEnabled(ChannelType channel) const;
```

---

## 📋 今後の実装予定

### Phase 1: インターフェース実装
- [x] `ArtifactIRenderer.cppm` 側にメソッド実装追加
- [x] チャンネル有効フラグメンバ変数追加
- [x] レンダーターゲットの depth view 初期化を追加
- [x] depth readback を追加
- [x] `MultiChannelImage` にまとめる入口を追加
- [ ] レンダーターゲット MRT 初期化処理追加
  - まだ RGBA 固定の描画経路で、MRT への実出力は未接続

### Phase 2: チャンネル別描画
- [ ] 深度バッファ書き出し
- [ ] 法線バッファ書き出し
- [ ] モーションベクトル書き出し
- [ ] オブジェクトID 書き出し

### Phase 3: パイプライン統合
- [ ] エフェクトシステムのマルチチャンネル対応
- [ ] ブレンドパイプラインのチャンネル別処理
- [ ] チャンネルビューアUI

---

## 💡 技術的なメモ

### 循環参照問題
`Artifact` モジュールが `Core.Channel` をインポートすると循環参照が発生するため、当面はヘッダー内に `ChannelType` を複製定義する方式を採用。
実装内部でのみ `ArtifactCore::ChannelType` とのマッピングを行う。

### 互換性
- 既存コードへの影響は一切なし
- デフォルトではマルチチャンネル無効状態で従来通り動作
- 必要な場合にのみ個別チャンネルを有効化可能

---

## 🔮 今後作成予定のマイルストーン

1.  **レイヤーコンポーネントシステム**
    - 既に基盤実装済み。Unityライクなコンポーネントアーキテクチャ
    - レイヤーへの機能追加がプラグイン的に可能になる

2.  **リアルタイム物理エンジン統合**
    - Box2D / Bullet の統合計画
    - モーショングラフィックス向け物理シミュレーション

3.  **ノードベースエフェクトエディタ**
    - 既存のエフェクトスタックの後継
    - Nuke / Blender ライクなノードグラフ

4.  **タイムラインカーブエディタ書き換え**
    - 現在の `QGraphicsScene` 実装の置き換え
    - オーナードロー方式でパフォーマンス10倍向上

5.  **ネイティブUSDサポート**
    - 3Dシーンのインポート/エクスポート
    - Hydra レンダラー統合
