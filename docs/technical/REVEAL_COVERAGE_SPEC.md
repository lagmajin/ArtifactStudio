# 共通 Reveal / Coverage 仕様書

**最終更新:** 2026-10-01

**状態:** 実装反映中（ソース実装あり、ビルド・実機検証未実施）

## 目的

静止画、連番画像、Text、Shape に共通の表示進行機能を提供する。既存の Opacity は全体の透け具合として維持し、独立した Progress が表示領域を制御する。ブラシで塗る、左から現れる、中心から広がる、粒子状に現れる演出を同じ契約で扱う。

本書と [実装計画書](../planned/MILESTONE_REVEAL_COVERAGE.md) は独立して読める一組の文書である。

## 参照と優先順位

ユーザー添付の `a02ac19f-100d-4a11-bd34-fe9dbb07da69.webp`（Text / Shape 共通化）と `ea56a8a0-42b2-4713-b39c-63868ac13c2a.webp`（ブラシ Reveal）の概念を基にする。参照画像はユーザーの Downloads にあり、リポジトリには未収録。本書は画像なしで解釈できるよう契約を記載する。

画像内の擬似コードは説明資料である。2枚目の「透明度を進行度として使う」は、1枚目の独立した Opacity / Coverage に合わせ、専用 Progress として採用する。添付画像の UI、文字、背景、コードをそのまま実装する指示とは扱わない。

## 用語とデータ

| 用語 | 契約 |
|---|---|
| Source | レイヤーの既存描画結果。Text の字形、Shape の Fill / Stroke、画像 alpha を含む |
| Opacity | 既存のレイヤー不透明度。Reveal から独立して評価する |
| Progress `p` | 0〜1 のアニメーション可能な表示進行度。UI は 0〜100% |
| Timing map `T(x,y)` | 0〜1 の表示開始順序。小さい値ほど先に現れる。色画像ではなく線形データ |
| Support `B(x,y)` | 0〜1 の最終表示範囲。ブラシ外など表示対象外を区別する |
| Coverage `C(x,y,p)` | Timing と Progress から算出する、現在の表示量 |
| Softness `s` | 0〜1 のタイミング境界幅。初期値 0。画素単位のぼかし半径とは異なる |

Timing と Support は別の意味を持つ。ブラシ外を Timing=1 だけで表すと100%で背景まで現れるため、Support が必要になる。既定の全域 Reveal では B=1。ブラシ形状を最終範囲として残す場合だけ B にその形状を保持する。

## 数値契約

入力 p、s、T、B は有限値に制限して [0,1] に clamp する。不正な保存値は Progress=1、Softness=0 に復旧し、診断を境界で記録する。

```text
q = Reverse ? (1 - T) : T
p <= 0 : C = 0
p >= 1 : C = B
0 < p < 1, s == 0 : C = B * (p >= q ? 1 : 0)
0 < p < 1, s > 0  : C = B * smoothstep(q - s/2, q + s/2, p)
```

端点は比較式より先に処理する。Progress=0 は T=0 でも完全非表示、Progress=1 は T=1 でも最終範囲を完全表示する。Softness=0 で同一端点の smoothstep を呼ばない。Reverse は表示順だけを反転し、Progress の端点と Support は変えない。消去演出は Progress を1→0へアニメーションする。

概念上の最終 alpha は `SourceAlpha × Coverage × ExistingMask × Opacity`。既存 mask / matte / Opacity を二重に適用しない。premultiplied RGBA では Coverage を RGB と alpha の両方に掛ける。straight RGBA の境界では alpha のみに掛け、既存の明示的な premultiply 処理を通す。

## パターンと範囲

| パターン | 表示順序 | 段階 |
|---|---|---|
| Linear | レイヤーローカル矩形内の方向への射影を正規化 | 初期 |
| Radial | 指定中心から四隅の最大距離までを正規化 | 初期 |
| Noise | 固定 seed とローカル座標から決まる順位 | 初期 |
| Brush | 順序付きストロークと dab の累積距離／順序 | 拡張 |
| Custom | 明示した単一チャンネルのタイミング画像 | 拡張 |

Linear / Radial / Noise の Support は全域1。Brush は各 dab の被覆と表示時刻を事前生成し、重なる地点の Timing は最初に到達した値、Support は被覆の最大値とする。複数ストロークは入力順で連結する。手描き風はこの方式のプリセットであり、実際の文字筆順を推測しない。

Custom は Timing 入力と任意の Support 入力を別途指定する。未指定 Support は1。色空間変換を自動適用せず、チャンネル、寸法、フィルタ、解像度を明示する。画像 alpha を Support として使う場合もユーザー設定を必要とする。

