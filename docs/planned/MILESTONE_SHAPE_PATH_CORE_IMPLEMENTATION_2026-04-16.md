# MILESTONE: ShapePath コア実装

作成日: 2026-04-16
対象: M13 ~ M14 (4月〜5月)
調査元: ArtifactStudio コードベース監査 (2026-04-16)
状態: ⚠️ 宣言のみで実装未完

---

## ■ 現状の問題定義

### 1. ShapePath は「宣言のみ」状態

`ArtifactCore/include/Shape/ShapePath.ixx`（295行）にはクラス定義があるが：

```cpp
private:
    class Impl;        // 前方宣言のみ
    Impl* impl_;       // Pimpl ポインター
```

**`ShapePath::Impl` の実体定義はどこにも存在しない。**
- `src/Shape/ShapePath.cppm` 这样的人 файл 不存在
- `include/Shape/` 内の他の .ixx ファイルにも Impl 定義なし
- `Core/ShapePath.ixx` は別のスタブクラス（28行）

結果として、全メソッドが宣言のみで**リンクエラー/未定義参照**になる状態。

### 2. ShapeGroup が COMPILE 失敗风险

`ShapeGroup.ixx:351-366` で `processedPaths()` は `pathShape->path()` を呼び出す：

```cpp
for (const auto& child : children_) {
    if (auto* pathShape = dynamic_cast<PathShape*>(child.get())) {
        paths.push_back(pathShape->path());  // ← ShapePath のコピー
    }
}
```

`PathShape` の `path()` は `const ShapePath&` を返すが、`ShapePath` の実体がないため**コンパイルエラーの可能性大**。

### 3. コード構成の不整合

- `.ixx` と `.cppm` の分離はプロジェクト標準（MaskPath が完璧な例）
- ところが ShapePath には `.cppm` がなく、`ShapeGroup.ixx` に実装が直書き（368行中約200行が実装）
- これは**プロジェクト標準パターン違反**で、保守性を損なう

---

## ■ 実装目標

### フェーズ1: ShapePath コア実装（M13 完了目標）

**期限**: 2026-04-30
**目標**: ShapePath が単体コンパイル・リンク可能になり、基本的なパス操作ができる

| 機能 | 達成条件 |
|------|----------|
| データ構造 | `ShapePath::Impl` が `std::vector<PathCommand>` を保持 |
| 構築 | `moveTo`, `lineTo`, `cubicTo`, `quadTo`, `close`, `arcTo` が正常動作 |
| 幾何学 | `boundingRect()`, `contains()`, `length()`, `pointAtLength()` が正確な値返す |
| 変換 | `translate`, `scale`, `rotate`, `transform` で座標変換 correct |
| Qt変換 | `toPainterPath()` / `fromPainterPath()` で QPainterPath と相互変換 |
| ユーティリティ | `clone()`, `reverse()`, `addPath()`, `simplify()` 実装 |

### フェーズ2: ShapeGroup 整理（M14 完了目標）

**期限**: 2026-05-15
**達成条件**: 
- `ShapeGroup.cppm` 新規作成、実装を移動
- `ShapeGroup.ixx` は宣言のみに（通常は200行以下）
- コンパイル時間短縮、依存関係明確化

### フェーズ3: MaskPath 相互変換（M14 完了目標）

**期限**: 2026-05-31
**目標**: `ShapePath ↔ MaskPath` 双方向変換、UI メニュー項目実装

---

## ■ 依存関係

### 外部依存
- Qt6: `QPainterPath`, `QPointF`, `QRectF`（既存プロジェクトでOK）
- C++23: モジュール、デフォルト可能な `std::vector`, `std::algorithm` など

### 内部依存
```
Shape.Path (ShapePath.ixx + .cppm)
    ├── import Shape.Types       // PathCommand, BezierSegment 等
    └── import Core.Math         // 数学関数（必要に応じて）

Shape.Group (ShapeGroup.ixx + .cppm) → import Shape.Path
Shape.Layer (ShapeLayer.ixx)       → import Shape.Path, Shape.Group
Shape.Operator (各演算子)          → import Shape.Path
```

**注意**: `Shape.Path` はモジュール境界で `Shape.Types` に依存。`Shape.Types` 自体に `ShapePath` への依存なし（循環依存回避済み）。

---

## ■ 実装タスク分解

### タスク1: ShapePath::Impl 設計（4h）

**担当**: 1人
**成果物**: `ArtifactCore/src/Shape/ShapePath.cppm` 実装骨架

内容：
- メンバー変数の決定：
  ```cpp
  struct Impl {
      std::vector<PathCommand> commands_;  // パスコマンド列
      mutable QRectF cachedBounds_;        // 計算済みバウンディングボックス
      mutable bool dirty_;                 // キャッシュ無効フラグ
  };
  ```
- キャッシュ戦略の設計（dirty flag 方式）
- move semantics 対応（ShapePath は宣言で `= default` できるか検討）

### タスク2: コアメソッド実装（12h）

