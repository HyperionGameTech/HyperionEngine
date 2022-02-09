#include "renderable.h"

namespace hyperion {

Renderable::Renderable(const fbom::FBOMType &loadable_type,
    RenderBucket bucket)
    : fbom::FBOMLoadable(fbom::FBOMObjectType("RENDERABLE").Extend(loadable_type)),
      m_bucket(bucket)
{
}

bool Renderable::IntersectRay(const Ray &ray, const Transform &transform, RaytestHit &out) const
{
    return (m_aabb * transform).IntersectRay(ray, out);
}

bool Renderable::IntersectRay(const Ray &ray, const Transform &transform, RaytestHitList_t &out) const
{
    RaytestHit intersection;

    if (IntersectRay(ray, transform, intersection)) {
        out.push_back(intersection);

        return true;
    }

    return false;
}

std::shared_ptr<Loadable> Renderable::Clone()
{
    return CloneImpl();
}

} // namespace hyperion
