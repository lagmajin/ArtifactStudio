# M-CBOX-1 Channel Box + Maya-Style Property Editor Milestone

作成日: 2026-07-07
ステータス: In Progress（Inspector内の基本Channel Boxを実装、操作拡張を継続）
**最終更新:** 2026-08-20
現行判定: 基本 Channel Box、Key All／Key Selected、Lock／Unlock、選択パスのTimeline／Graph Editor連携、レイヤー単位の選択復元まで実装済み。プロジェクト単位の専用永続化、Problem View診断、実機受入は未完了。
対象: `Artifact/src/Widgets/Inspector/ArtifactInspectorWidget.cppm`,
      `Artifact/src/Widgets/Property/ArtifactPropertyWidget.cppm`,
      `Artifact/src/Widgets/Property/ArtifactPropertyEditor.cppm`,
      `ArtifactCore/include/Animation/AnimatableValue.ixx`
位置づけ: Maya の Channel Box を ArtifactStudio に移植。Inspector を補完する
          コンパクトな数値直接編集 UI として、プロパティ選択→ドラッグスクラブ→キーイングを高速化する。
参照:
- `Artifact/src/Widgets/Inspector/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/Property/ArtifactPropertyEditor.cppm`
- `docs/WIDGET_MAP.md` (Inspector / PropertyEditor 責務)
- `docs/planned/MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md` (グラフエディタ強化)
- `docs/planned/MILESTONE_AUTO_ORIENT_2026-06-16.md` (Inspector 露出パターン)

### 2026-07-25 実装整理

- Inspector 上部に Transform／Opacity のコンパクトな Channel Box セクションを追加。
- 既存 Property Editor 行を再利用し、直接編集、スクラブ、キー表示、Auto-Key、Undo の経路を共有。
- 表示中の Transform／Opacity を一括キー化する `Key All` 操作を追加。
- フォーカス中のChannel Box行を対象にする `Key Selected` 操作を追加。
- 表示中チャンネルをまとめて Lock／Unlock し、ロック行を編集不可・暗色状態にする操作を追加。
- ロック状態をレイヤーID単位で `QSettings` に保存し、再表示時に復元するようにした。
- 明示的な複数チャンネル選択、Graph Editor／DopeSheet 絞り込み、プロジェクトJSONへの専用永続化は未完了。
- 2026-08-20: Property Editor行のowner-draw選択表示とCtrlクリックによる複数チャンネル選択を追加。選択パスをGraph Editor／Timelineマーカーへ伝播し、レイヤーID単位のQSettings復元を追加。

## Update 2026-08-15

Inspector内の基本Channel Box、Transform／Opacity行、直接編集・スクラブ・キー表示、Key All／Key Selected、Lock／Unlock、Undo、QSettingsによる復元を現行コードで確認した。残るのは明示的な複数チャンネル選択、Graph Editor／DopeSheet連携、プロジェクト単位の専用永続化、Problem View警告、runtime受入である。

---

## 1. 目的

Maya の Channel Box に相当する、コンパクトなプロパティ編集 UI を提供する。

現在の ArtifactStudio の Inspector はレイヤー/エフェクトの全プロパティを縦に並べるスタイルで、
以下のユースケースに弱い:

- 選択レイヤーの Transform だけを素早く数値編集したい
- 複数チャンネルを選択してまとめてキーを打ちたい（Key Selected）
- 値のドラッグスクラブで直感的に増減したい
- どのチャンネルにキーフレームがあるか一目で確認したい

Channel Box は Inspector の補完 UI として、
選択オブジェクトの主要チャンネル（Translate/Rotate/Scale/Visibility）を
コンパクトに一覧し、以下の Maya 風操作を提供する:

- チャンネル名クリックで選択 → 値フィールドを直接編集
- 値フィールドのドラッグスクラブ（仮想スライダー）
- 右クリックメニュー: Key Selected, Key All, Break Connections, Lock/Unlock
- キーフレーム有無の色分け表示（オレンジ=キーあり、グレー=キーなし）
- 選択チャンネルのみ Graph Editor / DopeSheet に表示

> 重要: これは Inspector の **置き換えではなく補完**。Inspector はエフェクトスタック等の
> リッチ編集用に残し、Channel Box は transform に特化した高速編集面とする。

---

## 2. 現状整理 (2026-07-07 基準)

### 2.1 既存資産

| 資産 | ファイル | 内容 |
|---|---|---|
| `ArtifactInspectorWidget` | `ArtifactInspectorWidget.cppm` | 選択レイヤー/エフェクトのプロパティ編集パネル |
| `ArtifactPropertyWidget` | `ArtifactPropertyWidget.cppm` | property row 編集 UI（pick-whip 入口含む） |
| `ArtifactPropertyEditor` | `ArtifactPropertyEditor.cppm` | property 編集の共通基盤 |
| `AnimatableValue<float>` | `ArtifactCore/include/Animation/AnimatableValue.ixx` | `hasKeyframeAt(RationalTime)` でキーの有無判定可能 |

### 2.2 不足（実装前のスナップショット）

以下の表は Channel Box 実装前の 2026-07-07 時点の記録であり、現行状態の判定には上記の 2026-08-15 監査を使用する。

