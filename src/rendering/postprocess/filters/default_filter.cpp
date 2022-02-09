#include "./default_filter.h"
#include "../../../asset/asset_manager.h"
#include "../../shader_manager.h"
#include "../../shaders/post/default_post_shader.h"

namespace hyperion {

DefaultFilter::DefaultFilter()
    : PostFilter(ShaderManager::GetInstance()->GetShader<DefaultPostShader>(ShaderProperties()))
{
}

void DefaultFilter::SetUniforms(Camera *cam)
{
}

} // namespace hyperion
