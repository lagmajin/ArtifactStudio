# Effect Sequence Groups Milestone

**最終更新:** 2026-09-03

**ステータス:** Not Started
**対象:** Effect Stack / layer effect serialization / GPU effect execution
**関連:** `docs/planned/MILESTONE_EFFECT_SYSTEM_IMPROVEMENT_2026-03-28.md`, `docs/planned/MILESTONE_EFFECT_UI_STANDARDIZATION_2026-06-07.md`

---

## 目的

Effect Stack 内に、順序付きの複数エフェクトを保持する `Sequence Group` を導入する。
グループはスタック上の 1 要素として扱い、内部エフェクトを上から順に適用する。制作時に関連処理をまとめ、stack の見通しと一括操作を改善する。

## 初期スコープ

- 入れ子なしの 1 階層グループ
- グループ作成、名称変更、削除、折りたたみ
- エフェクトをグループへ移動／グループ外へ戻す、グループ内順序変更
- グループ単位の enabled / bypass
- 既存の各エフェクトの enabled 状態を維持
- 保存・再読込・clipboard でグループ構造と順序を維持
- GPU 経路でグループ内部を宣言順に実行し、既存の単体エフェクトスタックと同じ結果にする
- すべての構造編集を既存 Undo 経路に接続する

## 非スコープ

- グループの再帰的な入れ子
- グループ単位の不透明度、ブレンドモード、マスク、調整レイヤー化
- 複数グループのプリセット資産化
- CPU/GPU の新規エフェクト実装やソフトレンダラー機能追加

## データ・実行契約

1. Stack item は `Effect` または `SequenceGroup` を明示的に識別する。
2. `SequenceGroup` は stable ID、表示名、enabled 状態、子 effect の順序を保持する。
3. enabled でないグループは子を実行せず、入力をそのまま次の stack item へ渡す。
4. enabled のグループは、子 effect を配列順に実行する。子が disabled の場合はその子だけを bypass する。
5. 旧形式の effect stack は、各 effect を root level item として読込み、保存互換性を壊さない。
6. UI 上の折りたたみ状態は制作UIの状態として扱い、レンダー結果には影響させない。

## 実装段階

### Phase 1 — Core stack model と保存

- group item のモデル、stable ID、順序操作を導入する。
- JSON / project / clipboard 読書きで旧形式を後方互換として扱う。
- add / remove / move / regroup を既存 Undo 基盤で一操作ずつ取り消せるようにする。

完了条件:

- 既存プロジェクトを壊さず読み込める。
- グループ作成・解除・順序変更を Undo / Redo できる。
- 保存後の再読込で構造、名前、有効状態、順序が一致する。

### Phase 2 — Effect Stack UI

- Effect Stack に hierarchy 表示と折りたたみを追加する。
- group enabled / bypass と child enabled の状態を明確に区別する。
- drag and drop または同等の既存操作で root / group 間の移動と並べ替えを行う。
- 選択・focus・削除操作が child と group を取り違えないようにする。

完了条件:

- group 内外の移動が一貫して見える。
- group bypass 時に child の個別 enabled 状態を変更しない。
- Inspector / Effect Stack の選択対象が一致する。

### Phase 3 — GPU render execution と検証

- render plan が group 境界を解釈し、内部 effect を順序通り実行する。
- bypass と空グループを input passthrough として扱う。
- GPU path の単体 stack と group 化した同一並びを比較する。

完了条件:

- 同じ effect 順序なら、group 化の有無で見た目が変わらない。
- disabled group、disabled child、空 group が表示不能や resource leak を起こさない。
- 保存・再読込・Undo / Redo・GPU preview を通した受入確認を記録する。

## 将来拡張

Phase 3 の受入後に、グループ単位の opacity / blend mode / mask、プリセット化、必要性が確認できた場合のみ入れ子を検討する。これらは初期データ形式と実行契約を複雑化するため、Sequence Group の基本操作・保存・GPU順序が安定する前には着手しない。
