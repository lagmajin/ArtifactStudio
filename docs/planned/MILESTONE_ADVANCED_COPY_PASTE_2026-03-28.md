# Milestone: 高度コピー/ペースト (2026-03-28)

**Status:** Not Started
**Goal:** レイヤー、エフェクト、キーフレーム、プロパティをコピー＆ペースト可能にする。
After Effects の Copy/Paste に匹敵する操作性。

---

## 現状

| 機能 | 状態 | 場所 |
|------|------|------|
| レイヤーのコピー/ペースト | ⚠️ 基本のみ | `ArtifactCompositionEditor.cppm` |
| エフェクトのコピー/ペースト | ❌ 未実装 | — |
| キーフレームのコピー/ペースト | ❌ 未実装 | — |
| プロパティ値のコピー/ペースト | ❌ 未実装 | — |
| クリップボード形式 | ❌ 定義なし | — |

---

## Architecture

```
ArtifactClipboardManager (新規シングルトン)
  ├── copyLayer(layerId)         → LayerClipData
  ├── copyEffect(layerId, effectId) → EffectClipData
  ├── copyKeyframes(layerId, propPath) → KeyframeClipData
  ├── copyPropertyValue(layerId, propPath) → ValueClipData
  ├── paste(compositionId, targetLayerId?) → ペースト実行
  ├── pasteEffect(layerId, targetEffectId?) → エフェクトペースト
  └── pasteKeyframes(layerId, targetPropPath?) → キーフレームペースト

クリップボードデータ形式:
  application/x-artifact-layer        (レイヤー全体)
  application/x-artifact-effect       (エフェクト)
  application/x-artifact-keyframes    (キーフレーム)
  application/x-artifact-property     (プロパティ値)
```

---

## Milestone 1: クリップボード基盤

**目的:** `ArtifactClipboardManager` シングルトンを作成し、データ保持とペースト機能を提供。

### Implementation

1. `ArtifactClipboardManager` クラス作成:
   - `copyLayer(layerId)` — レイヤー全体をシリアライズ
   - `copyEffect(layerId, effectId)` — エフェクト設定をコピー
   - `copyKeyframes(layerId, propPath, frameRange)` — キーフレーム範囲をコピー
   - `copyPropertyValue(layerId, propPath)` — プロパティ値をコピー
   - `paste*(...)` — 各種ペースト
   - `canPaste*()` — ペースト可能性チェック
   - `clipboardType()` — クリップボード内容の型を返す

2. クリップボードデータ構造:
   ```cpp
   struct LayerClipData {
       QString layerType;              // "Solid", "Image", "Text", etc.
       QString displayName;
       QJsonObject transformData;      // position, scale, rotation, anchor, opacity
       QJsonArray effectsData;         // 全エフェクトの設定
       QJsonObject styleData;          // ブレンドモード、カラーラベル
   };
   struct EffectClipData {
       QString effectId;
       QString displayName;
       QJsonObject properties;         // 全プロパティの値
   };
   struct KeyframeClipData {
       QString propertyPath;
       QJsonArray keyframes;           // [{frame, value, easing}, ...]
   };
   ```

### 見積
| タスク | 見積 |
|--------|------|
| ClipboardManager クラス作成 | 3h |
| LayerClipData シリアライズ/デシリアライズ | 3h |
| EffectClipData シリアライズ | 2h |
| KeyframeClipData シリアライズ | 2h |
| pasteLayer() 実装 | 3h |
| pasteEffect() 実装 | 2h |
| pasteKeyframes() 実装 | 2h |

### Acceptance Criteria
- レイヤーをコピーして別コンポにペーストできる
- エフェクトをコピーして別レイヤーにペーストできる
- キーフレーム範囲をコピーして別プロパティにペーストできる

---

## Milestone 2: ショートカット & メニュー接続

**目的:** Ctrl+C / Ctrl+V / Ctrl+Shift+V で操作可能にする。

### Implementation

