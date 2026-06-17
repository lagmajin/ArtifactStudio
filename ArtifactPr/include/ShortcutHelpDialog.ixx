module;

#include <QDialog>
#include <QTextEdit>

export module ArtifactPr.ShortcutHelpDialog;

import ArtifactPr.Shortcut;

export class ShortcutHelpDialog : public QDialog
{
public:
    explicit ShortcutHelpDialog(QWidget* parent = nullptr);

    /// shortcut registry を渡して dialog を更新。
    void setRegistry(const ArtifactPr::PrShortcutRegistry& reg);

private:
    QTextEdit* textEdit_ = nullptr;
};