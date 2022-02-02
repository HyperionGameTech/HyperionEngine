#ifndef GI_VOXEL_DEBUG_SHADER2_H
#define GI_VOXEL_DEBUG_SHADER2_H

#include "../lighting_shader.h"

namespace hyperion {
    class GIVoxelDebugShader2 : public LightingShader {
    public:
        GIVoxelDebugShader2(const ShaderProperties &properties);
        virtual ~GIVoxelDebugShader2() = default;

        virtual void ApplyMaterial(const Material &mat);
        virtual void ApplyTransforms(const Transform &transform, Camera *camera);
    };
} // namespace hyperion

#endif
