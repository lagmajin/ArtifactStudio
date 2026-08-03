# TBBmalloc 全面採用メモ

**作成日:** 2026-07-30
**種別:** 設計メモ（実装未着手）

---

## 現状

- ArtifactCore = STATIC ライブラリ
- Artifact = WIN32 EXE
- TBB は vcpkg 経由で両方にリンク済み
- `tbb12.dll` / `tbbmalloc.dll` は delay-load で bin フォルダに配置済み
- アロケータは MSVC CRT デフォルト（`_GUARDOVERFLOW_CRT_ALLOCATORS=1` はデバッグ用フラグで置換ではない）
- `ArtifactAllocators.ixx` に `FrameAllocator` / `TaskAllocator` / `ConcurrentAllocator`（std::pmr ベース）定義済みだが未使用
- `Parallel::For` はシリアルスタブ（TBB の並列実行は未接続）

---

## 推奨方式：tbbmalloc_proxy によるグローバル置換

TBB の `tbbmalloc_proxy.dll` / `tbbmalloc_proxy.lib` をリンクするだけで、MSVC CRT の `malloc`/`free`/`realloc`/`_msize` を透過的にフック。`new`/`delete`/`std::make_shared`/`std::make_unique` すべてが自動で tbbmalloc 経由になる。コード変更ゼロ。

### 変更箇所

**① Artifact/CMakeLists.txt（L675-676 付近）**

```cmake
# Before:
find_package(TBB REQUIRED)
target_link_libraries(Artifact PRIVATE TBB::tbb)

# After:
find_package(TBB REQUIRED)
target_link_libraries(Artifact PRIVATE TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy)
```

**② Artifact/CMakeLists.txt delay-load DLL リスト（L765 付近）**

`tbbmalloc_proxy.dll` を RELEASE_DLLS / DEBUG_DLLS に追加。

**③ ArtifactWorker（L789 付近）**

同様に `TBB::tbbmalloc_proxy` をリンク。

### ロールバック

`TBB::tbbmalloc_proxy` の 1 行をコメントアウトすれば CRT デフォルトに戻る。

---

## 事前確認事項

- vcpkg の `tbb` パッケージが `tbbmalloc_proxy` ターゲットを提供しているか
- 提供されていない場合、自前ビルドまたは vcpkg overlay で対応が必要
- `tbbmalloc_proxy.dll` がランタイムに存在するか（vcpkg マニフェストモードでは自動配置されない可能性あり）

---

## 代替方式

### scalable_allocator（STL コンテナ単位）

```cpp
#include <tbb/scalable_allocator.h>
std::vector<Vertex, tbb::scalable_allocator<Vertex>> verts;
```

### scalable_malloc（ホットパス手動確保）

```cpp
void* buf = scalable_malloc(size);
scalable_free(buf);
```

---

## 効果見込み

| 項目 | 改善 |
|------|------|
| マルチスレッド確保の競合 | あり（tbbmalloc はスレッドローカルキャッシュ） |
| 断片化耐性 | あり（segregated free list） |
| 毎フレーム GpuContext 確保のコスト | 誤差（そもそも毎フレーム確保をやめるべき） |
| 体感 FPS | 微増〜誤差範囲（真のボトルネックはアロケータではない） |

---

## 注意

- アロケータ交換だけではパフォーマンスレポート（REPORT_PREVIEW_RENDER_PERF_FIXES_2026-07-30.md）の核心的問題は解決しない。FX-3, B1, B2 の修正が先。
- `tbbmalloc_proxy` は EXE にのみ有効。DLL 境界を超えた確保/解放が混在すると動作未定義なので、サードパーティ DLL（OpenCV, FFmpeg）との境界に注意。
- `_GUARDOVERFLOW_CRT_ALLOCATORS=1` と `tbbmalloc_proxy` は両立するか要確認。
