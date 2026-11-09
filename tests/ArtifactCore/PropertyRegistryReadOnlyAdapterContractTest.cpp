#include <gtest/gtest.h>

#include <QString>
#include <QVariant>

#include <algorithm>

import Property;
import Property.Abstract;
import Property.Group;
import Property.Types;
import Memory.SharedPtr;

using namespace ArtifactCore;

namespace {
class RegisteredPropertyOwner {
public:
    explicit RegisteredPropertyOwner(
        const QString& ownerPath =
            QStringLiteral("contract.read-only-adapter.composition"),
        bool readOnly = true)
        : ownerPath_(ownerPath)
    {
        group_ = makeShared<PropertyGroup>(QStringLiteral("Transform"));

        opacity_ = makeShared<AbstractProperty>();
        opacity_->setName(QStringLiteral("opacity"));
        opacity_->setType(PropertyType::Float);
        opacity_->setValue(0.75);
        group_->addProperty(opacity_);

        auto visible = makeShared<AbstractProperty>();
        visible->setName(QStringLiteral("visible"));
        visible->setType(PropertyType::Boolean);
        visible->setValue(true);
        group_->addProperty(visible);

        globalPropertyRegistry().registerOwnerSnapshot(
            ownerPath_, QStringLiteral("Contract Composition"),
            QStringLiteral("Composition"), *group_, readOnly);
    }

    ~RegisteredPropertyOwner()
    {
        globalPropertyRegistry().unregisterOwner(ownerPath_);
    }

    const QString& ownerPath() const { return ownerPath_; }
    const SharedPtr<AbstractProperty>& opacity() const { return opacity_; }

private:
    QString ownerPath_;
    SharedPtr<PropertyGroup> group_;
    SharedPtr<AbstractProperty> opacity_;
};
}

TEST(PropertyRegistryReadOnlyAdapterContractTest,
     NestedOwnerAndPropertyQueriesReturnStableSnapshotsAndEnumerateConsistently)
{
    RegisteredPropertyOwner owner;
    const QString propertyPath = owner.ownerPath() + QStringLiteral(".opacity");

    const auto ownerSnapshot = PropertyRegistryReadOnlyAdapter::queryOwner(owner.ownerPath());
    ASSERT_TRUE(ownerSnapshot.isValid);
    EXPECT_EQ(ownerSnapshot.ownerPath, owner.ownerPath());
    EXPECT_EQ(ownerSnapshot.displayName, QStringLiteral("Contract Composition"));
    EXPECT_EQ(ownerSnapshot.ownerType, QStringLiteral("Composition"));
    EXPECT_TRUE(ownerSnapshot.readOnly);
    EXPECT_EQ(ownerSnapshot.propertyCount, 2);

    const auto propertySnapshot = PropertyRegistryReadOnlyAdapter::queryProperty(propertyPath);
    ASSERT_TRUE(propertySnapshot.isValid);
    EXPECT_EQ(propertySnapshot.ownerPath, owner.ownerPath());
    EXPECT_EQ(propertySnapshot.propertyName, QStringLiteral("opacity"));
    EXPECT_EQ(propertySnapshot.propertyType, QStringLiteral("Float"));
    EXPECT_DOUBLE_EQ(propertySnapshot.currentValue.toDouble(), 0.75);
    EXPECT_TRUE(propertySnapshot.isReadOnly);

    owner.opacity()->setValue(0.25);
    EXPECT_DOUBLE_EQ(propertySnapshot.currentValue.toDouble(), 0.75);
    EXPECT_DOUBLE_EQ(propertySnapshot.property->getValue().toDouble(), 0.25);

    const auto allOwners = PropertyRegistryReadOnlyAdapter::queryAllOwners();
    ASSERT_EQ(allOwners.size(), 1);
    EXPECT_EQ(allOwners.front().ownerPath, owner.ownerPath());
    const auto allProperties = PropertyRegistryReadOnlyAdapter::queryAllProperties();
    ASSERT_EQ(allProperties.size(), 2);
    EXPECT_TRUE(allProperties[0].isReadOnly);
    EXPECT_TRUE(allProperties[1].isReadOnly);
    EXPECT_TRUE(PropertyRegistryReadOnlyAdapter::listOwners().contains(owner.ownerPath()));

    const auto missing = PropertyRegistryReadOnlyAdapter::queryProperty(
        owner.ownerPath() + QStringLiteral(".missing"));
    EXPECT_FALSE(missing.isValid);
    EXPECT_EQ(missing.property, nullptr);
    EXPECT_FALSE(PropertyRegistryReadOnlyAdapter::queryOwner(
                     QStringLiteral("contract.read-only-adapter.unknown")).isValid);
}

