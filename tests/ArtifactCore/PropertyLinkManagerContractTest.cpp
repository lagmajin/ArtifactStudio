#include <gtest/gtest.h>

#include <QString>
#include <QVariant>

import Property;
import Property.LinkManager;

using namespace ArtifactCore;

namespace {
class LinkManagerReset {
public:
    ~LinkManagerReset() { PropertyLinkManager::instance().clear(); }
};
}

TEST(PropertyLinkManagerContractTest,
     UpdatesAllSupportedMappingsAndIgnoresInvalidOrNullEndpoints)
{
    LinkManagerReset reset;
    auto& manager = PropertyLinkManager::instance();
    manager.clear();

    Property source(QStringLiteral("source"), 0.25f);
    Property directTarget(QStringLiteral("direct"), 0.0f);
    Property secondDirectTarget(QStringLiteral("direct-second"), 0.0f);
    Property inverseTarget(QStringLiteral("inverse"), 0.0f);
    Property scaleTarget(QStringLiteral("scale"), 0.0f);
    Property offsetTarget(QStringLiteral("offset"), 0.0f);
    Property boolSource(QStringLiteral("enabled"), true);
    Property boolTarget(QStringLiteral("disabled"), true);
    Property intSource(QStringLiteral("count"), 7);
    Property customTarget(QStringLiteral("mapped-count"), 0);
    Property stringSource(QStringLiteral("label"), QStringLiteral("not-a-number"));
    Property invalidTarget(QStringLiteral("invalid-inverse"), 42.0f);
    Property nullSourceTarget(QStringLiteral("null-source-target"), 9.0f);
    Property emptyCustomTarget(QStringLiteral("empty-custom-target"), 17.0f);

    manager.addLink(source.get(), directTarget.get(), LinkType::Direct);
    manager.addLink(source.get(), secondDirectTarget.get(), LinkType::Direct);
    manager.addLink(source.get(), inverseTarget.get(), LinkType::Inverse);
    PropertyLink scaled;
    scaled.source = source.get();
    scaled.target = scaleTarget.get();
    scaled.type = LinkType::Scale;
    scaled.multiplier = 3.5f;
    manager.addLink(scaled);
    PropertyLink offset;
    offset.source = source.get();
    offset.target = offsetTarget.get();
    offset.type = LinkType::Offset;
    offset.offset = 4.25f;
    manager.addLink(offset);
    manager.addLink(boolSource.get(), boolTarget.get(), LinkType::Inverse);
    PropertyLink custom;
    custom.source = intSource.get();
    custom.target = customTarget.get();
    custom.type = LinkType::Custom;
    custom.customMapper = [](const QVariant& value) {
        return QVariant(value.toInt() * 3);
    };
    manager.addLink(custom);
    manager.addLink(stringSource.get(), invalidTarget.get(), LinkType::Inverse);
    manager.addLink(source.get(), nullptr, LinkType::Direct);
    manager.addLink(nullptr, nullSourceTarget.get(), LinkType::Direct);
    PropertyLink emptyCustom;
    emptyCustom.source = source.get();
    emptyCustom.target = emptyCustomTarget.get();
    emptyCustom.type = LinkType::Custom;
    manager.addLink(emptyCustom);

    EXPECT_EQ(manager.links().size(), 11u);
    manager.updateTargets(source.get());
    manager.updateTargets(boolSource.get());
    manager.updateTargets(intSource.get());
    manager.updateTargets(stringSource.get());
    manager.updateTargets(nullptr);

    EXPECT_FLOAT_EQ(directTarget->getValue().toFloat(), 0.25f);
    EXPECT_FLOAT_EQ(secondDirectTarget->getValue().toFloat(), 0.25f);
    EXPECT_FLOAT_EQ(inverseTarget->getValue().toFloat(), 0.75f);
    EXPECT_FLOAT_EQ(scaleTarget->getValue().toFloat(), 0.875f);
    EXPECT_FLOAT_EQ(offsetTarget->getValue().toFloat(), 4.5f);
    EXPECT_FALSE(boolTarget->getValue().toBool());
    EXPECT_EQ(customTarget->getValue().toInt(), 21);
    EXPECT_FLOAT_EQ(invalidTarget->getValue().toFloat(), 42.0f);
    EXPECT_FLOAT_EQ(nullSourceTarget->getValue().toFloat(), 9.0f);
    EXPECT_FLOAT_EQ(emptyCustomTarget->getValue().toFloat(), 17.0f);

    manager.clear();
    EXPECT_TRUE(manager.links().empty());
}

TEST(PropertyLinkManagerContractTest,
     CyclicLinksPropagateOnlyWhenTheirSourceIsExplicitlyUpdated)
{
    LinkManagerReset reset;
    auto& manager = PropertyLinkManager::instance();
    manager.clear();

    Property first(QStringLiteral("first"), 2.0f);
    Property second(QStringLiteral("second"), 10.0f);
    Property third(QStringLiteral("third"), -4.0f);
    manager.addLink(first.get(), second.get(), LinkType::Direct);
    manager.addLink(second.get(), third.get(), LinkType::Direct);
    manager.addLink(third.get(), first.get(), LinkType::Direct);

    manager.updateTargets(first.get());
    EXPECT_FLOAT_EQ(second->getValue().toFloat(), 2.0f);
    EXPECT_FLOAT_EQ(third->getValue().toFloat(), -4.0f);
    EXPECT_FLOAT_EQ(first->getValue().toFloat(), 2.0f);

    manager.updateTargets(second.get());
    EXPECT_FLOAT_EQ(third->getValue().toFloat(), 2.0f);
    EXPECT_FLOAT_EQ(first->getValue().toFloat(), 2.0f);

    first->setValue(7.0f);
    manager.updateTargets(first.get());
    EXPECT_FLOAT_EQ(second->getValue().toFloat(), 7.0f);
    EXPECT_FLOAT_EQ(third->getValue().toFloat(), 2.0f);
    EXPECT_FLOAT_EQ(first->getValue().toFloat(), 7.0f);
}

