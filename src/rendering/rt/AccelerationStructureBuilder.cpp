#include "AccelerationStructureBuilder.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

AccelerationStructureBuilder::AccelerationStructureBuilder(std::vector<Ref<Spatial>> &&spatials)
    : m_spatials(std::move(spatials))
{
}

std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> AccelerationStructureBuilder::Build(Engine *engine)
{
    if (m_spatials.empty()) {
        return {};
    }

    std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> acceleration_structures(m_spatials.size());

    for (auto &spatial : m_spatials) {
        std::unique_ptr<AccelerationGeometry> geometry;

        if (auto &mesh = spatial->GetMesh()) {
            geometry = std::make_unique<AccelerationGeometry>(
                mesh->BuildPackedVertices(),
                mesh->BuildPackedIndices()
            );
        }

        auto acceleration_structure = std::make_unique<BottomLevelAccelerationStructure>();
        acceleration_structure->SetTransform(spatial->GetTransform().GetMatrix());
        acceleration_structure->AddGeometry(std::move(geometry));

        HYPERION_ASSERT_RESULT(acceleration_structure->Create(engine->GetDevice(), engine->GetInstance()));

        acceleration_structures.push_back(std::move(acceleration_structure));
    }

    m_spatials.clear();

    return acceleration_structures;
}

} // namespace hyperion::v2