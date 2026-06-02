# Milestone: Timeline Overscroll Toggle Worknote

作成日: 2026-06-02
親: `MILESTONE_TIMELINE_ZOOM_PAN_2026-04-10.md`
関連: `MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md`

---

## 目的

タイムラインウィンドウの横スクロールについて、コンテンツ端での
`overscroll` 許可を On/Off できるようにするための作業メモ。

まずは「設定として保持できること」を優先し、挙動の細部は後から差し込める
ようにする。

---

## 仕様メモ

- `On`
  - コンテンツ端を少し越えてスクロールできる
  - 端での見切れや詰まり感を減らす
- `Off`
  - 既存どおりコンテンツ端で clamp する
  - 画面の移動量は常に内容境界に収める

---

## まず決めたいこと

1. 設定の置き場
   - アプリ全体設定にするか
   - タイムライン専用設定にするか
2. 初期値
   - `Off` をデフォルトにするか
   - `On` をデフォルトにするか
3. UI の入口
   - Timeline quick settings
   - Preferences
   - 右クリックメニュー

---

## 先にやらないこと

- 弾性スクロールのアニメーション表現
- タッチ/トラックパッド専用の別挙動
- スクロール以外のズームや再生ヘッド移動への波及

---

## 関連候補

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/docs/MILESTONE_TIMELINE_ZOOM_PAN_2026-04-10.md`

