#pragma once

#include <core/reflection/TypeId.hpp>

#include <core/Constants.hpp>

namespace Hyperion {

struct BoxedValue;

static constexpr auto BoxedValueTypeId = CONSTEXPR_TYPE_ID(BoxedValue);

template <class T>
constexpr bool IsBoxedValueV = CONSTEXPR_TYPE_ID(T) == BoxedValueTypeId;

} // namespace Hyperion
