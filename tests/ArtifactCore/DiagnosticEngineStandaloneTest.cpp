#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

#include <QString>

import Core.Diagnostics.DiagnosticEngine;
import Core.Diagnostics.ProjectDiagnostic;

using ArtifactCore::DiagnosticCategory;
using ArtifactCore::DiagnosticEngine;
using ArtifactCore::DiagnosticResult;
using ArtifactCore::DiagnosticSeverity;
using ArtifactCore::IValidationRule;
using ArtifactCore::ProjectDiagnostic;
using ArtifactCore::ValidationRuleRegistry;

namespace {

class CountingRule final : public IValidationRule {
public:
    explicit CountingRule(QString name, QString layerId = {})
        : layerId_(std::move(layerId))
    {
        name_ = std::move(name);
    }

    auto validate(const void*) -> std::vector<ProjectDiagnostic> override
    {
        ++calls;
        if (layerId_.isEmpty()) {
            return {};
        }
        auto diagnostic = ProjectDiagnostic(
            DiagnosticSeverity::Warning,
            DiagnosticCategory::Custom,
            name_);
        diagnostic.setSourceLayerId(layerId_);
        return {diagnostic};
    }

    int calls = 0;

private:
    QString layerId_;
};

auto makeDiagnostic(DiagnosticSeverity severity, DiagnosticCategory category,
                    QString layerId = {}, QString compId = {}) -> ProjectDiagnostic
{
    auto diagnostic = ProjectDiagnostic(severity, category, QStringLiteral("diagnostic"));
    diagnostic.setSourceLayerId(layerId);
    diagnostic.setSourceCompId(compId);
    return diagnostic;
}

} // namespace

TEST(DiagnosticEngineStandaloneTest, RegistryIgnoresNullRules)
{
    ValidationRuleRegistry registry;
    registry.registerRule(nullptr);
    EXPECT_TRUE(registry.getRules().empty());
}

TEST(DiagnosticEngineStandaloneTest, RegistryFindsRegisteredRuleByName)
{
    ValidationRuleRegistry registry;
    registry.registerRule(std::make_unique<CountingRule>(QStringLiteral("alpha")));
    ASSERT_NE(registry.getRule(QStringLiteral("alpha")), nullptr);
    EXPECT_EQ(registry.getRule(QStringLiteral("alpha"))->name(), QStringLiteral("alpha"));
    EXPECT_EQ(registry.getRule(QStringLiteral("missing")), nullptr);
}

TEST(DiagnosticEngineStandaloneTest, RegistryAllowsDuplicateNamesAndFindsFirst)
{
    ValidationRuleRegistry registry;
    registry.registerRule(std::make_unique<CountingRule>(QStringLiteral("same")));
    auto* first = registry.getRule(QStringLiteral("same"));
    registry.registerRule(std::make_unique<CountingRule>(QStringLiteral("same")));
    EXPECT_EQ(registry.getRules().size(), 2u);
    EXPECT_EQ(registry.getRule(QStringLiteral("same")), first);
}

TEST(DiagnosticEngineStandaloneTest, UnregisterRemovesEveryRuleWithMatchingName)
{
    ValidationRuleRegistry registry;
    registry.registerRule(std::make_unique<CountingRule>(QStringLiteral("same")));
    registry.registerRule(std::make_unique<CountingRule>(QStringLiteral("keep")));
    registry.registerRule(std::make_unique<CountingRule>(QStringLiteral("same")));
    registry.unregisterRule(QStringLiteral("same"));
    ASSERT_EQ(registry.getRules().size(), 1u);
    EXPECT_EQ(registry.getRule(QStringLiteral("keep"))->name(), QStringLiteral("keep"));
}

TEST(DiagnosticEngineStandaloneTest, SetRuleEnabledUpdatesAllDuplicateNames)
{
    ValidationRuleRegistry registry;
    registry.registerRule(std::make_unique<CountingRule>(QStringLiteral("same")));
    registry.registerRule(std::make_unique<CountingRule>(QStringLiteral("same")));
    registry.setRuleEnabled(QStringLiteral("same"), false);
    for (const auto& rule : registry.getRules())
        EXPECT_FALSE(rule->enabled());
}

