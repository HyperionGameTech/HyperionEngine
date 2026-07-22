/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/BakeData.hpp>

#include <Core/Memory/UniquePtr.hpp>

namespace Hyperion {

class FogVolume;
class VoxelOctree;

namespace Baking {

template <>
class BakeData<FogVolume> : public BakeDataBase
{
public:
    using VolumeBitmap = Bitmap3D_RG16F;
    using NoiseBitmap = Bitmap3D_R8;

    BakeData()
        : m_fogVolume(nullptr)
    {
    }

    BakeData(Span<const BakeEntity> bakeEntities, FogVolume* fogVolume)
        : BakeDataBase(bakeEntities),
          m_fogVolume(fogVolume)
    {
    }

    BakeData(const BakeData& other) = delete;
    BakeData(BakeData&& other) noexcept = default;

    BakeData& operator=(const BakeData& other) = delete;
    BakeData& operator=(BakeData&& other) noexcept = default;

    ~BakeData() override = default;

    HYP_FORCE_INLINE VoxelOctree* GetVoxelOctree() const
    {
        return m_voxelOctree.Get();
    }

    HYP_FORCE_INLINE VolumeBitmap& GetVolumeBitmap()
    {
        return m_volumeBitmap;
    }

    HYP_FORCE_INLINE const VolumeBitmap& GetVolumeBitmap() const
    {
        return m_volumeBitmap;
    }

    HYP_FORCE_INLINE NoiseBitmap& GetNoiseBitmap()
    {
        return m_noiseBitmap;
    }

    HYP_FORCE_INLINE const NoiseBitmap& GetNoiseBitmap() const
    {
        return m_noiseBitmap;
    }

    HYP_FORCE_INLINE const Vec3f& GetSunDirection() const
    {
        return m_sunDirection;
    }

    HYP_FORCE_INLINE void SetSunDirection(const Vec3f& direction)
    {
        m_sunDirection = direction;
    }

    float ComputeDirectionalShadow(const Vec3f& posWS) const;

    virtual Result Build() override;

protected:
    FogVolume* m_fogVolume;
    UniquePtr<VoxelOctree> m_voxelOctree;
    VolumeBitmap m_volumeBitmap;
    NoiseBitmap m_noiseBitmap;

    Vec3f m_sunDirection;
};

} // namespace Baking

} // namespace Hyperion
