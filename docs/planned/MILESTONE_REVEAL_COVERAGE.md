# 共通 Reveal / Coverage 実装計画

**最終更新:** 2026-10-01

**ステータス:** In Progress

## 目的と成果物

[共通 Reveal / Coverage 仕様書](../technical/REVEAL_COVERAGE_SPEC.md) に従い、Opacity から独立した Progress で、静止画・連番画像・Text・Shape の表示領域を制御する。全体フェードと空間的な表示進行を別々に編集できるようにする。

本書は実装計画であり、コード変更やビルドの実行指示ではない。初期完成は Linear / Radial / Noise と共通GPU適用、拡張完成は Brush / Custom の入力・事前生成・保存までを指す。

## 現状の根拠

| 確認したファイル | 確認した内容／注意 |
|---|---|
| `Artifact/src/Effects/LinearWipe/LinearWipeEffect.cppm` | resident HLSLとCPU実装がある。現行例はalphaだけを変更しており、新Revealのpremultiplied契約をそのまま満たす雛形とはしない |
| `Artifact/include/Effects/ArtifactAbstractEffect.ixx` | 固定容量の `GpuSpatialEffectNode` 配列とeffect mask APIがある。Reveal入力SRVへの転用可否は未確認 |
| `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm` | resident surfaceがlinear-premultipliedである。調整レイヤーのmask mixも存在し、通常レイヤーRevealと混同しない |
| `Artifact/src/Layer/ArtifactAbstractLayerPropertyRouting.cppm`、`ArtifactAbstractLayerPropertyPresentation.cppm`、`ArtifactAbstractLayerPersistence.cppm` | レイヤープロパティ・表示・保存の調査候補。今回その詳細経路は未調査 |
| `docs/WIDGET_MAP.md` | Properties、Effects、Componentsは独立した面。通常プロパティはレイヤー主要項目を優先する |
| `docs/planned/MILESTONE_LAYER_EFFECT_WIPE_SLIDE_DISSOLVE_ZOOM_2026-08-30.md` | GradientWipe等の計画がある。共通Revealとの重複を実装前に整理する |

名前の検索だけで既存機能不存在とは断定しない。最新ブランチの実ファイルと呼び出し元を段階0で確認する。

## 段階0 — 挿入位置と責務の確定

通常レイヤーのsource生成、effect、mask、matte、opacity、blendまでのcallerを追い、静止画・連番・Text・Shapeが共有できる境界を確定する。UI property変更からUndo、frame評価、保存までを追う。既存の独自コンテナ、revision、資産参照、GPU resource所有laneも確認する。

レイヤー共通設定と合成直前のCoverage適用を第一候補とする。effect方式は適用順序・並べ替え・重複適用が仕様と一致する場合だけ比較する。新たな公開moduleやCore APIを必要とするか、既存Artifact内で完結できるかを決める。

完了条件: 正確なcallerと編集対象一覧、適用順序、プロパティ／キー編集導線、旧保存互換、GPU資源境界を本計画に追記する。未確定APIをコードへ書かない。

## 段階1 — 共通設定・保存・編集

Enabled、Progress、Pattern、Softness、Reverse、方向／中心／seedを既存property登録に載せる。Progressを既存フレーム評価へ接続し、Undo/Redoとversion付き保存を追加する。旧プロジェクトでは無効にする。レイヤー固有のRevealセクションを用意し、タイムライン左ペインの表示制約を維持する。

完了条件: 値範囲、既定値、reset、Undo/Redo、キー評価、保存再読込の設計が全対象で共通になる。初期段階でBrush等の未完成設定を利用可能として露出しない。

## 段階2 — LinearのGPU最小実装

既存resident境界に端点処理とCoverage乗算を追加し、Linearを解析式で評価する。coverageはpremultiplied RGBA全成分へ掛け、既存Opacityを再適用しない。既存出力・作業領域を再利用し、Progress変更ではuniform以外の生成を起こさない。

完了条件: 静止画で0/25/50/75/100%、Softness=0/非ゼロ、Reverse、Opacity=0/0.5/1、半透明入力の数値契約が成立する。無効時のbypass、mask/matte/blendとの整合が確認可能になる。

## 段階3 — Text / Shape / 連番への共通化

TextとShapeの描画結果を同じ境界に通し、字形／形状のSource alphaを保持する。レイヤーローカル範囲と連番キャンバスを定義する。内容変更時のrevisionと範囲更新を確認し、Viewportのpan/zoomやレイヤー変換ではmapの生成を起こさない。

完了条件: 4種のレイヤーでLinearが動き、Text Animator、Fill/Stroke、既存変換を壊さない。空間Revealで文字筆順や厳密なTrim Pathsを実現したと報告しない。

## 段階4 — Radial / Noise

Radialの中心と正規化、Noiseのseed・空間スケール・決定性を実装する。Noiseは時間乱数を使わず、スクラブ・再読込で同じ結果にする。解像度やCPU/GPU間の決定性範囲を明記する。

完了条件: 共通端点、Reverse、Opacity独立性を全初期パターンで満たす。ここで初期リリース判定を行う。

## 段階5 — map入力とboundedキャッシュ

TimingとSupportの保持形式、単一チャンネルtexture、SRV、明示upload、generationとrevision、GPU資源寿命を設計して実装する。cache byte budgetとqueue上限を設定し、最新要求へ集約する。準備中・失敗・参照欠落の状態をUIとレンダー開始前の境界へ返す。

