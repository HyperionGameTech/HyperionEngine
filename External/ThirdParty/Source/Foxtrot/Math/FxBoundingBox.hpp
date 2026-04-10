#pragma once

#include "FxVec3.hpp"

class FxBoundingBox
{
public:
    FxBoundingBox() = default;
    FxBoundingBox(FxVec3f min, FxVec3f max);

    FxBoundingBox operator+(const FxBoundingBox& other) const;
    FxBoundingBox& operator+=(const FxBoundingBox& other);

    FxBoundingBox& operator=(const FxBoundingBox& other);

public:
    FxVec3f Min = FxVec3f::sZero;
    FxVec3f Max = FxVec3f::sZero;
};
