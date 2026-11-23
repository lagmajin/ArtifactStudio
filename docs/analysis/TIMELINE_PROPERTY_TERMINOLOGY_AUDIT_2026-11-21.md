# Timeline / Property Editor 用語・翻訳の静的監査

**最終更新:** 2026-11-21

## 目的と範囲

ビルド待ちの短時間で進められる UI 整理として、Timeline と Property Editor の表示語・翻訳経路をソースから確認した。対象は `ArtifactTimelineWidget`、`ArtifactLayerPanelWidget`、`ArtifactTimelineTrackPainterView`、`ArtifactPropertyEditor` と `Artifact/translations/en.json` / `ja.json`。本メモは静的確認のみで、UI 実行、キャプチャ、翻訳変更、ビルド、テストは行っていない。

ウィジェット責務は `docs/WIDGET_MAP.md` に従い、Timeline orchestration と左ペインの Layer Panel、Property Editor の区分を維持する。

## 用語・表示箇所

| 概念 | コード上の担当 | 表示語・ソース上の例 | 翻訳経路 / 状態 |
|---|---|---|---|
| Timeline の表示対象フィルター | `ArtifactTimelineWidget` | `All Layers`, `Selected`, `Animated`, `Keyframes + Important`, `Keyframes Only`, `Audio`, `Video` | コンボに `QStringLiteral` で直接設定。ツールチップ `Choose which layer properties are shown in the timeline` も直接設定され、翻訳キーを通らない。現状の明確な可視ローカライズ候補。 |
| キーフレームスニペット | `ArtifactTimelineWidget` | ボタン `Snippet`、ツールチップ `Save or apply named keyframe snippets` | 直接文字列で設定される。ツールバーに追加され、非表示指定は確認できない。可視ローカライズ候補。 |
| レイヤー／プロパティ列ヘッダー | `ArtifactLayerPanelWidget` | `Layer / Property` | 直接文字列。画面上の列ヘッダーとして使われる。可視ローカライズ候補。 |
| Timeline のキー情報・選択概要 | `ArtifactTimelineWidget` | `Keys: …`, `Selection: …`, `Current: …`, `Status: …` | 一部は英語の直接文字列。ただし確認した `keyframeStatusLabel_`、`currentLayerLabel_`、`frameSummaryLabel_`、`selectionSummaryLabel_` は `setVisible(false)` が設定される。したがって現状の可視 UI 問題とは扱わず、再表示する変更時の確認項目とする。 |
| キーフレームのツールチップ | `ArtifactTimelineTrackPainterView` | `Frame`, `Selection`, `Keyframe Area` など | `tt(key, fallback)` 経由で `TranslationManager` を利用。en/ja の `timeline` セクションに対応キーがあることを確認。 |
| Property row の操作名 | `ArtifactPropertyEditor` | `Previous keyframe`, `Keyframe`, `Expression` など | ツールチップ／短いアクセシブル名は `propertyUiText(key, fallback)` 経由。en/ja の `property` セクションに対応するキーがある。 |
| Property row のアクセシビリティ説明 | `ArtifactPropertyEditor` | `Property row: %1`, `Edit the %1 property`, `Toggle keyframe for %1` など | `QStringLiteral` による英語固定。日本語UIでは読み上げ名・説明が英語に残る可能性がある。可視ラベルの不一致ではなく、アクセシビリティ翻訳の確認候補。 |

## 静的に確認できたこと

