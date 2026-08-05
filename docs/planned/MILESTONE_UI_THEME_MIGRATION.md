# MILESTONE: UI Theme Migration — QSS → Theme Tokens + QCommonStyle

**日付**: 2026-03-31〜2026-04-03（統合+実装詳細化: 2026-08-04）
**最終更新:** 2026-08-05
**今回の実装:** ArtifactCommonStyle を QCommonStyle 基底へ移行し、AppMain の Fusion 生成依存を撤去。QADS の組み込み stylesheet クリアは例外として明文化。
**統合元**: `MILESTONE_QSS_REDUCTION` + `MILESTONE_QSS_EXORCISM_PROPERTY_THEME` + `MILESTONE_QSS_DECOMMISSION_COMMONSTYLE` + `MILESTONE_PROPERTY_WIDGET_UPDATE_CLEANUP_THEME` + `MILESTONE_UI_THEME_SYSTEM_ROLLOUT`
**静的監査**: 2026-07-25 — Phase 1〜3 実装済み。Phase 4 の QCommonStyle 移行、Phase 5 の QADS 例外明文化、Phase 6 の Property Row 状態表示・コンテキスト操作は実装済み。Legacy Knob 群には現行 PropertyEditor への移行先を示す deprecated 注記を追加。実行時確認は未完了。

## 現状

### 完了済み

| 項目 | ファイル | 状態 |
|------|---------|------|
| QSS 新規追加停止 | 全 widget | ✅ `setStyleSheet()` 追加なし |
| `ArtifactCommonStyle` | `Artifact/include/Widgets/CommonStyle.ixx` + `Artifact/src/Widgets/CommonStyle.cppm` | ✅ `QProxyStyle`+Fusion ベース、drawControl/drawPrimitive/drawComplexControl 実装 |
| Theme token ソース | `ArtifactCore::currentDCCTheme()` → backgroundColor/secondaryBackgroundColor/textColor/accentColor/borderColor | ✅ 実装済み |
| AppMain 設定 | `Artifact/src/AppMain.cppm` | ✅ `QApplication::setStyle(new ArtifactCommonStyle)` |
| Property/Inspector 移行 | `ArtifactPropertyWidget.cppm`, `ArtifactInspectorWidget.cppm`, `PropertyEditor/` | ✅ QPalette+theme token+owner-draw |
| Dock 移行 | `DockStyleManager.cppm` + `DockGlowStyle.ixx/.cppm` | ✅ QADS 再スタイル、`DockGlowStyle` |
| Rebuild debounce | `ArtifactPropertyWidget` | ✅ revision/signature/frame cache 実装 |

### 残存する `setStyleSheet()` 呼び出し（1箇所）

`Artifact/src/Widgets/Dock/DockStyleManager.cppm:200`:
```cpp
impl_->dockManager_->setStyleSheet(QString());  // QADS 組み込み light-theme stylesheet のクリア
```
これは QADS の組み込み stylesheet を無効化する後処理で、見た目の定義追加ではない。ただしこの呼び出し自体を `DockGlowStyle` + QPalette アプローチで置き換える余地がある。

---

## Phase 4: QCommonStyle 昇格

`ArtifactCommonStyle` の基底を `QProxyStyle(Fusion)` から `QCommonStyle` に切り替える。

### 現行コード

`Artifact/src/Widgets/CommonStyle.cppm:259`:
```cpp
ArtifactCommonStyle::ArtifactCommonStyle(QStyle* baseStyle)
    : QProxyStyle(baseStyle ? baseStyle : QStyleFactory::create(QStringLiteral("Fusion"))) {}
```

### Step 1: QCommonStyle への切り替えテスト

`ArtifactCommonStyle` のコンストラクタを `QCommonStyle` ベースに変更し、全 surface の表示崩れを洗い出す。

```cpp
// 変更前
ArtifactCommonStyle::ArtifactCommonStyle(QStyle* baseStyle)
    : QProxyStyle(baseStyle ? baseStyle : QStyleFactory::create(QStringLiteral("Fusion"))) {}

// 変更後（テスト用）
ArtifactCommonStyle::ArtifactCommonStyle(QStyle* baseStyle)
    : QCommonStyle() {}
```

