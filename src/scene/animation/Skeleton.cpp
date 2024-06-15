/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/animation/Skeleton.hpp>
#include <scene/animation/Bone.hpp>

#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererResult.hpp>

#include <core/HypClassUtils.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DEFINE_CLASS(Skeleton);

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(UpdateSkeletonRenderData) : renderer::RenderCommand
{
    ID<Skeleton> id;
    SkeletonBoneData bone_data;

    RENDER_COMMAND(UpdateSkeletonRenderData)(ID<Skeleton> id, const SkeletonBoneData &bone_data)
        : id(id),
          bone_data(bone_data)
    {
    }

    virtual Result operator()() override
    {
        SkeletonShaderData shader_data;
        Memory::MemCpy(shader_data.bones, bone_data.matrices->Data(), sizeof(Matrix4) * MathUtil::Min(std::size(shader_data.bones), bone_data.matrices->Size()));

        g_engine->GetRenderData()->skeletons.Set(id.ToIndex(), shader_data);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

Skeleton::Skeleton()
    : BasicObject(),
      m_root_bone(nullptr),
      m_mutation_state(DataMutationState::CLEAN)
{
}

Skeleton::Skeleton(const NodeProxy &root_bone)
    : BasicObject(),
      m_root_bone(nullptr),
      m_mutation_state(DataMutationState::CLEAN)
{
    if (root_bone.IsValid() && root_bone->GetType() == Node::Type::BONE) {
        m_root_bone = root_bone;
        static_cast<Bone *>(m_root_bone.Get())->SetSkeleton(this);
    }
}

Skeleton::~Skeleton()
{
    SetReady(false);
}

void Skeleton::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    m_mutation_state |= DataMutationState::DIRTY;
    
    Update(0.0166f);

    SetReady(true);
}

void Skeleton::Update(GameCounter::TickUnit)
{
    if (!m_mutation_state.IsDirty()) {
        return;
    }

    const SizeType num_bones = MathUtil::Min(SkeletonShaderData::max_bones, NumBones());

    if (num_bones != 0) {
        m_bone_data.SetMatrix(0, static_cast<Bone *>(m_root_bone.Get())->GetBoneMatrix());

        for (SizeType i = 1; i < num_bones; i++) {
            if (auto &descendent = m_root_bone->GetDescendents()[i - 1]) {
                if (!descendent.IsValid()) {
                    continue;
                }

                if (descendent->GetType() != Node::Type::BONE) {
                    continue;
                }

                m_bone_data.SetMatrix(i, static_cast<const Bone *>(descendent.Get())->GetBoneMatrix());
            }
        }

        PUSH_RENDER_COMMAND(UpdateSkeletonRenderData, GetID(), m_bone_data);
    }
    
    m_mutation_state = DataMutationState::CLEAN;
}

Bone *Skeleton::FindBone(const String &name)
{
    if (!m_root_bone) {
        return nullptr;
    }

    if (m_root_bone.GetName() == name) {
        return static_cast<Bone *>(m_root_bone.Get());
    }

    for (auto &node : m_root_bone->GetDescendents()) {
        if (!node) {
            continue;
        }

        if (node->GetType() != Node::Type::BONE) {
            continue;
        }

        Bone *bone = static_cast<Bone *>(node.Get());  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        if (bone->GetName() == name) {
            return bone;
        }
    }

    return nullptr;
}

uint Skeleton::FindBoneIndex(const String &name) const
{
    if (!m_root_bone) {
        return uint(-1);
    }

    uint index = 0;

    if (m_root_bone.GetName() == name) {
        return index;
    }

    for (auto &node : m_root_bone->GetDescendents()) {
        ++index;

        if (!node) {
            continue;
        }

        if (node->GetType() != Node::Type::BONE) {
            continue;
        }

        const Bone *bone = static_cast<const Bone *>(node.Get());  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        if (bone->GetName() == name) {
            return index;
        }
    }

    return uint(-1);
}

Bone *Skeleton::GetRootBone()
{
    return static_cast<Bone *>(m_root_bone.Get());
}

const Bone *Skeleton::GetRootBone() const
{
    return static_cast<const Bone *>(m_root_bone.Get());
}

void Skeleton::SetRootBone(const NodeProxy &root_bone)
{
    if (m_root_bone) {
        static_cast<Bone *>(m_root_bone.Get())->SetSkeleton(nullptr);

        m_root_bone = NodeProxy();
    }

    if (!root_bone.IsValid() || root_bone->GetType() != Node::Type::BONE) {
        return;
    }

    m_root_bone = root_bone;
    static_cast<Bone *>(m_root_bone.Get())->SetSkeleton(this);
}

SizeType Skeleton::NumBones() const
{
    if (!m_root_bone.IsValid()) {
        return 0;
    }

    return 1 + m_root_bone->GetDescendents().Size();
}

void Skeleton::AddAnimation(Animation &&animation)
{
    for (auto &track : animation.GetTracks()) {
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

const Animation *Skeleton::FindAnimation(const String &name, uint *out_index) const
{
    const auto it = m_animations.FindIf([&name](const auto &item) {
        return item.GetName() == name;
    });

    if (it == m_animations.End()) {
        return nullptr;
    }

    if (out_index != nullptr) {
        *out_index = m_animations.IndexOf(it);
    }

    return it;
}

} // namespace hyperion
