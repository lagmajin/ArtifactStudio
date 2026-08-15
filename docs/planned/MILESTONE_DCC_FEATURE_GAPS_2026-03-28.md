# DCC ツール機能ギャップ分析 (2026-03-28)

**最終更新:** 2026-08-15

## 概要

ArtifactStudio を After Effects / Nuke / Fusion / DaVinci Resolve 等の
業界標準 DCC ツールと比較し、不足機能を特定。

---

## 機能マトリクス

| カテゴリ | 実装済み | 未実装 | 優先度 |
|---------|---------|--------|--------|
| **タイムライン/コンポジション** | 複数コンポ、解像度/FPS/Duration 設定、ワークエリア | ネストコンポ、アスペクト比設定、コンポマーカー | High |
| **レイヤーシステム** | Solid, Image, Video, Text, SVG, Particle, Camera, Light, Null, Shape, 3D Model、ブレンドモード、マスク、ペアレント | アジャストメントレイヤー（旗のみ）、マットレイヤー | Medium |
| **キーフレームアニメーション** | トグルボタン、Linear/Bezier/Hold/Auto 補間、カーブエディタ、タイムリマップ、エクスプレッション UI | モーションパス可視化、ロービングキー、グラフエディタ連携、エクスプレッション言語実装 | High |
| **エフェクト/コンポジット** | Blur, Color Correction, Distortion, Noise, Transitions、エフェクトプリセット、3D コンポジット（深度/フォグ/シャドウ） | OFX プラグイン、サードパーティプラグイン、高度なキーイング、3D リフレクション | High |
| **カラーマネジメント** | sRGB, Linear, Rec.709, Rec.2020, DCI-P3, Adobe RGB、LUT、カラーホイール (Lift/Gamma/Gain)、HDR モード | ACES、EXR ワークフロー統合、カラーマネジメント UI | High |
| **レンダリング/出力** | レンダーキュー、MP4/MOV/WebM 等、PNG/JPEG/TIFF/BMP シーケンス、エンコーダープリセット、GPU (Diligent) | レンダーファーム、分散レンダリング | Medium |
| **インポート/エクスポート** | PNG, JPEG, TIFF, EXR, PSD, SVG、MP4/MOV/AVI/MKV/WebM、WAV/MP3/AAC/FLAC、JSON | Alembic, USD, FBX, OBJ, GLTF、プロジェクト相互運用 (AEP, Nuke, Fusion) | High |
| **UI/ワークフロー** | Undo/Redo（履歴ウィジェット）、ドッキングパネル (Qt ADS)、キーボードショートカット、Python スクリプティング、D&D インポート | カスタムワークスペース、バッチ処理、高度なコピー/ペースト（エフェクト/キー） | Medium |
| **3D 機能** | 3D カメラ、3D ライト、3D モデルビューアー、メッシュインポータ | マテリアル/シェーダーシステム、環境マップ、3D アニメーション | Medium |
| **MoGraph** | Clone Generator、Force/Vortex/Turbulence 等のエフェクタ、パーティクルシステム、シェイプレイヤー | タイポグラフィアニメーション、高度テキストアニメーション、Step/Random エフェクタ | Medium |

---

## 最も重要な不足機能 (High Priority)

### 1. OFX プラグインサポート
- ヘッダーがコメントアウト済み、実際のプラグインローディングなし
- サードパーティエフェクトエコシステムが存在しない
- **影響:** Nuke/Resolve のエフェクトが使えない

### 2. 3D マテリアル/シェーダーシステム
- マテリアルエディタ、シェーダーグラフが存在しない
- **影響:** 3D コンポジットの品質制限

### 3. プロジェクト相互運用
- AEP / Nuke / Fusion のインポート/エクスポートが存在しない
- **影響:** 他ツールとのワークフロー統合が不可能

### 4. 高度なアニメーション
- モーションパス可視化なし
- ロービングキーフレームなし
- エクスプレッション言語が未実装
- **影響:** プロ品質のアニメーションが作れない

### 5. ACES カラーマネジメント
- ACES / ACEScg 対応なし
- **影響:** VFX パイプライン参加不可

---

## 既存の強み

1. **堅牢な基盤:** タイムライン、レイヤー、キーフレームのアーキテクチャが充実
2. **現代的技術スタック:** C++20 modules, Qt, Diligent Engine (GPU)
3. **カラーマネジメント基盤:** 多数のカラースペース + LUT 対応
4. **充実したレンダーキュー:** 多形式対応 + プリセット
5. **AI 統合:** エクスプレッションコパイロット
6. **パーティクル/MoGraph:** Clone Generator + エフェクタ + パーティクル

## 2026-08-15 現行コード監査

この表は 2026-03-28 時点のギャップであり、現行実装では複数項目が更新されている。

- **アニメーション:** Motion Path overlay／編集、Auto-Orient、Curve Editor の Bezier／tangent／複数キー操作、Animation Layer の Additive／Override 基盤が実装済み。専用 timeline surface、Speed graph 編集、runtime 検証は未完了。
- **色管理:** ACES／OCIO config、working/display/view、LUT、HDR、Color Science Panel が実装済み。色管理 UI の一部と全経路の parity 検証は未完了。
- **インポート／エクスポート:** PSD／SVG／Lottie、OBJ／FBX の 3D viewer 経路などが追加済み。AEP／Nuke／Fusion の完全な相互運用、USD／Alembic の汎用 round-trip は未確認。
- **レンダリング:** Render Queue、GPU encode/decode、Render Farm の基盤が存在する。実ファイル生成、分散再開、長時間 runtime は未検証。
- **UI／ワークフロー:** Edit／Animation／View menu、Layer Panel の Frequent／All context menu、Python／CommandIR 系の基盤がある。共通 Menu Registry、ユーザー固有のメニュー履歴、完全な batch workflow は未完了。
- **3D:** Camera／Light／Model viewer、solid／wireframe、overlay／gizmo の主要経路は実装済み。高度な material graph、完全な camera parity、3D animation／runtime 検証は未完了。

判定: **旧表をそのまま現状の未実装一覧として使うことはできない。主要基盤は実装済みだが、DCC相互運用の完全性・高度機能・runtime／受け入れ検証が残る。**
