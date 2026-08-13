/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/Util/ShaderPropertyDictionary.hpp>
#include <Rendering/Shared.hpp>

namespace Hyperion {

ENGINE_API ShaderPropertyDictionary& ShaderPropertyDictionary::GetInstance()
{
    static ShaderPropertyDictionary s_instance;
    return s_instance;
}

StaticShaderPropertyId* StaticShaderPropertyId::s_head = nullptr;

} // namespace Hyperion
