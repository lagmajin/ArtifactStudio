**最終更新:** 2026-08-06

**ステータス:** In Progress

# 業務メディア基盤マイルストーン

業務映像制作に必要な時間、字幕、音声、色管理、メディア運用、納品機能を、既存のArtifactCore/Artifact基盤へ段階的に統合する。

## 実装順

### M1: フレーム精度の時間基盤（進行中）

- composition、media、audio、exportで共通のrational time baseを使う
- 29.97/59.94を29/59へ切り捨てない
- TimeCodeのH:M:S:F変換とフレーム番号変換を一致させる
- drop-frameのSMPTEスキップ規則を追加する
- JSON保存、再読込、UI表示、スクラブの往復精度を検証する

既存の`FrameRate`、`TimeCode`、`TimeCodeRange`、`RationalTime`を拡張し、新しい時間型を乱立させない。

### M2: 字幕・キャプション

- 既存の`NLE::SubtitleCue`をタイムライン上の字幕トラックへ接続
- SRT import/exportを維持し、drop-frame time baseとの変換を統一
- 字幕、キャプション、クローズドキャプションの表示・書出し責務を分離

### M3: メディア運用

- proxy、relink、conformの状態モデルを統一
- 欠損メディア検出、候補提示、再リンク結果の保存
- 自動保存、クラッシュ復旧、復旧世代の選択

### M4: 音声ポスト基盤

- マルチチャンネル音声と明示的なチャンネルマッピング
- 5.1/7.1を含むモニタリング・downmix
- BS.1770系の測定、ゲート、True Peak、LUFS正規化

### M5: 色管理・放送セーフ

- OCIOをpreview/export境界のsource of truthへ接続
- 10bit、Rec.709、Rec.2020、HLG、PQの入力・出力メタデータ
- 放送セーフな輝度・色域の検査と補正方針

### M6: 業務codec・納品

- MXF、XDCAM、DNx、ProResの入出力プロファイル
- 規格別書き出しプリセット
- QCレポートと納品メタデータ

FFmpegの汎用codec名判定だけで業務納品対応とみなさず、コンテナ、音声構成、色、timecode、メタデータをプロファイル単位で検証する。

### M7: 外部I/O・共同編集

- SDIは外部SDK・ボード依存のadapterとして分離
- 共有ストレージ、ロック、競合解決、複数人編集履歴

## 今回の実装

`TimeCode`が29.97/59.94を整数へ切り捨てていたため、H:M:S:F変換でフレーム単位のずれが発生し得た。ノンドロップの公称fps変換を丸め処理へ修正した。

drop-frameのスキップ規則、字幕UI、業務codecプロファイルはM1の検証項目を確定した後に実装する。

## 検証方針

- ビルド・テスト実行時に23.976、24、25、29.97、50、59.94を往復変換する
- 29.97/59.94のdrop-frame境界（分頭、10分境界、時境界）を確認する
- composition frame、media frame、字幕cue、export metadataの同一フレーム性を確認する
- 実装前に既存の時間APIを再利用できるか確認し、モジュール循環を増やさない
