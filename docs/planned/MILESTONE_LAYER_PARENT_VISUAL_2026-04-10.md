# レイヤー親子関係の視覚化

**最終更新:** 2026-08-15
**マイルストーン**: M-LA-3 Layer Parent-Child Relationship Visualization
**作成日**: 2026-04-10
**見積もり**: 8-10h
**優先度**: Low (細かいUX改善)

## 2026-08-15 現行コード照合

- ✅ `ArtifactAbstractLayer` は parent id／parent lookup／親変換の合成を実装し、self-parent と cyclic parent を拒否する。parent id は JSON に保存・再読込される。
- ✅ Timeline は Parent 列、inline parent combo、Pick Whip の drag MIME、親選択／解除 menu、親レイヤー選択を持つ。Service には単体・複数 layer の parent 設定と Undo 復元経路がある。
- ✅ 行表示では `Parent: <name>` と接続済み状態を表示し、Pick Whip の hover row／接続先も描画する。旧案の「親子操作が未実装」とは現状が異なる。
- ⚠️ 現行表示は専用 Parent 列と inline controls が中心で、ツリー状のインデント線、色分け、折りたたみ状態保存、親選択時の子孫ハイライトは一体化した仕様として確認できない。
- ⏳ ドラッグ時の恒常的な接続線アニメーション、表示設定、アクセシビリティ詳細、大量階層での性能、保存／Undo を含む runtime QA は未完了。

## Update 2026-08-15

親ID・親変換・循環拒否・JSON復元、Timeline の Parent 列／inline parent combo／Pick Whip／Undo は現行コードで確認済み。残るのはツリー状の接続線・色分け・子孫ハイライト・折りたたみ状態保存などの表示統合と、大量階層を含む runtime QA である。

## 概要

After Effects のレイヤーパネルで、親子関係を視覚的にわかりやすく表示する。
複雑な階層構造の理解を助け、操作ミスを防ぐ。

## 機能仕様

### 階層表示の改善
**ツリー表示拡張:**
- インデント線の強化 (破線→実線)
- 親子関係の色分け表示
- 折りたたみ/展開状態の保存

### 視覚的フィードバック
**関係性の可視化:**
- ドラッグ時の接続線表示
- 選択時の関係ハイライト
- 親レイヤーの強調表示

**例:**
```
Layer 1 (親)
└── Layer 2 (子) ← インデント線で接続
    └── Layer 3 (孫)
```

### インタラクション改善
**操作支援:**
- 親レイヤー選択時の子レイヤー強調
- グループ選択/解除のショートカット
- 階層移動のドラッグ&ドロップ

### 実装要件
- 既存レイヤーパネル拡張
- パフォーマンス最適化 (大量レイヤー対応)
- 設定でON/OFF可能
- アニメーション効果

### 実装場所
- `Artifact/src/Widgets/LayerPanel/ArtifactLayerPanelWidget.cppm` (拡張)
- 設定項目: `Preferences > Layers > Parent visualization`

## 技術的考慮
- QTreeWidget のカスタム描画
- メモリ使用量の最適化
- アクセシビリティ対応

## AEとの差別化
- より詳細な視覚的フィードバック
- アニメーション効果
- 設定可能な表示オプション

## テストケース
- 各種階層構造の正確な表示
- ドラッグ&ドロップ操作
- パフォーマンス劣化の確認
- 設定変更の反映
