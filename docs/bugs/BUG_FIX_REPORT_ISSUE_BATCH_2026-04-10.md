# Bug Fix Report — Issue Batch 2026-04-10

対象イシュー 4 件の根本原因・仮説・修正内容をまとめます。

---

## Issue 1: コンポジットエディタのパフォーマンス問題

### 症状
ギズモ（移動・回転・スケール）をドラッグ中に CPU 負荷が高く、操作が重い。

### 仮説と調査
`ArtifactCompositionEditor::mouseMoveEvent()` でギズモドラッグ中に毎イベントごと
`controller_->renderOneFrame()` が同期呼び出しされていた。
Qt の mouse move イベントは 1 フレームに何十回も発火しうる。
つまり **1 描画フレームに複数回フルレンダーが走る** ことになり、無駄なコストだった。

### 修正
`pendingGizmoDragRender_` フラグ + `QTimer::singleShot(0, ...)` デバウンスを導入。
1 イベントループティックに複数のマウスイベントが入っても、レンダーは 1 回に集約される。

```cpp
if (!pendingGizmoDragRender_) {
    pendingGizmoDragRender_ = true;
    QTimer::singleShot(0, this, [this]() {
        pendingGizmoDragRender_ = false;
        if (controller_) controller_->renderOneFrame();
    });
}
```

**修正ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

---

## Issue 2: タイムラインへの画像ドロップが重い

### 症状
コンポジットエディタへファイルをドラッグ＆ドロップすると UI がフリーズする。

### 仮説と調査
`dropEvent()` 内で `svc->importAssetsFromPaths()` を **同期呼び出し** していた。
この関数は OIIO でサムネイル生成・ファイルコピー等を行うためメインスレッドをブロックする。
`ArtifactLayerPanelWidget`（タイムライン側）はすでに `importAssetsFromPathsAsync` を使って
正しく非同期化されていたが、コンポジットエディタ側が未対応だった。

### 修正
`importAssetsFromPathsAsync()` に差し替え、レイヤー追加処理をコールバック内に移動。
ドロップ操作は即座に返り、インポート完了後にレイヤーが追加される。

**修正ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

---

## Issue 3: レイヤーソロビューが表示されない

### 症状
`ArtifactSoftwareLayerTestWidget`（レイヤーソロビュー）を開いても何も表示されない。

### 仮説と調査
`refreshPreview()` の冒頭に可視性ガード `if (!owner_->isVisible() || owner_->isHidden() ...)` がある。
コンストラクタから `refreshPreview()` を呼んでいるが、ウィジェットはその時点でまだ非表示 →
ガードで早期リターンとなりレンダーがスキップされる。

その後 `showEvent()` のオーバーライドが存在しなかったため、**ウィジェットが可視になっても
一切 `refreshPreview()` がトリガーされない**。レイヤー変更などの外部イベントが来るまで
ずっとブランクのままという状態だった。

`ArtifactSoftwareCompositionTestWidget`（コンポジションビュー）も同様だが、
こちらはコンポジション選択直後に別トリガーが来るためユーザーが気づきにくかった。

### 修正
`ArtifactSoftwareLayerTestWidget` に `showEvent()` を追加し、表示時に `refreshPreview()` を呼ぶ。

```cpp
void ArtifactSoftwareLayerTestWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (impl_) impl_->refreshPreview();
}
```

**修正ファイル**:
- `Artifact/src/Widgets/Render/ArtifactSoftwareRenderInspectors.cppm`
- `Artifact/include/Widgets/Render/ArtifactSoftwareRenderInspectors.ixx`

---

## Issue 4: レンダリングキューの出力に背景色しか出ない

### 症状
レンダーキューでレンダリングを実行すると、背景色で塗られた画像しか出力されない。
レイヤーが一切反映されない。

### 仮説と調査
`ArtifactImageLayer::loadFromPath()` は画像ヘッダーを同期で読み込んだあと、
OIIO による本体ロードを `QtConcurrent::run` で非同期開始する（`prefetchFuture_`）。

`toQImage()` は `prefetchFuture_.isRunning()` の場合に **`QImage()` を返して早期リターン**
する設計になっている。これはメインスレッドの描画ループ（「次フレームで再試行」前提）には
正しい挙動だが、**レンダーキューは QtConcurrent::run のバックグラウンドスレッドで
1 回だけ実行**されるため、再試行が発生しない。

結果として全画像レイヤーが `null` 扱いでスキップされ、背景色だけが出力されていた。

さらに、バックグラウンドスレッドから `impl_->cache_` に書き込むと、
メインスレッドの `prefetchWatcher_.finished` シグナルとのデータ競合が発生する危険があった。

### 修正
`toQImage()` にスレッド判定を追加:

- **メインスレッド**: 従来どおり `isRunning()` なら `QImage()` を返す（UI をブロックしない）
- **バックグラウンドスレッド**: `prefetchFuture_.waitForFinished()` でブロックし、
  `result()` から画像を直接返す。`impl_` への書き込みは行わないため競合しない。

```cpp
const bool isMainThread = (QThread::currentThread() == qApp->thread());

if (!isMainThread) {
    if (impl_->prefetchFuture_.isRunning() || impl_->prefetchFuture_.isFinished()) {
        impl_->prefetchFuture_.waitForFinished();
        QImage img = impl_->prefetchFuture_.result();
        if (!img.isNull()) return img;
    }
    return loadImageViaOIIO(impl_->sourcePath_);  // futureがない場合の同期フォールバック
}
```

また `isFinished()` パスも背景スレッドから `impl_` を書かないよう整理し、
`prefetchDone_` / `cache_` の書き込みはメインスレッドのみが行う構造に統一。

**修正ファイル**: `Artifact/src/Layer/ArtifactImageLayer.cppm`

---

## 変更ファイル一覧

| ファイル | 変更内容 |
|---|---|
| `Artifact/src/Layer/ArtifactImageLayer.cppm` | `toQImage()` バックグラウンドスレッド対応、スレッドセーフ化 |
| `Artifact/src/Widgets/Render/ArtifactSoftwareRenderInspectors.cppm` | `showEvent()` 追加 |
| `Artifact/include/Widgets/Render/ArtifactSoftwareRenderInspectors.ixx` | `showEvent()` 宣言追加 |
| `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | ドロップ非同期化、ギズモドラッグレンダースロットル |
