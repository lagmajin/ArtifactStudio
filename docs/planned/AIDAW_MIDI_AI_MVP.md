# AIDAW MIDI AI MVP

**最終更新:** 2026-09-09
**状態:** 設計中

## 目的

AIDAWを、自然言語からMIDI素材を提案できるクラウドAI中心の作曲アシスタントとして開始する。
最初から自動作曲を完成させるのではなく、ユーザーがAIの提案を確認し、採用・破棄・再生成できる編集体験を優先する。

## MVPの利用例

- 「Cマイナー、120 BPM、8小節のLo-fiベースを提案」
- 「このコード進行に合うメロディを提案」
- 「現在のメロディに対して、よりシンプルな別案を3つ作る」

## 既存ArtifactStudioから再利用する部分

### AI通信

- `Artifact/include/AI/AIClient.ixx`
- `Artifact/src/AI/AIClient.cppm`
- `Artifact/include/AI/Cloud/AICloudSessionController.ixx`
- `Artifact/include/AI/Cloud/AICloudWorkerProtocol.ixx`

既存の `AIClient` と `AICloudSessionController` を通信基盤として利用する。プロバイダ固有のHTTP処理をMIDI機能側へ複製しない。

### 将来の操作連携

- `ToolBridge`
- `WorkspaceAutomation`
- 既存のAI Cloud Widgetのプロバイダ／キー設定

MVPではAIが直接プロジェクトを変更せず、提案を返してユーザーが採用する。採用操作のUndo経路が定まった後に、ToolBridge経由の適用操作を追加する。

## 構成

```text
ユーザープロンプト
      |
      v
MidiSuggestionService
      |
      +-- AIClient / AICloudSessionController
      |
      v
構造化MIDI提案(JSON)
      |
      v
MidiProposalParser + MidiProposalValidator
      |
      v
MidiProposalModel
      |
      +-- プレビュー
      +-- 採用 / 破棄 / 再生成
      +-- Undo付きプロジェクト適用
```

## データモデル案

AI応答はMIDIバイナリではなく、検証可能なJSONに限定する。

```json
{
  "schemaVersion": 1,
  "tempo": 120,
  "timeSignature": [4, 4],
  "key": "C minor",
  "lengthBeats": 32,
  "tracks": [
    {
      "name": "Bass",
      "channel": 0,
      "notes": [
        { "pitch": 36, "startBeat": 0, "durationBeats": 1, "velocity": 90 }
      ]
    }
  ]
}
```

想定する責務は以下の通り。

- `MidiNote`: pitch、開始位置、長さ、velocity
- `MidiTrack`: トラック名、MIDI channel、ノート列
- `MidiProposal`: tempo、拍子、キー、長さ、トラック列、生成メタデータ
- `MidiProposalParser`: JSONからの変換
- `MidiProposalValidator`: 音域、時間、velocity、ノート数、重複、サイズ制限の検証

## AIプロンプト契約

system promptで以下を要求する。

- JSONオブジェクトのみを返す
- Markdown code fenceを使わない
- `schemaVersion`を必ず含める
- 不明な条件は勝手にプロジェクトを書き換えず、提案内容に明示する
- 指定された音域、拍子、長さを守る
- ノート数上限を守る

モデル出力は信頼せず、アプリ側のバリデーションを通過したものだけを表示・採用する。

## UIの最小範囲

1. MIDI AIパネル
2. プロンプト入力欄
3. Generate / Cancel
4. 提案トラックとノートの概要表示
5. Preview / Accept / Discard / Regenerate
6. エラーと検証警告の表示

本格的なピアノロールはMIDIモデルと採用処理が固まった後に追加する。既存のTimelineへ先に無理に統合しない。

## 実装フェーズ

### Phase 1: 契約とモデル

- MIDI提案モデルの追加
- JSON parser / validator
- 固定JSONを使う静的な変換経路

### Phase 2: クラウド接続

- `AIClient`を利用した提案リクエスト
- MIDI専用system prompt
- 非同期生成、キャンセル、エラー表示

### Phase 3: 提案UI

- 提案一覧とノート概要
- 採用・破棄・再生成
- 生成履歴のセッション内保持

### Phase 4: プロジェクト適用

- MIDIトラックへの適用
- Undo/Redo
- MIDI書き出し
- 保存・再読込

## 制約と方針

- クラウドAPIキーをプロジェクトファイルへ保存しない。
- AIの応答をそのまま実行しない。
- 採用前の提案はプロジェクト状態を変更しない。
- MIDI処理は既存の映像レイヤー責務へ混ぜず、音楽／MIDIの専用モデルとして分離する。
- 既存ArtifactStudioのAI通信コードを大規模改変しない。
- ビルド、CMake生成、テスト実行はユーザーの明示指示があるまで行わない。

## 未決定事項

- 初期プロバイダとモデルの既定値
- OpenAI互換APIを共通境界にするか
- MIDIファイル入出力ライブラリの選定
- AIDAW独自のプロジェクト／トラックモデル
- コード進行、スケール、ベロシティ等の音楽制約の表現
