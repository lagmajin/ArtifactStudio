module;

#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVector>
#include <QWidget>

module ArtifactPr.ColorPickerDialog;

import ArtifactPr.ColorPickerDialog;

namespace ArtifactPr {

namespace {
const QVector<QColor>& paletteColors() {
    // AE / Premiere 互換パレット (12 色)
    static const QVector<QColor> kPalette = {
        QColor(255, 255, 100),   // Comment yellow
        QColor(100, 200, 255),   // Chapter blue
        QColor(255, 100, 100),   // In red
        QColor(100, 255, 100),   // Out green
        QColor(255, 200, 100),   // orange
        QColor(200, 100, 255),   // purple
        QColor(255, 100, 200),   // pink
        QColor(100, 255, 200),   // mint
        QColor(180, 180, 180),   // gray
        QColor(255, 150, 50),    // amber
        QColor(50, 150, 255),    // azure
        QColor(150, 255, 50),    // lime
    };
    return kPalette;
}
} // namespace

PrColorPickerDialog::PrColorPickerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Pick Color"));
    setMinimumWidth(320);

    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(QStringLiteral("Choose a color:")));

    auto* grid = new QGridLayout();
    grid->setSpacing(6);
    const int kCols = 6;
    const auto& palette = paletteColors();
    for (int i = 0; i < palette.size(); ++i) {
        const QColor c = palette[i];
        auto* btn = new QPushButton();
        btn->setFixedSize(40, 40);
        btn->setProperty("artifactColorSwatch", c);
        connect(btn, &QPushButton::clicked, this, [this, c]() {
            onSwatchClicked(c);
        });
        // PrProxyStyle が artifactColorSwatch を解釈して
        // ボタンの背景を色 swatch として描画する想定。
        // ここでは単純化のため palette を直接設定 (palette は OK パターン)。
        QPalette pal = btn->palette();
        pal.setColor(QPalette::Button, c);
        btn->setPalette(pal);
        btn->setAutoFillBackground(true);
        grid->addWidget(btn, i / kCols, i % kCols);
    }
    layout->addLayout(grid);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    updateUi();
}

void PrColorPickerDialog::updateUi() {
    // 現状は swatch 側に palette 設定済みなので、ここでは何もしない。
}

void PrColorPickerDialog::onSwatchClicked(const QColor& c) {
    chosen_ = c;
    accept();
}

} // namespace ArtifactPr