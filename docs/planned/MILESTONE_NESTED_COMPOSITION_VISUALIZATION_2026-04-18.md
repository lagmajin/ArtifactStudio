# マイルストーン: ネストコンポジション 視覚化実装

作成日: 2026-04-18
優先度: 🔴 最高
対象バージョン: M12

---

## 概要

既に機能として動作しているネストコンポジションを、ユーザーが視覚的に認識・操作できるようにするためのUI実装マイルストーンです。

現在コア側ではネストコンポジションは完全に動作するものの、タイムライン上で通常レイヤーと区別がつかず、内部のタイミングや状態を確認する手段が存在しない状態。

---

## 目標

✅ After Effects 同等のネストコンポジション体験を実現する
✅ コンポジションレイヤーの視覚的区別
✅ ワンクリックでのコンポジション内側への移動
✅ イン/アウトポイントとタイミングの可視化
✅ ソースコンポジションとの双方向連携

---

## 実装タスク

### Phase 1: レイヤーパネル表示

- [ ] コンポジションレイヤー専用のアイコンと背景色
- [ ] サムネイルプレビュー表示
- [ ] コンポジション名の右側にネスト深度インジケータ
- [ ] ダブルクリックでコンポジションを開く動作
- [ ] 右クリックメニューに「コンポジションを開く」項目追加

### Phase 2: タイムライントラック表示

- [ ] コンポジションレイヤー専用のトラック背景スタイル
- [ ] 内部コンポジションのデュレーション表示
- [ ] ループ/ストレッチ状態のインジケータ
- [ ] タイムリマップ有効時の視覚的マーカー
- [ ] 折りたたみ式で内部レイヤーをインライン表示

### Phase 3: ナビゲーションシステム

- [ ] コンポジションビュー上部にブレッドクラムバー
- [ ] ネスト階層の視覚的表示
- [ ] 上位コンポジションへのワンクリック移動
- [ ] Tabキーでのコンポジション間ジャンプ
- [ ] コンポジション履歴の戻る/進むボタン

### Phase 4: タイミング連携

- [ ] 親コンポジション上での再生時、子コンポジションのプレイヘッドも同期
- [ ] コンポジション境界線の可視化
- [ ] イン/アウトポイント調整時の子コンポジションプレビュー
- [ ] タイムストレッチ時の比率表示

### Phase 5: ユーティリティ機能

- [ ] Alt + ドラッグでコンポジションの複製作成
- [ ] コンポジションレイヤーからソースを開くショートカット
- [ ] プリコンポーズコマンドの実装
- [ ] 選択レイヤーを新規コンポジションにまとめる機能

---

## 技術的仕様

### 対象ファイル

```
Artifact/src/Widgets/ArtifactLayerPanelWidget.cppm
Artifact/src/Widgets/ArtifactTimelineWidget.cppm
Artifact/src/Widgets/ArtifactCompositionEditor.cppm
Artifact/src/Model/ArtifactCompositionLayer.cppm
Artifact/include/Model/ArtifactCompositionLayer.ixx
```

### 依存関係
- 既存の `ArtifactCompositionLayer` クラスは完全に動作済み
- 既存のタイムラインレンダリングパイプラインを拡張するのみ
- コア側の修正は不要、UI側のみの変更で完了する

---

## 完了条件

- [ ] タイムライン上でコンポジションレイヤーが一目で判別できる
- [ ] ダブルクリックで即座にコンポジションの中に入れる
- [ ] ブレッドクラムで現在のネスト位置が常に表示される
- [ ] どの階層から再生しても全てのコンポジションのプレイヘッドが同期する
- [ ] 既存プロジェクトファイルとの互換性が維持される

---

## 推定工数
- Phase 1: 1日
- Phase 2: 2日
- Phase 3: 1日
- Phase 4: 1日
- Phase 5: 1日
- 合計: 6日

---

## 優先順位
🔴 最優先: M12 リリースまでに絶対に入れる
- この機能がないとコンポジションのネスト機能自体が存在しないのと同じユーザー体験になる
- 全ての複雑なコンポジション制作の根幹となる機能

---

## 関連ドキュメント

- [`docs/planned/MILESTONE_DCC_FEATURE_GAPS_2026-03-28.md`](docs/planned/MILESTONE_DCC_FEATURE_GAPS_2026-03-28.md)
- [`plans/AFTER_EFFECTS_GAP_ANALYSIS.md`](plans/AFTER_EFFECTS_GAP_ANALYSIS.md)

---

## Static audit follow-up (2026-07-25)

現行ソースでは `PreComposeManager` が nesting map / parent-child 関係 / nesting level / composition hierarchy / parent lookup / parent-child time conversion / precompose・unprecompose command を持つ。`ArtifactCompositionLayer` とタイムライン／Composition Editor 側の nested composition 接続も存在するため、コア機能が存在するという文書の前提は確認できる。

ただし、今回の静的確認では、専用アイコン・背景色・サムネイル・深度表示、timeline 内部展開、常時表示の breadcrumb、履歴 UI、親子 playhead の完全同期、Alt+drag 複製など、視覚化マイルストーンの全 UI 完了は確認できない。PreCompose API の存在だけでユーザー向け視覚化の Definition of Done 達成とは判定しない。

### Audit status

- Phase 1: 部分実装 — nested composition / precompose の操作基盤はあるが、専用 layer panel 表示は未確認
- Phase 2: 未完了 — timeline 内部表示、duration / loop / stretch / remap indicator は未確認
- Phase 3: 部分実装 — hierarchy / time conversion API はあるが、breadcrumb / history UI は未確認
- Phase 4: 部分実装 — nested timing API はあるが、親子 playhead の完全同期は未確認
- Phase 5: 部分実装 — precompose / unprecompose はあるが、複製・新規コンポジション化 UI は未確認

## Update 2026-08-15

- `ArtifactLayerMenu`、`ArtifactLayerPanelWidget`、`PrecomposeDialog` から、プリコンポーズ／解除と Undo 付きの操作導線を確認した。`PreComposeManager` には階層、時刻変換、循環拒否、復元経路がある。
- Project health／render queue 側にも nested composition の循環・matte 等を診断する経路があるため、ネストのデータ／操作基盤は当初記述より進んでいる。
- ただし、専用アイコン・深度表示・タイムライン内展開・常時表示 breadcrumb／履歴 UI・親子 playhead の完全同期・Alt+drag 複製は現行検索で確認できない。視覚化マイルストーンの判定は引き続き部分実装。ビルド・runtime 操作確認は未実施。
