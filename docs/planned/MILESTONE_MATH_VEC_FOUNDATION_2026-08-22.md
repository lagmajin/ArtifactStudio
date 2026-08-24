# MILESTONE: 公式数学型基盤 Math.Vec（glm ラップ）— QVector 排除 Phase 0/1

**最終更新:** 2026-08-22

## 目的

基礎クラス監査（2026-08-22）で判明した「Vec3 6 系統乱立・本命不在」問題の到達点として、glm を基盤にした ArtifactCore 公式数学型 `Math.Vec` を確立する。QVector3D（475 箇所/61 ファイル）排除の Phase 0（型確立）＋Phase 1（変換ユーティリティ）。

## 採用決定

- **方式**: glm 直接エイリアス（ラップ構造体なし）。全 glm API・SIMD 最適化をそのまま使用でき変換コストゼロ
- **バージョン**: glm 1.0.3 (vcpkg, header-only)。`glm::glm` は ArtifactCore に PUBLIC リンク済みで CMake 追加は不要
- **規約**: column-major / right-handed / float 既定。命名は小文字 glm 流 (`vec2/vec3/vec4/mat3/mat4/quat` + `dvec*/dmat4`)
- **Qt 変換**: 明示関数のみ（toQVector3D/toVec3/toQMatrix4x4 等）。暗黙変換は意図的に提供しない

## 実装内容 (2026-08-22)

| ファイル | 内容 |
|---|---|
| `ArtifactCore/include/Math/Vec.ixx` (新規) | モジュール `Math.Vec`。公式型エイリアス＋監査ギャップ補完（safeNormalize ゼロ除算対策/distanceSq/perComponentMin·Max·Abs/clampComponents/isFinite/epsilonEqual）＋Qt 境界変換。header-only。include はすべて GMF |
| `ArtifactCore/include/Graphics/Vector3D.ixx` (削除) | import ゼロの死型 Vec3（RayTrace 版とは別物）。**module purview 内 `import Math.Quaternion;` という AGENTS.md 違反も含んでいたため削除で解消**。`ArtifactCore::Vec3` の名前空間名が解放された |
| `ArtifactCore/cmake/ArtifactCoreSources.cmake` | Vector3D.ixx 登録削除／Math/Vec.ixx 追加 |
| `ArtifactCore/include/Utils/VectorLike.ixx` | 未使用 glm/QVector/opencv include 除去（concept のみに縮小） |
| `ArtifactCore/include/Core/Point2D.ixx` | 未使用 `<glm/glm.hpp>` 除去 |
| `Artifact/src/Service/ArtifactProjectService.cppm` | 未使用 `<glm/ext/matrix_projection.hpp>` 除去 |
| `tests/ArtifactCore/MathVecTest.cpp` (新規) + CMake 登録 | 数学系テスト第 1 号: エイリアス static_assert/safeNormalize(ゼロ・微小ベクトル)/distanceSq/成分演算/isFinite(NaN,Inf,行列)/epsilonEqual/Qt 往復(QVector3D, QMatrix4x4 数値一致含む)/QPointF |

## 移行ポリシー（今後のコード規約）

1. **新規コードは `import Math.Vec;` の型を使用**。Qt 型は UI/paintEvent 境界のみ
2. 既存 QVector3D は「触ったファイルから」toVec3()/toQVector3D() 経由で段階置換（一括置換しない）
3. Qt 変換は本モジュールの明示関数経由に集約（各ファイルでの成分直書きを禁止）

## 未検証事項（ビルド待ち）

J:\dev\ArtifactStudio 側には有効な build dir がないため今回も未検証:

1. ArtifactCore ビルド（Math.Vec 登録・Vector3D 削除の manifest 整合）
2. MathVecTest 実行（特に QMatrix4x4↔mat4 の column-major 往復一致）
3. check_module_hygiene 通過

## 次フェーズ候補（スコープ外）

- Camera/TransformHelper のシグナルを Math.Vec 経由に寄せる
- QVector3D ホットパス移行（アニメ評価/パーティクル/クローナーから、サブシステム単位）
- 自作 Matrix4x4(row-major) と glm mat4 の統合、Math.Quaternion の接続（toMatrix4x4 欠落解消）
- RayTrace::Vec3 の Math.Vec 移行
- 別タスク推奨: Point2DF/Rotation の rule-of-five 修正（二重解放リスク）
