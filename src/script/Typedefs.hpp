#ifndef ACE_TYPEDEFS_HPP
#define ACE_TYPEDEFS_HPP

#include <script/Script.hpp>

#include <stdint.h>

namespace hyperion {
namespace vm {
  
// forward declarations
struct Value;
struct VMState;
struct ExecutionThread;

} // namespace vm

typedef int32_t aint32;
typedef int64_t aint64;
typedef float   afloat32;
typedef double  afloat64;

} // namespace hyperion

#endif