TEST(PropertyRegistryReadOnlyAdapterContractTest,
     UnregisteredOwnerDisappearsFromQueriesAndEnumeration)
{
    QString ownerPath;
    {
        RegisteredPropertyOwner owner;
        ownerPath = owner.ownerPath();
        ASSERT_TRUE(PropertyRegistryReadOnlyAdapter::queryOwner(ownerPath).isValid);
        ASSERT_TRUE(PropertyRegistryReadOnlyAdapter::queryProperty(
            ownerPath + QStringLiteral(".opacity")).isValid);
    }

    EXPECT_FALSE(PropertyRegistryReadOnlyAdapter::queryOwner(ownerPath).isValid);
    EXPECT_FALSE(PropertyRegistryReadOnlyAdapter::queryProperty(
                     ownerPath + QStringLiteral(".opacity")).isValid);
    EXPECT_FALSE(PropertyRegistryReadOnlyAdapter::listOwners().contains(ownerPath));

    const auto owners = PropertyRegistryReadOnlyAdapter::queryAllOwners();
    EXPECT_TRUE(std::none_of(owners.cbegin(), owners.cend(),
        [&ownerPath](const auto& owner) { return owner.ownerPath == ownerPath; }));
    const auto properties = PropertyRegistryReadOnlyAdapter::queryAllProperties();
    EXPECT_TRUE(std::none_of(properties.cbegin(), properties.cend(),
        [&ownerPath](const auto& property) { return property.ownerPath == ownerPath; }));
}

TEST(PropertyRegistryReadOnlyAdapterContractTest,
     PreviouslyReturnedPropertySnapshotKeepsItsValueAndHandleAfterOwnerRemoval)
{
    const QString ownerPath =
        QStringLiteral("contract.read-only-adapter.detached-snapshot");
    PropertyRegistryReadOnlyAdapter::PropertyValueSnapshot snapshot;
    {
        RegisteredPropertyOwner owner(ownerPath);
        ASSERT_TRUE(PropertyRegistryReadOnlyAdapter::queryOwner(ownerPath).isValid);
        snapshot = PropertyRegistryReadOnlyAdapter::queryProperty(
            ownerPath + QStringLiteral(".opacity"));
        ASSERT_TRUE(snapshot.isValid);
        ASSERT_NE(snapshot.property, nullptr);
        EXPECT_TRUE(snapshot.isReadOnly);
        EXPECT_DOUBLE_EQ(snapshot.currentValue.toDouble(), 0.75);

        owner.opacity()->setValue(0.25);
        EXPECT_DOUBLE_EQ(snapshot.currentValue.toDouble(), 0.75);
        EXPECT_DOUBLE_EQ(snapshot.property->getValue().toDouble(), 0.25);
    }

    EXPECT_FALSE(PropertyRegistryReadOnlyAdapter::queryOwner(ownerPath).isValid);
    EXPECT_FALSE(PropertyRegistryReadOnlyAdapter::queryProperty(
                     ownerPath + QStringLiteral(".opacity")).isValid);
    ASSERT_NE(snapshot.property, nullptr);
    EXPECT_TRUE(snapshot.isReadOnly);
    EXPECT_DOUBLE_EQ(snapshot.currentValue.toDouble(), 0.75);
    EXPECT_DOUBLE_EQ(snapshot.property->getValue().toDouble(), 0.25);
}

