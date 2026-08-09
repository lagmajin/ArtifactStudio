**最終更新:** 2026-08-09

# QADSにおけるAEスタイルフォーカスフレームの実装監査レポート

## 1. 概要・結論

本レポートは、Qt Advanced Docking System（QADS）で、Adobe After Effects（AE）のようにアクティブな Dock パネルを青いフレームで強調する実装を監査したものです。実装済みのコードと、当初検討された未採用案を分離して記録します。

### 結論

1. 自作ドッキングシステムは不要です。QADS のドラッグ、浮動化、Splitter、レイアウト保存・復元をそのまま利用します。
2. 現在の実装は `DockStyleManager` と `DockGlowStyle` に集約されています。
3. `QEvent::Paint` を捕捉して `CDockAreaWidget::event()` を手動実行する方式や、透明な描画用オーバーレイは採用しません。
4. 現行の枠は単一の `QPainterPath` ではなく、QProxyStyle がタブとコンテンツの隣接矩形を描くことで、視覚的に連続した raised-tab silhouette を構成しています。

## 2. 現行実装の責務分担

| 役割 | 実装 | 内容 |
|---|---|---|
| フォーカス状態の追跡 | `Artifact/src/Widgets/Dock/DockStyleManager.cppm` | QADS の `focusedDockWidgetChanged`、`QApplication::focusChanged`、Dock 関連イベントを監視 |
| 状態の反映 | `DockStyleManager::refreshDockDecorations()` | `artifactActiveDock` / `artifactActiveTab` プロパティを更新し、対象を再ポリッシュ |
| 枠の描画 | `Artifact/src/Widgets/Dock/DockGlowStyle.cppm` | `QProxyStyle::drawControl`、`drawPrimitive`、`drawComplexControl` の標準描画後に装飾を追加 |
| コンテンツ枠 | `DockGlowStyle::drawDockWidgetGlow()` | アクティブタブ部分を上辺から除外し、左右・下辺・残りの上辺を描画 |
| タブ枠 | `DockGlowStyle::drawDockTabGlow()` | タブの上・左・右辺を描画し、下辺を背景色で消して継ぎ目を抑制 |

`DockStyleManager` のイベントフィルターは描画フックではありません。フォーカスやクリックを受けて遅延リフレッシュを予約する状態更新用です。

## 3. 当初案から修正すべき点

### 3.1 `QEvent::Paint` 内で `dockArea->event(event)` を呼ぶ案は不採用

イベントフィルターは対象ウィジェットの通常イベント配送より前に呼ばれます。その中で対象の `event()` を手動実行してから `QPainter` で描画し、さらに元のイベントを止める設計は、再入・二重描画・描画順序の契約を曖昧にします。

したがって、QADS の描画を横取りするのではなく、既存の `QProxyStyle` の描画フックを使います。標準描画後に装飾を追加でき、タブや Dock のマウスイベントもそのまま QADS に届きます。

### 3.2 透明オーバーレイ案は不要

`WA_TransparentForMouseEvents` を設定したオーバーレイはイベント横取りを避けられますが、QADS の浮動化、再ドック、DPI 変更、Z オーダー、リサイズ後の追従を別途管理する必要があります。現在の `DockGlowStyle` では既存の QADS ウィジェット自身に描画されるため、この管理コストがありません。

### 3.3 「完璧な一筆書き」という表現は撤回

現行実装は単一の `QPainterPath` を構築していません。Dock Widget の上辺にあるアクティブタブ領域を空け、タブの下辺を背景色で塗ることで、不要な境界線を消しています。

これは単一パスと同じ見た目を狙った実用的な構成ですが、数学的に一つの閉じたパスであることや、すべての QADS レイアウト形状で完全な連続線になることは保証しません。

## 4. QADS の座標と描画契約

`DockGlowStyle::drawDockWidgetGlow()` は、アクティブな `CDockWidgetTab` の矩形を Dock Widget 座標へ写像して使用します。これにより、タブの位置が左端・中央・右端のいずれでも、コンテンツ枠の上辺に不要な線を描かない構成を取れます。

描画の基本契約は以下です。

```text
QADS 標準描画
    ↓
DockGlowStyle::drawControl / drawPrimitive / drawComplexControl
    ↓
アクティブ Dock / Tab の属性を確認
    ↓
隣接矩形の枠を追加描画
```

QADS の組み込み stylesheet を空にする処理は、`QPalette` と `QProxyStyle` の描画を優先するための既存の例外です。新しい QSS の見た目定義を追加するものではありません。

## 5. 自作ドッキングシステムとの比較

| 評価項目 | QADS + `DockStyleManager` / `DockGlowStyle` | 完全自作 |
|---|---|---|
| 開発コスト | 既存実装を拡張 | 非常に高い |
| ドラッグ＆ドロップ | QADS のドックガイドを利用 | 全面実装が必要 |
| 浮動化・再ドック | QADS のライフサイクルを利用 | 独自管理が必要 |
| レイアウト保存・復元 | `saveState` / `restoreState` を利用 | 独自シリアライズが必要 |
| マウスイベント | 描画用オーバーレイなし | 実装次第 |
| フォーカス枠 | QProxyStyle で実装済み | 自由だが検証範囲が大きい |

## 6. 現在の実装状態

- `DockStyleManager` によるアクティブ Dock / Tab の状態管理は実装済みです。
- `DockGlowStyle` による Dock 枠とタブ枠の装飾は実装済みです。
- 描画専用の `CDockAreaWidget` イベントフィルターは不要です。
- 透明なフォーカス枠オーバーレイは不要です。
- 単一 `QPainterPath` による厳密な一筆書きは未実装であり、現行仕様上の必須条件ではありません。

## 7. 実機で確認する項目

ビルド・実機確認が必要な項目は次の通りです。

1. Dock の左端・中央・右端タブで、タブ下辺の不要な線が出ないこと。
2. タブ切り替え直後に旧 Dock の枠が残らず、新 Dock に枠が移ること。
3. QADS の floating 化、再ドック、レイアウト復元後も枠の位置が一致すること。
4. DPI 変更、ウィンドウリサイズ、最小化復帰後に枠がずれないこと。
5. Dock 内のクリック、タブ切り替え、ドラッグ操作が通常どおり通ること。
