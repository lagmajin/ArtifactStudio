#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

import Core.AI.CommandIR;
import Command.Serializable;

using namespace ArtifactCore;

namespace {

class FactoryProbeCommand final : public SerializableCommand {
public:
    FactoryProbeCommand(QString type, QString generation)
        : type_(std::move(type)), generation_(std::move(generation)) {}

    QString commandType() const override { return type_; }
    QJsonObject serialize() const override
    {
        return {{QStringLiteral("generation"), generation_}};
    }
    bool deserialize(const QJsonObject& data) override
    {
        generation_ = data.value(QStringLiteral("generation")).toString();
        return !generation_.isEmpty();
    }
    void undo() override {}
    void redo() override {}

    const QString& generation() const { return generation_; }

private:
    QString type_;
    QString generation_;
};

QVariantMap commandForSpec(const QVariantMap& spec)
{
    QVariantMap command{
        {QStringLiteral("type"), spec.value(QStringLiteral("type"))},
        {QStringLiteral("target"), QVariantMap{}},
    };
    const QVariantList required = spec.value(QStringLiteral("required")).toList();
    QVariantMap target;
    for (const QVariant& fieldValue : required) {
        const QString field = fieldValue.toString();
        if (field == QStringLiteral("target.layerId")) {
            target.insert(QStringLiteral("layerId"), QStringLiteral("layer-1"));
        } else if (field == QStringLiteral("target.propertyPath")) {
            target.insert(QStringLiteral("propertyPath"), QStringLiteral("transform.position"));
        } else if (field == QStringLiteral("keys[]")) {
            command.insert(QStringLiteral("keys"), QVariantList{
                QVariantMap{{QStringLiteral("frame"), 1},
                            {QStringLiteral("value"), 2.0}},
            });
        } else if (field == QStringLiteral("batches[]")) {
            command.insert(QStringLiteral("batches"), QVariantList{
                QVariantMap{{QStringLiteral("propertyPath"), QStringLiteral("transform.position")}},
            });
        } else if (field.endsWith(QStringLiteral("[]"))) {
            command.insert(field.left(field.size() - 2), QVariantList{QStringLiteral("value")});
        } else {
            command.insert(field, QStringLiteral("value"));
        }
    }
    command.insert(QStringLiteral("target"), target);
    return command;
}

} // namespace

TEST(CommandIRContractTest,
     CommandFactoryCreatorCanReplaceItsOwnRegistrationWithoutChangingInFlightCreate)
{
    auto& factory = CommandFactory::instance();
    const QString type =
        QStringLiteral("CommandIRContractTest.ReentrantFactoryGeneration");
    factory.registerCommand(type, [&factory, type] {
        factory.registerCommand(type, [type] {
            return std::make_unique<FactoryProbeCommand>(
                type, QStringLiteral("replacement"));
        });
        return std::make_unique<FactoryProbeCommand>(
            type, QStringLiteral("in-flight"));
    });

    auto inFlight = factory.create(type);
    ASSERT_NE(inFlight, nullptr);
    auto* inFlightProbe = dynamic_cast<FactoryProbeCommand*>(inFlight.get());
    ASSERT_NE(inFlightProbe, nullptr);
    EXPECT_EQ(inFlightProbe->generation(), QStringLiteral("in-flight"));

    auto subsequent = factory.create(type);
    ASSERT_NE(subsequent, nullptr);
    auto* subsequentProbe = dynamic_cast<FactoryProbeCommand*>(subsequent.get());
    ASSERT_NE(subsequentProbe, nullptr);
    EXPECT_EQ(subsequentProbe->generation(), QStringLiteral("replacement"));
}

