/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Config/Config.hpp>

namespace Hyperion {

class EngineConfig final : public Config<EngineConfig>
{
public:
    EngineConfig()
        : Config<EngineConfig>("EngineConfig")
    {
    }

    EngineConfig(const EngineConfig& other)
        : Config<EngineConfig>(static_cast<const Config<EngineConfig>&>(other))
    {
    }

    EngineConfig& operator=(const EngineConfig& other)
    {
        Config<EngineConfig>::operator=(static_cast<const Config<EngineConfig>&>(other));
        return *this;
    }

    EngineConfig(EngineConfig&& other) noexcept
        : Config<EngineConfig>(static_cast<Config<EngineConfig>&&>(other))
    {
    }

    EngineConfig& operator=(EngineConfig&& other) noexcept
    {
        Config<EngineConfig>::operator=(static_cast<Config<EngineConfig>&&>(other));
        return *this;
    }

    ~EngineConfig() override = default;

    HYP_FORCE_INLINE const ConfigValue& Get(UTF8StringView key) const
    {
        return Config<EngineConfig>::Get(key);
    }

    HYP_FORCE_INLINE void Set(UTF8StringView key, const ConfigValue& value)
    {
        Config<EngineConfig>::Set(key, value);
    }
};

} // namespace Hyperion
