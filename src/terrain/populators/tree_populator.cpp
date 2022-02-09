#include "tree_populator.h"

#include "../../asset/asset_manager.h"
#include "../../rendering/shader_manager.h"
#include "../../rendering/shaders/vegetation_shader.h"

namespace hyperion {
TreePopulator::TreePopulator(
    Camera *camera,
    unsigned long seed,
    double probability_factor,
    float tolerance,
    float max_distance,
    float spread,
    int num_entities_per_chunk,
    int num_patches
)
    : Populator(
        fbom::FBOMObjectType("TREE_POPULATOR_CONTROL"),
        camera,
        seed,
        probability_factor,
        tolerance,
        max_distance,
        spread,
        num_entities_per_chunk,
        num_patches
    )
{
}

TreePopulator::~TreePopulator()
{
}

std::shared_ptr<Entity> TreePopulator::CreateEntity(const Vector3 &position) const
{
    auto tree = AssetManager::GetInstance()->LoadFromFile<Entity>("models/pine/LoblollyPine.obj", true);

    for (int i = 0; i < tree->NumChildren(); i++) {
        tree->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.0f);
        tree->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.9f);
    }

    tree->GetChild("LoblollyPineNeedles_1")->GetMaterial().cull_faces = MaterialFaceCull::MaterialFace_None;
    // tree->GetChild("LoblollyPineNeedles_1")->GetMaterial().alpha_blended = true;
    tree->GetChild("LoblollyPineNeedles_1")->GetRenderable()->SetShader(
        ShaderManager::GetInstance()->GetShader<VegetationShader>(
            ShaderProperties()
                .Define("VEGETATION_FADE", false)
                .Define("VEGETATION_LIGHTING", true)
        )
    );
    tree->GetChild("LoblollyPineNeedles_1")->GetRenderable()->SetRenderBucket(Renderable::RB_TRANSPARENT);
    tree->SetLocalTranslation(position);
    tree->SetLocalScale(Vector3(1.3f) + MathUtil::Random(-0.5f, 0.5f));
    tree->SetLocalRotation(Quaternion(Vector3::UnitY(), MathUtil::DegToRad(MathUtil::Random(0, 359))));

    return tree;
}

std::shared_ptr<EntityControl> TreePopulator::CloneImpl()
{
    return std::make_shared<TreePopulator>(nullptr, m_seed, m_probability_factor,
        m_tolerance, m_max_distance, m_spread, m_num_entities_per_chunk,
        m_num_patches); // TODO
}
} // namespace hyperion
