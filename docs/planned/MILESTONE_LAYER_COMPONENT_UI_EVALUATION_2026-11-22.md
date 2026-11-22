# Component / Effect UI 評価計画

**最終更新:** 2026-11-22
**ステータス:** In Progress — 計画を作成。実アプリ評価は未実施。

## 目的と範囲

レイヤー Components 専用面と Effect 編集面で、既存の便利な専用 UI（Color Wheels、FloatColorPicker、effect 固有 UI）を評価し、次に追加する専用コントロールの優先度を決める。主な候補は色プロパティ向けのホイール／数値入力統合、component property editor の操作性、および effect descriptor に応じた編集コントロールである。

対象は既存 Artifact UI と現行の既存 UI capture harness。Composition Viewport のキャンバス内表示・ギズモ・HUD の変更、新しい event wiring、ArtifactWidgets 側の変更、画像処理アルゴリズムの品質評価は対象外。Component 設定は Components 専用面を正規の編集導線とし、通常 Property Widget へ露出しない。

## 現状の根拠と制限

- `docs/design/component-inspector/README.md` は Component 面に active component property editor を配置する設計を記録する。現行の未コミット差分には Components 面専用の埋め込み Property Editor がある。実アプリでの表示・編集・保存・Undo は未確認。
- `docs/design/color-picker/README.md` は承認済み FloatColorPicker の機能と、実画面未検証の項目を記録する。カラーピッカーは RGB/HSL/HSV/Lab/XYZ、float RGBA、履歴を含む。Color Wheels は色補正専用の別 UI であり、一般 color property の置き換えとみなさない。
- `tools/capture_ui_test.py` には `--components`、`--color-wheel-editor`、`--effect-suite-id` の既存経路がある。Color Wheel モードは専用ダイアログを開き、仮想pointerでホイールをドラッグして結果を記録する。Components モードはボタン選択を仮想入力で行い、property row数・可視状態・scroll到達性等を記録する。Effect Suite は ID・設定ごとの出力画像 capture 用で、editor interaction 全体は検証しない。
- 現行 Component fixture の名前対応は Physics / Script / Layout / Cloner / Fluid に限定される。既定Clonerのproperty editorが埋め込み表示されるかを数えるが、property 値の編集、layer 切替時の状態、保存再読込、Undo/Redo、関連効果の表示更新は現行レポートの合否条件に含まれない。Color Wheel fixture は pointer 操作と出力の検査があるため、こちらを新規 UI の対照例にできる。
- ビルド・テスト・CMake・Windows UI 操作はユーザーの明示指示後に実施する。本計画の作成では実行しない。

## 評価素材と条件

UI fixture は既存 Components capture fixture を使い、固定 layer・component 設定を記録する。Color Wheels は既存 `--color-wheel-editor` fixture を使う。色入力の UI 評価では不透明色、半透明色、暗色、高彩度色を含める。Effect UI の静的 fixture は既存 effect suite の代表として Glow と ColorCorrection 系から descriptor が異なるものを選び、登録 ID と property path を実行前に source と照合する。操作試験は実際の effect rack editor から開始し、suite capture と分けて記録する。

実行時は各 UI ごとに `ja`、`en`、`zh`、`zh-TW` を `--language` で明示する。Render Manager 以外に追加7言語を適用しない。解像度、DPI、theme、window geometry、build revision、入力 fixture/hash、capture 日を manifest に記録する。

## 評価ケース

| 面 | 操作 | 合否の証拠 |
|---|---|---|
| Components | component 選択、設定表示、長い設定群のスクロール | active component が一致、必要な行が欠けず最終行へ到達、選択変更後も対象が正しい |
| Components | 数値／enum／bool の変更、別 component へ移動して戻る | 値が正しく反映され、対象外の layer property が変化せず、既存 Undo で復元可能 |
| Components | 保存して再読込 | component enabled state と値が一致し、prerequisite が保たれる |
| Color Wheels | wheel と数値を操作、reset、undo | wheel・数値・preview が同期し、neutral と reset が一貫し、Undo が元値を復元 |
| FloatColorPicker | tab／channel／HEX／history／cancel | RGB/HSL/HSV/Lab/XYZ の値整合、alpha 保持、cancel 時未確定値の破棄、直近色の扱いを確認 |
| Effect editor | 選択・有効切替・専用コントロール編集・filter／preset | 対象 effect と表示値が一致し、preview、保存、Undo が一貫する。descriptor fallback は識別できる |
| Locales | 上記の主要画面 | 文字欠け、切れ、誤翻訳、fallback、tab order／keyboard focus を記録 |

各ケースで initial / interaction / final screenshot と fixture result を保持し、編集前後の property snapshot、Undo 後 snapshot、保存再読込 snapshot を比較する。Color Wheel の既存結果はその専用dialogのpointer操作証拠に限り、一般 color property やComponent面の証拠に転用しない。UI capture のみでは renderer の画質・GPU parity・性能の合格とはしない。

## 追加専用 UI の優先候補

1. **一般色プロパティの wheel + precise controls**: 色相／彩度／明度をホイールで素早く選び、float RGBA と既存色空間 tab で正確に詰める。既存 Color Wheels を複製せず、共通 color property editor として承認済み FloatColorPicker の責務を確認してから設計する。
2. **Effect descriptor presentation**: enum、range、color、curve、gradient、region 等を descriptor が明示した場合だけ専用 widget を選択し、未分類 property は既存 editor に fallback する。UI 標準化マイルストーンの未完了契約を先に棚卸しする。
3. **Component-specific controls**: 表示モードや分布等に専用 editor が有効な component だけに限定する。まず現在の埋め込み editor で編集・Undo・保存の到達性を検証し、component 個別 UI は操作上の不足が記録されたものから追加する。

## 画像エフェクト R&D との分離

本評価は UI 操作性の評価であり、エフェクト画像品質の R&D 完了を意味しない。新しい画像処理を追加・評価する場合は `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従い、対象画像、baseline、典型／失敗 fixture、色・alpha、時間、preview/final、D3D12/Vulkan、性能、統計、目視合否を開始前に別計画へ固定する。UI の色ホイールやパラメータ editor 自体は画像効果の評価対象ではない。

## 未実施・判定保留

- 実画面 capture、仮想操作、multi-locale review: 未実施（明示的な実行指示待ち）。
- Component 値編集、Undo/Redo、save/load: harness の合否条件が不足。次のハーネス拡張では座標クリックに依存せず安定した objectName / public property path を使い、値変更前後、別component／別layer切替後、Undo/Redo、project save/loadをresultへ記録する。既存 fixture の no-window 経路で可能な操作を優先し、描画・イベント配線の追加は行わない。
- FloatColorPicker の UI 操作: 既存の専用 capture モードがない。カラーホイールとの混同を避け、承認済みカラー picker surface 用の既存公開 API／fixture を検討する。
- Effect descriptor controls: descriptor の UI 到達、Undo、永続化、preview parity は未検証。まずrack選択で対象property surfaceへ遷移し、専用editorを操作してproperty snapshot・Undo・再読込を検査する操作fixtureを設計する。
- 受入基準・視覚評価者: capture 前に固定する。結果を見た後に閾値を決めない。
