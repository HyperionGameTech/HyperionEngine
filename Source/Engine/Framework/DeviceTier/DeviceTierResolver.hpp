/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Config/Config.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Framework/DeviceTier/DeviceFacts.hpp>
#include <Framework/DeviceTier/TierProfile.hpp>

namespace Hyperion {

class DeviceDetails;

ENGINE_API void BuildTierOverlay(const DeviceFacts& facts, const TierProfile& profile, ConfigOverlay& outOverlay, Map<String, String>* outSources = nullptr);

class ENGINE_API DeviceTierResolver
{
public:
    static DeviceTierResolver& GetInstance();

    void ResolvePreGpu();
    void ResolvePostGpu(const DeviceDetails& deviceDetails);

    String Describe() const;

    HYP_FORCE_INLINE const DeviceFacts& GetFacts() const
    {
        return m_facts;
    }

private:
    void LoadProfile();
    void ApplyOverlay();

    DeviceFacts m_facts;
    TierProfile m_profile;
    bool m_isProfileLoaded = false;

    SharedPtr<const ConfigOverlay> m_overlay;
    Map<String, String> m_sources;
};

} // namespace Hyperion
