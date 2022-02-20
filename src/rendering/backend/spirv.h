#ifndef BACKEND_SPIRV_H
#define BACKEND_SPIRV_H

#include "../../types.h"

#include <vector>

namespace hyperion {

struct SPIRVObject {
    using Raw_t = std::vector<byte>;

    enum class Type : int {
        Vertex = 0,
        Fragment,
        Geometry,
        Compute,
        /* Mesh shaders */
        Task,
        Mesh,
        /* Tesselation */
        TessControl,
        TessEval,
        /* Raytracing */
        RayGen,
        RayIntersect,
        RayAnyHit,
        RayClosestHit,
        RayMiss,
    };

    Raw_t raw;
    Type type;

    SPIRVObject(Type type) : type(type) {}
    SPIRVObject(Type type, const Raw_t &raw) : type(type), raw(raw) {}
    SPIRVObject(const SPIRVObject &other) : type(other.type), raw(other.raw) {}

    inline const uint32_t *VkCode() const { return reinterpret_cast<const uint32_t*>(raw.data()); }
};

} // namespace hyperion

#endif