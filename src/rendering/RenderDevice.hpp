/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

namespace hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class DeviceBase : public ObjectBase
{
    HYP_OBJECT_BODY(DeviceBase);

public:
    virtual ~DeviceBase() override = default;
};

} // namespace hyperion
