# VP マスク操作 要求動作一覧

**日付**: 2026-07-31
**ベース**: Adobe After Effects CC のマスク操作挙動

---

## 1. データモデル

### 1.1 MaskVertex（ベジェ制御点）
```
struct MaskVertex {
    QPointF position;     // アンカーポイント
    QPointF inTangent;    // 入力タンジェント（position相対）
    QPointF outTangent;   // 出力タンジェント（position相対）
};
```

### 1.2 MaskPath（ベジェパスマスク）
- 頂点リスト（std::vector<MaskVertex>）
- 閉じたパスか否か（isClosed）
- アニメーションキーフレーム対応（MaskPathKeyframeSnapshot）
- マスク属性: opacity, feather, expansion, inverted, mode

### 1.3 LayerMask（レイヤー付属マスクコンテナ）
- 複数 MaskPath を保持
- 合成モードごとに全体のアルファマスクを生成
- 有効/無効フラグ

### 1.4 MaskMode（合成モード）
- Add（加算）
- Subtract（減算）
- Intersect（交差）
- Difference（差分）

---

## 2. マスク描画（VPオーバーレイ）

### 2.1 パスライン描画
- [x] ベジェ曲線を18分割のポリラインで近似描画
- [x] 線色: 青系（#82C8FF, opacity 0.88）
- [x] 線幅: 5.2px
- [x] ホバー中マスク: オレンジ系（#FFDB6B, opacity 0.60）
- [x] ドラッグ中マスク: オレンジ系（#FFDB6B, opacity 0.60）
- 描画は `LineDebugKind::MaskPath` フラグでフィルタ可能

### 2.2 アンカーポイント（頂点）描画
- [x] 各頂点を矩形マーカー（drawMaskSquareMarker）で描画
- [x] 通常色: 白（#F8FDFF）
- [x] ホバー中: オレンジ（#FFC247）
- [x] ドラッグ中: 赤系（#FF663D）
- [x] アクティブマスク: #FFE68A
- [x] 影付き（shadowColor + shadowExpand 4px）
- [x] 頂点描画は `LineDebugKind::MaskHandle` フラグでフィルタ可能

### 2.3 ベジェハンドル（タンジェント）描画
- [x] 頂点から InTangent/OutTangent 方向への細線（handleStrokeWidth 3px）
- [x] 線色: 青系（#D1EBFF, opacity 0.88）
- [x] 先端に矩形マーカー（7px）
- [x] タンジェントが (0,0) の場合は非表示
- [x] ハンドル色: 青系（#B3E5FF, opacity 0.95）
- [x] ホバー中: オレンジ（#FFC74F）
- [x] ドラッグ中: 赤系（#FF703D）
- [x] アクティブマスク時: オレンジ系（#FFD166）

### 2.4 描画中のマスク（pendingMask）
- [x] ペンツールで作成中のマスクパスを別スタイルで描画
- [x] 線: 青系（#D1EBFF）、5.8px
- [x] 頂点: 青系（#D6FAFF）、影付き
- [x] 3頂点以上で初回頂点に戻ると閉じる操作可能

---

## 3. ペンツール（Pen Tool）

### 3.1 新規マスク作成
- [x] クリックで頂点を追加（pendingMaskPath_ に蓄積）
- [x] 最初のクリックで `beginMaskEditTransaction` を開始
- [x] 頂点間は自動的に直線で結ばれる
- [x] ドラッグでベジェハンドル付き頂点を作成（Ctrl+クリックでOutTangent操作）
- [x] 3頂点以上で初回頂点に近づくとパスを閉じる（`finalizePendingMaskCreation`）
- [x] 閉じると `MaskEditCommand` 経由で UndoManager に push
- [ ] Esc キーで作成中マスクをキャンセル（`clearPendingMaskCreation`）

### 3.2 頂点追加（既存パス）
- [x] セグメント上をクリック → `hitTestMaskSegment` → `insertVertexOnMaskSegment`
- [x] ベジェ曲線上の t 値を計算し、アンカーポイントを挿入
- [x] Undo 対応済み（MaskEditTransaction）

### 3.3 パスを閉じる
- [x] 開いたパスの最初の頂点をクリック → `setClosed(true)`
- [x] パスを閉じると自動的に新しい空パスが追加され、連続作成が可能

### 3.4 パス削除
- [ ] 頂点を Delete/Backspace で削除
- [ ] 頂点数が2未満になったらパス全体を削除
- [ ] パス削除の Undo 対応

---

## 4. 頂点選択とドラッグ

