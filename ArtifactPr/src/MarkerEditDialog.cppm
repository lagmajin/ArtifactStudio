module;

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

module ArtifactPr.MarkerEditDialog;

import ArtifactPr.MarkerEditDialog;
import ArtifactPr.EditorEngine;
import ArtifactPr.AppTheme;

MarkerEditDialog::MarkerEditDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Edit Marker"));
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    nameEdit_ = new QLineEdit();
    form->addRow(QStringLiteral("Name:"), nameEdit_);

    positionSpin_ = new QSpinBox();
    positionSpin_->setRange(0, 100000);
    positionSpin_->setSuffix(QStringLiteral(" frame"));
    form->addRow(QStringLiteral("Position:"), positionSpin_);

    typeCombo_ = new QComboBox();
    typeCombo_->addItem(QStringLiteral("Comment"), static_cast<int>(ArtifactPr::Marker::Type::Comment));
    typeCombo_->addItem(QStringLiteral("Chapter"), static_cast<int>(ArtifactPr::Marker::Type::Chapter));
    typeCombo_->addItem(QStringLiteral("In"),      static_cast<int>(ArtifactPr::Marker::Type::In));
    typeCombo_->addItem(QStringLiteral("Out"),     static_cast<int>(ArtifactPr::Marker::Type::Out));
    form->addRow(QStringLiteral("Type:"), typeCombo_);

    auto* colorLayout = new QHBoxLayout();
    colorBtn_ = new QPushButton(QStringLiteral("Pick Color..."));
    colorPicker_ = new ArtifactPr::PrColorPickerDialog(this);
    colorPicker_->setInitialColor(color_);
    connect(colorBtn_, &QPushButton::clicked, this, [this]() {
        colorPicker_->setInitialColor(color_);
        if (colorPicker_->exec() == QDialog::Accepted) {
            color_ = colorPicker_->selectedColor();
        }
    });
    colorLayout->addWidget(colorBtn_);
    colorLayout->addStretch();
    form->addRow(QStringLiteral("Color:"), colorLayout);

    layout->addLayout(form);

    layout->addWidget(new QLabel(QStringLiteral("Comment:")));
    commentEdit_ = new QPlainTextEdit();
    commentEdit_->setMaximumHeight(120);
    layout->addWidget(commentEdit_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        result_.name = nameEdit_->text();
        result_.comment = commentEdit_->toPlainText();
        result_.position = positionSpin_->value();
        result_.type = static_cast<ArtifactPr::Marker::Type>(typeCombo_->currentData().toInt());
        result_.color = color_;
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void MarkerEditDialog::setMarker(const ArtifactPr::Marker& marker) {
    result_ = marker;
    nameEdit_->setText(marker.name);
    commentEdit_->setPlainText(marker.comment);
    positionSpin_->setValue(static_cast<int>(marker.position));
    color_ = marker.color;
    for (int i = 0; i < typeCombo_->count(); ++i) {
        if (typeCombo_->itemData(i).toInt() == static_cast<int>(marker.type)) {
            typeCombo_->setCurrentIndex(i);
            break;
        }
    }
}