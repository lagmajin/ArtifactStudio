# Milestone M-PR-STYLE-1: ArtifactPr Style 違反修正

| 項目 | 値 |
|---|---|
| Status | **ready to implement** |
| Owner | (TBD) |
| Target phase | Phase A-0 (1〜2 day) |
| Subsystem | `ArtifactPr/src/ArtifactPrMainWindow.cppm`, `ArtifactPr/src/VideoPlayerWidget.cppm` |
| Created | 2026-06-16 |
| Related | `REPORT_ARTIFACT_PR_IMPLEMENTABILITY_2026-06-16.md` / AGENTS.md / `Artifact/src/Widgets/CommonStyle.cppm` |
| Supersedes | (なし) |
| Touches submodule | `ArtifactPr` のみ |

---

## 1. 背景

### 1.1 痛み

- `ArtifactPr` 配下に **`setStyleSheet(...)` を 26 箇所で新規使用** している
- これは **AGENTS.md / taste の禁止事項** に違反:
  > `QtCSS` / `setStyleSheet()` は絶対に新規追加しないこと。
- 既存 `Artifact/` アプリは **`Artifact::ArtifactCommonStyle` (QProxyStyle + Fusion)** と **`ArtifactCore::currentDCCTheme()` の theme token** を使っている。`ArtifactPr` だけが取り残されている

### 1.2 既存資産 (参考)

| ファイル | 内容 |
|---|---|
| `Artifact/src/Widgets/CommonStyle.cppm` | QProxyStyle + Fusion ベース。`polish(QWidget*)` / `polish(QPalette*)` で全 widget を統一 |
| `Artifact/src/Widgets/Dock/DockGlowStyle.cppm` | QProxyStyle で Dock の見た目調整 |
| `ArtifactCore::currentDCCTheme()` | theme token を返す (color / borderColor / ...) |

→ **`ArtifactPr` でも `ArtifactCommonStyle` を `QApplication::setStyle()` で適用すれば、26 箇所の大部分が自動解決** する。

### 1.3 26 箇所の分類

26 件を **widget 種別 × 役割** で分類:

#### カテゴリ A: QLabel (テキスト色・サイズ) — **9 件**
| 行 | 内容 | 置換 |
|---|---|---|
| 273 | "color: #888; font-size: 11px;" (description) | `ArtifactCore::theme().textColorMuted` |
| 295 | "color: #666; font-size: 11px;" (info) | 同上 |
| 721 | "color: #666; font-size: 11px;" (info) | 同上 |
| 792 | "color: #666; font-size: 11px;" (info) | 同上 |
| 831 | "color: #888;" (clipNameLabel) | `theme().textColorSecondary` |
| 876 | "color: #666; font-size: 11px;" (info) | 同上 |
| 1267 | "color: #888;" (zoom label) | 同上 |
| 1283 | "color: #aaa; min-width: 40px;" (zoomLevel) | `theme().textColorPrimary` |
| 1289 | "color: #888; padding: 2px 8px;" (sequenceInfo) | 同上 |
| 1720 | "background-color: #252525; color: #aaa; padding: 2px 8px; border-right: 1px solid #333;" (nameLabel) | **`ArtifactCore::theme().panelBackground`** + `theme().borderSubtle` |

#### カテゴリ B: QPushButton (背景色) — **5 件**
| 行 | 内容 | 置換 |
|---|---|---|
| 283 | "background-color: #4a6a8a; color: white; padding: 6px; border-radius: 3px;" (create proxy) | `theme().buttonPrimary` |
| 288 | "background-color: #4a8a4a; color: white; ..." (use proxy) | `theme().buttonSuccess` |
| 702 | "background-color: %1; color: white; ..." (transition button, color 引数) | **保留** — 動的色。`setProperty("themeOverride", color)` + QProxyStyle で対応 |
| 1261 | toolbar container 背景 | カテゴリ D 参照 |

