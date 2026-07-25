# M-FIGMA-2 Component / Instance System Milestone

作成日: 2026-07-07
ステータス: Draft
対象: `ArtifactCore/include/Shape/ShapeGroup.ixx`,
      `Artifact/src/Layer/ArtifactShapeLayer.cppm`,
      `Artifact/src/Layer/ArtifactAbstractLayer.cppm`,
      `Artifact/src/Widgets/Inspector/ArtifactInspectorWidget.cppm`,
      `Artifact/src/Project/ArtifactProjectManager.cppm`
位置づけ: Figma のコンポーネント/インスタンスシステムを ArtifactStudio に移植。
          マスターコンポーネントの編集が全インスタンスに伝播し、
          プロパティオーバーライドで個別カスタマイズを許す。
参照:
- `docs/planned/MILESTONE_BEYOND_AE_DIFFERENTIATION_2026-06-02.md` (Z-C2)
- `ArtifactCore/include/Shape/ShapeGroup.ixx`（clone, transform, operator）
- `docs/planned/MILESTONE_FIGMA_AUTO_LAYOUT_2026-07-07.md` (M-FIGMA-1)

---

## 1. 目的

Figma のコンポーネント/インスタンスシステムは、UI デザインの生産性を劇的に変えた。
ArtifactStudio にこれを導入することで、以下が可能になる:

- ボタン・ヘッダー・カードなどのモーション部品を「コンポーネント」として定義
- コンポーネントを複製した「インスタンス」を comp 内の複数箇所に配置
- マスターコンポーネントを編集すると、全インスタンスに即時反映
- 特定インスタンスの特定プロパティ（色、テキスト、サイズ）だけをオーバーライド

AE にはこの概念がなく、すべてのレイヤーは独立した複製。
「ボタンの角丸を1px大きくしたい」場合、全ボタンを手動で修正する必要がある。

> 重要: `ShapeGroup` の既存 `clone()` メソッドを拡張して、
> マスター参照を保持する「インスタンス」型を作る。


---

## 2. 現状整理

### 2.1 既存資産

| 資産 | 内容 |
|---|---|
| `ShapeElement::clone()` | 全 Shape 型に深いクローン。インスタンスの複製基盤として流用可能 |
| `ShapeGroup` 階層 | 子要素管理 + transform + operator。コンポーネントのコンテナとして自然 |
| `ArtifactProjectManager` | プロジェクト全体のアセット管理。コンポーネントライブラリの置き場所 |
| PreCompose | レイヤーを pre-comp に変換する既存フロー。コンポーネント化の参照パターン |

### 2.2 不足

| 軸 | 状況 |
|---|---|
| `ComponentMaster` データモデル | なし |
| インスタンスのマスター参照 | なし |
| プロパティオーバーライド | なし |
| マスター変更の伝播 | なし |
| Component Library パネル UI | なし |
| ネストコンポーネント | なし |
| Variant（コンポーネントバリアント） | なし |

### 2.3 コード検索: ComponentMaster → 0 hit, InstanceOverride → 0 hit

---

## 3. Scope / Non-Goals

### Scope
- `ComponentMaster`: マスター定義。ShapeGroup または Layer を内包
- `ComponentInstance`: マスター参照 + オーバーライドプロパティの辞書
- マスター編集 → 全インスタンス伝播
- オーバーライド: position, rotation, scale, fill color, text content, cornerRadius
- Component Library パネル（プロジェクト内コンポーネント一覧）
- Reset Instance / Detach Instance（インスタンスを通常レイヤーに戻す）

### Non-Goals
- クロスプロジェクトコンポーネント共有 → 将来
- Variant（コンポーネントバリアント）→ Figma の variant は別 milestone
- コンポーネントのキーフレーム継承 → 将来検討

---

## 4. Phases

### Phase 1: ComponentMaster + Instance コア (P0, 2 セッション)
- `ComponentMaster.ixx` / `.cppm` を `ArtifactCore` に新規追加
- `ComponentMaster`: id, name, `ShapeGroup` の所有、プロパティ公開リスト
- `ComponentInstance`: masterId 参照、オーバーライド辞書（`propertyPath → value`）
- `evaluate()`: マスターの値をベースに、オーバーライドを適用して最終値を返す
- `ShapeGroup` に `createMaster()` / `createInstance()` ファクトリ追加

**Done criteria:**
- マスター作成 → インスタンス生成 → オーバーライド適用 → 正しい最終値
- マスターの cornerRadius 変更 → 全インスタンスに伝播
- インスタンスの color override がマスター変更後も保持

### Phase 2: Component Library パネル (P0, 2 セッション)
- `ArtifactComponentLibraryWidget` 新規: プロジェクト内コンポーネント一覧 + サムネイル
- パネルから comp へドラッグでインスタンス配置
- インスタンス選択時に Inspector に「Master Component」セクション表示
  - "Go to Master" ボタン
  - オーバーライドプロパティ一覧 + Reset ボタン
  - "Detach Instance" ボタン

**Done criteria:**
- Component Library からドラッグ→comp にインスタンス配置
- マスター編集→インスタンスがビューポートで即更新
- Detach Instance で通常レイヤーに変換

### Phase 3: 永続化 + Diagnostics (P1, 1 セッション)
- project JSON に `components[]` セクション追加
- インスタンスのシリアライズ（masterId + overrides）
- 旧プロジェクト互換 / Problem View 診断（broken master reference etc.）

---

## 5. Done Criteria (全体)
- マスター編集→全インスタンス伝播 / オーバーライドの個別維持
- Component Library パネル + ドラッグ配置
- Detach / Reset 動作 / 保存復元
- 新規 signal-slot / QImage / setStyleSheet なし

---

## 6. 更新履歴
- 2026-07-07: 初版作成。Figma Component/Instance システム移植設計。

> 新規のコンポーネント管理システムを `ArtifactCore` に追加する。


---

## Static audit follow-up (2026-07-25)

Figma 固有の ComponentMaster／ComponentInstance は確認できない。ただし、ArtifactCompositionLayer に master property overrides の辞書、effective value、apply／clear、JSON 保存復元があり、TemplateSlot／TemplateVariation に別系統の template override 基盤も存在する。これらは再利用候補だが、ShapeGroup の master 参照や instance identity とは別契約である。

Component Library、Go to Master、Reset／Detach、master 変更の全 instance 伝播、ネスト component、Figma variant は未実装または未確認。したがって Phase 1〜3 は未完了で、既存 override／template 機能を Component System 完了の証拠とは扱わない。
