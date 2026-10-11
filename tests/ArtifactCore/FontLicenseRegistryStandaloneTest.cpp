#include <gtest/gtest.h>

#include <QString>
#include <QJsonObject>
#include <QJsonArray>

import Font.Descriptor;
import Font.LicenseRegistry;

using namespace ArtifactCore;

TEST(FontLicenseRegistryStandaloneTest, DefaultDescriptorIsInvalid)
{
    const FontDescriptor descriptor;
    EXPECT_FALSE(descriptor.isValid());
    EXPECT_TRUE(descriptor.family.isEmpty());
    EXPECT_TRUE(descriptor.fullPath.isEmpty());
}

TEST(FontLicenseRegistryStandaloneTest, FamilyNameAloneMakesDescriptorValid)
{
    FontDescriptor descriptor;
    descriptor.family = QStringLiteral("Installed Sans");
    EXPECT_TRUE(descriptor.isValid());
}

TEST(FontLicenseRegistryStandaloneTest, WhitespaceFamilyIsInvalidEvenWithPath)
{
    FontDescriptor descriptor;
    descriptor.family = QStringLiteral(" \t ");
    descriptor.fullPath = QStringLiteral("/fonts/font.ttf");
    EXPECT_FALSE(descriptor.isValid());
}

TEST(FontLicenseRegistryStandaloneTest, DescriptorValidityIgnoresOptionalStyleMetadata)
{
    FontDescriptor descriptor;
    descriptor.family = QStringLiteral("Installed Sans");
    descriptor.style = QStringLiteral("Bold Italic");
    descriptor.isFixedPitch = true;
    descriptor.weight = 75;
    descriptor.italic = true;
    EXPECT_TRUE(descriptor.isValid());
}

TEST(FontLicenseRegistryStandaloneTest, NewRegistryHasNoEntries)
{
    const FontLicenseRegistry registry;
    EXPECT_TRUE(registry.entries().isEmpty());
    EXPECT_FALSE(registry.contains(QStringLiteral("Family")));
    EXPECT_FALSE(registry.isAttested(QStringLiteral("Family")));
}

TEST(FontLicenseRegistryStandaloneTest, SetTrimsFamilyAndStyleBeforeIndexing)
{
    FontLicenseRegistry registry;
    FontLicenseEntry entry;
    entry.family = QStringLiteral("  Example Sans  ");
    entry.style = QStringLiteral("  Bold  ");
    entry.attestation = QStringLiteral("Reviewed");
    registry.set(entry);

    EXPECT_TRUE(registry.contains(QStringLiteral("Example Sans"), QStringLiteral("Bold")));
    EXPECT_TRUE(registry.contains(QStringLiteral(" Example Sans "), QStringLiteral(" Bold ")));
    EXPECT_EQ(registry.entry(QStringLiteral("Example Sans"), QStringLiteral("Bold")).attestation,
              QStringLiteral("Reviewed"));
}

TEST(FontLicenseRegistryStandaloneTest, EmptyFamilyEntriesAreIgnored)
{
    FontLicenseRegistry registry;
    FontLicenseEntry entry;
    entry.family = QStringLiteral(" \t ");
    entry.attestation = QStringLiteral("Reviewed");
    registry.set(entry);
    EXPECT_TRUE(registry.entries().isEmpty());
}

TEST(FontLicenseRegistryStandaloneTest, MissingEntryReturnsDefaultValue)
{
    const FontLicenseRegistry registry;
    const auto entry = registry.entry(QStringLiteral("Missing"), QStringLiteral("Bold"));
    EXPECT_TRUE(entry.family.isEmpty());
    EXPECT_TRUE(entry.style.isEmpty());
    EXPECT_TRUE(entry.licenseFilePath.isEmpty());
    EXPECT_TRUE(entry.attestation.isEmpty());
}

TEST(FontLicenseRegistryStandaloneTest, ExactStyleEntryTakesPrecedenceOverFamilyEntry)
{
    FontLicenseRegistry registry;
    FontLicenseEntry familyEntry;
    familyEntry.family = QStringLiteral("Example");
    familyEntry.attestation = QStringLiteral("Family review");
    registry.set(familyEntry);
    FontLicenseEntry styleEntry;
    styleEntry.family = QStringLiteral("Example");
    styleEntry.style = QStringLiteral("Bold");
    styleEntry.licenseFilePath = QStringLiteral("/licenses/bold.txt");
    registry.set(styleEntry);

    EXPECT_EQ(registry.entry(QStringLiteral("Example"), QStringLiteral("Bold")).licenseFilePath,
              QStringLiteral("/licenses/bold.txt"));
}

