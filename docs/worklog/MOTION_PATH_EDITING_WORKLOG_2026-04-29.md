# モーションパス編集 作業ログ

作成日: 2026-04-29

---

## 概要

Composition Editor のモーションパスを、単なる表示から実用的な編集対象に拡張した。
今回は「移動だけ」を主軸にしつつ、追加・削除・補間変更・hover 強調までまとめて入れている。

---

## 実施したこと

### 1. モーションパスの直接編集

- モーションパスのキーフレーム点をドラッグして移動できるようにした
- 変更時に undo / redo を積むようにした
- 親レイヤーがある場合は親空間へ戻してから位置を更新するようにした

### 2. キーフレーム操作のショートカット

- `Shift + クリック` で現在フレームにモーションパスキーを追加
- `Alt + クリック` で現在ヒットしたモーションパスキーを削除
- 右クリックのコンテキストメニューからも追加 / 削除を実行可能

### 3. 補間タイプの編集

- 現在フレームのモーションパスキーに対して補間を変更できるようにした
- 追加した項目:
  - Hold
  - Linear
  - Ease In
  - Ease Out
  - Ease In-Out
  - Bezier
  - Back
  - Expo
- 現在の補間と同じ項目は無効化して、選択状態が分かりやすいようにした

### 4. 表示の強化

- モーションパスのキー点を補間タイプごとに色分けした
- hover 中のキー点をリングで強調した
- キーが 1 個だけでも点が見えるようにした
- 現在フレームのキーを見分けやすくした

### 5. Core 側の補助 API

- `AnimatableTransform3D` に位置キーの値上書き API を追加した
- `AnimatableTransform3D` に位置キーの補間取得 / 更新 API を追加した
- `AnimatableValueT` に keyframe value の上書き API を追加した

---

## 主な変更ファイル

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
- `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`
- `ArtifactCore/src/Animation/AnimatableTransform3D.cppm`
- `ArtifactCore/include/Animation/AnimatableValue.ixx`

---

## 補足

- ビルドとテストは未実施
- 追加した操作は、右クリックメニューとショートカットの両方から触れる構成にしている
- 今回はハンドル編集までは入れず、位置キーの直接編集に範囲を絞った