| 軸 | 状況 | 影響 |
|---|---|---|
| Channel Box UI | なし | コンパクトなチャンネル一覧がない |
| ドラッグスクラブ | PropertyEditor 側で部分的 | Inspector 上の統一された操作がない |
| チャンネル色分け（キー有無） | なし | キーの有無を一目で判断できない |
| Key Selected / Key All | なし | 複数チャンネルへの一括キーイング不可 |
| Lock / Unlock channel | なし | チャンネル単位の編集ロック不可 |
| 選択チャンネルの Graph Editor 連携 | なし | 選択したチャンネルのみカーブ表示不可 |

---

## 3. Scope / Non-Goals

### Scope

- Channel Box ウィジェット（`ArtifactChannelBoxWidget`）
- チャンネル行の色分け（キーあり=オレンジ、キーなし=グレー、ロック=暗色）
- ドラッグスクラブによる値増減
- Key Selected / Key All / Break Connections の右クリックメニュー
- Lock / Unlock channel
- 選択チャンネルと Graph Editor / DopeSheet の連携
- Channel Box と Inspector のドッキング / タブ切替

### Non-Goals

- エフェクトプロパティの Channel Box 表示 → 将来拡張
- Expression / Set Driven Key のエディタ → 別 milestone
- Attribute Spreadsheet → 別 milestone
- 3D チャンネル（Rotate Order 等）→ 将来拡張

---

## 4. Phases

### Phase 1: Channel Box 基本 UI (P0, 2 セッション)

- `ArtifactChannelBoxWidget` を `QWidget` 派生で新規作成
- チャンネル行: 名前ラベル + 値エディット + 色分け表示
- 選択レイヤーの Transform チャンネル（Position.X/Y, Rotation, Scale.X/Y, Opacity）を表示
- 名前クリックで選択 / 値フィールドの直接編集

**Done criteria:**
- レイヤー選択で Channel Box が即更新
- 値フィールドの直接編集がレイヤーに反映
- キーフレームのあるチャンネルがオレンジ色で表示

### Phase 2: ドラッグスクラブ + 右クリックメニュー (P0, 1 セッション)

- 値フィールドのマウスドラッグによる値増減（仮想スライダー）
- Ctrl+ドラッグで微調整 / Shift+ドラッグで粗調整
- 右クリックメニュー: Key Selected, Key All, Break Connections, Lock, Unlock
- Lock 状態のチャンネルは値編集不可 / 暗色表示

**Done criteria:**
- 値フィールドのドラッグスクラブでリアルタイム値変更
- Key Selected で選択チャンネルのみキー挿入
- Lock されたチャンネルは編集不可 / 見た目で判別可能

### Phase 3: Inspector 統合 + Graph Editor 連携 (P1, 1 セッション)

- Inspector パネルに Channel Box タブ追加
- 選択チャンネルのみ Graph Editor に表示するフィルタ連携
- 選択チャンネルのみ DopeSheet に表示するフィルタ連携

**Done criteria:**
- Inspector と Channel Box をタブ切替可能
- Channel Box で選択したチャンネルだけ Graph Editor に表示

### Phase 4: 永続化 + Diagnostics (P2, 1 セッション)

- Channel Box の表示チャンネルプリセット保存（プロジェクト単位）
- Lock 状態の永続化（`layer.lockedChannels`）
- Problem View 診断（lock チャンネルのキー編集試行警告）

**Done criteria:**
- プロジェクト再読込で Channel Box 設定復元
- Lock チャンネルへのキー操作試行が Problem View に報告

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md` | Graph Editor フィルタ連携（Filter by Selected）の UI として機能 |
| `MILESTONE_AUTO_ORIENT_2026-06-16.md` | Inspector 露出パターンの参照 |
| `MILESTONE_AUTO_KEYING_GHOSTING_2026-07-07.md` | Auto-Keying + Keying Set の UI 土台 |
| `MILESTONE_ANIMATION_LAYERS_2026-07-07.md` | Anim Layer 選択に応じたチャンネル表示切替 |

---

## 6. リスク

1. **Inspector との責務重複**。Channel Box が Inspector の機能を重複実装しないよう、PropertyEditor 基盤を共有
2. **選択同期**。レイヤー選択 / Anim Layer 選択 / Channel Box 選択 / Graph Editor 選択の状態競合
3. **サブモジュール境界**: `Artifact` 側に閉じる。`ArtifactWidgets` は触らない

---

## 7. Done Criteria (全体)

- Channel Box が選択レイヤーの Transform チャンネルを表示
- キーフレーム有無の色分け / ドラッグスクラブ操作
- Key Selected / Key All / Lock / Unlock
- Graph Editor / DopeSheet との選択チャンネルフィルタ連携
- Inspector とのタブ切替統合
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot なし

---

## 8. 更新履歴

- 2026-07-07: 初版作成。Maya Channel Box の ArtifactStudio 移植設計。

## 2026-07-25 実装監査

- `ArtifactPropertyWidget` 内に Transform／Opacity の Channel Box セクションがあり、既存 Property Editor 行を再利用した直接編集・スクラブ・キー表示の経路を確認できる。
- `Key All`、フォーカス行への `Key Selected`、表示チャンネルの Lock／Unlock と編集不可表示、レイヤーID単位の QSettings 復元も実装されている。
- ただし独立 `ArtifactChannelBoxWidget`、明示的な複数チャンネル選択、Graph Editor／DopeSheet への selected-channel filter、プロジェクトJSONへの専用永続化、Problem View 診断は確認できない。
- よって Inspector 内の基本 Channel Box は実装済みだが、Phase 3／4 と全体 Done Criteria は未完了の In Progress 判定を維持する。
