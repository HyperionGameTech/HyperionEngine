/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <baking/BakeData.hpp>

namespace Hyperion {

class ReflectionProbe;

namespace Baking {

template <>
class BakeData<ReflectionProbe> : public BakeDataBase
{
public:
    using BitmapType = Bitmap_RGBA16F;

    BakeData()
        : m_envProbe(nullptr)
    {
    }

    BakeData(Span<const BakeEntity> bakeEntities, ReflectionProbe* envProbe)
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

protected:
    ReflectionProbe* m_envProbe;
    Array<LightmapRay> m_rays;
};

} // namespace Baking

} // namespace Hyperion