1. Timeline のトラック描画・キーフレーム tooltip と Property Editor の主要操作 tooltip には、`TranslationManager` を呼ぶ既存経路がある。表示対象や操作名すべてが翻訳未対応という状態ではない。
2. Timeline 表示対象フィルター、`Snippet` 操作、Layer Panel の `Layer / Property` は直接文字列である。ユーザーが選択した `ja` / `en` のどちらでも英語のままになる可能性が高い。
3. Timeline のキー状態・選択概要には英語の直接文字列があるが、該当ラベルは現在 `setVisible(false)`。現在の画面上の欠陥と断定せず、表示を有効化する際に翻訳契約を整える。
4. Property Editor では tooltip／短いアクセシブル名の翻訳キーが使われる一方、アクセシブル説明は英語固定の箇所がある。可視 UI と読み上げ情報を別々に受け入れ確認する必要がある。
5. `en.json` / `ja.json` の `timeline` / `property` セクションで、今回参照したキー（`timeline.frame`、`timeline.selection_keyframe`、`property.keyframe.*`、`property.expression*` など）は両言語に存在する。全 locale、全キーの網羅監査ではない。

## 次の小さな作業単位

### Slice A — Timeline 可視ラベルの翻訳契約

表示対象フィルター、フィルターツールチップ、`Snippet` ボタン／tooltip、Layer Panel 列ヘッダーについて、既存 `timeline` / `layer_panel` のキー命名に合わせたキー表を作る。コード変更時は ja/en と設定で選択できる全 locale のキーセットを揃え、翻訳漏れ時の fallback 表示を確認する。新規イベント配線や UI 構造変更は不要。

**実施済み（2026-11-21）:** 上記 4 点を `tt()` 経由へ移し、`en` / `ja` / `zh` / `zh-TW` に同一キーを追加した。設定画面で選択可能なのはこの 4 言語のみ（`ko` / `fr` / `de` / `es` / `pt` / `ru` / `ar` は 68 キーで選択閾値 185 未満、`ApplicationSettingDialog.cpp:236-244`）。追加キーは `timeline.display_mode_*`（7 種）、`timeline.display_mode_tip`、`timeline.snippet_button` / `snippet_tip` / `snippet_save_selected` / `snippet_apply` / `snippet_delete`、`layer_panel.header_layer_property`。`%1` プレースホルダは 4 言語で一致、JSON 重複キーなし、既存の `zh` に欠けていた `layer_panel.menu_trans_wipe` / `menu_trans_slide` は今回の変更前からの欠落で、範囲外として据え置いた。`_meta.lastUpdated` は `en` / `ja` のみ更新（`zh` / `zh-TW` は `_meta` を持たない）。実画面クリッピングは未確認。

### Slice B — アクセシビリティ文言

Property row の固定英語 `accessibleName` / `accessibleDescription` を洗い出し、翻訳 API を使う既存操作名と同じ locale 契約に揃える候補を整理する。表示ラベルや Inspector の責務は変えず、スクリーンリーダー向けの文言だけを対象にする。

### Slice C — 隠れた診断ラベルの再表示条件

Timeline のキー状態・選択概要が非表示であることを維持したまま、将来表示を有効化する変更があれば、その同じ変更で文言を翻訳キーへ移し、選択数の常時表示バッジを追加しないことを確認する。

## 制約と未確認事項

- 実画面での ja/en 切替、クリッピング、読み上げ動作は未確認。ウィジェット UI のロケールマトリクス（ja/en/zh/zh-TW 等）に従う実行確認は実施していない。Slice A の変更もビルド・実行・キャプチャ未実施で、`Layer / Property` ヘッダーは可変幅列に描画されるため、長い ja/zh ラベルがクリップしないかは実機確認が必要。
- `timeline` / `property` キー全体の locale 間 parity は監査していない。
- ここで挙げた英語固定文字列の一部は、アプリの意図的な英語用語である可能性も残る。翻訳実装前に既存の用語方針と画面上の責務を照合する。
- この調査を根拠に本番 UI を変更したり、非表示の選択概要を表示したりしない。

## 根拠ファイル

- `docs/WIDGET_MAP.md`
- `docs/planned/REPO_WIDE_UI_CONSISTENCY_AUDIT_2026-06-11.md`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cpp`
- `Artifact/translations/en.json`
- `Artifact/translations/ja.json`
