/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>

#include <Core/Math/Transform.hpp>
#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/BoundingSphere.hpp>

#include <Scene/Entity.hpp>

#include <Scene/Decal/DecalTypes.hpp>

namespace Hyperion {

class Decal;
class Material;

/// Entity type, representing to any N DecalInstance in the Scene for a specific Decal asset 
HYP_CLASS()
class ENGINE_API DecalProxy final : public Entity
{
    HYP_OBJECT_BODY(DecalProxy);

public:
    DecalProxy();
    explicit DecalProxy(const Handle<Decal>& decal);

    DecalProxy(const DecalProxy&) = delete;
    DecalProxy& operator=(const DecalProxy&) = delete;

    ~DecalProxy() override;

    HYP_METHOD(Property = "Decal", Serialize, Editor)
    HYP_FORCE_INLINE const Handle<Decal>& GetDecal() const
    {
        return m_decal;
    }

    HYP_METHOD(Property = "Decal", Serialize, Editor)
    void SetDecal(const Handle<Decal>& decal);

    HYP_METHOD()
    HYP_NODISCARD DecalId AddDecal(const Transform& transform);

    /// Adds the instance keeping its id. Does nothing and returns false if the id is already in use.
    bool AddDecalWithId(const DecalInstance& instance);

    /// Removes decal for \p id if present, returning true on success.
    /// returns false if the Decal was not removed
    bool RemoveDecal(DecalId id);

    /// Removes every decal whose origin lies within the sphere.
    /// DecalInstances that were removed are placed into \p outRemovedDecalInstances
    void RemoveDecalsInSphere(
        const BoundingSphere& bounds,
        Array<DecalInstance, SceneAllocator>& outRemovedDecalInstances);

    HYP_FORCE_INLINE const Array<DecalInstance>& GetInstances() const
    {
        return m_instances;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE uint32 NumDecals() const
    {
        return uint32(m_instances.Size());
    }

    HYP_METHOD()
    void ClearDecals();

    /// World space bounds of every placement's projection box.
    HYP_FORCE_INLINE const BoundingBox& GetDecalBounds() const
    {
        return m_decalBounds;
    }

    void UpdateRenderProxy(struct RenderProxyDecalProxy* proxy);

protected:
    void Init() override;

    void Update(float delta) override;

private:
    void OnDecalsChanged();
    void UpdateDecalBounds();

    HYP_FIELD(Property = "Instances", Serialize)
    Array<DecalInstance> m_instances;

    HYP_FIELD(Property = "NextDecalId", Serialize)
    uint32 m_nextDecalId;

    Handle<Decal> m_decal;

    BoundingBox m_decalBounds;

    Decal* m_cachedDecal;
    int m_cachedDecalVersion;

    Material* m_cachedMaterial;
    int m_cachedMaterialVersion;
};

} // namespace Hyperion
