module;

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QProxyStyle>
#include <QPushButton>
#include <QStyleFactory>
#include <QStyleOption>
#include <QStyleOptionButton>
#include <QWidget>
#include <QPainter>
#include <QString>

export module ArtifactPr.AppTheme;

import ArtifactPr.AppTheme;

namespace ArtifactPr {

// =====================================================================
// PrProxyStyle
// ---------------------------------------------------------------------
// ArtifactPr 専用の QProxyStyle。ArtifactCore::DccStyleTheme を基本に
// 26 件あった setStyleSheet を一括吸収する。
//
// 既存 setStyleSheet の役割:
//   - QLabel の muted / info / secondary 色
//   - QPushButton の背景色 (accent / buttonProxyCreate / buttonProxyUse)
//   - QListWidget / QLineEdit / QComboBox の panel + border
//   - QWidget コンテナ (timeline / toolbar / track / placeholder) の背景
//   - QSlider の groove + handle 描画
//   - QCheckBox の WindowText
//   - transition button の動的色 (setProperty("artifactAccentColor"))
//
// これらを polish() + drawControl() + drawComplexControl() で処理。
// =====================================================================

class PrProxyStyle : public QProxyStyle {
public:
    explicit PrProxyStyle(QStyle* baseStyle)
        : QProxyStyle(baseStyle ? baseStyle : QStyleFactory::create(QStringLiteral("Fusion"))) {}

    ~PrProxyStyle() override = default;

    // =================================================================
    // polish(QPalette*)
    // -----------------------------------------------------------------
    // QApplication::setStyle(new PrProxyStyle(...)) のあと、
    // QApplication::setPalette() で渡された palette に対して
    // 既存 setStyleSheet 由来の色値を上書きする。
    // =================================================================
    void polish(QPalette* palette) override {
        QProxyStyle::polish(palette);
        if (!palette) return;

        const auto& c = prLegacyColors();

        // WindowText (label text) - 既定 theme を保ちつつ、
        // 26 件で頻出した muted / info / secondary を ColorRole として用意
        palette->setColor(QPalette::WindowText, c.labelMuted);

        // Window / Base (背景) - 既存 setStyleSheet の panel / track / placeholder を
        // 異なる ColorRole で表現するのではなく、setAutoFillBackground(true) を
        // 併用する widget 側で個別に扱う。
    }

    // =================================================================
    // polish(QWidget*)
    // -----------------------------------------------------------------
    // widget 生成時に呼ばれて、surfaceKind property を解釈して
    // palette + autoFillBackground を設定する。
    // =================================================================
    void polish(QWidget* widget) override {
        QProxyStyle::polish(widget);
        if (!widget) return;

        const auto& c = prLegacyColors();
        const QString surfaceKind = widget->property(kPropSurfaceKind).toString();

        if (surfaceKind == kSurfaceTimelineRuler) {
            widget->setAutoFillBackground(true);
            QPalette p = widget->palette();
            p.setColor(QPalette::Window, c.panelBackgroundAlt);
            p.setColor(QPalette::WindowText, c.labelMuted);
            widget->setPalette(p);
        } else if (surfaceKind == kSurfacePanelToolbar) {
            widget->setAutoFillBackground(true);
            QPalette p = widget->palette();
            p.setColor(QPalette::Window, c.panelBackground);
            widget->setPalette(p);
        } else if (surfaceKind == kSurfaceTrackContent) {
            widget->setAutoFillBackground(true);
            QPalette p = widget->palette();
            p.setColor(QPalette::Window, c.trackContent);
            widget->setPalette(p);
        } else if (surfaceKind == kSurfaceMediaPlaceholder) {
            widget->setAutoFillBackground(true);
            QPalette p = widget->palette();
            p.setColor(QPalette::Window, c.mediaPlaceholder);
            p.setColor(QPalette::WindowText, c.labelInfo);
            widget->setPalette(p);
        }
    }

    // =================================================================
    // drawControl(CE_PushButtonLabel, ...)
    // -----------------------------------------------------------------
    // setProperty("artifactAccentColor", color) を持つ button は、
    // 既定の QProxyStyle::drawControl を呼ばずに、独自色で塗りつぶす。
    // 26 件のうち transition button (line 702) がこれに該当。
    // =================================================================
    void drawControl(ControlElement element,
                     const QStyleOption* option,
                     QPainter* painter,
                     const QWidget* widget = nullptr) const override {
        if (element == CE_PushButtonLabel && widget && widget->property(kPropAccentColor).isValid()) {
            const QStyleOptionButton* btnOpt = qstyleoption_cast<const QStyleOptionButton*>(option);
            if (!btnOpt || !painter) {
                QProxyStyle::drawControl(element, option, painter, widget);
                return;
            }
            const QColor accent = widget->property(kPropAccentColor).value<QColor>();

            // background
            QColor bg = accent;
            if (!(btnOpt->state & State_Enabled)) {
                bg = accent.darker(140);
            } else if (btnOpt->state & State_MouseOver) {
                bg = accent.lighter(110);
            } else if (btnOpt->state & State_Sunken) {
                bg = accent.darker(115);
            }
            painter->fillRect(btnOpt->rect, bg);

            // text
            painter->setPen(Qt::white);
            painter->drawText(btnOpt->rect, Qt::AlignCenter, widget->text());
            return;
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }

    // =================================================================
    // drawControl(CE_Slider, ...)
    // -----------------------------------------------------------------
    // 26 件のうち volumeSlider (line 843) と zoomSlider (line 1276) が
    // 独自 QSlider::groove / handle 描画をしていた。
    // accent color ベースで Fusion 標準より少しだけアクセントを効かせる。
    // =================================================================
    void drawComplexControl(ComplexControl control,
                            const QStyleOptionComplex* option,
                            QPainter* painter,
                            const QWidget* widget = nullptr) const override {
        if (control == CC_Slider && widget) {
            const auto& c = prLegacyColors();
            // 一旦 QProxyStyle に任せてから、handle の上にアクセント色を描画
            QProxyStyle::drawComplexControl(control, option, painter, widget);

            if (!painter || !option) return;
            const QStyleOptionSlider* sliderOpt = qstyleoption_cast<const QStyleOptionSlider*>(option);
            if (!sliderOpt || sliderOpt->orientation != Qt::Horizontal) return;

            // handle の中心位置
            QRect handle = subControlRect(CC_Slider, sliderOpt, SC_SliderHandle, widget);
            QPoint center = handle.center();
            const int radius = std::max(2, handle.width() / 3);
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(Qt::NoPen);
            painter->setBrush(c.sliderHandle);
            painter->drawEllipse(center, radius, radius);
            return;
        }
        QProxyStyle::drawComplexControl(control, option, painter, widget);
    }
};

} // namespace ArtifactPr