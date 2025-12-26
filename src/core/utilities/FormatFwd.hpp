// Forward declaration for Formatter struct (for specialization)

#pragma once

namespace Hyperion {
namespace utilities {

template <class StringType, class T, class T2 = void>
struct Formatter;

} // namespace utilities
} // namespace Hyperion

// placeholder define until Format.hpp is included and overrides this macro
#ifndef HYP_FORMAT
// Defines a default format macro that does nothing except returns the format string back to the caller
// keep VA_ARGS in the expression in case they have side-effects
#define HYP_FORMAT(fmt, ...) (__VA_ARGS__ __VA_OPT__(, ) fmt)
#endif