#### カテゴリ C: QListWidget / QLineEdit (入力系) — **4 件**
| 行 | 内容 | 置換 |
|---|---|---|
| 277 | "background-color: #2a2a2a; color: white; border: none;" (proxyList) | `theme().panelBackgroundSubtle` |
| 762 | "background-color: #333; color: white; padding: 4px; border: 1px solid #555; border-radius: 3px;" (searchEdit) | `theme().inputBackground` + `theme().borderSubtle` |
| 766 | "background-color: #2a2a2a; color: white; border: none;" (effectsList) | 同上 |
| 864 | "background-color: #333; color: white; padding: 4px; border: 1px solid #555; border-radius: 3px;" (speedCombo) | 同上 |

#### カテゴリ D: QWidget コンテナ背景 — **5 件**
| 行 | 内容 | 置換 |
|---|---|---|
| 972 | TimelineRulerWidget: "background-color: #2a2a2a; color: #777;" | `theme().timelineRulerBackground` |
| 1261 | toolbarWidget: "background-color: #252525;" | `theme().panelBackground` |
| 1724 | trackContent: "background-color: #1e1e1e;" | `theme().trackContentBackground` |
| 23 | VideoPlayerWidget placeholder: "background-color: #1a1a1a; color: #555;" | `theme().mediaPlaceholderBackground` |
| 1990 | dockManager: KDDockWidgets 専用 | **保留** — QProxyStyle ではなく `DockStyleManager::applyTheme()` 経由 (既存パターン) |

#### カテゴリ E: QSlider (サブコントロール) — **2 件**
| 行 | 内容 | 置換 |
|---|---|---|
| 843 | volumeSlider: groove + handle の独自スタイル | **`ArtifactCommonStyle::drawControl` で全 QSlider 統一** |
| 1276 | zoomSlider: 同上 | 同上 |

#### カテゴリ F: その他 — **1 件**
| 行 | 内容 | 置換 |
|---|---|---|
| 869 | reverseCheck_ "color: white;" | `QPalette::WindowText` 経由。QProxyStyle の `polish(QPalette*)` で一括 |

### 1.4 自動 vs 手動

| カテゴリ | 件数 | 対応 |
|---|---:|---|
| **自動** (QProxyStyle + theme token で一括解決) | 19 | `ArtifactCommonStyle` を `QApplication::setStyle()` で適用 |
| **半自動** (個別 palette / property 設定) | 6 | `ArtifactPrMainWindow::initTheme()` で widget 個別設定 |
| **保留** (動的色 / DockManager) | 1 | `setProperty("themeOverride", color)` + Dock の setStyleSheet(QString()) クリア |

→ 自動 19 + 半自動 6 = **25 件を完全置換可能**。残り 1 件 (transition button の動的色) は別 milestone で対応。

---

## 2. ゴール

- 26 箇所の `setStyleSheet` を **25 箇所 0 件に削減**
- `ArtifactCommonStyle` を `ArtifactPr` アプリに適用
- 既存見た目を **完全に維持** (theme token は同じ色値を参照)
- `Q_PROPERTY` 経由で **動的色 (transition button) は theme override** 対応
- DockManager の `setStyleSheet(...)` 1 件は **`QString()` でクリア** + `DockStyleManager` 経由に置換

---

## 3. 設計の柱

### 3.1 全体方針

```cpp
// main.cpp
int main(int argc, char** argv) {
    QApplication app(argc, argv);

    // 既存の Artifact::ArtifactCommonStyle を適用 (Fusion ベース + theme token)
    app.setStyle(new Artifact::ArtifactCommonStyle(QStyleFactory::create("Fusion")));

    // Theme token の初期化
    ArtifactCore::initDCCTheme(ArtifactCore::DCCTheme::Dark);

    // 以降の widget 生成は QProxyStyle が自動的に正しい色で描画
    ArtifactPrMainWindow window;
    window.show();
    return app.exec();
}
```

