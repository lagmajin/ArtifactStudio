# Dialog design and implementation rules

**最終更新:** 2026-09-21

ArtifactStudio の基本ダイアログを、暗色テーマ、入力密度、キーボード操作、mutation 経路の面で一貫させるための共通規則。専門ダイアログに固有の承認済み仕様がある場合は、その仕様を優先する。

## 1. 責務

- 1つのダイアログは、1つの明確な判断または設定確定に集中する。
- ダイアログは入力、検証結果、accept / reject を扱い、domain object を直接変更しない。
- mutation は既存 service、command、Undo 経路へ委譲する。UI ごとの代替 mutation 経路を作らない。
- Dock／パネルの配置管理、ドキュメントの保存確認、編集対象の変更など、別責務を同じダイアログへ混在させない。
- 破壊的、長時間、外部出力を伴う操作は通常の入力ダイアログと分離し、影響と対象を明示する。

## 2. 基本構造

小型ダイアログは次の順を基本とする。

1. タイトルバー: 16px で読めるソリッド系アイコン、短いタイトル、閉じる操作。
2. 任意の対象識別: パス、種別、選択件数などを1行で示す。
3. 主入力: ラベルを入力欄の上に置き、視線を左揃えにする。
4. 補助説明または検証メッセージ: 対応する入力欄の直下に置く。
5. 必要最小限の読取専用情報。
6. 細い区切りとフッター。右端へ secondary action、その右に primary action を置く。

- 単純な1項目ダイアログはおおむね 440–560px 幅を基準とし、情報量に応じて調整する。
- 外周余白は 20–24px、関連項目間は 8px、セクション間は 16–20px を目安とする。
- 入力欄と主要ボタンは 32px 以上の高さを確保する。
- 単純なフォームをカードで細分化しない。枠線と区切り線は階層の判読に必要な箇所だけに使う。
- 文言は短くし、タイトルと主ボタンは操作の動詞を一致させる。

## 3. 色とスタイル

- 背景、入力面、境界、本文、補助文字、accent、danger、disabled は既存 theme token と `QPalette` を使う。
- accent の具体的な青／アンバーは画像からハードコードしない。現在の theme semantic role を正とする。
- フォーカス、選択、検証エラーを色だけで区別せず、境界、テキスト、enabled state を併用する。
- `setStyleSheet()` / QtCSS を新規追加しない。必要な外観差は `QPalette`、owner-draw、`QProxyStyle`、既存 theme token で実現する。
- 細線や装飾過多を避け、アイコンは `Artifact/App/Icon/Studio/` の方針に沿うオリジナル SVG を優先する。

## 4. 入力と検証

- 最も重要な入力へ初期フォーカスを置く。既存値の置換が主目的なら、開いた時点で値を全選択する。
- 空文字、範囲外、書式不正、重複など予測可能なエラーは入力欄の直下へインライン表示する。通常の検証に別 `QMessageBox` を重ねない。
- エラー時も入力欄のフォーカスとユーザー入力を保持し、主操作を無効化する。
- 検証文は「何が問題か」を1文で示し、必要なら次の1文で回復方法を示す。
- 入力の trim、正規化、重複判定は domain の命名規則と一致させる。表示上だけ直して別の値を保存しない。
- 値が変化していない場合は不要な command や Undo entry を生成しない。
- 処理中は二重確定を防ぎ、主操作を無効化する。長時間処理は UI thread を占有せず、既存の非同期処理経路を使う。

## 5. 操作とアクセシビリティ

- `Enter` は有効な primary action、`Escape` は Cancel と同じ reject とする。Qt 標準のダイアログ操作を利用し、イベントハンドラへ固定キーを重ねて実装しない。
- タイトルバーの閉じる操作は、明示仕様がない限り Cancel と同じ結果にする。
- フォーカス順は表示順と一致させ、ラベルを入力コントロールへ関連付ける。
- アイコンだけの操作には accessible name と tooltip を付ける。重要な意味をアイコンだけに依存させない。
- 文字拡大、翻訳、125–200% DPI で切れないレイアウトを使い、固定幅は必要最小限にする。
- disabled、error、busy の各状態をスクリーンリーダーへ伝えられるテキストでも表現する。

## 6. Qt / C++ 実装

- 専用 UI が必要な操作は `QDialog` ベースで実装し、呼出側は結果と値を受け取って既存 service / command を呼ぶ。
- 新規 signal / slot を追加せず、既存 callback、command、service、event 経路を再利用する。
- 使用する Qt 型はファイル側で直接 include し、C++20 module の `#include` は global module fragment にのみ置く。
- PImpl を新設する場合はリポジトリ規則に従い、`Impl*` の明示所有とデストラクタでの解放を基本とする。
- `QColorDialog`、`QImage`、`QPainter` / Qt CompositionMode をダイアログ都合で新規導入しない。必要な既存 picker、buffer、owner-draw 経路を再利用する。
- ダイアログ表示中に毎フレーム画像生成、形式変換、大容量確保を行わない。プレビューが必要なら入力変更時などのコールドパスで更新する。
- 新しいウィジェット責務を作る場合は `docs/WIDGET_MAP.md` を更新する。

## 7. 確認項目

- 通常、focus、invalid、disabled、busy、Cancel の各状態が判読できる。
- マウス、Tab、Shift+Tab、Enter、Escape、タイトルバー close で同じ結果になる。
- accept 時だけ既存 service / command が1回呼ばれ、Cancel と無変更確定では mutation が発生しない。
- Undo / Redo 後に選択、表示名、関連タブ、Project、Timeline が既存同期経路で一致する。
- 同名、空白、長い名前、Unicode、IME、翻訳、DPI を確認する。
- QtCSS、新規 signal / slot、直接 domain mutation、別系統の Undo 実装が増えていない。

## 関連資料

- [Composition creation dialog](../composition-create-dialog/README.md)
- [Pre-compose dialog](../precompose-dialog/README.md)
- [Rename dialog](../rename-dialog/README.md)
