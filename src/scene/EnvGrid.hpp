/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENV_GRID_HPP
#define HYPERION_ENV_GRID_HPP

#include <core/Base.hpp>

#include <core/config/Config.hpp>

#include <core/containers/Bitset.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/math/BoundingBox.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderCommand.hpp>

#include <GameCounter.hpp>
#include <HashCode.hpp>

namespace hyperion {

class View;
class Scene;
class RenderEnvGrid;
class RenderEnvProbe;
class EnvProbe;

enum class EnvGridFlags : uint32
{
    NONE = 0x0,
    USE_VOXEL_GRID = 0x1,
    DEBUG_DISPLAY_PROBES = 0x2
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
    uint32 num_probes = 0;
    FixedArray<uint32, max_bound_ambient_probes * 2> indirect_indices = { 0 };
    FixedArray<Handle<EnvProbe>, max_bound_ambient_probes> env_probes = {};
    FixedArray<TResourceHandle<RenderEnvProbe>, max_bound_ambient_probes> env_render_probes = {};

    // Must be called in EnvGrid::Init(), before probes are used from the render thread.
    // returns the index
    uint32 AddProbe(const Handle<EnvProbe>& env_probe);

    // Must be called in EnvGrid::Init(), before probes are used from the render thread.
    void AddProbe(uint32 index, const Handle<EnvProbe>& env_probe);

    HYP_FORCE_INLINE void SetIndexOnGameThread(uint32 index, uint32 new_index)
    {
        AssertThrow(index < max_bound_ambient_probes);
        AssertThrow(new_index < max_bound_ambient_probes);

        indirect_indices[index] = new_index;
    }

    HYP_FORCE_INLINE uint32 GetIndexOnGameThread(uint32 index) const
    {
        return indirect_indices[index];
    }

    HYP_FORCE_INLINE const Handle<EnvProbe>& GetEnvProbeDirect(uint32 index) const
    {
        return env_probes[index];
    }

    HYP_FORCE_INLINE const Handle<EnvProbe>& GetEnvProbeOnGameThread(uint32 index) const
    {
        return env_probes[indirect_indices[index]];
    }

    HYP_FORCE_INLINE void SetIndexOnRenderThread(uint32 index, uint32 new_index)
    {
        AssertThrow(index < max_bound_ambient_probes);
        AssertThrow(new_index < max_bound_ambient_probes);

        indirect_indices[max_bound_ambient_probes + index] = new_index;
    }

    HYP_FORCE_INLINE uint32 GetIndexOnRenderThread(uint32 index) const
    {
        return indirect_indices[max_bound_ambient_probes + index];
    }

    HYP_FORCE_INLINE const Handle<EnvProbe>& GetEnvProbeOnRenderThread(uint32 index) const
    {
        return env_probes[indirect_indices[max_bound_ambient_probes + index]];
    }
};

struct EnvGridOptions
{
    EnvGridType type = ENV_GRID_TYPE_SH;
    Vec3u density = Vec3u::Zero();
    EnumFlags<EnvGridFlags> flags = EnvGridFlags::NONE;
};

HYP_CLASS()
class HYP_API EnvGrid : public HypObject<EnvGrid>
{
    HYP_OBJECT_BODY(EnvGrid);

public:
    EnvGrid();

    EnvGrid(
        const Handle<Scene>& parent_scene,
        const BoundingBox& aabb,
        const EnvGridOptions& options);

    EnvGrid(const EnvGrid& other) = delete;
    EnvGrid& operator=(const EnvGrid& other) = delete;
    ~EnvGrid();

    HYP_FORCE_INLINE EnvGridType GetEnvGridType() const
    {
        return m_options.type;
    }

    HYP_FORCE_INLINE const EnvGridOptions& GetOptions() const
    {
        return m_options;
    }

    HYP_FORCE_INLINE EnvProbeCollection& GetEnvProbeCollection()
    {
        return m_env_probe_collection;
    }

    HYP_FORCE_INLINE const EnvProbeCollection& GetEnvProbeCollection() const
    {
        return m_env_probe_collection;
    }

    HYP_FORCE_INLINE RenderEnvGrid& GetRenderResource() const
    {
        return *m_render_resource;
    }

    HYP_METHOD()
    const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    HYP_METHOD()
    void SetAABB(const BoundingBox& aabb);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene>& GetParentScene() const
    {
        return m_parent_scene;
    }

    HYP_METHOD()
    void SetParentScene(const Handle<Scene>& parent_scene);

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

    void Init();
    void Update(GameCounter::TickUnit delta);

private:
    HYP_FORCE_INLINE Vec3f SizeOfProbe() const
    {
        return m_aabb.GetExtent() / Vec3f(m_options.density);
    }

    void CreateEnvProbes();

    Handle<Scene> m_parent_scene;
    Handle<View> m_view;
    Handle<Camera> m_camera;

    EnvGridOptions m_options;

    HYP_FIELD(Property = "AABB", Serialize = true)
    BoundingBox m_aabb;

    Vec3f m_offset;
    BoundingBox m_voxel_grid_aabb;

    EnvProbeCollection m_env_probe_collection;

    RenderEnvGrid* m_render_resource;
};

} // namespace hyperion

#endif // !HYPERION_ENV_GRID_HPP
