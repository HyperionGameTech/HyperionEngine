/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/BakeData.hpp>

#include <Util/img/Bitmap.hpp>

namespace Hyperion {

class Light;

namespace Baking {

template <>
class BakeData<Light> : public BakeDataBase
{
public:
    using BitmapType = Bitmap_R16;

    BakeData()
        : m_light(nullptr)
    {
    }

    BakeData(Span<const BakeEntity> bakeEntities, Light* light)
        : BakeDataBase(bakeEntities),
          m_light(light)
    {
    }

    BakeData(const BakeData& other) = default;
    BakeData(BakeData&& other) noexcept = default;

    BakeData& operator=(const BakeData& other) = default;
    BakeData& operator=(BakeData&& other) noexcept = default;

    ~BakeData() override = default;

    HYP_FORCE_INLINE Light* GetLight() const
    {
        return m_light;
    }

    uint32 GetNumFaces() const;

    virtual Result Build() override;

    BitmapType ToBitmap() const;

protected:
    Light* m_light;
    Array<LightmapRay> m_rays;

    Mat4f m_viewProjMats[6];
    Mat4f m_projMat;
};

} // namespace Baking
} // namespace Hyperion
