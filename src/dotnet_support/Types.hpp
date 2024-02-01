#ifndef HYP_DOTNET_SUPPORT_TYPES_HPP
#define HYP_DOTNET_SUPPORT_TYPES_HPP

#include <type_traits>

namespace hyperion {
namespace dotnet {

using Delegate = std::add_pointer_t<void()>;

} // namespace dotnet
} // namespace hyperion

#endif