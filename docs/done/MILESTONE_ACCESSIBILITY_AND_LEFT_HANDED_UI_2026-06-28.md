# Accessibility and Left-Handed UI Support

**Date**: 2026-06-28
**Status**: Completed
**Parent**: `Settings.Accessibility`

Accessibility and Left-Handed UI Support が実装済みのため、この milestone を完了扱いにする。  
利き手設定、large targets、高コントラスト補助、色覚補助、hover 依存軽減が settings 層と主要 widget に接続されている。

## Evidence

- `Artifact/src/Settings/AccessibilitySettings.cppm`
- `ArtifactCore/include/Application/ArtifactAppSettings.ixx`
- `Artifact/src/Widgets/ArtifactMenuBar.cppm`
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`

## Result

- 左利き向けに menu alignment が変わる
- 主要 widget が accessibility settings を参照している
- large target / contrast / font scale / color deficiency / hover reduction の土台がある
