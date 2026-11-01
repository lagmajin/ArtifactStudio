#include <gtest/gtest.h>

#include <array>

#include <QKeyEvent>
#include <QKeySequence>
#include <QJsonObject>
#include <QString>

import UI.ShortcutBindings;

namespace {

using ArtifactCore::ShortcutBindings;
using ArtifactCore::ShortcutId;

QKeyEvent keyPress(int key, Qt::KeyboardModifiers modifiers = {},
                   bool autoRepeat = false)
{
    return QKeyEvent(QEvent::KeyPress, key, modifiers, QString(), autoRepeat, 1);
}

} // namespace

TEST(ShortcutBindingsContractTest, IdListCoversEveryOrdinalExactlyOnceWithDisplayName)
{
    const auto ids = ArtifactCore::allShortcutIds();
    std::array<bool, static_cast<std::size_t>(ShortcutId::Count)> seen{};

    for (const ShortcutId id : ids) {
        const auto ordinal = static_cast<std::size_t>(id);
        ASSERT_LT(ordinal, seen.size());
        EXPECT_FALSE(seen[ordinal]) << ordinal;
        seen[ordinal] = true;
        EXPECT_FALSE(ArtifactCore::shortcutDisplayName(id).isEmpty()) << ordinal;
    }

    for (std::size_t ordinal = 0; ordinal < seen.size(); ++ordinal) {
        EXPECT_TRUE(seen[ordinal]) << ordinal;
    }
}

TEST(ShortcutBindingsContractTest, DefaultsCanBeOverriddenAndEventMatchingHonorsOverride)
{
    auto& bindings = ShortcutBindings::instance();
    bindings.resetToDefaults();
    const auto id = ShortcutId::PlaybackToggle;
    EXPECT_EQ(bindings.shortcut(id), bindings.defaultShortcut(id));

    const QKeySequence custom(Qt::CTRL | Qt::Key_P);
    bindings.setShortcut(id, custom);
    EXPECT_EQ(bindings.shortcut(id), custom);
    auto matchingEvent = keyPress(Qt::Key_P, Qt::ControlModifier);
    auto defaultEvent = keyPress(Qt::Key_Space);
    auto repeatEvent = keyPress(Qt::Key_P, Qt::ControlModifier, true);
    EXPECT_TRUE(bindings.matches(&matchingEvent, id));
    EXPECT_FALSE(bindings.matches(&defaultEvent, id));
    EXPECT_FALSE(bindings.matches(&repeatEvent, id));
    bindings.resetToDefaults();
}

TEST(ShortcutBindingsContractTest, JsonRoundTripRestoresPortableOverridesAndRejectsUnknownOnlyData)
{
    auto& bindings = ShortcutBindings::instance();
    bindings.resetToDefaults();
    const auto id = ShortcutId::PlaybackToggle;
    const QKeySequence custom(Qt::ALT | Qt::Key_F8);
    bindings.setShortcut(id, custom);

    const QJsonObject json = bindings.toJson();
    EXPECT_EQ(json.value(QStringLiteral("format")).toString(),
              QStringLiteral("Artifact.Shortcuts.v1"));
    EXPECT_EQ(json.value(QStringLiteral("shortcuts")).toObject()
                  .value(QStringLiteral("PlaybackToggle")).toString(),
              custom.toString(QKeySequence::PortableText));

    bindings.setShortcut(id, QKeySequence(Qt::Key_J));
    EXPECT_TRUE(bindings.loadFromJson(json));
    EXPECT_EQ(bindings.shortcut(id), custom);

    const QJsonObject unknownOnly{
        {QStringLiteral("shortcuts"),
         QJsonObject{{QStringLiteral("UnknownShortcut"), QStringLiteral("Ctrl+U")}}}};
    EXPECT_FALSE(bindings.loadFromJson(unknownOnly));
    EXPECT_EQ(bindings.shortcut(id), custom);
    bindings.resetToDefaults();
}

TEST(ShortcutBindingsContractTest, ChangeListenerCanBeRemovedAndEmptyOverrideDisablesBinding)
{
    auto& bindings = ShortcutBindings::instance();
    bindings.resetToDefaults();
    const auto id = ShortcutId::PlaybackToggle;
    int notifications = 0;
    const auto token = bindings.addChangeListener(
        [&](ShortcutId changed) {
            EXPECT_EQ(changed, id);
            ++notifications;
        });

    bindings.setShortcut(id, QKeySequence(Qt::Key_Q));
    EXPECT_EQ(notifications, 1);
    bindings.removeChangeListener(token);
    bindings.setShortcut(id, QKeySequence());
    EXPECT_EQ(notifications, 1);
    auto defaultEvent = keyPress(Qt::Key_Space);
    EXPECT_FALSE(bindings.matches(&defaultEvent, id));
    bindings.resetToDefaults();
}

TEST(ShortcutBindingsContractTest, InvalidJsonValueDoesNotPartiallyReplaceOverrides)
{
    auto& bindings = ShortcutBindings::instance();
    bindings.resetToDefaults();
    const auto playbackId = ShortcutId::PlaybackToggle;
    const auto undoId = ShortcutId::Undo;
    const QKeySequence playbackCustom(Qt::CTRL | Qt::Key_P);
    const QKeySequence undoCustom(Qt::ALT | Qt::Key_U);
    bindings.setShortcut(playbackId, playbackCustom);
    bindings.setShortcut(undoId, undoCustom);

    const QJsonObject malformed{
        {QStringLiteral("shortcuts"),
         QJsonObject{
             {QStringLiteral("PlaybackToggle"), QStringLiteral("Alt+F8")},
             {QStringLiteral("Undo"), 17}}}};

    EXPECT_FALSE(bindings.loadFromJson(malformed));
    EXPECT_EQ(bindings.shortcut(playbackId), playbackCustom);
    EXPECT_EQ(bindings.shortcut(undoId), undoCustom);
    bindings.resetToDefaults();
}
