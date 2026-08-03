# Milestone: エフェクトパレット D&D + 簡易操作強化

**ステータス:** Not Started

**作成日:** 2026-07-30
**対象:** エフェクトパレットウィジェット新設、レイヤーパネル/ビューポートへの D&D 受付
**主目的:** エフェクトをパレットからレイヤーへドラッグ＆ドロップで直感的に適用可能にする

---

## 1. 背景

現在エフェクトの適用は右クリックメニュー（`ArtifactEffectMenu`）からのみ可能で、以下の問題がある：

- エフェクト一覧がメニューに隠れており、ブラウズしづらい
- 試したいエフェクトを素早く切り替えられない
- ドラッグ＆ドロップによる直感的な操作ができない

一方、D&D 基盤はすでに充実しており、6 箇所の既存実装に統一パターンがある。
カスタム MIME 命名規則も整備済み（`application/x-artifact-*`）。

---

## 2. Scope

### Phase 1: エフェクトパレット + レイヤーパネル D&D（コア）

- **EP-1: エフェクトパレットウィジェット新設**
  - ドッカブルパネル `ArtifactEffectPalette`
  - カテゴリ別ツリービュー（Color / Blur / Distort / Keying / Noise / Transition / Stylize / OFX）
  - 検索フィルター（インクリメンタル）
  - 既存の `ArtifactEffectService::availableEffects()` + `categoryForEffect()` を再利用
  - アイコン付きリスト（`QListView` + カスタムモデル）

- **EP-2: パレットからのドラッグ開始**
  - MIME タイプ `application/x-artifact-effect-add`
  - `QMimeData` に `EffectID` をエンコード
  - ドラッグプレビュー画像（エフェクト名 + アイコン）
  - 既存パターン準拠: `AssetBrowser::startDrag()` / `LayerPanel` の MIME 命名に倣う

- **EP-3: レイヤーパネルへのドロップ受付**
  - `ArtifactLayerPanelWidget::dropEvent` に MIME 判定を 1 行追加
  - ドロップ位置から対象レイヤーを特定（`rowAt(pos)`）
  - `effectService->addEffectToLayer(layerId, effectId)` 呼出
  - ドロップインジケーター表示（既存のハイライトパターン流用）

### Phase 2: ビューポート D&D + クイック適用（拡張）

- **EP-4: コンポジションビューポートへのドロップ受付**
  - `ArtifactCompositionEditor` にエフェクト MIME のドロップ処理追加
  - キャンバス上のヒットテストで対象レイヤーを特定
  - ビジュアルフィードバック（ドロップ領域ハイライト）

- **EP-5: ダブルクリック / Enter で即時適用**
  - パレット上で選択中エフェクトをアクティブレイヤーに即適用
  - ショートカットキー対応

### Phase 3: 発展（後続マイルストーン候補）

- **プリセット D&D:** エフェクトチェーン全体のドラッグ
- **エフェクト並び替え D&D:** パレット→Inspector 間（既存の EffectRackList 内部 D&D は完了済み）
- **クイックプレビュー:** ドラッグ中にホバーしたレイヤーにエフェクトを一時適用してプレビュー
- **お気に入り / 最近使用:** パレット上部にピン留めセクション

---

## 3. 実装詳細

### EP-1: パレットウィジェット

**新規ファイル:**
- `Artifact/src/Widgets/Effect/ArtifactEffectPalette.ixx` — モジュールインターフェース
- `Artifact/src/Widgets/Effect/ArtifactEffectPalette.cppm` — 実装

**ウィジェット構成:**
```
ArtifactEffectPalette (QDockWidget または QWidget)
├── QLineEdit（検索フィルター）
├── QTreeView / QListView
│   └── ArtifactEffectPaletteModel（QAbstractItemModel）
│       └── カテゴリ別グループ化 + フラットフィルター対応
└── コンテキストメニュー（お気に入り追加 等）
```

**データソース:**
- `ArtifactEffectService::availableEffects()` — 70+ エフェクトのハードコード済みリスト
- `ArtifactEffectMenu::categoryForEffect()` — カテゴリ分類ロジック（文字列ヒューリスティック）
- これらのロジックを `ArtifactEffectMenu.cppm` から共通化するか、パレット側にベンダーインライン化

### EP-2: ドラッグ開始

