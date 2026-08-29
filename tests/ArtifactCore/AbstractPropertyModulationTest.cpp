#include <gtest/gtest.h>

#include <cstdint>
#include <memory>

import Audio.Modulation.Modulator;
import Audio.Modulation.Router;
import Property.Abstract;
import Time.Rational;

using namespace ArtifactCore;
using namespace ArtifactCore::Audio::Modulation;

namespace {
class ConstantSource final : public IModulatorSource {
public:
    explicit ConstantSource(float value) : value_(value) {}
    void setSampleRate(float) override {}
    void reset() override {}
    float process(std::uint32_t) override { return value_; }
    bool bipolar() const override { return true; }
private:
    float value_;
};
}

TEST(AbstractPropertyModulationTest, AppliesRouterAfterBaseValueAndHonorsRange) {
    AbstractProperty property;
    property.setType(PropertyType::Float);
    property.setValue(8.0);
    property.setMinValue(0.0);
    property.setMaxValue(10.0);

    ModulationRouter router;
    router.setSmoothingTime(0.0f);
    const auto sourceId = router.addSource(std::make_unique<ConstantSource>(1.0f));
    ASSERT_TRUE(router.addAssignment(ModulationAssignment::forPropertyPath(
        sourceId, "effect.blur.amount", 4.0f)));
    router.process(64);

    const QVariant value = property.evaluateValue(
        RationalTime(0, 30), nullptr, std::nullopt, &router, "effect.blur.amount");
    EXPECT_DOUBLE_EQ(value.toDouble(), 10.0);
}
