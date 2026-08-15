# マイルストーン: マルチチャンネルレンダーパイプライン (AE互換)

**最終更新:** 2026-08-15

作成日: 2026-04-18
優先度: 🟠 高
対象バージョン: M13

## 現行コード監査 (2026-08-15)

`ArtifactIRenderer` の channel enable/readback API と `MultiChannelImage` への取得経路は実装されている。Composition Editor には Depth、Normal X/Y/Z、Object ID などの表示・書き出し導線があり、multi-channel EXR を含む保存案内も存在する。3D viewer 側では depth buffer と normal texture の生成・利用も確認できる。

ただし、これは「チャンネルを保持して表示・書き出しできる」基盤であり、目標の全パイプライン伝搬を意味しない。2D レイヤーの深度値・object/material ID の一貫した自動生成、velocity の実レンダリング、エフェクトとブレンドでの全チャンネル伝搬、チャンネル別出力の runtime parity は現行コードから確認できない。UI に表示モードがあっても、実データが全バックエンド・全レイヤーで埋まることは別途検証が必要である。

**判定:** チャンネル型・renderer 制御・readback/export・一部 3D depth/normal は実装済み。2D/3D の統一生成、velocity/object/material ID の実運用、effect/composite propagation、受入れテストは pending。

## Update 2026-08-15

- `ArtifactIRenderer` の channel enable／readback、`MultiChannelImage`、Render Queue／Screenshot／Render Output の multi-channel EXR 導線を再確認。
- Composition 側には depth／normal／velocity／albedo／ID の channel source、AOV remap、ping-pong view 保持、viewport channel display の経路がある。GPU backend 必須や未生成 AOV の診断も実装されている。
- ただし velocity は現状 3D object／camera motion が中心で 2D layer vectors は未対応。Object／Material ID は draft の packed／single-hit coverage、全レイヤーの自動生成・effect／blend 伝搬・runtime parity は未完了。

---

## 概要

After Effects 互換のマルチチャンネルレンダリングシステムの実装計画です。
RGBAに加え、深度、法線、速度、オブジェクトID、マテリアルID等の任意のチャンネルをレンダリングし、エフェクトパイプライン全体で伝搬させる事が可能になります。

**驚くべき事実:** マルチチャンネルの基盤は既に ArtifactCore に完全に実装済みです。

---

## ✅ 既に存在しているもの

### コアシステム (基盤実装済み)
```
✅ Channel 型定義システム
✅ VideoChannel 単一チャンネルコンテナ (Float32)
✅ VideoFrame マルチチャンネルフレームコンテナ
✅ MultiChannelImage 基本コンテナ
✅ 動的チャンネル追加/削除API
✅ 深度/法線/速度/オブジェクトID の型定義済
```

### 対応済みチャンネル一覧
| チャンネル名 | ステータス | 用途 |
|-----------|-----------|------|
| Red/Green/Blue/Alpha | ✅ | 標準カラー |
| Depth | ✅ | Zバッファ |
| Normal X/Y/Z | ✅ | 法線ベクトル |
| Velocity X/Y | ✅ | モーションベクトル |
| ObjectId | ✅ | オブジェクト選択マスク |
| MaterialId | ✅ | マテリアル別エフェクト |
| Emission | ✅ | 発光 |
| Custom | ✅ | ユーザー定義 |

---

## 🔄 現在のアーキテクチャ状況

### 現在のパイプライン (RGBA固定)
```
レイヤー描画 → RGBA バッファ → エフェクト適用 → ブレンド → 出力
```

### 目標のマルチチャンネルパイプライン
```
レイヤー描画 → [RGBA, Depth, Normal, Velocity, ObjectId]
                        ↓
                  エフェクト適用 (全チャンネル伝搬)
                        ↓
                  ブレンド (各チャンネル個別)
                        ↓
             出力 + チャンネル別エフェクト
```

---

## 📋 実装タスク

### Phase 1: レンダラー拡張
- [x] `ArtifactIRenderer` にチャンネル制御インターフェース追加
- [x] `swapchain` / `headless` の depth view 初期化
- [x] depth readback の入口を追加
- [x] `MultiChannelImage` へのまとめ入口を追加
- [ ] `PrimitiveRenderer3D` で深度/法線の書き出し対応
- [ ] 2D レイヤーの深度値生成
- [ ] モーションベクトルパスのレンダリング
- [ ] オブジェクトID の自動割り当て

