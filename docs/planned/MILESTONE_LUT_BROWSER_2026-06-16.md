# M-LUT-1 LUT Browser / Picker Milestone

作成日: 2026-06-16
対象: `Artifact/src/Color/ArtifactColorScienceManager.cppm`,
      `Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm`,
      `ArtifactCore/include/Color/ColorLUT*`,
      `Artifact/src/Color/ColorNodeGraph*`,
      `Artifact/src/Effect/ArtifactColorLUT*`,
      `Artifact/src/Widgets/Color/ColorPicker/*`,
      `Artifact/src/Project/ArtifactProjectManager.cppm`
位置づけ: `.cube / .3dl` などの LUT ファイルを、color science panel / Inspector / Asset Browser から **一覧 → プレビュー → 適用 → 保存** まで一気通貫で触れるようにする。
参照:
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` (P2)
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` (#23 LUT Browser/Picker)
- `docs/analysis/DESIRED_IMPORT_FORMATS_2026-04-19.md` (`.cube / .3dl`)
- `docs/done/MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md`
- `docs/planned/MILESTONE_COLOR_GRADING_WORKSPACE_2026-03-30.md`
- `docs/planned/MILESTONE_COLOR_PICKER_ENHANCED_2026-04-10.md`
- `docs/planned/MILESTONE_APP_COLOR_CORRECTION_RACK_2026-05-18.md`
- `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md` (linear canonical)

---

## 1. 目的

LUT 読み込みの **足場** は既にある。

- `ArtifactColorScienceManager.cppm:144-150` でディレクトリ走査
- `ArtifactColorSciencePanel.cppm:34,159-212` で LUT load / clear
- `ColorLUTEffect` (`Artifact.Effects.ColorLUT`) で適用

しかし、AE / DaVinci 的な **LUT Browser** としては 3 点で不足している。

- **一覧性が弱い**: ディレクトリ走査はあるが、グリッド表示やサムネイル、絞り込み検索がない
- **プレビューが弱い**: 適用前に LUT をかけた見た目を確認できない
- **永続化と共有が弱い**: よく使う LUT を favorites / recent / project 添付として持てない

この milestone は「LUT を点として扱う」から「**LUT を library として扱う**」へ寄せる foundation。`/third_party/aces` 等の固定 preset を再設計する作業ではなく、**ユーザーが持ち込んだ LUT を editor の文脈で扱える UI** に絞る。

> 重要: `ArtifactCore` 側の LUT データ構造（LUTData / ColorLUTEffect）は **触らない**。UI 側（`Artifact/.../Color/` 配下）と `ArtifactProjectManager` の側だけで成立させる。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `ArtifactCore/include/Color/ColorLUT*` で `.cube / .3dl` の parse と 1D/3D LUT 表現は実装済み
- `Artifact/src/Color/ArtifactColorScienceManager.cppm:144-150` の `scanLutDirectory()`
- `Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm:34,159-212` の LUT load / clear
- `Artifact/src/Color/ColorNodeGraph*` の node-based color grading

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Browser UI | panel は 1 件ずつ load する導線。grid / list / 検索なし | 大量 LUT から選べない |
| Preview | panel 上に LUT 適用前後のプレビューなし | 適用してみるまで結果が分からない |
| Favorites / Recent | なし | 同じ LUT を毎回探す |
| Project 添付 | なし（外部 path のみ） | 別環境で開くとリンク切れ |
| Drag & Drop | なし | ファイルから直接追加できない |
| Inspector 統合 | なし。`ColorLUTEffect` のパラメータ picker が LUT browser を知らない | color correction workflow が分断 |
| `RenderFormatContract` 整合 | `.cube` の多くは sRGB domain 前提。linear canonical への明示的 transform 経路が薄い | 適用結果の hue / luma が想定外 |
| Diagnostics | LUT load 失敗時の reason / invalid file の警告経路が弱い | 壊れた LUT を見過ごしやすい |

### 2.3 既存 milestone との関係

- `MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md` は pipeline / OCIO / ACES の **下位層**。本 milestone はその上に乗る **UI 層** として棲み分け
- `MILESTONE_APP_COLOR_CORRECTION_RACK_2026-05-18.md` は Inspector の color correction rack。本 milestone は LUT library を提供し、rack 側がそれを読みに来る
- `MILESTONE_COLOR_PICKER_ENHANCED_2026-04-10.md` は color picker 自体。本 milestone は picker を **呼んで LUT を filter する** 補助導線

---

## 3. 設計の柱

### 3.1 LUT データの三状態

LUT は 3 つの状態で扱う。

- **External**: ファイル path を持つ参照のみ。ファイル本体は project に含めない
- **Embedded**: ファイル本体を project に埋め込む。`<= 256 KB` の LUT のみ
- **Pack**: `.cube` 単体、または複数 LUT をまとめた `.lutpack`（project 添付）

`Embedded` の閾値は **256 KB** とする。それ以上は `External` のまま。閾値は `ArtifactProjectManager` 設定で上書き可能にし、デフォルト 256 KB を `LUTBrowserConfig` に書く。

### 3.2 Browser UI

`ArtifactColorSciencePanel` 内に **`LutBrowserPane`** を追加する。

- 上部: 検索バー + filter chip (Favorite / Recent / Linear / sRGB / Custom)
- 中央: グリッド表示（1 セル = 1 LUT、サムネイル + name + size + tag）
- ホバー: 256×256 の LUT 適用前 / 適用後 split preview
- 選択: 詳細 panel（metadata + tag + comment + 最近使った project）
- 右クリック: `Apply to Selected Layer / Apply to Adjustment Layer / Set as Default / Add to Favorites / Reveal in Explorer / Copy Path / Embed in Project / Remove from Project`

### 3.3 Preview

新規 `LutPreviewWidget` を `Artifact/Widgets/Color/LutPreviewWidget.cppm` に追加。

- 入力: 1 枚の静止画像（panel 設定の source image、または選択中 layer の current frame）
- 表示: before / after split (drag で分割位置変更)
- LUT 変更は即時反映
- 16:9 / 1:1 / 9:16 の aspect chip を提供

### 3.4 Favorites / Recent

`LutBrowserConfig` に `favorites: QStringList` と `recent: QStringList` を持たせる。保存先は `FastSettingsStore` キー `lut/browser/v1`、容量上限は favorite 64 件 / recent 64 件。

### 3.5 Project 添付

`ArtifactProjectManager` の project JSON に `lut.libraries` セクションを追加:

```json
{
  "lut": {
    "libraries": [
      {
        "id": "lib_001",
        "name": "Vintage Film Pack",
        "luts": [
          { "id": "lut_001", "name": "Kodak 2383", "kind": "embedded", "size": 4096, "dataBase64": "..." },
          { "id": "lut_002", "name": "Fuji F125", "kind": "external", "path": "/abs/path/f125.cube" }
        ]
      }
    ]
  }
}
```

- 復元時に external path が見つからない場合は `severity=warning` で Problem View に上げる
- external path を再リンクする導線を panel 内に置く

### 3.6 Inspector 統合

`ArtifactInspectorWidget` の effect stack に `ColorLUTEffect` があるとき、LUT file picker を開くボタンから `LutBrowserPane` を modal で起動する。選択結果は `ColorLUTEffect::setLutId(lutId)` で保存。

### 3.7 `RENDER_FORMAT_CONTRACT` 整合

`.cube` の domain tag（`TITLE` / `DOMAIN_MIN` / `DOMAIN_MAX`）を `LutLoader` が parse し、`RENDER_FORMAT_CONTRACT_2026-05-16.md` の linear canonical に揃える。

- domain が sRGB の場合: `sRGB → linear` decode を明示パスとして通す
- domain が linear の場合: そのまま
- domain が未指定: 警告ログ + linear として扱う

これは `ArtifactCore/src/Color/ColorLUT*` 側の責務に見えるが、本 milestone のスコープでは `LutLoader` (UI 側 wrapper) で **明示的な decode 経路** を持つ。Core 側の変更が必要なら別 milestone として切り出す。

### 3.8 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` に以下を contribution:

- `lut.missing-path` (severity=warning)
- `lut.oversize` (severity=info, 256 KB 超)
- `lut.invalid-cube-header` (severity=error)
- `lut.duplicate-name` (severity=info)

### 3.9 Drag & Drop

`ArtifactColorSciencePanel` / Asset Browser / Project View に **LUT ファイル** の drop を受け入れる。

- `.cube / .3dl` の場合は `LutLoader` で parse して library に追加
- `.lutpack` の場合は pack を展開して library に追加
- それ以外の拡張子は `severity=info` で silent drop

### 3.10 不変条件 (Guardrails)

- LUT parse は **すべて `ArtifactCore` 側の既存 parser** を使う。`Artifact/Color/` 配下に独自 parser を書かない
- プレビュー用画像は `QImage` を **新規 hot path に入れない**。LUT preview は GPU 側で完結させ、CPU への download は export 時に限定
- `setStyleSheet` / 新規 signal-slot / 新規 `QImage` 追加の禁止ルールは維持
- LUT library 1 個あたりの LUT 数は 256 件以下
- external path は絶対 path / 相対 path 両対応。relative 解決は `ArtifactProjectManager::projectRoot()` 起点
- project 添付は **オプトイン**。既定は external のまま

---

## 4. フェーズ計画

### Phase 1: Browser pane + grid 表示 (P0, 1〜2 セッション)

- `LutBrowserPane` を `ArtifactColorSciencePanel` 配下に追加
- 検索バー + grid 表示
- `LutLoader` を作って `scanLutDirectory()` を置き換え（既存 API は温存）

**Done criteria:**
- 指定 directory の `.cube / .3dl` が grid に並ぶ
- 検索バーで絞り込める
- 選択すると詳細 panel に metadata が出る

### Phase 2: Preview + apply (P0, 2 セッション)

- `LutPreviewWidget` 追加
- before / after split
- 選択 LUT を `ColorLUTEffect` に即時適用 / 解除

**Done criteria:**
- preview で適用結果を確認できる
- 選択 LUT を適用すると Inspector の effect パラメータが更新される
- 適用 / 解除が Undo で戻る

### Phase 3: Favorites / Recent / Project 添付 (P0, 1〜2 セッション)

- `LutBrowserConfig` 追加（`FastSettingsStore` 連携）
- project JSON に `lut.libraries` セクション
- 256 KB 閾値で external / embedded を切替

**Done criteria:**
- favorite / recent が保存 / 復元される
- project 保存 → 再読込で library が復元
- 256 KB 超は external のまま、それ以下は embedded

### Phase 4: Drag & Drop + Inspector picker (P1, 1 セッション)

- panel / Asset Browser / Project View への drop
- Inspector の `ColorLUTEffect` パラメータから `LutBrowserPane` を modal 起動

**Done criteria:**
- `.cube` を drop すると library に追加される
- Inspector から picker を開ける

### Phase 5: Domain tag 整合 + Diagnostics (P1, 1 セッション)

- `.cube` の `DOMAIN_MIN / DOMAIN_MAX` を linear canonical に揃える明示パスを追加
- Problem View に `lut.missing-path` などの健全性 contribution

**Done criteria:**
- sRGB domain の LUT が linear 経由で適用され、想定外の色味にならない
- Problem View に LUT 健全性が表示される

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md` | 下位 pipeline。本 milestone は UI 層。Core 側の LUT 構造は触らない。 |
| `MILESTONE_APP_COLOR_CORRECTION_RACK_2026-05-18.md` | 上位 rack。本 milestone は LUT library を提供。rack 側が参照する。 |
| `MILESTONE_COLOR_GRADING_WORKSPACE_2026-03-30.md` | workspace。本 milestone の panel は workspace の 1 セクション。 |
| `MILESTONE_COLOR_PICKER_ENHANCED_2026-04-10.md` | picker。本 milestone は picker から呼ばれる補助導線。 |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | diagnostics 文法。本 milestone Phase 5 が contribution。 |
| `MILESTONE_BLEND_MODE_DESIGN_2026-06-16.md` | 別 topic。 |
| `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md` | linear canonical の参照。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **`.cube` domain 整合**。Core 側に `LutLoader` がない場合は UI 側に置けるが、Core 側に置く方が筋。Phase 5 で「Core に置く / UI に置く」を決定する
2. **Embedded size**。256 KB は経験則。`.3dl` の中には数 MB の巨大 LUT もある。閾値調整 UI を Phase 3 で設ける
3. **External path 解決**。別 OS / 別環境での path 解決失敗の warning をどう UX で見せるか
4. **GPU プレビュー**。LUT preview を GPU 経由にする場合、layer blend 経路との衝突を避ける。Phase 2 で `LutPreviewWidget` を新規 widget として独立させ、render path には触らない

### 6.2 契約上の未解決

- **OCIO 連携**。`MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md` の OCIO 側と、本 milestone の `.cube / .3dl` 側の bridge。Phase 5 以降で別途設計
- **HDR LUT**。Rec.2020 / Rec.2100 の LUT は本 milestone のスコープ外
- **Pack フォーマット**。`.lutpack` は本 milestone で新設するか、既存の `.zip` を使い回すか Phase 3 で決定
- **Undo 粒度**。library への追加は Undo 対象、recent への記録は Undo 対象外、favorites は別

### 6.3 サブモジュール境界

- `ArtifactCore/include/Color/ColorLUT*` は **触らない**
- `ArtifactWidgets` は触らない
- `LutBrowserPane` / `LutPreviewWidget` は `Artifact/src/Widgets/Color/` 配下に新規追加
- `LutBrowserConfig` は `Artifact/src/Color/` 配下に追加（既存 `ArtifactColorScienceManager` と並走）

---

## 7. Done Criteria (全体)

- 指定 directory / drop / project 添付の 3 経路で LUT を library に入れられる
- 検索 / favorite / recent で絞り込める
- before / after split preview で適用結果を確認できる
- Inspector の `ColorLUTEffect` から picker を開ける
- project 保存 → 再読込で library と favorites / recent が復元される
- sRGB domain の LUT が linear canonical 経由で適用される
- Problem View に LUT 健全性が表示される
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot が増えていない
- `ArtifactCore` への bump が発生しない（UI 側のみで完結）

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md` との分業を明示。

## 2026-07-25 実装監査

ArtifactColorScienceManager の LUT scan／load／clear／metadata/error API、ColorSciencePanel の LUT Browser group、再スキャン、利用可能 LUT の一覧、選択プレビュー、Load/Clear、Drag & Drop の入口、FinalPostProcess の GPU LUT upload は実装を確認した。一方、専用 grid／検索・filter、before/after split preview、favorites／recent の永続化、project embedded/pack、Inspector picker、domain tag の linear canonical 整合、Problem View 診断は確認できない。したがって Phase 1 の基礎導線は部分実装、Phase 2〜5 と全体 Done Criteria は未完了・runtime未検証とする。