**担当**: 1-2人
**ファイル**: `ArtifactCore/src/Shape/ShapePath.cppm`

実装リスト：

| カテゴリ | メソッド | 工数 | 備考 |
|----------|----------|------|------|
| 構築 | `clear`, `moveTo`, `lineTo` | 1h | 単純な push_back |
| 構築 | `cubicTo`, `quadTo`, `close` | 1.5h | ベジェコマンド追加 |
| 構築 | `arcTo` | 2h | 円弧→ベジェ変換必要 |
| 図形プリミティブ | `setRectangle`, `setEllipse`, `setPolygon`, `setStar` | 2h | ヘルパー関数作成 |
| プロパティ | `name`, `setName`, `isClosed`, `setClosed`, `isEmpty`, `commandCount`, `commands` | 1h | 単純アクセサ |
| ジオメトリ | `boundingRect` | 2h | キャッシュ＋再計算ロジック |
| ジオメトリ | `contains` | 1.5h | 的境界＋点在判定（複雑） |
| ジオメトリ | `length`, `pointAtLength` | 2.5h | ベジェ曲線長さ計算（数値積分） |
| ジオメトリ | `toSegments` | 1h | PathCommand→BezierSegment 変換 |
| 変換 | `translate`, `scale`, `rotate`, `transform` | 2h | 全コマンドに行列適用 |
| Qt連携 | `toPainterPath()` | 1.5h | PathCommand→QPainterPath 変換 |
| Qt連携 | `fromPainterPath()` | 2h | QPainterPath→PathCommand 分解（既存参考） |
| ユーティリティ | `clone` | 0.5h | Impl コピー |
| ユーティリティ | `reverse` | 1h | コマンド順序＋ベジェ制御点反転 |
| ユーティリティ | `addPath` | 1h | コマンド連結 |
| ユーティリティ | `simplify` | 2h | Douglas-Peucker アルゴリズム |

**合計**: ~25h（3日）

### タスク3: ShapeGroup リファクタリング（6h）

**担当**: 1人
**ファイル新規**: `ArtifactCore/src/Shape/ShapeGroup.cppm`

内容：
1. `ShapeGroup.ixx` から全実装関数を `.cppm` へ移動
   - 約200行の実装コード
   - `addChild`, `insertChild`, `removeChild`, `clearChildren`
   - `childCount`, `childAt`, `children`, `indexOf`
   - `boundingRect`, `translate`, `scale`, `rotate`, `clone`
   - `processedPaths()`

2. `ShapeGroup.ixx` を宣言のみに（50-80行に縮小）

3. CMake に `ShapeGroup.cppm` 追加

### タスク4: MaskPath 相互変換（8h）

**担当**: 1人
**ファイル**: `ArtifactCore/src/Shape/ShapePath.cppm` に追加実装

内容：
- `MaskPath toMaskPath() const` 実装
  - `PathCommand` を `MaskVertex` に変換
  - cubicTo → outTangent/inTangent 設定
  - マスク属性デフォルト値設定（opacity=1, feather=0, inverted=false, mode=Add）

- `static ShapePath fromMaskPath(const MaskPath&)` 実装
  - `MaskVertex` を `PathCommand` に変換
  - ベジェ制御点復元

- **双方向テスト**（単体テストに追加）

### タスク5: 単体テスト作成（6h）

**担当**: 1人
**テストファイル**: `ArtifactCore/test/ShapePathTest.cpp`（新規）

テストケース：
1. 四角形作成 → `boundingRect` 確認
2. 円作成 → `pointAtPercent` と `length` の精度検証
3. 複雑パス（星形） → `contains`, `pointAtLength` 異常値チェック
4. 変換: translate/scale/rotate でバウンディングボックス正しく変化
5. QPainterPath 変換：往復で同じShapePathになる
6. simplify: 直線部分が縮小されること
7. MaskPath 変換：パス形状が維持されること、属性がデフォルト適用

### タスク6: 既存コードの COMPILE 検証（2h）

**担当**: 1人
**目的**: ShapePath 実装後、ShapeGroup, ShapeLayer 等が正しくコンパイル・リンクするか確認

手順：
1. ArtifactCore モジュールを個別ビルド
2. `ShapeGroup` の `processedPaths()` が呼び出せるか
3. `ShapeLayer` が `toPainterPath()` 使用できるか
4. CMake ターゲット `ArtifactCore` 完全ビルド

---

## ■ 工数とスケジュール

| タスク | 工数 (h) | 担当 | 期限 | 依存 |
|--------|----------|------|------|------|
| T1: Impl 設計 | 4 | A | 4/19 | – |
| T2: コア実装 | 12 | A | 4/26 | T1 |
| T3: 単体テスト | 6 | A | 4/28 | T2 |
| T4: ShapeGroup 整理 | 6 | B | 4/30 | T2 |
| T5: 相互変換 | 8 | A | 5/07 | T2 |
| T6: 統合テスト | 2 | A/B | 5/08 | T4,T5 |
| **合計** | **38** | – | – | – |

