module;

#include <QObject>
#include <QString>
#include <QTimer>

export module ArtifactPr.StatusNotifier;

export namespace ArtifactPr {

/// status bar に短期メッセージ (3 秒) を表示するヘルパ。
/// EditorEngine の projectModified / undo / redo signal に接続して使う。
/// 実際の表示は connect 先の QStatusBar に委譲 (QObject signal で通知)。
class PrStatusNotifier : public QObject {
public:
    explicit PrStatusNotifier(QObject* parent = nullptr);

    /// メッセージを送る (3 秒タイマで自動消去)。
    void notify(const QString& msg);

    /// 永続的な help overlay を要求 (PrKeyboardShortcutOverlay 用)。
    void requestHelp();

Q_SIGNALS:
    void temporaryMessage(const QString& msg, int timeoutMs);
    void helpRequested();

private:
    QTimer clearTimer_;
};

} // namespace ArtifactPr