/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <scene/Entity.hpp>

namespace Hyperion {

HYP_CLASS(Abstract)
class HYP_API VolumeBase : public Entity
{
    HYP_OBJECT_BODY(VolumeBase);

public:
    VolumeBase() = default;

    explicit VolumeBase(Name name)
        : Entity(name)
    {
    }

    VolumeBase(Name name, const BoundingBox& localBounds)
        : Entity(name)
    {
        m_localBounds = localBounds;
    }

    explicit VolumeBase(const BoundingBox& localBounds)
    {
        m_localBounds = localBounds;
    }

    virtual ~VolumeBase() override = default;
};

} // namespace Hyperion