確認項目:
- [ ] Main window chrome（タイトルバー、メニューバー、ツールバー）が正しく表示される
- [ ] Dock フレーム、タブ、スプリッターが正しく表示される
- [ ] Timeline のスクラブバー、キーフレーム、トラック表示が正しい
- [ ] PropertyEditor の row 表示、入力欄、ボタンが正しい
- [ ] Inspector の背景、ボーダー、テキスト色が正しい
- [ ] Asset Browser のグリッド、ツリービューが正しい
- [ ] Render Queue の表示が正しい
- [ ] すべてのダイアログ、コンボボックス、スピンボックスが正しい

### Step 2: 表示崩れの修正

Fusion に依存していた描画を `drawControl`/`drawPrimitive`/`drawComplexControl` 内で補完する。

代表的な修正対象（Fusion→QCommonStyle で変わるもの）:

| 要素 | QProxyStyle(Fusion) | QCommonStyle | 対応 |
|------|---------------------|-------------|------|
| `PE_FrameFocusRect` | 点線枠 | なし | `drawPrimitive` で自前描画 |
| `CE_PushButton` | styled panel | クラシック押しボタン | `drawControl` で自前描画 |
| `PM_TabBarTabHSpace` | 20 | 0 | `pixelMetric` で上書き |
| `CT_TabBarTab` | 角丸タブ | 矩形タブ | `sizeFromContents` + `drawControl` |
| `CC_ComboBox` | styled dropdown | OS 標準 | `drawComplexControl` で自前描画 |
| `CC_ScrollBar` | styled | 細い classic | `drawComplexControl` で自前描画 |

修正方針:
1. 各要素を1つずつ `QCommonStyle` で表示確認
2. 崩れる要素は `drawControl`/`drawPrimitive`/`drawComplexControl` 内で現在の Fusion 相当の見た目を自前描画
3. theme token から色を取得し、`QPalette` 経由で供給

### Step 3: アプリ標準として確定

すべての surface で表示崩れがなくなったら、`QCommonStyle` を正式な基底とする。

```cpp
ArtifactCommonStyle::ArtifactCommonStyle(QStyle* baseStyle)
    : QCommonStyle() {}
```

**Done when**: アプリ全体で `setStyleSheet()` が 0 になり、基底が `QCommonStyle` になる。

---

## Phase 5: DockStyleManager の QSS 完全撤去

### 現行コード

`Artifact/src/Widgets/Dock/DockStyleManager.cppm:200`:
```cpp
impl_->dockManager_->setStyleSheet(QString());
```

### 実装手順

1. `DockGlowStyle` に QPalette ベースの色供給を追加する
2. QADS の `CDockManager::setStyleSheet(QString())` 呼び出しを削除
3. QADS のデフォルト stylesheet が残っている場合、`CDockManager` に対して `QPalette` で上書きする方法を検討

QADS の stylesheet は QADS 内部でハードコードされているため、外部からの完全な撤去は QADS 側のパッチが必要になる可能性がある。その場合は:
- 現行の `setStyleSheet(QString())` を維持
- コメントで `// QSS 例外: QADS 組み込み stylesheet を無効化するために必要。QCommonStyle 移行後も維持` と明記
- 例外リストに登録

**Done when**: DockStyleManager から `setStyleSheet()` が消えるか、消せない場合に例外として文書化される。

---

## Phase 6: Row-Level UX Polish & Legacy Cleanup

### 6.1 Property Row の操作統一

`Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm` を中心に:

