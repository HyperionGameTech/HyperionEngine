#pragma once

#include <core/utilities/TypeId.hpp>

#include <core/Constants.hpp>

namespace hyperion {

struct HypData;

template <class T>
constexpr bool is_hyp_data_v = TypeId::ForType<T>() == TypeId::ForType<HypData>();

} // namespace hyperion
