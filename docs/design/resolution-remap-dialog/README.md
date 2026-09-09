# Resolution Remap dialog design reference

**最終更新:** 2026-09-09

`resolution-remap-dialog-v1-2026-09-09.png` は、現行 `ArtifactResolutionRemapDialog` の情報階層を整理する実装参照である。

- 旧解像度と新解像度、アスペクト比の差を最上段で示す。
- 左側の図形表示は旧／新フレームの比率比較だけに限定する。
- Impact は既存の mask path、keyframe track、anchor point の件数を示す。
- Remap Policy は既存の5種類だけを表示する。
- GPU preview、画像生成、readback、追加の補正機能は導入しない。
- QtCSS、新規 signal / slot 接続、`QImage` の追加根拠にはしない。

## 実装状況

Version 1 の二列比較、Impact、5ポリシー一覧、警告と主要操作の階層を `ArtifactResolutionRemapDialog` に反映済み。既存の remap 結果と Apply／Skip 経路は維持している。
