# マイルストーン: ステータスバー 機能監査 (2026-07-04)

**最終更新:** 2026-08-15

## Update 2026-08-15

- `ArtifactStatusBar` は Zoom、Coordinates、Frame、FPS、Memory、Project、Layer、Dropped Frames、Timeline Debug、Console、Accessibility の item model と個別 setter、表示切替 context menu、accessibility 名を実装済み。
- AppMain から zoom／timeline debug／console 等の更新が接続され、Playback／diagnostics／menu の状態を status bar に反映する経路もある。単なる QStatusBar の空欄状態ではない。
- ただしズームクリックリセット、GPU 使用率／温度、現在ツール、quick settings、通知／error 集約、encoding／color space、選択数、最終 render 時間、自動保存状態の専用表示は未確認または未実装。Dropped Frames も表示 API と実データ配線の完全性は未確認。

> VS Code / Blender / Premiere / Photoshop ステータスバー比較。

| 機能 | 参照元 | 状態 |
|---|---|---|
| **フレーム/時間表示** | AE/Premiere | ⚠️ |
| **ズームレベル表示＋クリックでリセット** | VS Code/Photoshop | ❌ |
| **再生状態インジケーター** | Premiere | ⚠️ |
| **Dropped Frames 表示** | AE/Premiere | ⚠️ PlaybackInfoWidget にあるが配線未 |
| **メモリ使用量表示** | Premiere | ❌ |
| **GPU 使用率/温度** | - | ❌ |
| **現在のツール表示** | Photoshop | ❌ |
| **クイック設定（クリックで切替: Snap/Grid/Ruler）** | Blender/Premiere | ❌ |
| **通知/エラーインジケーター** | VS Code | ❌ |
| **エンコーディング/カラースペース表示** | VS Code | ❌ |
| **選択オブジェクト数表示** | Blender | ❌ |
| **最終レンダリング時間表示** | AE | ❌ |
| **自動保存インジケーター** | Premiere/VS Code | ❌ |
