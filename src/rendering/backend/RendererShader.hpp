#ifndef HYPERION_V2_BACKEND_RENDERER_SHADER_H
#define HYPERION_V2_BACKEND_RENDERER_SHADER_H

#include <rendering/backend/Platform.hpp>

#include <core/lib/ByteBuffer.hpp>
#include <HashCode.hpp>

#include <util/Defines.hpp>

namespace hyperion {
namespace renderer {

struct ShaderObject
{
    ByteBuffer bytes;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(bytes);

        return hc;
    }
};

enum ShaderModuleType : uint
{
    UNSET = 0,
    VERTEX,
    FRAGMENT,
    GEOMETRY,
    COMPUTE,
    /* Mesh shaders */
    TASK,
    MESH,
    /* Tesselation */
    TESS_CONTROL,
    TESS_EVAL,
    /* Raytracing */
    RAY_GEN,
    RAY_INTERSECT,
    RAY_ANY_HIT,
    RAY_CLOSEST_HIT,
    RAY_MISS,

    MAX
};

namespace platform {

template <PlatformType PLATFORM>
struct ShaderModule;

template <PlatformType PLATFORM>
struct ShaderGroup;

template <PlatformType PLATFORM>
class ShaderProgram
{
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererShader.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using ShaderProgram = platform::ShaderProgram<Platform::CURRENT>;
using ShaderModule = platform::ShaderModule<Platform::CURRENT>;
using ShaderGroup = platform::ShaderGroup<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif