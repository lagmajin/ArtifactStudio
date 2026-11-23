# 連番画像 受入れマトリクス（2026-11-17）

**最終更新:** 2026-11-17

**ステータス:** 契約集約済み / runtime 受入れ未実施

**目的:** `docs/analysis/REPORT_DCC_GAP_UPDATE_2026-08-15.md` §4-1 の指摘（「欠番・開始番号・異なるサイズ・fps 変更・保存→再読込・キャッシュ無効化・フレーム外参照を受入条件として固定する必要がある」）を、実コードと既存契約テストに基づく 1 枚の受入表へ落とし込む。

**前提:** ビルド・テスト・実機確認はユーザー明示指示まで実行しない。したがって本表の「静的」はコード/テスト存在の確認、「runtime」は未実施を意味する。

## 根拠となる既存契約（重複定義を避け正本として参照）

- `docs/planned/MILESTONE_SOURCE_RELOCATION_CONTRACT_2026-08-21.md`（M-SRC-RELOC-1）… 保存キー棚卸し、解決優先順位、相対パス／Asset ID／absolute fallback
- `docs/planned/MILESTONE_IMAGE_SEQUENCE_WORKFLOW_COMPLETION_2026-07-27.md` … 検出・投入・永続化・時刻駆動の実装完了条件
- `docs/analysis/STILL_IMAGE_LAYER_ACCEPTANCE_MATRIX_2026-08-08.md` … 静止画側の受入れ（連番はここへ接続する）
- `tests/ArtifactCore/ImageAssetMetadataContractTest.cpp`（`ImageSequenceSourceContractTest`）… Core 側の契約テスト 20+ 件
- `tests/ArtifactCore/SourceResolutionContractTest.cpp` … source 解決の契約テスト

## 判定凡例

- **静的-済**: Core 契約テストまたは実コードで確認済み（本セッションはテスト未実行）
- **静的-経路**: 実コードの接続は確認済みだが専用テストなし
- **RT-未**: 実素材・実アプリでの runtime 受入れ未実施（ビルド／実機許可が必要）

---

## A. 検出とパス復元

| # | ケース | 期待値 | 根拠 | 状態 |
|---|---|---|---|---|
| A-1 | 連番ファイル名（`shot_0001.png` 等）を開く | padding／開始番号を保持し、欠番をギャップ分割される | `parseSequencePattern` / `open()` | 静的-済（MapsNumberedFilesAcrossGaps） |
| A-2 | 単一の番号付き画像を開く | `isSequence == false`、1 フレームとして扱う | `open()` | 静的-済（ASingleNumberedImagePathRemainsOneStillFrameNotASequence） |
| A-3 | 明示 frame path 列に欠番 slot を渡す | slot を圧縮せず保持、`frameAt()` は null、ファイル追加で復帰 | `openFramePaths()` / `noteFrameExistence()` | 静的-済（PreservesExplicitMissingFrameSlots） |
| A-4 | プロジェクトを別ディレクトリへ移設 | 解決順 `SavedAssetId → ProjectRelativePath → RegistryRelativePath → AbsolutePathFallback` で復旧 | M-SRC-RELOC-1 §解決優先順位 | 静的-済（SourceResolutionContractTest）／RT-未 |
| A-5 | `sequencePaths` と `sequencePathsRelative` の要素数が不一致 | 両配列の長い方まで走査し、frame index を切り詰めない | M-SRC-RELOC-1 §進捗 2026-10-02 | 静的-経路／RT-未 |
| A-6 | 代表フレームをリネームして欠落 | health report が `MissingAsset` をフレーム個別に報告 | `ArtifactProjectHealthChecker`（M-SRC-RELOC-1 §進捗 2026-10-02） | 静的-経路／RT-未 |
| A-7 | missing 状態から relink | 参照 layer／sequence、Asset registry、cache generation が一貫更新 | M-SRC-RELOC-1 完了条件 | RT-未 |

## B. 読み込みとフレーム解決

| # | ケース | 期待値 | 根拠 | 状態 |
|---|---|---|---|---|
| B-1 | 番号付きファイルのギャップ | `sourceFrameNumberAt()` / `sequenceIndexForSourceFrame()` が欠番を誤補間せず `-1` を返す | `seekSourceFrame()` 他 | 静的-済（MapsNumberedFilesAcrossGaps） |
| B-2 | 解像度が異なるフレームを含む sequence | そのフレームを cache へ取り込まず、UV／crop の不整合を防ぐ | `ArtifactImageLayer::draw()`（Image Sequence Workflow 2026-07-29 監査） | 静的-経路／RT-未 |
| B-3 | 再読込時に同一 source path で別 sequence を復元 | 旧 reader／cached frame を破棄し、旧フレームを再利用しない | JSON 復元経路（同監査） | 静的-経路／RT-未 |
| B-4 | `setFromQImage()`／単独画像へ切替 | sequence reader と cached index を破棄、prefetch generation を進める | 同監査 | 静的-経路／RT-未 |
| B-5 | 不正パス／範囲外 index | 直前フレームを残さず missing-image fallback へ遷移 | 同監査 | 静的-経路／RT-未 |
| B-6 | 16bit PNG の連番を読み込む | **現状は 8bit で入る（未着手）**。`ImageSequenceSource` が `QImage` を経由するため | `docs/analysis/PROFESSIONAL_VS_AUDIENCE_GAP_2026-10-04.md` B-10 | 未実装（既知ギャップ） |