## 座標と適用順序

レイヤーローカル座標に固定し、レイヤーの移動・回転・拡縮に追従する。画面座標、Viewport のズームや pan を生成キーに含めない。Text / Shape の基準矩形は内容 revision が変わる時に更新する。字形や図形のアニメーションで矩形が変わる場合の再正規化による見え方を初期段階で確認する。

論理的な適用順序は、レイヤー内容の描画 → レイヤーエフェクト → Reveal → 既存 mask / matte / Opacity と合成。Reveal はエフェクト後のレイヤー結果を覆う。実際の既存処理順への組み込みは実装前調査で確認し、mask / matte の意味を変更しない。Blur 等で広がった画素には基準矩形外の Coverage=0 を使う。

初期対象は静止画、連番画像、Text、Shape。連番は同じキャンバス寸法のローカル座標に固定する。調整レイヤー、3D、動画、世界座標 Reveal は初期対象外。

## Text / Shape の意味

Text は完成した字形の Source alpha を Coverage で制限する。Text Animator の既存 selector / 範囲や文字ごとのアニメーションは維持する。左からの表示は画素の空間順序であり、文字単位 selector や実筆順ではない。

Shape は Fill / Stroke を描画した結果全体に同じ Coverage を適用する。Radial は図形の幾何形状を変更しない。線が描かれる図示例は空間マスク演出として扱う。パス長に沿う厳密な Trim Paths、Stroke の開始／終了、Fill と Stroke の別 Progress は独立した将来仕様が必要で、本機能の完了条件には含めない。

## UI・編集・保存

レイヤー固有プロパティの Reveal セクションに Enabled、Progress、Pattern、Softness、Reverse とパターン設定を置く。既定 Enabled=false、Progress=1、Pattern=Linear、方向=左→右、中心=(0.5,0.5)、Noise seed=0。無効時は既存の表示と一致する。設定変更、リセット、Undo/Redo、キーフレーム、保存再読込は既存の property / command 経路を使う。

提案プロパティ名前空間は `reveal.*`。確定時に既存パスとの衝突を調査する。Progress のみ初期段階でアニメーション対応し、seed、入力マップ、ストロークは静的設定とする。パターンごとの不要な項目を隠す。

タイムライン左ペインに新規 Reveal グループを追加しない。Progress のキー評価は既存基盤を利用し、直接編集とキー表示の導線は既存専用プロパティ面で成立するか実装前に確認する。新規 signal/slot、固定ショートカット、QtCSS を追加しない。

保存には schema version、設定、seed、基準範囲、Custom 資産参照、Brush の順序付き入力を持たせる。生成 texture と GPU handle は保存しない。旧プロジェクトは Reveal 無効として開く。参照欠落時は設定を保持し、Reveal を一時 bypass して警告する。利用不能時に最終レンダーを黙って成功扱いにしない。

## GPU・キャッシュ契約

GPU/Diligent 経路を優先する。単純パターンは解析式評価を選べる。Brush / Custom は設定・入力変更時に生成／転送し、Progress 変更では uniform 更新と Coverage 評価だけを行う。Timing は線形単一チャンネルの形式を候補とし、必要な精度と既存 SRV 契約を確認して決める。

キーは layer identity、source/input revision、寸法、生成設定、seed、schema に基づく。Progress、Opacity、Viewport の表示状態はキーに含めない。生成完了は generation を照合して stale 結果を棄却する。undo、削除、composition切替、device loss を無効化境界として定義する。

cache byte budget、最大寸法、生成 queue 上限は明示的に設け、満杯時は最新要求への集約と defer を使う。異なる revision の map を新設定へ適用しない。準備中のプレビューは最後の完成した設定結果を保持し、状態を表示する。確定レンダーは準備完了をフレーム投入前に必要条件とし、失敗は明示する。

steady-state に texture/PSO生成、全画像コピー、readback、QImage変換、CPU再生成、無条件文字列ログを追加しない。容量とバックプレッシャーは [HOT_PATH_RULES](HOT_PATH_RULES.md) に従う。ソフトレンダラーの新機能実装は本計画で要求せず、利用不可の表示と互換性方針を先に決める。

## 受入れ条件

1. 全パターンで0%は非表示、100%はSourceとSupportの積になり、無効時は従来表示と一致する。
2. Opacity と Progress を独立アニメーションでき、既存 mask / matte / blend と二重乗算しない。
3. premultiplied alpha の縁にハローがなく、半透明入力、Softness=0、Reverse、矩形外を扱える。
4. Text / Shape / 静止画 / 連番で座標・保存・Undo・スクラブが一貫し、同じseedと設定で同じ結果になる。
5. Progress更新だけでは map 再生成、texture生成、upload/readback が発生しない。
6. 非同期生成の取り違え、資産欠落、device再生成、容量不足を明示した契約どおり処理する。

