/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

#include <core/Defines.hpp>

namespace hyperion {

HYP_CLASS(Abstract, NoScriptBindings, Pool = "EPN_RENDER")
class DeviceBase : public HypObjectBase
{
    HYP_OBJECT_BODY(DeviceBase);

public:
    virtual ~DeviceBase() override = default;
};

} // namespace hyperion
