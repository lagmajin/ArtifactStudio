# ニューラルテクスチャ圧縮 (NTC) 導入可能性調査

- 日付: 2026-07-27
- 種別: 技術調査 / 導入可否分析
- 対象: NVIDIA RTX Neural Texture Compression SDK (RTXNTC) v0.9.2 BETA
- 関連: `libs/DiligentEngine`, `ArtifactCore/include/Image/ImageF32x4_RGBA.ixx`, `vcpkg.json`, ルート `CMakeLists.txt`

## 結論

**技術的には導入可能。ただし現実的なのは「Vulkan バックエンド + NTC-on-load（BCn トランスコード）」構成のみ。**
本命の on-sample（VRAM 最小化）は SDK / ドライバ / DirectX 側がプレビュー段階であり時期尚早。
現在の開発優先方針（静止画 / 連番 / シェイプ / 合成 / 3D の完成度優先、2026-07-27）を踏まえると**今すぐの導入は非推奨**。SDK 自体も BETA のため急ぐ理由がない。

## NTC とは

- PBR マテリアルのテクスチャ群（albedo / normal / roughness / metalness / AO など最大 16ch）を
  1 つの NTC テクスチャセットとして小型 MLP + latent データに圧縮する方式。
- 圧縮率の目安: 64 bits/texel 相当の素材バンドル → 約 5 bits/texel（PSNR 40〜50dB、BCn 同等品質）。
  - 2k×2k バンドル例: Raw 32MB / BCn 12MB / NTC-on-load 2.5MB(ディスク・転送) / NTC-on-sample 2.5MB(VRAM 含む)。
- デコードはテクスチャ座標ごとの latent 読み出し + MLP 推論。Cooperative Vector 拡張
  （Vulkan / D3D12）で 2〜4 倍高速化。DP4a / 整数演算フォールバックあり（SM6 以上なら全ベンダーで動作）。

## RTXNTC SDK v0.9.2 BETA の要点（2026-07 時点）

| 項目 | 状況 |
| --- | --- |
| NTC-on-load | ロード時に展開して BCn へトランスコード。SM6 GPU なら全ベンダーで動作・**出荷可**（GTX1000 / RX6000 / Arc A で検証済み） |
| NTC-on-sample | サンプル時 MLP 推論。実用には CoopVec 必須級。**DX12 版 CoopVec は preview Agility SDK (1.717.x-preview) + 開発者モード + プレビュードライバ (590.26+) 依存で「DO NOT SHIP」明記**。**Vulkan 版 CoopVec は出荷可**（NVIDIA driver 570+） |
| 圧縮側 | **NVIDIA Turing 以上 + CUDA 12.9 が必須**（LibNTC の圧縮は CUDA 実装）。AMD / Intel 環境では圧縮不可（展開のみ可） |
| 統合手段 | LibNTC（C++、D3D12 / Vulkan 1.3 直接対応）+ `ntc-cli`（スクリプトパイプライン用）。GPU BCn エンコーダも同梱 |
| ライセンス | NVIDIA RTX SDKs LICENSE（再配布条件の詳細は**未検証**） |
| 既知の問題 | Intel Arc B 系での FP8 CoopVec 不具合、AMD での Inference-on-Feedback 不具合など |

## アプリ側の現状（コード調査済みの事実）

### 好材料

- Vulkan バックエンド有効（ルート `CMakeLists.txt` の `DILIGENT_NO_VULKAN OFF`）。
  LibNTC は Vulkan 1.3 でフル機能（CoopVec 含む）が出荷可能なので、**DX12 を有効化しなくても導入路がある**。
- ONNX Runtime 1.23.2 + DirectML が既に依存に存在（`vcpkg.json`、`ARTIFACT_HAS_ONNX_DML_BACKEND`）。
  ただし NTC は LibNTC 自前実装のため直接は使わない。
- NRD / llama.cpp など NVIDIA-RTX 系・AI 系サブモジュール運用の実績あり（`.gitmodules`）。

### 障壁

- **DX12 バックエンドは安定性理由で無効**（`DILIGENT_NO_DIRECT3D12 ON`）。
  ただし DX12 CoopVec はどのみち出荷不可なので当面は問題にならない。
- **BCn / DDS / KTX 等の圧縮テクスチャローダーが未実装**。
  NTC-on-load の出力先（BCn テクスチャ）を受ける基盤から作る必要がある。
- 内部画像表現が `ImageF32x4_RGBA`（F32 BGRA、16 bytes/px、OpenCV CV_32FC4）中心で、
  NTC の想定（8bit 級 PBR 素材）とは品質哲学が異なる。**2D 合成の本流パス（F32 リニア合成）には品質的に不適合**。
- DiligentEngine fork は v2.5.6 系で CoopVec を抽象化していない。
  on-sample をやるなら Vulkan ネイティブハンドル経由の interop が必要（工数大）。

## 適用先として現実的な範囲

1. **3D レイヤーの PBR マテリアルアセット** — NTC の設計対象そのもの。アセットライブラリのディスク / VRAM 削減に効く。
2. プロジェクト保存物・素材キャッシュのフットプリント削減（NTC-on-load でロード時 BCn 化）。
3. 静止画・連番画像レイヤーの編集本流（F32 リニア合成パス）には**不適合**（対象外とする）。

## 推奨ロードマップ

- **段階 0（前提整備・NTC 抜きでも価値あり）**:
  BCn / KTX2 圧縮テクスチャのロードと GPU アップロード対応。
  これだけで 3D レイヤーの VRAM を 1/4〜1/8 にでき、将来の NTC-on-load の受け皿になる。
- **段階 1**: 3D マテリアルアセット限定で LibNTC (Vulkan) による NTC-on-load → BCn トランスコード。
  圧縮は `ntc-cli` ベースのオフライン工程とし、NVIDIA GPU 非搭載環境では圧縮機能を無効化する。
- **段階 2（将来）**: SM6.9 / Agility SDK の正式化と Diligent 側 CoopVec 対応を待って on-sample を検討。

## 未検証事項

- NVIDIA RTX SDKs LICENSE の再配布条件の詳細（アプリ同梱可否の法務確認）。
- LibNTC と Diligent Vulkan デバイスの共存（同一 VkDevice / 別デバイスでの動作）— 実機検証が必要。
- DiligentEngine 将来バージョンでの CoopVec 抽象化の有無。

## 参照

- RTXNTC SDK: https://github.com/NVIDIA-RTX/RTXNTC
- D3D12 Cooperative Vector: https://devblogs.microsoft.com/directx/cooperative-vector/
- NVIDIA RTX Kit / Neural Rendering: https://developer.nvidia.com/blog/get-started-with-neural-rendering-using-nvidia-rtx-kit/
