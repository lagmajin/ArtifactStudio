# SoftBody GPU UV Deformation Progress

> 2026-07-20

`SoftBodyUVVertex` を GPU vertex stage で読むための独立 shader contract を追加した。

- `softBodyUVDeformVS.hlsl`
- solver が返す position / normalized UV を StructuredBuffer で読む
- local-to-clip transform は constant buffer から受け取る
- Image / Video texture の sample は fragment stage 側に残す
- `getGridTriangleIndices()` で stable triangle topology を取得できる
- `ArtifactAbstractLayer::softBodyDeformationMesh()` で layer 側から read-only packet を取得できる

未対応:

- `ArtifactIRenderer` / Diligent PSO への登録
- vertex buffer upload と lifetime 管理
- quad index / triangle topology の生成
- Image / Video layer への接続
- preview / render 共通 packet 化
- 実 GPU 検証
