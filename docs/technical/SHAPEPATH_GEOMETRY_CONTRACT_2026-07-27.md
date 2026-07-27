# ShapePath ジオメトリ契約

**作成日:** 2026-07-27  
**状態:** Draft  
**関連マイルストーン:** `docs/planned/MILESTONE_SHAPE_PATH_NATIVE_RENDER_PIPELINE_2026-07-27.md`

## 目的

`ArtifactCore::ShapePath` を、Qt の `QPainterPath` を描画バックエンドとして扱うクラスではなく、描画器が利用する正規のパスデータモデルとして整理する。本書は Phase 0 の契約であり、実装 API の追加・変更をまだ行わない。

## 正規データ

パスは順序付きの `PathCommand` 列で表現する。

| コマンド | 使用点 | 意味 |
|---|---:|---|
| `MoveTo` | 1 | 新しいサブパスの開始 |
| `LineTo` | 1 | 現在位置から終点までの直線 |
| `QuadTo` | 2 | 制御点 1 個の二次 Bézier |
| `CubicTo` | 3 | 制御点 2 個の三次 Bézier |
| `Close` | 0 | サブパス始点へ閉じる |

`PathCommand` 列が正規値であり、`QPainterPath` は import／export／互換フォールバック用の境界表現とする。`QPainterPath` から変換した場合も、以後の編集・保存では `ShapePath` の command 列を正とする。

## サブパス規則

- `MoveTo` がサブパスを開始する。最初の `MoveTo` より前の描画コマンドは無効入力として扱う。
- `Close` は現在のサブパスを閉じ、次の `LineTo`／曲線は閉じた終点から継続しない。新しい描画は `MoveTo` から開始する。
- `Close` のないサブパスは開いたまま保持する。fill 時の暗黙の閉鎖と stroke 時の開放端は renderer 側の責務とする。
- 空パス、`MoveTo` だけのパス、有限値でない座標は、描画可能な geometry を生成しない。
- command の点数はコマンド種別の必要数を満たすものとし、欠落点は描画入力から除外する。

## 座標・精度

- パス座標はレイヤーのローカル座標系で保持する。
- transform はパスデータを破壊的に変える操作と、renderer が描画時に適用する行列を区別する。
- `double` の計算結果が最終 geometry に渡る際は有限値を確認し、NaN／無限大を頂点へ流さない。
- flatten tolerance は出力解像度、transform の最大拡大率、品質設定から決定する。固定ピクセル数を Core の正規データへ保存しない。
- tolerance は正数に正規化し、未指定または不正値は安全な既定値へ戻す。

## Geometry 生成の責務

Core 側は次の値を Qt 型なしで計算できることを目標とする。

- 線分・二次／三次 Bézier の評価
- 曲線の解析的または保守的な bounds
- tolerance に基づく flatten／tessellation 用の線分列
- サブパス境界、閉鎖状態、総延長、接線

fill rule、stroke width、join、cap、dash、stroke alignment は、パスの正規データと分離した描画属性として renderer 境界へ渡す。これらを `QPainterPath` への変換で暗黙に確定させない。

## 互換境界

次の用途では `QPainterPath` を許可する。

- 既存 SVG／Qt API との入出力境界
- 旧プロジェクトの読み込み互換
- 新経路で扱えない入力を明示的に切り替えるフォールバック
- 移行中の比較・診断

通常のシェイプ描画、レイヤー合成、マスク合成の新規実装では `QPainter` の CompositionMode や `QImage` キャッシュへ逃がさない。フォールバック時は理由を診断情報に残す。

## 次の実装で検証するケース

- 空パス、単一点、開いた線、閉じた矩形
- 水平／垂直／斜め線分
- 極端な制御点を持つ二次／三次 Bézier
- 複数サブパスと穴を含む fill
- 負のスケール、回転、非常に大きい拡大率
- NaN／無限大、ゼロ tolerance、ゼロ stroke width
- `QPainterPath` との bounds／flatten 結果の許容差比較

## 未決事項

- renderer が受け取る geometry packet の具体的な型名と所有権
- fill tessellation の winding／even-odd 実装場所
- stroke の join／cap／dash／alignment の対応範囲
- GPU 経路と専用 CPU フォールバックの頂点フォーマット

これらは既存 `ArtifactIRenderer` と Diligent backend の責務を確認した設計レビュー後に決定する。現段階で Qt 型を置き換えるための新規公開 API は追加しない。
