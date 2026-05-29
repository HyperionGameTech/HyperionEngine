/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/threading/Task.hpp>
#include <Core/threading/Mutex.hpp>

#include <Core/reflection/ObjectMacros.hpp>

#include <Core/math/Vector2.hpp>
#include <Core/math/BoundingBox.hpp>

#include <Core/functional/Delegate.hpp>

#include <Core/HashCode.hpp>

#include <streaming/Streamable.hpp>

namespace Hyperion {

class WorldGrid;
class Scene;
class AssetReference;

HYP_ENUM()
enum class StreamingCellState : uint32
{
    INVALID = ~0u,

    UNLOADED = 0,
    UNLOADING,
    WAITING,
    LOADING,
    LOADED,

    MAX
};

HYP_STRUCT()
struct StreamingCellNeighbor
{
    HYP_STRUCT_BODY(StreamingCellNeighbor);

    HYP_FIELD(Serialize, Property = "Coord")
    Vec2i coord;

    HYP_FORCE_INLINE Vec2f GetCenter() const
    {
        return Vec2f(coord) - 0.5f;
    }
};

HYP_STRUCT(Size = 80)
struct StreamingCellInfo
{
    HYP_STRUCT_BODY(StreamingCellInfo);

    HYP_FIELD(Serialize, Property = "Coord")
    Vec2i coord;

    HYP_FIELD(Serialize, Property = "Extent")
    Vec3u extent;

    HYP_FIELD(Serialize, Property = "Scale")
    Vec3f scale = Vec3f::One();

    HYP_FIELD(Serialize, Property = "Bounds")
    BoundingBox bounds;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;

        hashCode.Add(coord);
        hashCode.Add(extent);
        hashCode.Add(scale);
        hashCode.Add(bounds);

        return hashCode;
    }
};

HYP_CLASS()
class ENGINE_API StreamingCell : public StreamableBase
{
    HYP_OBJECT_BODY(StreamingCell);

public:
    StreamingCell() = default;
    explicit StreamingCell(const StreamingCellInfo& cellInfo);
    virtual ~StreamingCell() override;

    HYP_METHOD()
    HYP_FORCE_INLINE const StreamingCellInfo& GetPatchInfo() const
    {
        return m_cellInfo;
    }

    HYP_FORCE_INLINE Span<const AssetReference> GetAssetReferences() const
    {
        return Span<const AssetReference>(m_assetReferences.Data(), m_assetReferences.Size());
    }

    void AddAssetReference(const AssetReference& assetReference, bool shouldLoad = false);
    void RemoveAssetReference(const AssetReference& assetReference);

    HYP_METHOD(Scriptable)
    void Update(float delta);

    Delegate<void, StreamingCell*> OnCellLoaded;
    Delegate<void, StreamingCell*> OnCellUnloaded;

protected:
    HYP_METHOD()
    virtual BoundingBox GetBoundingBox_Impl() const override
    {
        return m_cellInfo.bounds;
    }

    virtual void OnStreamStart_Impl() override;
    virtual void OnLoaded_Impl() override;
    virtual void OnRemoved_Impl() override;

    HYP_METHOD()
    virtual void Update_Impl(float delta)
    {
    }

    StreamingCellInfo m_cellInfo;
    Array<AssetReference, DynamicAllocator> m_assetReferences;
};

} // namespace Hyperion
