# MILESTONE: Scrub Accuracy / Expression Recursion / Cache Reuse - 2026-06-07

作成日: 2026-06-07  
対象: scrub 精度 / expression 再帰 / render cache 再利用  
優先度: 🟠 高

---

## 目的

このマイルストーンは、次の 3 つの制作ギャップをまとめる。

1. フレーム単位の scrub で特定フレームを飛ばさない
2. expression に安全な再帰とループを追加する
3. 書き出し時に同じフレームを何度もレンダリングしない

どれも「見えない / 遅い / 繰り返し無駄になる」問題を減らすのが主題。

---

## 対象ギャップ

### 1. Scrub Accuracy: 特定フレームが表示されない

不満:
- タイムラインをドラッグして scrub していると、特定のフレームだけ表示が飛ぶ
- render するとそのフレームは正しく出るのに、preview では見えない
- 10 年続くような再現しにくい欠陥になっている

改善:
- scrub 時の frame dispatch を絶対に飛ばさない
- 最後のフレームを確実に表示する
- heavy な場合でも、表示欠落より正確さを優先する

完了条件:
- どの frame でも preview 上で到達可能
- render と scrub で truth がずれない
- frame skip の原因を diagnostics から追える

---

### 2. Expression Recursion: 安全な再帰ができない

不満:
- `valueAtTime(time - 0.04)` のような過去参照はできるが、真の再帰が書きにくい
- フィボナッチや fractal のような recursive logic がすぐ stack overflow する
- loop も制限が厳しい

改善:
- 最大深度付きの安全な再帰を許可する
- メモ化 / dynamic programming を使えるようにする
- loop も同じ安全境界で扱う

完了条件:
- 再帰の深さが制御できる
- 同じ入力に対する repeated evaluation を抑えられる
- 暴走時の failure reason が読める

---

### 3. Cache Reuse: 同じフレームを何度もレンダリングする

不満:
- 連番 PNG や別フォーマット書き出しで、同じ frame を毎回ゼロから render する
- 一度レンダリングした frame を再利用できない
- export が不必要に重い

改善:
- frame cache を共有する
- 一度 render した frame をディスクに保存する
- 2 回目以降は cache を読むだけで変換できるようにする

完了条件:
- 同じ source / composition の再書き出しが速くなる
- cache hit が export で再利用される
- cache miss の理由が分かる

---

## 実装の読み替え

### Scrub Accuracy

- playback / scrub / diagnostics が同じ frame truth を読む
- 1 frame ずつの確実な dispatch を保証する
- 「表示されない frame」がある場合は cache ではなく sample / seek / invalidation を疑う

### Expression Recursion

- recursive expression を無制限にするのではなく、安全境界付きで許可する
- memoized recursion を優先し、同じサブ問題を何度も解かない
- loop と recursion を別扱いにせず、同じ runtime policy で制御する

### Cache Reuse

- frame cache を render の補助ではなく再利用の中心にする
- disk cache と frame cache の責務を分ける
- 同一 frame の重複 render を減らす

---

## 詳細実装スライス

### A. Scrub Accuracy

#### 入口
- `ArtifactTimelineWidget`
- `ArtifactTimelineTrackPainterView`
- `ArtifactTimelineScrubBar`
- `ArtifactPlaybackService`
- `ArtifactCompositionRenderController`

#### 触るもの
- frame dispatch
- seek path
- scrub input routing
- frame cache hit / miss
- playback diagnostics

#### データ契約
- requested frame
- committed frame
- displayed frame
- seek result
- cache state

#### 実装順
1. scrub から target frame を落とさない
2. seek / render の frame truth を統一する
3. last frame を必ず表示する fallback を作る
4. diagnostics に skip reason を残す

#### 失敗時の扱い
- frame が重くても skip せず遅延表示する
- 追従不能なら最後に確定した frame を保持する
- seek failure を silent にしない

#### Phase 1: frame dispatch hardening
- [ ] scrub 時の target frame を 1 frame 単位で保持する
- [ ] frame skip が起きたら reason を残す
- [ ] last frame を優先的に表示する

#### Phase 2: truth unification
- [ ] playback / scrub / diagnostics の current frame を一致させる
- [ ] seek と render の経路差を縮める
- [ ] 1 分 23 秒 15 フレーム目のような個別 frame の到達性を確認する