## 実装前に確定する事項

既存合成経路での正確な挿入箇所、プロパティ／キー編集導線、texture形式と精度、cache予算、Brush入力資産形式、Custom資産管理、ソフトレンダラー利用時の対応範囲を計画の各調査段階で確定する。未確認の API を前提に実装しない。

## 2026-10-01 実装の具体化

通常レイヤーの共通設定として `LayerRevealSettings` を保持する。対象は `ArtifactImageLayer`（連番を含む）、`ArtifactTextLayer`、`ArtifactShapeLayer`。Properties の Reveal セクションから編集し、Progress は0〜100%表示、保存値は0〜1とする。タイムラインのグループ表示制約は変更しない。

GPUプレビューは `CompositionRenderController::Impl::prepareGpuLayerForBlend` のresident effect処理後、external matte処理前に適用する。最終レンダーは Render Queue のGPUレイヤー描画結果に同じCoverage shaderを適用する。後者は既存のSprite PSOが `SRC_ALPHA` を使うため、描画境界でlinear premultipliedからlinear straightへの変換を明示する。最終レンダーでRevealがある場合、CPU経路は失敗を返し、XPUのCPU混在は利用しない。

| 設定 | 実装契約 |
|---|---|
| Pattern | Linear / Radial / Noise / Brush / Custom の列挙選択 |
| Linear | 角度は度単位。元のローカル矩形の縦横比を考慮する |
| Noise | 128×128の正規化ローカル格子と24bit seedで決定する |
| Brush | Single / Multiple / Handwriting のプリセット、正規化半径0.005〜0.5。円形の連続ストロークからTiming / Supportを生成する |
| 任意ストローク | `reveal.strokeData` に、ストロークごとの正規化座標ペアを持つJSON配列文字列を設定可能。合計256点、文字列32768文字まで。プリセットより優先し、プリセット切替では入力を破棄しない。専用キャンバス編集ツールは追加しない |
| Custom | Timing / Support に独立した画像ファイルを指定。各ファイルのraw channel 0を使う。Support省略時は1。色変換なし、非有限値は失敗 |
| 生成map | 512×512、GPU形式 `RG32_FLOAT`。R=Timing、G=Support。入力とGPU参照はnearest sampling。解像度はこの初版で固定 |
| 入力画像 | 最大2048×2048、1〜4チャンネルの2D画像。制限を超える入力を黙って縮小して受け入れない |
| キャッシュ | 1 map=2 MiB、最大32 map / RenderPipeline=64 MiB。CPU側の生成mapは所有レイヤー単位で2 MiB。無効化時に解放。プロジェクト／worker全体のCPUメモリ予算は今後の計測対象 |
| 保存 | schema version 1、静的設定、Progressのキー／式／envelope、ストローク入力、マップパスとAssetManagerの資産ID。GPU textureを保存しない |

Brush / Custom の生成・decodeは設定確定、復元、明示した `rebuildRevealMap()` の境界で同期実行する。連続プレビュー入力はmap生成項目を変更せず、確定時に反映する。非同期生成queueは導入していないため、生成jobのstale結果は発生しない。入力ファイルの再読込はパスの再確定または明示した再生成APIで行う。外部ファイルの自動監視は追加しない。

描画資源準備は生成epochと既存レイヤー配列のidentity／件数で変更を検出し、Progress / Opacity更新でmapの生成・uploadを行わない。GPU cache満杯時は追加確保せず、プレビューは最後のpresent済みフレームを維持する。slotが解放される変更後に再準備する。入力欠落時のプレビューはRevealをbypassし、Propertiesに説明を表示する。最終レンダーは欠落・容量不足・GPU失敗をフレーム失敗として返す。

Shader、uniform buffer、textureの確保はpipelineの初期化／resize／設定変更に伴う資源準備境界に限る。定常Progress更新の処理は定数更新と既存GPU作業画像を使うdispatch／copyである。これらは設計上の境界であり、実測済みの性能保証ではない。

既存の直接描画プレビューでGPU合成を使えない場合はRevealをbypassし、診断を一度出す。`OffscreenCompositionRenderer` などレイヤーの `draw()` を直接呼ぶ互換経路は今回の適用位置を通らないため、本仕様の対応経路として扱わない。GPUプレビュー、Render Queue、資産参照、mask / matte / effect併用、device再生成の実機受入れは未実施。