## C. 時刻・再生

| # | ケース | 期待値 | 根拠 | 状態 |
|---|---|---|---|---|
| C-1 | タイムライン fps ≠ ソース fps | `frameIndexAtTime()` が `floor(timelineFrame/timelineFps × sourceFps)` で解決 | `frameIndexAtTime()` | 静的-済（ConvertsTimelineFramesUsingSourceAndTimelineRates） |
| C-2 | フレーム外参照 | 0 .. `frameCount()-1` へ clamp／saturate | 同関数 | 静的-済（SaturatesUnrepresentableTimelinePositionsToLastFrame） |
| C-3 | `setFrameRate(12)` 後に `openFramePaths()` を再実行 | 指定 fps が 24 に戻らず保持される | `requestedFrameRate` 保持（src:68-71,711-720） | 静的-済（PreservesExplicitMissingFrameSlots 末尾） |
| C-4 | アニメーション GIF | 各 frame delay から実 fps を導出 | `open()` の `animatedGifFrameRate` | 静的-済（GIF frame delay テスト） |
| C-5 | 実コンポジションで連番を scrub | 時刻に応じてフレームが進む／欠番で停止しない | `ArtifactImageLayer::draw()` → `tryFrameAt()` | RT-未 |

## D. キャッシュ

| # | ケース | 期待値 | 根拠 | 状態 |
|---|---|---|---|---|
| D-1 | 連続 seek | bounded LRU（capacity 8 / 256 MiB）、hit/miss 取得、明示クリア | `frameCache*` API | 静的-済（CachesDecodedFramesAndExplicitlyClearsCache / KeepsDecodedFrameCacheWithinCapacity） |
| D-2 | 同一パスのファイル差し替え | size + lastModified で無効化し、旧 decoded frame を再利用しない | `frameAt()` / `tryFrameAt()` | 静的-済（InvalidatesCachedFrameWhenSourceFileChanges 他） |
| D-3 | close／再 open／編集後の非同期 prefetch | generation が進み、古い結果が placeholder や新 sequence を上書きしない | `resetPrefetchState()` | 静的-済（FailedReopenClearsPreviouslyOpenedSequence 他） |
| D-4 | 読込失敗フレーム | 負の結果も bounded cache に保持し、scrub 毎の再デコードを防ぐ | `frameAt()` の failedFrame 経路 | 静的-経路／RT-未 |

## E. 保存・再読込（アプリ側）

| # | ケース | 期待値 | 根拠 | 状態 |
|---|---|---|---|---|
| E-1 | sequence を Composition へドロップ | N 枚の個別レイヤーへ分解されず 1 レイヤーになる | Image Sequence Workflow 2026-07-28 | 静的-経路／RT-未 |
| E-2 | 保存→再読込 | `image.sequencePaths` / `image.sequenceFrameRate` が維持される | `ArtifactImageLayer` JSON | 静的-経路／RT-未 |
| E-3 | 再読込直後 | 旧 frame cache を再利用せず、prefetch を再起動する | 同監査 | 静的-経路／RT-未 |
| E-4 | Undo／Redo 後に保存 | pixel result、bounds、frame range、duration が変わらない | M-IMG-1 Phase 2 | RT-未 |

## F. 出力一致

| # | ケース | 期待値 | 根拠 | 状態 |
|---|---|---|---|---|
| F-1 | 同一フレームの Preview／Software Preview／Render Queue 比較 | 差分許容 2、許容超過ピクセル率 0.1% 以内 | M-IMG-1 2026-08-20 | RT-未 |
| F-2 | work area の先頭／末尾フレーム出力 | 先頭・末尾が欠けず、フレーム番号が一致 | `docs/planned/MILESTONES_BACKLOG.md` WALK 項目 | RT-未 |
| F-3 | 連番を effect 付きで出力 | effect／mask／blend が全フレームで一貫 | M-IMG-1 Phase 3 | RT-未 |

---

## 集約: 何が「まだ足りない」か

1. **契約そのものは概ね固定済み**（A〜D は Core 契約テスト＋既存マイルストーンで充足）。旧レポートの「受入条件を固定する必要がある」は、実態は「固定済みだが分散していて実行されていない」と読み替えるのが正しい。
2. **真の残課題は runtime 受入れ**（E-1〜E-4, F-1〜F-3, C-5, A-7）。これらはビルド・実素材・実機許可が前提で、現ルールでは未実行。
3. **実装側に残るギャップは B-6（16bit 連番が 8bit で入る）のみ**。ここは新規実装が必要で、受入表に入れても「期待値を満たさない」状態を記録するだけにとどめる。

## 実行に必要な条件

- ビルド／テスト実行の明示許可
- 受入素材: 連番 PNG（8bit/16bit 混合可）、欠番あり、開始番号が 1 でないもの、解像度不一致フレーム、GIF
- 比較ハーネス: `ArtifactSoftwareRenderTestWidget` の Preview／Render Queue capture（既存）

## 記録ルール

実行したら各ケースの状態を `RT-未` → `RT-済（日付）` へ更新し、失敗ケースは source／decode／color／cache／composite のどの段階で破綻したかを備考へ残す。Core 契約テストは更新せず、runtime 固有の不具合は別途 bug 文書へ分離する。
