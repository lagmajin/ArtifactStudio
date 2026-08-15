# マイルストーン: レイヤーコンポーネントシステム

**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

基盤は「接続するだけ」の段階を越えています。`LayerComponentHost`／descriptor／phase／scope、enabled phase 抽出、validation、runtime snapshot、保存・再読込が `ArtifactAbstractLayer` に接続されています。Inspector には Components 専用面があり、generator／transform／field／clone modifier の stack、追加／削除、専用 property widget の導線も確認できます。

ただし、legacy な `component.*` 実装フィールドと descriptor stack が併存し、全標準機能が descriptor component へ移行したわけではありません。component 間メッセージ／依存解決、全機能の runtime phase parity、preset、サードパーティ拡張、旧 API の完全互換検証は未完了・未検証です。通常 Property 表示と Components 専用面の責務分離は、現行設計ルールに沿って継続確認が必要です。

## Update 2026-08-15

- 現行コードでは `LayerComponentHost` の descriptor／phase／scope、validation、runtime snapshot、保存・再読込と、Inspector の Components 専用編集導線を確認できる。
- legacy `component.*` と descriptor stack の移行完了、component 間依存解決、全標準機能の runtime phase parity、preset／拡張 API／旧 API 互換は未完了または未検証。

- レイヤーJSON復元時に components／componentGraph が欠落している場合、既存インスタンスのcomponent有効状態・追加modifier・descriptorを持ち越さないよう stale state をクリアした。
- ビルド・component runtime parity検証は未実施。

### Evaluation Pipeline follow-up (Update 2026-08-15)

`LayerComponentSystem` の phase 列挙、同一 phase 内の order、required type／phase／enabled／cycle validation、phase-filtered query を確認した。`ArtifactAbstractLayer` では Cloner → Layout → Crowd → Dynamics → Fracture → Particle の中間 snapshot／event 経路も接続されているため、旧 evaluation pipeline 文書の foundation 記述は現行コードと一致する。

未完了なのは composition-world の AABB broadphase、fixed-step seek checkpoint、bake-to-keyframes／make-editable、全標準 component の runtime parity である。旧 `MILESTONE_LAYER_COMPONENT_EVALUATION_PIPELINE_2026-06-28.md` は本書に統合済みとして扱う。

作成日: 2026-04-18
優先度: 🟠 高
対象バージョン: M13

---

## 概要

Unity ライクなコンポーネントアーキテクチャをレイヤーシステムに導入します。
レイヤーに対して後付けで機能を追加できるようになり、コードの複雑さを劇的に削減します。

**現在の状況:** 基盤実装は既に完了しており、接続するだけの状態です。

---

## ✅ 既に存在しているもの

```
✅ コンポーネントインターフェース定義
✅ レイヤーへのコンポーネントアタッチ機構
✅ コンポーネントのシリアライズ/デシリアライズ
✅ 更新ループの実行機構
✅ コンポーネント間のメッセージ通信
```

現在はただ誰もこの機能を使っていないだけの状態です。

---

## 📋 実装タスク

### Phase 1: コアシステム有効化
- [ ] レイヤーベースクラスにコンポーネントリスト追加
- [ ] フレーム更新時のコンポーネント実行ループ
- [ ] プロジェクトファイルへのシリアライズ対応
- [ ] 既存プロジェクトとの後方互換性維持

### Phase 2: 標準コンポーネント実装
- [ ] トランスフォームコンポーネント
- [ ] フィジックスコンポーネント
- [ ] モーショントラッカーコンポーネント
- [ ] エクスプレッションコンポーネント
- [ ] ビヘイビアスクリプトコンポーネント

### Phase 3: UI 統合
- [ ] インスペクターへのコンポーネント一覧表示
- [ ] コンポーネント追加/削除UI
- [ ] コンポーネントプロパティ自動生成UI
- [ ] コンポーネントプリセット

### Phase 4: 移行
- [ ] 既存のレイヤー機能をコンポーネントにリファクタリング
- [ ] レイヤークラスの肥大化解消
- [ ] 従来のAPIの互換性レイヤー

---

## 💡 これで可能になること

✅ レイヤーに任意の機能を後付けで追加可能  
✅ サードパーティプラグインからのレイヤー機能拡張  
✅ 同じ機能を全てのレイヤータイプで共有  
✅ レイヤークラスの肥大化の完全な解消  
✅ データ駆動型のレイヤー動作定義

---

## 📊 実装難易度

| 項目 | 難易度 | 備考 |
|------|--------|------|
| コア有効化 | 🟢 超簡単 | 既にコードが存在する、数行追加するだけ |
| 標準コンポーネント | 🟢 簡単 | |
| UI統合 | 🟡 中 | |
| 移行作業 | 🟡 中 | 既存コードのリファクタリング |

**合計工数: 約 7日**

---

## 🔑 最大の利点

この機能を有効化する事により、今後全ての新規レイヤー機能をコンポーネントとして実装する事が可能になります。
レイヤークラスを修正する事なく、機能追加を行えるようになるため、今後数年間の開発速度が劇的に向上します。

これは現在のコードベースに存在する、最も大きな未活用の機能です。