TEST(PropertyRegistryReadOnlyAdapterContractTest,
     ReregisteringOwnerPathDoesNotRetargetPreviouslyReturnedSnapshots)
{
    const QString ownerPath =
        QStringLiteral("contract.read-only-adapter.reused-owner-path");
    PropertyRegistryReadOnlyAdapter::PropertyValueSnapshot originalSnapshot;
    {
        RegisteredPropertyOwner originalOwner(ownerPath);
        originalSnapshot = PropertyRegistryReadOnlyAdapter::queryProperty(
            ownerPath + QStringLiteral(".opacity"));
        ASSERT_TRUE(originalSnapshot.isValid);
        originalOwner.opacity()->setValue(0.25);
    }

    EXPECT_FALSE(PropertyRegistryReadOnlyAdapter::queryOwner(ownerPath).isValid);
    ASSERT_NE(originalSnapshot.property, nullptr);
    EXPECT_DOUBLE_EQ(originalSnapshot.property->getValue().toDouble(), 0.25);

    PropertyRegistryReadOnlyAdapter::PropertyValueSnapshot replacementSnapshot;
    {
        RegisteredPropertyOwner replacementOwner(ownerPath);
        replacementOwner.opacity()->setValue(0.5);
        replacementSnapshot = PropertyRegistryReadOnlyAdapter::queryProperty(
            ownerPath + QStringLiteral(".opacity"));
        ASSERT_TRUE(replacementSnapshot.isValid);
        ASSERT_NE(replacementSnapshot.property, nullptr);
        EXPECT_DOUBLE_EQ(replacementSnapshot.currentValue.toDouble(), 0.5);

        replacementOwner.opacity()->setValue(0.125);
        EXPECT_DOUBLE_EQ(replacementSnapshot.property->getValue().toDouble(), 0.125);
        EXPECT_DOUBLE_EQ(replacementSnapshot.currentValue.toDouble(), 0.5);
        EXPECT_DOUBLE_EQ(originalSnapshot.property->getValue().toDouble(), 0.25);
    }

    EXPECT_FALSE(PropertyRegistryReadOnlyAdapter::queryProperty(
                     ownerPath + QStringLiteral(".opacity")).isValid);
    EXPECT_DOUBLE_EQ(originalSnapshot.property->getValue().toDouble(), 0.25);
    EXPECT_DOUBLE_EQ(replacementSnapshot.property->getValue().toDouble(), 0.125);
}

TEST(PropertyRegistryReadOnlyAdapterContractTest,
     WritableOwnerStateMatchesAcrossOwnerSingleAndEnumeratedQueries)
{
    const QString ownerPath =
        QStringLiteral("contract.read-only-adapter.writable-owner");
    RegisteredPropertyOwner owner(ownerPath, false);

    const auto ownerSnapshot = PropertyRegistryReadOnlyAdapter::queryOwner(ownerPath);
    ASSERT_TRUE(ownerSnapshot.isValid);
    EXPECT_FALSE(ownerSnapshot.readOnly);

    const auto singleProperty = PropertyRegistryReadOnlyAdapter::queryProperty(
        ownerPath + QStringLiteral(".opacity"));
    ASSERT_TRUE(singleProperty.isValid);
    EXPECT_FALSE(singleProperty.isReadOnly);

    const auto allProperties = PropertyRegistryReadOnlyAdapter::queryAllProperties();
    const auto enumerated = std::find_if(allProperties.cbegin(), allProperties.cend(),
        [&ownerPath](const auto& property) {
            return property.ownerPath == ownerPath &&
                   property.propertyName == QStringLiteral("opacity");
        });
    ASSERT_NE(enumerated, allProperties.cend());
    EXPECT_EQ(enumerated->currentValue, singleProperty.currentValue);
    EXPECT_EQ(enumerated->isReadOnly, singleProperty.isReadOnly);
}
