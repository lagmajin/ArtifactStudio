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

import ArtifactPr.EditorEngine;
import ArtifactPr.AppTheme;
import ArtifactPr.Translation;

using ArtifactPr::trText;

MarkerEditDialog::MarkerEditDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(trText("pr.marker.title", "Edit Marker"));
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    nameEdit_ = new QLineEdit();
    form->addRow(trText("pr.marker.name", "Name:"), nameEdit_);

    positionSpin_ = new QSpinBox();
    positionSpin_->setRange(0, 100000);
    positionSpin_->setSuffix(trText("pr.marker.frame_suffix", " frame"));
    form->addRow(trText("pr.marker.position", "Position:"), positionSpin_);

    typeCombo_ = new QComboBox();
    typeCombo_->addItem(trText("pr.marker.type_comment", "Comment"),
                        static_cast<int>(ArtifactPr::Marker::Type::Comment));
    typeCombo_->addItem(trText("pr.marker.type_chapter", "Chapter"),
                        static_cast<int>(ArtifactPr::Marker::Type::Chapter));
    typeCombo_->addItem(trText("pr.marker.type_in", "In"),
                        static_cast<int>(ArtifactPr::Marker::Type::In));
    typeCombo_->addItem(trText("pr.marker.type_out", "Out"),
                        static_cast<int>(ArtifactPr::Marker::Type::Out));
    form->addRow(trText("pr.marker.type", "Type:"), typeCombo_);

    auto* colorLayout = new QHBoxLayout();
    colorBtn_ = new QPushButton(trText("pr.marker.pick_color", "Pick Color..."));
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
    form->addRow(trText("pr.marker.color", "Color:"), colorLayout);

    layout->addLayout(form);

    layout->addWidget(new QLabel(trText("pr.marker.comment", "Comment:")));
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
