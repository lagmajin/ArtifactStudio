# M-TEMPLATE-1 Project Template Gallery Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Widgets/Dialog/ArtifactCreateCompositionDialog.cppm`,
      `Artifact/src/Widgets/Welcome/ArtifactWelcomeScreen.cppm` (将来),
      `Artifact/src/Service/ArtifactProjectService.cppm`,
      `Artifact/src/Service/ArtifactTemplateService.cppm` (新規),
      `Artifact/src/Project/ArtifactProjectManager.cppm`,
      `ArtifactCore/src/Project/ProjectTemplate*`
位置づけ: AE の "Save as Template" / Premiere の "Project Templates" 互換の foundation。
参照:
- `docs/analysis/REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2
- `docs/analysis/MOTION_GRAPHICS_AD_PRODUCTION_THINKING_MEMO_2026-05-28.md` (テンプレ量産)
- `docs/planned/MILESTONE_AD_PRODUCTION_ACCELERATOR_2026-05-28.md`
- `docs/planned/MILESTONE_AD_PRODUCTION_ACCELERATOR_PHASE1_EXECUTION_2026-05-29.md`
- `docs/planned/MILESTONE_PRESET_BROWSER_STARTER_FLOW_EXECUTION_2026-05-31.md`
- `docs/planned/MILESTONE_WORKSPACE_PRESETS_2026-04-10.md`

---

## 1. 目的

`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2:

> - Project templates: 0 hit
> - Template gallery: 0 hit
> - Welcome screen: 12 hit (部分的)

`MILESTONE_AD_PRODUCTION_ACCELERATOR_2026-05-28.md` で 「広告制作のテンプレ量産」が目標だが、**テンプレート機能** の foundation がない。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/` 側に閉じる。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理

### 2.1 既存資産

- `ArtifactCreateCompositionDialog.cppm` — 新規 comp 作成 dialog
- `MILESTONE_AD_PRODUCTION_ACCELERATOR_2026-05-28.md` — 量産設計
- `MILESTONE_PRESET_BROWSER_STARTER_FLOW_EXECUTION_2026-05-31.md` — preset 起点
- `MILESTONE_WORKSPACE_PRESETS_2026-04-10.md` — workspace preset
- `ArtifactColorScienceManager.cppm` — preset 既存 (`M-LUT-1` で LUT library 化予定)

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Project template | 0 hit | comp + asset + param のセット保存なし |
| Template gallery | 0 hit | UI 不在 |
| Template versioning | 0 hit | テンプレ更新履歴なし |
| Variation binding | 0 hit | CSV / JSON 流し込み機能なし |

---

## 3. 設計の柱

### 3.1 ProjectTemplate データモデル

`ArtifactCore/src/Project/ProjectTemplate.ixx` を新規追加:

```cpp
namespace ArtifactCore {

struct ProjectTemplate {
    QString id;
    QString name;
    QString category;                  // "AD" / "Logo" / "Title" 等
    QString description;
    QString author;
    QStringList tags;

    // composition 定義
    QJsonObject compositionSnapshot;    // layer / prop / effect snapshot
    QStringList referencedAssets;       // 参照 asset の相対 path

    // variation binding (M-AD-1 由来)
    QList<SlotDefinition> slots;        // Scalar / Point / Color / Text / Enum

    // 関連ファイル
    QString thumbnailPath;              // preview 画像 (cache)
    QString manifestVersion;           // schema version

    // 永続化
    QJsonObject toJson() const;
    static ProjectTemplate fromJson(const QJsonObject& obj);

    // thumbnail 生成
    void generateThumbnail(const QString& outputPath);
};

} // namespace ArtifactCore
```

- 1 テンプレート = 1 composition snapshot + asset 参照 + slot 定義
- thumbnail は `<templateId>.png` として同 directory に

### 3.2 ArtifactTemplateService

`Artifact/src/Service/ArtifactTemplateService.cppm` を新規追加:

```cpp
class ArtifactTemplateService {
public:
    static ArtifactTemplateService& instance();

    // ロード
    QList<ProjectTemplate> allTemplates() const;
    QList<ProjectTemplate> templatesByCategory(const QString& cat) const;
    QList<ProjectTemplate> search(const QString& query) const;

    // 作成
    QString saveTemplate(const ProjectTemplate& tmpl);   // 新規 ID 採番

    // 適用
    QString instantiate(const QString& templateId,
                        const QString& projectRoot,
                        const QMap<QString, QVariant>& slotValues);

    // 削除
    bool removeTemplate(const QString& id);

    // 共有ディレクトリ
    QString defaultDirectory() const;
    void setDirectory(const QString& path);

signals:
    void templateAdded(const QString& id);
    void templateRemoved(const QString& id);
    void templateChanged(const QString& id);
};
```

- 起動時に `<appData>/templates/` を scan
- thumbnail cache も同 directory

### 3.3 Template Gallery UI

`Artifact/src/Widgets/Dialog/ArtifactTemplateGalleryDialog.cppm` を新規追加:

```cpp
class ArtifactTemplateGalleryDialog : public QDialog {
public:
    explicit ArtifactTemplateGalleryDialog(QWidget* parent = nullptr);

    void setCategoryFilter(const QString& cat);
    void setSearchQuery(const QString& q);