### 4.1 ヒットテスト
- [x] `hitTestMaskHandle`: キャンバス座標をレイヤーローカル空間に逆変換し、距離で判定
- [x] 頂点ヒット閾値: 12px / zoom（Accessibility::targetScale 適用）
- [x] ベジェハンドルヒット閾値: 14px / zoom
- [x] セグメントヒット閾値: 12px / zoom

### 4.2 頂点ドラッグ
- [x] 頂点をクリック → `isDraggingVertex_ = true`
- [x] マウス移動時、ビューポート→キャンバス→レイヤーローカル座標に変換して vertex.position を更新
- [x] ドラッグ中 `MaskEditTransaction` で Undo 管理

### 4.3 頂点ドラッグの修飾キー
- [x] なし: 自由移動
- [ ] Shift: 軸ロック（水平/垂直/45度）
- [ ] Ctrl: スナップ（コンポジション境界、他レイヤー頂点）

### 4.4 頂点の複数選択
- [ ] Shift+クリックで頂点を追加選択
- [ ] ラバーバンド選択（矩形ドラッグで範囲内の頂点を選択）
- [ ] 全選択（Ctrl+A）
- [ ] 選択解除（クリックで何もない場所をクリック）
- [ ] 複数選択頂点の同時ドラッグ

---

## 5. ベジェハンドル（タンジェント）操作

### 5.1 ハンドルドラッグ
- [x] InTangent / OutTangent を個別にドラッグ
- [x] `setMaskVertexHandle`: handleDelta を適用
- [x] デフォルトでミラーリング（InTangent と OutTangent が対称に連動）
- [x] Alt キーでタンジェントをブレイク（独立操作）

### 5.2 タンジェントのリセット
- [ ] ハンドルをダブルクリック → tangent = (0,0) にリセット
- [ ] 頂点を Ctrl+クリック → タンジェントをリセット

### 5.3 ハンドル表示の切り替え
- [x] 頂点選択時のみハンドル表示（現在は `LineDebugKind::MaskHandle` フラグ）
- [ ] 全頂点のハンドルを常時表示するオプション

---

## 6. 矩形ツール（Rectangle Tool）

### 6.1 矩形マスク作成
- [x] `RectangleToolMode::Mask` で選択レイヤーに矩形マスクを作成
- [x] ドラッグで矩形範囲を指定
- [x] QPainterPath から MaskPath リストに変換（`MaskPath::fromQPainterPath`）
- [x] Undo 対応

### 6.2 矩形マスクの形状属性
- [ ] 角丸半径（roundness）の変更
- [ ] 作成後に頂点編集で形状変更可能（4頂点のベジェパスに変換される）

### 6.3 修飾キー
- [x] なし: 自由矩形
- [ ] Shift: 正方形
- [ ] Alt: 中心から拡大

---

## 7. マスクのプロパティ操作

### 7.1 合成モード変更
- [x] マスクごとに MaskMode（Add/Subtract/Intersect/Difference）を設定可能
- [ ] VP上で右クリック→コンテキストメニューから変更
- [ ] Inspector で変更可能（Property Widget 経由）

### 7.2 マスクの有効/無効
- [x] LayerMask::setEnabled() で切り替え
- [ ] VP上のマスク名ラベルをクリックでトグル
- [ ] 無効マスクは非表示（描画スキップ）

### 7.3 フェザー
- [x] MaskPath に feather/featherHorizontal/featherVertical/featherInner/featherOuter プロパティ
- [ ] VP上でフェザー範囲を視覚的に表示
- [ ] VP上でフェザーハンドルをドラッグして調整

### 7.4 不透明度
- [x] MaskPath::opacity() プロパティ
- [ ] VP上で直接変更する UI

### 7.5 拡張/収縮（Expansion）
- [x] MaskPath::expansion() プロパティ
- [ ] VP上で拡張範囲を視覚的に表示

### 7.6 反転
- [x] MaskPath::isInverted() プロパティ
- [ ] VP上でトグル UI

---

## 8. マスクのアニメーション

### 8.1 キーフレーム
- [x] MaskPathKeyframeSnapshot によるフレーム単位のスナップショット
- [x] `setAnimationKeyframe(frame, snapshot)` / `sampleAtFrame(frame)`
- [ ] キーフレームの補間（線形 / ベジェ / ホールド）
- [ ] タイムライン上でのマスクキーフレーム表示
- [ ] VP上でのキーフレームナビゲーション（前後のキーフレームにジャンプ）

### 8.2 シェイプトゥイーン
- [ ] 異なる頂点数のマスクパス間の補間
- [ ] 最初の頂点の対応付け（first vertex matching）

