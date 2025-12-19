/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

#include <core/Name.hpp>
#include <core/reflection/Handle.hpp>

#include <core/utilities/Pair.hpp>

#include <core/math/MathUtil.hpp>
#include <core/Constants.hpp>
#include <core/Types.hpp>

namespace hyperion {

class GaussianSplatting;

class HYP_API RenderEnvironment final
{
public:
    RenderEnvironment();
    RenderEnvironment(const RenderEnvironment& other) = delete;
    RenderEnvironment& operator=(const RenderEnvironment& other) = delete;
    ~RenderEnvironment();

    const Handle<GaussianSplatting>& GetGaussianSplatting() const
    {
        return m_gaussianSplatting;
    }

    void Initialize();

private:
    Handle<GaussianSplatting> m_gaussianSplatting;
};

} // namespace hyperion
