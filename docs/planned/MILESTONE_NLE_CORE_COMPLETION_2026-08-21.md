# マイルストーン: NLE Core 完成度向上(編集操作・OTIO・Undo)

**最終更新:** 2026-08-21
**ステータス:** In Progress
**対象:** `ArtifactCore/include/NLE/*`, `ArtifactCore/src/NLE/*`, module `NLE.Core` / `NLE.OTIO`

## 目的

ArtifactCoreNLE(ヘッドラインコア)のUI以外の不足を解消し、編集操作の意味論を完成させ、OTIO相互運用の劣化をなくす。

## 実装済み (2026-08-21)

### 編集操作

- `trimClip()` の `TrimMode::Roll` を実装。同一トラック上の隣接(butt joint)クリップとの境界移動として `rollTrim()` に委譲。隣接クリップが無い場合は失敗を返す。
- `trimClip()` の `TrimMode::Slide` は `slideClip()` へ委譲(新タイムライン開始位置 = newSourceRange.start())。
- `slideClip()` を実装形状に修正。クリップのsource rangeと長さを保持したまま移動し、両隣がシフトを吸収する(前クリップはend/source endを延長、次クリップはstart/source startを前後)。

### モデル拡張

- `Clip` / `ClipDraft` に `nestedSequenceId` を追加。ネストシーケンスをクリップのソースとして保持できる。JSON保存/復元対応。
- `ClipResolver::resolveClip()` はネストクリップに対して「Nested sequence」診断を返す。

### Undo

- `NLEEditHistory` を追加(snapshotベース)。`capture(label)` → 編集 → `undo()/redo()`。IDカウンタ込みで完全復元(`loadFromJson` がnextId系を復元するため)。maxDepth制限付き。

### OTIO (`NLE.OTIO`)

- Transition 17種すべてが往復可能に。exportは `metadata.artifactKind` に種別を保存、importはmetadata優先→名前逆引き→Crossfadeフォールバック。
- クリップspeedは `LinearTimeWarp.1` エフェクトとしてexport/import。`reversed` は `metadata.artifactReversed`。
- Marker色は標準パレット名(RED/GREEN/…/BLACK)でexportし、正確な色を `metadata.artifactColor`(HexArgb)に併記。importはmetadata→色名→デフォルトの順で解決。
- 字幕を `metadata.artifactSubtitles` へ移動(公式OTIOライブラリ経由のround-tripで欠落しない配置)。旧トップレベルキーの読込も維持。
- ネストシーケンスをOTIO標準のStack子要素としてexport/import(import側は再帰)。`metadata.artifactNestedSequenceId` / `artifactTimelineRange` で同一性と範囲を保持。

## テスト

- `tests/ArtifactCore/NLETest.cpp` を追加(Roll trim / Slide / EditHistory / OTIO round-trip ×2)。`ArtifactCoreNLETest` としてCMake登録済み。ビルド・実行はユーザー指示待ち。

## 洗い出しで判明し修正した問題 (2026-08-21)

- **`rollTrim` 右クリップのsource符号反転バグ**: 境界移動量 `leftDelta` の符号が逆で、右クリップのsource in点が境界と逆方向に動いていた(負のsource位置も生成し得た)。in点が境界を追従しout点固定の正しい挙動に修正。
- **`createTransition` 検証不足**: left/rightクリップがtransitionのtrackIdと異なるトラックでも作成できていた(orphan_transitionはvalidate事後検知のみ)。同一トラック強制、`duration > 0` とrange妥当性チェックを追加。
- **`validate()` のネスト未検知**: `nestedSequenceId` 参照先欠落を `missing_nested_sequence` issue として検出するよう追加。

## 洗い出しで判明し修正した問題 (2026-08-21)

- **`rollTrim` 右クリップのsource符号反転バグ**: 境界移動量 `leftDelta` の符号が逆で、右クリップのsource in点が境界と逆方向に動いていた(負のsource位置も生成し得た)。in点が境界を追従しout点固定の正しい挙動に修正。
- **`createTransition` 検証不足**: left/rightクリップがtransitionのtrackIdと異なるトラックでも作成できていた(orphan_transitionはvalidate事後検知のみ)。同一トラック強制、`duration > 0` とrange妥当性チェックを追加。
- **`validate()` のネスト未検知**: `nestedSequenceId` 参照先欠落を `missing_nested_sequence` issue として検出するよう追加。
- **`propagateMoveLink` がslideClipを使用**: slideClipの意味論変更(両隣吸収)により移動伝播が壊れていたため `moveClip` へ戻した。
- **削除カスケード不完全** (残存問題1): `removeClip` がattachedTransitions・clip markers・linkGroup.membersを掃除し、`removeTrack` が残留transitionsも削除するようにした。`overwriteClip` は `removeClip` 経由で自動的に恩恵。ネスト子sequenceはシーケンス削除後も独立して存続する契約を明記。
- **ConformServiceがconformしない** (残存問題2): 非const化し、effectiveRangeへの実適用(source/trim range更新+duration再計算)を行うようにした。`success` は unresolvedClips が空の場合のみ true。
- **TimeBase.dropFrame 未使用** (残存問題3): `timecodeString()` / `frameFromTimecode()` を追加。drop-frameは標準の「毎分2フレーム、10分毎に無し」方式(nominal 30→2)。双方向round-trip可能。
- **speed/reversed が解決に未反映** (残存問題4): `ClipResolution` に speed/reversed を追加し、`timelineFrameToSourceFrame()` / `sourceFrameToTimelineFrame()` の時間写像関数を実装。
- **trim系のavailableRange検証なし** (残存問題6): `Impl::sourceRangeAllowed()` を追加し、Source/Ripple/Slip/Roll/Slide/slipClipが媒体の利用可能範囲外のsource範囲を作る編集を拒否するようにした。未知ソース・非有限範囲はブロックしない。
- **ネストクリップへのtransition export非対応** (残存問題5): exportを再構成し、ネストStack要素でも後続transitionを出力するようにした。

## 洗い出しで判明した残存問題(未修正)

1. **LinkingService 伝播が自動でない**: trim/move時にvideo/audioリンクグループへ自動反映されず、呼び出し側が明示的にpropagate*を呼ぶ設計(意図的な契約として維持。UI統合時に再評価)。
2. OTIO公式ライブラリとの実機互換性検証は未実施(subtitles等はmetadata配置で対応済み)。

## テスト

- `tests/ArtifactCore/NLETest.cpp`: Roll trim / Slide / EditHistory / 削除カスケード / Timecode round-trip(drop-frame含む) / 時間写像(speed・reversed) / Conform適用 / クロストラックtransition拒否 / OTIO round-trip ×2。`ArtifactCoreNLETest` としてCMake登録済み。ビルド・実行はユーザー指示待ち。

## 未実装(次の段階)

- Sequence→ArtifactComposition 変換ブリッジ、プレビュー再生・Render Queue統合(UI非依存だが設計が必要)
- ClipResolverでのspeed/retime反映(timeline duration = source duration / speed の時間写像ヘルパー)
- trimClip Roll時のsource availableRange検証
- ネストクリップへのtransition付与のOTIO export
- マルチカム、クリッププロパティキーフレーム、フリーズフレーム
- UI(タイムラインウィジェット接続)

## 対象外

- `ReactiveEvents` 関連
- GPU/Diligent backend構造変更
