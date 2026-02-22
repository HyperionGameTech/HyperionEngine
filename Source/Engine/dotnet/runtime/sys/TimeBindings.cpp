/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <Core/utilities/Time.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT uint64 Time_Now()
    {
        return uint64(Time::Now());
    }

} // extern "C"