### 3.2 自動置換される 19 件

`ArtifactCommonStyle` の `polish(QWidget*)` と `polish(QPalette*)` で一括適用。**既存コードの変更不要**。`setStyleSheet` をそのまま削除。

具体的な対応:
- **カテゴリ A (QLabel)**: `polish(QPalette*)` で `QPalette::WindowText` を `theme().textColorMuted` に設定
- **カテゴリ B (QPushButton)**: `ArtifactCommonStyle` 内に既存の `drawControl(CE_PushButton...)` があれば色適用
- **カテゴリ C (QListWidget / QLineEdit / QComboBox)**: 同様に palette 適用
- **カテゴリ D (QWidget)**: `setAutoFillBackground(true)` + palette 適用

### 3.3 半自動置換される 6 件

個別 widget で `setProperty("themeOverride", color)` + `ArtifactCommonStyle` が property を読んで動的色制御:

```cpp
// 例: transition button
auto* btn = new QPushButton(name);
btn->setProperty("artifactAccentColor", QVariant::fromValue(color));
QProxyStyle が artifactAccentColor を読み取って CE_PushButtonLabel で色を適用
```

### 3.4 動的色 (transition button) の設計

`ArtifactCommonStyle::drawControl(CE_PushButtonLabel, ...)` で:

```cpp
if (widget->property("artifactAccentColor").isValid()) {
    QColor c = widget->property("artifactAccentColor").value<QColor>();
    // background = c (disabled 時は少し暗く)
    // text = white
    painter->fillRect(option->rect, c);
    painter->setPen(Qt::white);
    painter->drawText(option->rect, Qt::AlignCenter, widget->text());
    return;  // QProxyStyle::drawControl を呼ばない
}
return QProxyStyle::drawControl(element, option, painter, widget);
```

### 3.5 DockManager 対応

`Artifact/src/Widgets/Dock/DockStyleManager.cppm:200` の既存パターンを使う:

```cpp
// 既存: dockManager->setStyleSheet(QString());  // クリア
// 加えて: DockStyleManager::applyTheme(dockManager, theme());  // 動的適用
```

---

## 4. フェーズ計画

### 4.1 Phase 1: ArtifactCommonStyle 適用 (Day 1 午前)

| タスク | 完了条件 |
|---|---|
| `ArtifactPr/src/main.cpp` で `QApplication::setStyle(new Artifact::ArtifactCommonStyle(...))` を追加 | 起動時に QProxyStyle が適用される |
| `ArtifactPr/src/ArtifactPrMainWindow.cppm` の `setStyleSheet(QStringLiteral("ads::CDockWidget..."))` を **削除** し、`DockStyleManager::applyTheme(dockManager, theme())` に置換 | dockManager 違反が解消 |
| `ArtifactCore::initDCCTheme(ArtifactCore::DCCTheme::Dark)` を main.cpp で呼ぶ | theme token が有効 |

**触るファイル**:
- `ArtifactPr/src/main.cpp` (確認 → 新規 import 追加)
- `ArtifactPr/src/ArtifactPrMainWindow.cppm:1990` (1 箇所削除)
- 新規: `ArtifactPr/include/AppTheme.ixx` (theme override property の宣言)

### 4.2 Phase 2: 自動置換 (Day 1 午後)

| タスク | 完了条件 |
|---|---|
| 26 箇所のうち、**自動置換可能な 19 件** を `setStyleSheet` 削除 | `git grep "setStyleSheet" ArtifactPr/src/` が **7 件以下** に減る |
| 既存テスト / ビルドが pass | CI green |

**触るファイル**:
- `ArtifactPr/src/ArtifactPrMainWindow.cppm` (19 箇所削除)
- `ArtifactPr/src/VideoPlayerWidget.cppm` (1 箇所削除)

### 4.3 Phase 3: 半自動置換 (Day 2 午前)

