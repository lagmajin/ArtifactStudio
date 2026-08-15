# 3Dレイヤー回転モデルの3軸化

**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

`ArtifactAbstractLayer` の 3 軸 snapshot、`rotationX/Y/Z` の JSON 保存／復元、Z→Y→X の共通行列生成、`Artifact3DModelLayer` の X/Y/Z 適用は現行コードで確認できる。旧 `rotation`／`rx` 互換も維持されている。一方、プロパティ UI の表示責務整理と、親子変換・モーションブラー・カメラ投影を組み合わせた runtime 受入はコード検索だけでは完了を証明できないため、受入条件全体は未検証とする。

## Update 2026-08-15

- `AnimatableTransform3D` の実装を再確認し、X/Y/Z の独立値・キーフレーム評価、Z→Y→X の Euler 行列、旧 `rotation` 値を Z 軸へ割り当てる互換経路を確認。
- 3D モデル側の X/Y/Z 適用と JSON／Undo 経路は実装済みと判断できる。
- Quaternion 編集、親子変換・モーションブラー・カメラ投影を組み合わせた runtime 受入、およびプロパティ UI の責務整理は未完了・未検証。

## 現状

- `AnimatableTransform3D` は既存のZ軸互換値に加え、X/Yの独立アニメーション値と3軸スナップショット値を保持する。
- `ArtifactAbstractLayer::setRotation3D(QVector3D)` は3軸値を `setRotationX/Y/Z` へ反映する。旧来の「Xのみ保存」という記述は現行コードと不一致。
- `AnimatableTransform3D` の共通行列生成はZ → Y → X順のEuler回転を適用し、`ArtifactAbstractLayer` のローカル変換にも接続済み。
- `Artifact3DModelLayer` と `ArtifactProcedural3DLayer` は `rotationX/Y/Z` のスナップショットをそれぞれのモデル行列へ適用する。
- JSON/UIには `rotationX` / `rotationY` / `rotationZ` が存在し、キーフレーム、Undo 用プロパティ経路、保存／再読込へ接続されている。残りは親子変換・モーションブラー・カメラ投影との runtime 受入である。

## 実装順序

1. `AnimatableTransform3D` にX/Y/Zの独立アニメーションAPIとスナップショット値を追加する。✅
2. 既存 `rotation` / `rx` はZ軸互換値として読み込む。
3. JSON出力では新形式を優先し、旧形式も引き続き読めるようにする。
4. Euler適用順序を固定する（Z → Y → X）。✅
5. `getLocalTransform4x4()` を共通の3軸行列生成経路へ変更する。✅
6. 3Dモデル、Procedural3D、Gizmo、Undo、保存／再読込を同じ値へ接続する。✅（プロパティ専用UIは未整理）
7. 親子変換、モーションブラー、カメラ投影との組み合わせを確認する。

## 互換方針

- 既存の単一 `rotation` はZ回転として扱う。
- 旧JSONにX/Y/Zが一部しかない場合は、欠落軸を0度で補完する。
- 新形式へ保存した後も、旧読込経路を削除しない。
- 角度の単位は既存どおりdegreeで統一する。

## 受け入れ条件

- X/Y/Zを個別に変更して平面が3軸で回転する。
- 各軸にキーフレームを設定・補間できる。
- 保存・再読込で3軸値が保持される。
- 旧単一rotationプロジェクトが見た目を変えずに開ける。
- Undo/Redoで3軸変更が一体として戻る。
- 親子レイヤーと3Dカメラの変換順が一貫する。

## 非対象

- Quaternion編集UI
- ボーン／スキニング
- 3Dデフォーマ
- カメラのPOI・被写界深度
