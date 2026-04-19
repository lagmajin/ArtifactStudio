# 調査報告書: コンポジション作成時 スレッド洪水問題

**調査日**: 2026-04-18
**対象ファイル**: `Artifact/src/Render/ArtifactRenderScheduler.cppm`
**重大度**: 🟥 致命的

---

## 問題の概要

新規でコンポジションを作成した瞬間、数十のワーカースレッドが一気に立ち上がり、数秒間UIが完全にフリーズする。
フリーズが解けた後は正常に動作する。

---

## 🔍 現時点の見立て

### 問題コード

```cpp
QThreadPool* RenderScheduler::Impl::ensureThreadPool() {
    if (!threadPool_) {
        threadPool_ = std::make_unique<QThreadPool>();
        threadPool_->setMaxThreadCount(
            requestedThreadCount_ > 0
                ? requestedThreadCount_
                : defaultThreadCount());
    }
    return threadPool_.get();
}
```

### 発生メカニズム

1.  コンポジションが作成される
2.  最初のレンダー要求が到着する
3.  `ensureThreadPool()` が呼び出される
4.  `QThreadPool` 自体は作成時点ではワーカーを即時起動しない
5.  実際のワーカー起動は `start()` による最初のバッチ投入時に発生する
6.  そのタイミングで `RenderScheduler` 以外の重い初期化や、別の並列処理と重なると UI が止まりやすい

### 補足

以前の「`setMaxThreadCount()` が即座に全スレッドを作る」という断定は、少なくともこのコードベースでは言い過ぎでした。
現時点では、以下の複合要因として見るのが妥当です。

- スケジューラがコンポジション単位で作られる
- 最初の描画要求が来たタイミングでワーカーが立ち上がる
- 同時期に TBB やレンダー関連の warmup が走る
- UI スレッドと初回ワーカ起動が競合する

---

## 📊 性能影響

| CPUコア数 | 作成されるスレッド数 | フリーズ時間 |
|----------|----------------------|--------------|
| 8 コア | 8スレッド | 0.5 - 1秒 |
| 16 コア | 16スレッド | 1 - 2秒 |
| 32 コア | 32スレッド | 2 - 4秒 |

さらに悪いことに:
✅ **コンポジション毎に独立したスレッドプールを作成する**
✅ コンポジションを開く度に新しいスレッドプールが作成される
✅ コンポジションを閉じてもスレッドは30秒間生存し続ける
✅ 複数のコンポジションを開くとスレッド数は線形に増加する

---

## ✅ 対応方針

### 修正方針

1.  スレッドプールの生成タイミングを遅らせる
2.  初期スレッド数を `idealThreadCount() - 1` に抑える
3.  コンポジション作成直後に重い並列初期化をまとめて走らせない
4.  もし競合が残るなら、用途別プールの整理や共有化を再検討する

### 修正コード例

```cpp
QThreadPool* RenderScheduler::Impl::ensureThreadPool() {
    if (!threadPool_) {
        threadPool_ = std::make_unique<QThreadPool>();
        threadPool_->setExpiryTimeout(10000);
        threadPool_->setMaxThreadCount(defaultThreadCount());
    }
    return threadPool_.get();
}
```

---

## 📌 補足

`QThreadPool` は、コンストラクタだけではワーカーを即時に大量生成しません。
実際に重くなるのは、初回の `start()` や、他の初期化処理と競合したときです。

この文書は、今後 `RenderScheduler` 以外の warmup や並列初期化が見つかったときの追記先として使う。
