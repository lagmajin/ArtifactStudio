module;

#include <QColor>
#include <QDialog>
#include <QString>

export module ArtifactPr.MarkerEditDialog;

import ArtifactPr.EditorEngine;
import ArtifactPr.ColorPickerDialog;

export class MarkerEditDialog : public QDialog
{
public:
    explicit MarkerEditDialog(QWidget* parent = nullptr);

    /// 編集対象の marker を設定。
    void setMarker(const ArtifactPr::Marker& marker);

    /// 編集結果を取得 (accept 後のみ有効)。
    ArtifactPr::Marker marker() const { return result_; }

private:
    ArtifactPr::Marker result_;

    QLineEdit* nameEdit_ = nullptr;
    QPlainTextEdit* commentEdit_ = nullptr;
    QComboBox* typeCombo_ = nullptr;
    QPushButton* colorBtn_ = nullptr;
    QSpinBox* positionSpin_ = nullptr;
    QColor color_;
    ArtifactPr::PrColorPickerDialog* colorPicker_ = nullptr;
};