TEST(CommandIRContractTest, VariantMapParsingTrimsTypeAndPreservesRequestPayload)
{
    const QVariantMap input{
        {QStringLiteral("type"), QStringLiteral("  set_property  ")},
        {QStringLiteral("target"), QVariantMap{
            {QStringLiteral("layerId"), QStringLiteral("layer-7")},
            {QStringLiteral("propertyPath"), QStringLiteral("transform.scale")},
        }},
        {QStringLiteral("value"), QVariant::fromValue(QVariantList{2.0, 3.0})},
        {QStringLiteral("metadata"), QVariantMap{{QStringLiteral("origin"), QStringLiteral("test")}}},
    };

    const CommandRequest request = CommandIR::fromVariantMap(input);

    EXPECT_EQ(request.type, QStringLiteral("set_property"));
    EXPECT_EQ(request.target.value(QStringLiteral("layerId")).toString(),
              QStringLiteral("layer-7"));
    EXPECT_EQ(request.target.value(QStringLiteral("propertyPath")).toString(),
              QStringLiteral("transform.scale"));
    EXPECT_EQ(request.arguments, input);
    EXPECT_EQ(request.metadata.value(QStringLiteral("origin")).toString(),
              QStringLiteral("test"));
}

TEST(CommandIRContractTest, SupportedCommandCatalogHasUniqueDescribedTypes)
{
    const QVariantList commands = CommandIR::supportedCommands();
    ASSERT_FALSE(commands.isEmpty());
    QStringList seenTypes;

    for (const QVariant& value : commands) {
        const QVariantMap spec = value.toMap();
        const QString type = spec.value(QStringLiteral("type")).toString();
        ASSERT_FALSE(type.isEmpty());
        EXPECT_FALSE(seenTypes.contains(type)) << type.toStdString();
        seenTypes.append(type);
        EXPECT_TRUE(CommandIR::isSupportedType(type));
        EXPECT_FALSE(CommandIR::describeType(type).trimmed().isEmpty())
            << type.toStdString();
        EXPECT_EQ(CommandIR::requiredFieldsFor(type),
                  spec.value(QStringLiteral("required")).toList())
            << type.toStdString();
        EXPECT_FALSE(CommandIR::undoLabelForType(type).trimmed().isEmpty())
            << type.toStdString();
    }
}

TEST(CommandIRContractTest,
     EverySupportedCommandHasAnExplicitNonGenericUndoLabel)
{
    for (const QVariant& value : CommandIR::supportedCommands()) {
        const QString type =
            value.toMap().value(QStringLiteral("type")).toString();
        const QString undoLabel = CommandIR::undoLabelForType(type);
        SCOPED_TRACE(type.toStdString());
        EXPECT_FALSE(undoLabel.isEmpty());
        EXPECT_NE(undoLabel, QStringLiteral("Command"))
            << "catalog additions must add a user-facing undo label";
    }
}

TEST(CommandIRContractTest, EveryCatalogRequiredFieldIsAcceptedByTheValidator)
{
    for (const QVariant& value : CommandIR::supportedCommands()) {
        const QVariantMap spec = value.toMap();
        const QVariantMap command = commandForSpec(spec);
        const CommandRequest request = CommandIR::fromVariantMap(command);
        const CommandResult result = CommandIR::validate(request);
        SCOPED_TRACE(spec.value(QStringLiteral("type")).toString().toStdString());
        EXPECT_TRUE(result.valid) << result.error.toStdString();
        EXPECT_TRUE(result.success) << result.error.toStdString();
        EXPECT_FALSE(result.executed);
        EXPECT_TRUE(result.diagnostics.isEmpty());
    }
}