| 項目 | 現状 | やること |
|------|------|---------|
| reset button | 実装済みの可能性あり | 全 row type で統一された位置・挙動を確認 |
| keyframe diamond | `ArtifactTimelineWidget` 側 | PropertyEditor row に keyframe 状態表示を追加 |
| expression indicator | 未確認 | 式が設定された row にインジケーター表示 |
| pick-whip | 未実装 | row 右端にドラッグ可能な pick-whip icon を追加 |
| copy-paste row value | 未実装 | コンテキストメニューに Copy Value / Paste Value |
| numeric drag | 実装済みの可能性あり | slider+spin+drag の一貫性確認 |
| read-only state | 実装済みの可能性あり | グレーアウト、編集不可表示の統一 |

実装優先度:
1. **keyframe state indicator** — row 左端にキーフレーム有無の菱形インジケーター（`CE_PushButton` 風の small diamond）
2. **expression indicator** — 式入力中の row に `=[...]` バッジ
3. **pick-whip** — row 右端に drag source icon。ドラッグで別プロパティへのリンク作成
4. **copy/paste** — コンテキストメニュー追加

### 6.2 Legacy Knob 撤去

`ArtifactWidgets/include/Knob/*` を確認し、使用されていない Knob クラスを削除または deprecated マークする。

```cpp
// deprecated 例
[[deprecated("Use PropertyEditor row slider instead")]]
class LegacyKnob : public QWidget { ... };
```

確認対象:
- [ ] `ArtifactWidgets/include/Knob/` 配下の全ファイル
- [ ] 各 Knob クラスの使用箇所を grep で確認
- [ ] 未使用なら削除、使用中なら PropertyEditor に移行計画を立てる

---

## Phase 7: Runtime 検証

### 7.1 Theme 切替テスト

`ApplicationSettingDialog` の theme selector で以下を切り替え:

- [ ] Dark → Light: 全 surface（Main window, Dock, Timeline, Property, Inspector, Asset Browser, Render Queue, 全ダイアログ）が正しく再描画される
- [ ] Light → Dark: 同上
- [ ] High-contrast: 同上
- [ ] 切替中にクラッシュしない
- [ ] 切替後、hover/selection/disabled の見え方が各 theme で一貫している

### 7.2 hover/selection/disabled の統一確認

以下の widget で、hover/selection/disabled の見た目が統一されていることを目視確認:

- [ ] `QPushButton` — 通常時 / hover / pressed / disabled
- [ ] `QComboBox` — 通常時 / hover / 展開中 / disabled
- [ ] `QSpinBox` / `QLineEdit` — 通常時 / focus / disabled
- [ ] `QTabBar` — 通常時 / hover / selected / disabled
- [ ] `QTreeView` / `QListView` — row hover / selection / disabled
- [ ] Dock tab — hover / active / inactive
- [ ] Timeline track — hover / selected
- [ ] Property row — hover / selected / keyframe / expression / disabled

### 7.3 DockStyleManager の QSS ゼロ確認

- [ ] `grep -r 'setStyleSheet' Artifact/src/ --include='*.cppm' --include='*.cpp'` で `DockStyleManager.cppm:200` 以外のヒットがない
- [ ] DockStyleManager の呼び出しが例外として文書化されている

---

## ファイル一覧（変更対象）

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P4 | `Artifact/src/Widgets/CommonStyle.cppm` | 基底を `QCommonStyle` に変更、drawControl/drawPrimitive/drawComplexControl 補完 |
| P5 | `Artifact/src/Widgets/Dock/DockStyleManager.cppm` | `setStyleSheet(QString())` の削除または例外文書化 |
| P6 | `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm` | keyframe indicator、expression badge、pick-whip、copy/paste |
| P6 | `ArtifactWidgets/include/Knob/*` | 未使用クラス削除または deprecated 化 |
| P7 | 全 surface | 目視検証、theme 切替テスト |

## 完了条件

- [ ] `QCommonStyle` が `ArtifactCommonStyle` の基底になっている
- [ ] `setStyleSheet()` が全 widget で 0 または例外として文書化されている
- [ ] Dark/Light/High-contrast 切替で全 surface が正しく再描画される
- [ ] hover/selection/disabled の見た目が全 widget で統一されている
- [ ] Property row に keyframe indicator と expression badge が表示される
- [ ] 未使用 Knob クラスが削除されている