```cpp
// ArtifactEffectPalette.cppm
void ArtifactEffectPalette::startDrag(Qt::DropActions supportedActions) {
    auto index = currentIndex();
    auto effectInfo = model_->effectAt(index);
    
    auto mimeData = new QMimeData();
    mimeData->setData("application/x-artifact-effect-add", 
                       effectInfo.id.value().toUtf8());
    mimeData->setText(effectInfo.displayName);
    
    auto drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(createDragPixmap(effectInfo));
    drag->exec(Qt::CopyAction);
}
```

### EP-3: レイヤーパネル ドロップ受付

`ArtifactLayerPanelWidget::dropEvent`（約 6729 行目）に以下を追加:

```cpp
if (mimeData->hasFormat("application/x-artifact-effect-add")) {
    auto effectId = EffectID(QString::fromUtf8(mimeData->data("application/x-artifact-effect-add")));
    auto layerIndex = layerAt(dropPosition);
    auto layerId = model_->layerIdAt(layerIndex);
    effectService_->addEffectToLayer(layerId, effectId);
    event->acceptProposedAction();
    return;
}
```

### EP-4: ビューポート ドロップ受付

`ArtifactCompositionEditor::dropEvent`（既存のファイル D&D 処理付近）に追加:

```cpp
if (mimeData->hasFormat("application/x-artifact-effect-add")) {
    auto effectId = EffectID(QString::fromUtf8(mimeData->data("application/x-artifact-effect-add")));
    auto layerId = hitTestLayerAt(event->position().toPoint());
    if (layerId.isValid()) {
        effectService_->addEffectToLayer(layerId, effectId);
        event->acceptProposedAction();
        return;
    }
}
```

---

## 4. 既存コード改変範囲

| ファイル | 変更内容 | リスク |
|----------|----------|--------|
| `Artifact/src/Widgets/Effect/ArtifactEffectPalette.ixx` | **新規** | 低 |
| `Artifact/src/Widgets/Effect/ArtifactEffectPalette.cppm` | **新規** | 低 |
| `Artifact/CMakeLists.txt` | モジュール登録（1行追加） | 低 |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` | `dropEvent` に MIME 判定追加（~5行） | 低 |
| `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | `dropEvent` に MIME 判定追加（~10行） | 低 |
| `Artifact/src/Widgets/Menu/ArtifactEffectMenu.cppm` | `categoryForEffect()` 共通化（任意） | 中 |

---

## 5. 既存 D&D パターン参照

| 参照元 | ファイル | パターン |
|--------|----------|----------|
| アセットブラウザ D&D ソース | `ArtifactAssetBrowser.cppm:1186` | `startDrag()` + pixmap + `QMimeData::setUrls()` |
| レイヤーパネル D&D 受付 | `ArtifactLayerPanelWidget.cppm:6729` | `dropEvent` + MIME 分岐 + `rowAt()` |
| カスタム MIME 定義 | `ArtifactLayerPanelWidget.cppm:564-565` | `application/x-artifact-layer-reorder` |
| エフェクトメニュー | `ArtifactEffectMenu.cppm:284,299` | エフェクト一覧取得 + 適用 |
| エフェクトサービス | `ArtifactEffectService.cppm:800` | `addEffectToLayer()` |

---

## 6. 完了条件

- [ ] EP-1: パレットが表示され、全エフェクトがカテゴリ別に一覧できる
- [ ] EP-1: 検索フィルターでエフェクトが絞り込める
- [ ] EP-2: パレットからエフェクトをドラッグ開始できる（MIME データ正）
- [ ] EP-3: レイヤーパネル行にドロップしてエフェクトが適用される
- [ ] EP-3: Undo/Redo が正しく動作する（AddEffectUndoCommand 経由）
- [ ] EP-4: コンポジションビューポートのレイヤー上にドロップして適用される
- [ ] EP-5: ダブルクリックでアクティブレイヤーに即適用される
- [ ] D&D 中にパレットの選択状態が壊れない
- [ ] 不正なドロップ先（レイヤー外）ではアクションがキャンセルされる

---

## 7. 非スコープ（明示的除外）

- エフェクトのライブプレビュー（ドラッグ中ホバーで一時適用）→ 後続マイルストーン
- エフェクトチェーン（プリセット）の D&D → 後続
- パレットのカスタマイズ（お気に入り、並び替え）→ 後続
- EffectRackList（Inspector 側）の改修 → 既存で動作中、本マイルストーンでは触らない
- `ArtifactWidgets` サブモジュールの変更 → 禁止
