module;

#include <QHash>
#include <QKeySequence>
#include <QString>
#include <QStringList>
#include <QKeyEvent>

export module ArtifactPr.Shortcut;

import UI.ShortcutBindings;

export namespace ArtifactPr {

/// 1 つの shortcut の定義。
/// key + modifier の組み合わせを人間可読な名前 (J / K / L / Ctrl+Z 等) で扱う。
struct PrShortcut {
    QString name;            // "Play / Pause"
    QString keys;            // "Space" / "Ctrl+Z" / "J"
    QString description;     // "Toggle playback"
    QString category;        // "Playback" / "Edit" / "Mark" / "Zoom"

    PrShortcut() = default;
    PrShortcut(QString n, QString k, QString d, QString c)
        : name(std::move(n)), keys(std::move(k)),
          description(std::move(d)), category(std::move(c)) {}
};

/// shortcut の登録簿。
/// MainWindow の keyPressEvent を switch 文で分岐させず、
/// QHash<QString, PrShortcut> + name ベース dispatch で扱う。
///
/// AGENTS.md 整合:
/// - 新規 connect は不要 (dispatch は関数ポインタ lambda で完結)
/// - QHash + name は immutable map。登録時に const 化される
class PrShortcutRegistry {
public:
    PrShortcutRegistry();

    /// 既存のショートカット一覧 (category 順、name 順)。
    const QList<PrShortcut>& all() const { return shortcuts_; }

    /// category 別に取得。
    QList<PrShortcut> byCategory(const QString& category) const;

    /// shortcut を登録。コンストラクタ外で追加も可能。
    void add(const PrShortcut& sc);

    /// shortcut がある name に登録されているか。
    bool has(const QString& name) const { return byName_.contains(name); }

    /// category のリスト ("Playback" / "Edit" / "Mark" / "Zoom")。
    QStringList categories() const;

    /// shortcut の help text を生成 (overlay UI 用)。
    QString helpText() const;

    /// Shared Artifact bindings を反映した表示用一覧。
    QList<PrShortcut> resolved() const;

private:
    QList<PrShortcut> shortcuts_;
    QHash<QString, int> byName_;
};

/// Resolve ArtifactPr actions through the shared, user-configurable binding
/// store used by Artifact. Actions without a matching shared id keep their
/// local Premiere-style default until a dedicated id is introduced.
ArtifactCore::ShortcutId sharedShortcutId(const QString& actionName);
bool matchesSharedShortcut(const QKeyEvent* event, const QString& actionName);
QKeySequence sharedShortcut(const QString& actionName);
bool loadSharedShortcuts();
bool saveSharedShortcuts();

} // namespace ArtifactPr
