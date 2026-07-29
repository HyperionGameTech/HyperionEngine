/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Baking/BakeData.hpp>
#include <Baking/BakerMemory.hpp>

namespace Hyperion {

class EnvProbe;

namespace Baking {

template <>
class BakeData<EnvProbe> : public BakeDataBase
{
public:
    using BitmapType = Bitmap_RGBA16F;
    using VisibilityBitmapType = Bitmap_RG16F;

    BakeData()
        : m_envProbe(nullptr)
    {
    }

    BakeData(Span<const BakeEntity> bakeEntities, EnvProbe* envProbe)
        : BakeDataBase(bakeEntities),
          m_envProbe(envProbe)
    {
    }

    BakeData(const BakeData& other) = default;
    BakeData(BakeData&& other) noexcept = default;

    BakeData& operator=(const BakeData& other) = default;
    BakeData& operator=(BakeData&& other) noexcept = default;

    ~BakeData() override = default;

    virtual Result Build() override;

    BitmapType ToBitmap() const;
    VisibilityBitmapType ToVisibilityBitmap() const;

protected:
    EnvProbe* m_envProbe;
    Array<LightmapRay, BakerAllocator> m_rays;
};

} // namespace Baking

} // namespace Hyperion
