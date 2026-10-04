/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/DeviceTier/DeviceTierResolver.hpp>
#include <Framework/DeviceTier/DeviceTier.hpp>
#include <Framework/DeviceTier/DeviceScoring.hpp>
#include <Framework/DeviceTier/DeviceTiers.hpp>

#include <Framework/Config/EngineConfig.hpp>
#include <Framework/CVarManager.hpp>
#include <Framework/DeviceDetails.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Core/Utilities/Format.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Engine);

namespace PlatformUtils {
ENGINE_API extern bool HasBattery();
ENGINE_API extern uint64 GetSystemMemoryBytes();
ENGINE_API extern uint32 GetLogicalCoreCount();
} // namespace PlatformUtils

static constexpr uint64 BytesPerMiB = 1024ull * 1024ull;

static DeviceFacts GatherPlatformFacts()
{
    DeviceFacts facts;

#if defined(HYP_ANDROID) || defined(HYP_IOS)
    facts.isMobilePlatform = true;
#endif

    facts.hasBattery = PlatformUtils::HasBattery();
    facts.logicalCores = PlatformUtils::GetLogicalCoreCount();
    facts.systemMemoryBytes = PlatformUtils::GetSystemMemoryBytes();

    return facts;
}

static ConfigValue ToConfigValue(const Variant<bool, double, String>& value)
{
    if (const bool* boolValue = value.TryGet<bool>())
    {
        return ConfigValue(*boolValue);
    }

    if (const double* numberValue = value.TryGet<double>())
    {
        return ConfigValue(*numberValue);
    }

    if (const String* stringValue = value.TryGet<String>())
    {
        return ConfigValue(*stringValue);
    }

    return ConfigValue();
}

static bool IsValidCondition(const TierRule& rule, const Map<Name, Name>& condition, const ANSIStringView& conditionName)
{
    const DeviceTierRegistry& registry = DeviceTierRegistry::GetInstance();

    for (const auto& entry : condition)
    {
        const DeviceTierAxisBase* axis = registry.FindAxis(GetNameView(entry.first));

        if (!axis)
        {
            HYP_LOG(Engine, Warning, "Device tier rule \"{}\" ignored: {} references unknown axis \"{}\"", GetNameView(rule.name), conditionName, GetNameView(entry.first));

            return false;
        }

        uint64 value = 0;

        if (!axis->ParseValue(GetNameView(entry.second), value))
        {
            HYP_LOG(Engine, Warning, "Device tier rule \"{}\" ignored: \"{}\" is not a value of {}", GetNameView(rule.name), GetNameView(entry.second), axis->GetName());

            return false;
        }
    }

    return true;
}

static bool DoesConditionMatch(const DeviceFacts& facts, const Map<Name, Name>& condition, bool isAtMost)
{
    const DeviceTierRegistry& registry = DeviceTierRegistry::GetInstance();

    for (const auto& entry : condition)
    {
        const DeviceTierAxisBase* axis = registry.FindAxis(GetNameView(entry.first));

        uint64 resolvedValue = 0;
        uint64 expectedValue = 0;

        if (!axis
            || !facts.TryGetResolved(axis->GetTypeId(), resolvedValue)
            || !axis->ParseValue(GetNameView(entry.second), expectedValue))
        {
            return false;
        }

        if (isAtMost ? resolvedValue > expectedValue : resolvedValue != expectedValue)
        {
            return false;
        }
    }

    return true;
}

void BuildTierOverlay(const DeviceFacts& facts, const TierProfile& profile, ConfigOverlay& outOverlay, Map<String, String>* outSources)
{
    for (const TierRule& rule : profile.rules)
    {
        if (!DoesConditionMatch(facts, rule.when, false) || !DoesConditionMatch(facts, rule.atMost, true))
        {
            continue;
        }

        for (const auto& setting : rule.set)
        {
            const String key = String(GetNameView(setting.first));

            outOverlay.Set(key, ToConfigValue(setting.second));

            if (outSources)
            {
                outSources->Set(key, String(GetNameView(rule.name)));
            }
        }
    }
}

DeviceTierResolver& DeviceTierResolver::GetInstance()
{
    static DeviceTierResolver s_instance;

    return s_instance;
}

