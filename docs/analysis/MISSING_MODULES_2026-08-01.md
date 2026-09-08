# 不在モジュール一覧 — 完全に存在しない or 空のスタブ

**日付**: 2026-08-01
**最終更新**: 2026-09-07
**定義**: 「不在」= ソースに1行も実装がない / 0バイト / 空名前空間 / 機能として存在しない

---

## A. 空スタブ・0バイト・即死コード（Compilerには通るが機能ゼロ）

| ファイル | 状態 |
|----------|------|
| ⚠️ `ArtifactRenderer/src/ExternalFrameRenderer.cpp`（部分実装・runtime未確認） | 現行ファイルは Blender Cycles adapter、job検証、frame/range出力、cache／cancel処理を実装。backend全種類の対応完了とは言えないため「不在」から部分実装へ訂正 |
| ✅ `ArtifactRenderController.cppm`（対応済み 2026-09-07） | 現行コードは command queue、clear／takeCommands、矩形・sprite登録、入力検証を実装 |
| ✅ `GeneratorManager.cppm`（対応済み 2026-09-07） | 現行コードは create／add／remove／get／contains／ids／clear と mutex保護を実装 |
| `ArtifactWidgets/src/Graphics/NodeWireGraphicItem.cppm` | 0バイト（未対応） |
| `ArtifactWidgets/src/Graphics/BackendSettingWidget.cppm` | 0バイト（未対応） |
| ✅ `SimpleSpline`（対応済み 2026-09-07） | 現行 `CloneGenerator.ixx` は保持点数を返し、Catmull-Rom補間で位置・接線を計算。空データ時の `{0,0,0}` は安全なfallback |
| ✅ DSL `CommandNode::compile()`（対応済み 2026-09-07） | `DSLParser.cppm` に UseComp／SelectLayers／SetProperty／AddKey／Rename／Delete／Group／Transaction の compile 実装を確認。旧記録の全 `nullptr` 指摘は現行コードに該当しない |
| ✅ `MetadataVectorizer`（対応済み 2026-09-07） | 現行コードは composition の duration / fps / 解像度 / layer / keyframe を読み取り、正規化してベクトル化。入力無視の固定値返却は確認されない |
| ✅ `BatchStabilizer`（対応済み 2026-09-07） | 現行コードは入力検証、画像読込、`VideoStabilizer` 実行、出力書込、progress／完了通知まで実装。動画連番バッチではなく単一画像ファイル処理のAPI |

## B. 完全に存在しないモジュール・サブシステム

### コンポジット
| 不在モジュール | 他アプリでの存在 | 必要性 |
|---------------|-----------------|--------|
| **OCIO v2 実ライブラリ統合** | AE/Nuke/Houdini/UE5 全て標準 | 🔴 致命的。自前3x3行列近似では実用不可 |
| **Deepコンポジット** | Nuke 標準。AE 一部 | 🟡 `DeepImageBuffer` とCPU合成・Deep EXR基盤は実装済み。GPU、DoF、制作UI統合が未完了 |
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
| **Wiggle / loopOut / smooth 式** | AE 標準 | 🟢 ExpressionEvaluator に wiggle、smooth、loopIn/Out、duration variants、time/frame conversion を実装済み。runtime確認待ち |
| **プロシージャルアニメプリセット** | Maya/Houdini | 🟢 Noise以外のパターンがない |
| **キーフレーム補助（Easy Ease/イーズイン/アウト）** | AE 標準 | 🟢 Timeline の Ease In/Out/In-Out、Easing Lab、コピー/ペースト導線を実装済み。runtime確認待ち |
| **リグレイヤーUI** | Maya/Spine/Live2D | 🟡 Rig Select/Weight、骨・コントロール・ウェイト編集、オーバーレイ、ポーズ操作を実装済み。専用パネルとruntime確認は残課題 |

### 3D
| 不在モジュール | 他アプリでの存在 | 必要性 |
|---------------|-----------------|--------|
| **サブディビジョンサーフェス** | Maya/Houdini/UE5 | 🟢 3Dモデリング用（2Dコンポジットアプリには不要か） |
| **スカルプティング** | Maya/Houdini/UE5 | 🟢 同上 |
| **CAD/NURBS** | Maya/Houdini | 🟢 同上 |
| **3D地面グリッド・グリッドフェード** | Maya/Blender/UE5 | 🟢 GroundGridSettings、XZ線生成、距離フェード、PrimitiveRenderer3D描画を実装済み。runtime確認待ち |
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
