/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIGHTMAP_VOLUME_HPP
#define HYPERION_LIGHTMAP_VOLUME_HPP

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/utilities/UUID.hpp>

#include <core/Handle.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

namespace hyperion {

class Texture;
class RenderLightmapVolume;

// @TODO: Create RenderLightmapVolume, and add it to the RenderState.
// Any visible objects that have a LightmapElementComponent with `volume` of this LightmapVolume
// should be bound to the RenderState, so we ensure the proper textures are available at render time.

// Could also group any rendered objects that have lightmaps to render with that lightmap bound globally.

HYP_ENUM()
enum class LightmapElementTextureType : uint32
{
    RADIANCE = 0,
    IRRADIANCE,

    MAX,

    INVALID = ~0u
};

HYP_STRUCT()

struct LightmapElementTextureEntry
{
    HYP_FIELD(Property = "TextureType", Serialize = true)
    LightmapElementTextureType type = LightmapElementTextureType::INVALID;

    HYP_FIELD(Property = "Texture", Serialize = true)
    Handle<Texture> texture;
};

HYP_STRUCT()

struct LightmapElement
{
    HYP_FIELD(Property = "Index", Serialize = true)
    uint32 index = ~0u;

    HYP_FIELD(Property = "Entries", Serialize = true)
    Array<LightmapElementTextureEntry> entries;

    HYP_METHOD()

    bool IsValid() const
    {
        return index != ~0u;
    }
};

HYP_CLASS()
class HYP_API LightmapVolume : public HypObject<LightmapVolume>
{
    HYP_OBJECT_BODY(LightmapVolume);

public:
    LightmapVolume();

    LightmapVolume(const BoundingBox& aabb);

    LightmapVolume(const LightmapVolume& other) = delete;
    LightmapVolume& operator=(const LightmapVolume& other) = delete;
    ~LightmapVolume();

    HYP_FORCE_INLINE RenderLightmapVolume& GetRenderResource() const
    {
        return *m_render_resource;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const UUID& GetUUID() const
    {
        return m_uuid;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    /*! \brief Add a LightmapElement to this volume.
     *  If \ref{element} has an index of ~0u, the index will be set based on where it would sit in the list of elements (at the back of the list)
     *  Otherwise, if index is not equal to ~0u, it will be inserted at the position corresponding to \ref{index}, assuming that position is not already taken by a valid LightmapElement.
     *  If that position is currently taken, false will be returned, indicating an unsuccessful insertion. Otherwise, true will be returned, indicating successful insertion. */
    HYP_METHOD()
    bool AddElement(LightmapElement element);

    HYP_METHOD()
    const LightmapElement* GetElement(uint32 index) const;

    void Init();

private:
    RenderLightmapVolume* m_render_resource;

    HYP_FIELD(Serialize = true)
    UUID m_uuid;

    HYP_FIELD(Serialize = true)
    BoundingBox m_aabb;

    HYP_FIELD(Serialize = true)
    Array<LightmapElement> m_elements;
};

} // namespace hyperion

#endif
