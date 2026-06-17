module;

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <algorithm>

module ArtifactPr.Shortcut;

import ArtifactPr.Shortcut;

namespace ArtifactPr {

PrShortcutRegistry::PrShortcutRegistry() {
    // Playback (J / K / L / Space)
    add(PrShortcut(QStringLiteral("togglePlayPause"),  QStringLiteral("Space"),      QStringLiteral("Toggle playback"),      QStringLiteral("Playback")));
    add(PrShortcut(QStringLiteral("pause"),            QStringLiteral("K"),          QStringLiteral("Pause playback"),       QStringLiteral("Playback")));
    add(PrShortcut(QStringLiteral("shuttleReverse"),   QStringLiteral("J"),          QStringLiteral("Shuttle reverse"),      QStringLiteral("Playback")));
    add(PrShortcut(QStringLiteral("shuttleForward"),   QStringLiteral("L"),          QStringLiteral("Shuttle forward"),      QStringLiteral("Playback")));
    add(PrShortcut(QStringLiteral("seekToStart"),      QStringLiteral("Home"),       QStringLiteral("Seek to start"),        QStringLiteral("Playback")));
    add(PrShortcut(QStringLiteral("seekToEnd"),        QStringLiteral("End"),        QStringLiteral("Seek to end"),          QStringLiteral("Playback")));

    // Edit (C / X / V / Z / Shift+Z / S / D / Delete / Shift+Delete)
    add(PrShortcut(QStringLiteral("splitClip"),        QStringLiteral("C"),          QStringLiteral("Split clip at playhead"), QStringLiteral("Edit")));
    add(PrShortcut(QStringLiteral("copyClip"),         QStringLiteral("Ctrl+C"),     QStringLiteral("Copy selected clip"),   QStringLiteral("Edit")));
    add(PrShortcut(QStringLiteral("cutClip"),          QStringLiteral("Ctrl+X"),     QStringLiteral("Cut selected clip"),    QStringLiteral("Edit")));
    add(PrShortcut(QStringLiteral("pasteClip"),        QStringLiteral("Ctrl+V"),     QStringLiteral("Paste at playhead"),    QStringLiteral("Edit")));
    add(PrShortcut(QStringLiteral("undo"),             QStringLiteral("Ctrl+Z"),     QStringLiteral("Undo"),                 QStringLiteral("Edit")));
    add(PrShortcut(QStringLiteral("redo"),             QStringLiteral("Ctrl+Shift+Z"), QStringLiteral("Redo"),               QStringLiteral("Edit")));
    add(PrShortcut(QStringLiteral("slipClip"),         QStringLiteral("S"),          QStringLiteral("Slip selected clip"),   QStringLiteral("Edit")));
    add(PrShortcut(QStringLiteral("slideClip"),        QStringLiteral("D"),          QStringLiteral("Slide selected clip"),  QStringLiteral("Edit")));
    add(PrShortcut(QStringLiteral("deleteClip"),       QStringLiteral("Delete"),     QStringLiteral("Delete selected clip"), QStringLiteral("Edit")));
    add(PrShortcut(QStringLiteral("rippleDeleteClip"), QStringLiteral("Shift+Delete"), QStringLiteral("Ripple delete selected clip"), QStringLiteral("Edit")));

    // Transition (T / Shift+T / W / Shift+W)
    add(PrShortcut(QStringLiteral("addCrossfade"),     QStringLiteral("T"),          QStringLiteral("Add crossfade transition"), QStringLiteral("Transition")));
    add(PrShortcut(QStringLiteral("addDipToBlack"),    QStringLiteral("Shift+T"),    QStringLiteral("Add dip-to-black transition"), QStringLiteral("Transition")));
    add(PrShortcut(QStringLiteral("addWipeLeft"),      QStringLiteral("W"),          QStringLiteral("Add wipe-left transition"), QStringLiteral("Transition")));
    add(PrShortcut(QStringLiteral("addWipeRight"),     QStringLiteral("Shift+W"),    QStringLiteral("Add wipe-right transition"), QStringLiteral("Transition")));

    // Mark (I / O / M)
    add(PrShortcut(QStringLiteral("setInPoint"),       QStringLiteral("I"),          QStringLiteral("Set in point at playhead"), QStringLiteral("Mark")));
    add(PrShortcut(QStringLiteral("setOutPoint"),      QStringLiteral("O"),          QStringLiteral("Set out point at playhead"), QStringLiteral("Mark")));
    add(PrShortcut(QStringLiteral("addMarker"),        QStringLiteral("M"),          QStringLiteral("Add marker at playhead"), QStringLiteral("Mark")));

    // Zoom (+ / -)
    add(PrShortcut(QStringLiteral("zoomIn"),           QStringLiteral("+"),          QStringLiteral("Zoom in timeline"),     QStringLiteral("Zoom")));
    add(PrShortcut(QStringLiteral("zoomInAlt"),        QStringLiteral("="),          QStringLiteral("Zoom in timeline"),     QStringLiteral("Zoom")));
    add(PrShortcut(QStringLiteral("zoomOut"),          QStringLiteral("-"),          QStringLiteral("Zoom out timeline"),    QStringLiteral("Zoom")));

    // Help
    add(PrShortcut(QStringLiteral("showHelp"),         QStringLiteral("?"),          QStringLiteral("Show keyboard shortcuts"), QStringLiteral("Help")));
}

void PrShortcutRegistry::add(const PrShortcut& sc) {
    byName_[sc.name] = shortcuts_.size();
    shortcuts_.append(sc);
}

QList<PrShortcut> PrShortcutRegistry::byCategory(const QString& category) const {
    QList<PrShortcut> result;
    for (const auto& sc : shortcuts_) {
        if (sc.category == category) {
            result.append(sc);
        }
    }
    return result;
}

QStringList PrShortcutRegistry::categories() const {
    QStringList result;
    for (const auto& sc : shortcuts_) {
        if (!result.contains(sc.category)) {
            result.append(sc.category);
        }
    }
    return result;
}

QString PrShortcutRegistry::helpText() const {
    QString out;
    const auto cats = categories();
    for (const auto& cat : cats) {
        out += QStringLiteral("== %1 ==\n").arg(cat);
        for (const auto& sc : byCategory(cat)) {
            out += QStringLiteral("  %1  %2  -- %3\n").arg(sc.keys, -16, QChar(' ')).arg(sc.name, -24).arg(sc.description);
        }
        out += QChar('\n');
    }
    return out;
}

} // namespace ArtifactPr