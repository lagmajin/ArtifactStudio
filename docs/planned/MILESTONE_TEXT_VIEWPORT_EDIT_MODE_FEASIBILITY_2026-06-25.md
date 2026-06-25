# Text Viewport Edit Mode Feasibility (2026-06-25)

## Conclusion

`Artifact` で「テキストレイヤー専用の Viewport Edit Mode」を作るのは実現可能。
ただし、単なるダブルクリック編集ではなく AE より強い直接編集体験にするなら、既存の `QPlainTextEdit` overlay を最終形にせず、Composition Editor 側に text edit session / layout hit-test / text overlay drawing を明示的に持たせる必要がある。

おすすめは次の方針。

1. 通常モードは今の select / move / scale / rotate を維持する。
2. Text Edit Mode は text layer 選択時だけ有効な modal viewport session として追加する。
3. 最初は既存の inline editor と `TextGizmo` を整理して、BBox / resize / commit / cancel を安定化する。
4. その後、caret / selection / baseline / glyph hit-test / inline HUD を段階的に追加する。

## Current State

既存コードには、土台になる要素がすでにある。

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
  - text layer をダブルクリックすると inline edit を開始する。
  - 実体は viewport 上に `QPlainTextEdit` を重ねる方式。
  - `Ctrl+Enter` で commit、`Escape` で cancel、focus out で commit。

- `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`
  - text layer 選択時に BBox outline と corner handle を描画する。
  - hit-test / cursor shape / mouse drag による text box resize がある。
  - resize は `ArtifactTextLayer::setMaxWidth()` / `setBoxHeight()` に反映する。

- `Artifact/src/Layer/ArtifactTextLayer.cppm`
  - `TextLayoutMode::Point / Box / Path` を持つ。
  - `text.maxWidth` と `text.boxHeight` が property / JSON / render key / draw path に通っている。
  - box text では `paragraphStyle_.boxWidth` / `boxHeight` を使って text rect を決める。
  - glyph shaping result と layout contract を内部に保持している。

- `docs/WIDGET_MAP.md`
  - Composition Editor は viewport 操作の担当。
  - `ArtifactCompositionRenderWidget` は viewport drawing / overlay drawing / direct manipulation surface。
  - `Overlay.Composition` は guides / HUD / snap hints / transform handles の担当。

つまり、「Viewport 上で text layer を直接編集する」入口は既にあり、専用 mode 化は拡張で進められる。

## Gap Analysis

| Feature | Current | Feasibility | Notes |
| --- | --- | --- | --- |
| Viewport 上の直接入力 | 部分実装あり | 高 | `QPlainTextEdit` overlay が既にある。まずはこれを Text Edit Mode の session として管理する。 |
| Text BBox 表示 | 部分実装あり | 高 | `TextGizmo` が outline / corner handles を描く。回転・スケール時の正確な見え方は要調整。 |
| Baseline 表示 | 未実装 | 中 | glyph layout / font metrics から描ける。複数行・縦書き・ruby・path text で仕様を分ける必要あり。 |
| Anchor handle | 既存 overlay あり | 高 | Composition Editor に anchor overlay / anchor edit action がある。text mode の handle と統合できる。 |
| Text box resize handle | 部分実装あり | 高 | `TextGizmo` が maxWidth / boxHeight を更新する。左/上ハンドル時の position 補正が未整理。 |
| Margin/Padding handle | 未実装 | 中 | 現状は text effect margin が内部計算。ユーザー編集可能な padding property を追加するなら Layer model 変更が必要。 |
| Selection highlight | 未実装 | 中-高 | glyph index range と rect 群が取れれば描ける。IME composition 中の表示は Qt input method と同期が必要。 |
| Caret 表示 | `QPlainTextEdit` 任せ | 中 | overlay widget 方式なら簡単。自前描画に移すなら cursor index / blink / IME preedit が必要。 |
| Inline HUD | 未実装 | 高 | Composition overlay の HUD と PropertyEditor の既存 property path を使える。新規 signal/slot は増やさず command/service 経由が良い。 |

## Recommended Architecture

### 0. Separate Quick Edit and Dedicated Text Editor

Composition Editor 側の Text Edit Mode は、あくまで「コンポジット中に軽く直す」ための quick edit として扱うのがよい。
本格的な文字組み・範囲編集・行編集・Animator 調整・baseline inspection は、別の text-focused editor widget に分ける。

推奨責務分担:

| Surface | Role | Editing Depth |
| --- | --- | --- |
| Composition Editor Text Edit Mode | 配置済み text layer の現場修正、box resize、anchor / bbox 確認 | Quick |
| Dedicated Text Editor Widget | 文字入力、range selection、glyph/line/baseline inspection、font/style/layout/animator の本格編集 | Full |
| Inspector / Property Editor | 数値・プリセット・詳細 property の安定編集 | Exact |

