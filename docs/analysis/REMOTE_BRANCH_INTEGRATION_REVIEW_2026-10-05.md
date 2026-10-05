# 開発ブランチ取り込みレビュー

**最終更新:** 2026-10-05

## 対象と方法

fetch 後の最新リモートブランチは `origin/codex/2026-10-02-dev`、先端は `772e1bfd`（2026-10-05 16:00:53 JST）。親の9コミット、Artifact の4コミット、ArtifactCore の2コミットを個別に確認した。比較元は親 `71cb541c`、Artifact `bdfebd22`、ArtifactCore `a97735bb`。いずれも取り込み先の祖先であり、fast-forward で履歴を維持した。今回のギズモ修正は既に取り込み元の履歴に含まれ、上書きや競合解消は不要だった。

差分に加え、画像 copy/move/SetCpuImage、pointwise Clamp、GPU generator の入力契約、キーの保存復元・snapshot command の直接実装を読んだ。実行結果や性能向上を確認したという意味ではない。

## 個別判断

| 親コミット | 実装・内容 | 判断と根拠 |
|---|---|---|
| `d2f39485` | Core `4a05899c`: cache wrapper の swap | 採用。Impl pointer を交換し、画素・GPU cache・dirty state を一体で移す。深い代入コピーを避ける。 |
| `f47a00de` | Artifact `353041a3`: effect stage の swap | 採用。current/next を別 wrapper のまま交互に使う。既存の最終出力境界は維持する。 |
| `040a1418` | Artifact `1fd7149f`: CPU consumer の capability 分離 | 採用。cpuImpl pointer に依存しない3種の CPU 実装を明示する。CPU mode は RAII で復元し、GPU-only は従来 configured 経路を保つ。CPU/GPU の数値一致は未検証。 |
| `a1181828` | CPU 境界の設計補足・Insight | 訂正して採用。画像の通常コピーは deep copy。3種の複製数は4回から2回であり、shallow / 1回という説明は誤りだった。 |
| `640a9e10` | Artifact `2086f6ca`: history / intermediate / allocation 削減 | 採用。出力 clone 後の CPU 画像を履歴へ移譲し、sample は独立コピー。revision/eviction を維持する。入力を読まない generator だけ直接 UAV 出力。pointwise は別 read/write texture を交互利用し、必要時だけ最終 copy。重複領域の source snapshot は通常 copy 自体が deep copy なので安全性の契約を維持する。 |
| `5d5744b7` | 性能修正の所有権・制限の記録 | 採用。履歴 map/list、画像 move の1x1再初期化、追加マスク workspace 初回確保、動的 profiling 設定参照は残る。ゼロアロケーション達成とは扱わない。 |
| `e453c00b` | Core `60fedbfa`: soft metadata | 採用。soft は既定 false の編集 metadata。保存 bridge、transform のキー復元に伝播し、補間評価は変えない。 |
| `3f09f1d9` | Artifact `426be62f`: soft key UI と resident adjustment | 採用。soft の追加/確定/破棄は既存 snapshot command を使い、通常キーは破棄しない。Undo・clipboard・text/deformation の保存経路にも属性を伝播。ChannelMixer の Clamp は RGB のみで alpha を変えない。Blur は全 stack の条件を先に確認し、GPU snapshot による rollback を持つ。対象外条件は従来経路を維持する。 |
| `772e1bfd` | soft key 仕様・GPU eligibility の記録 | 採用。プレビューにも仮キーを反映する仕様、旧版再保存の制限、GPU の限定条件と未検証事項を記載している。 |

## 維持した範囲と確認残

既存の未コミット設定・一時ファイルは保持し、取り込みに含めない。ArtifactWidgets と DiligentEngine の gitlink は変更しない。新しい module/CMake 登録、signal/slot、描画バックエンドは追加しない。既存の標準コンテナは値の型だけ変更する箇所があり、機械的な置換はしていない。追加マスクの再利用 buffer は既存独自 `Array<float>` を使う。

ビルド・CMake・テスト・実機確認はユーザーの明示指示がないため未実施。以下は受入れ確認が残る。

- GPU/CPU 混在 stack、mask/Mix、入力出力 alias、mode 復元、HDR/半透明の出力比較。
- 履歴の named input / 時間参照、seek/逆再生、revision 無効化、容量超過時の eviction。
- Generator 後の Filter、pointwise の奇数/偶数 segment、Blur 成功/途中失敗時の背景復元、D3D12/Vulkan resource state。
- Soft/通常キー混在、追加/確定/破棄、Undo/Redo、移動/複製、保存再読込、Text Animator / text / deformation。
- 既存 TextGizmo の press/move/release/cancel と soft metadata 保持。
- 追加マスク初回 cache 構築に残る RGBA32F workspace 確保と、保持メモリ・copy量の実測。
