/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Config/Config.hpp>

namespace Hyperion {

class EditorConfig final : public Config<EditorConfig>
{
public:
    EditorConfig()
        : Config<EditorConfig>("EditorConfig")
    {
    }

    EditorConfig(const EditorConfig& other)
        : Config<EditorConfig>(static_cast<const Config<EditorConfig>&>(other))
    {
    }

    EditorConfig& operator=(const EditorConfig& other)
    {
        Config<EditorConfig>::operator=(static_cast<const Config<EditorConfig>&>(other));
        return *this;
    }

    EditorConfig(EditorConfig&& other) noexcept
        : Config<EditorConfig>(static_cast<Config<EditorConfig>&&>(other))
    {
    }

    EditorConfig& operator=(EditorConfig&& other) noexcept
    {
        Config<EditorConfig>::operator=(static_cast<Config<EditorConfig>&&>(other));
        return *this;
    }

    ~EditorConfig() override = default;

    HYP_FORCE_INLINE const ConfigValue& Get(UTF8StringView key) const
    {
        return Config<EditorConfig>::Get(key);
    }

    HYP_FORCE_INLINE void Set(UTF8StringView key, const ConfigValue& value)
    {
        Config<EditorConfig>::Set(key, value);
    }
};

} // namespace Hyperion
