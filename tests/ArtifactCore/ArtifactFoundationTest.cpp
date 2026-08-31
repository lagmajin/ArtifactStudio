#include <gtest/gtest.h>
#include <QString>
#include <limits>

import Core.ArtifactFoundation;
import Core.ArtifactMath;
import Core.ArtifactAlgorithms;
import Core.ArtifactChrono;
import Core.ArtifactRegex;
import Core.ArtifactSet;
import Core.ArtifactTuple;
import Core.ArtifactUtility;

using namespace ArtifactCore;

namespace {
constexpr float kEpsilon = 1e-6f;
}

TEST(ArtifactMathTest, MinMaxClampMatchExpectedSemantics)
{
    EXPECT_EQ(artifactMax(3, 7), 7);
    EXPECT_EQ(artifactMin(3, 7), 3);
    EXPECT_EQ(artifactMax(1, 5, 3, 2), 5);
    EXPECT_EQ(artifactMin(4, 2, 8, 6), 2);
    EXPECT_EQ(artifactClamp(15, 0, 10), 10);
    EXPECT_EQ(artifactClamp(-5, 0, 10), 0);
    EXPECT_EQ(artifactClamp(5, 0, 10), 5);
}

TEST(ArtifactMathTest, FloatPrimitivesDelegateCorrectly)
{
    EXPECT_TRUE(artifactIsFinite(1.0f));
    EXPECT_FALSE(artifactIsFinite(1.0f / 0.0f));
    EXPECT_TRUE(artifactIsNaN(0.0f / 0.0f));
    EXPECT_EQ(artifactAbs(-4), 4);
    EXPECT_NEAR(artifactSqrt(4.0f), 2.0f, kEpsilon);
    EXPECT_NEAR(artifactPow(2.0f, 10.0f), 1024.0f, kEpsilon);
    EXPECT_EQ(artifactLround(3.75), 4);
    EXPECT_EQ(artifactLround(-3.75), -4);
    EXPECT_NEAR(artifactLerp(2.0f, 6.0f, 0.25f), 3.0f, kEpsilon);
}

TEST(ArtifactUtilityTest, MoveForwardExchangeBehaveLikeStdCounterparts)
{
    int source = 41;
    const int& ref = artifactMove(source);
    EXPECT_EQ(ref, 41);

    int target = 0;
    artifactExchange(target, artifactMove(source));
    EXPECT_EQ(target, 41);

    EXPECT_TRUE(artifactCmpEqual(7, 7));
    EXPECT_FALSE(artifactCmpEqual(-1, 7u));
    EXPECT_TRUE(artifactCmpLess(-1, 7u));

    const auto bits = artifactBitCast<std::uint32_t>(1.0f);
    EXPECT_EQ(bits, 0x3F800000u);
}

TEST(ArtifactExpectedTest, StoresValueOrErrorWithoutReplacingResult)
{
    ArtifactExpected<int> value(42);
    EXPECT_TRUE(value.hasValue());
    EXPECT_EQ(value.value(), 42);
    EXPECT_EQ(value.valueOr(7), 42);
    EXPECT_TRUE(value.toOptional().has_value());

    const ArtifactExpectedError expectedError{
        ArtifactExpectedErrorCode::NotFound, "missing"};
    ArtifactExpected<int> failure(expectedError);
    EXPECT_TRUE(failure.hasError());
    EXPECT_EQ(failure.valueOr(7), 7);
    EXPECT_EQ(failure.error().code, ArtifactExpectedErrorCode::NotFound);

    const auto doubled = value.transform([](const int input) { return input * 2; });
    EXPECT_TRUE(doubled.hasValue());
    EXPECT_EQ(doubled.value(), 84);
}

