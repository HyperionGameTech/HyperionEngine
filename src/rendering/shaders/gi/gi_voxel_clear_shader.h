#ifndef GI_VOXEL_CLEAR_SHADER_H
#define GI_VOXEL_CLEAR_SHADER_H

#include "../compute/compute_shader.h"

namespace hyperion {
class GIVoxelClearShader : public ComputeShader {
public:
    GIVoxelClearShader(const ShaderProperties &properties);
    virtual ~GIVoxelClearShader() = default;
};
} // namespace hyperion

#endif
