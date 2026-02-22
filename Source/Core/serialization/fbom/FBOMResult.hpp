/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>

namespace Hyperion::serialization {

struct FBOMResult
{
    enum
    {
        FBOM_OK = 0,
        FBOM_ERR = 1
    } value;

    String message;

    FBOMResult(decltype(FBOM_OK) value = FBOM_OK, UTF8StringView message = "")
        : value(value),
          message(message)
    {
    }

    FBOMResult(const FBOMResult& other) = default;
    FBOMResult& operator=(const FBOMResult& other) = default;
    FBOMResult(FBOMResult&& other) noexcept = default;
    FBOMResult& operator=(FBOMResult&& other) noexcept = default;

    HYP_FORCE_INLINE operator int() const
    {
        return int(value);
    }

    HYP_FORCE_INLINE bool IsOK() const
    {
        return value == FBOM_OK;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(value);
        hc.Add(message);

        return hc;
    }
};

} // namespace Hyperion::serialization
