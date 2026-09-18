/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/MaterialTypes.hpp>

#include <MaterialTypes.generated.inl>

namespace Hyperion {

const MaterialParameters& MaterialParameters::Defaults()
{
    static MaterialParameters s_defaults { NoInit };
        
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag, []()
                    {
                        memset(&s_defaults, 0, sizeof(MaterialParameters));

                        s_defaults.albedo = Vec4f::One();
                        s_defaults.roughness = 1.0f;
                        s_defaults.parallaxHeightScale = 0.02f;
                        s_defaults.ior = 1.5f;
                        s_defaults.uvScale = Vec2f::One();
                    });

    return s_defaults;
}

} // namespace Hyperion
