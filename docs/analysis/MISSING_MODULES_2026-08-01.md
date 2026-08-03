# 不在モジュール一覧 — 完全に存在しない or 空のスタブ

**日付**: 2026-08-01
**定義**: 「不在」= ソースに1行も実装がない / 0バイト / 空名前空間 / 機能として存在しない

---

## A. 空スタブ・0バイト・即死コード（Compilerには通るが機能ゼロ）

| ファイル | 状態 |
|----------|------|
| `ArtifactRenderer/src/ExternalFrameRenderer.cpp` | acceptが"diagnostic"のみ。全backend fallbackがプレースホルダ |
| `ArtifactRenderController.cppm` | 空の `namespace ArtifactCore {}` のみ。実装ゼロ |
| `GeneratorManager.cppm` | 空の `namespace {}` のみ |
| `ArtifactWidgets/src/Graphics/NodeWireGraphicItem.cpp` | 0バイト |
| `ArtifactWidgets/src/Graphics/BackendSettingWidget.cpp` | 0バイト |
| `SimpleSpline` (CloneGenerator) | `getPoint()` が固定 `{0,0,0}` / `pointCount()` が固定 `0` |
| DSL `CommandNode::compile()` | 全メソッド `nullptr` 返却（QueryNode は部分修正済み） |
| `MetadataVectorizer` | 入力を無視しハードコード固定値を返す |
| `BatchStabilizer` | I/O 完全スキップ、progress emit だけして `return true` |

## B. 完全に存在しないモジュール・サブシステム

### コンポジット
| 不在モジュール | 他アプリでの存在 | 必要性 |
|---------------|-----------------|--------|
| **OCIO v2 実ライブラリ統合** | AE/Nuke/Houdini/UE5 全て標準 | 🔴 致命的。自前3x3行列近似では実用不可 |
| **Deepコンポジット** | Nuke 標準。AE 一部 | 🟡 3DレンダリングAOVに必要 |
| **クリプトマット** | Nuke/UE5/AE | 🟡 マルチパスIDマスクの業界標準 |
| **EXRマルチパート** | Nuke/Houdini | 🟡 EXR 2.0 の標準機能 |
| **Alembic/USD フルサポート** | Maya/Houdini/UE5 | 🟡 3Dパイプライン連携 |
| **MaterialX** | Maya/Houdini/UE5 | 🟢 PBRマテリアル交換フォーマット |
| **ACES フルパイプライン** | 全アプリ | 🔴 RRT+ODT未実装（OCIO未統合が原因） |
| **ステンシルコンポジット** | Nuke/AE | 🟡 トラックマットより柔軟なマスク |

### キーイング・マスク
| 不在モジュール | 他アプリでの存在 | 必要性 |
|---------------|-----------------|--------|
| **IBK / Primatte級キーヤー** | Nuke 標準 | 🟡 ChromaKeyのみでは実用不足 |
| **LumaKeyer** | AE/Nuke 標準 | 🟡 明度ベースのキー |
| **DifferenceMatte** | AE/Nuke 標準 | 🟡 差分抽出 |
| **Rotobrush 2.0 / 3.0** | AE 独自 | 🟢 AIアシストロト。OpenCVRotoBrushEngine はあるが機能的か未確認 |
| **Content-Aware Fill** | AE 独自 | 🟢 動画の物体除去。AI連携の強みを活かせる |

### トラッキング
| 不在モジュール | 他アプリでの存在 | 必要性 |
|---------------|-----------------|--------|
| **プレーナートラッカー** | AE(Mocha)/Nuke | 🔴 コンポジットアプリの基本機能 |
| **オプティカルフロー** | AE/Nuke/Houdini | 🟡 時間補間・ワープに必須 |
| **フレーム補間** | AE(Flow)/Nuke | 🟡 スローモーション生成 |
| **ワープスタビライザーVFX** | AE 独自 | 🔴 既存コードありだがバグで機能せず。新規に作り直しが必要 |
| **3Dカメラソルバー** | AE/Nuke | 🟡 3D合成の基本 |

### アニメーション
| 不在モジュール | 他アプリでの存在 | 必要性 |
|---------------|-----------------|--------|
| **アニメーションレイヤー** | Maya/AE/UE5 | 🟡 ノンリニアアニメ編集 |
| **Wiggle / loopOut / smooth 式** | AE 標準 | 🟡 Expressionエンジンはあるがプリセット関数がない |
| **プロシージャルアニメプリセット** | Maya/Houdini | 🟢 Noise以外のパターンがない |
| **キーフレーム補助（Easy Ease/イーズイン/アウト）** | AE 標準 | 🟡 メニュー未実装。エンジンはある |
| **リグレイヤーUI** | Maya/Spine/Live2D | 🔴 設計書のみ。コードはRig2Dのデータモデルのみ |