| タスク | 完了条件 |
|---|---|
| transition button の動的色を `setProperty("artifactAccentColor", ...)` 化 | line 702 の `setStyleSheet(...)` が `setProperty(...)` に置換 |
| `ArtifactCommonStyle::drawControl(CE_PushButtonLabel, ...)` を拡張 | artifactAccentColor を読み取って描画 |
| 既存 6 件の見た目確認 (volume slider / zoom slider / reverse check 等) | 全 widget の表示が既存と同等 |

**触るファイル**:
- `Artifact/src/Widgets/CommonStyle.cppm` (drawControl 拡張)
- `ArtifactPr/src/ArtifactPrMainWindow.cppm:702` (setProperty 化)
- 残り 5 件の個別 palette / property 設定

### 4.4 Phase 4: 検証 (Day 2 午後)

| タスク | 完了条件 |
|---|---|
| `git grep "setStyleSheet" ArtifactPr/src/` 実行 | 違反 0 件 (transition button の動的色は property 経由) |
| 主要 widget (MediaPanel / ProjectPanel / TransportBar / VideoPlayer / Timeline / Effects / Volume / Proxy) を screenshot で確認 | 既存と同等の見た目 |
| Light / Dark theme 切替テスト | 両方で見え方が破綻しない |
| Unit test 追加 (widget が property 経由で theme override を読めるか) | test pass |

---

## 5. 既存 milestone / コードとの接続

### 5.1 既存 / 並走