TEST(CommandIRContractTest, MissingAndEmptyFieldsProduceStableValidationDiagnostics)
{
    CommandRequest missingTarget;
    missingTarget.type = QStringLiteral("set_property");
    missingTarget.arguments = {{QStringLiteral("value"), 1.0}};

    const CommandResult missing = CommandIR::validate(missingTarget);
    EXPECT_FALSE(missing.valid);
    EXPECT_FALSE(missing.success);
    EXPECT_TRUE(missing.retryable);
    EXPECT_EQ(missing.errorCode, QStringLiteral("COMMAND_INVALID"));
    EXPECT_EQ(missing.diagnostics.value(QStringLiteral("target.layerId")).toString(),
              QStringLiteral("missing"));
    EXPECT_EQ(missing.diagnostics.value(QStringLiteral("target.propertyPath")).toString(),
              QStringLiteral("missing"));

    QVariantMap emptyListCommand{
        {QStringLiteral("type"), QStringLiteral("set_keyframes")},
        {QStringLiteral("target"), QVariantMap{
            {QStringLiteral("layerId"), QStringLiteral("layer-1")},
            {QStringLiteral("propertyPath"), QStringLiteral("transform.position")},
        }},
        {QStringLiteral("keys"), QVariantList{}},
    };
    const CommandResult emptyList = CommandIR::validate(
        CommandIR::fromVariantMap(emptyListCommand));
    EXPECT_FALSE(emptyList.valid);
    EXPECT_EQ(emptyList.diagnostics.value(QStringLiteral("keys[]")).toString(),
              QStringLiteral("missing"));

    emptyListCommand[QStringLiteral("keys")] = QVariantList{QVariantMap{}};
    const CommandResult nonEmptyList = CommandIR::validate(
        CommandIR::fromVariantMap(emptyListCommand));
    EXPECT_TRUE(nonEmptyList.valid);
}

TEST(CommandIRContractTest, RequiredArrayFieldsUsePayloadNamesAndRejectInvalidShapes)
{
    const QVariantMap importSpec{{QStringLiteral("type"), QStringLiteral("import_asset")}};
    QVariantMap importCommand = commandForSpec(importSpec);
    importCommand.insert(QStringLiteral("filePaths"), QVariantList{QStringLiteral("a.png")});
    EXPECT_TRUE(CommandIR::validate(CommandIR::fromVariantMap(importCommand)).valid);

    importCommand.remove(QStringLiteral("filePaths"));
    CommandResult missingPaths = CommandIR::validate(CommandIR::fromVariantMap(importCommand));
    EXPECT_FALSE(missingPaths.valid);
    EXPECT_EQ(missingPaths.diagnostics.value(QStringLiteral("filePaths[]")).toString(),
              QStringLiteral("missing"));

    importCommand.insert(QStringLiteral("filePaths"), QStringLiteral("a.png"));
    CommandResult wrongPathsType = CommandIR::validate(CommandIR::fromVariantMap(importCommand));
    EXPECT_FALSE(wrongPathsType.valid);
    EXPECT_EQ(wrongPathsType.diagnostics.value(QStringLiteral("filePaths[]")).toString(),
              QStringLiteral("invalid_type"));

    const QVariantMap groupSpec{{QStringLiteral("type"), QStringLiteral("group_layers")}};
    QVariantMap groupCommand = commandForSpec(groupSpec);
    groupCommand.insert(QStringLiteral("layerIds"), QVariantList{QStringLiteral("layer-1")});
    groupCommand.insert(QStringLiteral("groupName"), QStringLiteral("Group"));
    EXPECT_TRUE(CommandIR::validate(CommandIR::fromVariantMap(groupCommand)).valid);
    groupCommand[QStringLiteral("layerIds")] = QVariantList{};
    CommandResult emptyLayerIds = CommandIR::validate(CommandIR::fromVariantMap(groupCommand));
    EXPECT_FALSE(emptyLayerIds.valid);
    EXPECT_EQ(emptyLayerIds.diagnostics.value(QStringLiteral("layerIds[]")).toString(),
              QStringLiteral("missing"));
}