TEST(ArtifactFunctionRefTest, BorrowsCallableWithoutOwnership)
{
    int offset = 3;
    auto addOffset = [&offset](const int value) { return value + offset; };
    ArtifactFunctionRef<int(int)> ref(addOffset);
    EXPECT_TRUE(ref.isValid());
    EXPECT_EQ(ref(4), 7);
    offset = 5;
    EXPECT_EQ(ref.invoke(4), 9);
    ref.clear();
    EXPECT_FALSE(ref.isValid());
}

TEST(ArtifactSaturationTest, ClampsIntegralOverflowAndConversions)
{
    EXPECT_EQ(addSat<unsigned char>(250, 10), std::numeric_limits<unsigned char>::max());
    EXPECT_EQ(subSat<unsigned char>(3, 10), 0);
    EXPECT_EQ(mulSat<int>(std::numeric_limits<int>::max(), 2),
              std::numeric_limits<int>::max());
    EXPECT_EQ(mulSat<int>(std::numeric_limits<int>::lowest(), 2),
              std::numeric_limits<int>::lowest());
    EXPECT_EQ(saturateCast<unsigned char>(300), std::numeric_limits<unsigned char>::max());
    EXPECT_EQ(saturateCast<unsigned char>(-4), 0);
    EXPECT_EQ(saturateCast<int>(std::numeric_limits<unsigned int>::max()),
              std::numeric_limits<int>::max());
}

TEST(ArtifactPtrTest, UniquePtrOwnsAndTransfers)
{
    struct Token {
        int value = 0;
        bool* destroyed = nullptr;
        explicit Token(int v, bool* flag) : value(v), destroyed(flag) {}
        ~Token() { if (destroyed) *destroyed = true; }
    };

    bool destroyed = false;
    auto token = makeUnique<Token>(7, &destroyed);
    ASSERT_TRUE(token);
    EXPECT_EQ(token->value, 7);
    EXPECT_EQ(token.get()->value, 7);

    UniquePtr<Token> moved = artifactMove(token);
    EXPECT_FALSE(token);
    ASSERT_TRUE(moved);
    EXPECT_EQ(moved->value, 7);

    Token* raw = moved.take();
    ASSERT_NE(raw, nullptr);
    EXPECT_FALSE(moved);
    delete raw;
    EXPECT_TRUE(destroyed);

    UniquePtr<Token> reset;
    reset.reset(new Token(9, nullptr));
    EXPECT_EQ(reset->value, 9);
    reset.reset();
    EXPECT_FALSE(reset);
}

TEST(ArtifactArrayTest, AppendInsertRemoveAndIterate)
{
    Array<int> values;
    values.append(1);
    values.append(2);
    values.append(3);
    ASSERT_EQ(values.size(), 3u);
    EXPECT_TRUE(values.contains(2));
    EXPECT_EQ(values.indexOf(3), 2);

    values.insert(1, 99);
    ASSERT_EQ(values.size(), 4u);
    EXPECT_EQ(values[1], 99);

    int sum = 0;
    for (const int value : values) sum += value;
    EXPECT_EQ(sum, 105);

    values.removeAt(1);
    EXPECT_EQ(values.size(), 3u);
    EXPECT_FALSE(values.contains(99));
}

TEST(ArtifactStringTest, SmallStringStoresAndAppends)
{
    String text("artifact");
    EXPECT_EQ(text.length(), 8u);
    EXPECT_FALSE(text.isEmpty());
    EXPECT_EQ(text.at(0), 'a');

    text += " studio";
    EXPECT_EQ(text.length(), 15u);
    EXPECT_EQ(text.at(8), ' ');
}

TEST(ArtifactDictTest, SetGetRemoveRoundTrip)
{
    Dict<QString, int> values;
    EXPECT_TRUE(values.isEmpty());

    values.set(QStringLiteral("alpha"), 1);
    values.set(QStringLiteral("beta"), 2);
    EXPECT_EQ(values.size(), 2);
    EXPECT_TRUE(values.contains(QStringLiteral("alpha")));

    const auto alpha = values.get(QStringLiteral("alpha"));
    ASSERT_TRUE(alpha.has_value());
    EXPECT_EQ(*alpha, 1);
    EXPECT_EQ(values.getOr(QStringLiteral("gamma"), 99), 99);

    values.remove(QStringLiteral("alpha"));
    EXPECT_FALSE(values.contains(QStringLiteral("alpha")));
    values.removeAll();
    EXPECT_TRUE(values.isEmpty());
}

