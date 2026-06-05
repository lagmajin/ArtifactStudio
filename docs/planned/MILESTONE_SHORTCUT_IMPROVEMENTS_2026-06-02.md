# ショートカット・操作感改善 マイルストーン

> Generated: 2026-06-02

## 背景

AEライクな操作感を目指しているが、ショートカット・キー操作で不足しているものが多い。
特に AE 必須の矩形選択、レイヤー複製、レイヤー重ね順、クリップ分割などが未実装で、
現状ではマウスドラッグ操作で補っており、キーボード中心のワークフローが阻害されている。

## 現状のAEライク要素 (OK)

| 操作 | 実装 |
|---|---|
| V/H/Z/W/E/R ツール切替 | `ArtifactCompositionRenderWidget.cppm` |
| Space 一時 Hand パン + 再生切替 | `ArtifactCompositionEditor.cppm` |
| Arrow ナッド (Shift x10) | `ArtifactCompositionRenderWidget.cppm` |
| `[]` レイヤー IN/OUT | `ArtifactCompositionRenderWidget.cppm` |
| `` ` `` Solo, `H` 表示切替/Shift+H Solo | `ArtifactCompositionEditor.cppm` |
| `K` 再生コントロール (J/K/L) | `ArtifactPlaybackShortcuts.cppm` |
| `I/O` In/Out, `M` Marker | `ArtifactPlaybackShortcuts.cppm` |
| タイムラインキーフレームコピペ | `ArtifactTimelineKeyBinding.ixx` |

## 追加すべきショートカット

### P0: 必須 (AE体験の根幹)

| 操作 | Keys | 実装箇所 |
|---|---|---|
| 矩形マルチ選択 (Marquee Select) | Viewport 空ドラッグ | `ArtifactCompositionRenderWidget.cppm` mouse handler |
| レイヤー複製 | `Ctrl+D` | `ArtifactCompositionRenderWidget.cppm` keyPressEvent |
| レイヤー重ね순 | `Ctrl+]`, `Ctrl+Shift+]`, `Ctrl+[`, `Ctrl+Shift+[` | `ArtifactCompositionRenderWidget.cppm` keyPressEvent |
| タイムライン Split | `S` (再生ヘッドでclip分割) | `ArtifactTimelineWidget.cpp` / `ArtifactTimelineTrackPainterView.cpp` |
| 選択キーフレーム削除 | `Delete` (選択済みkeyframes) | `ArtifactTimelineWidget.cpp` keyPressEvent |

### P1: 高優先度

| 操作 | Keys | 備考 |
|---|---|---|
| Zoom In | `=` / `+` | キーボードからのズーム |
| Zoom Out | `-` | 同上 |
| 100% Reset | `0` | Fit Frame ではない |
| Fit to Layer | `Shift+F` | レイヤーをviewportにフィット |
| Snap toggle | `Ctrl+;` | ビュースナップ ON/OFF |
| Pre-compose | `Ctrl+Shift+C` | プリコンポ化 |
| Inspector フォーカス | `T` | プロパティパネルへ |
| Align (L/R/T/B) | レイヤー整列 | 選択レイヤー整列 |

### P2: 中優先度

| 操作 | Keys | 備考 |
|---|---|---|
| タイムライン Skip | `,` / `.` | 前後フレームへ |
| ズームアウト | `Shift+Z` | Ctrl+Z 競合回避 |
| レイヤーカット | `Ctrl+X` | カット操作 |

## 既存の binding 修正点

- `Backspace` → `Delete` へ変更 (Mac/Windows両対応)
- `Ctrl+]` 等がメニューに未登録なのでOverrides registryに追加

## 影響範囲

- `ArtifactCore/include/UI/ShortcutBindings.ixx`  — enum拡張
- `ArtifactCore/src/UI/ShortcutBindings.cppm` — デフォルト binding 追加
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` — 矩形選択、レイヤー複製、重ね順
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp` — Split
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp` — keyframes選択削除
