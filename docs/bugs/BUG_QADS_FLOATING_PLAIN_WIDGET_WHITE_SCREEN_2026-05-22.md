# QADS floating 状態で純粋 QWidget 系ウィジェットが白画面になる調査レポート

> 2026-05-22 調査

## 現象

`PlaybackControlWidget` などの「純粋な Qt ウィジェット（GPU レンダラーを持たない QWidget 系）」を
QADS の floating dock として最初から開いた場合、白いまま描画されないことがある。

`CompositionEditor` の白画面バグ（`BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md`）とは
**根本原因が異なる**点に注意。

---

## 先行調査との分離

| 観点 | CompositionEditor バグ | 本バグ（PlaybackControl 等） |
|---|---|---|
| ウィジェット種別 | GPU renderer 付き（Diligent） | 純粋 QWidget / QPainter |
| 原因の核心 | SwapChain/renderer の未初期化 | Qt palette・autoFillBackground の floating lifecycle 競合 |
| 「dock に戻すと直る」 | ✅ renderer 初期化が再トリガーされる | ✅ `showEvent` / `visibilityChanged` 再発火でパレット再適用される |
| `WinIdChange` 追跡が必要か | ✅ 必要 | ⚠️ GPU surface 再生成は不要。ただし floating 再親化の保険として surface refresh trigger には使える |

---

## 起動フローと競合の詳細

### 1. 生成タイミング: `singleShot(0)` の中で floating 生成

`AppMain.cppm:1307-1314` で、`PlaybackControlWidget` は
`QTimer::singleShot(0, ...)` ラムダの中で生成・dock 登録されている。

```cpp
QTimer::singleShot(0, mw, [=, ...]() {
    auto* playbackControlWidget = new ArtifactPlaybackControlWidget(mw);
    mw->addDockedWidgetFloating(
        "Playback Control", "PlaybackControl",
        playbackControlWidget, QRect(120, 828, 720, 210));
    mw->setDockVisible("Playback Control", false);
    ...
});
```

この `singleShot(0)` が処理される時点では、
**メインウィンドウの `show()` はすでに完了**しており、イベントループは動いている。
つまり、ウィジェットは「起動後に動的に floating window として追加」される形になる。

### 2. `addDockedWidgetFloating` → `setDockVisible(false)` の連鎖

`addDockedWidgetFloating` は QADS が `CFloatingDockContainer` を生成し、
そこへ `CDockWidget` を配置する。その直後に `setDockVisible(false)` で非表示にする。

QADS 内部では:
1. `CFloatingDockContainer` が別 top-level window として生成される
2. この時点でウィジェットの `showEvent` / `show()` が一度発火しうる
3. しかし直後の `setDockVisible(false)` で `hide()` が呼ばれる

このため、**「最初の `show` が hide に打ち消される」状態**が起きる可能性がある。

### 3. `PlaybackControlWidget` コンストラクタのパレット適用タイミング

`ArtifactPlaybackControlWidget.cppm:1156-1183`:

```cpp
ArtifactPlaybackControlWidget::ArtifactPlaybackControlWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this))
{
    setAutoFillBackground(true);
    setAttribute(Qt::WA_StyledBackground, true);
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, QColor(theme.backgroundColor));
        ...
        applyPlaybackSurfacePalette(this, palette);  // (A)
    }
    impl_->setupUI();
    applyPlaybackSurfacePalette(this, palette());   // (B)
    ensurePolished();
    ...
    update();
}
```

コンストラクタ内で `(A)`, `(B)` と2回 `applyPlaybackSurfacePalette` を呼んでいる。
しかし、**この時点ではウィジェットはまだ native HWND を持っておらず、
floating container にも埋め込まれていない**。

`setAutoFillBackground(true)` は native window が確定して初めて効果を発揮する。
コンストラクタで設定したパレットが、後から QADS が floating window として
ウィジェットを reparent した際に**継承されない or 上書きされる可能性**がある。

### 4. QADS floating lifecycle での palette propagation の失敗