TEST(PropertyLinkManagerContractTest,
     IntegerInverseUsesNumericNegation)
{
    LinkManagerReset reset;
    auto& manager = PropertyLinkManager::instance();
    manager.clear();

    Property source(QStringLiteral("integer-source"), 7);
    Property target(QStringLiteral("integer-target"), 99);
    manager.addLink(source.get(), target.get(), LinkType::Inverse);

    manager.updateTargets(source.get());

    EXPECT_EQ(target->getValue().toInt(), -7);
}

TEST(PropertyLinkManagerContractTest,
     FanOutUsesStableSourceSnapshotWhenMapperMutatesSource)
{
    LinkManagerReset reset;
    auto& manager = PropertyLinkManager::instance();
    manager.clear();

    Property source(QStringLiteral("source"), 0.25f);
    Property mapperTarget(QStringLiteral("mapper-target"), 0.0f);
    Property laterTarget(QStringLiteral("later-target"), 0.0f);
    PropertyLink mutatingMapper;
    mutatingMapper.source = source.get();
    mutatingMapper.target = mapperTarget.get();
    mutatingMapper.type = LinkType::Custom;
    mutatingMapper.customMapper = [&source](const QVariant& value) {
        source->setValue(0.75f);
        return value;
    };
    manager.addLink(mutatingMapper);
    manager.addLink(source.get(), laterTarget.get(), LinkType::Direct);

    manager.updateTargets(source.get());

    EXPECT_FLOAT_EQ(source->getValue().toFloat(), 0.75f);
    EXPECT_FLOAT_EQ(mapperTarget->getValue().toFloat(), 0.25f);
    EXPECT_FLOAT_EQ(laterTarget->getValue().toFloat(), 0.25f);

    manager.updateTargets(source.get());

    EXPECT_FLOAT_EQ(mapperTarget->getValue().toFloat(), 0.75f);
    EXPECT_FLOAT_EQ(laterTarget->getValue().toFloat(), 0.75f);
}

TEST(PropertyLinkManagerContractTest,
     FanOutUsesStableLinkSnapshotWhenMapperAddsAnotherLink)
{
    LinkManagerReset reset;
    auto& manager = PropertyLinkManager::instance();
    manager.clear();

    Property source(QStringLiteral("source"), 0.375f);
    Property mapperTarget(QStringLiteral("mapper-target"), 0.0f);
    Property existingTarget(QStringLiteral("existing-target"), 0.0f);
    Property callbackAddedTarget(QStringLiteral("callback-added-target"), -1.0f);
    bool addedLink = false;
    PropertyLink mutatingMapper;
    mutatingMapper.source = source.get();
    mutatingMapper.target = mapperTarget.get();
    mutatingMapper.type = LinkType::Custom;
    mutatingMapper.customMapper = [&](const QVariant& value) {
        if (!addedLink) {
            addedLink = true;
            manager.addLink(source.get(), callbackAddedTarget.get(), LinkType::Direct);
        }
        return value;
    };
    manager.addLink(mutatingMapper);
    manager.addLink(source.get(), existingTarget.get(), LinkType::Direct);

    manager.updateTargets(source.get());

    EXPECT_FLOAT_EQ(mapperTarget->getValue().toFloat(), 0.375f);
    EXPECT_FLOAT_EQ(existingTarget->getValue().toFloat(), 0.375f);
    EXPECT_FLOAT_EQ(callbackAddedTarget->getValue().toFloat(), -1.0f)
        << "links added during mapping belong to the next update pass";

    manager.updateTargets(source.get());
    EXPECT_FLOAT_EQ(callbackAddedTarget->getValue().toFloat(), 0.375f);
}

TEST(PropertyLinkManagerContractTest,
     FanOutUsesStableLinkSnapshotWhenMapperClearsTheManager)
{
    LinkManagerReset reset;
    auto& manager = PropertyLinkManager::instance();
    manager.clear();

    Property source(QStringLiteral("source"), 0.625f);
    Property mapperTarget(QStringLiteral("mapper-target"), 0.0f);
    Property laterTarget(QStringLiteral("later-target"), -1.0f);
    PropertyLink clearingMapper;
    clearingMapper.source = source.get();
    clearingMapper.target = mapperTarget.get();
    clearingMapper.type = LinkType::Custom;
    clearingMapper.customMapper = [&manager](const QVariant& value) {
        manager.clear();
        return value;
    };
    manager.addLink(clearingMapper);
    manager.addLink(source.get(), laterTarget.get(), LinkType::Direct);

    manager.updateTargets(source.get());

    EXPECT_FLOAT_EQ(mapperTarget->getValue().toFloat(), 0.625f);
    EXPECT_FLOAT_EQ(laterTarget->getValue().toFloat(), 0.625f)
        << "clearing registrations during a callback does not invalidate the current pass";
    EXPECT_TRUE(manager.links().empty());

    source->setValue(0.875f);
    manager.updateTargets(source.get());
    EXPECT_FLOAT_EQ(mapperTarget->getValue().toFloat(), 0.625f);
    EXPECT_FLOAT_EQ(laterTarget->getValue().toFloat(), 0.625f)
        << "cleared registrations do not participate in later passes";
}