### 3D
| 不在モジュール | 他アプリでの存在 | 必要性 |
|---------------|-----------------|--------|
| **サブディビジョンサーフェス** | Maya/Houdini/UE5 | 🟢 3Dモデリング用（2Dコンポジットアプリには不要か） |
| **スカルプティング** | Maya/Houdini/UE5 | 🟢 同上 |
| **CAD/NURBS** | Maya/Houdini | 🟢 同上 |
| **3D地面グリッド・グリッドフェード** | Maya/Blender/UE5 | 🟡 設計書あり未実装 |
| **3Dビューポートギズモ完全版** | Maya/Blender | 🟡 フレームギズモがバグあり |

### ワークフロー
| 不在モジュール | 他アプリでの存在 | 必要性 |
|---------------|-----------------|--------|
| **ワークスペース保存/読込** | AE/Nuke/Maya/UE5 全標準 | 🔴 DCCの基本機能 |
| **Collaborative editing（マルチユーザー）** | UE5 Multi-User | 🟢 Network/CollaborationWebSocket はあるがエディタ連携は不在 |
| **クラウドレンダー連携** | Nuke/UE5 | 🟢 ネットワークレンダリングすら未完 |
| **プロジェクトテンプレート** | AE/UE5 | 🟡 スターターはあるが本格的なテンプレート機能なし |
| **オーディオ波形のVP重畳表示** | AE標準 | 🟡 AudioWaveformクラスはあるがVPオーバーレイ連携がない |

### 入出力
| 不在モジュール | 他アプリでの存在 | 必要性 |
|---------------|-----------------|--------|
| **SVG出力** | AE | 🟢 SVGレイヤーはあるが出力は未確認 |
| **Lottie/Bodymovin出力** | AE | 🟢 Webアニメーション用。2Dリグと相性が良い |
| **Spine JSON出力** | Spine | 🟢 2Dリグデータの相互運用 |
| **PSDレイヤー構造の完全保持** | AE | 🟡 PSDDocumentクラスはあるが編集往復は未確認 |
| **FFmpeg ハードウェアエンコード NVENC/AMF/QSV** | AE/AME | 🟡 `preferHardware` 引数はあるがどこまで動くか未確認 |

### AI / 自動化
| 不在モジュール | 他アプリでの存在 | 必要性 |
|---------------|-----------------|--------|
| **Auto-tagging / Auto-caption** | Premiere/DaVinci | 🟢 AI連携の強みを活かせる |
| **Style Transfer** | RunwayML | 🟢 ONNX Runtime が既にあるので追加容易 |
| **Super Resolution** | Topaz/UE5 TSR | 🟢 FSRはあるがAI超解像はない |
| **Auto-masking（SAM等）** | AE Rotobrush 3.0 | 🟢 Segment Anything等のONNXモデル統合 |

---

## C. 緊急度マトリックス

| 優先度 | 項目 | 理由 |
|--------|------|------|
| 🔴 P0 | OCIO v2 実ライブラリ統合 | 全アプリ標準。自前実装のままでは業界ワークフローに入れない |
| 🔴 P0 | プレーナートラッカー | コンポジットアプリの基本。ないと実務不可 |
| 🔴 P0 | ワークスペース保存 | DCCの基本機能。ないのは異常 |
| 🔴 P0 | ワープスタビライザー（作り直し） | 既存コードはバグで完全に機能しない |
| 🟡 P1 | アニメーションレイヤー | Maya/AEで標準のノンリニア編集 |
| 🟡 P1 | キーイング強化（IBK級） | クロマキーだけでは不十分 |
| 🟡 P1 | キーフレーム補助（Easy Ease UI） | エンジンはあるがメニューがない |
| 🟡 P1 | Deepコンポジット | 3DレンダリングAOVを扱うなら必須 |
| 🟢 P2 | Lottie出力 | Webアニメとの橋渡し。2Dリグと相性良し |
| 🟢 P2 | Style Transfer / Super Resolution | ONNX Runtimeが既にあるので追加容易 |
| 🟢 P2 | Rotobrush級 AIマスク | AI連携の強みを活かせる差別化ポイント |
| 🟢 P2 | Collaborative editing | 長期目標 |
