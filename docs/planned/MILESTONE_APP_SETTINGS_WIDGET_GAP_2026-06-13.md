# 実装案: App Settings - Widget 接続ギャップ

> 2026-06-13 作成  
> 状態: Partial（主要設定・EditMode／DisplayMode・Preview cache・Grid UI・CloneLayer Property Editor・既定Transform Effector接続済み、追加Effector型とruntime検証は未完了、静的確認 2026-07-30）

### 2026-07-29 Implementation Loop

- ✅ Preview Settings の RAM / disk cache トグルを ViewMenu に接続。
- ✅ Preview quality preset の選択を `previewQualityText` に保存し、起動後のメニュー状態へ復元。
- ✅ RAM cache の設定変更を実行中の `ArtifactPlaybackService` に同期。
- ✅ Disk cache の設定変更を実行中の `ArtifactPlaybackService` に同期。
- ✅ `previewCacheSizeMB` をディスクキャッシュの per-composition budget と global budget に実行時反映。
- ✅ Preview / Grid 設定の実装経路はコード上で接続済み（runtime 動作確認は未実施）。
- ✅ Grid 設定メニューの主間隔・分割数に現在値を表示し、設定状態をメニュー上で確認可能にした。
- ✅ CloneLayer の mode / count / transform / grid / radial / `useEffector` 等を既存 Property Editor の `Clone` グループへ接続。既定の `TransformCloneEffector` の strength / position / rotation / scale / color toggle も編集可能。Clone設定一式（transform stagesと既定Effector値・色を含む）はJSON保存・再読込にも対応。既定Effectorはゼロオフセットで初期化済み。
- ⏳ 追加Effector型の追加・個別編集UIとruntime検証は未完了。既存の ToolOptionsBar の「コピースタンプ」はブラシ系 Clone tool の半径・位置固定であり、CloneLayer の編集導線とは別責務。

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
| `compositionGridSettings` | ViewMenu の「グリッド設定」サブメニュー | ✅ 主間隔・分割数・Major／Minor／Axis 表示を接続 |

**提案**: ToolOptionsBar へ Grid サブパネル追加  
"グリッド表示中のみ表示" のコンテキストUI

### 3. Preview Settings 切り替え

| 設定 | ウィジェット | 状態 |
|------|-------------|------|
| `previewQualityText` | ViewMenu/Workspace | ✅ ViewMenu 選択・復元、Workspace 起動適用 |
| `previewEnableRamCache` | ViewMenu/Workspace | ✅ ViewMenu の品質プリセットメニューに接続 |
| `previewEnableDiskCache` | ViewMenu/Workspace | ✅ ViewMenu の品質プリセットメニューに接続 |

**提案**: ViewMenu → "Preview" サブメニューを拡張  
品質プリセット (Full/Half/Auto) + Cache トグル

### 4. Clone Effector UI （Blender風ワークフロー）

| 設定 | ウィジェット | 状態 |
|------|-------------|------|
| *(CloneLayer Settings)* | Property Editor の Clone group | ✅ 静的接続済み |
| *TransformCloneEffector* | ToolOptionsBar / Property Editor | ❌ 未接続 |

**提案**: "クローン" ツール選択時に Effectors セクション表示  
- Offset / Rotation / Scale（スライダー）
- Field Type（Sphere / Linear）
- Falloff（Hard / Smooth）

---

## 提案マイルストーン

**静的実装の完了マーク（2026-07-30）**: M-APP-SETT-1 Preview Settings UI 接続 ✅、M-APP-SETT-2 Grid Settings UI 接続 ✅、M-APP-SETT-3 CloneLayer Property Editor・既定Transform Effector接続 ✅。追加Effector型とruntime動作確認は未完了。

### M-APP-SETT-1: Preview Settings UI 接続
- ViewMenu に Preview Quality/Resolution アクション追加
- `previewQualityText`, `previewEnableRamCache`, `previewEnableDiskCache` 接続
- 作業時間: 2-3h

### M-APP-SETT-2: Grid Settings UI 接続
- ViewMenu の Grid Settings サブメニューへ接続
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
