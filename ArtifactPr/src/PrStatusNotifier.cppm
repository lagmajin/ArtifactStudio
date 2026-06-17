module;

#include <QObject>
#include <QString>
#include <QTimer>

module ArtifactPr.StatusNotifier;

import ArtifactPr.StatusNotifier;

namespace ArtifactPr {

PrStatusNotifier::PrStatusNotifier(QObject* parent)
    : QObject(parent) {
    clearTimer_.setSingleShot(true);
    clearTimer_.setInterval(3000);  // 3 秒
}

void PrStatusNotifier::notify(const QString& msg) {
    Q_EMIT temporaryMessage(msg, 3000);
    clearTimer_.start();
}

void PrStatusNotifier::requestHelp() {
    Q_EMIT helpRequested();
}

} // namespace ArtifactPr