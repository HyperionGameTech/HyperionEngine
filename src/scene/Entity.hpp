#ifndef HYPERION_V2_SPATIAL_H
#define HYPERION_V2_SPATIAL_H

#include <core/Base.hpp>
#include <rendering/Material.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/rt/BLAS.hpp>
#include <scene/animation/Skeleton.hpp>
#include <scene/VisibilityState.hpp>
#include <scene/Controller.hpp>
#include <math/Transform.hpp>
#include <math/BoundingBox.hpp>
#include <core/Scheduler.hpp>
#include <core/lib/FlatSet.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <util/Defines.hpp>

#include <mutex>
#include <vector>

namespace hyperion::v2 {

using renderer::VertexAttributeSet;
using renderer::AccelerationStructure;
using renderer::AccelerationGeometry;
using renderer::FaceCullMode;

class Entity : public BasicObject<STUB_CLASS(Entity)>
{
public:
    Entity();
    Entity(Name);
    ~Entity() = default;

    Bool IsReady() const;
    void Init();
};

} // namespace hyperion::v2

#endif
