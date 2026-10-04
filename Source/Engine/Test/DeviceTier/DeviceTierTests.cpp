/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Core/Config/Config.hpp>

#include <Core/Containers/String.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Format.hpp>

#include <Framework/DeviceTier/DeviceFacts.hpp>
#include <Framework/DeviceTier/DeviceScoring.hpp>
#include <Framework/DeviceTier/DeviceTier.hpp>
#include <Framework/DeviceTier/DeviceTierResolver.hpp>
#include <Framework/DeviceTier/DeviceTiers.hpp>
#include <Framework/DeviceTier/TierProfile.hpp>

namespace Hyperion {
namespace tests {
namespace devicetier {

namespace {

int g_passCount = 0;
int g_failCount = 0;

void Check(const ANSIStringView& testName, bool condition, const String& detail = "")
{
    if (condition)
    {
        ++g_passCount;
        HYP_LOG(Engine, Info, "[PASS] {}", testName);
    }
    else
    {
        ++g_failCount;
        HYP_LOG(Engine, Error, "[FAIL] {} {}", testName, detail);
    }
}

DeviceFacts MakeResolvedFacts(DeviceType type, DeviceGrade grade)
{
    DeviceFacts facts;
    facts.resolvedAxes.Set(TypeId::ForType<DeviceType>(), uint64(type));
    facts.resolvedAxes.Set(TypeId::ForType<DeviceGrade>(), uint64(grade));

    return facts;
}

TierRule MakeRule(const ANSIStringView& name, const ANSIStringView& whenAxis, const ANSIStringView& whenValue, const ANSIStringView& atMostAxis, const ANSIStringView& atMostValue)
{
    TierRule rule;
    rule.name = CreateNameFromDynamicString(name);

    if (whenAxis.Length() != 0)
    {
        rule.when.Set(CreateNameFromDynamicString(whenAxis), CreateNameFromDynamicString(whenValue));
    }

    if (atMostAxis.Length() != 0)
    {
        rule.atMost.Set(CreateNameFromDynamicString(atMostAxis), CreateNameFromDynamicString(atMostValue));
    }

    return rule;
}

double GetNumber(const ConfigOverlay& overlay, const UTF8StringView& key)
{
    const auto it = overlay.Find(String(key));

    return it != overlay.End() && it->second.IsNumber() ? it->second.AsNumber() : -1.0;
}

void TestTierResolution()
{
    const DeviceTierRegistry& registry = DeviceTierRegistry::GetInstance();

    {
        DeviceFacts facts;
        registry.Resolve(DeviceTierPhase::PreGpu, facts);
        Check("Resolve: plain desktop", facts.Get<DeviceType>() == DeviceType::Desktop);
    }

    {
        DeviceFacts facts;
        facts.hasBattery = true;
        registry.Resolve(DeviceTierPhase::PreGpu, facts);
        Check("Resolve: battery is a laptop", facts.Get<DeviceType>() == DeviceType::Laptop);
    }

    {
        DeviceFacts facts;
        facts.hasBattery = true;
        facts.isMobilePlatform = true;
        registry.Resolve(DeviceTierPhase::PreGpu, facts);
        Check("Resolve: mobile platform wins over battery", facts.Get<DeviceType>() == DeviceType::Mobile);
    }

    const struct
    {
        int score;
        DeviceGrade expected;
    } gradeCases[] = {
        { 0, DeviceGrade::Low },
        { GradeThreshold::Medium - 1, DeviceGrade::Low },
        { GradeThreshold::Medium, DeviceGrade::Medium },
        { GradeThreshold::High - 1, DeviceGrade::Medium },
        { GradeThreshold::High, DeviceGrade::High },
        { GradeThreshold::Ultra - 1, DeviceGrade::High },
        { GradeThreshold::Ultra, DeviceGrade::Ultra },
        { 100, DeviceGrade::Ultra }
    };

    for (const auto& gradeCase : gradeCases)
    {
        DeviceFacts facts;
        facts.score = gradeCase.score;
        registry.Resolve(DeviceTierPhase::PostGpu, facts);

        Check("Resolve: grade at score", facts.Get<DeviceGrade>() == gradeCase.expected, HYP_FORMAT("score={}", gradeCase.score));
    }

    {
        DeviceFacts facts;
        registry.Resolve(DeviceTierPhase::PreGpu, facts);
        Check("Resolve: post-GPU axis untouched by the pre-GPU phase", !facts.IsResolved<DeviceGrade>());
    }
}

void TestOverlayRules()
{
    TierProfile profile;

    TierRule highRule = MakeRule("High", {}, {}, "DeviceGrade", "High");
    highRule.set.Set(CreateNameFromDynamicString("Test.X"), Variant<bool, double, String>(1.0));

    TierRule mediumRule = MakeRule("Medium", {}, {}, "DeviceGrade", "Medium");
    mediumRule.set.Set(CreateNameFromDynamicString("Test.X"), Variant<bool, double, String>(2.0));
    mediumRule.set.Set(CreateNameFromDynamicString("Test.Y"), Variant<bool, double, String>(true));

    TierRule mobileRule = MakeRule("Mobile", "DeviceType", "Mobile", {}, {});
    mobileRule.set.Set(CreateNameFromDynamicString("Test.Y"), Variant<bool, double, String>(false));

    profile.rules.PushBack(highRule);
    profile.rules.PushBack(mediumRule);
    profile.rules.PushBack(mobileRule);

    {
        ConfigOverlay overlay;
        BuildTierOverlay(MakeResolvedFacts(DeviceType::Desktop, DeviceGrade::Ultra), profile, overlay);
        Check("Overlay: ultra desktop overrides nothing", overlay.Empty());
    }

    {
        ConfigOverlay overlay;
        BuildTierOverlay(MakeResolvedFacts(DeviceType::Desktop, DeviceGrade::High), profile, overlay);
        Check("Overlay: AtMost matches the boundary grade", GetNumber(overlay, "Test.X") == 1.0 && !overlay.Contains("Test.Y"));
    }

    {
        ConfigOverlay overlay;
        BuildTierOverlay(MakeResolvedFacts(DeviceType::Desktop, DeviceGrade::Low), profile, overlay);

        const auto yIt = overlay.Find("Test.Y");

        Check("Overlay: lower grades take the later rule", GetNumber(overlay, "Test.X") == 2.0);
        Check("Overlay: bool setting", yIt != overlay.End() && yIt->second.IsBool() && yIt->second.AsBool());
    }

    {
        ConfigOverlay overlay;
        Map<String, String> sources;
        BuildTierOverlay(MakeResolvedFacts(DeviceType::Mobile, DeviceGrade::Low), profile, overlay, &sources);

        const auto yIt = overlay.Find("Test.Y");
        const auto sourceIt = sources.Find("Test.Y");

        Check("Overlay: When rule overrides earlier values", yIt != overlay.End() && yIt->second.IsBool() && !yIt->second.AsBool());
        Check("Overlay: provenance records the winning rule", sourceIt != sources.End() && sourceIt->second == "Mobile");
    }

    {
        DeviceFacts facts;
        facts.resolvedAxes.Set(TypeId::ForType<DeviceType>(), uint64(DeviceType::Desktop));

        ConfigOverlay overlay;
        BuildTierOverlay(facts, profile, overlay);
        Check("Overlay: rules on unresolved axes are skipped", overlay.Empty());
    }
}

void TestProfileParsing()
{
    const String source = R"(
// comment
TierProfile "DeviceTiers" {
    Rules = [
        Rule "Parsed.A" {
            AtMost = { DeviceGrade = Medium }
            Set = {
                "Test.Count" = 16384
                "Test.Flag" = false
                "Test.Scale" = 0.25
                "Test.Text" = "abc"
            }
        }
        Rule "Parsed.B" {
            When = { DeviceType = Laptop, DeviceGrade = Ultra }
            Set = { "Test.Count" = 1 }
        }
    ]
}
)";

