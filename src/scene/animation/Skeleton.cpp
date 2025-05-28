/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/animation/Skeleton.hpp>
#include <scene/animation/Bone.hpp>
#include <scene/animation/Animation.hpp>

#include <rendering/RenderSkeleton.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/backend/RendererResult.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

Skeleton::Skeleton()
    : HypObject(),
      m_mutation_state(DataMutationState::CLEAN)
{
}

Skeleton::Skeleton(const Handle<Bone>& root_bone)
    : HypObject(),
      m_root_bone(root_bone),
      m_mutation_state(DataMutationState::CLEAN)
{
    if (m_root_bone)
    {
        m_root_bone->SetSkeleton(this);
    }
}

Skeleton::~Skeleton()
{
    if (m_render_resource != nullptr)
    {
        FreeResource(m_render_resource);

        m_render_resource = nullptr;
    }

    if (m_root_bone)
    {
        m_root_bone->SetSkeleton(nullptr);
    }

    SetReady(false);
}

void Skeleton::Init()
{
    if (IsInitCalled())
    {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            if (m_render_resource != nullptr)
            {
                FreeResource(m_render_resource);

                m_render_resource = nullptr;
            }
        }));

    m_render_resource = AllocateResource<RenderSkeleton>(this);

    m_mutation_state |= DataMutationState::DIRTY;

    Update(0.0166f);

    SetReady(true);
}

void Skeleton::Update(GameCounter::TickUnit)
{
    if (!m_mutation_state.IsDirty())
    {
        return;
    }

    const SizeType num_bones = MathUtil::Min(SkeletonShaderData::max_bones, NumBones());

    if (num_bones != 0)
    {
        SkeletonShaderData shader_data {};
        shader_data.bones[0] = m_root_bone->GetBoneMatrix();

        for (SizeType i = 1; i < num_bones; i++)
        {
            if (Node* descendent = m_root_bone->GetDescendants()[i - 1])
            {
                if (!descendent)
                {
                    continue;
                }

                if (descendent->GetType() != Node::Type::BONE)
                {
                    continue;
                }

                shader_data.bones[i] = static_cast<const Bone*>(descendent)->GetBoneMatrix();
            }
        }

        m_render_resource->SetBufferData(shader_data);
    }

    m_mutation_state = DataMutationState::CLEAN;
}

Bone* Skeleton::FindBone(UTF8StringView name) const
{
    if (!m_root_bone)
    {
        return nullptr;
    }

    if (m_root_bone->GetName() == name)
    {
        return static_cast<Bone*>(m_root_bone.Get());
    }

    for (Node* node : m_root_bone->GetDescendants())
    {
        if (!node)
        {
            continue;
        }

        if (node->GetType() != Node::Type::BONE)
        {
            continue;
        }

        Bone* bone = static_cast<Bone*>(node); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        if (bone->GetName() == name)
        {
            return bone;
        }
    }

    return nullptr;
}

uint32 Skeleton::FindBoneIndex(UTF8StringView name) const
{
    if (!m_root_bone)
    {
        return uint32(-1);
    }

    uint32 index = 0;

    if (m_root_bone->GetName() == name)
    {
        return index;
    }

    for (Node* node : m_root_bone->GetDescendants())
    {
        ++index;

        if (!node)
        {
            continue;
        }

        if (node->GetType() != Node::Type::BONE)
        {
            continue;
        }

        const Bone* bone = static_cast<const Bone*>(node); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        if (bone->GetName() == name)
        {
            return index;
        }
    }

    return uint32(-1);
}

const Handle<Bone>& Skeleton::GetRootBone() const
{
    return m_root_bone;
}

void Skeleton::SetRootBone(const Handle<Bone>& bone)
{
    if (m_root_bone)
    {
        m_root_bone->SetSkeleton(nullptr);

        m_root_bone.Reset();
    }

    if (!bone)
    {
        return;
    }

    m_root_bone = bone;
    m_root_bone->SetSkeleton(this);
}

SizeType Skeleton::NumBones() const
{
    if (!m_root_bone)
    {
        return 0;
    }

    return 1 + m_root_bone->GetDescendants().Size();
}

void Skeleton::AddAnimation(const Handle<Animation>& animation)
{
    if (!animation)
    {
        return;
    }

    for (const Handle<AnimationTrack>& track : animation->GetTracks())
    {
        if (track->GetDesc().bone_name.Empty())
        {
            track->SetBone(nullptr);

            continue;
        }

        track->SetBone(FindBone(track->GetDesc().bone_name));

        if (!track->GetBone())
        {
            HYP_LOG(Animation, Warning, "Skeleton could not find bone with name '{}'", track->GetDesc().bone_name);
        }
    }

    m_animations.PushBack(animation);
}

const Animation* Skeleton::FindAnimation(UTF8StringView name, uint32* out_index) const
{
    const auto it = m_animations.FindIf([&name](const auto& item)
        {
            return item->GetName() == name;
        });

    if (it == m_animations.End())
    {
        return nullptr;
    }

    if (out_index != nullptr)
    {
        *out_index = m_animations.IndexOf(it);
    }

    return it->Get();
}

} // namespace hyperion
