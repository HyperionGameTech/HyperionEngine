/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/utilities/Time.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT uint64 Time_Now()
{
    return uint64(Time::Now());
}

} // extern "C"