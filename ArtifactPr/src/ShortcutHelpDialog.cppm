module;

#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFile>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QFont>
#include <QFrame>
#include <QHash>
#include <QList>
#include <QPainter>
#include <QScrollArea>
#include <QString>
#include <QStringList>
#include <QTextEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QMap>
#include <QWidget>
#include <algorithm>
#include <utility>

module ArtifactPr.ShortcutHelpDialog;

import ArtifactPr.ShortcutHelpDialog;
import ArtifactPr.Shortcut;
import UI.ShortcutBindings;

namespace {

struct KeyboardKeyVisual {
    QString id;
    QString label;
    double units = 1.0;
};

struct KeyboardLayoutVisual {
    QString title;
    QString hint;
    QList<QList<KeyboardKeyVisual>> rows;
};

KeyboardKeyVisual key(QString id, QString label, double units = 1.0)
{
    return KeyboardKeyVisual{std::move(id), std::move(label), units};
}

KeyboardLayoutVisual makeAnsiLayout()
{
    return KeyboardLayoutVisual{
        QStringLiteral("US ANSI"),
        QStringLiteral("Common English / Chinese IME physical layout"),
        {
            {key(QStringLiteral("Esc"), QStringLiteral("Esc")), key(QStringLiteral("1"), QStringLiteral("1")), key(QStringLiteral("2"), QStringLiteral("2")), key(QStringLiteral("3"), QStringLiteral("3")), key(QStringLiteral("4"), QStringLiteral("4")), key(QStringLiteral("5"), QStringLiteral("5")), key(QStringLiteral("6"), QStringLiteral("6")), key(QStringLiteral("7"), QStringLiteral("7")), key(QStringLiteral("8"), QStringLiteral("8")), key(QStringLiteral("9"), QStringLiteral("9")), key(QStringLiteral("0"), QStringLiteral("0")), key(QStringLiteral("-"), QStringLiteral("-")), key(QStringLiteral("="), QStringLiteral("=")), key(QStringLiteral("Backspace"), QStringLiteral("Backspace"), 2.0)},
            {key(QStringLiteral("Tab"), QStringLiteral("Tab"), 1.5), key(QStringLiteral("Q"), QStringLiteral("Q")), key(QStringLiteral("W"), QStringLiteral("W")), key(QStringLiteral("E"), QStringLiteral("E")), key(QStringLiteral("R"), QStringLiteral("R")), key(QStringLiteral("T"), QStringLiteral("T")), key(QStringLiteral("Y"), QStringLiteral("Y")), key(QStringLiteral("U"), QStringLiteral("U")), key(QStringLiteral("I"), QStringLiteral("I")), key(QStringLiteral("O"), QStringLiteral("O")), key(QStringLiteral("P"), QStringLiteral("P")), key(QStringLiteral("["), QStringLiteral("[")), key(QStringLiteral("]"), QStringLiteral("]")), key(QStringLiteral("\\"), QStringLiteral("\\"), 1.5)},
            {key(QStringLiteral("Caps"), QStringLiteral("Caps"), 1.75), key(QStringLiteral("A"), QStringLiteral("A")), key(QStringLiteral("S"), QStringLiteral("S")), key(QStringLiteral("D"), QStringLiteral("D")), key(QStringLiteral("F"), QStringLiteral("F")), key(QStringLiteral("G"), QStringLiteral("G")), key(QStringLiteral("H"), QStringLiteral("H")), key(QStringLiteral("J"), QStringLiteral("J")), key(QStringLiteral("K"), QStringLiteral("K")), key(QStringLiteral("L"), QStringLiteral("L")), key(QStringLiteral(";"), QStringLiteral(";")), key(QStringLiteral("'"), QStringLiteral("'")), key(QStringLiteral("Enter"), QStringLiteral("Enter"), 2.25)},
            {key(QStringLiteral("Shift"), QStringLiteral("Shift"), 2.25), key(QStringLiteral("Z"), QStringLiteral("Z")), key(QStringLiteral("X"), QStringLiteral("X")), key(QStringLiteral("C"), QStringLiteral("C")), key(QStringLiteral("V"), QStringLiteral("V")), key(QStringLiteral("B"), QStringLiteral("B")), key(QStringLiteral("N"), QStringLiteral("N")), key(QStringLiteral("M"), QStringLiteral("M")), key(QStringLiteral(","), QStringLiteral(",")), key(QStringLiteral("."), QStringLiteral(".")), key(QStringLiteral("/"), QStringLiteral("/")), key(QStringLiteral("Shift"), QStringLiteral("Shift"), 2.75)},
            {key(QStringLiteral("Ctrl"), QStringLiteral("Ctrl"), 1.25), key(QStringLiteral("Win"), QStringLiteral("Win"), 1.25), key(QStringLiteral("Alt"), QStringLiteral("Alt"), 1.25), key(QStringLiteral("Space"), QStringLiteral("Space"), 6.25), key(QStringLiteral("Alt"), QStringLiteral("Alt"), 1.25), key(QStringLiteral("Menu"), QStringLiteral("Menu"), 1.25), key(QStringLiteral("Ctrl"), QStringLiteral("Ctrl"), 1.25), key(QStringLiteral("Home"), QStringLiteral("Home"), 1.25), key(QStringLiteral("End"), QStringLiteral("End"), 1.25), key(QStringLiteral("Delete"), QStringLiteral("Del"), 1.25)}
        }
    };
}

KeyboardLayoutVisual makeIsoLayout(const QString& title, const QString& hint)
{
    auto layout = makeAnsiLayout();
    layout.title = title;
    layout.hint = hint;
    layout.rows[1].last().label = QStringLiteral("#");
    layout.rows[1].last().units = 1.0;
    layout.rows[2].last().units = 1.5;
    layout.rows[3].first().units = 1.25;
    layout.rows[3].insert(1, key(QStringLiteral("<"), QStringLiteral("<")));
    return layout;
}

KeyboardLayoutVisual makeJisLayout()
{
    auto layout = makeAnsiLayout();
    layout.title = QStringLiteral("Japanese JIS");
    layout.hint = QStringLiteral("Adds language keys around Space and a shorter right Shift");
    layout.rows[0].insert(layout.rows[0].size() - 1, key(QStringLiteral("^"), QStringLiteral("^")));
    layout.rows[1].last().label = QStringLiteral("]");
    layout.rows[2].insert(layout.rows[2].size() - 1, key(QStringLiteral(":"), QStringLiteral(":")));
    layout.rows[3].first().units = 1.75;
    layout.rows[3].insert(1, key(QStringLiteral("\\"), QStringLiteral("\\")));
    layout.rows[3].last().units = 2.0;
    layout.rows[4] = {
        key(QStringLiteral("Ctrl"), QStringLiteral("Ctrl"), 1.2),
        key(QStringLiteral("Win"), QStringLiteral("Win"), 1.1),
        key(QStringLiteral("Alt"), QStringLiteral("Alt"), 1.1),
        key(QStringLiteral("Muhenkan"), QStringLiteral("Muhenkan"), 1.5),
        key(QStringLiteral("Space"), QStringLiteral("Space"), 3.5),
        key(QStringLiteral("Henkan"), QStringLiteral("Henkan"), 1.5),
        key(QStringLiteral("Kana"), QStringLiteral("Kana"), 1.2),
        key(QStringLiteral("Alt"), QStringLiteral("Alt"), 1.1),
        key(QStringLiteral("Ctrl"), QStringLiteral("Ctrl"), 1.2),
        key(QStringLiteral("Home"), QStringLiteral("Home"), 1.2),
        key(QStringLiteral("End"), QStringLiteral("End"), 1.2),
        key(QStringLiteral("Delete"), QStringLiteral("Del"), 1.2)
    };
    return layout;
}

KeyboardLayoutVisual makeKoreanLayout()
{
    auto layout = makeAnsiLayout();
    layout.title = QStringLiteral("Korean");
    layout.hint = QStringLiteral("ANSI-like with Hangul / Hanja language keys");
    layout.rows[4] = {
        key(QStringLiteral("Ctrl"), QStringLiteral("Ctrl"), 1.25),
        key(QStringLiteral("Win"), QStringLiteral("Win"), 1.25),
        key(QStringLiteral("Alt"), QStringLiteral("Alt"), 1.25),
        key(QStringLiteral("Space"), QStringLiteral("Space"), 4.75),
        key(QStringLiteral("Hangul"), QStringLiteral("Hangul"), 1.5),
        key(QStringLiteral("Hanja"), QStringLiteral("Hanja"), 1.5),
        key(QStringLiteral("Ctrl"), QStringLiteral("Ctrl"), 1.25),
        key(QStringLiteral("Home"), QStringLiteral("Home"), 1.25),
        key(QStringLiteral("End"), QStringLiteral("End"), 1.25),
        key(QStringLiteral("Delete"), QStringLiteral("Del"), 1.25)
    };
    return layout;
}

QString baseKeyName(QString shortcut)
{
    const auto pieces = shortcut.split(QChar('+'), Qt::SkipEmptyParts);
    if (pieces.isEmpty()) {
        return shortcut.trimmed();
    }
    return pieces.last().trimmed();
}

QString visualKeyName(QString shortcut)
{
    auto baseKey = baseKeyName(std::move(shortcut)).toUpper();
    if (baseKey == QStringLiteral("+")) {
        return QStringLiteral("=");
    }
    if (baseKey == QStringLiteral("?")) {
        return QStringLiteral("/");
    }
    if (baseKey == QStringLiteral("DEL")) {
        return QStringLiteral("DELETE");
    }
    return baseKey;
}

class KeyboardPreviewWidget final : public QWidget {
public:
    explicit KeyboardPreviewWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        layouts_.append(makeAnsiLayout());
        layouts_.append(makeIsoLayout(QStringLiteral("UK / ISO"), QStringLiteral("Large Enter, short left Shift, extra ISO key")));
        layouts_.append(makeJisLayout());
        layouts_.append(makeAnsiLayout());
        layouts_.last().title = QStringLiteral("Chinese IME");
        layouts_.last().hint = QStringLiteral("Usually ANSI physical keys with IME conversion layered above");
        layouts_.append(makeKoreanLayout());
        setMinimumHeight(300);
    }

