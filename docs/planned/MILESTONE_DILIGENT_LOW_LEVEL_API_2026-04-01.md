# Milestone: Diligent Low-Level Rendering API Expansion (2026-04-01)

**Status:** Phase 1 Complete (アウトライン矩形 PSO 修正済み)
**Goal:** `ArtifactIRenderer` の低レベル描画 API を拡充し、2D/3D ギズモ、テキスト、バッチ描画を完全対応にする

---

## 現状

| カテゴリ | 状態 |
|---------|------|
| **Solid Rect / Sprite** | ✅ 実装済み |
| **Line / Thick Line / Dot Line** | ✅ 実装済み |
| **Bezier (2次/3次)** | ✅ 実装済み (CPU セグメント分割) |
| **Circle / Crosshair / Checkerboard** | ✅ 実装済み |
| **3D Gizmo (Line/Arrow/Circle/Quad)** | ✅ 実装済み |
| **PSO 管理 (15ペア)** | ✅ 実装済み (TBB並列作成) |
| **テキスト描画** | ❌ 未実装 |
| **アウトライン矩形 PSO** | ⚠️ シェーダのみ、PSO未作成 |
| **バッチ描画** | ❌ 未実装 |
| **アップスケール設定** | ❌ no-op |
| **3D Line thickness** | ❌ 無視されている |

---

## Phase 1: Bug Fixes & API Parity (P0)

### 対象
1. **`outlinePsoAndSrb_` の PSO 作成漏れ修正**
   - シェーダは作成済みだが `createPSOs()` で PSO が生成されていない
   - `drawRectOutlineLocal()` が機能しない

2. **`drawLineLocal()` を `ArtifactIRenderer` に公開**
   - `PrimitiveRenderer2D` には実装済み
   - public API として追加するだけ

3. **`draw3DLine()` thickness 対応**
   - 現在 `(void)thickness` で無視
   - 太線としてジオメトリ展開

### 見積: 2h

---

## Phase 2: Text Rendering API (P0)

### 対象
1. **`drawText()` API の追加**
   - フリーフォント (stb_truetype) または Qt フォントレンダリング
   - パラメータ: 位置、文字列、フォントサイズ、色、アライメント

2. **テキスト用 PSO の追加**
   - `TextPSO` (SDF or bitmap フォント)
   - グリフキャッシュ (GPU texture atlas)

3. **デバッグ HUD 用オーバーレイ**
   - FPS 表示、レイヤー情報、デバッグテキスト

### 対象ファイル
| ファイル | 内容 |
|---------|------|
| `Artifact/include/Render/ArtifactIRenderer.ixx` | `drawText()` 宣言追加 |
| `Artifact/src/Render/ArtifactIRenderer.cppm` | 実装 |
| `Artifact/src/Render/TextRenderer.ixx` | テキストレンダラ (新規) |

### 見積: 8h

---

## Phase 3: Batch Rendering System (P1)

### 対象
1. **同一 PSO の draw call バッチ化**
   - 頂点バッファの動的拡張
   - 同一マテリアルの描画を1 call に統合

2. **ベジエ曲線の最適化**
   - 現在: CPU セグメント分割 → 個別 draw call
   - 改善後: 全セグメントを1バッファに蓄積 → 1 call

3. **スプライトバッチ**
   - 複数スプライトを1テクスチャアトラスにパック
   - 1 draw call で描画

### 対象ファイル
| ファイル | 内容 |
|---------|------|
| `Artifact/src/Render/BatchRenderer2D.ixx` | バッチレンダラ (新規) |
| `Artifact/src/Render/ArtifactIRenderer.cppm` | バッチ統合 |

### 見積: 12h

---

## Phase 4: Advanced Features (P2)

### 対象
1. **アンチエイリアスライン**
   - MSAA またはシェーダベース AA
   - カメラ距離に応じたライン幅自動調整

2. **`setUpscaleConfig()` の実装**
   - FSR / RSR 統合
   - 4K レンダリング時の性能向上

3. **3D ギズモ拡張**
   - Sphere / Cube 基本メッシュ描画
   - 選択ハイライト

### 見積: 16h

---

## Recommended Order

| 順序 | フェーズ | 見積 | 優先度 |
|---|---|---|--------|
| 1 | **Phase 1: Bug Fixes & API Parity** | 2h | P0 |
| 2 | **Phase 2: Text Rendering API** | 8h | P0 |
| 3 | **Phase 3: Batch Rendering System** | 12h | P1 |
| 4 | **Phase 4: Advanced Features** | 16h | P2 |

**総見積: ~38h**

---

## 既存の関連ファイル

| ファイル | 内容 |
|---------|------|
| `Artifact/include/Render/ArtifactIRenderer.ixx` | IRenderer インターフェース |
| `Artifact/src/Render/ArtifactIRenderer.cppm` | 実装 |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 2D プリミティブ |
| `Artifact/src/Render/PrimitiveRenderer3D.cppm` | 3D プリミティブ |
| `Artifact/src/Render/ShaderManager.cppm` | PSO/SRB 管理 |
| `Artifact/src/Render/DiligentDeviceManager.cppm` | デバイス管理 |
