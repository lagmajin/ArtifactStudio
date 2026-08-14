# 3Dレイヤー回転モデルの3軸化

**最終更新:** 2026-08-14

## 現状

- `AnimatableTransform3D` は既存のZ軸互換値に加え、X/Yの独立アニメーション値と3軸スナップショット値を保持する。
- `ArtifactAbstractLayer::setRotation3D(QVector3D)` は `rot.x()` のみを保存する。
- `AnimatableTransform3D` の共通行列生成はZ → Y → X順のEuler回転を適用する。`ArtifactAbstractLayer`側の値接続は未完了。
- `Artifact3DModelLayer` と `ArtifactProcedural3DLayer` もZ軸単一回転を直接適用する。
- JSON/UIには `rotationX` / `rotationY` / `rotationZ` が存在する箇所があるが、内部モデルと一貫していない。

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
