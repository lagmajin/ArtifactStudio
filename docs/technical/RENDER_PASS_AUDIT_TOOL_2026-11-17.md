# Render Pass Audit Tool

**最終更新:** 2026-11-17

## 目的

マルチパスレンダリングの「どのパスが宣言され、どれが実際に実行されているか」を静的に監査する。
ドキュメント間で RenderGraph の状態（`ADVANCED_RENDERING_GAP_2026-08-13.md` は「実行バックボーン未接続」、
`RENDERGRAPH_AUDIT_2026-08-05.md` は「診断は稼働」と後日訂正）が食い違っていた問題への、ソースを一次情報とする再現手段。

実装: [`tools/render_pass_audit.py`](../../tools/render_pass_audit.py)

## 実行

リポジトリルートで次を実行する。ビルド・CMake・テストは不要。

```bash
python tools/render_pass_audit.py
python tools/render_pass_audit.py --check          # 想定外の未実行パスがあれば exit 1
python tools/render_pass_audit.py --json
python tools/render_pass_audit.py --all-shaders    # 全 HLSL を列挙
python tools/render_pass_audit.py --output report.md
```

## 監査項目

1. `buildFrameRenderPassPlan()` が宣言する `FrameRenderPassKind` の順序
2. `FunctionalRenderPass` として生成されるパス（行番号・名前付き）
3. 宣言のみで executor パスを持たないパス
4. `runWithRenderGraph` / `runAllWithRenderGraph` / 直接 `run` の呼び出し件数
5. `RenderGraph` の使用形態（診断／順序検証／実行）。`executeParallel()` の有無を重点的に見る
6. `GIRenderGraphAdapter` / `PointwiseRenderGraphAdapter` の呼び出し元
7. C++ の dispatch 参照がない HLSL。`.vcxitems` / CMake に載っているだけの「コンパイル対象だが未配線」を区別する

## 2026-11-17 時点のベースライン

| 項目 | 結果 |
|---|---|
| 宣言パス | Setup → Base → Surface → Mask → Composite → Post → Overlay → Flush → Present |
| executor パス | Setup / Base / RAM Composite / Post(Resolve) / Direct Composite / Overlay / Present（計9箇所） |
| 宣言のみ | Surface / Mask（per-layer pipeline が直接呼び出し）、Flush（インライン計測） |
| executor 呼出 | `runWithRenderGraph` 7、`runAllWithRenderGraph` 1（RAM preview 2-pass）、直接 `run` 0 |
| RenderGraph | インスタンス2（診断 :24865、順序検証 :44245）。controller 内に `execute()` なし。executor の直列ラッパーのみ |
| アダプタ | 2つとも定義のみ・呼び出し元ゼロ |
| HLSL | 361 ファイル中 354 に C++ dispatch 参照なし。うち 334 はビルド一覧にのみ掲載 |

`Surface` / `Mask` / `Flush` は既知の直接実行ステージとして既定で除外する（`--known-inline` で変更可）。
`--check` はこの除外 set に含まれない未実行パスを検出した場合のみ失敗する。

## 制限事項

- 静的テキスト解析のため、実行時の分岐で到達しないパスや、リフレクション的に構築されるパスは判定できない。
- HLSL の「ホスト参照」はファイル名（stem）の一致で判定する。文字列連結で名前を組み立てる呼び出しは取りこぼす可能性がある。
- 本ツールは配線状況の可視化のみを行い、画質・性能の合否は判定しない。画像品質に関わる変更には
  [`IMAGE_EFFECT_RND_EVALUATION_RULES.md`](IMAGE_EFFECT_RND_EVALUATION_RULES.md) の評価計画が必要。

## 関連

- `docs/analysis/ADVANCED_RENDERING_GAP_2026-08-13.md`
- `docs/analysis/RENDERGRAPH_AUDIT_2026-08-05.md`
- `docs/analysis/RENDERGRAPH_IMPLEMENTATION_HANDOFF_2026-08-05.md`
- `docs/planned/MILESTONE_RENDER_THREAD_SEPARATION_FEASIBILITY_2026-10-16.md`
