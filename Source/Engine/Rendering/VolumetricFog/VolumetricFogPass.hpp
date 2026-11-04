/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>
#include <Core/Math/Mat4f.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class Mesh;
class Texture;
class CBufferAllocator;
struct RenderSetup;

struct VolumetricFogGridShaderData
{
    Vec4u dimensions;
    Vec4f depthParams;
    Vec4f jitter;
    Vec4f params;
};

static_assert(sizeof(VolumetricFogGridShaderData) == 64);

struct VolumetricFogVolumeShaderData
{
    Vec4f aabbMin;
    Vec4f aabbMax;
    Vec4f lighting;
    Vec4f phase;
};

static_assert(sizeof(VolumetricFogVolumeShaderData) == 64);

class VolumetricFogPass
{
public:
    static constexpr uint32 MaxFogVolumes = 8;

    explicit VolumetricFogPass(const Vec2u& extent);

    VolumetricFogPass(const VolumetricFogPass&) = delete;
    VolumetricFogPass& operator=(const VolumetricFogPass&) = delete;

    ~VolumetricFogPass();

    void Create();
    void Resize(const Vec2u& extent);

    void Update(Frame* frame, const RenderSetup& renderSetup);
    void Render(Frame* frame, const RenderSetup& renderSetup);

    HYP_FORCE_INLINE bool IsActive() const
    {
        return m_isActive;
    }

    float GetDistance() const;

    void WriteShaderData(CBufferAllocator& cbufferAllocator) const;

    const GpuImageViewRef& GetIntegratedView() const;

private:
    void CreateNoiseTexture();
    void GenerateNoise(Frame* frame);
    void CreateGridTextures(const Vec3u& dimensions);

    void ComputeAmbient(Frame* frame, const RenderSetup& renderSetup, const VolumetricFogGridShaderData& grid);
    void Inject(Frame* frame, const RenderSetup& renderSetup, const VolumetricFogGridShaderData& grid);
    void ResolveTemporal(Frame* frame, const RenderSetup& renderSetup, const VolumetricFogGridShaderData& grid);
    void Integrate(Frame* frame, const VolumetricFogGridShaderData& grid);

    Vec2u m_extent;
    Vec3u m_gridDimensions;
    Vec3u m_ambientDimensions;

    Handle<Texture> m_noiseTexture;
    bool m_noiseReady;

    Handle<Texture> m_ambientTexture;
    Handle<Texture> m_litTexture;
    Handle<Texture> m_historyTextures[2];
    Handle<Texture> m_integratedTexture;
    uint32 m_historyIndex;

    Handle<Mesh> m_quadMesh;

    VolumetricFogGridShaderData m_gridShaderData;
    VolumetricFogGridShaderData m_previousGridShaderData;
    Mat4f m_previousViewProjection;
    uint32 m_lastRenderedFrame;

    bool m_isActive;
};

} // namespace Hyperion
