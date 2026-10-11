#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

import Core.ArtifactVariant;

using ArtifactCore::ArtifactVariant;

using TestVariant = ArtifactVariant<int, std::string>;

namespace {

struct TrackedValue {
    static inline int alive = 0;

    explicit TrackedValue(int initial) : value(initial) { ++alive; }
    TrackedValue(const TrackedValue& other) : value(other.value) { ++alive; }
    TrackedValue(TrackedValue&& other) noexcept : value(other.value) { ++alive; }
    TrackedValue& operator=(const TrackedValue&) = default;
    TrackedValue& operator=(TrackedValue&&) noexcept = default;
    ~TrackedValue() { --alive; }

    int value;
};

struct alignas(64) OverAlignedValue {
    char padding[64]{};
    int value = 0;
};

} // namespace

static_assert(std::is_copy_constructible_v<TestVariant>);
static_assert(std::is_move_constructible_v<TestVariant>);

TEST(ArtifactVariantContractTest, ConstructionRecordsFirstAlternative)
{
    const TestVariant value(42);
    EXPECT_TRUE(value.isSet());
    EXPECT_TRUE(static_cast<bool>(value));
    EXPECT_EQ(value.index(), 0u);
    EXPECT_TRUE(value.holdsAlternative<int>());
    EXPECT_EQ(value.get<int>(), 42);
}

TEST(ArtifactVariantContractTest, ConstructionRecordsLaterAlternative)
{
    const TestVariant value(std::string("variant"));
    EXPECT_TRUE(value.isSet());
    EXPECT_EQ(value.index(), 1u);
    EXPECT_TRUE(value.holdsAlternative<std::string>());
    EXPECT_EQ(value.get<std::string>(), "variant");
}

TEST(ArtifactVariantContractTest, DefaultConstructionStartsUnset)
{
    const TestVariant value;
    EXPECT_FALSE(value.isSet());
    EXPECT_FALSE(static_cast<bool>(value));
}

TEST(ArtifactVariantContractTest, CopyAndMoveConstructionPreserveUnsetState)
{
    const TestVariant empty;
    const TestVariant copied(empty);
    TestVariant moved(std::move(copied));

    EXPECT_FALSE(copied.isSet());
    EXPECT_FALSE(moved.isSet());
}

TEST(ArtifactVariantContractTest, AssignmentFromUnsetValueClearsActiveAlternative)
{
    const TestVariant empty;
    TestVariant copied(std::string("copy target"));
    copied = empty;
    EXPECT_FALSE(copied.isSet());

    TestVariant moved(std::string("move target"));
    TestVariant moveSource;
    moved = std::move(moveSource);
    EXPECT_FALSE(moved.isSet());
    EXPECT_FALSE(moveSource.isSet());
}

TEST(ArtifactVariantContractTest, GetIfReturnsNullForDifferentAlternative)
{
    const TestVariant value(5);
    EXPECT_EQ(value.getIf<std::string>(), nullptr);
    ASSERT_NE(value.getIf<int>(), nullptr);
    EXPECT_EQ(*value.getIf<int>(), 5);
}

TEST(ArtifactVariantContractTest, MutableGetIfAllowsInPlaceUpdate)
{
    TestVariant value(5);
    int* held = value.getIf<int>();
    ASSERT_NE(held, nullptr);
    *held = 11;
    EXPECT_EQ(value.get<int>(), 11);
}

TEST(ArtifactVariantContractTest, ConstGetIfReturnsConstPointer)
{
    const TestVariant value(std::string("read only"));
    const auto* held = value.getIf<std::string>();
    ASSERT_NE(held, nullptr);
    EXPECT_EQ(*held, "read only");
}

TEST(ArtifactVariantContractTest, CopyConstructionDuplicatesValue)
{
    const TestVariant original(std::string("copy"));
    const TestVariant copy(original);
    ASSERT_TRUE(copy.holdsAlternative<std::string>());
    EXPECT_EQ(copy.get<std::string>(), "copy");
}

TEST(ArtifactVariantContractTest, CopyHasIndependentMutableValue)
{
    const TestVariant original(17);
    TestVariant copy(original);
    copy.get<int>() = 23;
    EXPECT_EQ(original.get<int>(), 17);
    EXPECT_EQ(copy.get<int>(), 23);
}