TEST(ArtifactAlgorithmsTest, SortIsOrderedAndHandlesEdges)
{
    Array<int> values;
    values.append(5);
    values.append(1);
    values.append(4);
    values.append(2);
    values.append(3);
    artifactSort(values);
    EXPECT_TRUE(artifactIsSorted(values.begin(), values.end()));
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[4], 5);

    // Custom comparator sorts descending.
    artifactSort(values, [](const int a, const int b) { return a > b; });
    EXPECT_EQ(values[0], 5);
    EXPECT_EQ(values[4], 1);

    // Trivial sizes stay safe.
    Array<int> single;
    single.append(9);
    artifactSort(single);
    EXPECT_EQ(single.size(), 1u);
    Array<int> empty;
    artifactSort(empty);
    EXPECT_EQ(empty.size(), 0u);
}

TEST(ArtifactAlgorithmsTest, FindAndPredicates)
{
    const Array<int> values = [] {
        Array<int> v;
        v.append(10);
        v.append(20);
        v.append(30);
        return v;
    }();

    EXPECT_TRUE(artifactContains(values, 20));
    EXPECT_FALSE(artifactContains(values, 40));

    const auto found = artifactFind(values.begin(), values.end(), 20);
    ASSERT_NE(found, values.end());
    EXPECT_EQ(*found, 20);

    const auto missing = artifactFind(values.begin(), values.end(), 40);
    EXPECT_EQ(missing, values.end());

    EXPECT_TRUE(artifactAllOf(values.begin(), values.end(),
                             [](const int v) { return v > 0; }));
    EXPECT_TRUE(artifactAnyOf(values.begin(), values.end(),
                             [](const int v) { return v == 30; }));
    EXPECT_TRUE(artifactNoneOf(values.begin(), values.end(),
                              [](const int v) { return v < 0; }));

    const auto smallest = artifactMinElement(values.begin(), values.end());
    ASSERT_NE(smallest, values.end());
    EXPECT_EQ(*smallest, 10);
}

TEST(ArtifactAlgorithmsTest, RemoveIfCompactsAndShrinks)
{
    Array<int> values;
    for (int i = 0; i < 6; ++i) values.append(i);
    const std::size_t remaining =
        artifactRemoveIf(values, [](const int v) { return v % 2 == 0; });

    EXPECT_EQ(remaining, 3u);
    EXPECT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 3);
    EXPECT_EQ(values[2], 5);
}

TEST(ArtifactAlgorithmsTest, SortedRangeOperations)
{
    Array<int> sorted;
    for (const int v : {2, 4, 6, 8, 10}) sorted.append(v);

    const auto lower = artifactLowerBound(sorted.begin(), sorted.end(), 6);
    ASSERT_NE(lower, sorted.end());
    EXPECT_EQ(*lower, 6);

    EXPECT_TRUE(artifactBinarySearch(sorted.begin(), sorted.end(), 8));
    EXPECT_FALSE(artifactBinarySearch(sorted.begin(), sorted.end(), 7));

    artifactReverse(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted[0], 10);
    EXPECT_EQ(sorted[4], 2);
}

TEST(ArtifactMathTest, NumericTraitsExposeLimits)
{
    EXPECT_EQ(NumericTraits<int>::maxValue(), std::numeric_limits<int>::max());
    EXPECT_EQ(NumericTraits<int>::minValue(), std::numeric_limits<int>::lowest());
    EXPECT_TRUE(NumericTraits<int>::isInteger);
    EXPECT_TRUE(NumericTraits<double>::isSigned);
    EXPECT_FALSE(NumericTraits<unsigned char>::isSigned);
}

