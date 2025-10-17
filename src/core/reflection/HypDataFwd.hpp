#pragma once

#include <core/reflection/TypeId.hpp>

#include <core/Constants.hpp>

namespace hyperion {

struct HypData;

template <class T>
constexpr bool IsHypDataV = TypeId::ForType<T>() == TypeId::ForType<HypData>();

} // namespace hyperion