#### Phase 3: fallback safety
- [ ] heavy な場合でも最後の frame を表示する
- [ ] cache miss 時の fallback を明示する
- [ ] 予期しない skip を diagnostics へ出す

---

### B. Expression Recursion

#### 入口
- `ArtifactExpressionCopilotWidget`
- expression parser / evaluator
- property editor expression UI
- text animator / keyframe evaluation

#### 触るもの
- expression runtime
- call stack guard
- memoization cache
- loop policy
- diagnostics / error reporting

#### データ契約
- max recursion depth
- evaluation budget
- memoization key
- recursion stack state
- loop iteration budget

#### 実装順
1. 深度制限を追加する
2. memoization を入れる
3. loop を統一 policy にする
4. failure reason を可視化する

#### 失敗時の扱い
- depth 超過時は明示的に止める
- 無限再帰は stack overflow ではなく policy error にする
- memoization 不能な式は fallback evaluation に切り替える

#### Phase 1: safe recursion gate
- [ ] recursion depth limit を追加する
- [ ] depth 超過を policy error として返す
- [ ] stack overflow を避ける guard を入れる

#### Phase 2: memoization
- [ ] 同一入力の repeated evaluation をキャッシュする
- [ ] Fibonacci のような再帰を memoization で速くする
- [ ] cache key を expression + state で切る

#### Phase 3: loop policy
- [ ] loop / recursion を同じ budget 管理にする
- [ ] `loopIn` / `loopOut` の制限を明示する
- [ ] DP 的な再利用を許可する

#### Phase 4: diagnostics
- [ ] recursion depth / iteration count を表示する
- [ ] policy error の理由を読めるようにする
- [ ] runaway expression を止めた理由を残す

---

### C. Cache Reuse

#### 入口
- render queue
- preview cache
- export / frame cache
- disk cache / cache manager

#### 触るもの
- frame cache
- disk cache
- render queue job
- output format conversion
- cache key / invalidation

#### データ契約
- composition id
- source revision
- render settings hash
- frame index
- output format

#### 実装順
1. frame cache key を統一する
2. export 間で cache を共有する
3. disk へ持ち出せるようにする
4. stale cache を無効化する

#### 失敗時の扱い
- source が変わったら cache を無効化する
- format / codec mismatch は再変換に回す
- cache を読めない場合は render fallback に戻す

#### Phase 1: shared frame cache
- [ ] 同じ composition の frame cache を共有する
- [ ] 連番 PNG と動画 export の間で cache を使えるようにする
- [ ] cache key を render settings で安定化する

#### Phase 2: disk reuse
- [ ] 既レンダーフレームを disk に保存する
- [ ] export 2 回目以降は読み込み優先にする
- [ ] cache hit / miss を診断表示する

#### Phase 3: invalidation policy
- [ ] source revision 変更時に stale cache を無効化する
- [ ] 別 format 出力時の再変換経路を整理する
- [ ] cache reuse が壊れた時の理由を出す

#### 具体的 UI 案
- cache hit を render queue に badge 表示する
- `Reuse available` / `Stale` / `Miss` を明示する
- export dialog で cache 再利用の有無を短く出す

---

## 推奨実行順

1. Scrub Accuracy
2. Cache Reuse
3. Expression Recursion

理由:
- scrub の frame 欠落は制作中の信用を直接壊す
- cache reuse は export 速度に効きやすく、効果が見えやすい
- expression recursion は強力だが、深度制御や diagnostics を先に整えたい

---

## 関連

- [`docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md)
- [`docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md)
- [`docs/planned/MILESTONE_EXPRESSION_SYSTEM_2026-03-29.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_EXPRESSION_SYSTEM_2026-03-29.md)
- [`docs/planned/MILESTONE_EXPRESSION_LOOPOUT_RUNTIME_2026-06-02.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_EXPRESSION_LOOPOUT_RUNTIME_2026-06-02.md)
- [`docs/planned/MILESTONE_PREVIEW_PLAYBACK_PERFORMANCE_LOW_LEVEL_AI_2026-05-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_PREVIEW_PLAYBACK_PERFORMANCE_LOW_LEVEL_AI_2026-05-23.md)

---

## 備考

- これは実装タスクの詳細設計ではなく、scrub / expression / cache の 3 点を実行可能な単位にまとめた計画文書。
- ビルドやテストは実施していない。
