# PImpl と専用メモリアロケータの設計プラン

**最終更新:** 2026-08-23  
**ステータス:** Not Started

## Phase 0 棚卸し結果（2026-08-23）

### 確認できた既存基盤

- `ArtifactCore/include/Memory/ArtifactAllocators.ixx` に `CountingMemoryResource`、`FrameAllocator`、`TaskAllocator`、`ConcurrentAllocator` が既に存在する。
- 既存基盤は `std::pmr::memory_resource` を中心にしており、診断用の確保量・未解放量・ピーク量・回数を取得できる。
- `TaskAllocator` は thread-confined、`ConcurrentAllocator` は共有利用向けという寿命・スレッド前提が既にコメント化されている。
- 現時点で、PImpl の `Impl` オブジェクトをこの allocator 基盤から直接確保する共通 adapter は確認できない。

### 確認できた PImpl の状態

- `Artifact`／`ArtifactCore` に `Impl*` と `new Impl`／`delete impl_` の組み合わせが多数存在する。
- `ArtifactCore/src/Utils/UniString.cppm`、`Tag.cppm`、`Path.cppm`、`Id.cppm`、`NameGenerator.cppm` など、値型に近い PImpl はコピー・代入時にも個別確保を行う。
- `Artifact/src/Widgets/` 以下には Qt widget の PImpl が多数あり、QObject の親子所有、UI thread、シグナル接続を伴うため、最初の adapter 導入対象には適さない。
- 既存の品質監査では、生 `Impl*` のコピー／ムーブ禁止不足と、デストラクタ欠落によるリーク候補が別問題として報告されている。allocator 導入と所有権修正を一括で行わない。

### Phase 1 の第一候補

最初の実装候補は、UI／GPU／動画デコードから離れた小型の `ArtifactCore` ユーティリティ型とする。候補順位は次の通り。

1. `Path` — コピー意味論と寿命が比較的単純で、外部 resource の所有が少ないかを先に確認する。
2. `NameGenerator` — 長寿命サービスでなければ pool 適用効果を測りやすい。
3. `Tag`／`MultipleTag` — コピー・代入の既存契約を先に確認できる場合のみ対象とする。

`UniString` と `Id` は生成頻度が高い可能性がある一方、値型として広く使われるため、Phase 1 の最初の対象からは外す。コピー・ムーブ・例外・ABI の回帰面が大きいため、adapter の正しさを確認した後に別設計として扱う。

### Phase 0 での判断

- グローバルな `tbbmalloc_proxy` 置換は、PImpl の所有ドメインと DLL 境界を明確化する目的に対して広すぎるため、この計画の第一段階では採用しない。
- 既存 PMR allocator を再利用するが、`Impl` の解放を `memory_resource::deallocate` に委ねる専用 `PimplAllocation` adapter を追加する方向とする。
- `FrameAllocator`／`TaskAllocator`／`ConcurrentAllocator` を PImpl のデフォルト所有者として直接使わない。各 allocator の寿命・thread-confined 条件が PImpl の通常寿命と一致することを個別に証明できた場合だけ利用する。
- UI widget、レンダラー、FFmpeg／外部 DLL 境界、ReactiveEvents は Phase 1 の対象外とする。

### Phase 0 の未確認事項

- `Path` の `Impl` が保持する具体的な型と、PMR allocator を渡せる構造か。
- `ArtifactCore` の module／static library 境界で `std::pmr::memory_resource` の実体を安全に共有できるか。
- 既存の allocation 診断を PImpl 型名・allocation tag と結び付ける場所。
- `ArtifactCore` の公開 API で allocator の差し替えを許可する必要があるか。

これらをコード変更前に確認し、確認できない場合は `Path` ではなく独立した小型試験型で adapter の動作だけを検証する。

## 1. 目的

PImpl による実装隠蔽、C++20 module／DLL 境界の安定性、メモリ確保経路の制御を両立する。
汎用ヒープへの直接依存を減らし、必要な箇所ではプール・アリーナ・診断用アロケータへ差し替えられる構造を用意する。

本計画は「すべての `new` を禁止する」ものではない。PImpl のオブジェクト単位確保を完全に消す場合は in-place PImpl、確保コストと断片化を抑える場合は固定サイズプールを候補とする。

## 2. 適用範囲

### 対象

