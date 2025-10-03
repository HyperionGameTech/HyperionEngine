#pragma once

#include <core/utilities/TypeId.hpp>

#include <core/Constants.hpp>

namespace hyperion {

struct HypData;

template <class T>
constexpr bool IsHypDataV = TypeId::ForType<T>() == TypeId::ForType<HypData>();

} // namespace hyperion
