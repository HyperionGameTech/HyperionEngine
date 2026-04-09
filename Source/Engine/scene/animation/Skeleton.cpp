/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/animation/Skeleton.hpp>
#include <scene/animation/Bone.hpp>
#include <scene/animation/Animation.hpp>

#include <rendering/RenderProxy.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <engine/EngineDriver.hpp>

#include <Skeleton.generated.inl>

namespace Hyperion {

static const Name s_nameSkeletonDefault = NAME("<unnamed skeleton>");

Skeleton::Skeleton()
    : m_renderProxyVersion(0)
{
}

Skeleton::Skeleton(const Handle<Bone>& rootBone)
    : m_rootBone(rootBone),
      m_renderProxyVersion(0)
{
    if (m_rootBone)
    {
        m_rootBone->SetSkeleton(this);
    }
}

Skeleton::~Skeleton()
{
    if (m_rootBone)
    {
        m_rootBone->SetSkeleton(nullptr);
    }

    SetReady(false);
}

void Skeleton::Init()
{
    HYP_SCOPE;

    AssetObject::Init();

    SetReady(true);
}

Bone* Skeleton::FindBone(StringHash name) const
{
    HYP_SCOPE;

    if (!m_rootBone)
    {
        return nullptr;
    }

    if (m_rootBone->GetBoneName() == name)
    {
        return static_cast<Bone*>(m_rootBone.Get());
    }

    // @TODO Profile this

    for (Node* node : m_rootBone->GetDescendants())
    {
        if (!node)
        {
            continue;
        }

        Bone* bone = ObjCast<Bone>(node);

        if (!bone)
        {
            continue;
        }

        if (bone->GetBoneName() == name)
        {
            return bone;
        }
    }

    return nullptr;
}

uint32 Skeleton::FindBoneIndex(StringHash name) const
{
    HYP_SCOPE;

    if (!m_rootBone)
    {
        return uint32(-1);
    }

    uint32 index = 0;

    if (m_rootBone->GetName() == name)
    {
        return index;
    }

    for (Node* node : m_rootBone->GetDescendants())
    {
        ++index;

        if (!node)
        {
            continue;
        }

        Bone* bone = ObjCast<Bone>(node);

        if (!bone)
        {
            continue;
        }

        if (bone->GetName() == name)
        {
            return index;
        }
    }

    return uint32(-1);
}

const Handle<Bone>& Skeleton::GetRootBone() const
{
    return m_rootBone;
}

void Skeleton::SetRootBone(const Handle<Bone>& bone)
{
    HYP_SCOPE;

    if (m_rootBone)
    {
        m_rootBone->SetSkeleton(nullptr);

        m_rootBone.Reset();
    }

    if (!bone)
    {
        return;
    }

    m_rootBone = bone;
    m_rootBone->SetSkeleton(this);
}

void Skeleton::UpdateRenderProxy(RenderProxySkeleton* proxy)
{
    HYP_SCOPE;

    proxy->skeleton = WeakHandleFromThis();

    if (m_rootBone != nullptr)
    {
        SkeletonShaderData& bufferData = proxy->bufferData;
        bufferData.bones[0] = m_rootBone->GetBoneMatrix();

        uint32 descendantIndex = 1;

        for (Node* descendant : m_rootBone->GetDescendants())
        {
            if (descendantIndex >= SkeletonShaderData::maxBones)
            {
                HYP_LOG_ONCE(Animation, Warning, "Skeleton has more bones than supported by the shader ({}). Some bones will be ignored in skinning.", SkeletonShaderData::maxBones);
                break;
            }

            if (!descendant)
            {
                ++descendantIndex;
                continue;
            }

            Bone* bone = ObjCast<Bone>(descendant);

            if (!bone)
            {
                ++descendantIndex;
                continue;
            }

            bufferData.bones[descendantIndex] = bone->GetBoneMatrix();

            ++descendantIndex;
        }
    }
}

} // namespace Hyperion