- 新規または変更対象となる PImpl クラス
- Artifact／ArtifactCore のアプリケーション所有オブジェクト
- DLL・module 境界をまたぐ可能性のある実装オブジェクト
- メモリ使用量、確保回数、解放元を追跡したい長寿命サービス

### 対象外

- Qt の所有権規約を一括変更すること
- `libs/DiligentEngine`、`ArtifactWidgets` などサブモジュールの変更
- Renderer のホットパスへの新規アロケータ導入
- ReactiveEvents 系統の変更
- 既存コード全体の機械的な PImpl 化

## 3. 基本方針

1. 公開面には `Impl*` と最小限のライフサイクル宣言だけを置く。
2. `Impl` の生成と破棄は同じ所有側・同じアロケータで完結させる。
3. `std::shared_ptr`／`std::unique_ptr` を新規の PImpl 所有表現には採用しない。
4. 実装側の `new`／`delete` を直接呼ばず、必要に応じて専用アロケータへ隔離する。
5. まず通常の専用アロケータで正しさを確立し、その後にプール化または in-place 化を検討する。
6. アロケータ最適化よりも ABI、安全性、例外時の解放、スレッド境界を優先する。

## 4. 推奨アーキテクチャ

```text
Public PImpl class
    └─ Impl*
        └─ ImplAllocator / AllocationDomain
            ├─ Default heap adapter
            ├─ Fixed-size pool adapter
            ├─ Arena adapter（寿命が同じ集合のみ）
            └─ Tracking/debug adapter
```

### 4.1 PImpl の所有契約

```cpp
class Example {
public:
    Example();
    ~Example();

    Example(const Example&) = delete;
    Example& operator=(const Example&) = delete;
    Example(Example&&) = delete;
    Example& operator=(Example&&) = delete;

private:
    class Impl;
    Impl* impl_ = nullptr;
};
```

実装側では、確保、placement construction、破棄、解放を同じ `.cppm`／`.cpp` に閉じ込める。

```cpp
Example::Example()
{
    void* memory = ExampleAllocator::allocate(sizeof(Impl), alignof(Impl));
    try {
        impl_ = std::construct_at(static_cast<Impl*>(memory));
    } catch (...) {
        ExampleAllocator::deallocate(memory, sizeof(Impl), alignof(Impl));
        throw;
    }
}

Example::~Example()
{
    if (impl_ == nullptr)
        return;

    std::destroy_at(impl_);
    ExampleAllocator::deallocate(impl_, sizeof(Impl), alignof(Impl));
    impl_ = nullptr;
}
```

上記は設計例であり、実装時にはプロジェクトの既存 allocator／診断 API を優先して再利用する。

### 4.2 アロケータ API

最初の API は状態を持たない最小インターフェースにする。

```cpp
class ExampleAllocator {
public:
    static void* allocate(std::size_t size, std::size_t alignment);
    static void deallocate(void* pointer,
                           std::size_t size,
                           std::size_t alignment) noexcept;
};
```

必要になった場合だけ、次の拡張を追加する。

- `AllocationTag` によるカテゴリ別統計
- `try_allocate` による例外を投げない経路
- thread-local cache
- pool の acquire／release
- アロケータドメインの明示的な寿命管理

公開 module interface へ実装詳細や allocator の具象型を漏らさない。ポインタ・参照で保持するだけの型は前方宣言を優先し、不要な import を追加しない。

## 5. 実装方式の選択基準

| 方式 | 適用条件 | 利点 | 主なリスク |
|---|---|---|---|
| 専用 heap adapter | まず導入する標準方式 | ABI を保ちやすい、実装が単純 | 個別確保は残る |
| 固定サイズ pool | 同じ `Impl` を大量生成 | 高速、断片化を抑制 | サイズ変更、所有ドメイン、スレッド返却 |
| arena | 同じタイミングで一括破棄 | 解放コストが低い | 個別破棄、長寿命混在に不向き |
| in-place PImpl | サイズ上限を固定できる小型型 | ヒープ確保なし | ABI と容量上限が結び付く |
| PMR adapter | STL 内部の一時領域も制御したい | 標準コンテナと接続しやすい | allocator の伝播規則が複雑 |

初期採用は専用 heap adapter とし、計測で確保回数または断片化が問題になった型だけ pool／arena を選択する。in-place PImpl は ABI 制約を受け入れられる小型の値型に限定する。

## 6. 段階導入計画

### Phase 0: 棚卸し