    void setShortcuts(const QList<ArtifactPr::PrShortcut>& shortcuts)
    {
        shortcutLabels_.clear();
        for (const auto& sc : shortcuts) {
            const auto baseKey = visualKeyName(sc.keys);
            if (baseKey.isEmpty()) {
                continue;
            }
            auto text = sc.keys;
            if (!sc.description.isEmpty()) {
                text += QStringLiteral(" - ");
                text += sc.description;
            }
            shortcutLabels_[baseKey].append(text);
        }
        updateGeometry();
        update();
    }

    QSize sizeHint() const override
    {
        return QSize(920, 700);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), palette().color(QPalette::Base));

        const int margin = 12;
        const int gap = 18;
        const int layoutHeight = 120;
        int y = margin;
        for (const auto& layout : layouts_) {
            paintLayout(painter, layout, QRect(margin, y, width() - margin * 2, layoutHeight));
            y += layoutHeight + gap;
        }
    }

private:
    void paintLayout(QPainter& painter, const KeyboardLayoutVisual& layout, const QRect& bounds)
    {
        const auto textColor = palette().color(QPalette::Text);
        const auto mutedColor = palette().color(QPalette::Mid);
        const auto keyColor = palette().color(QPalette::Button);
        const auto activeColor = palette().color(QPalette::Highlight);
        const auto activeTextColor = palette().color(QPalette::HighlightedText);

        QFont titleFont = font();
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.setPen(textColor);
        painter.drawText(QRect(bounds.left(), bounds.top(), 150, 20), Qt::AlignLeft | Qt::AlignVCenter, layout.title);

        QFont hintFont = font();
        hintFont.setPointSize(std::max(7, hintFont.pointSize() - 1));
        painter.setFont(hintFont);
        painter.setPen(mutedColor);
        painter.drawText(QRect(bounds.left() + 160, bounds.top(), bounds.width() - 160, 20), Qt::AlignLeft | Qt::AlignVCenter, layout.hint);

        const int top = bounds.top() + 24;
        const int rowHeight = 17;
        const int rowGap = 4;
        const int keyGap = 3;
        const double maxUnits = maxRowUnits(layout);
        const double unit = std::max(16.0, (bounds.width() - keyGap * 16) / maxUnits);

        QFont keyFont = font();
        keyFont.setPointSize(std::max(7, keyFont.pointSize() - 1));
        painter.setFont(keyFont);

        int rowIndex = 0;
        for (const auto& row : layout.rows) {
            int x = bounds.left();
            const int y = top + rowIndex * (rowHeight + rowGap);
            for (const auto& keyVisual : row) {
                const int keyWidth = std::max(18, int(keyVisual.units * unit));
                const QString lookup = keyVisual.id.toUpper();
                const bool active = shortcutLabels_.contains(lookup);
                const QRect keyRect(x, y, keyWidth, rowHeight);
                painter.setPen(active ? activeColor.darker(125) : palette().color(QPalette::Mid));
                painter.setBrush(active ? activeColor : keyColor);
                painter.drawRoundedRect(keyRect.adjusted(0, 0, -1, -1), 3, 3);

                painter.setPen(active ? activeTextColor : textColor);
                painter.drawText(keyRect.adjusted(3, 0, -3, 0), Qt::AlignCenter, keyVisual.label);
                x += keyWidth + keyGap;
            }
            ++rowIndex;
        }
    }

    double maxRowUnits(const KeyboardLayoutVisual& layout) const
    {
        double result = 1.0;
        for (const auto& row : layout.rows) {
            double total = 0.0;
            for (const auto& keyVisual : row) {
                total += keyVisual.units;
            }
            result = std::max(result, total);
        }
        return result;
    }

    QList<KeyboardLayoutVisual> layouts_;
    QHash<QString, QStringList> shortcutLabels_;
};

} // namespace