TEST(DiagnosticEngineStandaloneTest, ClearRulesEmptiesRegistry)
{
    ValidationRuleRegistry registry;
    registry.registerRule(std::make_unique<CountingRule>(QStringLiteral("one")));
    registry.clearRules();
    EXPECT_TRUE(registry.getRules().empty());
    EXPECT_EQ(registry.getRule(QStringLiteral("one")), nullptr);
}

TEST(DiagnosticEngineStandaloneTest, ValidateAllRunsEnabledRulesAndCachesResult)
{
    DiagnosticEngine engine;
    auto active = std::make_unique<CountingRule>(QStringLiteral("active"), QStringLiteral("layer-a"));
    auto* activePtr = active.get();
    auto inactive = std::make_unique<CountingRule>(QStringLiteral("inactive"), QStringLiteral("layer-b"));
    auto* inactivePtr = inactive.get();
    inactive->setEnabled(false);
    engine.ruleRegistry().registerRule(std::move(active));
    engine.ruleRegistry().registerRule(std::move(inactive));

    EXPECT_FALSE(engine.hasCachedResult());
    const auto result = engine.validateAll(nullptr);
    EXPECT_TRUE(engine.hasCachedResult());
    EXPECT_EQ(activePtr->calls, 1);
    EXPECT_EQ(inactivePtr->calls, 0);
    EXPECT_EQ(result.getWarningCount(), 1);
}

TEST(DiagnosticEngineStandaloneTest, ValidateAllCanReturnEmptyCachedResult)
{
    DiagnosticEngine engine;
    engine.validateAll(nullptr);
    EXPECT_TRUE(engine.hasCachedResult());
    EXPECT_TRUE(engine.getCachedResult().diagnostics().empty());
}

TEST(DiagnosticEngineStandaloneTest, ValidateDeltaWithoutCacheRunsFullValidation)
{
    DiagnosticEngine engine;
    auto rule = std::make_unique<CountingRule>(QStringLiteral("source"), QStringLiteral("layer-a"));
    auto* rulePtr = rule.get();
    engine.ruleRegistry().registerRule(std::move(rule));

    const auto result = engine.validateDelta(nullptr, {QStringLiteral("layer-a")});
    EXPECT_EQ(rulePtr->calls, 1);
    EXPECT_EQ(result.getWarningCount(), 1);
}

TEST(DiagnosticEngineStandaloneTest, ValidateDeltaWithNoChangedIdsReturnsCache)
{
    DiagnosticEngine engine;
    auto rule = std::make_unique<CountingRule>(QStringLiteral("source"), QStringLiteral("layer-a"));
    auto* rulePtr = rule.get();
    engine.ruleRegistry().registerRule(std::move(rule));
    const auto first = engine.validateAll(nullptr);
    const auto next = engine.validateDelta(nullptr, {});
    EXPECT_EQ(rulePtr->calls, 1);
    EXPECT_EQ(next.getWarningCount(), first.getWarningCount());
}

TEST(DiagnosticEngineStandaloneTest, ValidateDeltaForUnrelatedIdKeepsCache)
{
    DiagnosticEngine engine;
    auto rule = std::make_unique<CountingRule>(QStringLiteral("source"), QStringLiteral("layer-a"));
    auto* rulePtr = rule.get();
    engine.ruleRegistry().registerRule(std::move(rule));
    engine.validateAll(nullptr);
    const auto next = engine.validateDelta(nullptr, {QStringLiteral("unrelated")});
    EXPECT_EQ(rulePtr->calls, 1);
    EXPECT_EQ(next.getWarningCount(), 1);
}

TEST(DiagnosticEngineStandaloneTest, ValidateDeltaForDiagnosticSourceRevalidatesAllRules)
{
    DiagnosticEngine engine;
    auto rule = std::make_unique<CountingRule>(QStringLiteral("source"), QStringLiteral("layer-a"));
    auto* rulePtr = rule.get();
    engine.ruleRegistry().registerRule(std::move(rule));
    engine.validateAll(nullptr);
    const auto next = engine.validateDelta(nullptr, {QStringLiteral("layer-a")});
    EXPECT_EQ(rulePtr->calls, 2);
    EXPECT_EQ(next.getWarningCount(), 1);
}

