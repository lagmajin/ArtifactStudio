# 実装案: App Settings - Widget 接続ギャップ

> 2026-06-13 作成  
> 状態: 一部未接続

---

## 既存接続状況（良好）

| 設定 | ウィジェット | 接続状況 |
|------|-------------|--------|
| `toolbarShowGrid` / `toolbarShowGuide` | ArtifactToolBar (gridToggleAction / guideToggleAction) | ✅ 接続済み |
| `timelineShyActive` | ArtifactTimelineGlobalSwitches (shyBtn) | ✅ 接続済み |
| `timelineMotionBlurActive` | ArtifactTimelineGlobalSwitches (motionBlurBtn) | ✅ 接続済み |
| `timelineGraphEditorActive` | ArtifactTimelineGlobalSwitches (graphEditorBtn) | ✅ 接続済み |
| `timelineAllowOverscroll` | ArtifactTimelineGlobalSwitches (overscrollBtn) | ✅ 接続済み |
| `compositionShowMotionPathOverlay` | ArtifactTimelineGlobalSwitches (motionPathBtn) | ✅ 接続済み |
| `themeName` | ArtifactMainWindow | ✅ 適用済み |
| `previewResolutionPercent` | ArtifactViewMenu (menu items) | ✅ 切り替り動作 |

---

## 未接続（ UI 追加可能 ）

### 1. EditMode / DisplayMode UI 接続

| 設定 | ウィジェット | 提案 |
|------|-------------|------|
| *(EditMode は ArtifactToolService)* | ArtifactToolBar へ追加 | ✅ M-APP-7 で対応 |
| *(DisplayMode は ArtifactToolService)* | ArtifactToolBar へ追加 | ✅ M-APP-8 で対応 |

**提案**: ツールバーにモード切替プルダウンを追加  
VSコードのモード選択プルダウンと同様のUI

### 2. Composition Grid Settings

| 設定 | ウィジェット | 状態 |
|------|-------------|------|
| `compositionGridSettings` | ArtifactCompositionRenderController | ⚠️ 実装済みがコード内にある |
| `compositionGridSettings` | ToolOptionsBar へ追加 | ❌ 未接続 |

**提案**: ToolOptionsBar へ Grid サブパネル追加  
"グリッド表示中のみ表示" のコンテキストUI

### 3. Preview Settings 切り替え

| 設定 | ウィジェット | 状態 |
|------|-------------|------|
| `previewQualityText` | ViewMenu/Workspace | ⚠️ 未接続 |
| `previewEnableRamCache` | ViewMenu/Workspace | ❌ 未接続 |
| `previewEnableDiskCache` | ViewMenu/Workspace | ❌ 未接続 |

**提案**: ViewMenu → "Preview" サブメニューを拡張  
品質プリセット (Full/Half/Auto) + Cache トグル

### 4. Clone Effector UI （Blender風ワークフロー）

| 設定 | ウィジェット | 状態 |
|------|-------------|------|
| *(CloneLayer Settings)* | ToolOptionsBar | ❌ 未接続 |
| *TransformCloneEffector* | ToolOptionsBar | ❌ 未接続 |

**提案**: "クローン" ツール選択時に Effectors セクション表示  
- Offset / Rotation / Scale（スライダー）
- Field Type（Sphere / Linear）
- Falloff（Hard / Smooth）

---

## 提案マイルストーン

### M-APP-SETT-1: Preview Settings UI 接続
- ViewMenu に Preview Quality/Resolution アクション追加
- `previewQualityText`, `previewEnableRamCache` 接続
- 作業時間: 2-3h

### M-APP-SETT-2: Grid Settings UI 接続
- ToolOptionsBar へ Grid Settings サブパネル追加
- `compositionGridSettings` → UI 双方向バインド
- 作業時間: 2h

### M-APP-SETT-3: Clone/Modifier Tool UI
- ToolOptionsBar へ Clone Effector 切り替え追加
- 作業時間: 3-4h

---

## コード参照

- `Artifact/include/Service/ArtifactToolService.ixx` - EditMode/DisplayMode API
- `Artifact/include/Tool/Tool.ixx` - EditMode/DisplayMode enum
- `Artifact/src/Widgets/ArtifactToolBar.cppm` - ツールバー接続
- `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm` - ViewMenu
- `ArtifactCore/include/Application/ArtifactAppSettings.ixx` - 設定API