**想定スケジュール**:
- **Week 1 (4/16-4/20)**: T1 設計 + T2 実装開始（コア構築）
- **Week 2 (4/21-4/27)**: T2 実装継続 + T3 テスト（ until 4/28）
- **Week 3 (4/28-5/04)**: T4 ShapeGroup 整理 + T5 相互変換（GW中）
- **Week 4 (5/05-5/09)**: T6 統合テスト、レビュー、マージ

---

## ■ 品質基準（Completion Criteria）

### ✓ Phase 1 完了条件
- [ ] `ArtifactCore` がエラーなくコンパイル・リンクする
- [ ] ShapePath 単体テスト 80% 以上パス
- [ ] toPainterPath() が QPainterPath と同一視覚結果を生成（ピクセル比較）
- [ ] コードレビュー通過（Pimpl パターン正しく使用、メモリ管理OK）

### ✓ Phase 2 完了条件
- [ ] ShapeGroup.cppm が独立してコンパイル可能
- [ ] ShapeGroup.ixx が宣言のみ（200行以下）
- [ ] 既存の ShapeGroup  Tests が通る

### ✓ Phase 3 完了条件
- [ ] ShapePath → MaskPath 変換でパス形状が維持（頂点座標誤差 < 0.001px）
- [ ] MaskPath → ShapePath 変換で QPainterPath 往復一致
- [ ] UI メニュー「シェイプをマスクに変換」「マスクをシェイプに変換」が有効（実装は別途）

---

## ■ リスクと対策

| リスク | 影響 | 発生確率 | 対策 |
|--------|------|----------|------|
| ベジェ曲線長さ計算の誤差 | 中 | 高 | 数値積分（Gauss-Legendre）採用、充分なサンプリング |
| arcTo の実装複雑さ | 高 | 中 | Qt の `QPainterPath::arcTo` を参考に実装 |
| キャッシュ無効化漏れ | 中 | 中 | translate/scale/rotate 全てで dirty_=true 設定、UnitTest で検証 |
| ShapeGroup 循環依存 | 高 | 低 | include 関係を再確認（既に Types で分離られている） |
| コンパイル時間増大 | 低 | 中 | ヘッダーインクルード最小化、forward declaration 活用 |

---

## ■ 参考情報

### 関係ファイル一覧

```
ArtifactCore/
├── include/Shape/
│   ├── ShapePath.ixx     (295行)  ← 宣言改訂必要（Impl定義追加）
│   ├── ShapeGroup.ixx    (368行)  ← 実装移動後は宣言のみ
│   ├── ShapeLayer.ixx    (214行)
│   ├── ShapeOperator.ixx ( 48行)
│   ├── ShapeTypes.ixx    (189行)  ← PathCommand, BezierSegment 定義
│   └── TrimPaths.ixx     ( 86行)
├── src/Shape/              ← ディレクトリ存在せず（新規作成必要なし）
│   ├── ShapePath.cppm      (新規予定 250行)
│   └── ShapeGroup.cppm     (新規予定 200行)
└── test/
    └── ShapePathTest.cpp   (新規予定 300行)
```

### MaskPath との比較（実装完了例）

MaskPath は以下のように完全実装済み：
```
Artifact/
├── include/Mask/
│   ├── MaskPath.ixx   (107行)  ← Impl 定義含む
│   └── LayerMask.ixx  ( 74行)
├── src/Mask/
│   ├── MaskPath.cppm  (223行)  ← 全実装
│   └── LayerMask.cppm (158行)
```

ShapePath もこの MaskPath と同様の構成を目指す。

---

## ■ 備考

### QPainterPath 精度問題

`toPainterPath()` は Qt の QPainterPath に変換するが、QPainterPath は float 精度（qreal が float マシン）的可能性あり。AfterEffects レベルのサブピクセル精度を求める場合は：

1. **内部的に double 保持**（ShapePath::Impl の座標は double）
2. QPainterPath 変換時に float への丸めが発生（QPointF は qreal、Windows では float）
3. 高精度が必要なラスタライズは QPainterPath 依存しない独自実装を別途検討

ただし、まずは ShapePath コア実装を優先。精度問題は「既知の課題」として後回しで可。

---

## ■ 次のアクション

1. **即時（Today）**:
   - [ ] このマイルストーンを `docs/planned/MILESTONE_SHAPE_PATH_CORE_IMPLEMENTATION_2026-04-16.md` として保存
   - [ ] AGENTS.md に ShapePath 実装タスクを追加（必要に応じて）

2. **Week 1**:
   - [ ] T1: ShapePath::Impl 設計、実装開始
   - [ ] ビルドシステム確認（CMakeLists.txt に ShapePath.cppm 追加方法調査）

3. **決定待ち**:
   - [ ] 実際の実装者の割り当て
   - [ ] 工数見積もりの承認（38h = 5人日相当）

---

**作成者**: Kilo（AI Agent）
**最終更新**: 2026-04-16
