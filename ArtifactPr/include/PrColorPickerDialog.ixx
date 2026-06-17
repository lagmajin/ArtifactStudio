module;

#include <QColor>
#include <QDialog>
#include <QString>
#include <QVector>

export module ArtifactPr.ColorPickerDialog;

export namespace ArtifactPr {

/// 自前 color picker。8 色パレットから選択。
/// QColorDialog / QColorDialog::getColor() を使わず、
/// taste 整合 (QColorDialog 禁止) を守る。
class PrColorPickerDialog : public QDialog {
public:
    explicit PrColorPickerDialog(QWidget* parent = nullptr);

    /// 現在の選択色 (accept 後のみ有効)。
    QColor selectedColor() const { return chosen_; }

    /// 初期表示色。
    void setInitialColor(const QColor& c) { chosen_ = c; updateUi(); }

private:
    QColor chosen_;

    void updateUi();
    void onSwatchClicked(const QColor& c);
};

} // namespace ArtifactPr