TEST(DiagnosticEngineStandaloneTest, ClearCacheResetsAvailabilityAndDiagnostics)
{
    DiagnosticEngine engine;
    auto rule = std::make_unique<CountingRule>(QStringLiteral("source"), QStringLiteral("layer-a"));
    auto* rulePtr = rule.get();
    engine.ruleRegistry().registerRule(std::move(rule));
    engine.validateAll(nullptr);
    engine.clearCache();
    EXPECT_FALSE(engine.hasCachedResult());
    EXPECT_TRUE(engine.getCachedResult().diagnostics().empty());

    engine.validateDelta(nullptr, {QStringLiteral("unrelated")});
    EXPECT_EQ(rulePtr->calls, 2);
}

TEST(DiagnosticEngineStandaloneTest, ResultCountsEachSeverityIndependently)
{
    DiagnosticResult result;
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Error, DiagnosticCategory::File));
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Warning, DiagnosticCategory::Performance));
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Info, DiagnosticCategory::Custom));
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Error, DiagnosticCategory::Matte));

    EXPECT_EQ(result.getErrorCount(), 2);
    EXPECT_EQ(result.getWarningCount(), 1);
    EXPECT_EQ(result.getInfoCount(), 1);
    EXPECT_TRUE(result.hasErrors());
    EXPECT_TRUE(result.hasWarnings());
}

TEST(DiagnosticEngineStandaloneTest, ResultSeverityFiltersReturnOnlyMatchingEntries)
{
    DiagnosticResult result;
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Error, DiagnosticCategory::File));
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Warning, DiagnosticCategory::Performance));
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Info, DiagnosticCategory::Custom));

    EXPECT_EQ(result.getErrors().size(), 1u);
    EXPECT_EQ(result.getErrors().front().getSeverity(), DiagnosticSeverity::Error);
    EXPECT_EQ(result.getWarnings().size(), 1u);
    EXPECT_EQ(result.getWarnings().front().getSeverity(), DiagnosticSeverity::Warning);
}

TEST(DiagnosticEngineStandaloneTest, ResultCategoryFilterReturnsAllMatchingEntries)
{
    DiagnosticResult result;
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Error, DiagnosticCategory::File));
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Warning, DiagnosticCategory::File));
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Info, DiagnosticCategory::Custom));

    const auto files = result.getByCategory(DiagnosticCategory::File);
    ASSERT_EQ(files.size(), 2u);
    EXPECT_EQ(files[0].getCategory(), DiagnosticCategory::File);
    EXPECT_EQ(files[1].getCategory(), DiagnosticCategory::File);
}

TEST(DiagnosticEngineStandaloneTest, ResultSourceFilterMatchesLayerOrComposition)
{
    DiagnosticResult result;
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Error, DiagnosticCategory::File,
                                        QStringLiteral("layer-a"), QStringLiteral("comp-x")));
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Warning, DiagnosticCategory::Matte,
                                        QStringLiteral("layer-b"), QStringLiteral("comp-y")));
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Info, DiagnosticCategory::Custom,
                                        QStringLiteral("layer-c"), QStringLiteral("comp-x")));

    EXPECT_EQ(result.getBySource(QStringLiteral("layer-a"), QStringLiteral("missing")).size(), 1u);
    EXPECT_EQ(result.getBySource(QStringLiteral("missing"), QStringLiteral("comp-x")).size(), 2u);
}

TEST(DiagnosticEngineStandaloneTest, ResultMergeAppendsEntriesWithoutReplacingExistingOnes)
{
    DiagnosticResult left;
    DiagnosticResult right;
    left.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Error, DiagnosticCategory::File));
    right.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Warning, DiagnosticCategory::Performance));
    right.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Info, DiagnosticCategory::Custom));

    left.merge(right);
    ASSERT_EQ(left.diagnostics().size(), 3u);
    EXPECT_EQ(left.getErrorCount(), 1);
    EXPECT_EQ(left.getWarningCount(), 1);
    EXPECT_EQ(left.getInfoCount(), 1);
}

