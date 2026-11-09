#include <gtest/gtest.h>

#include <QString>
#include <QVariant>
#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

import Data.DataPropertyBridge;
import Data.ExpressionDataFunctions;
import Data.CsvDataSource;
import Data.FileWatcher;
import Core.ArtifactString;
import Property;
import Property.Abstract;
import Property.Group;
import Property.Types;
import Memory.SharedPtr;

using namespace ArtifactCore;

namespace {
class TemporaryExpressionCsv {
public:
    TemporaryExpressionCsv()
    {
        static std::atomic_uint64_t nextId{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("artifact-expression-data-" + std::to_string(stamp) + "-" +
             std::to_string(nextId.fetch_add(1)) + ".csv");
    }

    ~TemporaryExpressionCsv()
    {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    const std::filesystem::path& path() const { return path_; }

    bool write(const std::string& contents) const
    {
        std::ofstream file(path_, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return false;
        file << contents;
        file.close();
        return file.good();
    }

private:
    std::filesystem::path path_;
};

class TemporaryExpressionDirectory {
public:
    TemporaryExpressionDirectory()
    {
        static std::atomic_uint64_t nextId{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("artifact-expression-collision-" + std::to_string(stamp) + "-" +
             std::to_string(nextId.fetch_add(1)));
    }

    ~TemporaryExpressionDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

class RegisteredExpressionSource {
public:
    explicit RegisteredExpressionSource(String uri) : uri_(std::move(uri))
    {
        csvDataSourceAutoRegister();
    }

    ~RegisteredExpressionSource()
    {
        DataPropertyBridge::unregisterSource(uri_);
    }

    void registerAt(int index)
    {
        ExpressionDataFunctions::registerDataSource(uri_, index);
    }

private:
    String uri_;
};

class RegisteredBridgeSource {
public:
    explicit RegisteredBridgeSource(String uri) : uri_(std::move(uri))
    {
        DataPropertyBridge::registerDataSource(uri_);
    }

    ~RegisteredBridgeSource()
    {
        unregister();
    }

    void unregister()
    {
        if (!registered_) return;
        DataPropertyBridge::unregisterSource(uri_);
        registered_ = false;
    }

private:
    String uri_;
    bool registered_ = true;
};
}

TEST(DataExpressionBridgeContractTest, CSVRegistrationFeedsExpressionCountsAndIndexedValues)
{
    TemporaryExpressionCsv file;
    ASSERT_TRUE(file.write("0,1\r\n10,2.5\r\n-4,42\r\n7,8\r\n9,10\r\n"));

    const String uri(file.path().string());
    constexpr int uriIndex = 731904;
    std::unordered_map<std::string, ExpressionDataFunc> functions;
    QString propertyOwnerPath;
    {
        RegisteredExpressionSource registration(uri);
        registration.registerAt(uriIndex);
        ExpressionDataFunctions::registerFunctions(functions);
        ASSERT_TRUE(functions.contains("dataGet"));
        ASSERT_TRUE(functions.contains("dataRowCount"));
        ASSERT_TRUE(functions.contains("dataColCount"));

        const auto registeredUris = DataPropertyBridge::registeredUris();
        ASSERT_TRUE(std::any_of(registeredUris.begin(), registeredUris.end(), [&uri](const String& value) {
            return value == uri;
        }));
        EXPECT_TRUE(FileWatcher::instance().isWatching(uri));

        const QString displayName = QString::fromUtf8(uri.data(), static_cast<int>(uri.length()));
        const auto owners = globalPropertyRegistry().owners();
        const auto ownerIt = std::find_if(owners.cbegin(), owners.cend(), [&displayName](const auto& owner) {
            return owner.displayName == displayName && owner.ownerType == QStringLiteral("Data");
        });
        ASSERT_NE(ownerIt, owners.cend());
        propertyOwnerPath = ownerIt->ownerPath;
        PropertyOwnerDescriptor descriptor;
        SharedPtr<PropertyGroup> group;
        ASSERT_TRUE(globalPropertyRegistry().tryGetOwner(propertyOwnerPath, &descriptor, &group));
        ASSERT_NE(group, nullptr);
        EXPECT_EQ(descriptor.ownerType, QStringLiteral("Data"));
        ASSERT_EQ(group->propertyCount(), 2u);

        const auto firstColumn = group->findProperty(QStringLiteral("0"));
        const auto secondColumn = group->findProperty(QStringLiteral("1"));
        ASSERT_NE(firstColumn, nullptr);
        ASSERT_NE(secondColumn, nullptr);
        EXPECT_EQ(firstColumn->getType(), PropertyType::String);
        EXPECT_EQ(secondColumn->getType(), PropertyType::String);
        EXPECT_EQ(firstColumn->getValue().toString(), QStringLiteral("10, -4, 7..."));
        EXPECT_EQ(secondColumn->getValue().toString(), QStringLiteral("2.5, 42, 8..."));

        EXPECT_DOUBLE_EQ(functions.at("dataRowCount")({static_cast<double>(uriIndex)}), 4.0);
        EXPECT_DOUBLE_EQ(functions.at("dataColCount")({static_cast<double>(uriIndex)}), 2.0);
        EXPECT_DOUBLE_EQ(functions.at("dataGet")({0.0, 1.0, 0.0, static_cast<double>(uriIndex)}), 2.5);
        EXPECT_DOUBLE_EQ(functions.at("dataGet")({1.0, 0.0, 0.0, static_cast<double>(uriIndex)}), -4.0);
        EXPECT_DOUBLE_EQ(functions.at("dataGet")({2.0, 0.0, 0.0, static_cast<double>(uriIndex)}), 7.0);
        EXPECT_DOUBLE_EQ(functions.at("dataGet")({4.0, 0.0, 0.0, static_cast<double>(uriIndex)}), 0.0);
        EXPECT_DOUBLE_EQ(functions.at("dataGet")({0.0, 1.0}), 0.0);
        EXPECT_DOUBLE_EQ(functions.at("dataRowCount")({999999.0}), 0.0);
        EXPECT_DOUBLE_EQ(functions.at("dataColCount")({999999.0}), 0.0);
    }

    EXPECT_DOUBLE_EQ(functions.at("dataRowCount")({static_cast<double>(uriIndex)}), 0.0);
    const auto remainingUris = DataPropertyBridge::registeredUris();
    EXPECT_TRUE(std::none_of(remainingUris.begin(), remainingUris.end(), [&uri](const String& value) {
        return value == uri;
    }));
    EXPECT_FALSE(globalPropertyRegistry().containsOwner(propertyOwnerPath));
    EXPECT_FALSE(FileWatcher::instance().isWatching(uri));
}

TEST(DataExpressionBridgeContractTest,
     NonNumericCsvCellsReturnZeroWithoutThrowingFromExpressionFunction)
{
    TemporaryExpressionCsv file;
    ASSERT_TRUE(file.write("0\nnot-a-number\n"));

    const String uri(file.path().string());
    constexpr int uriIndex = 731905;
    std::unordered_map<std::string, ExpressionDataFunc> functions;
    RegisteredExpressionSource registration(uri);
    registration.registerAt(uriIndex);
    ExpressionDataFunctions::registerFunctions(functions);
    ASSERT_TRUE(functions.contains("dataGet"));
    ASSERT_EQ(toStdString(DataPropertyBridge::getValue(
                  uri, 0, String("0"))),
              "not-a-number");

    double result = 1.0;
    EXPECT_NO_THROW(result = functions.at("dataGet")(
                        {0.0, 0.0, 0.0, static_cast<double>(uriIndex)}));
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(DataExpressionBridgeContractTest,
     UnregisterAndReregisterSameUriLoadsReplacementFileContents)
{
    TemporaryExpressionCsv file;
    ASSERT_TRUE(file.write("value\n1\n"));

    const String uri(file.path().string());
    constexpr int uriIndex = 731906;
    std::unordered_map<std::string, ExpressionDataFunc> functions;
    RegisteredExpressionSource registration(uri);
    registration.registerAt(uriIndex);
    ExpressionDataFunctions::registerFunctions(functions);
    ASSERT_TRUE(functions.contains("dataGet"));

    const auto readValue = [&functions, uriIndex]() {
        return functions.at("dataGet")(
            {0.0, 0.0, 0.0, static_cast<double>(uriIndex)});
    };
    EXPECT_DOUBLE_EQ(readValue(), 1.0);

    DataPropertyBridge::unregisterSource(uri);
    EXPECT_DOUBLE_EQ(readValue(), 0.0);
    ASSERT_TRUE(file.write("value\n2\n"));
    registration.registerAt(uriIndex);

    EXPECT_EQ(toStdString(DataPropertyBridge::getValue(uri, 0, String("0"))), "2");
    EXPECT_DOUBLE_EQ(readValue(), 2.0);
}

TEST(DataExpressionBridgeContractTest,
     NonFiniteAndUnrepresentableIndicesReturnZeroWithoutThrowing)
{
    TemporaryExpressionCsv file;
    ASSERT_TRUE(file.write("value\n17\n"));

    const String uri(file.path().string());
    constexpr int uriIndex = 731907;
    RegisteredExpressionSource registration(uri);
    registration.registerAt(uriIndex);
    std::unordered_map<std::string, ExpressionDataFunc> functions;
    ExpressionDataFunctions::registerFunctions(functions);
    ASSERT_TRUE(functions.contains("dataGet"));
    ASSERT_TRUE(functions.contains("dataRowCount"));
    ASSERT_TRUE(functions.contains("dataColCount"));

    const std::array invalidIndices{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        1.0e100,
        -1.0e100,
    };
    for (const double invalidIndex : invalidIndices) {
        EXPECT_DOUBLE_EQ(functions.at("dataGet")(
                             {invalidIndex, 0.0, 0.0,
                              static_cast<double>(uriIndex)}),
                         0.0);
        EXPECT_DOUBLE_EQ(functions.at("dataGet")(
                             {0.0, invalidIndex, 0.0,
                              static_cast<double>(uriIndex)}),
                         0.0);
        EXPECT_DOUBLE_EQ(functions.at("dataRowCount")({invalidIndex}), 0.0);
        EXPECT_DOUBLE_EQ(functions.at("dataColCount")({invalidIndex}), 0.0);
    }
}

TEST(DataExpressionBridgeContractTest, SanitizedOwnerPathCollisionDoesNotCoupleSources)
{
    TemporaryExpressionDirectory directory;
    const auto nestedPath = directory.path() / "nested" / "source.csv";
    const auto dottedPath = directory.path() / "nested.source.csv";
    std::filesystem::create_directories(nestedPath.parent_path());
    {
        std::ofstream nestedFile(nestedPath, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(nestedFile.is_open());
        nestedFile << "value\nnested\n";
        nestedFile.close();
        ASSERT_TRUE(nestedFile.good());
    }
    {
        std::ofstream dottedFile(dottedPath, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(dottedFile.is_open());
        dottedFile << "value\ndotted\n";
        dottedFile.close();
        ASSERT_TRUE(dottedFile.good());
    }

    csvDataSourceAutoRegister();
    const String nestedUri(nestedPath.string());
    const String dottedUri(dottedPath.string());
    const QString nestedDisplay = QString::fromUtf8(nestedUri.data(), static_cast<int>(nestedUri.length()));
    const QString dottedDisplay = QString::fromUtf8(dottedUri.data(), static_cast<int>(dottedUri.length()));
    RegisteredBridgeSource nestedRegistration(nestedUri);
    RegisteredBridgeSource dottedRegistration(dottedUri);

    auto findOwnerPath = [](const QString& displayName) {
        const auto owners = globalPropertyRegistry().owners();
        const auto it = std::find_if(owners.cbegin(), owners.cend(), [&displayName](const auto& owner) {
            return owner.displayName == displayName && owner.ownerType == QStringLiteral("Data");
        });
        return it == owners.cend() ? QString{} : it->ownerPath;
    };
    const QString nestedOwner = findOwnerPath(nestedDisplay);
    const QString dottedOwner = findOwnerPath(dottedDisplay);

    ASSERT_FALSE(nestedOwner.isEmpty());
    ASSERT_FALSE(dottedOwner.isEmpty());
    EXPECT_NE(nestedOwner, dottedOwner);
    EXPECT_NE(DataPropertyBridge::getValue(nestedUri, 0, String("value")), "");
    EXPECT_NE(DataPropertyBridge::getValue(dottedUri, 0, String("value")), "");

    nestedRegistration.unregister();
    EXPECT_TRUE(globalPropertyRegistry().containsOwner(dottedOwner));
    EXPECT_EQ(toStdString(DataPropertyBridge::getValue(dottedUri, 0, String("value"))), "dotted");
}