- PImpl 対象候補を列挙する。
- 各 `Impl` の生成頻度、寿命、破棄スレッド、サイズ変動を記録する。
- 既存の allocator、メモリ診断、所有権規約を確認する。
- module interface と実装ファイルの依存を分離する。

完了条件: 対象候補ごとに「標準 adapter／pool／arena／in-place」の選択理由がある。

### Phase 1: 最小 adapter

- 1 クラスだけを対象に専用 adapter を導入する。
- `new`／`delete` を placement construction と adapter に置き換える。
- constructor 例外、destructor、null 状態、二重解放を確認する。
- コピー／ムーブ方針を明示する。

完了条件: 挙動を変えずに allocation／deallocation の経路を観測できる。

### Phase 2: 診断と契約

- allocation tag、サイズ、アラインメント、所有スレッドを記録できるようにする。
- 解放元が異なる場合の検出を追加する。
- debug build では未解放、二重解放、サイズ不一致を報告する。
- production build ではログや同期コストを抑える。

完了条件: 失敗時に「どの型を、どのドメインが、どのアロケータで確保したか」が特定できる。

### Phase 3: pool／arena の限定導入

- 生成頻度が高くサイズ固定の型だけ pool 化する。
- arena は所有オブジェクトと同じ寿命で確実に一括破棄できる場合だけ使う。
- worker thread で確保し UI thread で破棄するような型には、返却キューまたは thread-safe domain を設計してから適用する。

完了条件: peak memory、allocation count、fragmentation、終了処理が標準 adapter より悪化しない。

### Phase 4: in-place PImpl の評価

- 小型かつサイズ上限が安定した候補のみ評価する。
- `sizeof`／`alignof` の静的検証を追加する。
- 容量超過時の扱いを明確にする。
- 公開型のサイズ増加が ABI に与える影響を確認する。

完了条件: ヒープ回避の効果が ABI コストを上回ると判断できる。

## 7. 例外・スレッド・DLL 境界の契約

- allocation 成功後、construction 失敗時は必ず同じ adapter でメモリを返す。
- destructor は noexcept 相当で扱い、破棄中に例外を外へ出さない。
- allocator の所有ドメインより長く `Impl*` を保持しない。
- 異なる DLL／CRT／module の allocator で確保と解放を混在させない。
- cross-thread destruction を許す場合、deallocation domain を明示する。
- pool／arena の破棄前に、所属する全 PImpl が破棄済みであることを保証する。
- `Impl*` を外部へ返す API は原則として公開しない。必要なら lifetime token など別契約を設ける。

## 8. 検証計画

ビルド・テスト実行は実装着手時に別途許可を得て行う。計画段階では次の検証項目を定義する。

### 静的確認

- `Impl` の確保と解放が同一実装境界にある。
- module purview 後への `#include` 追加がない。
- 不要な import、自己 import、循環依存を増やしていない。
- `shared_ptr`／`unique_ptr` を新規 PImpl 所有に追加していない。
- 既存のサブモジュールを変更していない。

### 動的確認

- 通常生成・破棄
- constructor 途中の例外
- 大量生成・大量破棄
- move／copy 禁止契約の確認
- 異なる thread での破棄
- DLL／module 境界をまたぐ生成と破棄
- pool 枯渇、arena 寿命超過、二重解放

### 性能確認

- allocation 回数
- allocation／deallocation の時間
- peak resident memory
- fragmentation
- frame-time への影響
- pool 化前後の差分

## 9. 受け入れ基準

- PImpl の公開 ABI と所有契約が文書化されている。
- `Impl` の確保元・解放元が一意である。
- constructor 例外時にリークしない。
- debug でサイズ・アラインメント・ドメイン不一致を検出できる。
- pool／arena を使う型には適用理由と寿命制約が記録されている。
- 標準 adapter から別方式へ差し替えても公開 API を変更しない。
- GPU／レンダリングのホットパスへ不用意に導入していない。
- 実装後のビルド・テスト・runtime 検証結果が別途記録されている。

## 10. 未決事項

1. 既存 ArtifactCore／Artifact に公式 allocator 抽象が存在するか。
2. DLL 境界で統一すべき allocation domain の単位。
3. 既存のメモリ診断・プロファイラへ allocation tag を追加できるか。
4. pool の thread-safe 要件と返却キューの有無。
5. in-place PImpl を許可する公開型の最大サイズ基準。

未決事項は Phase 0 の棚卸しで確定し、実装前に対象クラス単位の設計レビューを行う。