QADS が `CFloatingDockContainer` を生成する際、内部で `QWidget::show()` を呼ぶ。
この時 Qt は `QStyle::polish()` を top-level から子ウィジェットへ再適用する。

`ArtifactCommonStyle` または Fusion スタイルが floating window に対して
`Window` ロールの色を上書きする可能性がある。

具体的には:
1. floating container の `show()` → スタイルが `QPalette::Window` を白 or デフォルト値にリセット
2. `PlaybackControlWidget` の `autoFillBackground=true` が有効なので、**白で塗りつぶされる**
3. `paintEvent` で `fillRect(rect(), QColor(theme.backgroundColor))` を呼んでいるが、
   `update()` が来なければ実行されない

### 5. `refreshDockWidgetSurface` / `scheduleFloatingRefresh` の限界

`ArtifactMainWindow.cppm:287-327` の floating refresh 系ヘルパーは
`applyLazyDockSurfacePalette` を呼ぶが、これは**lazy dock（初回表示時に生成するウィジェット）向け**の設計。

`PlaybackControlWidget` は lazy ではなく `singleShot(0)` で即時生成されるため、
このパスを経由しない可能性がある。

また `scheduleFloatingRefresh` → `refreshFloatingWidgetTree` は
`ArtifactProjectView::refreshVisibleContent()` と `update()` しか呼ばず、
`applyLazyDockSurfacePalette` を再実行しない（L251-272参照）。

---

## 主要仮説

### 仮説 A: floating 化時の `QStyle::polish()` がパレットを白にリセットする

- QADS floating container の `show()` → `QStyle::polish(widget)` の連鎖
- `ArtifactCommonStyle` が `QPalette::Window` を正しく設定しない場合、白になる
- `autoFillBackground=true` が有効なので、そのまま白矩形で塗られる

**確認方法:** `showEvent` 内か floating container 表示直後に `palette().color(QPalette::Window)` をログ出力

### 仮説 B: コンストラクタでのパレット設定が reparent で無効化される

- コンストラクタ（HWND 取得前）でのパレット設定が floating window 生成時に失われる
- `setDockVisible(false)` → `setDockVisible(true)` のサイクルで `showEvent` は来るが、
  パレット再適用がされない

**確認方法:** `showEvent` をオーバーライドしてパレット再適用し、再現するか確認

### 仮説 C: `setDockVisible(false)` → `true` のサイクルで `update()` が来ない

- 最初に `false` で作られた floating dock を `true` にすると QADS は
  `CDockWidget::toggleView(true)` を呼ぶ
- この経路で `paintEvent` のトリガーとなる `update()` が来ない場合、白いまま

**確認方法:** `toggleView(true)` の直後に `widget->update()` + `repaint()` を明示的に挿入して確認

---

## 既存 `CompositionEditor` バグとの共通点と差異

### 共通する問題の本質

**「QADS floating lifecycle（生成・show/hide・reparent の順序）が
既存の Qt ウィジェット初期化 contract と噛み合っていない」**

という点は同じ。

### 差異

CompositionEditor の白画面は:
- `renderer 未初期化` または `SwapChain が古い native parent を指している`

PlaybackControl などの白画面は:
- `QPalette::Window が白にリセットされている` または
- `update()` が届かず `paintEvent` が発火しない

解決アプローチが異なるため、**同じヘルパーで一括修正しようとすると責務が混じる**。

---

## 対応策

### 短期: `showEvent` でパレット再適用 + `update()` を保証する

`ArtifactPlaybackControlWidget` に `showEvent` オーバーライドを追加:

```cpp
void ArtifactPlaybackControlWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // floating 化後のパレットリセット対策
    applyPlaybackSurfacePalette(this, palette());
    update();
}
```

これにより、`setDockVisible(true)` → `showEvent` 経由でパレットが復元される。

### 2026-05-22 Follow-up

`showEvent` だけでは QADS floating container 側の後続 `polish` / `reparent`
で再び白 palette に戻るケースが残ったため、`ArtifactPlaybackControlWidget`
側で lifecycle repair を強化した。