TEST(ArtifactVariantContractTest, MoveConstructionTransfersStringValue)
{
    TestVariant original(std::string("move"));
    TestVariant moved(std::move(original));
    ASSERT_TRUE(moved.holdsAlternative<std::string>());
    EXPECT_EQ(moved.get<std::string>(), "move");
}

TEST(ArtifactVariantContractTest, MovedFromStringKeepsAValidActiveAlternative)
{
    TestVariant original(std::string("source"));
    TestVariant moved(std::move(original));

    ASSERT_TRUE(original.isSet());
    EXPECT_TRUE(original.holdsAlternative<std::string>());
    EXPECT_NO_THROW(static_cast<void>(original.get<std::string>().size()));
    EXPECT_EQ(moved.get<std::string>(), "source");
}

TEST(ArtifactVariantContractTest, CopyAssignmentReplacesActiveAlternative)
{
    TestVariant destination(std::string("old"));
    const TestVariant source(29);
    destination = source;
    ASSERT_TRUE(destination.holdsAlternative<int>());
    EXPECT_EQ(destination.get<int>(), 29);
}

TEST(ArtifactVariantContractTest, MoveAssignmentReplacesActiveAlternative)
{
    TestVariant destination(31);
    TestVariant source(std::string("replacement"));
    destination = std::move(source);
    ASSERT_TRUE(destination.holdsAlternative<std::string>());
    EXPECT_EQ(destination.get<std::string>(), "replacement");
}

TEST(ArtifactVariantContractTest, SameAlternativeAssignmentsReplaceStoredValue)
{
    TestVariant copied(std::string("old copy value"));
    const TestVariant copySource(std::string("new copy value"));
    copied = copySource;
    EXPECT_EQ(copied.get<std::string>(), "new copy value");

    TestVariant moved(std::string("old move value"));
    TestVariant moveSource(std::string("new move value"));
    moved = std::move(moveSource);
    EXPECT_EQ(moved.get<std::string>(), "new move value");
}

TEST(ArtifactVariantContractTest, SelfCopyAssignmentKeepsValue)
{
    TestVariant value(std::string("self"));
    value = value;
    ASSERT_TRUE(value.holdsAlternative<std::string>());
    EXPECT_EQ(value.get<std::string>(), "self");
}

TEST(ArtifactVariantContractTest, SelfMoveAssignmentKeepsValue)
{
    TestVariant value(std::string("self move"));
    value = std::move(value);

    ASSERT_TRUE(value.holdsAlternative<std::string>());
    EXPECT_EQ(value.get<std::string>(), "self move");
}

TEST(ArtifactVariantContractTest, SwapExchangesDifferentAlternatives)
{
    TestVariant integer(37);
    TestVariant text(std::string("swapped"));
    integer.swap(text);
    ASSERT_TRUE(integer.holdsAlternative<std::string>());
    ASSERT_TRUE(text.holdsAlternative<int>());
    EXPECT_EQ(integer.get<std::string>(), "swapped");
    EXPECT_EQ(text.get<int>(), 37);
}

TEST(ArtifactVariantContractTest, SwapExchangesValuesWithinSameAlternative)
{
    TestVariant left(std::string("left"));
    TestVariant right(std::string("right"));

    left.swap(right);

    EXPECT_EQ(left.get<std::string>(), "right");
    EXPECT_EQ(right.get<std::string>(), "left");
}

TEST(ArtifactVariantContractTest, SelfSwapPreservesActiveValue)
{
    TestVariant value(std::string("self swap"));
    value.swap(value);

    ASSERT_TRUE(value.holdsAlternative<std::string>());
    EXPECT_EQ(value.get<std::string>(), "self swap");
}

TEST(ArtifactVariantContractTest, NonTrivialAlternativeLifetimeBalancesAcrossOperations)
{
    ASSERT_EQ(TrackedValue::alive, 0);
    {
        using TrackedVariant = ArtifactVariant<TrackedValue, int>;
        TrackedVariant first(TrackedValue(1));
        TrackedVariant copied(first);
        TrackedVariant moved(std::move(copied));
        TrackedVariant other(TrackedValue(2));

        moved = other;
        other = std::move(first);
        moved.swap(other);
        EXPECT_EQ(moved.get<TrackedValue>().value, 1);
        EXPECT_EQ(other.get<TrackedValue>().value, 2);
    }
    EXPECT_EQ(TrackedValue::alive, 0);
}