この分離にすると、Composition Editor の event routing を重くしすぎず、Text 専用 editor では IME / caret / selection / glyph hit-test / HUD / layout diagnostics を深く作り込める。

### 0.1 Diligent Native Text Editor Widget

本格編集用 widget は Qt text widget ではなく、Diligent ネイティブ描画を使う専用 surface が合っている。

候補名:

- `ArtifactTextEditorWidget`
- `ArtifactTextLayoutEditorWidget`
- `TextLayerEditorSurface`

基本構成:

- Qt widget shell は input / focus / docking / menu integration を担当する。
- 表示は `ArtifactIRenderer` / `PrimitiveRenderer2D` / Diligent surface を使う。
- glyph / baseline / selection / caret / range selector / inline HUD を同じ renderer overlay として描く。
- 入力は Phase 1 では Qt input method を受け、内部 text model に反映する。
- 描画結果は `ArtifactTextLayer` の model / `TextLayoutContract` / glyph layout を single source of truth にする。

この専用 widget の価値:

- `QPlainTextEdit` overlay の矩形制約から抜けられる。
- rotated / scaled / vertical writing / ruby / path text の表示と編集を、実際の layout contract に合わせられる。
- baseline / ascender / descender / line box / cluster boundary / selection range を常時 inspect できる。
- Text Animator の range selector を文字単位で直接編集できる。
- 将来の GPU direct glyph draw と同じ見た目で編集できる。

ただし、低レベル Diligent / DX12 backend を直接広く触る必要はない。
まずは既存の renderer abstraction と `PrimitiveRenderer2D` の text draw 系を使い、必要な primitive が足りない場合だけ薄く追加する。

### 0.2 Text Editor Widget Milestone

`M-CE-TEXT-1 Text Viewport Edit Mode` の次に、専用 editor を別 milestone として切る。

推奨マイルストーン名:

`M-TEXT-EDITOR-1 Diligent Native Text Editor Widget`

初期スコープ:

1. Dockable `ArtifactTextEditorWidget` shell を追加する。
2. 選択中の `ArtifactTextLayer` を editor target として開く。
3. Diligent surface 上に paragraph box / line boxes / baseline / glyph boxes を描く。
4. caret / selection range を renderer overlay として描く。
5. text 入力は Qt input method event を受けて `ArtifactTextLayer::setText()` に反映する。
6. font size / leading / tracking / alignment の mini HUD を Diligent overlay で描く。
7. commit / revert / apply-to-layer の流れを固定する。

将来スコープ:

- range selector の直接編集
- per-character style override
- ruby / vertical writing inspector
- path text editor
- typography preset browser
- GPU glyph atlas / direct glyph draw の preview

この専用 editor は新規 `.cppm` / `.ixx` と CMake 登録を伴う可能性が高い。
実装時は Composition Editor への小改修よりビルド影響が大きいため、まず設計ファイルと widget map 更新を入れてから段階実装する。

### 1. Add a Text Edit Session

Composition Editor / Render Controller に、通常 transform drag とは別の `TextViewportEditSession` 相当の状態を持たせる。

責務:

- 対象 text layer id
- editing state: inactive / hovering / editing / selecting / resizing / moving anchor
- caret index
- selection anchor / selection active end
- IME preedit text
- active text handle
- inline HUD visibility and placement

これは新しいグローバル signal/slot ではなく、既存の viewport event routing と controller method で閉じるのが安全。

### 2. Keep `QPlainTextEdit` for Phase 1, then Reduce It

最初から完全自前 text editor を作ると IME / selection / undo / clipboard で重くなる。

Phase 1 は既存の `QPlainTextEdit` overlay を Text Edit Mode の実体として使い、以下だけを整える。

- layer transform に追従して overlay geometry を更新
- mode enter / exit を明示化
- commit / cancel / focus loss の挙動を固定
- text layer 選択解除時に安全に閉じる
- resize 中は editor を隠すか geometry を追従

Phase 2 以降で、caret / selection / baseline / bbox を renderer overlay で描き、入力だけ Qt text control に寄せる hybrid にする。

### 3. Expose Text Layout Metrics

Text Edit Mode の本命は、文字単位 hit-test と visual feedback。
そのため `ArtifactTextLayer` 内部の shaping / layout 結果を editor overlay から読める contract が必要。

最小 API 候補:

- text box rect in layer local coordinates
- line boxes
- baseline segments
- glyph/cluster boxes
- char index -> caret rect
- viewport point -> char index
- selection range -> highlight rects

注意点:

