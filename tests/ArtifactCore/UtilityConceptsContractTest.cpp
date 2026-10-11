#include <gtest/gtest.h>

#include <QString>

#include <concepts>
#include <string>
#include <string_view>

import Numeric.Like;
import Utils.Point.Like;
import Utils.String.Like;
import Vector.Like;

using namespace ArtifactCore;

namespace {

struct IntegerExtent {
    int width() const { return 20; }
    long height() const { return 10; }
};

struct FloatingExtent {
    float width() const { return 20.5f; }
    double height() const { return 10.25; }
};

struct MissingHeight {
    int width() const { return 1; }
};

struct NonNumericExtent {
    const char* width() const { return "wide"; }
    int height() const { return 1; }
};

struct ArithmeticValue {
    float value = 0.0f;
    ArithmeticValue operator+(ArithmeticValue rhs) const { return {value + rhs.value}; }
    ArithmeticValue operator-(ArithmeticValue rhs) const { return {value - rhs.value}; }
    ArithmeticValue operator*(ArithmeticValue rhs) const { return {value * rhs.value}; }
    ArithmeticValue operator/(ArithmeticValue rhs) const { return {value / rhs.value}; }
    explicit operator float() const { return value; }
};

struct MissingDivision {
    MissingDivision operator+(MissingDivision) const;
    MissingDivision operator-(MissingDivision) const;
    MissingDivision operator*(MissingDivision) const;
    explicit operator float() const;
};

struct NonConvertibleArithmetic {
    bool operator+(NonConvertibleArithmetic) const;
    NonConvertibleArithmetic operator-(NonConvertibleArithmetic) const;
    NonConvertibleArithmetic operator*(NonConvertibleArithmetic) const;
    NonConvertibleArithmetic operator/(NonConvertibleArithmetic) const;
    explicit operator float() const;
};

struct MissingFloatConversion {
    MissingFloatConversion operator+(MissingFloatConversion) const;
    MissingFloatConversion operator-(MissingFloatConversion) const;
    MissingFloatConversion operator*(MissingFloatConversion) const;
    MissingFloatConversion operator/(MissingFloatConversion) const;
};

struct FloatVectorFields {
    float x = 1.0f;
    float y = 2.0f;
};

struct ConvertibleVectorFields {
    int x = 3;
    double y = 4.0;
};

struct MissingVectorY {
    float x = 1.0f;
};

struct MethodOnlyVector {
    float x() const { return 1.0f; }
    float y() const { return 2.0f; }
};

struct NonConvertibleVectorFields {
    const char* x = "x";
    float y = 2.0f;
};

} // namespace

static_assert(HasMethods<IntegerExtent>);
static_assert(IntSizeLike<IntegerExtent>);
static_assert(SizeLike<IntegerExtent>);
static_assert(FloatSizeLike<FloatingExtent>);
static_assert(SizeLike<FloatingExtent>);
static_assert(!HasMethods<MissingHeight>);
static_assert(!SizeLike<MissingHeight>);
static_assert(!SizeLike<NonNumericExtent>);
static_assert(NumericLike<int>);
static_assert(NumericLike<double>);
static_assert(NumericLike<ArithmeticValue>);
static_assert(!NumericLike<MissingDivision>);
static_assert(!NumericLike<NonConvertibleArithmetic>);
static_assert(!NumericLike<MissingFloatConversion>);
static_assert(FloatingLike<float>);
static_assert(FloatingLike<double>);
static_assert(!FloatingLike<int>);
static_assert(!FloatingLike<ArithmeticValue>);
static_assert(VectorLike<FloatVectorFields>);
static_assert(VectorLike<ConvertibleVectorFields>);
static_assert(!VectorLike<MissingVectorY>);
static_assert(!VectorLike<MethodOnlyVector>);
static_assert(!VectorLike<NonConvertibleVectorFields>);
static_assert(StringLike<QString>);
static_assert(StringLike<std::string>);
static_assert(StringLike<std::wstring>);
static_assert(StringLike<std::string_view>);
static_assert(StringLike<const char*>);
static_assert(!StringLike<int>);

TEST(UtilityConceptsContractTest, IntegralWidthAndHeightSatisfyIntegerSizeLike)
{
    EXPECT_TRUE(IntSizeLike<IntegerExtent>);
    EXPECT_TRUE(SizeLike<IntegerExtent>);
}

TEST(UtilityConceptsContractTest, FloatingWidthAndHeightSatisfyFloatingSizeLike)
{
    EXPECT_TRUE(FloatSizeLike<FloatingExtent>);
    EXPECT_TRUE(SizeLike<FloatingExtent>);
}