TEST(FontLicenseRegistryStandaloneTest, StyleLookupFallsBackToFamilyEntry)
{
    FontLicenseRegistry registry;
    FontLicenseEntry familyEntry;
    familyEntry.family = QStringLiteral("Example");
    familyEntry.attestation = QStringLiteral("Family reviewed");
    registry.set(familyEntry);

    const auto found = registry.entry(QStringLiteral("Example"), QStringLiteral("Light"));
    EXPECT_EQ(found.attestation, QStringLiteral("Family reviewed"));
    EXPECT_TRUE(registry.isAttested(QStringLiteral("Example"), QStringLiteral("Light")));
}

TEST(FontLicenseRegistryStandaloneTest, LicensePathOrAttestationCountsAsAttested)
{
    FontLicenseRegistry registry;
    FontLicenseEntry byPath;
    byPath.family = QStringLiteral("Path Font");
    byPath.licenseFilePath = QStringLiteral("/license.txt");
    registry.set(byPath);
    FontLicenseEntry byAttestation;
    byAttestation.family = QStringLiteral("Reviewed Font");
    byAttestation.attestation = QStringLiteral("Reviewed by team");
    registry.set(byAttestation);
    FontLicenseEntry noteOnly;
    noteOnly.family = QStringLiteral("Note Font");
    noteOnly.note = QStringLiteral("See legal");
    registry.set(noteOnly);

    EXPECT_TRUE(registry.isAttested(QStringLiteral("Path Font")));
    EXPECT_TRUE(registry.isAttested(QStringLiteral("Reviewed Font")));
    EXPECT_FALSE(registry.isAttested(QStringLiteral("Note Font")));
}

TEST(FontLicenseRegistryStandaloneTest, UpdatingSameKeyReplacesStoredEntry)
{
    FontLicenseRegistry registry;
    FontLicenseEntry entry;
    entry.family = QStringLiteral("Example");
    entry.style = QStringLiteral("Regular");
    entry.note = QStringLiteral("old");
    registry.set(entry);
    entry.note = QStringLiteral("new");
    registry.set(entry);

    EXPECT_EQ(registry.entries().size(), 1);
    EXPECT_EQ(registry.entry(QStringLiteral("Example"), QStringLiteral("Regular")).note,
              QStringLiteral("new"));
}

TEST(FontLicenseRegistryStandaloneTest, RemovingStyleDoesNotRemoveFamilyFallback)
{
    FontLicenseRegistry registry;
    FontLicenseEntry familyEntry;
    familyEntry.family = QStringLiteral("Example");
    familyEntry.attestation = QStringLiteral("Family level");
    registry.set(familyEntry);
    FontLicenseEntry boldEntry;
    boldEntry.family = QStringLiteral("Example");
    boldEntry.style = QStringLiteral("Bold");
    boldEntry.attestation = QStringLiteral("Bold level");
    registry.set(boldEntry);

    registry.remove(QStringLiteral("Example"), QStringLiteral("Bold"));

    EXPECT_TRUE(registry.contains(QStringLiteral("Example"), QStringLiteral("Bold")));
    EXPECT_TRUE(registry.isAttested(QStringLiteral("Example"), QStringLiteral("Bold")));
    EXPECT_EQ(registry.entry(QStringLiteral("Example"), QStringLiteral("Bold")).attestation,
              QStringLiteral("Family level"));
}

TEST(FontLicenseRegistryStandaloneTest, JsonRoundTripPreservesFieldsAndNormalizedKeys)
{
    FontLicenseRegistry original;
    FontLicenseEntry entry;
    entry.family = QStringLiteral("  Example  ");
    entry.style = QStringLiteral("  Medium  ");
    entry.licenseFilePath = QStringLiteral("/licenses/example.txt");
    entry.attestation = QStringLiteral("Reviewed");
    entry.note = QStringLiteral("Internal font");
    original.set(entry);

    FontLicenseRegistry restored;
    restored.fromJson(original.toJson());

    const auto result = restored.entry(QStringLiteral("Example"), QStringLiteral("Medium"));
    EXPECT_EQ(result.family, QStringLiteral("Example"));
    EXPECT_EQ(result.style, QStringLiteral("Medium"));
    EXPECT_EQ(result.licenseFilePath, QStringLiteral("/licenses/example.txt"));
    EXPECT_EQ(result.attestation, QStringLiteral("Reviewed"));
    EXPECT_EQ(result.note, QStringLiteral("Internal font"));
}

