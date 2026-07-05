# マイルストーン: Inspector / PropertyEditor 機能監査 (2026-07-04)

> 作成: 2026-07-04

## 監査サマリー

Inspector 周りは3つの主要ファイルで構成:
- `ArtifactInspectorWidget`（2,551行）— 右ドック、レイヤー選択表示、エフェクトスタック操作
- `ArtifactPropertyWidget`（2,158行）— プロパティ行のレイアウト、キーフレームコントロール
- `ArtifactPropertyEditor`（2,657行）— 個別プロパティエディタ（Numeric/Color/Bool/String/Enum/Rotation/Path/ObjectRef/DashPattern 他）



---

## 🔴 P0: プロパティ行の操作系

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Property Reset ボタン（デフォルト値に戻す）** | AE/C4D/Blender | ⚠️ 計画あり (MILESTONE_PROPERTY_RESET_BUTTONS_2026-04-10) |
| **Numeric Field Quick Calc（数式入力）** | AE | ⚠️ 計画あり (MILESTONE_NUMERIC_FIELD_QUICK_CALC_2026-06-07)。`*2`, `/3`, `+100` 等の式入力 |
| **Drag-to-Scrub（数値フィールドをドラッグで増減）** | AE/Blender/Nuke | ❌ 数値フィールド上で左右ドラッグによる粗調/微調 |
| **Ctrl+Drag 微調整（0.1x 速度）** | AE/Blender | ❌ |
| **Shift+Drag 粗調整（10x 速度）** | AE/Blender | ❌ |
| **Property Keyframe ナビゲーション矢印** | AE | ⚠️ ◆ ボタンはあるが前後キーフレームジャンプ矢印が未確認 |
| **Property の Show in Timeline ボタン** | AE | ❌ プロパティを選択するとタイムラインでそのキーフレームだけ表示 |
| **Property Pick Whip（ドラッグで参照リンク）** | AE | ⚠️ 計画あり (MILESTONE_PARENT_PICK_WHIP_2026-07-03) |
| **Expression フィールド（プロパティに式を入力）** | AE | ❌ 各プロパティに expression を直接入力できるテキストフィールド |
| **Expression の有効/無効トグル** | AE | ❌ |

---

## 🔴 P0: エフェクトスタック操作

| 機能 | 参照元 | 状態 |
|---|---|---|
| **エフェクトのドラッグ並び替え** | AE/Nuke | ⚠️ |
| **エフェクトの複製（Duplicate）** | AE | ❌ |
| **エフェクトのコピー＆ペースト（レイヤー間）** | AE | ❌ |
| **エフェクトの一時無効（FX バイパス）** | AE | ❌ 個別エフェクト単位の ON/OFF スイッチ |
| **エフェクトの一括無効（全エフェクト ON/OFF）** | AE | ❌ |
| **エフェクトプリセット保存/適用** | AE | ❌ エフェクト＋パラメータをプリセットとして保存 |
| **エフェクト検索/フィルタ** | AE | ❌ エフェクトパネル内のインクリメンタル検索 |
| **Adjustment Layer 視覚インジケーター** | AE | ⚠️ AdjustableLayer はあるが Inspector 上の表示が弱い |

---

## 🟡 P1: インスペクタ表示・レイアウト

| 機能 | 参照元 | 状態 |
|---|---|---|
| **複数選択時の一括編集** | AE/Blender/Unity | ❌ 複数レイヤー選択時に共通プロパティを一括変更 |
| **複数選択時の「値が異なる」表示（Mixed）** | AE/Unity | ❌ 選択間で値が異なる場合「--」や「Mixed」表示 |
| **セクション折りたたみ/展開** | AE/Unity/Nuke | ⚠️ |
| **セクション全展開/全折りたたみ** | AE | ❌ Alt+クリックで全セクション一括操作 |
| **プロパティ検索フィルタ（Ctrl+F）** | Unity/Blender | ❌ Inspector 内のプロパティ名検索 |
| **プロパティのお気に入りピン留め** | C4D | ❌ よく使うプロパティを常に上部に固定表示 |
| **プロパティのソロ表示（1つだけ表示）** | Nuke | ❌ 選択プロパティだけを Inspector に表示 |
| **キーフレームのあるプロパティのみ表示** | AE (U キー) | ❌ |
| **変更されたプロパティのみ表示（デフォルトから差分）** | Unity | ❌ |