| milestone / コード | 接続 |
|---|---|
| `Artifact/src/Widgets/CommonStyle.cppm` | **直接流用**。QProxyStyle + Fusion |
| `ArtifactCore::currentDCCTheme()` | **直接流用**。theme token の source of truth |
| `Artifact/src/Widgets/Dock/DockStyleManager.cppm:200` | 既存パターン。`QString()` でクリア + `applyTheme()` |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` | diagnostics 文法と整合 |

### 5.2 触らないもの

- `ArtifactWidgets/` サブモジュール (明示依頼時のみ)
- `ArtifactCore/` サブモジュール (theme token の追加は別 milestone で要相談)
- `libs/`, `third_party/*`

---

## 6. リスクと軽減

| リスク | 影響 | 軽減策 |
|---|---|---|
| **`ArtifactCommonStyle` を ArtifactPr で使うと、見た目が大きく変わる** | UX 破壊 | theme token を ArtifactPr 既存の色値 (`#2a2a2a`, `#4a6a8a` 等) と一致させる。Phase 4 で screenshot 比較 |
| **`ArtifactCore` の theme token が未公開** | import できない | `ArtifactPr` から `import ArtifactCore;` でアクセス可能か確認 → 不可能なら `ArtifactPr` 内に theme ミラーを作成 |
| **DockManager の KDDockWidgets 用 style** | 既存 1 件の違反が残る | `DockStyleManager::applyTheme` の既存パターンで対応 |
| **Transition button の動的色** | theme override で色再現できない | `ArtifactCommonStyle` に `artifactAccentColor` property ハンドラを追加 |
| **submodule bump 手順** | push のたびに手順が必要 | `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠。ArtifactPr 完了 → parent gitlink 更新 → parent push |

---

## 7. 不変条件 (AGENTS.md / taste 整合)

### 7.1 守るべきルール

- **新規 `QtCSS` / `setStyleSheet(...)` の追加禁止** (AGENTS.md)。本 milestone は違反の解消
- **`QImage` の新規採用禁止**。本 milestone では触らない
- **新規 signal-slot 接続の追加禁止**。既存 `connect` の範囲で対応
- **theme token は `ArtifactCore::currentDCCTheme()` の single source of truth**

### 7.2 推奨パターン

```cpp
// OK: theme token 経由
widget->setPalette(ArtifactCore::currentDCCTheme().panelBackground);
widget->setProperty("artifactAccentColor", color);

// NG: setStyleSheet
widget->setStyleSheet("background-color: #2a2a2a; color: white;");
```

---

## 8. Done Criteria (Definition of Done)

- [ ] Phase 1: `QApplication::setStyle(new Artifact::ArtifactCommonStyle(...))` が main.cpp で動作
- [ ] Phase 1: dockManager の `setStyleSheet(QStringLiteral("ads::CDockWidget..."))` 削除
- [ ] Phase 2: 自動置換可能な 19 件の `setStyleSheet` がすべて削除
- [ ] Phase 3: 残り 6 件の `setStyleSheet` が property / palette 経由に置換
- [ ] Phase 3: transition button の動的色が `artifactAccentColor` property で再現
- [ ] Phase 4: `git grep "setStyleSheet" ArtifactPr/src/` の結果が **0 件**
- [ ] Phase 4: Light / Dark theme 切替で全 widget が破綻しない
- [ ] Phase 4: 主要 widget の screenshot が既存と同等
- [ ] Unit test で `artifactAccentColor` の読み取りを検証
- [ ] submodule bump が `.github/GIT_WORKFLOW_PARENT_CHILD.md` 通り完了
- [ ] `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の diagnostics 文法と整合
- [ ] `ArtifactCore::currentDCCTheme()` への新規 token 追加が 0 件 (既存 token でカバー)

---

## 9. 関連ファイル (変更)

### 9.1 変更

| ファイル | 変更内容 |
|---|---|
| `ArtifactPr/src/main.cpp` | QApplication::setStyle() 追加 |
| `ArtifactPr/src/ArtifactPrMainWindow.cppm` | 26 箇所中 25 箇所の setStyleSheet 削除 + property / palette 置換 |
| `ArtifactPr/src/VideoPlayerWidget.cppm:23` | 1 箇所の setStyleSheet 削除 + palette 化 |
| `Artifact/src/Widgets/CommonStyle.cppm` | `drawControl(CE_PushButtonLabel, ...)` で `artifactAccentColor` property ハンドラ追加 |
| `ArtifactPr/include/AppTheme.ixx` | 新規。theme override property の宣言 |

### 9.2 新規

| ファイル | 役割 |
|---|---|
| `ArtifactPr/include/AppTheme.ixx` | theme override property / helper |
| `ArtifactPr/src/AppTheme.cppm` | 同上 実装 |
| `ArtifactPr/tests/StyleRemediationTest.cpp` | artifactAccentColor / theme override 検証 |

### 9.3 触らない

- `ArtifactWidgets/` サブモジュール
- `ArtifactCore/` サブモジュール (theme token の追加は別 milestone)
- `libs/`, `third_party/*`

---

## 10. 実装メモ (開発者向け)

### 10.1 ArtifactPr 側で import する module

```cpp
import Artifact;
import ArtifactCore;
// QApplication::setStyle(new Artifact::ArtifactCommonStyle(QStyleFactory::create("Fusion")));
// ArtifactCore::initDCCTheme(ArtifactCore::DCCTheme::Dark);
// ArtifactCore::currentDCCTheme() を widget ごとに参照
```

### 10.2 property 名 (予約)

```cpp
static const char* kArtifactAccentColor = "artifactAccentColor";  // 動的色 (transition button)
static const char* kArtifactSurfaceKind = "artifactSurfaceKind";  // "trackContent" / "mediaPlaceholder" / ...
```

### 10.3 検証コマンド

```bash
# 違反件数 (0 が目標)
git grep "setStyleSheet" ArtifactPr/src/

# 違反箇所の行番号付き
findstr /S /N /C:"setStyleSheet" ArtifactPr\src\*.cppm
```

---

## 11. 更新履歴

- 2026-06-16: 初版作成。26 箇所を 4 カテゴリに分類。`ArtifactCommonStyle` 流用で 19 件自動 / 6 件半自動 / 1 件動的色 (property 化) の方針を確定。