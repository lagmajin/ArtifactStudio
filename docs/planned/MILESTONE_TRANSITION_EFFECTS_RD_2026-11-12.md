# Transition Effects R&D Suite

**最終更新:** 2026-11-12
**ステータス:** In Progress

**進捗:** 2026-11-12 — 既存 `TransitionRenderContractTest` が入力A/Bの原寸PNGも保存するよう変更し、`tools/analyze_transition_capture.py` で全kindのprogress 0/1を入力画像と画素単位比較、隣接progress間の変更画素数をPNGから再計算してreport値と照合する仕組みを追加。全capture frameのalphaも不透明入力契約（alpha=255）と照合する。期待する16 kind、progress列、端点source、画素数、全PNGの存在・形式・寸法・読込可否を確認し、画像とcapture reportのSHA-256を `verification_report.json` に記録する。さらに `review.html` を生成し、入力、全kind grid、kind別progress stripと各フレームをブラウザで一覧できるようにした。解析器・C++ testは未実行。既存capture reportにはraw timingが含まれない。

## 目的

既存のトランジション実装を、再現可能な入力・進行条件・出力キャプチャで比較し、視覚品質、時間的一貫性、alpha／色の扱い、実行コストを評価できる R&D スイートとして整える。製品機能やアルゴリズムを追加する前に、各経路の実際の契約と限界を把握する。

このスイートは主に `ArtifactCore` の `Video.Transitions.*` を対象とする。`Artifact` の `TransitionManager`、NLE transition model、ArtifactPr timeline は別の経路として記録し、同一実装・同一品質だと仮定しない。既存の統合計画は [transition-effects-expansion-2026-07-09.md](../../plans/transition-effects-expansion-2026-07-09.md) を参照する。

## 既存資産と初期所見

- `tests/ArtifactCore/TransitionRenderContractTest.cpp` は、factory 登録、決定性、出力形状、alpha、left frame のみの変更、進行端点、進行ごとの画素変化、複数種のwipe形状などを契約テストしている。
- `tests/ArtifactTestTransitionRenderer.h` と `tests/ArtifactTestTransitionBridge.h` が合成診断画像と共通レンダー入口を提供する。
- `tests/ArtifactCore/TransitionMcpMain.cpp` は同じCPUレンダー経路を headless MCP lab として公開し、単体描画、strip、grid、差分、PNG出力を行える。
- 既存契約テストは実験基盤として有用だが、テストの存在は実行成功、実装経路の製品利用、視覚品質、性能を証明しない。
- 既存計画では NLE→Video 接続、`Artifact` の別系統、旧計画の前提訂正が記録されている。R&Dではこれらの統合未完了をレンダー品質評価と混同しない。
- 現状のMCP lab説明は「CPU・GPU deviceなし」。したがって、この時点でGPU parityや実時間性能を測定済みとして扱わない。
- `tools/analyze_transition_capture.py` は既存contract testのcapture bundleを検査する。画像ファイルの整合性に加え、progress端点を原寸入力A/Bと画素単位比較し、隣接progress間の変更画素数を画像から再計算する。見た目の良し悪しは目視が必要であり、runtime測定値は生成しない。

## R&D対象と評価軸

| 領域 | 対象 | 評価 |
|---|---|---|
| 登録と適用範囲 | `TransitionKind`、factory、具象実装 | 具象種の列挙・生成、未登録・nullの理由、Cut等の非具象値の扱い |
| 視覚挙動 | crossfade、wipe/reveal、spatial、motion/deformation、stylized | 端点、途中経過、境界形状、入力由来画素の分布、時間方向の滑らかさ |
| 画像契約 | RGBA8 CPU入力と出力 | 寸法、alpha保持／合成、透明領域、色順、境界外参照、無効サイズ |
| パラメータ | direction、origin、frame index、fps、size、progress等 | 適用される値、clamp、既定値、設定に対する単調性・決定性 |
| コスト | CPUレンダー経路 | 解像度・transition kind・progress段階ごとのraw timing、必要なら割当計測 |
| 統合経路 | NLE、Artifact effect、ArtifactPr | データ対応と駆動点の静的監査。実際に接続されているとソース／実行で確認できるまで統合済み扱いしない |

## 決定論的fixture案

初期fixtureは人工生成し、外部メディアやdecoder依存を避ける。

1. **色ブロック＋番号付き象限:** A/B由来領域と幾何変形を判別する。
2. **高コントラスト格子と斜線:** seam、aliasing、歪み、反転を観察する。
3. **単色・グラデーション:** blend、色順、progressの基本特性を測る。
4. **alphaパターン:** 不透明、半透明、透明黒、透明hidden-RGBを組み合わせる。
5. **小寸法／非正方形／odd dimensions:** 境界とサイズ依存を確認する。

まず96×64の既存診断対を互換基準として残し、追加fixtureは小さな標準寸法と選定した高解像度で段階導入する。入力画像、条件manifest、renderer/analysis toolはSHA-256を記録する。

## Capture manifest と記録契約

各captureは少なくとも次を記録する。

- suite schema/version、transition stable nameとenum値、source revision
- 入力fixture名とhash、幅・高さ、pixel/channel/alpha契約
- progressの数値、direction/originなどの全設定、frame index、fps
- 実行経路とbackend。未確認・未計測は明示し、CPU結果からGPU動作を推定しない
- warm-up回数、測定反復数、反復ごとのraw milliseconds。統計量はraw sampleから算出
- 出力画像path/hash、入力A/Bとの画像差分指標、生成時エラーや未対応理由