TEST(CommandIRContractTest,
     SpecializedRequiredArraysReportWrongPayloadTypesConsistently)
{
    const struct {
        const char* commandType;
        const char* arrayField;
    } cases[] = {
        {"set_keyframes", "keys"},
        {"batch_set_keyframes", "batches"},
    };
    const QVariantList wrongShapes{
        QStringLiteral("not-an-array"),
        QVariantMap{{QStringLiteral("not"), QStringLiteral("an array")}},
        42,
    };

    for (const auto& testCase : cases) {
        for (const QVariant& wrongShape : wrongShapes) {
            QVariantMap command{
                {QStringLiteral("type"), QString::fromLatin1(testCase.commandType)},
                {QStringLiteral("target"), QVariantMap{
                    {QStringLiteral("layerId"), QStringLiteral("layer-1")},
                    {QStringLiteral("propertyPath"), QStringLiteral("transform.position")},
                }},
                {QString::fromLatin1(testCase.arrayField), wrongShape},
            };
            const CommandResult result =
                CommandIR::validate(CommandIR::fromVariantMap(command));
            SCOPED_TRACE(QStringLiteral("%1 field=%2 type=%3")
                             .arg(QString::fromLatin1(testCase.commandType),
                                  QString::fromLatin1(testCase.arrayField),
                                  QString::fromLatin1(wrongShape.typeName()))
                             .toStdString());
            EXPECT_FALSE(result.valid);
            EXPECT_EQ(result.diagnostics
                          .value(QString::fromLatin1(testCase.arrayField) +
                                 QStringLiteral("[]"))
                          .toString(),
                      QStringLiteral("invalid_type"));
        }
    }
}

TEST(CommandIRContractTest, ReadOnlyClassificationMatchesExactTypesOnly)
{
    const QStringList readOnlyTypes{
        QStringLiteral("get_scene_info"), QStringLiteral("get_layer_info"),
        QStringLiteral("get_keyframes"), QStringLiteral("get_render_status"),
        QStringLiteral("list_available_effects"), QStringLiteral("list_compositions"),
        QStringLiteral("list_project_items"),
    };
    for (const QString& type : readOnlyTypes) {
        EXPECT_TRUE(CommandIR::isReadOnlyType(type)) << type.toStdString();
    }
    EXPECT_FALSE(CommandIR::isReadOnlyType(QStringLiteral("delete_layer")));
    EXPECT_FALSE(CommandIR::isReadOnlyType(QStringLiteral("get_scene_info_extra")));
    EXPECT_FALSE(CommandIR::isReadOnlyType(QStringLiteral("GET_SCENE_INFO")));
}

TEST(CommandIRContractTest, EmptyAndUnsupportedTypesHaveDistinctFailureContracts)
{
    CommandRequest empty;
    const CommandResult missing = CommandIR::validate(empty);
    EXPECT_EQ(missing.errorCode, QStringLiteral("COMMAND_INVALID"));
    EXPECT_TRUE(missing.retryable);
    EXPECT_EQ(missing.error, QStringLiteral("Missing command.type"));

    CommandRequest unsupported;
    unsupported.type = QStringLiteral("delete_everything");
    const CommandResult rejected = CommandIR::validate(unsupported);
    EXPECT_EQ(rejected.errorCode, QStringLiteral("UNSUPPORTED_COMMAND"));
    EXPECT_FALSE(rejected.retryable);
    EXPECT_EQ(rejected.error, QStringLiteral("Unsupported command type"));
    EXPECT_FALSE(CommandIR::isSupportedType(QStringLiteral(" DELETE_LAYER ")));
}

TEST(CommandIRContractTest, DirectRequestsNormalizeTypeForValidationAndUndoLabel)
{
    CommandRequest request;
    request.type = QStringLiteral("  set_property  ");
    request.target = {{QStringLiteral("layerId"), QStringLiteral("layer-1")},
                      {QStringLiteral("propertyPath"), QStringLiteral("transform.opacity")}};
    request.arguments = {{QStringLiteral("value"), 0.5}};

    const CommandResult result = CommandIR::validate(request);

    EXPECT_TRUE(result.valid) << result.error.toStdString();
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.executed);
    EXPECT_EQ(result.type, request.type);
    EXPECT_EQ(result.undoLabel, QStringLiteral("Set Property"));
}