TEST(ArtifactVariantContractTest, DeterministicStateTransitionsMatchStandardVariantModel)
{
    std::array<ArtifactVariant<int, std::string>, 4> actual{};
    std::array<std::variant<std::monostate, int, std::string>, 4> expected{};
    std::mt19937 generator(0xA17F4A1u);
    std::uniform_int_distribution<int> operationDistribution(0, 4);
    std::uniform_int_distribution<int> slotDistribution(0, 3);
    std::uniform_int_distribution<int> valueDistribution(-1000, 1000);
    std::uniform_int_distribution<int> textDistribution(0, 9999);

    const auto expectEquivalent = [&] {
        for (std::size_t index = 0; index < actual.size(); ++index) {
            if (actual[index].holdsAlternative<int>()) {
                ASSERT_TRUE(std::holds_alternative<int>(expected[index]));
                EXPECT_EQ(actual[index].get<int>(), std::get<int>(expected[index]));
            } else if (actual[index].holdsAlternative<std::string>()) {
                ASSERT_TRUE(std::holds_alternative<std::string>(expected[index]));
                EXPECT_EQ(actual[index].get<std::string>(), std::get<std::string>(expected[index]));
            } else {
                EXPECT_TRUE(std::holds_alternative<std::monostate>(expected[index]));
                EXPECT_FALSE(actual[index].isSet());
            }
        }
    };

    for (int step = 0; step < 2000; ++step) {
        const int operation = operationDistribution(generator);
        const auto target = static_cast<std::size_t>(slotDistribution(generator));
        const auto source = static_cast<std::size_t>(slotDistribution(generator));

        if (operation == 0) {
            const int value = valueDistribution(generator);
            actual[target] = ArtifactVariant<int, std::string>(value);
            expected[target] = value;
        } else if (operation == 1) {
            const auto value = std::string("value-") + std::to_string(textDistribution(generator));
            actual[target] = ArtifactVariant<int, std::string>(value);
            expected[target] = value;
        } else if (operation == 2) {
            actual[target] = actual[source];
            expected[target] = expected[source];
        } else if (operation == 3) {
            actual[target].swap(actual[source]);
            std::swap(expected[target], expected[source]);
        } else {
            actual[target] = ArtifactVariant<int, std::string>();
            expected[target] = std::monostate{};
        }

        SCOPED_TRACE(step);
        expectEquivalent();
    }
}

TEST(ArtifactVariantContractTest, OverAlignedAlternativeStorageHonorsAlignment)
{
    using AlignedVariant = ArtifactVariant<OverAlignedValue, std::string>;
    OverAlignedValue source{};
    source.value = 73;

    AlignedVariant original(source);
    ASSERT_NE(original.getIf<OverAlignedValue>(), nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(original.getIf<OverAlignedValue>())
                  % alignof(OverAlignedValue),
              0u);
    EXPECT_EQ(original.get<OverAlignedValue>().value, 73);

    AlignedVariant copied(original);
    ASSERT_NE(copied.getIf<OverAlignedValue>(), nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(copied.getIf<OverAlignedValue>())
                  % alignof(OverAlignedValue),
              0u);
    EXPECT_EQ(copied.get<OverAlignedValue>().value, 73);
}

TEST(ArtifactVariantContractTest, RepeatedAlternativeReplacementBalancesNonTrivialLifetimes)
{
    using TrackedVariant = ArtifactVariant<TrackedValue, int, std::string>;
    ASSERT_EQ(TrackedValue::alive, 0);
    {
        TrackedVariant value(TrackedValue(0));
        ASSERT_EQ(TrackedValue::alive, 1);
        for (int iteration = 1; iteration <= 600; ++iteration) {
            if (iteration % 3 == 0) {
                value = TrackedVariant(TrackedValue(iteration));
                EXPECT_EQ(TrackedValue::alive, 1);
                EXPECT_EQ(value.get<TrackedValue>().value, iteration);
            } else if (iteration % 3 == 1) {
                value = TrackedVariant(iteration);
                EXPECT_EQ(TrackedValue::alive, 0);
                EXPECT_EQ(value.get<int>(), iteration);
            } else {
                value = TrackedVariant(std::to_string(iteration));
                EXPECT_EQ(TrackedValue::alive, 0);
                EXPECT_EQ(value.get<std::string>(), std::to_string(iteration));
            }
        }
    }
    EXPECT_EQ(TrackedValue::alive, 0);
}