TEST(ArtifactUtilityTest, PairComparesByMembers)
{
    const auto pair = artifactMakePair(1, QStringLiteral("one"));
    EXPECT_EQ(pair.first, 1);
    EXPECT_EQ(pair.second, QStringLiteral("one"));

    const auto same = artifactMakePair(1, QStringLiteral("one"));
    EXPECT_EQ(pair, same);

    const auto different = artifactMakePair(2, QStringLiteral("one"));
    EXPECT_NE(pair, different);
}

TEST(ArtifactArrayTest, StaticArrayIsFixedCapacity)
{
    StaticArray<int, 4> squares;
    for (std::size_t i = 0; i < squares.size(); ++i) squares[i] = static_cast<int>(i * i);

    EXPECT_EQ(squares.size(), 4u);
    EXPECT_FALSE(squares.isEmpty());
    EXPECT_EQ(squares[3], 9);

    squares.fill(7);
    EXPECT_EQ(squares[0], 7);
    EXPECT_EQ(squares[3], 7);

    int sum = 0;
    for (const int value : squares) sum += value;
    EXPECT_EQ(sum, 28);
}

TEST(ArtifactTupleTest, ElementAccessAndSize)
{
    constexpr Tuple<int, float, char> triple(1, 2.5f, 'x');

    EXPECT_EQ(tupleSizeV<Tuple<int, float, char>>, 3u);
    EXPECT_EQ(artifactGet<0>(triple), 1);
    EXPECT_NEAR(artifactGet<1>(triple), 2.5f, kEpsilon);
    EXPECT_EQ(artifactGet<2>(triple), 'x');

    auto mutableTriple = artifactMakeTuple(10, 20);
    artifactGet<0>(mutableTriple) += 5;
    EXPECT_EQ(artifactGet<0>(mutableTriple), 15);
    EXPECT_EQ(artifactGet<1>(mutableTriple), 20);

    const Tuple<int, float> same1(1, 2.0f);
    const Tuple<int, float> same2(1, 2.0f);
    const Tuple<int, float> different(1, 3.0f);
    EXPECT_EQ(same1, same2);
    EXPECT_NE(same1, different);
}

TEST(ArtifactStringTest, AsciiHelpersCoverCaseTrimAndSplit)
{
    EXPECT_EQ(asciiLower(StringView("MiXeD")), String("mixed"));
    EXPECT_EQ(asciiUpper(StringView("MiXeD")), String("MIXED"));
    EXPECT_EQ(asciiTrimmed(StringView("  hello \t")), String("hello"));

    EXPECT_TRUE(asciiStartsWith(StringView("artifact"), StringView("art")));
    EXPECT_FALSE(asciiStartsWith(StringView("art"), StringView("artifact")));
    EXPECT_TRUE(asciiEndsWith(StringView("render.cpp"), StringView(".cpp")));

    const Array<String> parts = asciiSplit(StringView("a,b,,c"), ',');
    ASSERT_EQ(parts.size(), 4u);
    EXPECT_EQ(parts[0], String("a"));
    EXPECT_EQ(parts[2], String(""));
    EXPECT_EQ(parts[3], String("c"));

    EXPECT_EQ(asciiJoin(parts, ";"), String("a;b;;c"));
}

TEST(ArtifactAlgorithmsTest, AccumulateIotaCountMinMax)
{
    Array<int> values;
    values.resize(5);
    artifactIota(values.begin(), values.end(), 10);
    EXPECT_EQ(values.size(), 5u);
    EXPECT_EQ(values[4], 14);

    EXPECT_EQ(artifactAccumulate(values.begin(), values.end(), 0), 60);
    EXPECT_EQ(artifactAccumulate(values.begin(), values.end(), 1,
                                [](const int a, const int b) { return a * b; }),
              240240);

    EXPECT_EQ(artifactCount(values.begin(), values.end(), 12), 1);
    EXPECT_EQ(artifactCountIf(values.begin(), values.end(),
                             [](const int v) { return v > 11; }), 3);

    const auto [minIt, maxIt] = artifactMinMaxElement(values.begin(), values.end());
    EXPECT_EQ(*minIt, 10);
    EXPECT_EQ(*maxIt, 14);
}

