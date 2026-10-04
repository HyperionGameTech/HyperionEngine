/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/DeviceTier/DeviceTiers.hpp>
#include <Framework/DeviceTier/DeviceTier.hpp>
#include <Framework/DeviceTier/DeviceScoring.hpp>

#include <DeviceTiers.generated.inl>

namespace Hyperion {

HYP_DEFINE_DEVICE_TIER_AXIS(DeviceType, DeviceTierPhase::PreGpu, "devicetype")
HYP_DEFINE_DEVICE_TIER_AXIS(DeviceGrade, DeviceTierPhase::PostGpu, "devicegrade")

namespace {

class MobileTier final : public DeviceTier<DeviceType::Mobile>
{
public:
    bool Decide(const DeviceFacts& facts) const override
    {
        return facts.isMobilePlatform;
    }
};

class LaptopTier final : public DeviceTier<DeviceType::Laptop>
{
public:
    bool Decide(const DeviceFacts& facts) const override
    {
        return facts.hasBattery;
    }
};

class MediumGradeTier final : public DeviceTier<DeviceGrade::Medium>
{
public:
    bool Decide(const DeviceFacts& facts) const override
    {
        return facts.score >= GradeThreshold::Medium;
    }
};

class HighGradeTier final : public DeviceTier<DeviceGrade::High>
{
public:
    bool Decide(const DeviceFacts& facts) const override
    {
        return facts.score >= GradeThreshold::High;
    }
};

class UltraGradeTier final : public DeviceTier<DeviceGrade::Ultra>
{
public:
    bool Decide(const DeviceFacts& facts) const override
    {
        return facts.score >= GradeThreshold::Ultra;
    }
};

HYP_REGISTER_DEVICE_TIER(MobileTier)
HYP_REGISTER_DEVICE_TIER(LaptopTier)
HYP_REGISTER_DEVICE_TIER(MediumGradeTier)
HYP_REGISTER_DEVICE_TIER(HighGradeTier)
HYP_REGISTER_DEVICE_TIER(UltraGradeTier)

} // anonymous namespace

} // namespace Hyperion
