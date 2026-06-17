module;

#include <QColor>
#include <QString>

export module ArtifactPr.AppTheme;

export namespace ArtifactPr {

// =====================================================================
// theme override property names (reserved)
// =====================================================================
// QObject::setProperty() で widget に動的色を伝えるための名前。
// PrProxyStyle が描画時にこれらの property を読み取り、既定の
// theme token (DccStyleTheme) より優先して適用する。

inline constexpr const char* kPropAccentColor = "artifactAccentColor";
inline constexpr const char* kPropSurfaceKind = "artifactSurfaceKind";

// surface kind に指定できる値 (PrProxyStyle が解釈する)
inline constexpr const char* kSurfaceTimelineRuler = "timelineRuler";
inline constexpr const char* kSurfacePanelToolbar  = "panelToolbar";
inline constexpr const char* kSurfaceTrackContent  = "trackContent";
inline constexpr const char* kSurfaceMediaPlaceholder = "mediaPlaceholder";

// =====================================================================
// 26 件の既存 setStyleSheet が指定していた色値を token 化。
// PrProxyStyle::polish() がこの値を読み取って palette を構築する。
// 既存見た目を維持しつつ、setStyleSheet を完全に削除するのが目的。
// =====================================================================

struct PrLegacyColors {
    QColor labelMuted;          // #888
    QColor labelInfo;           // #666
    QColor labelSecondary;      // #aaa
    QColor panelBackground;     // #252525
    QColor panelBackgroundAlt;  // #2a2a2a
    QColor trackContent;        // #1e1e1e
    QColor inputBackground;     // #333
    QColor borderSubtle;        // #555
    QColor borderStrong;        // #333
    QColor sliderHandle;        // #4a9eff
    QColor mediaPlaceholder;    // #1a1a1a
    QColor buttonProxyCreate;   // #4a6a8a
    QColor buttonProxyUse;      // #4a8a4a
};

/// 既存 setStyleSheet が指定していた色値 (Dark theme 既定)。
/// Light theme に切替えた場合は別途 palette override が必要。
inline const PrLegacyColors& prLegacyColors() {
    static const PrLegacyColors kDark = {
        QColor("#888888"),  // labelMuted
        QColor("#666666"),  // labelInfo
        QColor("#aaaaaa"),  // labelSecondary
        QColor("#252525"),  // panelBackground
        QColor("#2a2a2a"),  // panelBackgroundAlt
        QColor("#1e1e1e"),  // trackContent
        QColor("#333333"),  // inputBackground
        QColor("#555555"),  // borderSubtle
        QColor("#333333"),  // borderStrong
        QColor("#4a9eff"),  // sliderHandle
        QColor("#1a1a1a"),  // mediaPlaceholder
        QColor("#4a6a8a"),  // buttonProxyCreate
        QColor("#4a8a4a"),  // buttonProxyUse
    };
    return kDark;
}

} // namespace ArtifactPr