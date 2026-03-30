# コア機能 不足分析レポート

**作成日:** 2026-03-28  
**ステータス:** 分析完了  
**対象:** ArtifactCore / Artifact

---

## 概要

既存のコア機能を分析し、不足している機能・優先度の高い未実装機能を抽出した。

---

## 発見された未実装機能（優先度順）

### 🔴 優先度高：コア機能

#### 1. VideoLayer::generateProxy() 未実装

**場所:** `Artifact/src/Layer/ArtifactVideoLayer.cppm:696`

```cpp
bool ArtifactVideoLayer::generateProxy(ProxyQuality quality)
{
    // TODO: Implement actual proxy generation using FFmpeg or OpenCV
    // For now, just return false to indicate it's not implemented
    return false;
}
```

**影響:**
- 高解像度動画のプレビューが重い
- 4K/8K フッテージの実用的な編集が困難

**工数:** 16-24h  
**依存:** FFmpeg または OpenCV

---

#### 2. WebUI ブリッジの未実装機能

**場所:** `Artifact/src/Widgets/WebUI/ArtifactWebBridge.cppm`

**TODO 一覧:**
```cpp
// TODO: construct LayerID from string
// TODO: Look up effect by ID from the current layer and call setPropertyValue()
// TODO: add composition count, layer count, etc.
// TODO: Get current selected layer and serialize its effects/properties to JSON
```

**影響:**
- WebUI からの操作が制限される
- 自動化・スクリプティングが困難

**工数:** 8-12h  
**依存:** なし

---

#### 3. プロジェクト管理の未実装機能

**場所:** `Artifact/src/Project/ArtifactProject.cppm`

**TODO 一覧:**
```cpp
// bool ArtifactProject::Impl::removeById() - TODO: container_.remove() not implemented
// TODO: ダーティ状態が変更されたときの通知を実装する
```

**影響:**
- コンポジション削除機能が不完全
- 未保存状態の管理が不十分

**工数:** 4-6h  
**依存:** なし

---

#### 4. ROI Scissor 実装

**場所:** `Artifact/include/Render/ArtifactIRenderer.ixx`

**状況:**
- `pushScissor()` / `popScissor()` が未実装
- ROI 最適化の最終段階

**工数:** 4-6h  
**依存:** ROI 実装（完了済み）

---

### 🟡 優先度中：UX 機能

#### 5. Composition Editor 未実装機能

**場所:** `docs/planned/MILESTONE_COMPOSITION_EDITOR_LAYER_VIEW.md`

**未実装一覧:**
- ❌ 回転ハンドル
- ❌ アンカーポイント編集
- ❌ レイヤービュー：バウンディングボックス
- ❌ ルーラー
- ❌ マルチビュー

**工数:** 20-30h  
**依存:** TransformGizmo

---

#### 6. Text Layer インライン編集

**場所:** `docs/planned/MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md`

**状況:**
- Phase 1 の最小入り口だけ作成済み
- in-canvas caret / selection / IME は未実装

**工数:** 12-16h  
**依存:** Text Renderer

---

#### 7. Reactive Event System

**場所:** `docs/planned/MILESTONE_REACTIVE_EVENT_SYSTEM_2026-03-28.md`

**未実装:**
- ⬜ `CollisionDetector` (Core)
- ⬜ `ReactiveEventEngine` (App)
- ⬜ `ReactionExecutor` (App)
- ⬜ `ReactiveRuleEditor` (UI)

**工数:** 24-32h  
**依存:** Layer Property System

---

### 🟢 優先度低：拡張機能

#### 8. RAM Preview Cache

**場所:** `docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md`

**状況:**
- RAM preview としての cache range / prewarm / playback priority はまだ独立した仕様になっていない

**工数:** 16-20h  
**依存:** Playback Engine

---

#### 9. Motion Tracking Workflow

**場所:** `docs/planned/MILESTONE_MOTION_TRACKING_SYSTEM_2026-03-25.md`

**未実装:**
- tracker editor
- overlay
- stabilize
- bake

**工数:** 32-48h  
**依存:** Computer Vision Library

---

#### 10. Batch Rendering

**場所:** `docs/planned/MILESTONE_BATCH_RENDERING_2026-03-28.md`

**未実装:**
- ❌ 一括ジョブ追加
- ❌ プロジェクト全コンポジション追加
- ❌ テンプレートプリセット一括適用

**工数:** 12-16h  
**依存:** RenderQueue

---

## コアディレクトリ構成分析

### 既存ディレクトリ（69 個）