1. Edit メニューに項目追加:
   - `Copy Layer` (Ctrl+C)
   - `Paste Layer` (Ctrl+V)
   - `Copy Effect` (Ctrl+Shift+C)
   - `Paste Effect` (Ctrl+Shift+V)
   - `Copy Keyframes` — タイムライン選択範囲をコピー
   - `Paste Keyframes` — カーソル位置にペースト
   - `Copy Property Value` — プロパティ右クリックメニュー

2. コンテキストメニュー:
   - レイヤーパネル右クリック → Copy Layer / Paste Layer
   - タイムライン右クリック → Copy Keyframes / Paste Keyframes
   - プロパティ右クリック → Copy Value / Paste Value

3. ペースト時のオプション:
   - `Ctrl+V` — 現在位置にペースト
   - `Ctrl+Shift+V` — 元のタイミングでペースト
   - キーフレームペースト → カーソル位置にオフセット

### 見積
| タスク | 見積 |
|--------|------|
| Edit メニュー項目追加 | 1h |
| ショートカットバインド | 1h |
| コンテキストメニュー追加 | 2h |
| ペースト位置計算 (タイミングオフセット) | 2h |

---

## Milestone 3: 複数選択コピー

**目的:** 複数レイヤー/複数エフェクトを一括コピー。

### Implementation

1. 複数レイヤーコピー:
   - `copyLayers(layerIdList)` — 選択された全レイヤーをコピー
   - ペースト時は相対位置を維持

2. 複数エフェクトコピー:
   - `copyEffects(layerId, effectIdList)` — 複数エフェクトをコピー

3. 複数キーフレームコピー:
   - タイムラインで選択した範囲の全キーフレームをコピー

### 見積
| タスク | 見積 |
|--------|------|
| 複数レイヤーコピー | 3h |
| 相対位置維持ペースト | 2h |
| 複数エフェクトコピー | 2h |
| 複数キーフレームコピー | 2h |

---

## Milestone 4: システムクリップボード連携

**目的:** 他のアプリとの画像コピペ。

### Implementation

1. 画像コピー:
   - レイヤー/コンポジションを QImage としてシステムクリップボードへ
   - `QClipboard::setImage()`

2. 画像ペースト:
   - システムクリップボードから QImage を取得
   - 新規レイヤーとして追加

3. テキストペースト:
   - テキストレイヤーとしてペースト

### 見積
| タスク | 見積 |
|--------|------|
| 画像コピー | 1h |
| 画像ペースト | 1h |
| テキストペースト | 1h |
| クリップボード監視 | 1h |

---

## Deliverables

| ファイル | 内容 |
|---------|------|
| `ArtifactCore/include/Clipboard/ClipboardManager.ixx` (新規) | クリップボードマネージャ |
| `ArtifactCore/src/Clipboard/ClipboardManager.cppm` (新規) | 実装 |
| `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm` (拡張) | メニュー項目追加 |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` (拡張) | コンテキストメニュー |
| `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` (拡張) | プロパティ右クリックメニュー |

---

## Recommended Order

| 順序 | マイルストーン | 見積 | 理由 |
|---|---|---|---|
| 1 | **M1 クリップボード基盤** | 17h | 全機能の前提 |
| 2 | **M2 ショートカット & メニュー** | 6h | UX 向上、即効性高 |
| 3 | **M3 複数選択コピー** | 9h | プロダクション必須 |
| 4 | **M4 システムクリップボード連携** | 4h | 他アプリ連携 |

**総見積: ~36h**

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm` | - | Edit メニュー |
| `Artifact/src/Widgets/ArtifactTimelineWidget.cpp` | - | タイムライン |
| `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` | - | プロパティ |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | - | レイヤーパネル |
| `Artifact/src/Composition/ArtifactAbstractComposition.cppm` | - | コンポジション |
| `ArtifactCore/include/Serialization/ArtifactProjectExporter.ixx` | - | JSON シリアライズパターン |
