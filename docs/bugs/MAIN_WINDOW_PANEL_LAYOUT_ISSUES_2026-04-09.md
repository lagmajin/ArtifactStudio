# Main Window Panel Layout Issues (2026-04-09)

現在のメインウィンドウ周りで、レイアウトの余白配分に関する気になる点を 2 件まとめたメモ。

対象の主戦場は `Artifact/src/Widgets/ArtifactMainWindow.cppm` と、その中央に載る各 dock / central panel。

---

## Issue 1: セントラルウィジェット側のパネル幅が小さい

### 症状

中央領域に表示されるパネルの横幅が足りず、Project View / Asset Browser / Inspector の読み取り性が落ちる。

### 観点

- dock の初期幅が狭い
- 中央領域が必要以上に圧縮されている
- selection summary や status chip が横方向に窮屈

### 優先度

- 中

### 関連候補

- `ArtifactMainWindow.cppm`
- `ArtifactProjectManagerWidget.cppm`
- `ArtifactAssetBrowser.cppm`
- `ArtifactInspectorWidget.cppm`

### 備考

`ArtifactProjectManagerWidget` と `ArtifactAssetBrowser` の両方で、横幅がもう少しあると選択情報や状態チップが読みやすい。

---

## Issue 2: 下のパネルの高さが足りない

### 症状

下側に配置されるパネルの高さが不足し、内容が詰まって見える。

### 観点

- bottom dock の初期高さが低い
- project / timeline / audio mixer / render queue のいずれかが短く見える
- 1 回の視認で必要な情報を読み切りにくい

### 優先度

- 中

### 関連候補

- `ArtifactMainWindow.cppm`
- `ArtifactTimelineWidget.cpp`
- `ArtifactCompositionAudioMixerWidget.cppm`
- `ArtifactRenderCenterWindow.cppm`

### 備考

横幅不足と同様に、初期レイアウトの配分が原因の可能性が高い。  
まずは dock の最低サイズと初期サイズを見直し、必要なら panel ごとに優先度を調整する。

---

## ざっくり優先順

1. Issue 1: セントラルウィジェット側のパネル幅が小さい
2. Issue 2: 下のパネルの高さが足りない

