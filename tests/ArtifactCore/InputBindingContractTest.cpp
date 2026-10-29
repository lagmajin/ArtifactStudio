#include <gtest/gtest.h>

#include <QString>
#include <Qt>

import Input.Operator;

namespace {

using ArtifactCore::InputBinding;
using ArtifactCore::InputEvent;

InputEvent keyEvent(InputEvent::Type type, int key,
                    InputEvent::Modifiers modifiers = {})
{
    InputEvent event;
    event.type = type;
    event.keyCode = key;
    event.modifiers = modifiers;
    return event;
}

} // namespace

TEST(InputBindingContractTest, MatchesOnlyKeyDownWithConfiguredKey)
{
    InputBinding binding(QStringLiteral("contract-test"));
    binding.setKeyCode(Qt::Key_K);

    EXPECT_TRUE(binding.matches(keyEvent(InputEvent::Type::KeyDown, Qt::Key_K)));
    EXPECT_FALSE(binding.matches(keyEvent(InputEvent::Type::KeyUp, Qt::Key_K)));
    EXPECT_FALSE(binding.matches(keyEvent(InputEvent::Type::KeyDown, Qt::Key_J)));
    EXPECT_FALSE(binding.matches(keyEvent(InputEvent::Type::MouseDown, Qt::Key_K)));
}

TEST(InputBindingContractTest, EnforcesRequiredAndForbiddenModifierMasks)
{
    InputBinding binding(QStringLiteral("modifier-contract-test"));
    binding.setKeyCode(Qt::Key_S);
    binding.setRequiredModifiers(InputEvent::ModifierKey::LCtrl);
    binding.setForbiddenModifiers(InputEvent::ModifierKey::LShift);

    EXPECT_FALSE(binding.matches(keyEvent(InputEvent::Type::KeyDown, Qt::Key_S)));
    EXPECT_TRUE(binding.matches(keyEvent(
        InputEvent::Type::KeyDown, Qt::Key_S,
        InputEvent::ModifierKey::LCtrl)));
    EXPECT_TRUE(binding.matches(keyEvent(
        InputEvent::Type::KeyDown, Qt::Key_S,
        InputEvent::ModifierKey::LCtrl | InputEvent::ModifierKey::RAlt)));
    EXPECT_FALSE(binding.matches(keyEvent(
        InputEvent::Type::KeyDown, Qt::Key_S,
        InputEvent::ModifierKey::LCtrl | InputEvent::ModifierKey::LShift)));
}

TEST(InputBindingContractTest, RejectsWrongSideModifierWhenThatSideIsRequired)
{
    InputBinding binding(QStringLiteral("modifier-side-contract-test"));
    binding.setKeyCode(Qt::Key_Z);
    binding.setRequiredModifiers(InputEvent::ModifierKey::LCtrl);

    EXPECT_FALSE(binding.matches(keyEvent(
        InputEvent::Type::KeyDown, Qt::Key_Z,
        InputEvent::ModifierKey::RCtrl)));
    EXPECT_TRUE(binding.matches(keyEvent(
        InputEvent::Type::KeyDown, Qt::Key_Z,
        InputEvent::ModifierKey::LCtrl)));
}
