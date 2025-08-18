/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/config/Config.hpp>

#include <core/containers/Bitset.hpp>

#include <core/object/HypObject.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/math/BoundingBox.hpp>

#include <scene/Entity.hpp>

#include <rendering/RenderCollection.hpp>
#include <rendering/RenderCommand.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

class View;
class Scene;
class EnvProbe;
class Texture;
class RenderProxyEnvGrid;

enum class EnvGridFlags : uint32
{
    NONE = 0x0,
    USE_VOXEL_GRID = 0x1
};

HYP_MAKE_ENUM_FLAGS(EnvGridFlags);

enum EnvGridType : uint32
{
    ENV_GRID_TYPE_INVALID = uint32(-1),
    ENV_GRID_TYPE_SH = 0,
    ENV_GRID_TYPE_VOXEL,
    ENV_GRID_TYPE_LIGHT_FIELD,
    ENV_GRID_TYPE_MAX
};

struct EnvProbeCollection
{
    uint32 numProbes = 0;
    Array<uint32, DynamicAllocator> indirectIndices;
    Array<EnvProbe*, DynamicAllocator> envProbes;

    EnvProbeCollection()
    {
        indirectIndices.Resize(g_maxBoundAmbientProbes * 2);
        envProbes.Resize(g_maxBoundAmbientProbes);
    }

    // Must be called in EnvGrid::Init(), before probes are used from the render thread.
    // returns the index
    uint32 AddProbe(const Handle<EnvProbe>& envProbe);

    // Must be called in EnvGrid::Init(), before probes are used from the render thread.
    void AddProbe(uint32 index, const Handle<EnvProbe>& envProbe);

    HYP_FORCE_INLINE void SetIndexOnGameThread(uint32 index, uint32 newIndex)
    {
        Assert(index < g_maxBoundAmbientProbes);
        Assert(newIndex < g_maxBoundAmbientProbes);

        indirectIndices[index] = newIndex;
    }

    HYP_FORCE_INLINE uint32 GetIndexOnGameThread(uint32 index) const
    {
        return indirectIndices[index];
    }

    HYP_FORCE_INLINE EnvProbe* GetEnvProbeDirect(uint32 index) const
    {
        return envProbes[index];
    }

    HYP_FORCE_INLINE EnvProbe* GetEnvProbeOnGameThread(uint32 index) const
    {
        return envProbes[indirectIndices[index]];
    }

    HYP_FORCE_INLINE void SetIndexOnRenderThread(uint32 index, uint32 newIndex)
    {
        Assert(index < g_maxBoundAmbientProbes);
        Assert(newIndex < g_maxBoundAmbientProbes);

        indirectIndices[g_maxBoundAmbientProbes + index] = newIndex;
    }

    HYP_FORCE_INLINE uint32 GetIndexOnRenderThread(uint32 index) const
    {
        return indirectIndices[g_maxBoundAmbientProbes + index];
    }

    HYP_FORCE_INLINE EnvProbe* GetEnvProbeOnRenderThread(uint32 index) const
    {
        return envProbes[indirectIndices[g_maxBoundAmbientProbes + index]];
    }
};

struct EnvGridOptions
{
    EnvGridType legacyEnvGridType = ENV_GRID_TYPE_SH;
    Vec3u density = { 3, 3, 3 };
    EnumFlags<EnvGridFlags> flags = EnvGridFlags::NONE;
};

HYP_CLASS()
class HYP_API EnvGrid : public Entity
{
    HYP_OBJECT_BODY(EnvGrid);

public:
    EnvGrid();
    EnvGrid(const BoundingBox& aabb, const EnvGridOptions& options);
    EnvGrid(const EnvGrid& other) = delete;
    EnvGrid& operator=(const EnvGrid& other) = delete;
    ~EnvGrid() override;

    HYP_FORCE_INLINE const EnvGridOptions& GetOptions() const
    {
        return m_options;
    }

    HYP_METHOD()
    const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    virtual void UpdateRenderProxy(RenderProxyEnvGrid* proxy) = 0;

protected:
    HYP_FORCE_INLINE Vec3f SizeOfProbe() const
    {
        return m_aabb.GetExtent() / Vec3f(m_options.density);
    }

    EnvGridOptions m_options;

    HYP_FIELD(Property = "AABB", Serialize = true)
    BoundingBox m_aabb;

    Vec3f m_offset;
};

HYP_CLASS()
class HYP_API LegacyEnvGrid : public EnvGrid
{
    HYP_OBJECT_BODY(LegacyEnvGrid);

public:
    LegacyEnvGrid()
        : LegacyEnvGrid(BoundingBox(-50.0f, 50.0f), {})
    {
    }

    LegacyEnvGrid(const BoundingBox& aabb, const EnvGridOptions& options)
        : EnvGrid(aabb, options),
          m_voxelGridAabb(aabb)
    {
        m_entityInitInfo.receivesUpdate = true;
        m_entityInitInfo.canEverUpdate = true;
    }

    ~LegacyEnvGrid() override = default;

    HYP_FORCE_INLINE EnvGridType GetEnvGridType() const
    {
        return m_options.legacyEnvGridType;
    }

    HYP_FORCE_INLINE EnvProbeCollection& GetEnvProbeCollection()
    {
        return m_envProbeCollection;
    }

    HYP_FORCE_INLINE const EnvProbeCollection& GetEnvProbeCollection() const
    {
        return m_envProbeCollection;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetVoxelGridTexture() const
    {
        return m_voxelGridTexture;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetLightFieldIrradianceTexture() const
    {
        return m_irradianceTexture;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetLightFieldDepthTexture() const
    {
        return m_depthTexture;
    }

    HYP_METHOD()
    void SetAABB(const BoundingBox& aabb);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<View>& GetView() const
    {
        return m_view;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Camera>& GetCamera() const
    {
        return m_camera;
    }

    HYP_METHOD()
    void Translate(const BoundingBox& aabb, const Vec3f& translation);

    void UpdateRenderProxy(RenderProxyEnvGrid* proxy) override;

private:
    void Init() override;
    void Update(float delta) override;

    void OnAttachedToNode(Node* node) override;
    void OnDetachedFromNode(Node* node) override;

    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

    void OnAddedToScene(Scene* scene) override;
    void OnRemovedFromScene(Scene* scene) override;
    
    void CreateEnvProbes();

    Handle<View> m_view;
    Handle<Camera> m_camera;

    BoundingBox m_voxelGridAabb;

    EnvProbeCollection m_envProbeCollection;

    Handle<Texture> m_voxelGridTexture;

    Handle<Texture> m_irradianceTexture;
    Handle<Texture> m_depthTexture;

    HashCode m_cachedOctantHashCode;
};

} // namespace hyperion

