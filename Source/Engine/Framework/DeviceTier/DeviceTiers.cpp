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
HYP_DEFINE_DEVICE_TIER_AXIS(DevicePlatform, DeviceTierPhase::PreGpu, "deviceplatform")
HYP_DEFINE_DEVICE_TIER_AXIS(DeviceGrade, DeviceTierPhase::PostGpu, "devicegrade")

namespace {

#if defined(HYP_WEB)
static constexpr DevicePlatform CurrentPlatform = DevicePlatform::Web;
#elif defined(HYP_ANDROID)
static constexpr DevicePlatform CurrentPlatform = DevicePlatform::Android;
#elif defined(HYP_IOS)
static constexpr DevicePlatform CurrentPlatform = DevicePlatform::IOS;
#elif defined(HYP_MACOS)
static constexpr DevicePlatform CurrentPlatform = DevicePlatform::MacOS;
#elif defined(HYP_LINUX)
static constexpr DevicePlatform CurrentPlatform = DevicePlatform::Linux;
#else
static constexpr DevicePlatform CurrentPlatform = DevicePlatform::Windows;
#endif

template <DevicePlatform Platform>
class PlatformTier final : public DeviceTier<Platform>
{
public:
    bool Decide(const DeviceFacts& facts) const override
    {
        // baseline implementation:
        // are we current built for `Platform`?
        return CurrentPlatform == Platform;
    }
};

using MacOSTier = PlatformTier<DevicePlatform::MacOS>;
using LinuxTier = PlatformTier<DevicePlatform::Linux>;
using AndroidTier = PlatformTier<DevicePlatform::Android>;
using IOSTier = PlatformTier<DevicePlatform::IOS>;
using WebTier = PlatformTier<DevicePlatform::Web>;

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

HYP_REGISTER_DEVICE_TIER(MacOSTier)
HYP_REGISTER_DEVICE_TIER(LinuxTier)
HYP_REGISTER_DEVICE_TIER(AndroidTier)
HYP_REGISTER_DEVICE_TIER(IOSTier)
HYP_REGISTER_DEVICE_TIER(WebTier)
HYP_REGISTER_DEVICE_TIER(MobileTier)
HYP_REGISTER_DEVICE_TIER(LaptopTier)
HYP_REGISTER_DEVICE_TIER(MediumGradeTier)
HYP_REGISTER_DEVICE_TIER(HighGradeTier)
HYP_REGISTER_DEVICE_TIER(UltraGradeTier)

} // anonymous namespace

} // namespace Hyperion
