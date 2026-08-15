# Milestone: Proxy Workflow (2026-03-29)

**最終更新:** 2026-08-15
**Status:** Proxy 管理・品質選択・生成 API は実装済み、編集／render 切替と backend／runtime parity は未完了
**Goal:** 重いフッテージの低解像度プロキシを作成・使用して編集パフォーマンスを向上。
レンダリング時にフル解像度に自動切り替え。

---

## コンセプト

```
高解像度フッテージ (4K, 200MB)
  → プロキシ生成 (480p, 5MB)
  → 編集中はプロキシを使用（軽い）
  → レンダリング時にフル解像度に自動切り替え
```

---

## 現状

| 機能 | 状態 |
|------|------|
| フッテージの直接読み込み | ✅ 完成 |
| プロキシ生成 | ❌ 未実装 |
| プロキシ切り替え | ❌ 未実装 |
| 自動プロキシ生成 | ❌ 未実装 |

---

## Implementation

### 1. ProxyGenerator
- 入力: 高解像度ビデオ/画像
- 出力: 低解像度プロキシ (MP4, JPEG, PNG)
- プリセット: 1/4, 1/2, 1/8 解像度
- バッチ処理対応

### 2. Proxy メタデータ
```cpp
struct ProxyInfo {
    bool hasProxy = false;
    QString proxyPath;
    int proxyWidth, proxyHeight;
    float scaleFactor = 1.0f;
    bool useProxy = true;  // 編集中の使用フラグ
};
```

### 3. フッテージ切り替え
- 編集中: プロキシを使用
- レンダリング時: フル解像度に自動切り替え
- ビューポートの品質ドロップダウン: "Auto / Proxy / Full"

---

## 見積

| タスク | 見積 |
|--------|------|
| ProxyGenerator (FFmpeg ベース) | 3h |
| プロキシメタデータ管理 | 2h |
| レイヤーへのプロキシ切替ロジック | 2h |
| UI (品質ドロップダウン) | 1h |

**総見積: ~8h**

## Static Audit (2026-07-25)

現行実装は計画時の「Not Started」から大きく進展している。`ProxyService` に None／Half／Quarter／Eighth の品質 enum、ProxyInfo、生成・削除・存在確認・情報取得・batch API があり、VideoLayer に proxy path／quality／serialize／clear の状態がある。Project Manager には品質選択付き生成、単体／複数生成、stale 再生成、Reveal、Clear、Global Proxy toggle、queue progress、Ready／Stale／Missing 表示が実装され、Inspector にも proxy 状態表示がある。

ただし、ProxyService の実装本体や FFmpeg／画像変換の実動作、編集時 proxy 使用と render 時 full 解像度への自動切替、Auto／Proxy／Full の統一 viewport UI、生成失敗・キャンセル・再開・大量 batch の進捗／エラー処理、proxy metadata の永続化と runtime round-trip は静的検索だけでは確認できない。Proxy path を VideoLayer に同期する導線はあるが、Project Manager 内の簡易 queue と共通 ProxyService の責務が完全に一元化されているとは言い切れない。

判定: **Proxy 管理 UI と layer 状態の主要基盤は実装済み。** 本格生成 backend、render 時切替、永続化、失敗経路、runtime 検証が残っている。

## 現行コード監査 (2026-08-15)

- `AssetConverter::generateProxy()`、`AssetMetaFile` の proxy 解像度・path metadata、`AssetImportSetting::generateProxyOnImport` が存在し、ProxyService／Project Manager／Timeline layer menu から品質選択、生成、reveal、clear、batch の導線がある。
- `ArtifactVideoLayer` は proxy path／quality を保持し、serialized property として扱う。NLE core にも source proxy の availability／online／useProxy 状態があるため、単純な「未実装」ではなく管理モデルは進展している。
- ただし、FFmpeg／画像変換の実生成が全 source 種別で動くこと、生成失敗・cancel・再開・stale 検出の受入れ、preview／playback が実際に proxy を選択することは静的コードだけでは確定できない。
- render／export 時に full-resolution source へ自動復帰する共通スコープ、Auto／Proxy／Full の一貫した viewport 設定、proxy metadata の project round-trip、巨大 batch の進捗・エラー集約も未検証。

判定: **proxy のデータモデル、生成／管理 UI、品質選択の基盤は実装済み。実際の decode 切替、render 時の full 解像度復帰、生成 backend の安定性、保存・runtime parity は pending。**

## Update 2026-08-15

- `AssetConverter::generateProxy()`、`AssetMetaFile` の proxy metadata、import 時の生成設定、ProxyService／Project Manager／Timeline menu の品質選択・生成・reveal・clear・batch 導線を再確認。
- `ArtifactVideoLayer` の proxy path／quality／serialization と NLE source proxy の availability／online／useProxy 状態も確認できる。
- preview／playback の実 decode 切替、render／export 時の full-resolution 復帰、全 source 種別の生成安定性、cancel／resume／stale、metadata round-trip、Auto／Proxy／Full の runtime parity は未完了・未検証。