    // 選択
    QString selectedTemplateId() const;
    QMap<QString, QVariant> slotValues() const;

private:
    void rebuildUI();
};
```

- 1 ページ = 6 テンプレート (grid)
- カテゴリフィルタ + search
- 選択時に Slot 入力 dialog をポップアップ
- thumbnail クリックで preview

### 3.4 Welcome Screen 統合

既存 Welcome screen に Template Gallery への導線:

- "New from Template" ボタン
- Recent Projects
- 起動時の default 動作

### 3.5 Slot Definition

`SlotDefinition`:

```cpp
struct SlotDefinition {
    QString name;            // "Title" / "ProductName" 等
    QString label;           // UI label
    QString type;            // "Scalar" / "Point" / "Color" / "Text" / "Enum"
    QVariant defaultValue;
    QMap<QString, QString> enumOptions;  // "Type=Enum" 時
    QStringList tags;
};
```

- Text slot は `M-TXT-3 Source Text Keyframe` の layer と紐付け
- Color slot は color node のプロパティと紐付け
- Scalar は keyframe value と紐付け

### 3.6 Template 適用時の流れ

```
[Template 選択]
  → [Slot dialog]
    → user が slot 値を入力
      → instantiate()
        → composition snapshot を project root に展開
        → asset 参照を解決 (missing なら警告)
        → slot 値を layer property に注入
        → keyframe として保存
        → 新規 project として開く
```

### 3.7 AD Production Accelerator 統合

`MILESTONE_AD_PRODUCTION_ACCELERATOR_2026-05-28.md` の `TemplateSlot` と整合:

- 1 テンプレート = 1 ad campaign
- slot = 1 ad variant field (商品名 / 日付 / CTA 等)
- `instantiate()` で 1 variant project を作成

### 3.8 Project 保存

- `ArtifactTemplateService::defaultDirectory` は `FastSettingsStore` に保存
- 個別テンプレは `<appData>/templates/<id>/template.json`
- thumbnail は `<appData>/templates/<id>/thumbnail.png`

### 3.9 不変条件 (Guardrails)

- 既存 `ArtifactCreateCompositionDialog` の API は温存
- 新規 signal-slot 接続は `templateAdded / templateRemoved / templateChanged` 3 個に限定
- `QImage` / `setStyleSheet` 流入禁止
- thumbnail は `QImage` 経由で生成後、**PNG で書き出して破棄** (memory 解放)
- 既存 `MILESTONE_AD_PRODUCTION_ACCELERATOR_2026-05-28.md` の `TemplateSlot` と整合

### 3.10 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `template.asset-missing` (severity=warning, template 参照 asset が無い)
- `template.slot-invalid` (severity=info, slot 値未入力)
- `template.version-mismatch` (severity=info, schema version 不一致)

---

## 4. フェーズ計画

### Phase 1: Core data + ProjectTemplate (P0, 1 セッション)

- `ArtifactCore/src/Project/ProjectTemplate.ixx` 新規
- 永続化 (toJson / fromJson)
- thumbnail 生成 (PNG)

**Done criteria:**
- 1 template を保存 / 読み込み
- thumbnail 生成
- slot 定義の永続化

### Phase 2: ArtifactTemplateService (P0, 1〜2 セッション)

- service 実装
- scan / search / save / instantiate
- default directory 設定

**Done criteria:**
- `<appData>/templates/` を scan
- search で絞り込み
- instantiate で project 生成

### Phase 3: Template Gallery dialog (P0, 1〜2 セッション)

- grid 表示
- カテゴリ + search
- slot 入力 dialog

**Done criteria:**
- gallery 表示
- slot 入力 → project 生成
- 選択 template の thumbnail 表示

### Phase 4: Welcome Screen 統合 (P0, 1 セッション)

- 起動時の "New from Template" ボタン
- Recent Projects

**Done criteria:**
- Welcome screen から template gallery に遷移
- recent projects 表示

### Phase 5: AD Production 統合 (P1, 1 セッション)

- `MILESTONE_AD_PRODUCTION_ACCELERATOR` との統合
- CSV / JSON 流し込み

**Done criteria:**
- 1 template + 1 CSV で複数 variant 自動生成
- M-AD-1 と整合

### Phase 6: Project 保存 + Diagnostics (P1, 1 セッション)

- `defaultDirectory` を `FastSettingsStore` に保存
- Problem View への `template.*` 健全性 contribution

**Done criteria:**
- 設定永続化
- Problem View 表示

### Phase 7: Template versioning + sharing (P2, 別 milestone 推奨)

- template の更新履歴
- 共有 / クラウド
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_TEMPLATE_SHARING_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_AD_PRODUCTION_ACCELERATOR_2026-05-28.md` | 量産。本 milestone は foundation を提供。 |
| `MILESTONE_PRESET_BROWSER_STARTER_FLOW_EXECUTION_2026-05-31.md` | preset 起点。並走。 |
| `MILESTONE_WORKSPACE_PRESETS_2026-04-10.md` | workspace とは別概念。 |

---

## 6. リスクと未解決論点

### 6.1 実装リスク

1. **template 容量**。複数 template + thumbnail で disk 消費
2. **slot type 拡張**。新 type 追加時の互換性
3. **versioning**。schema version 不一致時の挙動
4. **AD production 統合**。CSV parse との接続点

### 6.2 設計未解決

- **template 共有**。クラウド / network share (Phase 7 以降)
- **template marketplace**。Adobe Stock 風 (長期)
- **template 派生**。既存 template からの派生 (Phase 7 以降)

### 6.3 サブモジュール境界

- `ArtifactCore/src/Project/ProjectTemplate.ixx` を新規追加
- `Artifact/src/Service/ArtifactTemplateService.cppm` を新規追加
- `ArtifactCore/CMakeLists.txt` に登録
- `ArtifactWidgets` 触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- ProjectTemplate の保存 / 読み込み
- 1 template から project 生成 (instantiate)
- Template Gallery dialog 表示
- Slot dialog 経由で値入力
- Welcome screen 統合
- AD production との CSV / JSON 統合
- 旧 project が default で動作
- Problem View に `template.*` 健全性表示
- 新規 `QImage` / `setStyleSheet` / signal-slot が増えていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2 を正式 milestone に起こした。