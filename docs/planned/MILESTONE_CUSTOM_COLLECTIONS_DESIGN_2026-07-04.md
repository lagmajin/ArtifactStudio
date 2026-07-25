# 自前コレクションライブラリ 全体設計 (2026-07-04)

> std も Qt も使わない。純粋 C++20。`ArtifactHashMap` と同じ哲学。
> 依存: `<cstddef>`, `<cstdint>`, `<cassert>`, `<new>`, `<utility>` のみ。

## ロードマップ

| Phase | 型 | 置換元 | 使用箇所数 | 工数 |
|---|---|---|---|---|
| P0-1 | `Array<T>` | `std::vector` / `QVector` | 3,069+4,429 | 大 |
| P0-2 | `String` | `std::string` | 1,548 | 中 |
| P0-3 | `Ptr<T>` / `Ref<T>` / `Owned<T>` | `shared_ptr`/`unique_ptr` | 1,132 | 大 |
| P1-1 | `Mutex` / `Lock` / `Cond` | `std::mutex`等 | 808 | 中 |
| P1-2 | `Callback<Sig>` | `std::function` | 406 | 小 |
| P1-3 | `Set<T>` | `std::set`/`unordered_set` | 14 | 小 |
| P2-1 | `Queue<T>` | `std::queue` | 3 | 小 |
| P2-2 | `Thread` | `std::thread` | 43 | 中 |

### 既存の自前実装（完了済み）

| 型 | 状態 |
|---|---|
| `HashMap<K,V>` | ✅ 完全実装 (`ArtifactHashMap`) |
| `Optional<T>` | ✅ 完全実装 (`ArtifactOptional`) |
| `Atomic<T>` | ✅ 薄いラッパー (`ArtifactAtomic`) |
| `Span<T>` | ✅ 完全実装 (`ArtifactSpan`) |
| `Variant` | ✅ 完全実装 (`ArtifactVariant`) |

---

## 全型 API 設計

### `Array<T>` — 動的配列（P0-1 最優先）

```cpp
template <typename T>
class Array {
public:
    Array() = default;
    Array(std::initializer_list<T> init);
    Array(const Array& other);           // deep copy
    Array(Array&& other) noexcept;       // move
    ~Array();

    // 追加
    void add(const T& value);
    void add(T&& value);
    void addAll(const Array& other);

    // 安全アクセス（範囲チェック付き）
    Optional<T&> at(size_t index);
    Optional<const T&> at(size_t index) const;
    Optional<T&> first();
    Optional<T&> last();

    // 削除
    void removeAt(size_t index);
    void removeFirst();
    void removeLast();
    void removeAll();   // clear

    // 容量
    size_t size() const;
    size_t capacity() const;
    bool isEmpty() const;
    void reserve(size_t n);

    // イテレーション（安全: 範囲forのみ許可）
    T* begin();
    T* end();
    const T* begin() const;
    const T* end() const;

    // operator[] は提供しない（範囲チェックなしのため危険）
};
```

### `String` — 文字列（P0-2）

```cpp
class String {
public:
    String() = default;
    String(const char* cstr);
    String(const char* data, size_t len);
    String(const String& other);
    String(String&& other) noexcept;
    ~String();

    // 基本的な API のみ。複雑な操作は free function で
    size_t length() const;
    bool isEmpty() const;
    const char* data() const;
    char at(size_t index) const;

    String& operator+=(const String& other);
    bool operator==(const String& other) const;

    // 部分文字列（ゼロコピー view）
    StringView sub(size_t start, size_t len) const;
};
```

### `Ptr<T>` / `Ref<T>` — スマートポインタ（P0-3）

```cpp
// 参照カウント方式の共有ポインタ
template <typename T>
class Ptr {
public:
    Ptr();                              // null
    explicit Ptr(T* raw);               // 所有権取得
    Ptr(const Ptr& other);              // 参照カウント++
    Ptr(Ptr&& other) noexcept;          // ムーブ
    ~Ptr();                             // 参照カウント--。0 で delete

    // operator-> と operator* は提供しない
    Optional<T&> get();                 // 安全アクセス
    Optional<const T&> get() const;

    template <typename F>
    void ifValid(F&& fn);               // null なら呼ばれない

    bool isValid() const;
    int useCount() const;               // デバッグ用
    void reset();                       // 解放
};

// 非 null 共有参照（デフォルト構築不可）
template <typename T>
class Ref {
public:
    explicit Ref(T* raw);               // raw は非 null 必須（assert）
    explicit Ref(const Ptr<T>& ptr);    // null なら assert
    Ref(const Ref& other);
    ~Ref();

    T* operator->();
    T& operator*();
    const T* operator->() const;
    const T& operator*() const;
};
```

### `Mutex` / `Lock` — スレッド同期（P1-1）

```cpp
class Mutex {
public:
    Mutex();
    ~Mutex();
    Mutex(const Mutex&) = delete;
    void lock();
    void unlock();
    bool tryLock();
};

class Lock {  // RAII
public:
    explicit Lock(Mutex& m);
    ~Lock();
    Lock(const Lock&) = delete;
};
```

---

## 実装方針

- 全型 `.ixx` にテンプレート実装を含める（ヘッダオンリー）
- メモリ: `new`/`delete` のみ。アロケータ抽象化はしない（シンプルさ優先）
- 例外: 使用しない。失敗は assert または `Optional` で表現
- 既存の `HashMap`, `Optional`, `Atomic`, `Span`, `Variant` はそのまま流用
- 名前空間: `ArtifactCore`

---

## Next Execution Slice

### Phase 0A の着手点

- `Array<T>` を最初に固め、`add / addAll / removeAt / size / begin-end` だけの最小 API で置換の受け皿を作る
- `String` は `length / data / at / += / == / sub` のみに絞り、`StringView` を先に依存として確定する
- `Optional<T&>` を使うアクセス経路を、境界チェック付き API として既存コードへ差し込みやすい形にする
- `operator[]` や複雑なアルゴリズム系 API は Phase 0 では足さず、移行時の面積を小さく保つ

### Phase 0B の着手点

- `Ptr<T>` と `Ref<T>` を `Array<T>` / `String` より後に回し、参照カウントの責務と null 許容の境界を明確にする
- `Ref<T>` は non-null 前提の短い経路に限定し、既存の raw pointer 置換を一気に広げない
- `Owned<T>` は move-only の所有権表現として最後に足し、既存 `unique_ptr` 代替の最小路線に留める

### Phase 1 前提

- `Mutex / Lock / Cond` はコレクション置換の土台が固まってから進める
- `Callback<Sig>` と `Set<T>` は API 利用箇所の偏りを見てから広げる
- `Queue<T>` / `Thread` は基礎型の採用が進んでから再評価する

## 2026-07-25 実装監査

- `ArtifactArray`／`ArtifactString`／`ArtifactPtr`／`ArtifactSet`／`ArtifactQueue`／`ArtifactThread`／`ArtifactCallback` と既存の `ArtifactHashMap`／`ArtifactOptional`／`ArtifactAtomic`／`ArtifactSpan`／`ArtifactVariant` の基盤型を確認できる。
- `ArtifactFoundation` から自前型をまとめて公開する経路も存在するため、P0／P1 の受け皿は部分的に整備済みである。
- ただしロードマップに示された std::vector／std::string／スマートポインタ／同期型等の既存利用箇所全体の置換は未完了で、使用箇所数規模の移行完了や品質検証も確認できない。
- よって本マイルストーンは基盤型の一部実装済み、全体移行は未着手・継続中と判定する。