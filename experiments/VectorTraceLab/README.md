# VectorTraceLab

**最終更新:** 2026-11-20

画像→ベクター変換（Image-to-Vector / Tracing）R&D のオフライン実験資産。

親マイルストン: `docs/planned/MILESTONE_IMAGE_TO_VECTOR_RD_2026-11-20.md`
評価計画: [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md)

## 目的

既知のベクタ定義から生成した合成ロゴ／イラストを入力に、色面分解と輪郭近似の品質を
決定論的 fixture と事前登録ゲートで評価する。製品コード（ArtifactCore / Artifact / ArtifactWidgets）は変更しない。

## ファイル

| ファイル | 役割 |
|---|---|
| `EVALUATION_PLAN.md` | 事前登録済みの評価計画（開始前に作成済み） |
| `generate_fixtures.py` | 決定的 fixture 生成器（seed 20261120、SS=4、sRGB 8-bit、straight alpha） |
| `vector_reference.py` | 正解ベクタの保持、ラスタライズ、制御／baseline 方式、指標計算 |
| `run_rd0.py` | R0 実行: fixture 生成 → identity チェック → baseline 計測 → manifest/metrics/gates/review 出力 |
| `RESEARCH_NOTES.md` | 実行所見（実行後に追記） |
| `requirements.txt` | Python 依存 |
| `.gitignore` | 生成物の ignore 規則 |
| `captures/rd0/` | R0 の証跡（既定 ignore。manifest / metrics / gates は選んで追跡） |

## 実行

ビルド・CMake・製品テスト・実アプリ起動は不要（ユーザーの明示指示までは行わない）。

```bash
python generate_fixtures.py --out captures/rd0/fixtures
python run_rd0.py --out captures/rd0
```

生成されるレビュー画像は目視用であり、測定データではない。数値は `metrics.json`、条件は `manifest.json`、
ゲートは `gates.json` を参照する。

## 現状

- R0 (rd0): 未実施（fixture 生成・baseline 取得はこれから）
- R1 以降: 未実施

## 注意

- `captures/` 配下の PNG / NPY / PFM / EXR 等はルート `.gitignore` で既定除外。
  最終証跡だけを条件・取得日・出所・hash 付きで個別 `git add -f` する。
- blind 割当て鍵はレビュー完了まで追加しない。