TEST(DiagnosticEngineStandaloneTest, ResultClearResetsEntriesAndCounts)
{
    DiagnosticResult result;
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Error, DiagnosticCategory::File));
    result.clear();

    EXPECT_TRUE(result.diagnostics().empty());
    EXPECT_EQ(result.getErrorCount(), 0);
    EXPECT_FALSE(result.hasErrors());
}

TEST(DiagnosticEngineStandaloneTest, ResultGetDiagnosticsReturnsSnapshot)
{
    DiagnosticResult result;
    result.addDiagnostic(makeDiagnostic(DiagnosticSeverity::Error, DiagnosticCategory::File));
    auto snapshot = result.getDiagnostics();
    snapshot.clear();

    EXPECT_EQ(result.diagnostics().size(), 1u);
}

TEST(DiagnosticEngineStandaloneTest, PerformanceFactorySetsWarningMetadata)
{
    const auto diagnostic = ProjectDiagnostic::createPerformanceWarning(
        QStringLiteral("slow layer"), QStringLiteral("layer-7"));

    EXPECT_TRUE(diagnostic.isWarning());
    EXPECT_EQ(diagnostic.getCategory(), DiagnosticCategory::Performance);
    EXPECT_EQ(diagnostic.getMessage(), QStringLiteral("slow layer"));
    EXPECT_EQ(diagnostic.getSourceLayerId(), QStringLiteral("layer-7"));
    EXPECT_FALSE(diagnostic.getFixAction().isEmpty());
}

TEST(DiagnosticEngineStandaloneTest, MissingFileFactorySetsErrorAndRelinkHint)
{
    const auto diagnostic = ProjectDiagnostic::createMissingFile(
        QStringLiteral("missing.png"), QStringLiteral("layer-2"));

    EXPECT_TRUE(diagnostic.isError());
    EXPECT_EQ(diagnostic.getCategory(), DiagnosticCategory::File);
    EXPECT_TRUE(diagnostic.getMessage().contains(QStringLiteral("missing.png")));
    EXPECT_EQ(diagnostic.getSourceLayerId(), QStringLiteral("layer-2"));
    EXPECT_FALSE(diagnostic.getDescription().isEmpty());
    EXPECT_FALSE(diagnostic.getFixAction().isEmpty());
}

TEST(DiagnosticEngineStandaloneTest, CircularDependencyFactoryAttachesCompositionSource)
{
    const auto diagnostic = ProjectDiagnostic::createCircularDependency(
        QStringLiteral("A -> B -> A"), QStringLiteral("comp-a"));

    EXPECT_TRUE(diagnostic.isError());
    EXPECT_EQ(diagnostic.getCategory(), DiagnosticCategory::CircularDep);
    EXPECT_EQ(diagnostic.getSourceCompId(), QStringLiteral("comp-a"));
    EXPECT_TRUE(diagnostic.getMessage().contains(QStringLiteral("A -> B -> A")));
}

TEST(DiagnosticEngineStandaloneTest, MissingMatteFactoryAttachesLayerAndRepairHint)
{
    const auto diagnostic = ProjectDiagnostic::createMissingMatte(
        QStringLiteral("matte-layer"), QStringLiteral("source-layer"));

    EXPECT_TRUE(diagnostic.isError());
    EXPECT_EQ(diagnostic.getCategory(), DiagnosticCategory::Matte);
    EXPECT_EQ(diagnostic.getSourceLayerId(), QStringLiteral("source-layer"));
    EXPECT_TRUE(diagnostic.getMessage().contains(QStringLiteral("matte-layer")));
    EXPECT_FALSE(diagnostic.getFixAction().isEmpty());
}

TEST(DiagnosticEngineStandaloneTest, ExpressionErrorFactoryIncludesExpressionAndLayer)
{
    const auto diagnostic = ProjectDiagnostic::createExpressionError(
        QStringLiteral("1 / 0"), QStringLiteral("text-layer"));

    EXPECT_TRUE(diagnostic.isError());
    EXPECT_EQ(diagnostic.getCategory(), DiagnosticCategory::Expression);
    EXPECT_EQ(diagnostic.getSourceLayerId(), QStringLiteral("text-layer"));
    EXPECT_TRUE(diagnostic.getMessage().contains(QStringLiteral("1 / 0")));
    EXPECT_FALSE(diagnostic.getDescription().isEmpty());
}
