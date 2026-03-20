# QADS フローティングウィンドウ リサイズ不具合 — 調査結果と修正方針

> 2026-03-20 調査

## 症状

フローティング状態の dock ウィンドウをライブリサイズすると、拡大時に黒領域が増える、縮小時に残像が残る。

## 根本原因

**Windows の live resize は `DefWindowProc` の modal sizing loop 内で動作する。この間、Qt のイベントループは回らない。**

以下が現行コードの問題点:

### 問題1: `scheduleFloatingWidgetRefresh` の QTimer が modal loop 中に処理されない

`FloatingDockContainer.cpp:133-168` の `scheduleFloatingWidgetRefresh()` は `QTimer::singleShot(0, ...)` と `QTimer::singleShot(16, ...)` を使うが、これらは Qt イベントループに投げる。modal sizing loop 中はイベントループが回らないため、**resize 完了まで実行されない**。

### 問題2: 同期 refresh が Qt の layout propagation より先に走る

`nativeEvent()` の `WM_SIZE` / `WM_SIZING` / `WM_WINDOWPOSCHANGED` ハンドラで、毎回 `refreshFloatingWidgetSubtree(this)` を同期実行している (line 1023, 1029)。この関数は `layout()->activate()` → `update()` → `repaint()` を再帰的に呼ぶが、呼び出しタイミングは **Qt 側の `QWidget::resizeEvent()` / layout geometry 反映より前段**。つまり古いジオメトリで描画 → Qt が後から新しいジオメトリで上書き → その間に DWM が露出したフレームバッファに黒/残骸が残る。

### 問題3: `RDW_UPDATENOW` が古いジオメトリで即座に描画させる

`scheduleFloatingWidgetRefresh` 内で `::RedrawWindow(..., RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW)` を呼んでいる (line 150-151, 165-166)。`RDW_UPDATENOW` は即座に子ウィンドウに `WM_PAINT` を送るが、この段階で Qt の layout がまだ完了していなければ、**古いサイズの pixmap で描画**される。

### 問題4: `WM_ERASEBKGND` の独自ハンドラが Qt と競合する可能性

`nativeEvent()` の `WM_ERASEBKGND` ハンドラ (line 1049-1059) で独自に `FillRect(hdc, &rect, hbr)` を行い `*result = 1` を返している。DWM が live resize 中に `WM_ERASEBKGND` を非同期で送るタイミングと、Qt の backing store 更新が噛み合わず、独自ハンドラが塗った色の上に Qt が描画する前に露出領域が DWM に公開される可能性がある。

## 修正方針

### 方針: `WM_SIZE`/`WM_SIZING` の同期 refresh をやめ、`InvalidateRect` のみにする

1. `nativeEvent()` の `WM_SIZE` / `WM_SIZING` / `WM_WINDOWPOSCHANGED` ハンドラから `refreshFloatingWidgetSubtree(this)` を削除
2. 代わりに `::InvalidateRect(reinterpret_cast<HWND>(winId()), nullptr, TRUE)` のみ呼ぶ（再描画要求を保留するだけ）
3. `WM_EXITSIZEMOVE` ハンドラで初めて `refreshFloatingWidgetSubtree(this)` + `RedrawWindow(RDW_UPDATENOW)` を実行
4. `scheduleFloatingWidgetRefresh` の QTimer パスは残すが、modal loop 外でしか処理されないことを前提にする

### 補足: `WM_ERASEBKGND` ハンドラは削除検討

Qt に委譲 (`return false`) にするか、`WA_NoSystemBackground` を設定して背景消去を完全にスキップする方向も検討する。

## 対象ファイル

`third_party/Qt-Advanced-Docking-System/src/FloatingDockContainer.cpp`

## 変更箇所の具体的コード

### 変更1: `nativeEvent()` の `WM_SIZE` / `WM_SIZING` / `WM_WINDOWPOSCHANGED` (line 1022-1031)

現在:
```cpp
case WM_SIZE:
     refreshFloatingWidgetSubtree(this);
     scheduleFloatingWidgetRefresh(this);
     break;

case WM_SIZING:
case WM_WINDOWPOSCHANGED:
     refreshFloatingWidgetSubtree(this);
     scheduleFloatingWidgetRefresh(this);
     break;
```

修正後:
```cpp
case WM_SIZE:
case WM_SIZING:
case WM_WINDOWPOSCHANGED:
     ::InvalidateRect(reinterpret_cast<HWND>(winId()), nullptr, TRUE);
     scheduleFloatingWidgetRefresh(this);
     break;
```

### 変更2: `nativeEvent()` の `WM_EXITSIZEMOVE` (line 1033-1047)

現在:
```cpp
case WM_EXITSIZEMOVE:
     if (d->isState(DraggingFloatingWidget))
     {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        {
            d->handleEscapeKey();
        }
        else
        {
            d->titleMouseReleaseEvent();
        }
     }
     scheduleFloatingWidgetRefresh(this);
     break;
```

修正後:
```cpp
case WM_EXITSIZEMOVE:
     if (d->isState(DraggingFloatingWidget))
     {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        {
            d->handleEscapeKey();
        }
        else
        {
            d->titleMouseReleaseEvent();
        }
     }
     refreshFloatingWidgetSubtree(this);
     ::RedrawWindow(reinterpret_cast<HWND>(winId()), nullptr, nullptr,
         RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
     break;
```

### 変更3 (任意): `WM_PAINT` ハンドラ (line 1061-1066) を削除

```cpp
// 削除
case WM_PAINT:
     if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
     {
        refreshFloatingWidgetSubtree(this);
     }
     break;
```

`WM_PAINT` 内で `refreshFloatingWidgetSubtree` を呼ぶと、Qt の paint サイクルを巻き戻す可能性がある。modal loop 外で `scheduleFloatingWidgetRefresh` が処理されるため不要。

### 変更4 (任意): `WM_ERASEBKGND` ハンドラ (line 1049-1059) を削除

```cpp
// 削除
case WM_ERASEBKGND:
{
    HDC hdc = (HDC)msg->wParam;
    RECT rect;
    GetClientRect(msg->hwnd, &rect);
    HBRUSH hbr = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdc, &rect, hbr);
    DeleteObject(hbr);
    *result = 1;
    return true;
}
```

Qt に委譲する。もしくは `CFloatingDockContainer` コンストラクタで `setAttribute(Qt::WA_NoSystemBackground)` を設定して背景消去をスキップする。

## 検証手順

1. ビルド
2. アプリ起動 → パネルをフローティング
3. ウィンドウ端をドラッグしてライブリサイズ（10秒以上）
4. 確認項目:
   - 拡大時に黒領域が出ない
   - 縮小時に残像が残らない
   - リサイズ終了直後に内容が即座に表示される
5. 異なるパネル（Project、Timeline、Inspector）で繰り返す
