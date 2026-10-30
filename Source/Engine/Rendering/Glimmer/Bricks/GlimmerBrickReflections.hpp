/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RawBuffer.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector2.hpp>

namespace Hyperion {

class Texture;
class View;
class GBuffer;
class EnvProbe;
class GlimmerScenePassData;

class GlimmerBrickReflections final
{
public:
    GlimmerBrickReflections();

    GlimmerBrickReflections(const GlimmerBrickReflections& other) = delete;
    GlimmerBrickReflections& operator=(const GlimmerBrickReflections& other) = delete;

    ~GlimmerBrickReflections();

    void Render(Frame* frame, View* view, GBuffer* gbuffer, const Vec2u& extent, const GlimmerScenePassData* scene, EnvProbe* skyProbe);

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_isValid;
    }

    // rgb = radiance times a, a = how much of the pixel's reflection was traced, through the bricks or the heightfield
    const GpuImageViewRef& GetImageView() const;

private:
    void CreateTextures(const Vec2u& extent);
    void CreateSlotBuffers(uint32 numCrossings);

    // half resolution
    Handle<Texture> m_rawColorTexture;
    Handle<Texture> m_rawInfoTexture;
    FixedArray<Handle<Texture>, 2> m_guideTextures;
    FixedArray<Handle<Texture>, 2> m_keptColorTextures;
    FixedArray<Handle<Texture>, 2> m_keptInfoTextures;

    Handle<Texture> m_fullTexture;
    Handle<Texture> m_unusedOutputTexture;

    RWStructuredBuffer m_blocksBuffer;
    RWStructuredBuffer m_claimsBuffer;
    RWStructuredBuffer m_requestsBuffer;
    RWStructuredBuffer m_wantedBuffer; // what each resolved pixel found missing, until the finish; then the pixels of the view that get a ray after the spread
    RWStructuredBuffer m_listBuffer;
    RWStructuredBuffer m_countsBuffer;

    Vec2u m_extent;
    uint32 m_numCrossings = 0;
    uint32 m_frame = 0;
    uint32 m_parity = 0;
    bool m_hasHistory = false;
    bool m_hasRequests = false;
    bool m_isValid = false;
};

} // namespace Hyperion
