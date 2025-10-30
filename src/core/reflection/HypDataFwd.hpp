#pragma once

#include <core/reflection/TypeId.hpp>

#include <core/Constants.hpp>

namespace hyperion {

struct HypData;

static constexpr auto HypDataTypeId = CONSTEXPR_TYPE_ID(HypData);

template <class T>
constexpr bool IsHypDataV = CONSTEXPR_TYPE_ID(T) == HypDataTypeId;

} // namespace hyperion