ShortcutHelpDialog::ShortcutHelpDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Keyboard Shortcuts"));
    setMinimumSize(780, 680);

    auto* layout = new QVBoxLayout(this);

    keyboardPreview_ = new KeyboardPreviewWidget(this);
    auto* previewScrollArea = new QScrollArea(this);
    previewScrollArea->setWidgetResizable(true);
    previewScrollArea->setFrameShape(QFrame::NoFrame);
    previewScrollArea->setWidget(keyboardPreview_);
    layout->addWidget(previewScrollArea, 1);

    textEdit_ = new QTextEdit();
    textEdit_->setReadOnly(true);
    QFont monoFont(QStringLiteral("Consolas"));
    monoFont.setStyleHint(QFont::Monospace);
    textEdit_->setFont(monoFont);
    layout->addWidget(textEdit_, 1);

    auto* editorLabel = new QLabel(QStringLiteral("Editable shortcuts (shared with Artifact)"), this);
    layout->addWidget(editorLabel);

    editorTable_ = new QTableWidget(this);
    editorTable_->setColumnCount(5);
    editorTable_->setHorizontalHeaderLabels({QStringLiteral("Category"), QStringLiteral("Action"), QStringLiteral("Default"), QStringLiteral("Shortcut"), QStringLiteral("Description")});
    editorTable_->horizontalHeader()->setStretchLastSection(true);
    editorTable_->setSelectionMode(QAbstractItemView::NoSelection);
    editorTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    editorTable_->setSortingEnabled(true);
    editorTable_->setAlternatingRowColors(true);
    editorTable_->setMinimumHeight(180);
    layout->addWidget(editorTable_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Apply, this);
    auto* resetButton = buttons->addButton(QStringLiteral("Reset Defaults"), QDialogButtonBox::ResetRole);
    auto* exportButton = buttons->addButton(QStringLiteral("Export Preset..."), QDialogButtonBox::ActionRole);
    auto* importButton = buttons->addButton(QStringLiteral("Import Preset..."), QDialogButtonBox::ActionRole);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() {
        QMap<QString, QString> seen;
        for (auto it = editors_.cbegin(); it != editors_.cend(); ++it) {
            const QString sequence = it.value()->keySequence().toString(QKeySequence::PortableText);
            if (sequence.isEmpty()) continue;
            if (seen.contains(sequence)) {
                QMessageBox::warning(this, QStringLiteral("Shortcut Conflict"),
                                     QStringLiteral("%1 is already assigned to %2.")
                                         .arg(it.key(), seen.value(sequence)));
                return;
            }
            seen.insert(sequence, it.key());
        }

        auto& bindings = ArtifactCore::ShortcutBindings::instance();
        for (auto it = editors_.cbegin(); it != editors_.cend(); ++it) {
            const auto id = ArtifactPr::sharedShortcutId(it.key());
            if (id != ArtifactCore::ShortcutId::Count) {
                bindings.setShortcut(id, it.value()->keySequence());
            }
        }
        if (!ArtifactPr::saveSharedShortcuts()) {
            QMessageBox::warning(this, QStringLiteral("Save Shortcuts"),
                                 QStringLiteral("The shortcuts were applied for this session, but could not be saved."));
        }
        if (editorTable_) {
            ArtifactPr::PrShortcutRegistry updated;
            textEdit_->setPlainText(updated.helpText());
            if (auto* preview = dynamic_cast<KeyboardPreviewWidget*>(keyboardPreview_)) {
                preview->setShortcuts(updated.resolved());
            }
        }
    });
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        ArtifactCore::ShortcutBindings::instance().resetToDefaults();
        if (!ArtifactPr::saveSharedShortcuts()) {
            QMessageBox::warning(this, QStringLiteral("Save Shortcuts"),
                                 QStringLiteral("Defaults were restored for this session, but could not be saved."));
        }
        for (auto it = editors_.begin(); it != editors_.end(); ++it) {
            const auto id = ArtifactPr::sharedShortcutId(it.key());
            if (id != ArtifactCore::ShortcutId::Count) {
                it.value()->setKeySequence(ArtifactCore::ShortcutBindings::instance().shortcut(id));
            }
        }
        setRegistry(ArtifactPr::PrShortcutRegistry());
    });
    connect(exportButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Export Shortcut Preset"), QStringLiteral("artifactpr-shortcuts.json"),
            QStringLiteral("Shortcut Preset (*.json);;All Files (*)"));
        if (path.isEmpty()) return;

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(this, QStringLiteral("Export Shortcut Preset"),
                                 QStringLiteral("Could not write the shortcut preset."));
            return;
        }
        const QJsonDocument document(ArtifactCore::ShortcutBindings::instance().toJson());
        const QByteArray payload = document.toJson(QJsonDocument::Indented);
        if (file.write(payload) != payload.size()) {
            QMessageBox::warning(this, QStringLiteral("Export Shortcut Preset"),
                                 QStringLiteral("The shortcut preset could not be written completely."));
        }
    });
    connect(importButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Import Shortcut Preset"), QString(),
            QStringLiteral("Shortcut Preset (*.json);;All Files (*)"));
        if (path.isEmpty()) return;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, QStringLiteral("Import Shortcut Preset"),
                                 QStringLiteral("Could not read the shortcut preset."));
            return;
        }
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject() ||
            !ArtifactCore::ShortcutBindings::instance().loadFromJson(document.object())) {
            QMessageBox::warning(this, QStringLiteral("Import Shortcut Preset"),
                                 QStringLiteral("The file does not contain valid Artifact shortcut bindings."));
            return;
        }
        if (!ArtifactPr::saveSharedShortcuts()) {
            QMessageBox::warning(this, QStringLiteral("Save Shortcuts"),
                                 QStringLiteral("The preset was imported for this session, but could not be saved."));
        }
        setRegistry(ArtifactPr::PrShortcutRegistry());
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttons);
}

