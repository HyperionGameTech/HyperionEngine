#include "Skeleton.hpp"
#include "Bone.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

Skeleton::Skeleton()
    : EngineComponentBase(),
      m_root_bone(nullptr),
      m_shader_data_state(ShaderDataState::DIRTY)
{
}

Skeleton::Skeleton(std::unique_ptr<Bone> &&root_bone)
    : EngineComponentBase(),
      m_root_bone(std::move(root_bone))
{
    if (m_root_bone != nullptr) {
        m_root_bone->SetSkeleton(this);
    }
}

Skeleton::~Skeleton()
{
    Teardown();
}

void Skeleton::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    EnqueueRenderUpdates();

    SetReady(true);

    OnTeardown([this]() {
        HYP_FLUSH_RENDER_QUEUE();
        
        SetReady(false);
    });
}

void Skeleton::EnqueueRenderUpdates() const
{
    if (!m_shader_data_state.IsDirty()) {
        return;
    }

    const auto num_bones = MathUtil::Min(SkeletonShaderData::max_bones, NumBones());

    if (num_bones != 0) {
        SkeletonShaderData &shader_data = Engine::Get()->GetRenderData()->skeletons.Get(m_id.value - 1); /* TODO: is this fully thread safe? */

        shader_data.bones[0] = m_root_bone->GetBoneMatrix();

        for (SizeType i = 1; i < num_bones; i++) {
            if (auto &descendent = m_root_bone->GetDescendents()[i - 1]) {
                if (!descendent) {
                    continue;
                }

                if (descendent.Get()->GetType() != Node::Type::BONE) {
                    continue;
                }

                shader_data.bones[i] = static_cast<const Bone *>(descendent.Get())->GetBoneMatrix();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            }
        }

        Engine::Get()->GetRenderData()->skeletons.Set(m_id.value - 1, shader_data);
    }
    
    m_shader_data_state = ShaderDataState::CLEAN;
}

Bone *Skeleton::FindBone(const String &name)
{
    if (m_root_bone == nullptr) {
        return nullptr;
    }

    if (m_root_bone->GetName() == name) {
        return m_root_bone.get();
    }

    for (auto &node : m_root_bone->GetDescendents()) {
        if (!node) {
            continue;
        }

        if (node.Get()->GetType() != Node::Type::BONE) {
            continue;
        }

        auto *bone = static_cast<Bone *>(node.Get());  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        if (bone->GetName() == name) {
            return bone;
        }
    }

    return nullptr;
}

void Skeleton::SetRootBone(std::unique_ptr<Bone> &&root_bone)
{
    m_root_bone = std::move(root_bone);

    if (m_root_bone != nullptr) {
        m_root_bone->SetSkeleton(this);
    }
}

SizeType Skeleton::NumBones() const
{
    if (m_root_bone == nullptr) {
        return 0;
    }

    return 1 + m_root_bone->GetDescendents().Size();
}

void Skeleton::AddAnimation(std::unique_ptr<Animation> &&animation)
{
    if (animation == nullptr) {
        return;
    }

    for (auto &track : animation->GetTracks()) {
        track.bone = nullptr;

        if (track.bone_name.Empty()) {
            continue;
        }

        track.bone = FindBone(track.bone_name);

        if (track.bone == nullptr) {
            DebugLog(
                LogType::Warn,
                "Skeleton could not find bone with name \"%s\"\n",
                track.bone_name.Data()
            );
        }
    }

    m_animations.PushBack(std::move(animation));
}

Animation *Skeleton::FindAnimation(const String &name, UInt *out_index) const
{
    const auto it = m_animations.FindIf([&name](const auto &item) {
        return item->GetName() == name;
    });

    if (it == m_animations.End()) {
        return nullptr;
    }

    if (out_index != nullptr) {
        *out_index = m_animations.IndexOf(it);
    }

    return it->get();
}

} // namespace hyperion::v2