TEST(CommandIRContractTest, CommandResultVariantMapRoundTripPreservesDiagnostics)
{
    CommandResult original;
    original.success = true;
    original.valid = true;
    original.executed = true;
    original.type = QStringLiteral("set_property");
    original.undoLabel = QStringLiteral("Set Property");
    original.diagnostics = {{QStringLiteral("changedCount"), 1}};
    original.details = QVariantList{QStringLiteral("layer-1"), 3};

    const CommandResult restored = commandResultFromVariantMap(original.toVariantMap());
    EXPECT_TRUE(restored.success);
    EXPECT_TRUE(restored.valid);
    EXPECT_TRUE(restored.executed);
    EXPECT_EQ(restored.type, original.type);
    EXPECT_EQ(restored.undoLabel, original.undoLabel);
    EXPECT_EQ(restored.diagnostics, original.diagnostics);
    EXPECT_EQ(restored.details, original.details);
}

TEST(CommandIRContractTest, SafeWriteDryRunRequiresWarningsForHighRiskOperations)
{
    SafeWriteDryRunResult dryRun;
    dryRun.operationName = QStringLiteral("replace_project_assets");
    EXPECT_TRUE(dryRun.isValid());

    dryRun.riskLevel = SafeWriteRiskLevel::High;
    EXPECT_FALSE(dryRun.isValid());
    dryRun.warnings = {QStringLiteral("Existing files may be replaced.")};
    EXPECT_TRUE(dryRun.isValid());

    dryRun.wouldFail = true;
    EXPECT_FALSE(dryRun.isValid());
}

TEST(CommandIRContractTest, SafeWriteDryRunRejectsUnknownRiskLevel)
{
    SafeWriteDryRunResult dryRun;
    dryRun.operationName = QStringLiteral("replace_project_assets");
    dryRun.riskLevel = static_cast<SafeWriteRiskLevel>(999);

    EXPECT_FALSE(dryRun.isValid());
    EXPECT_EQ(safeWriteRiskLevelName(dryRun.riskLevel), QStringLiteral("High"));
}

TEST(CommandIRContractTest, SafeWriteConfirmationAndPlanRequireMatchingOperation)
{
    SafeWriteConfirmationPayload confirmation;
    confirmation.operationName = QStringLiteral("replace_project_assets");
    EXPECT_FALSE(confirmation.isValid());
    confirmation.reason = QStringLiteral("User requested asset replacement.");
    EXPECT_FALSE(confirmation.isValid());
    confirmation.previewMessage = QStringLiteral("3 assets will be replaced.");
    EXPECT_TRUE(confirmation.isValid());

    SafeWriteExecutionPlan plan;
    plan.dryRun.operationName = confirmation.operationName;
    plan.dryRun.riskLevel = SafeWriteRiskLevel::High;
    plan.dryRun.warnings = {QStringLiteral("Existing files may be replaced.")};
    plan.confirmation = confirmation;
    EXPECT_TRUE(plan.isValid());

    plan.confirmation.operationName = QStringLiteral("different_operation");
    EXPECT_FALSE(plan.isValid());
}

TEST(CommandIRContractTest,
     ExecutionPlanRequiresValidDryRunAndHonorsOptionalConfirmation)
{
    SafeWriteConfirmationPayload confirmation;
    confirmation.operationName = QStringLiteral("refresh_asset_metadata");
    confirmation.required = false;
    EXPECT_TRUE(confirmation.isValid());

    SafeWriteExecutionPlan plan;
    plan.dryRun.operationName = confirmation.operationName;
    plan.dryRun.riskLevel = SafeWriteRiskLevel::High;
    plan.confirmation = confirmation;
    EXPECT_FALSE(plan.isValid());

    plan.dryRun.warnings = {QStringLiteral("Metadata may be updated.")};
    EXPECT_TRUE(plan.isValid());

    plan.dryRun.wouldFail = true;
    EXPECT_FALSE(plan.isValid());
}