変更点:

1. `showEvent` だけでなく `event()` で `ParentChange` / `Polish` /
   `PolishRequest` / `Show` / `WinIdChange` を拾う
2. theme から dark palette を作り直して `applyPlaybackSurfacePalette()` を再適用する
3. `showEvent` 後に `QTimer::singleShot(0, ...)` で QADS の後続 polish 後にも
   再適用 + repaint する

狙いは、floating dock の生成直後・再表示直後・native window 付け替え直後の
どの順序でも最終的に Playback widget 側が palette と repaint を取り戻すこと。

### 中期: `applyLazyDockSurfacePalette` を floating refresh パスで確実に呼ぶ

`scheduleFloatingRefresh` → `refreshFloatingWidgetTree` が
`applyLazyDockSurfacePalette` を再実行するように修正:

```cpp
void refreshFloatingWidgetTree(QWidget *widget) {
    if (!widget) return;
    applyLazyDockSurfacePalette(widget);   // 追加
    for (auto *projectView : widget->findChildren<ArtifactProjectView *>()) {
        if (projectView) projectView->refreshVisibleContent();
    }
    widget->update();
}
```

ただし **パフォーマンス影響に注意**。
live resize 中に毎回 `findChildren` を走らせると重くなる可能性があるため、
`WM_EXITSIZEMOVE` 後（`BUG_QADS_FLOATING_RESIZE_FIX.md` の方針に従う）に限定すべき。

### 長期: `addDockedWidgetFloating` + `setDockVisible(false)` パターンを見直す

現在の使い方:

```cpp
mw->addDockedWidgetFloating(...);
mw->setDockVisible("Playback Control", false);
```

これは QADS に「floating で作って即 hide」を要求する。
QADS 内部の show/hide サイクルがパレット・スタイルに悪影響を与えている可能性がある。

代替案:
1. 生成時は非表示のまま dock に登録する（`addDockedWidget` + `setDockVisible(false)`）
2. ユーザーが明示的に floating 化する（QADS の float ボタン or `toggleView(true)` + floating 指定）

または、`setDockVisible(true)` を呼ぶ直前にパレット再適用を行う専用の
`ensureWidgetReadyForFloating(dock)` を `ArtifactMainWindow` に設ける。

---

## 観測優先ログポイント

白画面の際に次をログ出力すると原因が絞れる:

1. `showEvent` が来ているか（ウィジェット側で `qInfo() << "showEvent" << isVisible()`）
2. `palette().color(QPalette::Window)` が dark theme 値か白かを確認
3. `autoFillBackground()` が `true` のままか確認
4. `update()` が `showEvent` 後に呼ばれているか

---

## 影響範囲

同じ `addDockedWidgetFloating` + `setDockVisible(false)` パターンを使う
**他の純粋 QWidget 系 floating dock も同様に発症する可能性がある**:

- `Debug Console`
- `Frame Debug`
- `App Debugger`
- `Debug Render Harness`

これらは `singleShot(0)` ラムダ内で同じパターンで生成されている（`AppMain.cppm:1316-1357`）。

---

## 関連ドキュメント

- [`BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md`](./BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md) — GPU renderer 付き版の同種バグ
- [`BUG_QADS_FLOATING_RESIZE_FIX.md`](./BUG_QADS_FLOATING_RESIZE_FIX.md) — floating リサイズ時の黒領域/残像問題
- [`MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md`](../planned/MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md) — 関連マイルストーン

## 関連ファイル

- `Artifact/src/AppMain.cppm` — `PlaybackControlWidget` の dock 生成 (L1307-1314)
- `Artifact/src/Widgets/Control/ArtifactPlaybackControlWidget.cppm` — 白画面対象ウィジェット
- `Artifact/src/Widgets/ArtifactMainWindow.cppm` — `refreshFloatingWidgetTree`, `scheduleFloatingRefresh`, `applyLazyDockSurfacePalette`