### Phase 2: エフェクトパイプライン拡張
- [ ] `AbstractImageEffect` インターフェースを VideoFrame 対応に更新
- [ ] 既存エフェクトの後方互換性維持
- [ ] チャンネル選択UI
- [ ] 深度/法線を使った標準エフェクト実装
  - 深度ベースぼかし
  - 法線ベースライティング
  - オブジェクトID マスク

### Phase 3: ブレンド演算拡張
- [ ] 各チャンネル個別のブレンドモード
- [ ] 深度バッファテスト
- [ ] 法線空間ブレンド
- [ ] ベロシティバッファ合成

### Phase 4: ユーザーインターフェース
- [ ] チャンネルビューア切り替え
- [ ] レイヤー毎のチャンネル出力設定
- [ ] エフェクトでのチャンネルマッピングUI
- [ ] レンダーキューでのマルチチャンネル出力

---

## 💡 技術的特徴

### AEとの互換性
✅ OpenEXR マルチチャンネル出力と完全互換  
✅ 深度/ベロシティパスのフォーマットは AE/NUKE と完全一致  
✅ オブジェクトID は Cryptomatte 互換フォーマットに対応予定

### パフォーマンス
- 全チャンネル Float32 高精度
- GPU上でのチャンネル伝搬
- 不要なチャンネルは動的に省略可能
- メモリ使用量は要求チャンネル数に比例

---

## 🎯 これで可能になる機能

1.  **リアルタイム深度フィールド**
2.  **ポストプロセスモーションブラー**
3.  **法線マップを使ったライティングエフェクト**
4.  **オブジェクト別マスク**
5.  **マテリアル別カラーグレーディング**
6.  **3Dデータを使った全てのポストエフェクト**

---

## 📊 実装難易度

| 項目 | 難易度 | 備考 |
|------|--------|------|
| レンダラー拡張 | 🟢 簡単 | インターフェース追加のみ |
| エフェクトシステム | 🟡 中 | 後方互換性維持が必要 |
| ブレンド拡張 | 🟡 中 | 各チャンネルのブレンドロジック追加 |
| UI | 🟢 簡単 | 既存コンポーネントの流用可能 |

**合計工数: 約 10日**

---

## 🔑 最大の利点

コアシステムは100%完成しています。必要な作業は「既にあるChannelシステムをレンダーパイプラインに接続する」だけです。
他のDCCツールが何ヶ月もかけて開発する機能を、このプロジェクトでは数週間で実装可能です。

---

## 関連ファイル

- [`ArtifactCore/include/Channel/Channel.ixx`](ArtifactCore/include/Channel/Channel.ixx)
- [`Artifact/include/Render/ArtifactIRenderer.ixx`](Artifact/include/Render/ArtifactIRenderer.ixx)
- [`ArtifactCore/include/ImageProcessing/AbstractImageEffect.ixx`](ArtifactCore/include/ImageProcessing/AbstractImageEffect.ixx)

---

## Static audit follow-up (2026-07-25)

現行ソースでは `MultiChannelImage`、named channel の EXR export / async export、`ArtifactIRenderer` の multi-channel enable と GPU readback、Render Queue の AOV 選択・保存・preview、Screenshot / Render Output の Multi-channel EXR UI が実装されている。Depth / Normal / Velocity / ObjectID / MaterialID / Albedo / Emission の出力入口も確認でき、Phase 1 の初期項目と Phase 4 の出力導線は大きく進展している。

一方、2D レイヤーの全チャンネル生成、AbstractImageEffect の VideoFrame 対応、任意チャンネルのエフェクト伝搬、チャンネル別 blend、channel viewer、レイヤー単位 mapping UI、実際の全 AOV の生成品質は未確認である。文書の「コアシステム 100% 完成」は、レンダーパイプライン全体の完了を意味しないため、表現を部分実装として扱う。

### Audit status

- Phase 1: 部分実装 — renderer control / readback / named AOV export は実装済み相当。2D depth・全 velocity / ObjectID 生成は未確認
- Phase 2: 未完了 — channel-aware effect pipeline と標準 channel effect は未確認
- Phase 3: 未完了 — channel 個別 blend / depth test / normal-space blend は未確認
- Phase 4: 部分実装 — Render Queue / Screenshot の AOV UI はあるが、channel viewer・layer mapping は未確認
- Definition of Done: マルチチャンネル export 基盤は導入済み、汎用 pipeline 統合は未完了
