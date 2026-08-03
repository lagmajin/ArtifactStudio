# MotionSketch / Puppet / TrackPoint Tool 要求動作一覧

**日付**: 2026-07-31
**ベース**: Adobe After Effects CC 特殊ツール

---

## MotionSketch Tool — `ToolType::MotionSketch`

### 1. 動作概要
- レイヤーをVP上でドラッグ → その動きが position キーフレームとして記録される
- コンポジションを再生しながらドラッグすることで、リアルタイムの動きをキャプチャ
- 一種の「パフォーマンスキャプチャ」

### 2. 操作フロー
| 手順 | 動作 |
|------|------|
| 1 | MotionSketch ツール選択 |
| 2 | 対象レイヤーを選択（事前選択） |
| 3 | キャンバス上でマウスダウン → スケッチ開始（同時にコンポジション再生開始） |
| 4 | ドラッグ → position キーフレームがリアルタイム記録 |
| 5 | マウスアップ → スケッチ終了、再生停止 |

### 3. パラメータ
| パラメータ | 説明 | 範囲 |
|-----------|------|------|
| Smoothing | キーフレームの平滑化度合い | 1 - 100 |
| Sample Rate | サンプリングレート（fps） | 1 - 60 |
| Show Wireframe | スケッチ中にワイヤーフレーム表示 | on/off |
| Background | スケッチ中に背景表示するか | on/off |

### 4. スムージング
- 記録された生データに対して移動平均/ベジェ平滑化を適用
- Smoothing値が高いほどキーフレーム数が減少し、動きが滑らかに
- 0 = 生データそのまま、100 = 最大平滑化

### 5. 実装現状

#### 実装済み
- [x] ArtifactMotionSketchTool クラス
- [x] beginSketch / finishSketch
- [x] MotionSketchUndoCommand（位置キーフレームのスナップショット + 復元）
- [x] サンプリング（sampleInterval = ~60fps）
- [x] 平滑化パラメータ（smoothing = 0.5f）

#### 未実装
- [ ] **スケッチ中のコンポジション自動再生/停止**
- [ ] **ワイヤーフレーム表示（Show Wireframe）**
- [ ] **スムージングのVP上調整**
- [ ] **スケッチ中の速度/加速度表示**
- [ ] **Show Background オプション**
- [ ] **Sample Rate のVP上変更**

---

## Puppet Tool — `ToolType::Puppet`

### 1. 動作概要
- レイヤーにメッシュを生成し、ピン（制御点）を使って変形させる
- メッシュは三角形分割され、ピンを移動すると周囲のメッシュが追従
- 人形の関節を動かすような自然な変形（MLS / ARAP などのアルゴリズム）

### 2. ピンの種類
| タイプ | 説明 |
|--------|------|
| Position | 移動可能な制御点（デフォルト） |
| Starch | 剛性ピン — 周囲の変形を抑制（硬さ） |
| Bend | 曲げピン — 角度つき変形の基準点 |
| Overlap | 重なり順制御ピン — 前後関係の制御 |

### 3. 操作フロー

#### 3.1 ピン追加
- キャンバスをクリック → その位置に Position ピンを追加
- Ctrl+クリック → Starch ピン追加
- 最初のピンは Extend 値（メッシュの拡張範囲）を決定

#### 3.2 ピン操作
- ピンをクリック → 選択
- 選択ピンをドラッグ → メッシュ変形
- ピンをDelete → 削除
- 選択ピンの周囲に回転ハンドル表示 → ドラッグで回転

#### 3.3 メッシュ
| プロパティ | 説明 | デフォルト |
|-----------|------|-----------|
| Triangle Count | メッシュの三角形数 | 200-500 |
| Expansion | メッシュの範囲拡張（px） | 1-100 |

### 4. ピンのアニメーション
- 各ピンの位置/回転をキーフレームアニメーション可能
- ピンの追加/削除も時間軸で管理
- Mesh 1 / Mesh 2 / Mesh 3 と複数メッシュの切り替え

### 5. 実装現状

#### 実装済み
- [x] ArtifactPuppetTool クラス
- [x] PinRecord（id, layerId, canvasPos, type, rotation, depth）
- [x] ピン追加（addPin）
- [x] ピンヒットテスト（hitTestPin）
- [x] ピン削除（removePin）
- [x] レイヤー変形（deformLayer）
- [x] OpenCVPuppetEngine（ARAP メッシュ変形）
- [x] handleMousePress / handleMouseMove 連携

#### 未実装
- [ ] **メッシュのVP上視覚表示**（三角形ワイヤーフレーム）
- [ ] **ピンのVP上表示**（黄/赤/青の円形マーカー + タイプ別色分け）
- [ ] **回転ハンドルの表示と操作**
- [ ] **Starch（剛性）量のVP上調整**
- [ ] **Overlap（前後関係）のUI**
- [ ] **メッシュ密度のVP上変更**
- [ ] **Extend（拡張範囲）の調整ハンドル**
- [ ] **複数メッシュ（Mesh 1/2/3）の切り替え**
- [ ] **ピンのキーフレームアニメーション**
- [ ] **Bend ピンタイプ**

---

## TrackPoint Tool — `ToolType::TrackPoint`

### 1. 動作概要
- VP上の特徴点を追跡し、その動きを Null レイヤーの position キーフレームに変換
- モーショントラッキングの基本的なワークフロー

### 2. トラッキングの種類
| タイプ | 説明 |
|--------|------|
| Point | 単一ポイントの X/Y 追跡 |
| Rotation | 2ポイント間の角度変化を追跡 |
| Scale | 2ポイント間の距離変化を追跡 |
| Perspective | 4ポイントの透視変換を追跡 |

### 3. 操作フロー
| 手順 | 動作 |
|------|------|
| 1 | TrackPoint ツール選択 |
| 2 | キャンバス上で追跡したい特徴点の周囲をドラッグ → トラック領域を設定 |
| 3 | トラックパネルで「Analyze Forward/Backward」を実行 |
| 4 | トラック結果を確認（トラックパスのVP表示） |
| 5 | 「Apply」→ Null レイヤーにキーフレームを書き出し |

### 4. トラックポイントのプロパティ
| プロパティ | 説明 |
|-----------|------|
| Feature Center | 追跡点の中心位置 |
| Feature Size | 特徴領域のサイズ（外側の矩形） |
| Search Size | 探索領域のサイズ（フレーム間で特徴を探す範囲） |
| Confidence | 追跡の信頼度（自動表示） |
| Adaptive Feature | フレーム毎に特徴を更新するか |

### 5. トラッキングアルゴリズム
- NCC（正規化相互相関）
- KLT（Lucas-Kanade オプティカルフロー）
- サブピクセル精度

### 6. 実装現状

#### 実装済み
- [x] ArtifactPointTrackerTool クラス
- [x] applyTrackingResult（NCC結果 → Nullレイヤーキーフレーム）
- [x] MotionTracker（Core側の追跡エンジン）
- [x] handleMousePress 連携
- [x] トラッカーギズモ（trackerGizmo_）

#### 未実装
- [ ] **トラック領域のVP上矩形表示**
- [ ] **トラック領域のドラッグ設定UI**
- [ ] **Analyze Forward/Backward のUIトリガー**
- [ ] **トラックパスのVP上表示**（結果の軌跡）
- [ ] **Confidence の可視化**
- [ ] **Feature Size / Search Size のVP上調整**
- [ ] **Rotation / Scale トラッキングモード**
- [ ] **Perspective トラッキング**
- [ ] **サブピクセル精度のオプション**
