#ifndef NAMES_PAIR_HPP
#define NAMES_PAIR_HPP

#include <core/lib/DynArray.hpp>
#include <Types.hpp>

#include <cstdint>
#include <utility>

namespace hyperion::compiler {

typedef std::pair<UInt16, Array<UInt8>> NamesPair_t;

} // namespace hyperion::compiler

#endif