void DeviceTierResolver::ResolvePreGpu()
{
    LoadProfile();

    m_facts = GatherPlatformFacts();

    DeviceTierRegistry::GetInstance().Resolve(DeviceTierPhase::PreGpu, m_facts);

    ApplyOverlay();
}

void DeviceTierResolver::ResolvePostGpu(const DeviceDetails& deviceDetails)
{
    LoadProfile();

    m_facts.hasGpu = true;
    m_facts.gpu = deviceDetails.info;
    m_facts.score = ComputeDeviceGradeScore(m_facts);

    DeviceTierRegistry::GetInstance().Resolve(DeviceTierPhase::PostGpu, m_facts);

    ApplyOverlay();

    HYP_LOG(Engine, Info, "Device tiers:\n{}", Describe());
}

String DeviceTierResolver::Describe() const
{
    String result;

    result += HYP_FORMAT("  platform: mobile={} battery={} cores={} ram={}MiB\n",
        m_facts.isMobilePlatform, m_facts.hasBattery, m_facts.logicalCores, m_facts.systemMemoryBytes / BytesPerMiB);

    if (m_facts.hasGpu)
    {
        result += HYP_FORMAT("  gpu: \"{}\" vram={}MiB score={}\n", m_facts.gpu.gpuModel, m_facts.gpu.vramBytes / BytesPerMiB, m_facts.score);
    }
    else
    {
        result += "  gpu: (not selected yet)\n";
    }

    const DeviceTierRegistry& registry = DeviceTierRegistry::GetInstance();

    for (const DeviceTierAxisBase* axis : registry.GetAxes())
    {
        uint64 value = 0;

        const bool isResolved = m_facts.TryGetResolved(axis->GetTypeId(), value);
        const bool isForced = m_facts.forcedAxes.Contains(axis->GetTypeId());

        result += HYP_FORMAT("  {}: {}{}\n", axis->GetName(), isResolved ? String(axis->GetValueName(value)) : String("(unresolved)"), isForced ? " (forced from command line)" : "");
    }

    result += HYP_FORMAT("  config overrides: {}\n", m_overlay ? m_overlay->Size() : 0);

    if (m_overlay)
    {
        for (const auto& entry : *m_overlay)
        {
            const auto sourceIt = m_sources.Find(entry.first);

            result += HYP_FORMAT("    {} = {}  ({})\n", entry.first, entry.second.ToString(), sourceIt != m_sources.End() ? sourceIt->second : String("?"));
        }
    }

    return result;
}

void DeviceTierResolver::LoadProfile()
{
    if (m_isProfileLoaded)
    {
        return;
    }

    m_isProfileLoaded = true;

    const FilePath profilePath = EngineGlobals::GetConfigDirectory() / "DeviceTiers.hmf";

    FileByteReader stream { profilePath };

    if (stream.Eof())
    {
        HYP_LOG(Engine, Info, "No device tier profile at {}", profilePath);

        return;
    }

    HMF::ParseResult parseResult = HMF::Parse(profilePath, stream);

    if (parseResult.HasError())
    {
        HYP_LOG(Engine, Warning, "Failed to parse device tier profile {}: {}", profilePath, parseResult.GetError().GetMessage());

        return;
    }

    if (!parseResult.GetValue().Is<TierProfile>())
    {
        HYP_LOG(Engine, Warning, "Device tier profile {} is not a TierProfile", profilePath);

        return;
    }

    const TierProfile& parsedProfile = parseResult.GetValue().Get<TierProfile>();

    for (const TierRule& rule : parsedProfile.rules)
    {
        if (IsValidCondition(rule, rule.when, "When") && IsValidCondition(rule, rule.atMost, "AtMost"))
        {
            m_profile.rules.PushBack(rule);
        }
    }

    HYP_LOG(Engine, Info, "Loaded {} device tier rules from {}", m_profile.rules.Size(), profilePath);
}

void DeviceTierResolver::ApplyOverlay()
{
    SharedPtr<ConfigOverlay> overlay = MakeShared<ConfigOverlay>();

    m_sources.Clear();

    BuildTierOverlay(m_facts, m_profile, *overlay, &m_sources);

    m_overlay = SharedPtr<const ConfigOverlay>(std::move(overlay));

    ConfigBase::SetOverlay("EngineConfig", m_overlay);

    EngineConfig engineConfig;
    engineConfig.Load();

    CVarManager::GetInstance().InitFromConfig(engineConfig);
}

} // namespace Hyperion