TEST(ArtifactUtilityTest, HashCombineIsDeterministicAndSpreads)
{
    const std::size_t seed = 42;
    EXPECT_EQ(artifactHashCombine(seed, 7), artifactHashCombine(seed, 7));
    EXPECT_NE(artifactHashCombine(seed, 7), artifactHashCombine(seed, 8));
}

TEST(ArtifactChronoTest, DurationConversionsAndComparisons)
{
    const auto second = Duration::fromSeconds(1.0);
    EXPECT_NEAR(second.toMillis(), 1000.0, 1e-9);
    EXPECT_EQ(second.nanos(), 1'000'000'000LL);

    const auto half = Duration::fromMillis(500);
    EXPECT_EQ(second / 2, half);
    EXPECT_EQ(half + half, second);
    EXPECT_GT(second, half);
    EXPECT_LE(half, second);

    EXPECT_NEAR(Duration::fromMicros(1500).toMillis(), 1.5, 1e-9);
}

TEST(ArtifactChronoTest, SteadyClockIsMonotonicAndMeasures)
{
    const auto start = SteadyClock::now();
    // Busy-wait briefly; monotonicity is the contract under test.
    auto spin = SteadyClock::now();
    while (SteadyClock::elapsed(start).toNanos() < 1'000'000LL) {
        spin = SteadyClock::now();
        EXPECT_GE(spin, start);
        if (spin > start) break;
    }
    const auto elapsed = SteadyClock::elapsed(start);
    EXPECT_GT(elapsed.nanos(), 0);

    Stopwatch stopwatch;
    stopwatch.start();
    stopwatch.stop();
    const auto stopped = stopwatch.elapsed();
    stopwatch.start();
    EXPECT_TRUE(stopwatch.isRunning());
    EXPECT_GE(stopwatch.elapsed(), stopped);
}

TEST(ArtifactRegexTest, LiteralsClassesAndQuantifiers)
{
    RegexErrorCode error = RegexErrorCode::None;
    ArtifactRegex regex(QStringLiteral("ab+c"), false, &error);
    ASSERT_EQ(error, RegexErrorCode::None);
    ASSERT_TRUE(regex.isValid());

    EXPECT_FALSE(regex.matches(QStringLiteral("ac")));
    EXPECT_TRUE(regex.matches(QStringLiteral("xabcz")));

    const RegexMatch match = regex.search(QStringLiteral("--abbbc--"));
    ASSERT_TRUE(match.matched);
    EXPECT_EQ(match.groups[0].position, 2u);
    EXPECT_EQ(match.groups[0].length, 5u);
}

TEST(ArtifactRegexTest, GroupsAlternationAndAnchors)
{
    RegexErrorCode error = RegexErrorCode::None;
    ArtifactRegex regex(QStringLiteral("(\\w+)@(example|test)\\.com$"),
                        false, &error);
    ASSERT_EQ(error, RegexErrorCode::None);

    const RegexMatch match =
        regex.search(QStringLiteral("contact: user@example.com"));
    ASSERT_TRUE(match.matched);
    EXPECT_EQ(match.groups[0].position, 9u);
    EXPECT_EQ(match.groups[1].length, 4u);   // "user"
    EXPECT_EQ(match.groups[2].length, 7u);   // "example"

    EXPECT_FALSE(regex.matches(QStringLiteral("user@example.org")));
    EXPECT_TRUE(regex.matches(QStringLiteral("a@test.com")));
}

TEST(ArtifactRegexTest, ReplaceAllSubstitutesCaptures)
{
    RegexErrorCode error = RegexErrorCode::None;
    ArtifactRegex regex(QStringLiteral("(\\w+)=(\\w+)"), false, &error);
    ASSERT_EQ(error, RegexErrorCode::None);

    const QString replaced = regex.replaceAll(
        QStringLiteral("width=10 height=20"), QStringLiteral("$2 x $1"));
    EXPECT_EQ(replaced, QStringLiteral("10 x width 20 x height"));

    // Invalid pattern reports an error and stays invalid.
    ArtifactRegex broken(QStringLiteral("(unclosed"), false, &error);
    EXPECT_EQ(error, RegexErrorCode::MissingClosingParen);
    EXPECT_FALSE(broken.isValid());
}

TEST(ArtifactRegexTest, LookaheadAssertsWithoutConsuming)
{
    RegexErrorCode error = RegexErrorCode::None;
    ArtifactRegex positive(QStringLiteral("foo(?=123)"), false, &error);
    ASSERT_EQ(error, RegexErrorCode::None);

    const RegexMatch match = positive.search(QStringLiteral("foo123bar"));
    ASSERT_TRUE(match.matched);
    // The lookahead text is NOT part of the match.
    EXPECT_EQ(match.groups[0].length, 3u);
    EXPECT_EQ(match.groups[0].position, 0u);
    EXPECT_FALSE(positive.matches(QStringLiteral("foo456bar")));

    // Negative lookahead: 'a' not followed by 'b'.
    ArtifactRegex negative(QStringLiteral("a(?!b)"), false, &error);
    ASSERT_EQ(error, RegexErrorCode::None);
    const Array<RegexMatch> hits = negative.findAll(QStringLiteral("ab ac ad"));
    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(hits[0].groups[0].position, 3u); // "ac"
    EXPECT_EQ(hits[1].groups[0].position, 6u); // "ad"
}

TEST(ArtifactRegexTest, BackreferencesMatchRepeatedText)
{
    RegexErrorCode error = RegexErrorCode::None;
    ArtifactRegex repeatedWord(QStringLiteral("(\\w+) \\1"), false, &error);
    ASSERT_EQ(error, RegexErrorCode::None);

    EXPECT_TRUE(repeatedWord.matches(QStringLiteral("hello hello world")));
    EXPECT_FALSE(repeatedWord.matches(QStringLiteral("hello world")));

    const RegexMatch match = repeatedWord.search(QStringLiteral("say bye bye now"));
    ASSERT_TRUE(match.matched);
    EXPECT_EQ(match.groups[1].length, 3u); // "bye"

    // Backreference to an unopened group is a compile error.
    ArtifactRegex forward(QStringLiteral("\\1(ab)"), false, &error);
    EXPECT_EQ(error, RegexErrorCode::InvalidEscape);
    EXPECT_FALSE(forward.isValid());
}

TEST(ArtifactSetTest, SelfContainedHashSetOperations)
{
    HashSet<int> values;
    EXPECT_TRUE(values.isEmpty());

    EXPECT_TRUE(values.add(1));
    EXPECT_FALSE(values.add(1)); // duplicate rejected
    EXPECT_TRUE(values.add(2));
    EXPECT_EQ(values.size(), 2u);
    EXPECT_TRUE(values.contains(2));

    int visited = 0;
    bool sawOne = false;
    for (const int value : values) {
        ++visited;
        if (value == 1) sawOne = true;
    }
    EXPECT_EQ(visited, 2);
    EXPECT_TRUE(sawOne);

    const Array<int> all = values.values();
    ASSERT_EQ(all.size(), 2u);

    EXPECT_TRUE(values.remove(1));
    EXPECT_FALSE(values.remove(1));
    EXPECT_EQ(values.size(), 1u);

    values.clear();
    EXPECT_TRUE(values.isEmpty());
}
