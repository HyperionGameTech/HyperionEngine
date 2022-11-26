#include "AccelerationStructureBuilder.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

AccelerationStructureBuilder::AccelerationStructureBuilder(std::vector<Handle<Entity>> &&entities)
    : m_entities(std::move(entities))
{
}

std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> AccelerationStructureBuilder::Build()
{
    if (m_entities.empty()) {
        return { };
    }

    std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> acceleration_structures(m_entities.size());

    for (auto &entity : m_entities) {
        std::unique_ptr<AccelerationGeometry> geometry;

        if (auto &mesh = entity->GetMesh()) {
            geometry = std::make_unique<AccelerationGeometry>(
                mesh->BuildPackedVertices(),
                mesh->BuildPackedIndices(),
                0u, // TODO
                0u // TODO
            );
        }

        auto acceleration_structure = std::make_unique<BottomLevelAccelerationStructure>();
        acceleration_structure->SetTransform(entity->GetTransform().GetMatrix());
        acceleration_structure->AddGeometry(std::move(geometry));

        HYPERION_ASSERT_RESULT(acceleration_structure->Create(Engine::Get()->GetGPUDevice(), Engine::Get()->GetGPUInstance()));

        acceleration_structures.push_back(std::move(acceleration_structure));
    }

    m_entities.clear();

    return acceleration_structures;
}

} // namespace hyperion::v2