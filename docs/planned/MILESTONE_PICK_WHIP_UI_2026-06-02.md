# M-MOTION-4 Pick-Whip UI for Property Linking (2026-06-02)

日付：2026-06-02
目標：プロパティリンクの視覚的ドラッグ UI（Pick-Whip）を実装し、コード上のリンク機能を直感的な操作で使えるようにする。

---

## Goal

- プロパティ名横のアイコンをドラッグして、別レイヤーのプロパティにドロップすることでリンクを作成
- リンク後は Inspector 上でリンク先・リンクタイプを確認・編集可能
- `PropertyLinkManager` の既存機能と統合

---

## Definition of Done

- [ ] **Pick-Whip アイコン** - プロパティ名の右側に `◎` アイコンを表示
- [ ] **ドラッグ操作** - アイコンをドラッグするとワイヤー（曲線）がマウスを追従
- [ ] **ドロップ** - 別レイヤーのプロパティ名上でドロップ → リンク作成
- [ ] **リンク種別選択** - ドロップ後に Direct/Inverse/Scale/Offset を選択するポップアップ
- [ ] **リンク解除** - リンク済みプロパティのアイコンを右クリック → 「リンク解除」
- [ ] **Inspector 表示** - リンク中プロパティの値表示にリンクインジケータ表示
- [ ] **無効ドロップ** - 互換性のない型（Float ↔ Color など）の場合はドロップ拒否

---

## Design Reference

After Effects の Pick-Whip UI を参考に、以下の要素を実装する：
- 渦巻きアイコンではなく `◎`（target icon）
- ドラッグ中のワイヤーはベジェ曲線で滑らかに
- リンク先プロパティはハイライト表示

---

## Implementation Phases

### Phase 1: ドラッグ＆ワイヤー描画

**完了条件**:
- [ ] `ArtifactPropertyWidget` のプロパティ行に Pick-Whip アイコンボタンを追加
- [ ] アイコンのマウスプレスでドラッグ開始
- [ ] ドラッグ中は Composition Editor / Inspector 全体にワイヤー (QPainterPath ベジェ) をオーバーレイ描画
- [ ] マウスカーソルがホバー中の有効ターゲットをハイライト

### Phase 2: リンク作成・編集UI

**完了条件**:
- [ ] ドロップ時のリンクタイプ選択ポップアップ
- [ ] `PropertyLinkManager::addLink()` を呼び出してリンク作成
- [ ] リンク済みプロパティの表示更新（インジケータ＋値表示のグレーアウト）
- [ ] リンク解除UI
- [ ] リンク一覧の確認パネル（デバッグ用）

### Phase 3: 型チェックと互換性

**完了条件**:
- [ ] 同一型のみリンク許可（Float↔Float, Color↔Color...）
- [ ] リンク拒否時のフィードバック表示

---

## Dependencies

- ArtifactPropertyWidget (プロパティ行 UI)
- PropertyLinkManager (リンクコア)
- ArtifactCompositionEditor / Inspector (オーバーレイ描画)

---

## Total Estimate

| Phase | 時間 |
|---|---|
| Phase 1: ドラッグ＆ワイヤー描画 | 6-10h |
| Phase 2: リンク作成・編集UI | 4-6h |
| Phase 3: 型チェック | 2-3h |
| **合計** | **12-19h** |

## 2026-07-25 実装監査

PropertyLinkManager／property binding・expression の Core 基盤は確認できるが、Pick-Whip 用のプロパティ行アイコン、ドラッグ中のベジェワイヤー、ターゲット hover、ドロップ時の Direct／Inverse／Scale／Offset 選択、リンク解除・インジケータ、型互換性拒否 UI は確認できない。したがってリンクコアは部分的に存在するものの、M-MOTION-4 の視覚的操作導線は未実装・未検証とする。
