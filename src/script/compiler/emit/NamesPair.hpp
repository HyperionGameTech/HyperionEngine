#ifndef NAMES_PAIR_HPP
#define NAMES_PAIR_HPP

#include <cstdint>
#include <vector>
#include <utility>

namespace hyperion::compiler {

typedef std::pair<uint16_t, std::vector<uint8_t>> NamesPair_t;

} // namespace hyperion::compiler

#endif