    HMF::ParseResult parseResult = HMF::Parse(source);

    Check("Parse: profile parses", !parseResult.HasError(), parseResult.HasError() ? String(parseResult.GetError().GetMessage()) : String());

    if (parseResult.HasError() || !parseResult.GetValue().Is<TierProfile>())
    {
        Check("Parse: result is a TierProfile", false);

        return;
    }

    const TierProfile& profile = parseResult.GetValue().Get<TierProfile>();

    Check("Parse: rule count", profile.rules.Size() == 2);

    if (profile.rules.Size() != 2)
    {
        return;
    }

    const TierRule& ruleA = profile.rules[0];
    const TierRule& ruleB = profile.rules[1];

    Check("Parse: AtMost entry", ruleA.atMost.Size() == 1 && ruleA.atMost.Contains(CreateNameFromDynamicString("DeviceGrade")));
    Check("Parse: When entries", ruleB.when.Size() == 2);

    const auto countIt = ruleA.set.Find(CreateNameFromDynamicString("Test.Count"));
    const auto flagIt = ruleA.set.Find(CreateNameFromDynamicString("Test.Flag"));
    const auto scaleIt = ruleA.set.Find(CreateNameFromDynamicString("Test.Scale"));
    const auto textIt = ruleA.set.Find(CreateNameFromDynamicString("Test.Text"));

    Check("Parse: integer becomes a number", countIt != ruleA.set.End() && countIt->second.Is<double>() && countIt->second.Get<double>() == 16384.0);
    Check("Parse: bool setting", flagIt != ruleA.set.End() && flagIt->second.Is<bool>() && !flagIt->second.Get<bool>());
    Check("Parse: float setting", scaleIt != ruleA.set.End() && scaleIt->second.Is<double>() && scaleIt->second.Get<double>() == 0.25);
    Check("Parse: string setting", textIt != ruleA.set.End() && textIt->second.Is<String>() && textIt->second.Get<String>() == "abc");
}

void TestConfigOverlay()
{
    ConfigOverlay overlay;
    overlay.Set("Rendering.Test.Value", ConfigValue(5.0));

    ConfigBase::SetOverlay("DeviceTierTestConfig", SharedPtr<const ConfigOverlay>(MakeShared<ConfigOverlay>(overlay)));

    ConfigBase config { "DeviceTierTestConfig" };
    config.Load();

    Check("Config overlay: Get returns the overlay value", config.Get("Rendering.Test.Value").IsNumber() && config.Get("Rendering.Test.Value").AsNumber() == 5.0);
    Check("Config overlay: not part of the loaded contents", !config.ToString().Contains("Test"));

    config.Set("Rendering.Test.Value", ConfigValue(7.0));

    Check("Config overlay: Set wins over the overlay", config.Get("Rendering.Test.Value").AsNumber() == 7.0);

    ConfigBase::SetOverlay("DeviceTierTestConfig", SharedPtr<const ConfigOverlay>());
}

} // namespace

ENGINE_API void RunDeviceTierTests()
{
    g_passCount = 0;
    g_failCount = 0;

    TestTierResolution();
    TestOverlayRules();
    TestProfileParsing();
    TestConfigOverlay();

    HYP_LOG(Engine, Info, "========== Device Tier Test Results: {} passed, {} failed ==========", g_passCount, g_failCount);

    if (g_failCount > 0)
    {
        HYP_LOG(Engine, Error, "!!! DEVICE TIER TEST HAD FAILURES !!!");
    }
}

} // namespace devicetier
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
