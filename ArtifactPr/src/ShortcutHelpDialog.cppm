module;

#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

module ArtifactPr.ShortcutHelpDialog;

import ArtifactPr.ShortcutHelpDialog;
import ArtifactPr.Shortcut;

ShortcutHelpDialog::ShortcutHelpDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Keyboard Shortcuts"));
    setMinimumSize(520, 480);

    auto* layout = new QVBoxLayout(this);

    textEdit_ = new QTextEdit();
    textEdit_->setReadOnly(true);
    QFont monoFont(QStringLiteral("Consolas"));
    monoFont.setStyleHint(QFont::Monospace);
    textEdit_->setFont(monoFont);
    layout->addWidget(textEdit_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttons);
}

void ShortcutHelpDialog::setRegistry(const ArtifactPr::PrShortcutRegistry& reg) {
    textEdit_->setPlainText(reg.helpText());
}