- `.ixx` に重い import を増やさない。
- まずは既存の `Text.LayoutContract` / `Text.GlyphLayout` を再利用し、公開 API は小さくする。
- 実装詳細は `.cppm` に閉じる。

### 4. Split Handles by Mode

通常モード:

- layer selection
- move
- scale
- rotate
- generic anchor overlay

Text Edit Mode:

- text bbox
- line/baseline overlay
- caret
- selection highlight
- text box resize
- padding/margin handles
- anchor handle
- inline HUD

`TextGizmo` は今のままだと text animator preview / box resize / future character selection が混ざりやすい。
将来的には以下に分けると読みやすい。

- `TextBoxGizmo`: bbox / resize / padding
- `TextCaretOverlay`: caret / selection / IME preedit
- `TextLayoutOverlay`: baseline / line boxes / glyph boxes
- `TextInlineHud`: font / size / leading / tracking

ただしファイル追加は CMake 登録を伴うので、初手は既存 `ArtifactTextGizmo.cppm` と `ArtifactCompositionEditor.cppm` 内で小さく整理してから分割を判断する。

## Implementation Plan

### Phase A: Mode Shell

- Text layer double click or Text tool clickで Text Edit Mode に入る。
- Escape で通常モードへ戻る。
- mode active 中は通常 layer drag を抑制する。
- mode state を debug overlay / status text で確認できるようにする。

Risk: event routing の衝突。
Mitigation: `CompositionRenderController::handleMousePress/Move/Release` の先頭で text session が処理できるか判定し、処理済みなら generic selection へ流さない。

### Phase B: Stable Box Editing

- `TextGizmo` の box resize を「右/下」だけでなく「左/上」でも layer position と box size が破綻しないようにする。
- box resize 後に property panel / render cache / undo 相当の更新経路を確認する。
- rotated layer での handle hit-test を改善する。

Risk: 現状の bbox は transformed bounding box なので、local text box と world transform が混ざる。
Mitigation: local rect -> world corners -> viewport の順で描画し、property 更新は local width/height に限定する。

### Phase C: Layout Overlays

- line box
- baseline
- paragraph box
- glyph/cluster debug boxes

最初は debug toggle 扱いでよい。
常時表示は情報量が多いので、Text Edit Mode 中だけ濃く表示する。

Risk: Qt text draw と custom glyph layout の差。
Mitigation: まずは render path が使う layout contract から描く。差が出る場合は mismatch を diagnostic として出す。

### Phase D: Caret / Selection / IME

- `QPlainTextEdit` overlay から cursor position / selection を読み、renderer overlay に反映する。
- IME preedit は最初は Qt widget の表示に任せる。
- 自前 caret は QPlainTextEdit を透明化または最小表示にできる段階で導入する。

Risk: 日本語 IME で composition window / candidate window の位置がずれる。
Mitigation: Phase D までは native Qt text edit の IME を使い、完全自前化は後回し。

### Phase E: Inline HUD

- HUD は selection 近傍または bbox 上部に出す。
- font family / size / leading / tracking / alignment を最小セットにする。
- 既存 property path (`text.fontSize`, `text.leading`, `text.tracking`, `text.maxWidth`, `text.boxHeight` など) を使う。

Risk: PropertyEditor と二重編集になる。
Mitigation: HUD は quick edit、Inspector は full edit と位置付ける。更新経路は同じ property setter に寄せる。

## Risk Notes

- `QImage` を増やす必要はない。編集 overlay は viewport/UI 側で完結できる。
- `QColorDialog` / QtCSS / 新規 global signal-slot は不要。
- Diligent / DX12 backend を触る必要は基本的にない。
- `.ixx` の公開 API 拡張は慎重にする。layout metrics の公開は小さな struct / query method に絞る。
- 新規 `.cppm` を増やす場合は CMake 明示登録が必要。最初の調査実装では既存ファイル内の整理を優先する。
- build / CMake / test は明示許可が必要。実装時はまず静的確認と差分確認から始める。

## Verdict

実現可能性は高い。

特に `direct input` / `BBox` / `text box resize` は既存実装があるため、最初の milestone は大きくない。
一方で、強い Text Edit Mode として価値が出るのは `layout metrics` を viewport overlay に出せるようになってから。

推奨マイルストーン名:

`M-CE-TEXT-1 Text Viewport Edit Mode`

推奨スコープ:

1. Text Edit Mode state を追加
2. double click / Text tool で mode enter
3. existing inline editor を session 管理へ移す
4. BBox / resize handle を mode 中の正規 overlay にする
5. baseline / line box の debug overlay まで入れる

この範囲なら既存の設計に沿って進めやすく、後続の caret / selection / IME / HUD へ自然につながる。
