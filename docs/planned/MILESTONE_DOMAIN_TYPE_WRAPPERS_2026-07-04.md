# マイルストーン: テンプレート露出削減 → ドメイン型ラッパー (2026-07-04)

> 目標: `std::vector<std::shared_ptr<X>>` のような素のテンプレート露出を撲滅し、
> AI が誤らない自己文書化的な型で包む。

## 問題

現在のコードベースの典型的なパターン:

```cpp
// ❌ AI が間違いやすい素のテンプレート露出
std::vector<std::shared_ptr<ArtifactAbstractLayer>> layers_;
std::unordered_map<LayerID, std::shared_ptr<ArtifactAbstractEffect>> effects_;
std::optional<FramePosition> inPoint_;
QVector<QPair<QString, QVariant>> propertyOverrides_;
std::function<void(const QString&, bool)> completionHandler_;

// 使い方
layers_.push_back(layer);
auto it = effects_.find(id);
if (it != effects_.end()) { auto& e = it->second; ... }
```

問題点:
- AI が `push_back` / `emplace_back` / `insert` を間違える
- `shared_ptr` / `unique_ptr` / `weak_ptr` の選択ミス
- `find()` → `end()` チェック忘れ
- `optional` の `value()` / `*` の使い分けミス
- 型名が長すぎて可読性を損なう

## 解決: ドメイン型ラッパー

```cpp
// ✅ AI が間違えにくいドメイン型
LayerList layers_;                          // 内部は QVector<ArtifactAbstractLayerPtr>
EffectRegistry effects_;                    // 内部は QHash<LayerID, ArtifactAbstractEffectPtr>
OptFrame inPoint_;                          // 内部は std::optional<FramePosition>
PropertyOverrides overrides_;               // 内部は QVector<PropertyOverride>
CompletionHandler onDone_;                  // 内部は std::function<...>
```

## 設計方針

| 原則 | 説明 |
|---|---|
| **型名は名詞** | `LayerList`, `EffectRegistry`, `PropertyBag` — 用途が一目で分かる |
| **内部実装を隠蔽** | `.add()`, `.remove()`, `.contains()`, `.get()` の統一 API |
| **null 安全** | ポインタ型は `get()` が `nullptr` チェック付き。生ポインタ露出禁止 |
| **暗黙変換なし** | 内部型との暗黙変換は禁止。明示的 `toStd()` / `toQt()` のみ |


---

## コレクション型（vector 代替 / 3,069 箇所）

### `Artifact::Array<T>` — 動的配列（QVector ラッパー）

```cpp
// 最小 API — AI が使うメソッドを限定
export template <typename T>
class Array {
public:
    void add(const T& item);          // push_back の統一名称
    void add(T&& item);
    void remove(int index);
    void removeAll();                 // clear より明示的
    T& at(int index);                 // [] より安全（範囲チェック付き）
    const T& at(int index) const;
    T* find(auto predicate);         // find_if ラッパー。nullptr で未発見
    int size() const;
    bool isEmpty() const;
    T& first();                       // front() より名詞的
    T& last();
    
    // range-for 対応のみ公開
    auto begin();
    auto end();
};
```

### ドメイン別名

```cpp
using LayerList      = Array<ArtifactAbstractLayerPtr>;
using CompositionList = Array<ArtifactCompositionPtr>;
using EffectStack    = Array<ArtifactAbstractEffectPtr>;
using KeyframeArray  = Array<KeyframeData>;
using AssetPathList  = Array<QString>;
using FrameBuffer    = Array<ImageF32x4_RGBA>;
```

---

## マップ型（unordered_map / map 代替 / 241 箇所）

### `Artifact::Dict<K,V>` — 辞書（QHash/QMap ラッパー）

```cpp
export template <typename K, typename V>
class Dict {
public:
    void set(const K& key, const V& value);  // insert/emplace 統一
    V get(const K& key) const;               // 見つからない場合はデフォルト値
    V getOr(const K& key, const V& fallback) const;
    bool contains(const K& key) const;
    void remove(const K& key);
    int size() const;
    Array<K> keys() const;
    Array<V> values() const;
};
```

### ドメイン別名

```cpp
using EffectRegistry    = Dict<LayerID, ArtifactAbstractEffectPtr>;
using PropertyBag       = Dict<QString, QVariant>;
using KeyframeChannel   = Dict<FramePosition, KeyframeValue>;
using LayerCache        = Dict<LayerID, CachedRenderData>;
using ProjectAssetIndex = Dict<QString, AssetInfo>;
```

---

## ポインタ型（shared_ptr / unique_ptr 代替 / 1,132 箇所）

### `Artifact::Ptr<T>` / `Artifact::WeakPtr<T>` / `Artifact::Owned<T>`

