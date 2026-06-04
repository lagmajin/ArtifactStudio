# タイムライン キーボードショートカット

## ナビゲーション

| ショートカット | 説明 |
|---------------|------|
| `Home` | タイムライン先頭に移動 |
| `End` | タイムライン末尾に移動 |
| `PageUp` | 10 フレーム戻る |
| `PageDown` | 10 フレーム進む |
| `Shift+J` | 先頭の keyframe に移動 |
| `Shift+K` | 最後の keyframe に移動 |
| `Ctrl+PageUp` | 直前の keyframe に移動 |
| `Ctrl+PageDown` | 次の keyframe に移動 |
| `Delete` / `Backspace` | 選択中の keyframe を削除 |
| `Ctrl+D` | 選択中の keyframe を現在位置へ複製 |
| `Ctrl+X` | 選択中の keyframe を切り取り |
| `Ctrl+Z` | アンドゥ |
| `Ctrl+Y` | リドゥ |
| `Enter` / `Keypad Enter` | 検索ヒットを次へ進める（検索が空なら keyframe を次へ進める） |
| `Shift+Enter` / `Shift+Keypad Enter` | 検索ヒットを前へ戻す（検索が空なら keyframe を前へ戻す） |
| `F3` | 検索ヒットを次へ進める（検索が空なら keyframe を次へ進める） |
| `Shift+F3` | 検索ヒットを前へ戻す（検索が空なら keyframe を前へ戻す） |
| `←` (左矢印) | 1 フレーム戻る（クリップ選択時は移動） |
| `→` (右矢印) | 1 フレーム進む（クリップ選択時は移動） |

## 選択操作

| ショートカット | 説明 |
|---------------|------|
| `Delete` / `Backspace` | 選択クリップを削除 |
| `Ctrl+A` | 全てのクリップを選択 |
| `Esc` | 選択を解除 |

## 実装詳細

### ナビゲーション

- **Home/End**: シーク位置を即座に先頭/末尾に移動
- **PageUp/PageDown**: 10 フレーム単位での移動（長押しで連続移動可能）
- **Shift+J/Shift+K**: 選択レイヤーの先頭 / 最後の keyframe へ移動
- **Ctrl+PageUp/PageDown**: 選択レイヤーの前後 keyframe へ移動
- **Delete/Backspace**: 選択中の keyframe を削除
- **Ctrl+D**: 選択中の keyframe を playhead に複製して選択を維持
- **Ctrl+X**: 選択中の keyframe を切り取り
- **Shift + drag**: keyframe 移動を 10 フレーム単位にスナップ
- **Drag near playhead**: current frame に近いときは playhead に吸い付く
- **Drag feedback**: 既存 keyframe に重なると drag tooltip に collision 数が出る
- **Enter / Shift+Enter**: search hit を前後移動し、検索が空のときは keyframe hit を前後移動する
- **F3 / Shift+F3**: search hit を前後移動し、検索が空のときは keyframe hit を前後移動する
- **矢印キー**: 
  - クリップ未選択時：シーク位置のみ移動
  - クリップ選択時：選択クリップを移動

### 選択操作

- **Delete/Backspace**: 現在選択されているクリップを削除
- **Ctrl+A**: 全クリップ選択（マルチクリップ編集用）
- **Esc**: 選択状態をクリア（作業リセット用）

## 今後の拡張候補

### 再生制御
- `Space` - 再生/一時停止トグル

### マーカー・リージョン
- `M` - マーカー追加
- `I` - イン点設定
- `O` - アウト点設定
- `Ctrl+I` - イン点に移動
- `Ctrl+O` - アウト点に移動

### ズーム
- `+` / `-` - ズームイン/アウト
- `Ctrl+0` - フィット表示

### クリップ操作
- `T` - クリップ分割（ Razor Tool）
- `V` - 選択ツール
- `M` - 移動ツール
- `G` - グループ化

## 実装方針

1. **Phase 1（今回）**: ナビゲーション・選択操作 ✅
2. **Phase 2**: 再生制御（Space など）
3. **Phase 3**: Undo/Redo 統合（Ctrl+Z / Ctrl+Y、keyframe 編集を含む）✅
4. **Phase 4**: マーカー・リージョン操作
5. **Phase 5**: ツール切り替え

## テストチェックリスト

- [ ] `Home` でフレーム 0 に移動
- [ ] `End` で最終フレームに移動
- [ ] `PageUp` で 10 フレーム戻る
- [ ] `PageDown` で 10 フレーム進む
- [ ] `Ctrl+PageUp` で直前の keyframe に移動
- [ ] `Ctrl+PageDown` で次の keyframe に移動
- [ ] `Delete` で選択中の keyframe が削除される
- [ ] `Ctrl+D` で選択中の keyframe を複製できる
- [ ] `Ctrl+X` で選択中の keyframe を切り取れる
- [ ] `Ctrl+Z` で keyframe 編集を戻せる
- [ ] `Ctrl+Y` で keyframe 編集をやり直せる
- [ ] `F3` で検索ヒットまたは keyframe を次へ進められる
- [ ] `Shift+F3` で検索ヒットまたは keyframe を前へ戻せる
- [ ] `Delete` で選択クリップが削除される
- [ ] `Ctrl+A` で全クリップ選択
- [ ] `Esc` で選択解除
- [ ] 矢印キーでクリップ未選択時はシーク移動
- [ ] 矢印キーでクリップ選択時はクリップ移動
