# マイルストーン: Image Sequence Workflow Completion

> 開始日: 2026-07-27
> 状態: In Progress

## 目的

静止画・連番画像を、Asset BrowserからCompositionへの投入まで一貫した素材として扱えるようにする。動画デコード対応には依存せず、連番画像の編集・確認・再リンク・プレビューの完成度を優先する。

## 現状

- 連番候補の検出と欠番によるグループ分割を実装済み。
- `ImageSequenceSource` に限定サイズのフレームキャッシュを追加済み。
- Asset Browser側にはsequenceの代表フレーム、開始フレーム、桁数、構成パスを保持する既存経路がある。
- Asset Browser上の明示的な展開、sequence単位の状態表示、preview導線の一貫性は未完了。

### 2026-07-27 Progress

- Asset Browserのダブルクリック導線を実装。
- フォルダはブラウズ移動し、sequence／ファイルはメタデータ・プレビュー選択を更新する。
- importは明示操作に限定し、閲覧と適用の混同を避ける。
- 欠番、padding、フレーム範囲をsequence行と情報面へ表示する。
- sequence選択時は全フレームをimport対象へ展開する。
- context menuのimport操作名にフレーム数を表示する。
- frame cacheはbounded LRUで保持し、ファイル差し替えを更新時刻・サイズで検出する。
- cache hit / miss取得と明示クリアのAPIをArtifactCoreへ追加する。
- sequence行をコンテキストメニューから展開／折りたたみでき、個別フレーム行を確認できる。
- 展開したフレームの選択・ドラッグ・importは親sequence全体の関係を維持する。
- キャッシュ保持数・上限の診断 API と、seek時の次フレーム先読みを追加する。
- `ImageSequenceSource` でも代表フレームの padding を sequence 識別条件にする。

## Phase 1: Sequence Item Presentation

- sequenceを1行または1タイルの論理アセットとして表示する。
- 代表フレーム、フレーム範囲、欠番状態、桁数を表示する。
- 展開時に個別フレームを確認できる導線を追加する。
- standalone imageとsequenceの見た目・操作責務を混同しない。

## Phase 2: Sequence-Aware Workflow

- sequence単位でimport、relink、previewを実行する。
- 個別フレームを選択した場合も、sequence全体との関係を失わない。
- 欠番、読込失敗、サイズ不一致をsequence単位で診断する。
- 既存のAsset Browser / Project View責務を越えてComposition編集を持ち込まない。

## Phase 3: Preview and Cache Hardening

- frame cacheのhit/missと容量を診断可能にする。
- 同一パス差し替え時に古いフレームを再利用しない。
- scrub時の近傍フレーム先読みはbounded cacheの範囲で行う。
- 画像本体のQImage化は入出力・Qt境界に限定し、合成本流へ拡張しない。

## 完了条件

- Asset Browserでsequenceが単一素材として認識できる。
- 展開して個別フレームを確認できる。
- 欠番・読込失敗・relink状態をsequence単位で把握できる。
- sequenceをCompositionへ投入し、再読込後もフレーム範囲と素材関係が維持される。
- キャッシュが無制限に増えず、同一フレームの不要な再読込を抑制できる。

## 非対象

- 動画デコーダー、動画コンテナ、音声同期。
- 全フレームの無制限メモリ保持。
- Asset BrowserへのComposition編集責務の移植。

## 次の実装単位

1. preview / import / relink の状態表示を共通化する。
2. 実素材でのキャッシュhit/miss/保持数確認を行う。