完了条件: ProgressとOpacityの変更は再生成を起こさず、入力変更・resize・Undo・削除・device lossで適切に無効化する。stale完了やqueue満杯で誤ったmapを適用しない。

## 段階6 — Brush / Custom

Brushは順序付きストローク/dabをcold pathでTimingとSupportへ変換する。重なりは最初の到達時刻と最大被覆を使う。手描き・複数ストローク・粒子は同じ生成契約のプリセットとして段階的に追加する。新規ペイントツール一式は本計画の前提にせず、既存入力の再利用可否を確認する。

Customは既存asset管理でTimingと任意Supportを参照し、チャンネルと線形データ読み込みを明示する。入力パスだけに依存せず移動・再リンク・保存互換を扱う。

完了条件: 100%でブラシ外が露出せず、Supportを保持した最終形となる。参照欠落・map精度・再生成待ちの契約を満たし、入力資産と設定を保存再現できる。

## 段階7 — 受入れと性能確認

実行はユーザーの明示指示後に行う。検証項目は仕様書の受入れ条件に対応させる。

| 項目 | 確認内容 |
|---|---|
| 数値 | T=0/1、p=0/1、Softness=0、Reverse、Support=0/0.5/1 |
| 描画 | 半透明の縁、HDR/線形色、既存mask/matte/blend、Blur後の範囲 |
| レイヤー | 静止画、連番、Text Animator併用、Shape Fill/Stroke |
| 編集 | Progressキー、スクラブ・逆再生、reset、Undo/Redo、保存再読込 |
| 資源 | stale結果、composition切替、削除、device loss、欠落資産、budget満杯 |
| 性能 | Progress更新中のallocation/texture生成数、upload/readback bytes、CPU/GPU frame time、cache hit/miss、queue depth |

基準比較はReveal無効／Linear／map型を同じ場面で測る。目標はsteady-stateのmap生成・texture生成・upload/readbackがゼロであること。性能の絶対予算は段階0で対象機材・解像度と合わせて設定し、測定前に軽量と断定しない。

## 作業上の制約と依存

実装にはArtifact子リポジトリへの変更が必要になる見込みだが、今回の依頼は文書作成である。Coreや他サブモジュールの変更は、必要性を確認して対象を明示してから進める。親子の同名開発ブランチと子先行commit/push順を守る。

新規signal/slot、QtCSS、QColorDialog、QImageベースの本流合成を導入しない。ソフトレンダラー拡張、3D、動画、厳密Trim Paths、文字筆順推定は別計画とする。C++ modulesの依存を広げず既存実装ファイルを優先し、独自コンテナと [HOT_PATH_RULES](../technical/HOT_PATH_RULES.md) に従う。

## 完了判定

初期完了は段階0〜4と段階7の初期対象検証、拡張完了は段階5〜6を含む全検証を満たした時点とする。未ビルド／未実機の状態をCompleteとしない。実装開始時に本書をIn Progressへ更新し、完了時は文書ライフサイクルに従ってdoneへ移動し参照リンクを更新する。

## 2026-10-01 実装状況

段階0〜6に対応するソースをArtifact内に追加した。段階7のビルド・実機・性能検証はユーザーからの実行指示がなく未実施のため、マイルストーンはIn Progressを維持する。

| 対象 | 変更した経路 |
|---|---|
| 共通状態 | `include/Layer/ArtifactAbstractLayer.ixx`、`src/Layer/ArtifactAbstractLayerImpl.cppm` |
| 編集と評価 | `ArtifactAbstractLayerPropertyRouting.cppm`：有限値clamp、Progressの既存時間評価とキー編集、固定上限付きBrush / Custom生成、AssetManager参照 |
| プロパティ | `ArtifactAbstractLayerPropertyGroups.cppm` とShapeの独自グループ列挙：Revealセクション、既存enum / path / percent editorの利用 |
| 保存 | `ArtifactAbstractLayerPersistence.cppm`：version 1、生成入力、資産ID、Progressのキー／式／envelope。旧データは無効 |
| GPU実行 | `src/Render/ArtifactRenderLayerPipeline.cppm`：専用Reveal node、固定32slot cache、cold準備とdispatchの分離、RGBA全成分のCoverage乗算 |
| GPUプレビュー | `CompositionRenderController::Impl::prepareGpuLayerForBlend`：resident effects後に適用。Reveal有効時にGPU合成を選び、フレーム同期を行い、部分再合成から除外する |
| 最終レンダー | `ArtifactRenderQueueService.cppm`：workerごとのpipeline、Coverage shader共用、Spriteへのstraight RGBA境界、GPU必須判定と失敗理由 |

この初版のBrushは幾何ストロークであり、複雑なブラシ先端textureや筆圧・実筆順推定は追加していない。Customはraw channel 0と固定512解像度を明示している。専用ストローク編集ツールと外部ファイルの自動監視は未実装。詳細な初版契約と対応経路は仕様書末尾に記載した。

既存の `PropertyGroup` 列挙とrender layer配列のAPIを維持するため、新しい補助関数も既存の `std::vector` 参照を受け取る。新しい可変所有コンテナとしては独自 `Array<float>` を使い、strokeの作業領域とGPU slotは固定長とした。

差分の空白検査を行った。ビルド、CMake、テスト、GPU実行、割当計測は実施していない。次の受入れでは、5パターンの端点、ProgressキーとUndo、保存復元、半透明縁、Text / Shape内容変更、mask / matte / effects、マップ欠落、32slot容量不足、GPU final workerのdevice所有と再生成を確認する。
