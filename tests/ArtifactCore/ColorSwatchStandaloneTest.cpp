#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

import Color.Swatch;
import Color.Float;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
class TemporaryGpl {
public:
    TemporaryGpl()
    {
        static std::atomic_uint64_t nextId{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("artifact-color-swatch-" + std::to_string(stamp) + "-" +
             std::to_string(nextId.fetch_add(1)) + ".gpl");
    }

    ~TemporaryGpl()
    {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    const std::filesystem::path& path() const { return path_; }

    void write(const std::string& contents) const
    {
        std::ofstream file(path_, std::ios::binary | std::ios::trunc);
        file << contents;
        file.close();
        if (!file) throw std::runtime_error("failed to write temporary GPL palette");
    }

private:
    std::filesystem::path path_;
};
}

TEST(ColorSwatchStandaloneTest, DefaultPaletteHasDefaultNameAndNoEntries)
{
    const ColorSwatch palette;
    EXPECT_EQ(toStdString(palette.getName()), "New Palette");
    EXPECT_EQ(palette.count(), 0);
}

TEST(ColorSwatchStandaloneTest, ExplicitNameCanBeReadAndChanged)
{
    ColorSwatch palette(String("Warm tones"));
    EXPECT_EQ(toStdString(palette.getName()), "Warm tones");
    palette.setName(String("Cool tones"));
    EXPECT_EQ(toStdString(palette.getName()), "Cool tones");
}

TEST(ColorSwatchStandaloneTest, AddColorPreservesColorAndEntryName)
{
    ColorSwatch palette;
    palette.addColor(FloatColor(0.1f, 0.2f, 0.3f, 0.4f), String("sample"));
    ASSERT_EQ(palette.count(), 1);
    EXPECT_EQ(toStdString(palette.at(0).name), "sample");
    EXPECT_FLOAT_EQ(palette.at(0).color.r(), 0.1f);
    EXPECT_FLOAT_EQ(palette.at(0).color.g(), 0.2f);
    EXPECT_FLOAT_EQ(palette.at(0).color.b(), 0.3f);
    EXPECT_FLOAT_EQ(palette.at(0).color.a(), 0.4f);
}

TEST(ColorSwatchStandaloneTest, AddColorWithoutNameUsesEmptyName)
{
    ColorSwatch palette;
    palette.addColor(FloatColor(0.1f, 0.2f, 0.3f, 1.0f));
    ASSERT_EQ(palette.count(), 1);
    EXPECT_TRUE(palette.at(0).name.isEmpty());
}

TEST(ColorSwatchStandaloneTest, AddEntryAppendsAfterExistingEntries)
{
    ColorSwatch palette;
    palette.addColor(FloatColor(1.0f, 0.0f, 0.0f, 1.0f), String("red"));
    palette.addEntry(SwatchEntry(FloatColor(0.0f, 0.0f, 1.0f, 1.0f), String("blue")));
    ASSERT_EQ(palette.count(), 2);
    EXPECT_EQ(toStdString(palette.at(0).name), "red");
    EXPECT_EQ(toStdString(palette.at(1).name), "blue");
}

TEST(ColorSwatchStandaloneTest, OutOfRangeAccessReturnsEmptyEntry)
{
    ColorSwatch palette;
    palette.addColor(FloatColor(1.0f, 0.0f, 0.0f, 1.0f), String("red"));
    const auto& missing = palette.at(10);
    EXPECT_TRUE(missing.name.isEmpty());
    EXPECT_FLOAT_EQ(missing.color.r(), 0.0f);
    EXPECT_FLOAT_EQ(missing.color.g(), 0.0f);
    EXPECT_FLOAT_EQ(missing.color.b(), 0.0f);
}

TEST(ColorSwatchStandaloneTest, RemoveAtErasesRequestedEntryAndPreservesOrder)
{
    ColorSwatch palette;
    palette.addColor(FloatColor(1.0f, 0.0f, 0.0f, 1.0f), String("red"));
    palette.addColor(FloatColor(0.0f, 1.0f, 0.0f, 1.0f), String("green"));
    palette.addColor(FloatColor(0.0f, 0.0f, 1.0f, 1.0f), String("blue"));
    palette.removeAt(1);
    ASSERT_EQ(palette.count(), 2);
    EXPECT_EQ(toStdString(palette.at(0).name), "red");
    EXPECT_EQ(toStdString(palette.at(1).name), "blue");
}

TEST(ColorSwatchStandaloneTest, RemoveAtIgnoresOutOfRangeIndex)
{
    ColorSwatch palette;
    palette.addColor(FloatColor(1.0f, 0.0f, 0.0f, 1.0f), String("red"));
    palette.removeAt(1);
    palette.removeAt(999);
    EXPECT_EQ(palette.count(), 1);
}

TEST(ColorSwatchStandaloneTest, ClearRemovesAllEntriesButPreservesPaletteName)
{
    ColorSwatch palette(String("Colors"));
    palette.addColor(FloatColor(0.2f, 0.3f, 0.4f, 1.0f), String("one"));
    palette.clear();
    EXPECT_EQ(palette.count(), 0);
    EXPECT_EQ(toStdString(palette.getName()), "Colors");
}

TEST(ColorSwatchStandaloneTest, FindColorMatchesAllChannelsIncludingAlpha)
{
    ColorSwatch palette;
    palette.addColor(FloatColor(0.1f, 0.2f, 0.3f, 0.4f), String("target"));
    palette.addColor(FloatColor(0.1f, 0.2f, 0.3f, 0.5f), String("other alpha"));
    EXPECT_EQ(palette.findColor(FloatColor(0.1f, 0.2f, 0.3f, 0.4f)), 0);
    EXPECT_EQ(palette.findColor(FloatColor(0.1f, 0.2f, 0.3f, 0.5f)), 1);
    EXPECT_EQ(palette.findColor(FloatColor(0.1f, 0.2f, 0.3f, 0.6f)), -1);
}

TEST(ColorSwatchStandaloneTest, FindColorUsesStrictTolerance)
{
    ColorSwatch palette;
    palette.addColor(FloatColor(0.5f, 0.5f, 0.5f, 1.0f), String("gray"));
    EXPECT_EQ(palette.findColor(FloatColor(0.5005f, 0.5f, 0.5f, 1.0f), 0.001f), 0);
    EXPECT_EQ(palette.findColor(FloatColor(0.5011f, 0.5f, 0.5f, 1.0f), 0.001f), -1);
}

TEST(ColorSwatchStandaloneTest, FindColorReturnsFirstMatchingEntry)
{
    ColorSwatch palette;
    const FloatColor color(0.3f, 0.4f, 0.5f, 1.0f);
    palette.addColor(color, String("first"));
    palette.addColor(color, String("second"));
    EXPECT_EQ(palette.findColor(color), 0);
}

TEST(ColorSwatchStandaloneTest, SortOrdersEntriesByAverageRgbBrightness)
{
    ColorSwatch palette;
    palette.addColor(FloatColor(1.0f, 0.0f, 0.0f, 1.0f), String("red"));
    palette.addColor(FloatColor(0.0f, 0.0f, 0.0f, 1.0f), String("black"));
    palette.addColor(FloatColor(1.0f, 1.0f, 1.0f, 1.0f), String("white"));
    palette.sort();
    EXPECT_EQ(toStdString(palette.at(0).name), "black");
    EXPECT_EQ(toStdString(palette.at(1).name), "red");
    EXPECT_EQ(toStdString(palette.at(2).name), "white");
}

TEST(ColorSwatchStandaloneTest, CopyConstructionCopiesNameAndEntriesIndependently)
{
    ColorSwatch original(String("Original"));
    original.addColor(FloatColor(0.2f, 0.3f, 0.4f, 1.0f), String("entry"));
    ColorSwatch copy(original);
    copy.setName(String("Copy"));
    copy.removeAt(0);
    EXPECT_EQ(toStdString(original.getName()), "Original");
    EXPECT_EQ(original.count(), 1);
    EXPECT_EQ(toStdString(copy.getName()), "Copy");
    EXPECT_EQ(copy.count(), 0);
}

TEST(ColorSwatchStandaloneTest, CopyAssignmentReplacesExistingContent)
{
    ColorSwatch source(String("Source"));
    source.addColor(FloatColor(0.2f, 0.3f, 0.4f, 1.0f), String("source entry"));
    ColorSwatch destination(String("Old"));
    destination.addColor(FloatColor(1.0f, 1.0f, 1.0f, 1.0f), String("old entry"));
    destination = source;
    destination.setName(String("Destination copy"));
    destination.clear();
    EXPECT_EQ(toStdString(source.getName()), "Source");
    ASSERT_EQ(source.count(), 1);
    EXPECT_EQ(toStdString(source.at(0).name), "source entry");
}

TEST(ColorSwatchStandaloneTest, SelfCopyAssignmentPreservesPalette)
{
    ColorSwatch palette(String("Self"));
    palette.addColor(FloatColor(0.2f, 0.3f, 0.4f, 1.0f), String("entry"));
    palette = palette;
    EXPECT_EQ(toStdString(palette.getName()), "Self");
    ASSERT_EQ(palette.count(), 1);
    EXPECT_EQ(toStdString(palette.at(0).name), "entry");
}

TEST(ColorSwatchStandaloneTest, ImportGplReadsNameColorsAndSkipsComments)
{
    TemporaryGpl file;
    file.write("GIMP Palette\nName:Palette\nColumns: 2\n# ignored\n255 0 0 Red\n0 128 255 Sky\n");
    ColorSwatch palette;
    ASSERT_TRUE(palette.importGPL(file.path()));
    EXPECT_EQ(toStdString(palette.getName()), "Palette");
    ASSERT_EQ(palette.count(), 2);
    EXPECT_EQ(toStdString(palette.at(0).name), "Red");
    EXPECT_FLOAT_EQ(palette.at(0).color.r(), 1.0f);
    EXPECT_FLOAT_EQ(palette.at(1).color.g(), 128.0f / 255.0f);
    EXPECT_EQ(toStdString(palette.at(1).name), "Sky");
}

TEST(ColorSwatchStandaloneTest, ImportGplIgnoresMalformedColorRows)
{
    TemporaryGpl file;
    file.write("GIMP Palette\n0 0 0 Black\nnot a color\n1 2 Broken\n255 255 255 White\n");
    ColorSwatch palette;
    ASSERT_TRUE(palette.importGPL(file.path()));
    ASSERT_EQ(palette.count(), 2);
    EXPECT_EQ(toStdString(palette.at(0).name), "Black");
    EXPECT_EQ(toStdString(palette.at(1).name), "White");
}

TEST(ColorSwatchStandaloneTest, ValidHeaderOnlyPaletteReplacesPreviousEntries)
{
    TemporaryGpl file;
    file.write("GIMP Palette\n# empty palette\n");
    ColorSwatch palette;
    palette.addColor(FloatColor(1.0f, 0.0f, 0.0f, 1.0f), String("old"));

    ASSERT_TRUE(palette.load(file.path()));
    EXPECT_EQ(palette.count(), 0);
}

TEST(ColorSwatchStandaloneTest, MissingGplFileLeavesExistingPaletteUntouched)
{
    TemporaryGpl file;
    ColorSwatch palette(String("Existing"));
    palette.addColor(FloatColor(0.2f, 0.3f, 0.4f, 1.0f), String("keep"));

    EXPECT_FALSE(palette.importGPL(file.path()));
    EXPECT_EQ(toStdString(palette.getName()), "Existing");
    ASSERT_EQ(palette.count(), 1);
    EXPECT_EQ(toStdString(palette.at(0).name), "keep");
}

TEST(ColorSwatchStandaloneTest, GplImportAcceptsValidRowWithoutEntryName)
{
    TemporaryGpl file;
    file.write("GIMP Palette\n0 0 0\n");
    ColorSwatch palette;

    ASSERT_TRUE(palette.importGPL(file.path()));
    ASSERT_EQ(palette.count(), 1);
    EXPECT_TRUE(palette.at(0).name.isEmpty());
}

TEST(ColorSwatchStandaloneTest, InvalidGplHeaderDoesNotClearExistingPalette)
{
    TemporaryGpl file;
    file.write("Not a palette\n");
    ColorSwatch palette(String("Existing"));
    palette.addColor(FloatColor(0.2f, 0.3f, 0.4f, 1.0f), String("keep"));
    EXPECT_FALSE(palette.importGPL(file.path()));
    EXPECT_EQ(palette.count(), 1);
    EXPECT_EQ(toStdString(palette.at(0).name), "keep");
}

TEST(ColorSwatchStandaloneTest, GplExportCanBeLoadedAndPreservesRgbEntries)
{
    TemporaryGpl file;
    ColorSwatch source(String("RoundTrip"));
    source.addColor(FloatColor(1.0f, 0.0f, 0.0f, 1.0f), String("Red"));
    source.addColor(FloatColor(0.0f, 128.0f / 255.0f, 1.0f, 1.0f), String("Sky"));
    ASSERT_TRUE(source.exportGPL(file.path()));
    ColorSwatch restored;
    ASSERT_TRUE(restored.load(file.path()));
    ASSERT_EQ(restored.count(), 2);
    EXPECT_EQ(toStdString(restored.at(0).name), "Red");
    EXPECT_NEAR(restored.at(1).color.g(), 128.0f / 255.0f, 1e-6f);
}

TEST(ColorSwatchStandaloneTest, GplExportClampsRgbChannelsToByteRange)
{
    TemporaryGpl file;
    ColorSwatch source;
    source.addColor(FloatColor(-0.5f, 0.5f, 2.0f, 0.25f), String("bounded"));
    ASSERT_TRUE(source.exportGPL(file.path()));
    ColorSwatch restored;
    ASSERT_TRUE(restored.load(file.path()));
    ASSERT_EQ(restored.count(), 1);
    EXPECT_FLOAT_EQ(restored.at(0).color.r(), 0.0f);
    EXPECT_NEAR(restored.at(0).color.g(), 127.0f / 255.0f, 1e-6f);
    EXPECT_FLOAT_EQ(restored.at(0).color.b(), 1.0f);
    EXPECT_FLOAT_EQ(restored.at(0).color.a(), 1.0f);
}

TEST(ColorSwatchStandaloneTest, GplExtensionMatchingIsCaseSensitive)
{
    TemporaryGpl gpl;
    std::filesystem::path uppercase = gpl.path();
    uppercase.replace_extension(".GPL");
    ColorSwatch palette;
    EXPECT_FALSE(palette.load(uppercase));
}

TEST(ColorSwatchStandaloneTest, UnsupportedExtensionFailsLoadAndSave)
{
    TemporaryGpl gpl;
    std::filesystem::path unsupported = gpl.path();
    unsupported.replace_extension(".json");
    ColorSwatch palette;
    EXPECT_FALSE(palette.load(unsupported));
    EXPECT_FALSE(palette.save(unsupported));
}
