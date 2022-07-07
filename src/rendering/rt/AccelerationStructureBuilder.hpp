#ifndef HYPERION_V2_ACCELERATION_STRUCTURE_BUILDER_H
#define HYPERION_V2_ACCELERATION_STRUCTURE_BUILDER_H

#include <scene/Spatial.hpp>

#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::BottomLevelAccelerationStructure;
using renderer::AccelerationGeometry;

class Engine;

class AccelerationStructureBuilder {
public:
    AccelerationStructureBuilder() = default;
    AccelerationStructureBuilder(std::vector<Ref<Spatial>> &&spatials);
    AccelerationStructureBuilder(const AccelerationStructureBuilder &other) = delete;
    AccelerationStructureBuilder &operator=(const AccelerationStructureBuilder &other) = delete;
    ~AccelerationStructureBuilder() = default;

    void AddSpatial(Ref<Spatial> &&spatial) { m_spatials.push_back(std::move(spatial)); }

    std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> Build(Engine *engine);

private:
    std::vector<Ref<Spatial>> m_spatials;
};

} // namespace hyperion::v2

#endif