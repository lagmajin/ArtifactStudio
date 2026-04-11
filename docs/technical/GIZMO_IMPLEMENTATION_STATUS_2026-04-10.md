# Gizmo Implementation Status (2026-04-10)

## 対象

- 2D transform gizmo: `Artifact/src/Widgets/Render/TransformGizmo.cppm`
- public interface: `Artifact/include/Widgets/Render/TransformGizmo.ixx`
- composition editor host: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

## 現状サマリ

- Move gizmo: 実装済み
- Scale gizmo: 実装済み、現状でおおむね OK
- Rotate gizmo: 実装あり。ただし ImGuizmo ライクな品質には未到達
- Anchor gizmo: 実装済み
- 3D gizmo: 別実装 (`Artifact3DGizmo`) で存在

## 判定メモ

### Scale Gizmo

- 四隅、上下左右、中央スケールのハンドルがある
- ドラッグ処理も既存で成立している
- 現時点では「使える」判定

### Rotate Gizmo

- 旧実装の「上に突き出た回転ノブ」は削除済み
- 現在は外周リング + ドラッグ中の弧表示に変更済み
- ただし ImGuizmo の rotate gizmo と比べると、まだ以下が不足

- 軸方向や視覚的な意味が弱い
- リングの情報設計がまだ簡素
- ドラッグ中フィードバックは出るが、完成度は低い
- 「ImGuizmo に似ても似つかない」という評価は妥当

## なぜレイヤー上に X のような線が出るのか

原因は `Scale` 表示時に、中心から四隅へ線を引いているためです。

該当箇所:

- `Artifact/src/Widgets/Render/TransformGizmo.cppm:467`
- `Artifact/src/Widgets/Render/TransformGizmo.cppm:468`
- `Artifact/src/Widgets/Render/TransformGizmo.cppm:469`
- `Artifact/src/Widgets/Render/TransformGizmo.cppm:470`

内容としては、`center_c -> tl_c / tr_c / bl_c / br_c` をすべて描いているため、
矩形内部に対角線 2 本ぶんの見え方が出て、結果として `X` に見えます。

これは現在の scale gizmo が

- 中心から上下左右へ伸びる軸線
- 中心から四隅へ伸びる補助線

を同時に描いているためです。

## 問題整理

### P1: Rotate gizmo の再設計が必要

- 目標は ImGuizmo 系の rotate 表現
- 現状は「外周リングを置いた」段階で、デザインも操作感も未完成

### P1: Scale gizmo の X 線は視認性を落としている

- スケール用補助線としては情報量が多すぎる
- レイヤー内容の視認を邪魔する
- 少なくとも常時表示はやめる候補

## 次の改善候補

1. Scale gizmo の `center -> corner` 4 本線を廃止する
2. 角ハンドルだけ残し、内部の X は出さない
3. Rotate gizmo を ImGuizmo 参照で再設計する
4. Rotate 時の開始角 / 現在角 / sweep 表示をもっと明確にする
5. Rotate hit area と visual thickness を分離する

## 実装方針メモ

- Scale は「現状維持ベースでノイズ削減」がよい
- Rotate は「部分修正」ではなく見た目と hit test をまとめて再設計した方が安全
- ImGuizmo 参照メモとして `docs/ImGuizmo_Gizmo_Structure_Analysis.md` を併読対象にする