TEST(FontLicenseRegistryStandaloneTest, JsonImportClearsPreviousEntriesAndSkipsNonObjects)
{
    FontLicenseRegistry registry;
    FontLicenseEntry oldEntry;
    oldEntry.family = QStringLiteral("Old");
    registry.set(oldEntry);
    const QJsonObject valid{
        {QStringLiteral("ignored"), QJsonArray{1, 2, 3}},
        {QStringLiteral("entry"), QJsonObject{
            {QStringLiteral("family"), QStringLiteral("New")},
            {QStringLiteral("style"), QStringLiteral("Regular")},
            {QStringLiteral("attestation"), QStringLiteral("Accepted")},
        }},
    };

    registry.fromJson(valid);

    EXPECT_FALSE(registry.contains(QStringLiteral("Old")));
    EXPECT_TRUE(registry.contains(QStringLiteral("New"), QStringLiteral("Regular")));
    EXPECT_EQ(registry.entries().size(), 1);
}

TEST(FontLicenseRegistryStandaloneTest, RemovingFamilyEntryAndClearWork)
{
    FontLicenseRegistry registry;
    FontLicenseEntry first;
    first.family = QStringLiteral("First");
    registry.set(first);
    FontLicenseEntry second;
    second.family = QStringLiteral("Second");
    registry.set(second);

    registry.remove(QStringLiteral("First"));
    EXPECT_FALSE(registry.contains(QStringLiteral("First")));
    EXPECT_TRUE(registry.contains(QStringLiteral("Second")));
    registry.clear();
    EXPECT_TRUE(registry.entries().isEmpty());
}

TEST(FontLicenseRegistryStandaloneTest, RemoveNormalizesFamilyAndStyleArguments)
{
    FontLicenseRegistry registry;
    FontLicenseEntry entry;
    entry.family = QStringLiteral("Family");
    entry.style = QStringLiteral("Bold");
    registry.set(entry);

    registry.remove(QStringLiteral(" Family "), QStringLiteral(" Bold "));

    EXPECT_TRUE(registry.entries().isEmpty());
}

TEST(FontLicenseRegistryStandaloneTest, JsonUsesNormalizedCompositeFamilyStyleKey)
{
    FontLicenseRegistry registry;
    FontLicenseEntry entry;
    entry.family = QStringLiteral(" Family ");
    entry.style = QStringLiteral(" Regular ");
    registry.set(entry);

    const QJsonObject json = registry.toJson();

    ASSERT_EQ(json.size(), 1);
    EXPECT_TRUE(json.contains(QStringLiteral("Family|Regular")));
    EXPECT_EQ(json.value(QStringLiteral("Family|Regular")).toObject()
                  .value(QStringLiteral("family")).toString(), QStringLiteral("Family"));
}

TEST(FontLicenseRegistryStandaloneTest, JsonImportSkipsEntriesWithoutValidFamily)
{
    FontLicenseRegistry registry;
    const QJsonObject json{
        {QStringLiteral("blank"), QJsonObject{
            {QStringLiteral("family"), QStringLiteral("  ")},
            {QStringLiteral("style"), QStringLiteral("Regular")},
        }},
        {QStringLiteral("wrong type"), QJsonObject{
            {QStringLiteral("family"), 42},
            {QStringLiteral("style"), QStringLiteral("Bold")},
        }},
    };

    registry.fromJson(json);

    EXPECT_TRUE(registry.entries().isEmpty());
}

TEST(FontLicenseRegistryStandaloneTest, ImportingEmptyJsonClearsRegistry)
{
    FontLicenseRegistry registry;
    FontLicenseEntry entry;
    entry.family = QStringLiteral("Existing");
    registry.set(entry);

    registry.fromJson(QJsonObject{});

    EXPECT_TRUE(registry.entries().isEmpty());
}

TEST(FontLicenseRegistryStandaloneTest, CopyingRegistryKeepsIndependentLogicalContents)
{
    FontLicenseRegistry original;
    FontLicenseEntry entry;
    entry.family = QStringLiteral("Family");
    entry.note = QStringLiteral("original");
    original.set(entry);
    FontLicenseRegistry copy = original;
    entry.note = QStringLiteral("copy");
    copy.set(entry);
    FontLicenseEntry added;
    added.family = QStringLiteral("Added");
    copy.set(added);

    EXPECT_EQ(original.entry(QStringLiteral("Family")).note, QStringLiteral("original"));
    EXPECT_FALSE(original.contains(QStringLiteral("Added")));
    EXPECT_EQ(copy.entry(QStringLiteral("Family")).note, QStringLiteral("copy"));
}

TEST(FontLicenseRegistryStandaloneTest, EntriesReturnsDetachedValueSnapshot)
{
    FontLicenseRegistry registry;
    FontLicenseEntry entry;
    entry.family = QStringLiteral("Family");
    entry.note = QStringLiteral("stored");
    registry.set(entry);

    auto snapshot = registry.entries();
    ASSERT_EQ(snapshot.size(), 1);
    snapshot[0].note = QStringLiteral("modified snapshot");

    EXPECT_EQ(registry.entry(QStringLiteral("Family")).note, QStringLiteral("stored"));
}
