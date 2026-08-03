# Brush / Eraser Tool 要求動作一覧

**日付**: 2026-07-31
**ベース**: Adobe After Effects CC Brush Tool / Eraser Tool

---

## Brush Tool（ブラシツール） — `ToolType::Brush`

### 1. ブラシ描画の基本フロー

#### 1.1 前提条件
- Paintレイヤー（ArtifactPaintLayer）が選択されている必要がある
- Paintレイヤーがない場合は自動生成するオプション（AEではデフォルト生成）

#### 1.2 ストローク描画
- キャンバス上でマウスダウン → ストローク開始
- ドラッグ中、一定間隔（5ポイント毎）で PaintLayer に applyStroke()
- マウスアップ → ストローク終了、残りのポイントを apply
- 各ストロークは独立した履歴エントリとして管理

### 2. ブラシプロパティ

#### 2.1 基本プロパティ
| プロパティ | 説明 | 範囲 |
|-----------|------|------|
| Diameter | ブラシサイズ（px） | 1 - 2500 |
| Angle | ブラシの回転角度 | 0° - 360° |
| Roundness | 真円度（100% = 正円） | 0% - 100% |
| Hardness | 硬さ（エッジのぼかし） | 0% - 100% |
| Opacity | 不透明度 | 0% - 100% |
| Flow | 流量（1ストローク中の重ね塗り量） | 0% - 100% |
| Spacing | ブラシ点の間隔 | 1% - 1000% |

#### 2.2 ブラシ先端形状
- 円形（デフォルト）
- 楕円（Angle + Roundness で調整）
- カスタムブラシ（テクスチャ/画像ベース）

#### 2.3 ブラシダイナミクス
| パラメータ | 制御元 | 説明 |
|-----------|--------|------|
| Size Jitter | Pen Pressure / Random | サイズのランダム変動 |
| Angle Jitter | Pen Pressure / Random | 角度のランダム変動 |
| Roundness Jitter | Pen Pressure / Random | 真円度の変動 |
| Opacity Jitter | Pen Pressure / Random | 不透明度の変動 |
| Flow Jitter | Pen Pressure / Random | 流量の変動 |
| Scatter | Pen Pressure / Random | ブラシ点の散らばり |

### 3. ペンタブレット対応

#### 3.1 筆圧対応プロパティ
- Diameter（筆圧でサイズ変更）
- Opacity（筆圧で不透明度変更）
- Flow（筆圧で流量変更）
- Angle（筆圧/傾きで角度変更）
- Roundness（筆圧/傾きで真円度変更）

#### 3.2 Wacom / WinTab 連携
- 筆圧値（0.0 - 1.0）の取得
- ペンの傾き（tilt X / tilt Y）の取得
- ペンの回転（barrel rotation）の取得
- スタイラスと消しゴムの自動切替

### 4. Paintレイヤー管理

#### 4.1 Paintレイヤーの特性
- 各ブラシストロークは独立したベクター/ラスター要素
- ストロークの順序変更可能
- ストローク単位の表示/非表示
- ストローク単位のブレンドモード

#### 4.2 ストロークのプロパティ
- 色（Fill Color）
- 不透明度
- ブレンドモード
- ブラシ設定（Diameter/Hardness/Angle/Roundness）
- Channels（RGBAのどのチャンネルに描画するか）
- Duration（表示期間）

### 5. Clone Stamp Tool（クローンスタンプ）

AEではブラシツールのバリエーションとしてクローンスタンプがあり、ソースレイヤーからサンプルして描画する。

- Alt+クリックでソースポイント設定
- ソースポイントと描画位置のオフセットを維持しながら複製
- ソースの時間オフセット設定
- ソースレイヤーの選択

### 6. VP上のブラシプレビュー

#### 6.1 カーソル
- 現在のブラシサイズを円で表示
- ブラシの先端形状（Angle + Roundness）を反映
- 色は現在のFill Colorを薄く表示

#### 6.2 ストロークプレビュー
- 描画中のストロークをリアルタイム表示
- 筆圧変動によるサイズ/不透明度変化を反映

---

## Eraser Tool（消しゴムツール） — `ToolType::Eraser`

### 1. 消しゴムの基本動作
- Paintレイヤー上の既存ストロークを消去
- ブラシツールと同じブラシ形状/サイズを使用
- マスクとして機能（透明度を低下させるのではなく、実際にピクセルを消去）

### 2. 消しゴムモード
| モード | 説明 |
|--------|------|
| Paint Eraser | ストローク単位で消去（AEデフォルト） |
| Layer Eraser | レイヤー全体のピクセルを消去 |
| Last Stroke Only | 直前のストロークのみ消去 |

### 3. 消しゴムのオプション
- ブラシサイズ/硬さ（ブラシツールと共有）
- 消去の強さ（Eraser Strength）

---

## 実装現状

### 実装済み
- [x] BrushTool クラス（mousePressEvent / mouseMoveEvent / mouseReleaseEvent）
- [x] ArtifactPaintLayer（applyStroke）
- [x] BrushStroke データ構造（points, radius, opacity, eraser flag）
- [x] 5ポイント毎の逐次適用
- [x] Eraserモードフラグ（eraserMode_）

### 未実装
- [ ] **Paintレイヤーの自動作成**（ブラシツール選択時、対象レイヤーがない場合）
- [ ] **VP上のブラシカーソルプレビュー**（円形 + サイズ + 色）
- [ ] **ブラシパネル**（Diameter/Hardness/Angle/Roundness/Opacity/Flow/Spacing）
- [ ] **ブラシダイナミクス**（Size/Opacity Jitter、Scatter）
- [ ] **ペンタブレット筆圧対応**
- [ ] **ストロークのVP上選択・編集・削除**
- [ ] **ストロークの順序変更（Paintパネル）**
- [ ] **ストロークのDuration設定**
- [ ] **Clone Stamp Tool**
- [ ] **Eraser Tool のVP上操作**
- [ ] **Last Stroke Only 消去**
- [ ] **PaintレイヤーとMaskレイヤーの相互変換**