---

## 9. マスクのレイヤー操作

### 9.1 マスクの並び順
- [ ] マスクの上下入れ替え（合成順序の変更）
- [ ] マスクの複製（Ctrl+D）

### 9.2 マスクのコピー/ペースト
- [ ] 同一レイヤー内でマスクをコピー
- [ ] レイヤー間でマスクをコピー
- [ ] シェイプマスクとマスクパスの相互変換

### 9.3 マスクの親レイヤー追従
- [x] マスクはレイヤーの transform に従って変形
- [x] globalTransform → invTransform でローカル座標に変換して操作

---

## 10. クイックマスクプリセット

### 10.1 定義済みプリセット
- [x] `QuickMaskPreset` enum: Circular, Rectangular, Linear, Radial, Threshold
- [x] `applyQuickMaskPreset` / `armQuickMaskPreset`
- [x] 適用時に `beginMaskEditTransaction` → `markMaskEditDirty` → `commitMaskEditTransaction`

### 10.2 プリセットの操作
- [ ] VP上でプリセットマスクの中心/半径/角度をドラッグ調整
- [ ] プリセットマスクを通常のベジェマスクに変換

---

## 11. コンテキストメニュー

### 11.1 マスク右クリックメニュー
- [ ] マスクの追加
- [ ] マスクの削除
- [ ] マスクモードの変更
- [ ] 反転
- [ ] マスクのロック
- [ ] マスクの色変更

---

## 12. キーボードショートカット

| キー | 動作 | 状態 |
|------|------|------|
| Delete/Backspace | 選択頂点を削除 | [ ] 未実装 |
| Ctrl+A | 全頂点選択 | [ ] 未実装 |
| Ctrl+D | マスク複製 | [ ] 未実装 |
| Esc | ペンツールの作成中マスクキャンセル | [ ] 未実装 |
| Shift+ドラッグ | 軸ロック | [ ] 未実装 |
| Alt+ドラッグ | タンジェントブレイク | [x] 実装済み |
| Ctrl+ドラッグ | 頂点スナップ | [ ] 未実装 |

---

## 13. 実装現状まとめ

### 13.1 実装済み
- [x] MaskVertex / MaskPath / LayerMask データモデル
- [x] MaskPathKeyframeSnapshot アニメーション基盤
- [x] パスライン描画（ベジェ曲線近似）
- [x] アンカーポイント描画（矩形マーカー + 影）
- [x] ベジェハンドル描画（線 + 先端マーカー）
- [x] pendingMask 描画
- [x] ペンツール: 頂点追加（クリック）
- [x] ペンツール: パス閉じる（3頂点以上で初回頂点クリック）
- [x] ペンツール: セグメント上クリックで頂点挿入
- [x] 頂点ドラッグ
- [x] ベジェハンドルドラッグ（ミラーリング + Altブレイク）
- [x] 矩形ツール: 矩形マスク作成
- [x] MaskEditTransaction（Undo管理）
- [x] クイックマスクプリセット
- [x] ホバー/ドラッグ中の色変更
- [x] LineDebugKind フィルタ（MaskPath / MaskHandle）

### 13.2 未実装
- [ ] 頂点の複数選択（Shift+クリック / ラバーバンド / 全選択）
- [ ] 複数頂点の同時ドラッグ
- [ ] 頂点ドラッグの Shift 軸ロック
- [ ] 頂点/パスの削除（Delete/Backspace）
- [ ] Esc でペンツールキャンセル
- [ ] タンジェントのダブルクリックリセット
- [ ] 矩形ツールの Shift 正方形 / Alt 中心拡大
- [ ] 角丸半径の調整
- [ ] マスクの並び順入れ替え
- [ ] マスクの複製（Ctrl+D）
- [ ] マスクのコピー/ペースト（レイヤー間含む）
- [ ] マスクモードのVP上変更UI
- [ ] マスク有効/無効のVP上トグル
- [ ] フェザーのVP上視覚表示とハンドル調整
- [ ] 不透明度のVP上変更
- [ ] 拡張/収縮のVP上視覚表示
- [ ] 反転のVP上トグル
- [ ] キーフレーム補間（線形/ベジェ/ホールド）
- [ ] タイムライン上のマスクキーフレーム表示
- [ ] シェイプトゥイーン（異なる頂点数間の補間）
- [ ] プリセットマスクのドラッグ調整
- [ ] プリセット→ベジェマスク変換
- [ ] コンテキストメニュー
- [ ] マスクのロック
- [ ] マスクの色変更
