# Artifact ドラッグ&ドロップ・ワークフロー改善提案

## 現在の状況
- ✅ `ArtifactProjectView`: ドラッグ&ドロップハンドラー実装済み
- ✅ `ArtifactLayerPanelWidget`: ドラッグ&ドロップハンドラー実装済み
- ❌ 実装の詳細が不明瞭（MIMEデータ処理、ImageLayer作成ロジックなど）

---

## 🎯 優先度別改善案

### **P1: 必須機能（実装必要）**

#### 1. **プロジェクトビュー → タイムラインレイヤーパネル ドラッグ&ドロップ**
```
目的: プロジェクトビューから画像をドラッグしてタイムラインに追加
流れ:
1. プロジェクトビューから画像をドラッグ開始
2. タイムラインレイヤーパネルへドラッグオーバー
3. ドロップで自動的にImageLayerを作成・追加

実装箇所:
- ArtifactProjectView::dropEvent() - ドラッグ開始時にMIMEデータを設定
- ArtifactLayerPanelWidget::dragEnterEvent() - 画像MIMEタイプを検証
- ArtifactLayerPanelWidget::dropEvent() - ImageLayer作成とコンポジション追加ロジック
```

#### 2. **ImageLayer自動作成と配置**
```
問題: ドロップ時にどのレイヤーの下に配置するか、タイミングをどうするか
改善案:
- ドロップ位置に基づいて挿入位置を決定
- z順序（レイヤースタック）を正しく反映
- 既存レイヤーの上に新規レイヤーを追加
```

---

### **P2: 高優先度（UX向上）**

#### 3. **ビジュアルフィードバック改善**
```
実装内容:
- ドラッグ中のカーソル表示（許可/禁止アイコン）
- ドロップ可能な領域のハイライト表示
- 挿入位置のプレビュー表示（グレーアウトプレースホルダー）
```

#### 4. **複数画像の一括ドラッグ**
```
実装内容:
- プロジェクトビューで複数画像を選択
- 複数ファイルをMIMEデータに含める
- ドロップ時に複数ImageLayerを連続作成
```

#### 5. **タイムライン位置の自動調整**
```
実装内容:
- ドロップ時にレイヤーの再生開始位置を自動設定
- 既存レイヤーとの時間重複を検出
- 警告や自動配置オプションを提供
```

---

### **P3: 中優先度（ワークフロー効率化）**

#### 6. **アンドゥ/リドゥ統合**
```
実装内容:
- ドラッグでレイヤー追加時にコマンドをUndoStackに記録
- 複数レイヤー追加を1つのアンドゥアクションにまとめる
```

#### 7. **ドラッグ中のプレビュー**
```
実装内容:
- ドラッグ中にドロップ先レイヤーパネルに一時プレビュー表示
- フェードイン/アウトアニメーション
- キャンセル時のスムーズな復帰
```

#### 8. **プロジェクトビューの画像プレビュー**
```
実装内容:
- プロジェクトビューにサムネイル表示
- ホバー時に拡大プレビュー表示
- ドラッグ時にカスタム自由な形状を表示
```

---

### **P4: 低優先度（オプション強化）**

#### 9. **ドラッグ時のレイアウト自動配置**
```
実装内容:
- 複数レイヤーをドラッグ時に自動的にスタック配置
- グリッドレイアウト生成
- 時間軸に沿った配置
```

#### 10. **クイックアクション（右クリックメニュー）**
```
実装内容:
- プロジェクトビューで「Add to Timeline」オプション
- 「Add to Current Composition」オプション
- ドラッグ以外のオプションも提供
```

#### 11. **DragProxy（カスタムドラッグウィジェット）**
```
実装内容:
- ドラッグ中にカスタム画像/テキストを表示
- ファイルパス、ファイル数などを表示
- 動的にサイズ変更
```

#### 12. **フィルター&検索をドラッグフレンドリーに**
```
実装内容:
- フィルタリング後も正しくドラッグ可能にする
- 検索結果の画像をドラッグ可能にする
```

---

## 🛠️ 実装の手順

### **ステップ1: 基本的なドラッグ&ドロップ動作確認**
```cpp
// ArtifactLayerPanelWidget::dropEvent()
void ArtifactLayerPanelWidget::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();

    // ファイルパスを取得
    if (mimeData->hasUrls()) {
        foreach (QUrl url, mimeData->urls()) {
            QString filePath = url.toLocalFile();
            if (isImageFile(filePath)) {
                // ImageLayer作成
                createImageLayerFromFile(filePath);
            }
        }
        event->acceptProposedAction();
    }
}
```

### **ステップ2: MIMEデータの統一化**
```cpp
// ArtifactProjectView::mouseMoveEvent()
// ドラッグ開始時にカスタムMIMEデータを作成
QMimeData* mimeData = new QMimeData();
mimeData->setData("application/x-artifact-image", filePath.toUtf8());
mimeData->setText(filePath);  // フォールバック
```

### **ステップ3: ビジュアルフィードバック**
```cpp
// dragEnterEvent時のハイライト表示
void ArtifactLayerPanelWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        highlightDropZone(true);
        event->acceptProposedAction();
    }
}

void ArtifactLayerPanelWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    highlightDropZone(false);
}
```

---

## 📊 実装優先度マトリックス

| 機能 | 実装難度 | 効果 | 優先度 |
|------|--------|------|--------|
| 基本ドラッグ&ドロップ | ⭐ | ⭐⭐⭐⭐⭐ | P1 |
| ImageLayer自動作成 | ⭐⭐ | ⭐⭐⭐⭐⭐ | P1 |
| ビジュアルフィードバック | ⭐⭐ | ⭐⭐⭐⭐ | P2 |
| 複数選択ドラッグ | ⭐⭐ | ⭐⭐⭐ | P2 |
| アンドゥ/リドゥ | ⭐⭐⭐ | ⭐⭐⭐ | P3 |
| ドラッグプレビュー | ⭐⭐⭐ | ⭐⭐ | P4 |

---

## 📝 推奨実装順序

1. **Phase 1（今すぐ）**: P1機能の実装
2. **Phase 2（1週間以内）**: P2のビジュアルフィードバック
3. **Phase 3（以降）**: P3/P4の追加機能

---

## ⚠️ 注意点

- **MIMEデータ互換性**: Windowsのファイルエクスプローラーからのドラッグにも対応
- **パフォーマンス**: 大量のレイヤー追加時のUX
- **エラーハンドリング**: 無効なファイル形式の処理
- **UndoStack**: 複数レイヤー追加時の1つのアンドゥアクション統合