---

## 🟡 P1: 数値入力・カラー

| 機能 | 参照元 | 状態 |
|---|---|---|
| **数値スライダー（インライン）** | Blender/Unity | ⚠️ 一部あり |
| **値のステップスナップ（整数/0.1/0.01切替）** | Blender | ❌ |
| **カラーピッカー（FloatColorPicker）** | - | ✅ 既存 |
| **カラーパレット（保存＋呼出し）** | AE/Unity | ❌ |
| **カラーピッカー with Contrast Ratio** | Figma/DevTools | ❌ WCAG コントラスト比表示 |
| **色温度/露出/色相による色調整** | Resolve | ❌ |
| **Hex/RGB/HSL 入力切替** | AE/Figma | ⚠️ |

---

## 🔵 P2: 高度機能

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Property Animation プレビュー（ミニグラフ）** | C4D | ❌ プロパティ行に小さな値グラフをインライン表示 |
| **Property Modifier Stack** | Blender | ❌ Generator→Modifier→Property の順に適用される修飾子スタック |
| **Property Driver（別プロパティで駆動）** | Blender | ❌ Scripted Expression の簡易版。A の値で B を制御 |
| **Property Group（カスタムプロパティグループ）** | Blender | ❌ ユーザー定義のプロパティグループ |
| **Component-wise 編集（X/Y/Z 個別リンク切替）** | Unity/Blender | ❌ Position.x だけ編集、または全軸連動 |
| **スクリプト埋め込みプロパティ** | C4D (Python tag) | ❌ |
| **Property Copy/Paste（レイヤー間でプロパティ値のみコピペ）** | AE | ❌ |
| **Undo per Property** | AE/Unity | ⚠️ 全体 Undo はあるがプロパティ単位は不明 |

---

## 📊 優先度マトリクス

| 優先 | カテゴリ | 件数 | 代表機能 |
|---|---|---|---|
| 🔴 | プロパティ行操作 | 10 | Reset/DragScrub/QuickCalc/Expression/PickWhip |
| 🔴 | エフェクトスタック | 8 | 並替/複製/コピペ/FXバイパス/プリセット保存 |
| 🟡 | インスペクタ表示 | 9 | 複数選択一括編集/Mixed表示/検索/ピン留め/キーフレームフィルタ |
| 🟡 | 数値入力/カラー | 6 | スライダー/ステップスナップ/カラーパレット |
| 🔵 | 高度機能 | 8 | PropertyAnim/MiniGraph/ModifierStack/Driver |

---

## 関連文書

- `Artifact/docs/PROPERTY_EDITOR_AUDIT_2026-03-11.md` — 既存監査
- `docs/planned/MILESTONE_PROPERTY_WIDGET_ROW_ALIGNMENT_*` — 行整列計画
- `docs/planned/MILESTONE_NUMERIC_FIELD_QUICK_CALC_2026-06-07.md` — 数式入力
- `docs/planned/MILESTONE_PROPERTY_RESET_BUTTONS_2026-04-10.md` — Reset ボタン
- `docs/done/MILESTONE_PARENT_PICK_WHIP_2026-07-03.md` — Pick Whip
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` — 2,551行
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` — 2,158行
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm` — 2,657行
既に完了したマイルストーン: M-UI-23 Row Alignment、M-UI-3 Inspector Usability

以下、AE Effect Controls / Nuke Properties Bin / Blender Properties Editor / C4D Attribute Manager / Unity Inspector / Figma Properties Panel の **6 アプリ群**と比較。