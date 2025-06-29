#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderSkeleton.hpp>

#include <scene/Entity.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

extern HYP_API SafeDeleter* g_safe_deleter;

#pragma region RenderProxy

void RenderProxy::SafeRelease()
{
    g_safe_deleter->SafeRelease(std::move(mesh));
    g_safe_deleter->SafeRelease(std::move(material));
    g_safe_deleter->SafeRelease(std::move(skeleton));
}

void RenderProxy::IncRefs() const
{
    if (material.IsValid())
    {
        material->GetRenderResource().IncRef();
    }

    if (mesh.IsValid())
    {
        mesh->GetRenderResource().IncRef();
    }

    if (skeleton.IsValid())
    {
        skeleton->GetRenderResource().IncRef();
    }
}

void RenderProxy::DecRefs() const
{
    if (material.IsValid())
    {
        material->GetRenderResource().DecRef();
    }

    if (mesh.IsValid())
    {
        mesh->GetRenderResource().DecRef();
    }

    if (skeleton.IsValid())
    {
        skeleton->GetRenderResource().DecRef();
    }
}

#pragma endregion RenderProxy

} // namespace hyperion