実装上の引数が分からない場合にmanifest上で架空のpropertyを作らず、まずソース監査台帳に「未確認」と記載する。画像変換や色／alpha正規化を追加する場合は元データを保持し、変換条件と実装adapterを記録する。

## 初期実験計画

### Phase 1 — 台帳と既存ハーネス監査

- `TransitionKind` の全値、具象実装、factory登録、既存test coverageを対応付ける。
- `ArtifactTestTransitionBridge` が設定値を各具象実装へどう渡すか、入力形式と出力変異契約を確認する。
- 既存のテスト出力（PNG、contact sheet、behavior report）の形式と出力先、再現性を確認する。
- `Artifact` effect系は別台帳を作成し、登録率・null fallback・alpha経路をソースから再確認する。修正はこのR&D準備の範囲外。

### Phase 2 — Capture基盤とfixture

- 既存の人工A/B診断fixtureとcapture reportを最初のfixtureとして利用し、追加fixtureは後続に回す。
- 既存の受け入れテストtargetは `ArtifactCoreTransitionRenderContractTest`（`tests/ArtifactCore/CMakeLists.txt` 登録）。実行時に `ARTIFACT_TRANSITION_CAPTURE_DIR` で指定した出力先へPNG/reportを保存し、そのdirectoryを `python tools/analyze_transition_capture.py <capture-directory>` で検証する。既定出力先はOS temp配下の `artifact-transitions`。
- 実行手順（対象buildがすでに存在し、ユーザーがテスト実行を指示した後）:
  ```powershell
  $env:ARTIFACT_TRANSITION_CAPTURE_DIR = "$PWD\transition-capture"
  & "<build>\bin\<config>\ArtifactCoreTransitionRenderContractTest.exe"
  python tools/analyze_transition_capture.py $env:ARTIFACT_TRANSITION_CAPTURE_DIR
  ```
- capture testはcontact sheetに加え `input_A.png` / `input_B.png` を原寸で保存する。解析器は `verification_report.json` を作り、期待16 kind、progress列、端点のLeft→Right、開始／終了画像の完全一致、隣接画像間の実変更画素数とreport値の一致、不透明入力に対するalpha保持、各stepの分類pixel count、全PNGの存在・RGBA形式・寸法・decode可否を検査する。出力画像と元reportのSHA-256を保存し、差し替えや再生成を追跡できる。
- 同時に `review.html` を出力し、入力A/B、progress別全kind grid、各kindのprogress stripと個別フレームをブラウザで見比べられるようにする。HTMLはローカル画像を参照し、外部リソースに依存しない。
- 自動検査PASSはcapture bundleとreport上の基本的な挙動が整合する意味に限る。画像を開いてcontact sheetと各progressを目視し、方向・境界・破綻を別途確認する。
- 共通progress列は0、0.125、0.25、0.5、0.75、0.875、1を使い、動きの速い／確率的な系統は連続フレーム追加を検討する。
- 全具象kindの小規模coverage captureと、選定代表kindの詳細captureを分ける。
- CPU timingは画像生成・初期化とtransition処理を分離できる場合に分けて記録する。分離不能はtotalのみ記録する。

### Phase 3 — 視覚・時間解析

- 端点差、隣接progress間差分、A/B source classification、alpha誤差、edge/seam位置・連続性を算出する。
- Wipe/reveal系はcoverage領域と境界の位置・形状を評価し、crossfadeとは別の指標で扱う。
- motion/deformation系は特徴点の移動、反転、画面外流出と時間差分を評価する。単一の汎用「品質スコア」へまとめない。
- 指標は既知のsynthetic fixture上で妥当性確認し、スクリーンショットを併記して数値だけで品質判定しない。

### Phase 4 — 比較・報告

- kindごとに入力、全条件、progress strip、解析値、raw timing、未対応条件をひとまとめにした結果bundleを出す。
- build、host、実行経路が一致する比較と、異なる環境の参考比較を分離する。
- 次の作業候補（不具合、未実装、統合、最適化）は根拠付きで提示し、別途承認された依頼なしに製品範囲へ拡張しない。

## 完了条件

- 全 `TransitionKind` と具象factory登録の静的台帳がある。
- fixture、capture条件、結果の出所がhashとmanifestで追跡できる。
- 端点・alpha・進行挙動・境界・時間一貫性をカバーする測定／目視資料がある。
- raw timingと実際のbackend情報が保存され、未測定・未対応が明確である。
- NLE、Artifact effect、ArtifactPrの接続状態を混同しない説明がある。

## 制約・検証状態

- これはR&D準備計画であり、アルゴリズム実装・統合作業を自動的に承認するものではない。
- C++ contract testとMCP self-checkの起動、captureの実行、解析器の実行、性能測定は未実施。既存capture JSONはraw timing、source revision、binary hash、build config、host/CPU情報を出さないため、同一環境の性能・backend parity比較に不足する。capture metadata拡張は次段階。
- 解析器はPillowを必要とする。これは既存の `tools/ui_visual_compare.py` と同じ画像依存である。
- ビルド、テスト、CMake configureは明示指示を得てから行う。
- 子リポジトリ／サブモジュールは変更しない。
