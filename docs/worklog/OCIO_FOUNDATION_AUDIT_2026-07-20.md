# OpenColorIO 基盤監査メモ (2026-07-20)

## 結論

OpenColorIO / ACES の基盤は既に実装済みであり、新しい OCIO 管理クラスを追加しない。

既存の責務は次のとおり。

- `Color.OCIOConfig`: config JSON、roles、color spaces、display/view、built-in ACES / sRGB / Rec.709 / Rec.2020 preset
- `ArtifactOCIOManager`: config lifecycle、working space、display/view/looks の選択、永続化、ColorScienceManager との同期
- `ArtifactColorManagement`: `ColorSpace`、LUT、color grading の CPU 側処理
- `ArtifactCore::ImageF32x4_RGBA`: CPU fallback の明示的な画像境界

## 現在の位置づけ

現在の OCIO 実装は「外部 OpenColorIO ライブラリが無い場合にも動く簡易フォールバック」である。
`ArtifactOCIOManager::applyViewTransformToImage()` は color-space matrix による CPU 変換を行うが、OCIO の実際の View Transform / Look / Display の完全な評価ではない。

したがって、既存基盤を production pipeline として拡張する場合の次の作業は、管理 API の重複ではなく以下に限定する。

1. preview / export の render boundary で active OCIO config と view を適用する経路を確定する
2. scene-linear / display-referred の境界と premultiplied alpha の扱いを共通化する
3. 必要になった時点で、外部 OCIO の Processor を使う backend を既存 manager の内部実装として追加する
4. 外部 backend が無い場合は、現在の built-in preset / matrix fallback を維持する

## 実装上の注意

- `ArtifactColorManagement` と `ArtifactOCIOManager` を並列に増やさず、OCIO manager を config/view の source of truth とする
- `ImageF32x4_RGBA` の CPU fallback をレンダリング本流へ広げない
- Qt の `QImage` / `QPainter` 合成へ戻さない
- View Transform と単純な色域変換を同一視しない
- 実際の OpenColorIO ライブラリ導入時も、設定・Processor の所有権は `ArtifactOCIOManager` 内に閉じる

## ライセンス

OpenColorIO を直接リンク・同梱する段階では、採用バージョンの LICENSE と NOTICE を確認し、`docs/THIRD_PARTY_NOTICES.md` に追記する。今回の変更は既存実装の監査メモのみで、外部コードや外部ライブラリは追加していない。