```cpp
// Ptr = QSharedPointer ラッパー。null 安全 API
export template <typename T>
class Ptr {
public:
    T* operator->();               // 従来通りのアクセス
    T& operator*();
    T* get();                      // 生ポインタ（AI に使わせない）
    bool isValid() const;          // nullptr チェックの統一名称
    explicit operator bool() const;
    template <typename U>
    Ptr<U> cast() const;           // dynamicCast ラッパー
};

// Owned = QScopedPointer ラッパー。所有権明確化
export template <typename T>
class Owned {
    // unique_ptr 相当。move のみ、copy 禁止
};

// WeakPtr = QWeakPointer ラッパー
export template <typename T>
class WeakPtr {
public:
    Ptr<T> lock() const;           // promote to Ptr
    bool isExpired() const;
};
```

### ドメイン別名

```cpp
using LayerPtr       = Ptr<ArtifactAbstractLayer>;
using LayerWeakPtr   = WeakPtr<ArtifactAbstractLayer>;
using CompPtr        = Ptr<ArtifactAbstractComposition>;
using EffectPtr      = Ptr<ArtifactAbstractEffect>;
using AssetPtr       = Ptr<Asset>;
using ProjectPtr     = Ptr<ArtifactProject>;
```

---

## オプショナル型（std::optional 代替 / 171 箇所）

### `Artifact::Opt<T>` — 値の有無を表現

```cpp
export template <typename T>
class Opt {
public:
    bool hasValue() const;          // has_value() より自然
    T& value();                     // 例外の代わりに assert
    const T& value() const;
    T valueOr(const T& fallback) const;
    void set(const T& v);
    void clear();
    explicit operator bool() const;
};
```

### ドメイン別名

```cpp
using OptFrame      = Opt<FramePosition>;
using OptLayer      = Opt<LayerID>;
using OptColor      = Opt<QColor>;
using OptPath       = Opt<QString>;
```

---

## スレッド型（mutex / atomic 代替 / 808 箇所）

### `Artifact::Lock` / `Artifact::Atomic<T>`

```cpp
// RAII 排他ロック（QMutexLocker ラッパー）
class Lock {
public:
    Lock();                         // 内部で QMutex を自動確保
    ~Lock();                        // 自動 unlock
    // copy 禁止、move 禁止
};

// アトミック型
template <typename T>
class Atomic {
public:
    T load() const;
    void store(const T& value);
    T exchange(const T& value);
    bool compareExchange(const T& expected, const T& desired);
};
```

---

## 関数型（std::function 代替 / 406 箇所）

### `Artifact::Callback<Signature>` / `Artifact::Action`

```cpp
// Callback = 単発コールバック
template <typename Signature>
class Callback {
    // std::function ラッパー。null チェック付き
};

// Action = 引数なしコールバック
using Action = Callback<void()>;

// ドメイン別名
using LayerVisitor     = Callback<void(const LayerPtr&)>;
using FrameVisitor     = Callback<void(FramePosition)>;
using CompletionHandler = Callback<void(bool success)>;
using PropertyObserver  = Callback<void(const QString& propertyName, const QVariant& value)>;
```

---

## 全貌サマリ

| カテゴリ | 新設型 | 置換元 | 箇所数 | AI エラー防止効果 |
|---|---|---|---|---|
| コレクション | `Array<T>`, `LayerList` 他 | `std::vector`, `QVector` | 3,069+ | ★★★ |
| マップ | `Dict<K,V>`, `PropertyBag` 他 | `std::map`, `QHash` | 241+ | ★★★ |
| ポインタ | `Ptr<T>`, `WeakPtr<T>`, `Owned<T>` | `shared_ptr`, `unique_ptr` | 1,132 | ★★★ |
| オプショナル | `Opt<T>` | `std::optional` | 171 | ★★ |
| スレッド | `Lock`, `Atomic<T>` | `mutex`, `atomic` | 808 | ★★★ |
| 関数 | `Callback<Sig>`, `Action` | `std::function` | 406 | ★★ |

### 想定モジュール構成

```
ArtifactCore/
  include/
    Foundation/
      Array.ixx        — Array<T>, Dict<K,V>
      Pointer.ixx      — Ptr<T>, WeakPtr<T>, Owned<T>
      Optional.ixx     — Opt<T>
      Threading.ixx    — Lock, Atomic<T>
      Callback.ixx     — Callback<Sig>, Action
    Domain/
      LayerTypes.ixx   — LayerPtr, LayerList, EffectStack
      CompTypes.ixx    — CompPtr, CompositionList
      PropertyTypes.ixx — PropertyBag, OptFrame, OptColor
```

### 実装優先度

| Phase | 内容 | 新設型 | 工数 |
|---|---|---|---|
| P0 | `Array<T>` + `Ptr<T>` + `Dict<K,V>` | 基盤3型 | 小（実装は薄いラッパー） |
| P0 | 全 `.cppm` から `#include <vector>` 削除 | — | 中（機械的に可能） |
| P1 | `Lock` + `Atomic<T>` + `Opt<T>` | スレッド・オプショナル | 中 |
| P1 | ドメイン別名定義 (`LayerList` 他) | typedef | 小 |
| P2 | `Callback<Sig>` + `Action` | 関数 | 中 |
| P3 | 全コードの段階的移行 | — | 大 |
| **C++20 modules 対応** | ヘッダ依存最小化。前方宣言で済むよう .ixx はポインタのみ |