void ShortcutHelpDialog::setRegistry(const ArtifactPr::PrShortcutRegistry& reg) {
    if (auto* preview = dynamic_cast<KeyboardPreviewWidget*>(keyboardPreview_)) {
        preview->setShortcuts(reg.resolved());
    }
    textEdit_->setPlainText(reg.helpText());

    if (!editorTable_) return;
    const bool sortingEnabled = editorTable_->isSortingEnabled();
    editorTable_->setSortingEnabled(false);
    editors_.clear();
    editorTable_->setRowCount(0);
    const auto shortcuts = reg.all();
    for (const auto& shortcut : shortcuts) {
        const auto id = ArtifactPr::sharedShortcutId(shortcut.name);
        if (id == ArtifactCore::ShortcutId::Count) continue;

        const int row = editorTable_->rowCount();
        editorTable_->insertRow(row);
        editorTable_->setItem(row, 0, new QTableWidgetItem(shortcut.category));
        editorTable_->setItem(row, 1, new QTableWidgetItem(shortcut.name));
        editorTable_->setItem(row, 2, new QTableWidgetItem(
            ArtifactCore::ShortcutBindings::instance().defaultShortcut(id).toString(QKeySequence::NativeText)));
        auto* edit = new QKeySequenceEdit(ArtifactCore::ShortcutBindings::instance().shortcut(id), editorTable_);
        edit->setMaximumSequenceLength(1);
        edit->setToolTip(shortcut.description);
        editorTable_->setCellWidget(row, 3, edit);
        editorTable_->setItem(row, 4, new QTableWidgetItem(shortcut.description));
        editors_.insert(shortcut.name, edit);
    }
    editorTable_->setSortingEnabled(sortingEnabled);
}