TEST(UtilityConceptsContractTest, HasMethodsRequiresBothDimensions)
{
    EXPECT_FALSE(HasMethods<MissingHeight>);
    EXPECT_FALSE(SizeLike<MissingHeight>);
}

TEST(UtilityConceptsContractTest, WidthMustBeIntegralOrFloatingPoint)
{
    EXPECT_FALSE(IntSizeLike<NonNumericExtent>);
    EXPECT_FALSE(FloatSizeLike<NonNumericExtent>);
    EXPECT_FALSE(SizeLike<NonNumericExtent>);
}

TEST(UtilityConceptsContractTest, IntegerAndFloatingScalarsAreNumericLike)
{
    EXPECT_TRUE(NumericLike<int>);
    EXPECT_TRUE(NumericLike<double>);
}

TEST(UtilityConceptsContractTest, UserDefinedArithmeticTypeCanSatisfyNumericLike)
{
    EXPECT_TRUE(NumericLike<ArithmeticValue>);
    const ArithmeticValue result = ArithmeticValue{6.0f} / ArithmeticValue{2.0f};
    EXPECT_FLOAT_EQ(static_cast<float>(result), 3.0f);
}

TEST(UtilityConceptsContractTest, NumericLikeRequiresAllFourArithmeticOperators)
{
    EXPECT_FALSE(NumericLike<MissingDivision>);
}

TEST(UtilityConceptsContractTest, ArithmeticResultsMustConvertBackToTheOperandType)
{
    EXPECT_FALSE(NumericLike<NonConvertibleArithmetic>);
}

TEST(UtilityConceptsContractTest, NumericLikeRequiresExplicitFloatConversion)
{
    EXPECT_FALSE(NumericLike<MissingFloatConversion>);
}

TEST(UtilityConceptsContractTest, FloatingLikeIsLimitedToFloatingPointScalars)
{
    EXPECT_TRUE(FloatingLike<float>);
    EXPECT_TRUE(FloatingLike<double>);
    EXPECT_FALSE(FloatingLike<int>);
    EXPECT_FALSE(FloatingLike<ArithmeticValue>);
}

TEST(UtilityConceptsContractTest, VectorLikeAcceptsFloatCoordinateMembers)
{
    EXPECT_TRUE(VectorLike<FloatVectorFields>);
}

TEST(UtilityConceptsContractTest, VectorLikeAcceptsMembersConvertibleToFloat)
{
    EXPECT_TRUE(VectorLike<ConvertibleVectorFields>);
}

TEST(UtilityConceptsContractTest, VectorLikeRequiresBothPublicCoordinateMembers)
{
    EXPECT_FALSE(VectorLike<MissingVectorY>);
}

TEST(UtilityConceptsContractTest, VectorLikeRequiresFieldsRatherThanAccessorMethods)
{
    EXPECT_FALSE(VectorLike<MethodOnlyVector>);
}

TEST(UtilityConceptsContractTest, VectorLikeRejectsNonConvertibleCoordinateMembers)
{
    EXPECT_FALSE(VectorLike<NonConvertibleVectorFields>);
}

TEST(UtilityConceptsContractTest, StringLikeAcceptsQStringAndPreservesItsContents)
{
    const QString source = QStringLiteral("Artifact");
    EXPECT_TRUE(StringLike<QString>);
    EXPECT_EQ(toQString(source), source);
}

TEST(UtilityConceptsContractTest, StringLikeConvertsStdStringAsUtf8)
{
    const std::string source = "Artifact";
    EXPECT_TRUE(StringLike<std::string>);
    EXPECT_EQ(toQString(source), QStringLiteral("Artifact"));
}

TEST(UtilityConceptsContractTest, StringLikeConvertsWideString)
{
    const std::wstring source = L"Artifact";
    EXPECT_TRUE(StringLike<std::wstring>);
    EXPECT_EQ(toQString(source), QStringLiteral("Artifact"));
}

TEST(UtilityConceptsContractTest, StringLikeConvertsStringView)
{
    constexpr std::string_view source("view");
    EXPECT_TRUE(StringLike<std::string_view>);
    EXPECT_EQ(toQString(source), QStringLiteral("view"));
}

TEST(UtilityConceptsContractTest, StringLikeConvertsUtf8CharacterPointer)
{
    const char* source = "literal";
    EXPECT_TRUE(StringLike<const char*>);
    EXPECT_EQ(toQString(source), QStringLiteral("literal"));
}

TEST(UtilityConceptsContractTest, StringLikeRejectsNonStringTypes)
{
    EXPECT_FALSE(StringLike<int>);
}