```
ArtifactCore/include/
├── AI/
├── Audio/          ✅ 充実
├── Animation/      ✅ 充実
├── Composition/    ✅ 充実
├── Layer/          ✅ 充実
├── Property/       ✅ 充実
├── Render/         ✅ 充実（ROI 追加済み）
├── Effect/         ✅ 充実
├── Mask/           ✅ 充実
├── Text/           ✅ 充実
├── Particle/       ✅ 充実
├── Physics/        ⚠️ 最小限
├── Tracking/       ⚠️ 最小限
├── Reactive/       ❌ 未実装
├── Crowd/          ❌ 未使用？
├── VST/            ❌ 未使用？
└── ...
```

### 不足しているコア機能

#### 1. **Reactive システム**（新規）

**必要性:** ⭐⭐⭐  
**理由:** インタラクティブなアニメーション・ゲーム的要素に必要

**実装予定:**
```
ArtifactCore/include/Reactive/
├── CollisionDetector.ixx      // 衝突検出
├── EventTrigger.ixx          // イベントトリガー
└── Reaction.ixx              // リアクション定義
```

---

#### 2. **Tracking システム**（拡充）

**必要性:** ⭐⭐⭐  
**理由:** 動画編集の核心機能

**実装予定:**
```
ArtifactCore/include/Tracking/
├── MotionTracker.ixx         // 動き追跡
├── FeatureDetector.ixx       // 特徴点検出
└── Stabilizer.ixx            // 手ぶれ補正
```

---

#### 3. **Physics システム**（拡充）

**必要性:** ⭐⭐  
**理由:** 物理ベースアニメーションに必要

**実装予定:**
```
ArtifactCore/include/Physics/
├── RigidBody.ixx            // 剛体シミュレーション
├── SoftBody.ixx             // 軟体シミュレーション
└── Fluid.ixx                // 流体シミュレーション
```

---

#### 4. **Cache システム**（新規）

**必要性:** ⭐⭐⭐  
**理由:** パフォーマンス向上に必須

**実装予定:**
```
ArtifactCore/include/Cache/
├── FrameCache.ixx           // フレームキャッシュ
├── TextureCache.ixx         // テクスチャキャッシュ
└── ProxyCache.ixx           // プロキシキャッシュ
```

---

## 推奨実装順序

### 第 1 段階：コア機能補完（24-36h）

1. **VideoLayer::generateProxy()**（16-24h）
   - FFmpeg または OpenCV 統合
   - 高解像度動画対応

2. **プロジェクト管理**（4-6h）
   - removeById() 実装
   - ダーティ状態通知

3. **WebUI ブリッジ**（4-6h）
   - LayerID 変換
   - プロパティ操作

---

### 第 2 段階：レンダリング最適化（4-6h）

4. **ROI Scissor 実装**（4-6h）
   - pushScissor() / popScissor()
   - GPU 最適化

---

### 第 3 段階：新機能（24-32h）

5. **Cache システム**（16-20h）
   - FrameCache
   - ProxyCache

6. **Reactive システム**（8-12h）
   - CollisionDetector
   - EventTrigger

---

## 工数サマリー

| 優先度 | 機能 | 工数 |
|--------|------|------|
| 🔴高 | VideoLayer Proxy | 16-24h |
| 🔴高 | プロジェクト管理 | 4-6h |
| 🔴高 | WebUI ブリッジ | 8-12h |
| 🔴高 | ROI Scissor | 4-6h |
| 🟡中 | Composition Editor | 20-30h |
| 🟡中 | Text インライン編集 | 12-16h |
| 🟡中 | Reactive システム | 24-32h |
| 🟢低 | RAM Preview Cache | 16-20h |
| 🟢低 | Motion Tracking | 32-48h |
| 🟢低 | Batch Rendering | 12-16h |

**合計:** 148-210h

---

## 即時実装推奨（3 機能）

### 1. VideoLayer::generateProxy()（16-24h）

**理由:**
- ユーザー体験に直結
- 高解像度動画編集に必須
- 競合優位性向上

---

### 2. ROI Scissor（4-6h）

**理由:**
- ROI 実装の最終段階
- パフォーマンス向上効果大
- 実装コストが低い

---

### 3. プロジェクト管理（4-6h）

**理由:**
- 基本機能の完成
- 安定性向上
- 実装コストが低い

---

## 関連ドキュメント

- `docs/planned/MILESTONES_BACKLOG.md` - 全体バックログ
- `docs/planned/MILESTONE_APP_LAYER_IMPROVEMENTS_2026-03-28.md` - アプリ層改善
- `docs/planned/MILESTONE_VIDEO_PROXY_IMPROVEMENT_2026-03-28.md` - ビデオプロキシ改善

---

**文書終了**
