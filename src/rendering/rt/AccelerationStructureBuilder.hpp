#ifndef HYPERION_V2_ACCELERATION_STRUCTURE_BUILDER_H
#define HYPERION_V2_ACCELERATION_STRUCTURE_BUILDER_H

#include <scene/Entity.hpp>
#include <core/lib/DynArray.hpp>

#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::BottomLevelAccelerationStructure;
using renderer::AccelerationGeometry;

class Engine;

class AccelerationStructureBuilder {
public:
    AccelerationStructureBuilder() = default;
    AccelerationStructureBuilder(std::vector<Handle<Entity>> &&entities);
    AccelerationStructureBuilder(const AccelerationStructureBuilder &other) = delete;
    AccelerationStructureBuilder &operator=(const AccelerationStructureBuilder &other) = delete;
    ~AccelerationStructureBuilder() = default;

    void AddEntity(Handle<Entity> &&entity) { m_entities.push_back(std::move(entity)); }

    std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> Build(Engine *engine);

private:
    std::vector<Handle<Entity>> m_entities;
};

} // namespace hyperion::v2

#endif