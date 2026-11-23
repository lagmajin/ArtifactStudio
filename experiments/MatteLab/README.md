# Matte Lab

**最終更新:** 2026-11-15
**ステータス:** rd0〜rd5 実行済み（classical baseline 4 種 + U2Net + MODNet + BiRefNet + 併用経路 2 種 + guided refinement 3 種）

静止画 1 枚から前景 alpha matte を取り出す品質を測る研究用 Python ラボです。GT alpha が厳密に既知の合成 fixture を使い、GrabCut 系の二値マスクが破綻する境界（髪・獣毛・細線・半透明・ソフトシャドウ）で、classical baseline と学習型マットモデルの差を切り分けます。ここで得た結果は製品実装・既定値・UI の承認ではありません。

製品コード（Artifact / ArtifactCore）は変更しません。Artifact バイナリは起動しません。

評価計画は [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md) に事前登録済みです。`docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従います。マイルストンは `docs/planned/MILESTONE_AI_MATTE_EXTRACTION_RD_2026-11-15.md`。

## 対象と非対象

**対象**

- 合成 fixture 8 種（アンチエイリアス円、髪状細線、ガラス、細ワイヤ、獣毛境界、ソフトシャドウ、グリーンスクリーン、CC0 実写合成）
- classical baseline: GrabCut（矩形）、GrabCut（GT trimap oracle）、chroma key、Otsu luma
- 学習型候補: U2Net（rd1）、MODNet（rd3）、BiRefNet 512（rd4）。RMBG 系はライセンス上見送り、未入手
- 併用経路（rd2）: U2Net の α から trimap を作り GrabCut へ渡す 2 変種（二値 / 未知帯のみ軟らかさ維持）
- 後処理（rd5）: guided filter（入力画像をガイド）による α refinement を各モデルの raw と並べて測定
- 指標: SAD / MSE / Gradient / Connectivity、境界帯 MAE、半透明 MAE、部分 α 回収率、合成誤差、runtime

**非対象（未検証として記録）**

- 動画・フレーム間伝播、temporal stability
- trimap 自動生成、Roto Brush の操作 UX
- 製品 GPU 経路、Composition Viewer、Render Queue、D3D12 / Vulkan parity
- OCIO / HDR、EXR、linear working space
- DirectML 推論、VRAM
- 実写素材の主観品質（GT matte 未整備）

## 実行

Python 3.10 以降。`numpy` / `opencv-python-headless` / `pillow` が必要です（`requirements.txt`）。学習モデルを使う場合のみ `onnxruntime` を追加します。

```powershell
python experiments/MatteLab/matte_lab.py --rd rd0
python experiments/MatteLab/matte_lab.py --rd rd1 --onnx u2net=models/u2net.onnx --onnx-preset u2net=u2net
python experiments/MatteLab/matte_lab.py --rd rd2 --onnx u2net=models/u2net.onnx --onnx-preset u2net=u2net --derive-trimap
python experiments/MatteLab/matte_lab.py --rd rd3 --onnx u2net=models/u2net.onnx --onnx modnet=models/modnet.onnx --onnx-preset u2net=u2net --onnx-preset modnet=modnet
python experiments/MatteLab/matte_lab.py --rd rd4 --onnx u2net=models/u2net.onnx --onnx modnet=models/modnet.onnx --onnx birefnet=models/birefnet_512.onnx --onnx-preset u2net=u2net --onnx-preset modnet=modnet --onnx-preset birefnet=birefnet
python experiments/MatteLab/matte_lab.py --rd rd5 --onnx u2net=models/u2net.onnx --onnx modnet=models/modnet.onnx --onnx birefnet=models/birefnet_512.onnx --onnx-preset u2net=u2net --onnx-preset modnet=modnet --onnx-preset birefnet=birefnet --refine
```

出力は `captures/<rd>/`（`manifest.json`, `metrics.json`, `gates.json`, レビュー PNG）。fixture は seed `20261115` で決定論生成します。`gates.json` には学習型候補のゲート判定（`verdicts`）も機械判定で入ります。

学習モデルは `models/` に配置します（リポジトリには含めません）。入手方法・ライセンス・入力契約は [`MODELS.md`](MODELS.md) を参照してください。前処理は候補名か `--onnx-preset` で `ONNX_PRESETS` のプリセットに紐付け、manifest に記録します。

## ディレクトリ構成

| パス | 内容 |
|---|---|
| `EVALUATION_PLAN.md` | 事前登録済みの評価計画 |
| `MODELS.md` | 学習モデルの出所・ライセンス・配置手順 |
| `matte_lab.py` | fixture 生成、classical baseline、ONNX 候補、指標、manifest 出力 |
| `models/` | 学習モデル（git 管理外） |
| `captures/` | 実行結果（rd0 / rd1 …） |

## 主な結果

rd0（8 fixture × 4 baseline、2026-11-15 実行）の全数値は `captures/rd0/metrics.json`、確定ゲートは `gates.json`。要点だけ残します。

**classical baseline は境界を表現できない**

- 二値マスク系（`grabcut_rect` / `grabcut_gt_trimap` / `otsu_luma`）は部分 α をほぼ 1 画素も返さない。`pred.frac_partial_5_95` は全 fixture で 0.000 か、trivial な AA 画素のみ。
- `hair_strands` の境界帯 MAE は `grabcut_rect` 0.672、GT trimap を与えた oracle でも 0.091。事前登録ゲート 0.05 に届かない。
- `soft_shadow`（接地影）も oracle 0.130、`glass_panel`（半透明板）は oracle 0.331。半透明ゲート 0.08 はどの baseline も通らない。
- 半透明 MAE は baseline 0.29〜0.64。α の連続値そのものを扱う手法が無いことの直接的な証拠。

**GrabCut が破綻するケース**

- `hair_strands` で `grabcut_rect` は矩形内のほぼ全域を前景に拡大した（予測 α 平均 0.480、GT 0.043、SAD 118.38）。細線主体の画像では矩形初期化が効かない。
- `glass_panel` も予測 α 平均 0.576（GT 0.105）で、半透明板を不透明塊として拾う。SAD 123.67。
- `disc_aa` / `fur_edge` のように塊がはっきりした fixture では `grabcut_rect` は良好（SAD 0.28 / 1.16）。失敗は素材依存で、一律に悪いわけではない。

**trimap oracle でも埋まらない差**

- `thin_wire`（SAD 110.43 → 7.64）、`hair_strands`（118.38 → 10.75）のように trimap は大きく効く。一方で境界帯 MAE は 0.09〜0.33 に留まり、「trimap さえ良ければ matte が正しくなる」とは言えない。
- `photo_composite` は oracle でも SAD 79.21、境界帯 MAE 0.221。実写テクスチャでは GrabCut の色モデルが境界で滑る。

**chroma key は適用可能な素材でのみ強い**

- `green_screen` では `chroma_key` が SAD 4.17 で最良の非 oracle（`grabcut_rect` 19.15、oracle 3.46）。ただし毛状の縁は α が 0/1 に寄り、部分 α 率 0.006 で軟らかさは失われる。
- テクスチャ背景の fixture では chroma key は大半が 0 になる。単色背景が無い素材に色キーを持ち込んでも分離できないことの記録であり、欠陥ではなく適用範囲の確認。

**コスト**

- GrabCut は 512×512 で 1.2〜7.6 s、640×640 で 9.1 s（5 iteration、CPU 単一プロセス）。chroma / Otsu は 1.3〜13 ms。学習モデルの比較対象としては GrabCut がすでに重い。
- GrabCut は `cv2.setRNGSeed` を呼び出しごとに固定したため、再実行で指標は完全一致する（変動するのは timing のみ）。

## rd1: U2Net の実測

`captures/rd1/`（8 fixture × 5 候補、`u2net` は 320×320 固定入力・CPU 単一スレッド）。ゲート判定は `gates.json` の `verdicts.u2net`。**8 fixture すべてで不合格**（合格 0 / 8）。

| fixture | SAD rect → u2net | 境界帯 MAE rect → u2net | 部分 α 回収 rect → u2net |
|---|---|---|---|
| `disc_aa` | 0.28 → 0.94 | 0.024 → 0.079 | 0.197 → 0.276 |
| `hair_strands` | 118.38 → 14.11 | 0.672 → 0.101 | 0.071 → 0.290 |
| `glass_panel` | 123.67 → 44.05 | 0.623 → 0.568 | 0.000 → 0.004 |
| `thin_wire` | 110.43 → 18.44 | 0.373 → 0.216 | 0.173 → 0.176 |
| `fur_edge` | 1.16 → 1.26 | 0.085 → 0.092 | 0.193 → 0.206 |
| `soft_shadow` | 42.82 → 10.08 | 0.459 → 0.140 | 0.052 → 0.366 |
| `green_screen` | 19.15 → 4.83 | 0.659 → 0.160 | 0.008 → 0.203 |
| `photo_composite` | 176.54 → 137.56 | 0.448 → 0.376 | 0.129 → 0.154 |

（部分 α 回収は GT 部分 α 画素のうち ±0.1 以内の率。ゲート 0.50 は `hair_strands` / `fur_edge` に適用）

**U2Net の SAD 改善は「細線を拾った」ではなく「ほぼ空マスクを返した」**

- `hair_strands` の予測 α 平均は 0.015（GT 0.043）、`thin_wire` は 0.011（GT 0.078）、`photo_composite` は 0.013（GT 0.346）。SAD が下がったのは前景を取り逃して 0 に近づいたためで、境界帯 MAE は 0.10〜0.38 に留まる。
- 視覚的にも `review/hair_strands/u2net.png` の予測 α はほぼ黒で、髪状の線は復元されていない。saliency モデルの出力であって matte ではない。

**U2Net が実際に機能した箇所**

- `soft_shadow`: 予測 α 平均 0.163（GT 0.199）、境界帯 0.140、灰色背景の合成誤差 mean 27.6 → 8.4。接地影の減衰を部分的に表現できている。
- `green_screen` / `disc_aa` / `fur_edge`: 予測 α 平均が GT に近い（0.210 / 0.347 / 0.270 対 GT 0.222 / 0.344 / 0.266）。塊状の被写体の分離は安定。
- `glass_panel`: 半透明板を不透明寄りに拾い（予測平均 0.272 対 GT 0.105）、半透明 MAE 0.607。ゲート 0.08 には遠い。

**ゲート判定（`verdicts.u2net.summary`）**

- 不合格ゲートの内訳: 境界帯 MAE 8/8、半透明 MAE 8/8、SAD 40% 改善 3/8、gradient 2/8、connectivity 3/8、合成誤差 2/8、部分 α 回収 2/2。
- 塊状 fixture（`disc_aa` / `fur_edge`）では GrabCut がすでに SAD 0.28 / 1.16 と良く、U2Net はそこを悪化させる。saliency モデルは「GrabCut が既に成功している画」に上積みしない。
- 320×320 固定入力のため、512×512 fixture では境界が resample でぼける。境界帯 MAE の下限は入力解像度に律速されている可能性がある（未検証）。

**コスト**

- U2Net は 0.92〜0.98 s / 枚（320×320、CPU 1 スレッド）。GrabCut（1.2〜9.1 s）より速い。決定論的で、同一プロセス内の再推論は出力が完全一致（α の SHA-256 一致）。

**結論（rd1 時点）**

- 「学習型 saliency モデル 1 本を入れれば髪・半透明の matte が得られる」は支持されない。U2Net は二値寄りの前景分離で、部分 α 回収は 0.29 / 0.21 でゲート 0.50 に届かない。
- 次段（rd2）で意味があるのは、(a) matte 専用モデル（MODNet / BiRefNet / RMBG 系）との比較、(b) 入力解像度を上げた変種、(c) saliency 出力を trimap として GrabCut に渡す併用経路、の 3 つ。いずれも未実施。

## rd2: saliency → trimap → GrabCut 併用経路

`--derive-trimap` で、U2Net の α から trimap を作り GrabCut に渡す 2 候補を追加した（`captures/rd2/`）。

- `u2net_trimap_grabcut`: `α≥0.90` を 2px 収縮して確実前景、`α≤0.10` を 2px 膨張して確実背景、残りを未知として `GC_INIT_WITH_MASK`。出力は二値。
- `u2net_trimap_soft`: 同じ GrabCut マスクを使い、未知帯だけモデルの α を残す。

| fixture | SAD rect / u2net / hybrid / hybrid-soft | 境界帯 MAE rect / hybrid / hybrid-soft | 確実前景率（trimap） |
|---|---|---|---|
| `disc_aa` | 0.28 / 0.94 / 0.34 / 0.40 | 0.024 / **0.029** / 0.034 | 0.333 |
| `hair_strands` | 118.38 / 14.11 / 11.22 / 11.26 | 0.672 / 0.095 / 0.095 | 0.000 |
| `glass_panel` | 123.67 / 44.05 / 44.01 / 43.93 | 0.623 / 0.568 / 0.567 | 0.258 |
| `thin_wire` | 110.43 / 18.44 / 20.29 / 18.69 | 0.373 / 0.237 / 0.219 | 0.0001 |
| `fur_edge` | 1.16 / 1.26 / 1.11 / **0.92** | 0.085 / 0.081 / **0.068** | 0.257 |
| `soft_shadow` | 42.82 / 10.08 / 10.09 / 10.13 | 0.459 / 0.142 / 0.143 | 0.153 |
| `green_screen` | 19.15 / 4.83 / **4.18** / 4.44 | 0.659 / 0.153 / 0.158 | 0.198 |
| `photo_composite` | 176.54 / 137.56 / 137.94 / 137.90 | 0.448 / 0.378 / 0.377 | 0.003 |

**分かったこと**

- **モデルが実際に前景を出している画では、GrabCut が退行を戻す。** `disc_aa` は SAD 0.94 → 0.34、境界帯 MAE 0.079 → 0.029 でゲート 0.05 を満たす。`fur_edge` も soft 版で SAD 1.26 → 0.92、境界帯 0.092 → 0.068。saliency 出力を trimap として使う経路自体は機能する。
- **モデルが空マスクを返す画では、併用経路も空のまま。** `hair_strands` / `thin_wire` / `photo_composite` の確実前景率は 0.000 / 0.0001 / 0.003 で、GrabCut に前景シードが無い。SAD 11.22 / 20.29 / 137.94 は U2Net 単体とほぼ同じで、改善ではない。退化の継承であり、ガードが無いと危険。
- **「未知帯だけモデルの α を残す」はほぼ無効。** U2Net の出力は飽和しており、未知帯は全画素の 0.8〜1.9% しかない。soft 版と二値版の指標は `disc_aa` 以外ほぼ一致する。saliency モデルの出力に matte の軟らかさは無い。
- **GrabCut が既に最良の画では、40% 改善ゲートは原理的に満たせない。** `disc_aa`（rect 0.28）は hybrid 0.34 で SAD ゲート不合格。改善幅ではなく「退行しないこと」を見るゲートが別途必要な領域である。
- 半透明は依然として全候補が不合格（半透明 MAE 8/8、`glass_panel` 0.567）。trimap を良くしても、α の連続値を出力する機構が無い。
- 実行時間は GrabCut 併用で 1.0〜1.3 s / 枚（U2Net 単体 0.85〜0.88 s）。再実行で指標一致。

**併用経路の結論:** モデル単体より悪化しないための安全弁としては有効だが、モデルが失敗している素材を救うものではない。実運用では「モデル出力の前景面積が閾値未満なら併用をやめる」等のガードが必須（未実装・未検証）。

## rd3: matte 専用モデル MODNet との比較

`captures/rd3/`（u2net と modnet を同一実行で比較、8 fixture）。MODNet は 512×512 動的入力、0.30〜0.33 s / 枚。

| fixture | SAD rect / u2net / modnet | 境界帯 MAE rect / u2net / modnet | 半透明 MAE modnet | 部分 α 回収 modnet |
|---|---|---|---|---|
| `disc_aa` | 0.28 / 0.94 / **0.21** | 0.024 / 0.079 / **0.017** | **0.075** | **0.693** |
| `hair_strands` | 118.38 / 14.11 / 11.22 | 0.672 / 0.101 / 0.095 | 0.358 | 0.263 |
| `glass_panel` | 123.67 / 44.05 / 68.99 | 0.623 / 0.568 / 0.592 | 0.614 | 0.000 |
| `thin_wire` | 110.43 / 18.44 / 20.21 | 0.373 / 0.216 / 0.236 | 0.534 | 0.171 |
| `fur_edge` | 1.16 / 1.26 / 6.90 | 0.085 / 0.092 / 0.097 | 0.197 | 0.349 |
| `soft_shadow` | 42.82 / 10.08 / 52.13 | 0.459 / 0.140 / 0.166 | 0.355 | 0.360 |
| `green_screen` | 19.15 / 4.83 / 38.45 | 0.659 / 0.160 / 0.214 | 0.397 | 0.208 |
| `photo_composite` | 176.54 / 137.56 / 136.33 | 0.448 / 0.376 / 0.372 | 0.421 | 0.163 |

**MODNet は初めて「軟らかい α」を出した**

- `disc_aa` で境界帯 MAE 0.017、部分 α 回収 0.693、半透明 MAE 0.075。**事前登録ゲートのうち境界帯 0.05 と半透明 0.08 を、このラボで初めて満たした**（`verdicts.modnet` の `checks` を参照）。レビュー画像でも AA 縁の α が連続値で再現されている。
- 不合格は SAD 40% 改善（0.28 → 0.21 で 27% 改善）と connectivity（0.022 → 0.050）。GrabCut が既にほぼ完璧な画で「40% 改善」を要求するゲートの設計問題が再確認された。

**MODNet は人物ドメインに閉じている**

- `hair_strands`（予測 α 平均 0.000）、`soft_shadow`（0.000）、`green_screen`（0.076）はほぼ空。人物 matting 用の学習分布から外れた被写体では、saliency モデルよりさらに極端に「何も出さない」。
- `fur_edge` は SAD 1.16 → 6.90、`green_screen` は 19.15 → 38.45 と、GrabCut より悪化する画がある。ドメイン外で無条件に使うと退行する。
- `glass_panel` の半透明 MAE 0.614 で、半透明板は依然として不可。

**rd3 の結論**

- 「matte 専用モデルなら軟らかい α が出る」は、**輪郭が閉じた単純形状では支持された**（`disc_aa`）。ただし本ラボの素材は人物ではないため、MODNet 本来の適用範囲での性能は測れていない。
- ゲート合格は依然 0/8 fixture（u2net 0/8、modnet 0/8）。合格 fixture が出ない主因は「SAD 40% 改善」「部分 α 回収 0.50（髪・獣毛）」「半透明」の 3 つで、いずれも単一モデルでは埋まらない。
- 次段は、(a) 人物・実写 fixture の追加（MODNet 本来のドメイン）、(b) BiRefNet / RMBG 系の高精度モデル、(c) `disc_aa` 型の成功を他の形状へ広げられるかの切り分け（解像度・輪郭の複雑さ）。

## rd4: BiRefNet 512 の比較（一般物体の dichotomous segmentation）

`captures/rd4/`（u2net / modnet / birefnet を同一実行で比較）。BiRefNet は `BiRefNet_512x512-ONNX`（swin-large、940 MB、MIT、512×512 固定）、0.63〜0.70 s/枚 × 8 fixture。

| fixture | SAD rect / u2net / modnet / birefnet | 境界帯 MAE rect / birefnet | 部分 α 回収 birefnet | 半透明 MAE birefnet |
|---|---|---|---|---|
| `disc_aa` | 0.28 / 0.94 / 0.21 / **0.13** | 0.024 / **0.011** ✓ | **0.546** ✓ | 0.119 |
| `fur_edge` | 1.16 / 1.26 / 6.90 / **0.68** | 0.085 / **0.050** ✓ | 0.358 | 0.195 |
| `thin_wire` | 110.43 / 18.44 / 20.21 / **7.21** | 0.373 / 0.084 | 0.348 | 0.243 |
| `hair_strands` | 118.38 / 14.11 / 11.22 / **9.69** | 0.672 / 0.082 | 0.345 | 0.231 |
| `soft_shadow` | 42.82 / 10.08 / 52.13 / **9.70** | 0.459 / 0.140 | 0.360 | 0.355 |
| `green_screen` | 19.15 / 4.83 / 38.45 / **3.11** | 0.659 / 0.116 | 0.316 | 0.228 |
| `glass_panel` | 123.67 / 44.05 / 68.99 / **39.83** | 0.623 / 0.514 | 0.028 | 0.552 |
| `photo_composite` | 176.54 / 137.56 / 136.33 / **126.77** | 0.448 / 0.344 | 0.201 | 0.379 |

**分かったこと**

- **BiRefNet は 3 モデル中もっとも広く効く。** SAD は 7/8 で `grabcut_rect` 比 40% 以上改善（唯一の例外は `photo_composite` の 28%）。`disc_aa`（SAD 0.13）と `fur_edge`（0.68）は境界帯 MAE ゲート 0.05 も満たす。
- **軟らかさは改善するが、目標値には届かない。** 部分 α 回収は `disc_aa` 0.546、`fur_edge` 0.358、`hair_strands` 0.345 で、髪・獣毛の 0.50 は未達。予測 α は依然 0/1 に寄る。
- **半透明はどのモデルでも不可。** `glass_panel` の半透明 MAE は 0.552（ゲート 0.08）。GT trimap を与えた GrabCut oracle でも 0.331 なので、この fixture とゲート値の組み合わせは現状の手法では原理的に到達しにくい。
- 3 モデルとも fixture 合格は 0/8。BiRefNet は SAD と境界帯の 2 ゲートを半数の fixture で満たすが、半透明と部分 α 回収で落ちる。
- コストは 0.63〜0.70 s / 枚。U2Net（0.84 s）とほぼ同オーダーだが、出力の質は明確に上。GrabCut は 1.1〜8.3 s。

**rd4 の結論**

- 「saliency モデルより BiRefNet は matte に近い」は支持される。ただしゲート全体を通すには、半透明 α を扱えるモデル（または半透明 fixture をゲート対象から外す設計判断）が必要。
- 事前登録ゲートのうち半透明 MAE 0.08 は、`glass_panel`（0.55）と `photo_composite`（0.38）で全モデルが外す。次段では (a) 半透明専用モデル、(b) 半透明 fixture の α 分布を緩めた現実的なゲート値、(c) α を別経路で推定する併用、のいずれかを設計判断として決める必要がある。

## rd5: guided filter による α refinement

`--refine` で `<model>_refined`（入力画像をガイドにした guided filter、radius 2、eps 1e-4）を 3 モデル分追加（`captures/rd5/`）。前処理の異なる 3 モデルを同一条件で比較するため、u2net / modnet / birefnet の raw と refined を並べて記録した。

| fixture | birefnet SAD → refined | birefnet 境界帯 → refined | birefnet 部分 α 回収 → refined |
|---|---|---|---|
| `hair_strands` | 9.69 → **7.46** | 0.082 → **0.063** | 0.345 → **0.464** |
| `thin_wire` | 7.21 → **5.11** | 0.084 → **0.060** | 0.348 → **0.386** |
| `green_screen` | 3.11 → **2.24** | 0.116 → **0.084** | 0.316 → **0.395** |
| `disc_aa` | 0.13 → 0.19 | 0.011 → 0.016 | 0.546 → 0.326 |
| `fur_edge` | 0.68 → 0.78 | 0.050 → 0.057 | 0.358 → 0.306 |
| `glass_panel` | 39.83 → 39.87 | 0.514 → 0.514 | 0.028 → 0.028 |
| `soft_shadow` | 9.70 → 9.69 | 0.140 → 0.140 | 0.360 → 0.360 |
| `photo_composite` | 126.77 → 126.73 | 0.344 → 0.344 | 0.201 → 0.202 |

| fixture | u2net SAD → refined | u2net 境界帯 → refined |
|---|---|---|
| `disc_aa` | 0.94 → **0.34** | 0.079 → **0.027** |
| `fur_edge` | 1.26 → **1.12** | 0.092 → **0.082** |
| `thin_wire` | 18.44 → 17.97 | 0.216 → 0.210 |
| `green_screen` | 4.83 → 4.25 | 0.160 → 0.152 |

**分かったこと**

- **低解像度モデルの α は refinement で大きく回復する。** U2Net（320 入力）は `disc_aa` の SAD が 0.94 → 0.34、境界帯が 0.079 → 0.027 になり、RAW の劣化が「モデルの予測ミス」ではなく「バイリニア拡大によるぼけ」だったことを示す。
- **高解像度モデルでは一様に効かない。** BiRefNet は hair / thin_wire / green_screen で改善（SAD 最大 29% 減、部分 α 回収 0.345 → 0.464）するが、`disc_aa` 0.13 → 0.19、`fur_edge` 0.68 → 0.78 と既に良い境界を壊す。SAD ゲートの不合格数が 1 → 3 に増えた。
- **eps は 0〜1 スケールでは効かない。** radius 2 / 4 / 8 と eps 1e-6 / 1e-4 / 1e-2 をスイープしたが、eps による差は観測されなかった（`var_g` が eps より常に大きい）。radius は 2 が最良で、8 では劣化する。
- **半透明は不変。** `glass_panel` は raw / refined とも 0.514 で、guided filter は半透明 α を推定しない。
- ゲート合格は 6 候補すべて 0/8。refinement で合格 fixture は増えないが、SAD・境界帯・部分 α 回収の 3 指標で「raw より良い方向」の fixture は U2Net 5/8、BiRefNet 3/8。

**rd5 の結論:** guided filter は「モデルの解像度不足によるぼけ」を安価に補償するが、「モデルが構造を取り違えている画」は改善せず、既に正しい境界を壊す。実運用では raw と refined を出し分けるか、エッジ活動量などで選択する分岐が必要（未実装）。

## 免責

研究用の測定値であり、製品の画質・性能・既定値を保証しません。実写素材は `experiments/GlowLab/assets/SOURCES.md` の CC0 記録に従います。数値の改善を視覚品質の証明として扱いません。